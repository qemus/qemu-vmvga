<h1 align="center">Helios<br />
<div align="center">
  
[![Build]][build_url]
[![Version]][release_url]
[![Size]][release_url]

</div></h1>

Custom QEMU build with patches for accelerated Windows graphics.

## What is Helios? ☀️

Helios is a paravirtualized graphics stack that allows a Windows virtual machine to use the host GPU for hardware-accelerated graphics without passing the physical GPU through to the guest.

Traditional GPU passthrough gives the Windows guest direct ownership of a physical GPU. The normal vendor driver is then installed inside Windows and applications interact with essentially the same GPU stack they would use on bare metal. This provides excellent compatibility, but the GPU generally becomes dedicated to that VM while it is running.

Helios takes a very different approach, instead of exposing the physical GPU itself, QEMU presents a `virtio-gpu` device to the guest. Graphics commands are translated into Vulkan and transported through the virtual device to the Linux host, where they are executed by the host's normal Vulkan driver on the real GPU.

The physical GPU therefore remains owned by Linux and can continue to be shared with the host and other workloads.

At a high level:

```text
Windows application
        │
        ▼
Direct3D / Vulkan
        │
        ▼
Helios graphics stack
        │
        ▼
Mesa Venus
        │
        ▼
virtio-gpu
        │
        ▼
QEMU + virglrenderer
        │
        ▼
Host Vulkan driver
        │
        ▼
Physical GPU
```

This is closer to **API-level GPU paravirtualization** than traditional PCI passthrough. Windows sees a virtual graphics adapter, while the actual rendering work is ultimately performed by the host GPU.

## Why is this needed? 🤔

QEMU has supported accelerated `virtio-gpu` graphics for Linux guests for years.

Linux already contains the `virtio_gpu` kernel driver, while Mesa provides the userspace graphics drivers required to make use of it. A Linux guest can therefore use VirGL for OpenGL or Venus for Vulkan and send rendering commands through QEMU to the host GPU.

Windows does not normally have an equivalent accelerated `virtio-gpu` stack. The standard Windows `virtio-gpu` driver can provide a framebuffer and display output, but that alone does not provide a complete accelerated 3D graphics stack comparable to what Linux guests have.

Helios fills that gap and implements a Windows WDDM render and display adapter around `virtio-gpu`. Windows therefore sees Helios as an actual graphics adapter and can use it for desktop composition and application rendering, while the expensive GPU work is forwarded to the host instead of being executed by a software renderer inside the VM.

This avoids two undesirable alternatives:

- Software rendering, where Windows performs rendering on the CPU.
- PCI GPU passthrough, where a physical GPU must be dedicated directly to the VM.

Helios sits between those two approaches: the guest receives accelerated graphics while the host keeps ownership of the GPU.

## How rendering works 🚀

The core transport used by Helios is **Venus**, Mesa's virtual Vulkan implementation.

Venus serializes Vulkan operations in the guest and sends them through `virtio-gpu`. On the host, `virglrenderer` decodes those commands and submits equivalent work to the native Vulkan driver.

For Direct3D workloads, Helios uses the same principle by translating Direct3D into Vulkan before it enters the Venus transport.

A simplified Direct3D path looks like this:

```text
Windows application
        │
        ▼
Direct3D runtime
        │
        ▼
Helios user-mode driver
        │
        ▼
DXVK
        │
        ▼
Mesa Venus Vulkan ICD
        │
        ▼
Helios kernel-mode driver
        │
        ▼
virtio-gpu command stream
        │
        ▼
QEMU
        │
        ▼
virglrenderer / Venus renderer
        │
        ▼
Host Vulkan driver
        │
        ▼
GPU
```

The Windows kernel-mode driver does not need to understand or emulate the instruction set of a particular Intel, AMD, or NVIDIA GPU. The guest produces Venus command streams and the host graphics stack handles the real hardware. This is what makes the approach largely independent of the physical GPU vendor.

### GPU memory

Sending every texture, vertex buffer and rendered frame through an emulated device one byte at a time would be far too expensive.

`virtio-gpu` therefore supports **blob resources** and host-visible memory.

Large graphics allocations can be backed by host resources and exposed to the guest through the `virtio-gpu` host-memory PCI region. The guest can map those resources instead of repeatedly copying their contents through the control queue.

