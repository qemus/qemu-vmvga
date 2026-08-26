# QEMU VMVGA 🖥️
  
Enhanced VMware SVGA II (`vmware-svga`) device implementation for QEMU.

## Purpose 🎯

QEMU’s stock VMware SVGA II device provides only a minimal implementation of the hardware, and most legacy 2D FIFO commands are missing. This fork provides a substantially more complete and compatible VMware SVGA II device.

It implements the full legacy 2D command stack used by VMware display drivers, including rectangle operations, raster operations, bitmap and pixmap patterns, glyph rendering, offscreen surfaces, alpha blending, hardware cursors, and display updates.

It also improves the parts surrounding those commands: PCI compatibility, register and FIFO behavior, VRAM and surface-memory calculations, bounds checking, dirty-memory scanning, damage tracking, command batching, and optimized pixel operations.

The result is better driver compatibility, more reliable rendering, and more efficient display updates—particularly when QEMU’s VNC output is used.

## Building 🔨

The source is designed to be overlaid onto a QEMU source tree before QEMU is built. Downstream projects can therefore fetch or vendor this repository, copy the files into their QEMU source tree, and then run their existing QEMU build process.

## Usage 🚀

Once included in QEMU, the device is exposed using QEMU's existing VMware SVGA interface:

```text
-vga vmware
```

or:

```text
-device vmware-svga
```

The guest still requires a compatible VMware SVGA display driver to use device-specific features.

## Acknowledgements 🙏

The implementation is derived from QEMU's VMware SVGA II device originally written by Andrzej Zaborowski and includes later work by [Christopher Eric Lentocha](https://github.com/CE1CECL).
