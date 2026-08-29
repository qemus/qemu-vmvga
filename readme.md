# QEMU VMVGA 🖥️
  
Enhanced VMware SVGA II (`vmware-svga`) graphics device implementation for QEMU.

## Purpose 🎯

QEMU’s stock VMware SVGA II device provides only a minimal implementation of the hardware. Most legacy 2D FIFO commands are missing, and 3D acceleration is also absent.

This fork provides a substantially more complete and compatible VMware SVGA II device,
 with both 2D and 3D graphics acceleration.

### 3D acceleration

The device supports accelerated Direct3D workloads from Direct3D 9 through Direct3D 11.

VMware SVGA 3D commands submitted by the guest are processed by the QEMU device and rendered through [DXVK](https://github.com/doitsujin/dxvk), which translates the Direct3D graphics operations to Vulkan on the host.

This extends the device beyond basic framebuffer and desktop acceleration, allowing compatible VMware display drivers to expose modern Direct3D functionality to the guest.

### Legacy 2D acceleration

The implementation also provides the full legacy 2D command stack used by VMware display drivers, including:

- Rectangle operations
- Raster operations
- Bitmap and pixmap patterns
- Glyph rendering
- Offscreen surfaces
- Alpha blending
- Hardware cursors
- Display updates

The surrounding VMware SVGA II implementation has also been improved substantially, including:

- PCI compatibility
- Register and FIFO behavior
- VRAM and surface-memory calculations
- Bounds checking
- Dirty-memory scanning
- Damage tracking
- Command batching
- Optimized pixel operations

Together, these improvements provide better VMware driver compatibility, more reliable rendering, accelerated 2D and 3D graphics, and more efficient display updates—particularly when QEMU’s VNC output is used.

## Building 🔨

The source is designed to be overlaid onto a QEMU source tree before QEMU is built. Downstream projects can therefore fetch or vendor this repository, copy the files into their QEMU source tree, and then run their existing QEMU build process.

The 3D implementation requires [DXVK](https://github.com/doitsujin/dxvk) and a Vulkan-capable host graphics stack.

## Usage 🚀

Once included in QEMU, the device is exposed using QEMU's existing VMware SVGA interface:

```text
-vga vmware
```

or:

```text
-device vmware-svga
```

The guest still requires a compatible VMware SVGA display driver to use the device-specific acceleration features.

## Acknowledgements 🙏

The implementation is derived from QEMU's VMware SVGA II device originally written by Andrzej Zaborowski and includes later work by [Christopher Eric Lentocha](https://github.com/CE1CECL).
