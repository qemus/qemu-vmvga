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

typedef enum vmsvga3d_dxvk_view_kind_e {
  VMSVGA3D_DXVK_VIEW_SHADER_RESOURCE = 0,
  VMSVGA3D_DXVK_VIEW_RENDER_TARGET,
  VMSVGA3D_DXVK_VIEW_DEPTH_STENCIL,
} VMSVGA3DDxvkViewKind;

typedef void (*VMSVGA3DDxvkSurfaceViewVisitor)(
    void *opaque, VMSVGA3DDxvkViewKind kind, uint32_t cid, uint32_t view_id);
struct vmsvga3d_d3d9_resource_plan_s;
struct vmsvga3d_d3d9_rect_s;
struct vmsvga3d_d3d9_transfer_surface_s;
struct vmsvga3d_d3d9_vertex_element_s;
struct vmsvga3d_d3d9_viewport_s;
struct vmsvga3d_d3d9_material_s;
struct vmsvga3d_d3d9_light_s;
struct vmsvga3d_d3d10_create_desc_s;
struct vmsvga3d_d3d10_rtv_desc_s;
struct vmsvga3d_d3d10_dsv_desc_s;
struct vmsvga3d_d3d10_srv_desc_s;
struct vmsvga3d_d3d10_box_s;
struct vmsvga3d_d3d10_input_element_s;
struct vmsvga3d_d3d10_shader_info_s;
struct vmsvga3d_d3d10_stream_output_plan_s;
struct vmsvga3d_d3d10_blend_desc_s;
struct vmsvga3d_d3d10_depth_stencil_desc_s;
struct vmsvga3d_d3d10_rasterizer_desc_s;
struct vmsvga3d_d3d10_sampler_desc_s;

typedef struct vmsvga3d_dxvk_subresource_data_s {
  const void *data;
  uint32_t row_pitch;
  uint32_t slice_pitch;
} VMSVGA3DDxvkSubresourceData;

typedef struct vmsvga3d_dxvk_d3d11_index_binding_s {
  void *buffer;
  uint32_t format;
  uint32_t offset;
} VMSVGA3DDxvkD3D11IndexBinding;

VMSVGA3DDxvk *vmsvga3d_dxvk_create(uint32_t width, uint32_t height,
                                    Error **errp);
void vmsvga3d_dxvk_destroy(VMSVGA3DDxvk *dxvk);
/* Legacy SVGA3D is available once the D3D9 runtime is ready. */
bool vmsvga3d_dxvk_ready(const VMSVGA3DDxvk *dxvk);
/* vGPU10/DX is an optional upgrade and is never attempted before D3D9. */
bool vmsvga3d_dxvk_d3d11_ready(const VMSVGA3DDxvk *dxvk);

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
bool vmsvga3d_dxvk_surface_generate_mipmaps(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t filter);
bool vmsvga3d_dxvk_d3d9_query_begin(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_type,
    uint32_t issue_flags);
bool vmsvga3d_dxvk_d3d9_query_end(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t issue_flags);
bool vmsvga3d_dxvk_d3d9_query_get_data(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t data_size,
    uint32_t flags, uint32_t *result);
void vmsvga3d_dxvk_d3d9_query_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid);

/* D3D11 residency is distinct from the legacy D3D9 resource path. */
bool vmsvga3d_dxvk_d3d11_surface_materialize(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_create_desc_s *desc,
    const VMSVGA3DDxvkSubresourceData *initial_data,
    uint32_t initial_data_count);
bool vmsvga3d_dxvk_d3d11_shader_resource_view_ensure(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_srv_desc_s *desc);
bool vmsvga3d_dxvk_d3d11_shader_resource_view_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id);
bool vmsvga3d_dxvk_d3d11_render_target_view_ensure(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_rtv_desc_s *desc);
bool vmsvga3d_dxvk_d3d11_render_target_view_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id);
bool vmsvga3d_dxvk_d3d11_depth_stencil_view_ensure(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_dsv_desc_s *desc);
bool vmsvga3d_dxvk_d3d11_depth_stencil_view_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id);
bool vmsvga3d_dxvk_d3d11_generate_mips(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id);
void vmsvga3d_dxvk_d3d11_view_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid);
void vmsvga3d_dxvk_d3d11_surface_invalidate_views(
    VMSVGA3DDxvkSurface *surface);
void vmsvga3d_dxvk_d3d11_surface_visit_views(
    VMSVGA3DDxvkSurface *surface, VMSVGA3DDxvkSurfaceViewVisitor visitor,
    void *opaque);
bool vmsvga3d_dxvk_d3d11_surface_resident(
    const VMSVGA3DDxvkSurface *surface);
bool vmsvga3d_dxvk_d3d11_copy_subresource_region(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *destination,
    uint32_t destination_subresource, uint32_t destination_x,
    uint32_t destination_y, uint32_t destination_z,
    VMSVGA3DDxvkSurface *source, uint32_t source_subresource,
    const struct vmsvga3d_d3d10_box_s *source_box);
