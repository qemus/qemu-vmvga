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

#ifndef HW_DISPLAY_VMWARE_VGA_DXVK_H
#define HW_DISPLAY_VMWARE_VGA_DXVK_H

#include <stdbool.h>
#include <stdint.h>

#include "qapi/error.h"

typedef struct vmsvga3d_dxvk_s VMSVGA3DDxvk;
typedef struct vmsvga3d_dxvk_surface_s VMSVGA3DDxvkSurface;
struct vmsvga3d_d3d9_resource_plan_s;
struct vmsvga3d_d3d9_transfer_surface_s;

VMSVGA3DDxvk *vmsvga3d_dxvk_create(uint32_t width, uint32_t height,
                                    Error **errp);
void vmsvga3d_dxvk_destroy(VMSVGA3DDxvk *dxvk);
bool vmsvga3d_dxvk_ready(const VMSVGA3DDxvk *dxvk);

/* Guest surface lifetime is tracked immediately; D3D9 residency is lazy. */
VMSVGA3DDxvkSurface *vmsvga3d_dxvk_surface_create(VMSVGA3DDxvk *dxvk,
                                                    uint32_t sid);
void vmsvga3d_dxvk_surface_destroy(VMSVGA3DDxvkSurface *surface);
bool vmsvga3d_dxvk_surface_info(
    const VMSVGA3DDxvkSurface *surface,
    struct vmsvga3d_d3d9_transfer_surface_s *info);
bool vmsvga3d_dxvk_surface_materialize(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d9_resource_plan_s *plan);
void vmsvga3d_dxvk_surface_evict(VMSVGA3DDxvkSurface *surface);
bool vmsvga3d_dxvk_surface_upload_level(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t level,
    const void *data, uint32_t row_bytes, uint32_t rows);
bool vmsvga3d_dxvk_surface_readback_level(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t level,
    void *data, uint32_t row_bytes, uint32_t rows);

#endif