The command stream tells the host GPU what to do, while large resource data can remain in shared or host-visible allocations.

This distinction is important: Venus primarily transports **commands and synchronization**, not a complete copy of every GPU resource for every draw call.

## Display and scanout 🖥️

Rendering a frame and displaying that frame are separate problems.

Current Helios is both a WDDM render adapter and a display adapter. Windows' Desktop Window Manager can compose the desktop directly on the Helios adapter, and the resulting primary surface becomes a `virtio-gpu` scanout resource.

The presentation path is roughly:

```text
Windows / DWM
     │
     ▼
Helios WDDM display driver
     │
     ▼
virtio-gpu SET_SCANOUT_BLOB
     │
     ▼
QEMU
     │
     ▼
DMA-BUF / Vulkan image
     │
     ▼
QEMU display backend
     │
     ├── egl-headless
     ├── VNC
     └── SDL
```

This is where stock QEMU is not quite sufficient for the Helios architecture.

## Why does Helios need a custom QEMU? 🧩

Standard QEMU already supports virtio-gpu blob resources, Venus and DMA-BUF scanout. The difficult part is the exact layout of the image Windows asks QEMU to display.

A normal framebuffer has an obvious linear layout:

```text
pixel rows
pixel rows
pixel rows
pixel rows
```

Given an offset, stride, width and height, QEMU can calculate where every pixel resides.

Modern Vulkan images do not necessarily work like this. For performance reasons a GPU normally stores render targets using an implementation-specific **optimal tiling** layout:

```text
VK_IMAGE_TILING_OPTIMAL
```

The bytes in such an allocation are arranged according to rules chosen by the Vulkan driver and GPU. A simple `offset + stride × height` calculation does not describe the physical image layout.

Helios can expose exactly such a host-backed Vulkan image as the Windows desktop primary.

The underlying resource can be exported as a DMA-BUF, but that does not always mean stock QEMU has enough information to correctly import and display it.

In particular:

- the resource may use `VK_IMAGE_TILING_OPTIMAL`;
- the guest-visible pitch describes the logical Windows surface, not necessarily the native GPU allocation;
- a DMA-BUF may have no useful DRM modifier describing the private Vulkan layout;
- some drivers can export a modifier even though recreating the image purely from that modifier is not sufficient for this particular resource;
- treating an opaque native image as ordinary linear memory can result in corruption or black frames.

The Helios QEMU changes bridge that gap.

### Native Venus scanout

The patched QEMU carries additional metadata from the virglrenderer resource into the display path, including the DMA-BUF modifier and original allocation size.

Where a normal EGL DMA-BUF import is possible, QEMU continues to use it.

When the resource is a native Vulkan image whose layout cannot safely be reconstructed by the ordinary EGL path, the Helios display code can instead recreate the corresponding Vulkan image and import the DMA-BUF allocation directly.

For an optimal image, the Vulkan memory requirement is compared against the original allocation size. The fallback is accepted only when the reconstructed image matches the backing allocation exactly.

That check is important: blindly attaching a DMA-BUF to a Vulkan image with the wrong layout would make QEMU interpret unrelated bytes as pixels.

Once imported, the image can be copied into a host-visible staging resource for display backends that require CPU-accessible pixels, such as the VNC path.

Conceptually:

```text
Helios primary
     │
     ▼
Venus HOST3D blob
     │
     ▼
host Vulkan image
     │
     ▼
DMA-BUF
     │
     ├── importable layout ─────► EGL DMA-BUF scanout
     │
     └── native OPTIMAL layout
               │
               ▼
        Vulkan re-import
               │
               ▼
        staging/readback
               │
               ▼
        QEMU display surface
```

This does **not** introduce a new guest-facing `virtio-gpu` protocol.

Helios continues to use the normal `virtio-gpu` blob, scanout and resource-flush commands. The additional logic is on the host side and teaches QEMU how to correctly display resources whose native layout the normal scanout implementation cannot represent.

## Why not just use GPU passthrough? 🔀

Helios and PCI passthrough solve different problems.

With passthrough:

```text
Windows
   │
vendor GPU driver
   │
physical GPU
```

With Helios:

```text
Windows
   │
Helios + Venus
   │
virtio-gpu
   │
Linux graphics stack
   │
physical GPU
```

Passthrough provides the guest with the real hardware and therefore gives the highest degree of compatibility with vendor-specific functionality.

Helios deliberately does not expose the physical PCI GPU to Windows. The host remains in control of the device.

As a result, Helios is not intended to provide every feature of the native Windows NVIDIA, AMD or Intel driver. Vendor-specific technologies that require the real device and its proprietary Windows driver are outside the purpose of this architecture.

The advantage is that the same physical GPU can remain available to Linux while also accelerating the Windows guest.

## What this repository provides 📦

This repository does **not** contain the complete Helios Windows graphics driver. It provides the custom **host-side QEMU binary** required by the Helios graphics stack.

The Windows driver, Mesa Venus ICD, DXVK integration and other guest-side components live separately. This project only maintains the QEMU changes required to make the resulting `virtio-gpu` resources work correctly with the host display stack.

## Design 📦

The binary is based on upstream QEMU 11.1.0 plus the patch and source files stored directly in this repository.

QEMU loadable modules are disabled for this build. OpenGL/VirGL support is compiled into the executable so the custom binary does not depend on Debian's version-matched QEMU module files. Runtime graphics libraries remain dynamically linked and are supplied by the normal QEMU environment.

## Patch stack 🛠️

The build carries these changes on top of QEMU 11.1.0:

- Native Venus optimal scanout support
- SDL compositor EGL context validation
- Modifier-backed Venus scanout reconstruction
- Helios scanout tracing
- Optional HOST3D blob budgeting
- Non-power-of-two `virtio-gpu` host memory sizes
- Vulkan scanout publication pacing
- Adaptive VNC lossy-damage coalescing

### Native Venus optimal scanout

Carries the native Vulkan allocation metadata required to correctly present Helios scanout resources and provides the Vulkan readback path used when ordinary DMA-BUF/EGL import cannot describe the image layout.

### SDL EGL validation

Ensures SDL's OpenGL display path actually obtained an EGL-backed context before attempting DMA-BUF scanout. This is particularly important on Wayland and hybrid-GPU systems where the compositor and rendering Vulkan device may not use the same graphics stack.

### Modifier-backed reconstruction

Allows the native Vulkan reconstruction path to handle resources exported with a DRM modifier as well as modifier-less optimal images, while retaining strict validation of the underlying allocation.

### Scanout tracing

Adds tracing around blob layout, DMA-BUF identity and display reads so problems such as stale bindings, incorrect offsets or incomplete frames can be diagnosed at the actual QEMU scanout boundary.

### HOST3D blob budgeting

Adds optional accounting and limits for host-backed 3D blob allocations. This prevents an unbounded collection of HOST3D resources from consuming host GPU-visible memory.

### Non-power-of-two host memory

`virtio-gpu`'s shared-memory region does not inherently need to have a power-of-two size, but a PCI BAR does.

The patch keeps the requested `virtio-gpu` host-memory region at its real size while rounding only the containing PCI BAR to a valid power-of-two size.

### Vulkan publication pacing

Guest rendering and resource flushes can occur substantially faster than a remote display such as VNC can consume frames.

The patched `egl-headless` path therefore continues capturing the latest GPU state while pacing publication to the remote display. This avoids performing expensive GPU-to-CPU publication work for intermediate frames that VNC would never display anyway, while still preserving the newest and final frame.

### Adaptive VNC damage coalescing

Improves VNC's handling of rapidly changing lossy regions by coalescing damage more effectively instead of repeatedly encoding redundant intermediate updates.

## Acknowledgements 🙏

Special thanks to [TibixDev](https://github.com/TibixDev) and the [WinBoat](https://github.com/winboat-org/winboat) team, this project would not exist without their invaluable work.

[build_url]: https://github.com/qemus/qemu-helios/
[release_url]: https://github.com/qemus/qemu-helios/releases/

[Build]: https://github.com/qemus/qemu-helios/actions/workflows/build.yml/badge.svg
[Size]: https://img.shields.io/badge/size-18.4_MB-steelblue?style=flat&color=066da5
[Version]: https://img.shields.io/github/v/tag/qemus/qemu-helios?label=version&sort=semver&color=066da5