bool vmsvga3d_dxvk_d3d11_copy_resource(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *destination,
    VMSVGA3DDxvkSurface *source);
bool vmsvga3d_dxvk_d3d11_resolve_subresource(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *destination,
    uint32_t destination_subresource, VMSVGA3DDxvkSurface *source,
    uint32_t source_subresource, uint32_t format);
bool vmsvga3d_dxvk_d3d11_update_subresource(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t subresource,
    const struct vmsvga3d_d3d10_box_s *box, const void *data,
    uint32_t row_pitch, uint32_t depth_pitch);
bool vmsvga3d_dxvk_d3d11_readback_subresource(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t subresource,
    void *data, uint32_t row_bytes, uint32_t row_pitch, uint32_t row_count,
    uint32_t depth_pitch, uint32_t depth_count);
bool vmsvga3d_dxvk_d3d11_constant_buffer_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t slot, const void *data, uint32_t size);
bool vmsvga3d_dxvk_d3d11_constant_buffer_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t slot);
bool vmsvga3d_dxvk_d3d11_set_constant_buffers(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t start_slot, uint32_t buffer_count);
bool vmsvga3d_dxvk_d3d11_set_shader_resources(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t start_slot, uint32_t view_count, const uint32_t *view_ids);
bool vmsvga3d_dxvk_d3d11_set_vertex_buffers(
    VMSVGA3DDxvk *dxvk, uint32_t start_slot, uint32_t buffer_count,
    VMSVGA3DDxvkSurface *const *surfaces, const uint32_t *strides,
    const uint32_t *offsets);
bool vmsvga3d_dxvk_d3d11_set_stream_output_targets(
    VMSVGA3DDxvk *dxvk,
    VMSVGA3DDxvkSurface *const surfaces[SVGA3D_DX_MAX_SOTARGETS],
    const uint32_t offsets[SVGA3D_DX_MAX_SOTARGETS]);
bool vmsvga3d_dxvk_d3d11_set_index_buffer(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    uint32_t format, uint32_t offset);
bool vmsvga3d_dxvk_d3d11_create_immutable_index_buffer(
    VMSVGA3DDxvk *dxvk, const void *indices, uint32_t size,
    void **buffer);
bool vmsvga3d_dxvk_d3d11_get_index_buffer(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkD3D11IndexBinding *binding);
bool vmsvga3d_dxvk_d3d11_read_index_buffer(
    VMSVGA3DDxvk *dxvk, void *buffer, uint32_t offset, uint32_t bytes,
    void **data, uint32_t *data_bytes);
bool vmsvga3d_dxvk_d3d11_set_native_index_buffer(
    VMSVGA3DDxvk *dxvk, void *buffer, uint32_t format, uint32_t offset);
void vmsvga3d_dxvk_d3d11_release_index_buffer(void *buffer);
bool vmsvga3d_dxvk_d3d11_draw(
    VMSVGA3DDxvk *dxvk, uint32_t vertex_count,
    uint32_t start_vertex_location);
bool vmsvga3d_dxvk_d3d11_draw_indexed(
    VMSVGA3DDxvk *dxvk, uint32_t index_count,
    uint32_t start_index_location, int32_t base_vertex_location);
bool vmsvga3d_dxvk_d3d11_draw_instanced(
    VMSVGA3DDxvk *dxvk, uint32_t vertex_count_per_instance,
    uint32_t instance_count, uint32_t start_vertex_location,
    uint32_t start_instance_location);
bool vmsvga3d_dxvk_d3d11_draw_indexed_instanced(
    VMSVGA3DDxvk *dxvk, uint32_t index_count_per_instance,
    uint32_t instance_count, uint32_t start_index_location,
    int32_t base_vertex_location, uint32_t start_instance_location);
bool vmsvga3d_dxvk_d3d11_draw_auto(VMSVGA3DDxvk *dxvk);
bool vmsvga3d_dxvk_d3d11_present_blt(
    VMSVGA3DDxvk *dxvk,
    VMSVGA3DDxvkSurface *source, uint32_t source_subresource,
    uint32_t source_format, const SVGA3dBox *source_box,
    const SVGA3dSize *source_size, bool source_srgb,
    VMSVGA3DDxvkSurface *destination, uint32_t destination_subresource,
    uint32_t destination_format, const SVGA3dBox *destination_box,
    const SVGA3dSize *destination_size);
