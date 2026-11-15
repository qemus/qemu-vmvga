# VMVGA

VMVGA is a virtual graphics adapter for QEMU implementing the VMware SVGA/SVGA3D interfaces to provide GPU acceleration.

## Purpose 🖥️

QEMU’s stock VMware SVGA II device (`vmware-svga`) provides only a minimal implementation of the hardware. Most legacy 2D FIFO commands are missing, and VMware's 3D acceleration interfaces are not implemented.

This fork provides a substantially improved VMware SVGA II device, with both 2D and 3D graphics acceleration.

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

## Download 📦

You can download a [pre-built binary](https://github.com/qemus/qemu-vmvga/releases/latest/download/qemu-system-x86_64) based on QEMU 11.1.1 from the [Releases](https://github.com/qemus/qemu-vmvga/releases) page.

## Building 🔨

The source is designed to be overlaid onto a QEMU source tree before QEMU is built. Downstream projects can therefore fetch or vendor this repository, copy the files into their QEMU source tree, and then run their existing QEMU build process.

## Usage 🚀

Once included in QEMU, a new display device will be available:

```text
-device vmvga
```

3D acceleration requires the VMware SVGA display drivers installed in the guest, a Vulkan-capable graphics card, and the `dxvk-native` package installed on the host.

Then start the machine with the VMware backdoor port enabled:

```text
-machine type=xxx,vmport=on
```

Unfortunately KVM raises a general protection exception when the user-mode component of these drivers tries to access this VMware backdoor I/O port, causing the driver to disable 3D acceleration.

To work around this problem, add the following setting to your host KVM module configuration:

```sh
echo "options kvm enable_vmware_backdoor=Y" | sudo tee /etc/modprobe.d/kvm-vmware-backdoor.conf
sudo reboot
```

If KVM is built directly into the kernel rather than loaded as a module, add `kvm.enable_vmware_backdoor=1` to the kernel command line instead.

## Recommended drivers 💿

The following VMware SVGA driver versions are recommended when using VMVGA, as they were the only ones used during development and testing:

| Windows version | Recommended driver | VMware Tools |
|---|---|---|
| Windows 10 / 11 / Server 2016-2025 | 9.17.11 | [13.1.5](https://packages.vmware.com/tools/releases/13.1.5/windows/VMware-tools-windows-13.1.5-25544008.iso) |
| Windows 7 / 8 / Server 2008-2012 | 8.17.03 | [11.3.5](https://packages.vmware.com/tools/releases/11.3.5/windows/VMware-tools-windows-11.3.5-18557794.iso) |
| Windows Vista / Server 2008 | 8.16.01 | [10.3.10](https://packages.vmware.com/tools/releases/10.3.10/windows/VMware-tools-windows-10.3.10-12406962.iso) |
| Windows XP / Server 2003 | 12.01.08 | [10.0.12](https://packages.vmware.com/tools/frozen/windows/winPreVista.iso) |
| Windows 2000 | 11.09.01 | [10.0.12](https://packages.vmware.com/tools/frozen/windows/winPreVista.iso) |
| Windows 95 / 98 / ME | 10.7.0 | [7.7.0](https://packages.vmware.com/tools/frozen/windows/winPre2k.iso) |

## Acknowledgements 🙏

The implementation is derived from QEMU's VMware SVGA II device originally written by Andrzej Zaborowski and includes later work by [Christopher Eric Lentocha](https://github.com/CE1CECL).
