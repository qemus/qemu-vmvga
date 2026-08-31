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
struct vmsvga3d_d3d9_rect_s;
struct vmsvga3d_d3d9_transfer_surface_s;
struct vmsvga3d_d3d9_vertex_element_s;
struct vmsvga3d_d3d9_viewport_s;
struct vmsvga3d_d3d9_material_s;
struct vmsvga3d_d3d9_light_s;
struct vmsvga3d_d3d10_create_desc_s;
struct vmsvga3d_d3d10_rtv_desc_s;

typedef struct vmsvga3d_dxvk_subresource_data_s {
  const void *data;
  uint32_t row_pitch;
  uint32_t slice_pitch;
} VMSVGA3DDxvkSubresourceData;

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

/* D3D11 residency is distinct from the legacy D3D9 resource path. */
bool vmsvga3d_dxvk_d3d11_surface_materialize(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_create_desc_s *desc,
    const VMSVGA3DDxvkSubresourceData *initial_data,
    uint32_t initial_data_count);
bool vmsvga3d_dxvk_d3d11_clear_render_target_view(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_rtv_desc_s *desc, const float color[4]);
void vmsvga3d_dxvk_surface_evict(VMSVGA3DDxvkSurface *surface);
bool vmsvga3d_dxvk_surface_upload_level(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t level,
    const void *data, uint32_t row_bytes, uint32_t rows);
bool vmsvga3d_dxvk_surface_readback_level(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t level,
    void *data, uint32_t row_bytes, uint32_t rows);
bool vmsvga3d_dxvk_surface_upload_buffer(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const void *data, uint32_t size);
bool vmsvga3d_dxvk_surface_stretch_rect(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *source,
    uint32_t source_level, const struct vmsvga3d_d3d9_rect_s *source_rect,
    VMSVGA3DDxvkSurface *destination, uint32_t destination_level,
    const struct vmsvga3d_d3d9_rect_s *destination_rect, uint32_t filter);
bool vmsvga3d_dxvk_clear(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *const color_targets[8],
    const uint32_t color_levels[8], VMSVGA3DDxvkSurface *depth_stencil,
    uint32_t depth_stencil_level,
    const struct vmsvga3d_d3d9_rect_s *rects, uint32_t rect_count,
    const struct vmsvga3d_d3d9_rect_s *clear_scissor, uint32_t flags,
    uint32_t color, float depth, uint32_t stencil);

/* Concrete D3D9 draw execution used by the VGPU9 bridge. */
bool vmsvga3d_dxvk_reset_state(VMSVGA3DDxvk *dxvk);
bool vmsvga3d_dxvk_set_render_target(VMSVGA3DDxvk *dxvk, uint32_t index,
                                     VMSVGA3DDxvkSurface *surface,
                                     uint32_t level);
bool vmsvga3d_dxvk_set_depth_stencil(VMSVGA3DDxvk *dxvk,
                                     VMSVGA3DDxvkSurface *surface,
                                     uint32_t level);
bool vmsvga3d_dxvk_set_render_state(VMSVGA3DDxvk *dxvk, uint32_t state,
                                    uint32_t value);
bool vmsvga3d_dxvk_set_texture(VMSVGA3DDxvk *dxvk, uint32_t stage,
                               VMSVGA3DDxvkSurface *surface);
bool vmsvga3d_dxvk_set_texture_stage_state(VMSVGA3DDxvk *dxvk,
                                           uint32_t stage, uint32_t state,
                                           uint32_t value);
bool vmsvga3d_dxvk_set_sampler_state(VMSVGA3DDxvk *dxvk, uint32_t sampler,
                                     uint32_t state, uint32_t value);
bool vmsvga3d_dxvk_set_transform(VMSVGA3DDxvk *dxvk, uint32_t type,
                                 const float matrix[16]);
bool vmsvga3d_dxvk_set_viewport(
    VMSVGA3DDxvk *dxvk, const struct vmsvga3d_d3d9_viewport_s *viewport);
bool vmsvga3d_dxvk_set_scissor(
    VMSVGA3DDxvk *dxvk, const struct vmsvga3d_d3d9_rect_s *rect);
bool vmsvga3d_dxvk_set_material(
    VMSVGA3DDxvk *dxvk, const struct vmsvga3d_d3d9_material_s *material);
bool vmsvga3d_dxvk_set_light(
    VMSVGA3DDxvk *dxvk, uint32_t index,
    const struct vmsvga3d_d3d9_light_s *light);
bool vmsvga3d_dxvk_light_enable(VMSVGA3DDxvk *dxvk, uint32_t index,
                                bool enabled);
bool vmsvga3d_dxvk_set_clip_plane(VMSVGA3DDxvk *dxvk, uint32_t index,
                                  const float plane[4]);
void *vmsvga3d_dxvk_shader_create(VMSVGA3DDxvk *dxvk, uint32_t stage,
                                  const uint32_t *bytecode);
void vmsvga3d_dxvk_shader_destroy(void *shader);
bool vmsvga3d_dxvk_shader_bind(VMSVGA3DDxvk *dxvk, uint32_t stage,
                               void *shader);
bool vmsvga3d_dxvk_shader_constant(VMSVGA3DDxvk *dxvk, uint32_t target,
                                   uint32_t reg, const uint32_t values[4]);
void *vmsvga3d_dxvk_vertex_declaration_create(
    VMSVGA3DDxvk *dxvk,
    const struct vmsvga3d_d3d9_vertex_element_s *elements);
void vmsvga3d_dxvk_vertex_declaration_destroy(void *declaration);
bool vmsvga3d_dxvk_vertex_declaration_bind(VMSVGA3DDxvk *dxvk,
                                           void *declaration);
bool vmsvga3d_dxvk_set_stream_source(VMSVGA3DDxvk *dxvk, uint32_t stream,
                                     VMSVGA3DDxvkSurface *surface,
                                     uint32_t offset, uint32_t stride);
bool vmsvga3d_dxvk_set_stream_frequency(VMSVGA3DDxvk *dxvk,
                                        uint32_t stream, uint32_t frequency);
bool vmsvga3d_dxvk_set_indices(VMSVGA3DDxvk *dxvk,
                               VMSVGA3DDxvkSurface *surface);
bool vmsvga3d_dxvk_begin_scene(VMSVGA3DDxvk *dxvk);
bool vmsvga3d_dxvk_end_scene(VMSVGA3DDxvk *dxvk);
bool vmsvga3d_dxvk_draw_primitive(VMSVGA3DDxvk *dxvk,
                                  uint32_t primitive_type,
                                  uint32_t start_vertex,
                                  uint32_t primitive_count);
bool vmsvga3d_dxvk_draw_indexed_primitive(
    VMSVGA3DDxvk *dxvk, uint32_t primitive_type, int32_t base_vertex_index,
    uint32_t min_vertex_index, uint32_t num_vertices, uint32_t start_index,
    uint32_t primitive_count);

#endif