void vmsvga3d_dxvk_d3d11_constant_buffer_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid);
bool vmsvga3d_dxvk_d3d11_blend_state_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const struct vmsvga3d_d3d10_blend_desc_s *desc);
bool vmsvga3d_dxvk_d3d11_blend_state_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id);
bool vmsvga3d_dxvk_d3d11_depth_stencil_state_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const struct vmsvga3d_d3d10_depth_stencil_desc_s *desc);
bool vmsvga3d_dxvk_d3d11_depth_stencil_state_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id);
bool vmsvga3d_dxvk_d3d11_rasterizer_state_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const struct vmsvga3d_d3d10_rasterizer_desc_s *desc);
bool vmsvga3d_dxvk_d3d11_rasterizer_state_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id);
bool vmsvga3d_dxvk_d3d11_sampler_state_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const struct vmsvga3d_d3d10_sampler_desc_s *desc);
bool vmsvga3d_dxvk_d3d11_sampler_state_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id);
bool vmsvga3d_dxvk_d3d11_set_blend_state(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const float blend_factor[4], uint32_t sample_mask);
bool vmsvga3d_dxvk_d3d11_set_depth_stencil_state(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    uint32_t stencil_ref);
bool vmsvga3d_dxvk_d3d11_set_rasterizer_state(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id);
bool vmsvga3d_dxvk_d3d11_set_samplers(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t start_slot, uint32_t sampler_count, const uint32_t *state_ids);
bool vmsvga3d_dxvk_d3d11_set_primitive_topology(
    VMSVGA3DDxvk *dxvk, uint32_t topology);
bool vmsvga3d_dxvk_d3d11_set_viewports(
    VMSVGA3DDxvk *dxvk, uint32_t viewport_count, const void *viewports);
bool vmsvga3d_dxvk_d3d11_set_scissor_rects(
    VMSVGA3DDxvk *dxvk, uint32_t rect_count, const void *rects);
void vmsvga3d_dxvk_d3d11_state_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid);
bool vmsvga3d_dxvk_d3d11_input_layout_ensure(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t layout_id,
    uint32_t shader_id,
    const struct vmsvga3d_d3d10_input_element_s *elements,
    uint32_t element_count);
bool vmsvga3d_dxvk_d3d11_set_input_layout(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t layout_id);
bool vmsvga3d_dxvk_d3d11_input_layout_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t layout_id);
void vmsvga3d_dxvk_d3d11_input_layout_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid);
bool vmsvga3d_dxvk_d3d11_shader_object_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type);
bool vmsvga3d_dxvk_d3d11_shader_object_exists(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t *shader_type);
bool vmsvga3d_dxvk_d3d11_shader_bind_info(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    struct vmsvga3d_d3d10_shader_info_s *info);
bool vmsvga3d_dxvk_d3d11_shader_info_for_realize(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type, struct vmsvga3d_d3d10_shader_info_s **info);
bool vmsvga3d_dxvk_d3d11_shader_info(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type, const struct vmsvga3d_d3d10_shader_info_s **info);
bool vmsvga3d_dxvk_d3d11_stream_output_cached(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stream_output_id,
    struct vmsvga3d_d3d10_stream_output_plan_s *plan);
bool vmsvga3d_dxvk_d3d11_stream_output_cache(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stream_output_id,
    const struct vmsvga3d_d3d10_stream_output_plan_s *plan);
bool vmsvga3d_dxvk_d3d11_stream_output_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stream_output_id);
bool vmsvga3d_dxvk_d3d11_shader_realize(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t stream_output_id,
    const struct vmsvga3d_d3d10_stream_output_plan_s *stream_output);
bool vmsvga3d_dxvk_d3d11_shader_set(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type);
bool vmsvga3d_dxvk_d3d11_shader_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id);
bool vmsvga3d_dxvk_d3d11_shader_bytecode(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type, const void **bytecode, uint32_t *bytecode_size);
void vmsvga3d_dxvk_d3d11_shader_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid);
bool vmsvga3d_dxvk_d3d11_query_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id,
    uint32_t d3d_query, uint32_t misc_flags);
bool vmsvga3d_dxvk_d3d11_query_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id);
bool vmsvga3d_dxvk_d3d11_query_exists(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id);
bool vmsvga3d_dxvk_d3d11_query_begin(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, bool issue_begin);
bool vmsvga3d_dxvk_d3d11_query_end(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, bool issue_end);
bool vmsvga3d_dxvk_d3d11_query_get_data(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, void *data,
    uint32_t data_size, uint32_t getdata_flags, bool *ready);
bool vmsvga3d_dxvk_d3d11_query_pending(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id);
bool vmsvga3d_dxvk_d3d11_set_predication(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, bool enabled,
    bool predicate_value);
void vmsvga3d_dxvk_d3d11_query_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid);
bool vmsvga3d_dxvk_d3d11_clear_render_target_view(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    const float color[4]);
bool vmsvga3d_dxvk_d3d11_clear_depth_stencil_view(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    uint32_t clear_flags, float depth, uint8_t stencil);
bool vmsvga3d_dxvk_d3d11_set_render_targets(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t render_target_count,
    const uint32_t *render_target_ids, uint32_t depth_stencil_view_id,
    uint32_t uav_start_slot);
void vmsvga3d_dxvk_surface_evict(VMSVGA3DDxvkSurface *surface);
void vmsvga3d_dxvk_surface_set_renderer(VMSVGA3DDxvkSurface *surface,
                                        VMSVGA3DDxvk *dxvk);
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
