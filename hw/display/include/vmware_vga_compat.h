/*

 QEMU VMware Super Video Graphics Array 2 [SVGA-II]

 Copyright (c) 2026 QEMU VMVGA (https://github.com/qemus/qemu-vmvga)

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

*/

#ifndef QEMU_VMVGA_COMPAT_H
#define QEMU_VMVGA_COMPAT_H

/*
 * qemu-vmvga replaces an internal QEMU device implementation, so it has to
 * adapt to QEMU internal API changes. Keep those differences here instead of
 * spreading version checks throughout vmware_vga.c.
 *
 * The supported QEMU API families are 7.x, 9.x and 11.x.
 */
#ifndef QEMU_VERSION_MAJOR
#error "qemu-vmvga requires QEMU_VERSION_MAJOR from QEMU's build configuration"
#endif

#if QEMU_VERSION_MAJOR == 7
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#elif QEMU_VERSION_MAJOR == 9
#include "hw/pci/pci_device.h"
#include "hw/qdev-properties.h"
#elif QEMU_VERSION_MAJOR == 11
#include "hw/pci/pci_device.h"
#include "hw/core/qdev-properties.h"
#else
#error "qemu-vmvga supports QEMU major versions 7, 9 and 11"
#endif

#include "qemu/module.h"
#include "qom/object.h"
#include "ui/console.h"

static inline void vmvga_console_mouse_set(QemuConsole *con, int x, int y,
                                            bool on)
{
#if QEMU_VERSION_MAJOR == 11 && QEMU_VERSION_MINOR >= 1
    qemu_console_set_mouse(con, x, y, on);
#else
    dpy_mouse_set(con, x, y, on);
#endif
}

static inline void vmvga_console_update(QemuConsole *con, int x, int y,
                                        int w, int h)
{
#if QEMU_VERSION_MAJOR == 11 && QEMU_VERSION_MINOR >= 1
    qemu_console_update(con, x, y, w, h);
#else
    dpy_gfx_update(con, x, y, w, h);
#endif
}

static inline void vmvga_console_set_cursor(QemuConsole *con,
                                            QEMUCursor *cursor)
{
#if QEMU_VERSION_MAJOR == 11 && QEMU_VERSION_MINOR >= 1
    qemu_console_set_cursor(con, cursor);
#else
    dpy_cursor_define(con, cursor);
#endif
}

static inline void vmvga_console_set_surface(QemuConsole *con,
                                             DisplaySurface *surface)
{
#if QEMU_VERSION_MAJOR == 11 && QEMU_VERSION_MINOR >= 1
    qemu_console_set_surface(con, surface);
#else
    dpy_gfx_replace_surface(con, surface);
#endif
}

static inline QemuConsole *vmvga_graphic_console_create(
    DeviceState *dev, uint32_t head, const GraphicHwOps *ops, void *opaque)
{
#if QEMU_VERSION_MAJOR == 11 && QEMU_VERSION_MINOR >= 1
    return qemu_graphic_console_create(dev, head, ops, opaque);
#else
    return graphic_console_init(dev, head, ops, opaque);
#endif
}

static inline void vmvga_cursor_unref(QEMUCursor *cursor)
{
#if QEMU_VERSION_MAJOR == 7
    cursor_put(cursor);
#else
    cursor_unref(cursor);
#endif
}

#if QEMU_VERSION_MAJOR == 7
#define VMVGA_REGISTER_VGA_VMSTATE(_vga) \
    vmstate_register(NULL, 0, &vmstate_vga_common, (_vga))
#define VMVGA_SET_LEGACY_RESET(_dc, _reset) ((_dc)->reset = (_reset))
#else
#define VMVGA_REGISTER_VGA_VMSTATE(_vga) \
    vmstate_register_any(NULL, &vmstate_vga_common, (_vga))
#define VMVGA_SET_LEGACY_RESET(_dc, _reset) \
    device_class_set_legacy_reset((_dc), (_reset))
#endif

/*
 * QEMU 11.0 already has the const Property/class_init API and no longer uses
 * DEFINE_PROP_END_OF_LIST().  The display callback API changes below only
 * arrive in QEMU 11.1, so keep these compatibility boundaries separate.
 */
#if QEMU_VERSION_MAJOR == 11
#define VMVGA_PROPERTY_QUALIFIER const
#define VMVGA_PROPERTY_END
#define VMVGA_CLASS_INIT_DATA const void *
#define VMVGA_GLOBAL_VMSTATE_PROPERTY(_state, _field)
#else
#define VMVGA_PROPERTY_QUALIFIER
#define VMVGA_PROPERTY_END DEFINE_PROP_END_OF_LIST(),
#define VMVGA_CLASS_INIT_DATA void *
#define VMVGA_GLOBAL_VMSTATE_PROPERTY(_state, _field) \
    DEFINE_PROP_BOOL("global-vmstate", _state, _field, false),
#endif

#if QEMU_VERSION_MAJOR == 11 && QEMU_VERSION_MINOR >= 1
#define VMVGA_GFX_UPDATE_RET bool
#define VMVGA_GFX_UPDATE_FALLBACK(_s) \
    return (_s)->vga.hw_ops->gfx_update(&(_s)->vga)
#define VMVGA_GFX_UPDATE_DONE() return true
#else
#define VMVGA_GFX_UPDATE_RET void
#define VMVGA_GFX_UPDATE_FALLBACK(_s) \
    do { \
        (_s)->vga.hw_ops->gfx_update(&(_s)->vga); \
        return; \
    } while (0)
#define VMVGA_GFX_UPDATE_DONE() return
#endif

#endif
