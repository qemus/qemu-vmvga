# QEMU VMVGA 🖥️
  
Enhanced VMware SVGA II (`vmware-svga`) device implementation for QEMU.

This repository contains replacement QEMU display-device source code and the VMware SVGA headers it depends on. It is intended to be integrated into a QEMU source tree and built as part of QEMU; it does not produce a standalone guest driver or executable.

## Purpose 🎯

QEMU includes a VMware SVGA II compatible display device in `hw/display/vmware_vga.c`. This fork extends that implementation with additional VMware SVGA registers, FIFO commands, capability handling, cursor and display behavior, and newer VMware SVGA definitions.

The goal is to provide a more complete `vmware-svga` implementation for QEMU builds that need broader compatibility with VMware guest display drivers, including legacy Windows driver paths.

## Repository layout 📁

- `hw/display/vmware_vga.c` — QEMU VMware SVGA II device implementation.
- `hw/display/include/` — VMware SVGA Device Developer Kit headers used by the implementation.

## Integration 🔗

The source is designed to be overlaid onto a QEMU source tree before QEMU is built.

For example:

```sh
VMVGA=/path/to/qemu-vmvga
QEMU=/path/to/qemu

install -m 0644 "$VMVGA/hw/display/vmware_vga.c" "$QEMU/hw/display/vmware_vga.c"
mkdir -p "$QEMU/hw/display/include"
cp -a "$VMVGA/hw/display/include/." "$QEMU/hw/display/include/"
```

Downstream projects can therefore fetch or vendor this repository, copy the files into their QEMU source tree, and then run their existing QEMU build process.

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

## Building 🔨

This repository intentionally does not need to publish its own QEMU binary or container image. The authoritative build and runtime validation should happen in the downstream QEMU projects that consume this source, so the VMVGA code is tested together with the exact QEMU version and build configuration in which it will be used.

## Acknowledgements 🙏

The implementation is derived from QEMU's VMware SVGA II device originally written by Andrzej Zaborowski and includes later work by [Christopher Eric Lentocha](https://github.com/CE1CECL).
