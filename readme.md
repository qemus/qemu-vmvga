<div align="center"><a href="https://github.com/qemus/qemu-vmvga"><img src="https://github.com/qemus/qemu-vmvga/raw/master/.github/logo.png" title="Logo" style="max-width:100%;" width="256" /></a>
</div>
<div align="center">
<br/>
</div>

VMVGA is a virtual graphics adapter for QEMU implementing the VMware SVGA/SVGA3D interfaces to provide GPU acceleration.

## Purpose 🖥️

QEMU’s stock VMware SVGA II device (`vmware-svga`) provides only a minimal implementation of the hardware. Most legacy 2D FIFO commands are missing, and VMware's 3D acceleration interfaces are not implemented.

This fork provides a substantially more complete and compatible VMware SVGA II device, with both 2D and 3D graphics acceleration.

## Features ✨

The core device implementation has been improved substantially, including:

- PCI compatibility
- Register and FIFO behavior
- VRAM and surface-memory
- Dirty-memory scanning
- Damage tracking
- Command batching
- Optimized pixel operations

Together, these improvements provide better VMware driver compatibility, more reliable rendering, and more efficient display updates.

### 3D acceleration

The device supports DirectX acceleration. VMware SVGA 3D commands are processed by the QEMU device and rendered through [DXVK](https://github.com/doitsujin/dxvk), which translates the Direct3D graphics operations to Vulkan on the host.

It requires a Vulkan-capable graphics card, and the DXVK package to be present on the host, otherwise it automatically falls back to 2D acceleration.

### 2D acceleration

The implementation provides the full legacy 2D command stack used by VMware display drivers, including:

- Rectangle operations
- Raster operations
- Bitmap and pixmap patterns
- Glyph rendering
- Offscreen surfaces
- Alpha blending
- Hardware cursors
- Display updates

These commands provide substantially improved compatibility with legacy VMware display drivers and enable accelerated desktop rendering, particularly when QEMU's VNC output is used.

## Building 🔨

The source is designed to be overlaid onto a QEMU source tree before QEMU is built. Downstream projects can therefore fetch or vendor this repository, copy the files into their QEMU source tree, and then run their existing QEMU build process.

## Usage 🚀

Once included in QEMU, a new display device will be available:

```text
-device vmvga
```

3D acceleration requires the official VMware SVGA display drivers to be installed in the guest, on modern Windows versions they will be automatically retrieved via Windows Update.

Unfortunately KVM raises a general protection exception when the user-mode component of these drivers tries to access the VMware backdoor I/O port, causing the driver to disable 3D acceleration.

To work around this problem, add the following setting to your host KVM module configuration:

```sh
echo "options kvm enable_vmware_backdoor=Y" | sudo tee /etc/modprobe.d/kvm-vmware-backdoor.conf
sudo reboot
```

If KVM is built directly into the kernel rather than loaded as a module, add `kvm.enable_vmware_backdoor=1` to the kernel command line instead.

## Acknowledgements 🙏

The implementation is derived from QEMU's VMware SVGA II device originally written by Andrzej Zaborowski and includes later work by [Christopher Eric Lentocha](https://github.com/CE1CECL).
