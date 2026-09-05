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

#ifndef HW_DISPLAY_VMWARE_VGA_D3D11_H
#define HW_DISPLAY_VMWARE_VGA_D3D11_H

#include <stdbool.h>
#include <stdint.h>

#include "vmware_vga_d3d10.h"

/*
 * Pure vGPU11 -> Direct3D 11.x translation.
 *
 * D3D10-compatible protocol objects are intentionally translated by the
 * D3D10 module.  This file only owns D3D11 additions and the small amount of
 * post-processing needed to produce complete D3D11 descriptors.
 */

typedef enum vmsvga3d_d3d11_level_e {
    VMSVGA3D_D3D11_LEVEL_INVALID = 0,
    VMSVGA3D_D3D11_LEVEL_11_0,
    VMSVGA3D_D3D11_LEVEL_11_1,
} VMSVGA3DD3D11Level;

typedef struct vmsvga3d_d3d11_format_s {
    uint32_t dxgi_format;
    VMSVGA3DD3D11Level min_level;
} VMSVGA3DD3D11Format;

#define VMSVGA3D_D3D11_INPUT_SEMANTIC VMSVGA3D_D3D10_INPUT_SEMANTIC

typedef VMSVGA3DD3D10InputElement VMSVGA3DD3D11InputElement;
typedef VMSVGA3DD3D10StencilFace VMSVGA3DD3D11StencilFace;
typedef VMSVGA3DD3D10DepthStencilDesc VMSVGA3DD3D11DepthStencilDesc;
typedef VMSVGA3DD3D10SamplerDesc VMSVGA3DD3D11SamplerDesc;
typedef VMSVGA3DD3D10RTVDesc VMSVGA3DD3D11RTVDesc;
typedef VMSVGA3DD3D10QueryInfo VMSVGA3DD3D11QueryInfo;

typedef struct vmsvga3d_d3d11_rt_blend_s {
    uint32_t blend_enable;
    uint32_t logic_op_enable;
    uint32_t src_blend;
    uint32_t dest_blend;
    uint32_t blend_op;
    uint32_t src_blend_alpha;
    uint32_t dest_blend_alpha;
    uint32_t blend_op_alpha;
    uint32_t logic_op;
    uint8_t write_mask;
} VMSVGA3DD3D11RTBlend;

typedef struct vmsvga3d_d3d11_blend_desc_s {
    uint32_t alpha_to_coverage_enable;
    uint32_t independent_blend_enable;
    VMSVGA3DD3D11RTBlend render_target[SVGA3D_DX_MAX_RENDER_TARGETS];
} VMSVGA3DD3D11BlendDesc;

typedef struct vmsvga3d_d3d11_rasterizer_desc_s {
    uint32_t fill_mode;
    uint32_t cull_mode;
    uint32_t front_counter_clockwise;
    int32_t depth_bias;
    float depth_bias_clamp;
    float slope_scaled_depth_bias;
    uint32_t depth_clip_enable;
    uint32_t scissor_enable;
    uint32_t multisample_enable;
    uint32_t antialiased_line_enable;
    uint32_t forced_sample_count;
} VMSVGA3DD3D11RasterizerDesc;

typedef struct vmsvga3d_d3d11_resource_policy_s {
    uint32_t usage;
    uint32_t bind_flags;
    uint32_t cpu_access_flags;
    uint32_t misc_flags;
    uint32_t structure_byte_stride;
} VMSVGA3DD3D11ResourcePolicy;

typedef struct vmsvga3d_d3d11_srv_desc_s {
    uint32_t format;
    uint32_t view_dimension;
    uint32_t most_detailed_mip;
    uint32_t mip_levels;
    uint32_t first_array_slice;
    uint32_t array_size;
    uint32_t first_element;
    uint32_t num_elements;
    uint32_t flags;
} VMSVGA3DD3D11SRVDesc;

typedef struct vmsvga3d_d3d11_dsv_desc_s {
    uint32_t format;
    uint32_t view_dimension;
    uint32_t flags;
    uint32_t mip_slice;
    uint32_t first_array_slice;
    uint32_t array_size;
} VMSVGA3DD3D11DSVDesc;

typedef struct vmsvga3d_d3d11_uav_desc_s {
    uint32_t format;
    uint32_t view_dimension;
    uint32_t mip_slice;
    uint32_t first_array_slice;
    uint32_t array_size;
    uint32_t first_w_slice;
    uint32_t w_size;
    uint32_t first_element;
    uint32_t num_elements;
    uint32_t flags;
} VMSVGA3DD3D11UAVDesc;

typedef struct vmsvga3d_d3d11_uav_define_plan_s {
    SVGA3dUAViewId view_id;
    SVGACOTableDXUAViewEntry entry;
} VMSVGA3DD3D11UAVDefinePlan;

typedef struct vmsvga3d_d3d11_uav_destroy_plan_s {
    SVGA3dUAViewId view_id;
} VMSVGA3DD3D11UAVDestroyPlan;

typedef struct vmsvga3d_d3d11_uav_set_plan_s {
    uint32_t uav_splice_index;
    uint32_t count;
    SVGA3dUAViewId ids[SVGA3D_DX11_1_MAX_UAVIEWS];
    bool shadow_update_atomic;
} VMSVGA3DD3D11UAVSetPlan;

typedef struct vmsvga3d_d3d11_cs_uav_set_plan_s {
    uint32_t start_index;
    uint32_t count;
    SVGA3dUAViewId ids[SVGA3D_DX11_1_MAX_UAVIEWS];
    bool shadow_update_atomic;
} VMSVGA3DD3D11CSUAVSetPlan;

typedef struct vmsvga3d_d3d11_uav_clear_uint_plan_s {
    SVGA3dUAViewId view_id;
    uint32_t values[4];
} VMSVGA3DD3D11UAVClearUintPlan;

typedef struct vmsvga3d_d3d11_uav_clear_float_plan_s {
    SVGA3dUAViewId view_id;
    float values[4];
} VMSVGA3DD3D11UAVClearFloatPlan;

typedef struct vmsvga3d_d3d11_copy_structure_count_plan_s {
    SVGA3dUAViewId source_view_id;
    SVGA3dSurfaceId destination_sid;
    uint32_t destination_byte_offset;
} VMSVGA3DD3D11CopyStructureCountPlan;

typedef struct vmsvga3d_d3d11_set_structure_count_plan_s {
    SVGA3dUAViewId view_id;
    uint32_t structure_count;
} VMSVGA3DD3D11SetStructureCountPlan;

typedef struct vmsvga3d_d3d11_draw_indexed_instanced_indirect_plan_s {
    SVGA3dSurfaceId args_buffer_sid;
    uint32_t aligned_byte_offset;
} VMSVGA3DD3D11DrawIndexedInstancedIndirectPlan;

typedef struct vmsvga3d_d3d11_draw_instanced_indirect_plan_s {
    SVGA3dSurfaceId args_buffer_sid;
    uint32_t aligned_byte_offset;
} VMSVGA3DD3D11DrawInstancedIndirectPlan;

typedef struct vmsvga3d_d3d11_dispatch_plan_s {
    uint32_t thread_group_count_x;
    uint32_t thread_group_count_y;
    uint32_t thread_group_count_z;
} VMSVGA3DD3D11DispatchPlan;

typedef struct vmsvga3d_d3d11_constant_buffer_plan_s {
    SVGA3dShaderType shader_type;
    uint32_t stage_index;
    uint32_t slot;
    SVGA3dSurfaceId sid;
    uint32_t offset_in_bytes;
    uint32_t size_in_bytes;
    bool shadow_update;
    bool unbind;
    bool create_buffer;
    bool has_initial_data;
    uint32_t initial_data_offset;
    uint32_t backend_copy_size;
    uint32_t backend_buffer_size;
} VMSVGA3DD3D11ConstantBufferPlan;

typedef struct vmsvga3d_d3d11_constant_buffer_offset_plan_s {
    SVGA3dShaderType shader_type;
    uint32_t stage_index;
    uint32_t slot;
    uint32_t offset_in_bytes;
} VMSVGA3DD3D11ConstantBufferOffsetPlan;

typedef struct vmsvga3d_dxvk_s VMSVGA3DDxvk;
typedef struct vmsvga3d_dxvk_surface_s VMSVGA3DDxvkSurface;

#ifdef __cplusplus
extern "C" {
#endif

VMSVGA3DD3D11Format vmsvga3d_d3d11_surface_format(SVGA3dSurfaceFormat format);
VMSVGA3DD3D11Level vmsvga3d_d3d11_shader_define_entry(
    const SVGA3dCmdDXDefineShader *src, SVGACOTableDXShaderEntry *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_stream_output_mob_entry(
    const SVGA3dCmdDXDefineStreamOutputWithMob *src,
    SVGACOTableDXStreamOutputEntry *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_stream_output_bind(
    SVGACOTableDXStreamOutputEntry *entry, uint32_t mobid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes);
VMSVGA3DD3D11Level vmsvga3d_d3d11_rasterizer_define_entry(
    const SVGA3dCmdDXDefineRasterizerState_v2 *src,
    SVGACOTableDXRasterizerStateEntry *entry);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_define_entry(
    const SVGA3dCmdDXDefineUAView *src, SVGACOTableDXUAViewEntry *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_destroy_entry(
    SVGACOTableDXUAViewEntry *entry);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_set_structure_count(
    SVGACOTableDXUAViewEntry *entry, uint32_t structure_count);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_define_plan(
    const SVGA3dCmdDXDefineUAView *src, uint32_t cotable_count,
    VMSVGA3DD3D11UAVDefinePlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_destroy_plan(
    const SVGA3dCmdDXDestroyUAView *src, uint32_t cotable_count,
    VMSVGA3DD3D11UAVDestroyPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_set_plan(
    const SVGA3dCmdDXSetUAViews *src, uint32_t count,
    const SVGA3dUAViewId *ids, uint32_t cotable_count,
    VMSVGA3DD3D11UAVSetPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_cs_uav_set_plan(
    const SVGA3dCmdDXSetCSUAViews *src, uint32_t count,
    const SVGA3dUAViewId *ids, uint32_t cotable_count,
    VMSVGA3DD3D11CSUAVSetPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_clear_uint_plan(
    const SVGA3dCmdDXClearUAViewUint *src, uint32_t cotable_count,
    VMSVGA3DD3D11UAVClearUintPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_clear_float_plan(
    const SVGA3dCmdDXClearUAViewFloat *src, uint32_t cotable_count,
    VMSVGA3DD3D11UAVClearFloatPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_copy_structure_count_plan(
    const SVGA3dCmdDXCopyStructureCount *src, uint32_t cotable_count,
    VMSVGA3DD3D11CopyStructureCountPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_set_structure_count_plan(
    const SVGA3dCmdDXSetStructureCount *src, uint32_t cotable_count,
    VMSVGA3DD3D11SetStructureCountPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_draw_indexed_instanced_indirect_plan(
    const SVGA3dCmdDXDrawIndexedInstancedIndirect *src,
    VMSVGA3DD3D11DrawIndexedInstancedIndirectPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_draw_instanced_indirect_plan(
    const SVGA3dCmdDXDrawInstancedIndirect *src,
    VMSVGA3DD3D11DrawInstancedIndirectPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_dispatch_plan(
    const SVGA3dCmdDXDispatch *src, VMSVGA3DD3D11DispatchPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_plan(
    uint32_t slot, SVGA3dShaderType type, SVGA3dSurfaceId sid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes, bool surface_available,
    uint32_t surface_bytes, bool has_surface_data,
    VMSVGA3DD3D11ConstantBufferPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_offset_plan(
    const SVGA3dCmdDXSetConstantBufferOffset *src, SVGA3dShaderType type,
    VMSVGA3DD3D11ConstantBufferOffsetPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_offset_snapshot_plan(
    const VMSVGA3DD3D11ConstantBufferOffsetPlan *offset_plan,
    const SVGA3dConstantBufferBinding *binding, bool surface_available,
    uint32_t surface_bytes, bool has_surface_data,
    VMSVGA3DD3D11ConstantBufferPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid,
    const VMSVGA3DD3D11ConstantBufferPlan *plan,
    const uint8_t *surface_data, uint32_t surface_bytes);
VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_offset_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid,
    const VMSVGA3DD3D11ConstantBufferOffsetPlan *offset_plan,
    const SVGA3dConstantBufferBinding *binding,
    const uint8_t *surface_data, uint32_t surface_bytes,
    bool surface_available, bool has_surface_data);
VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffers_bind_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dShaderType type,
    uint32_t start_slot, uint32_t buffer_count,
    const uint32_t *first_constants, const uint32_t *constant_counts);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_define_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id,
    const SVGACOTableDXUAViewEntry *entry);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_destroy_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_set_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid,
    const VMSVGA3DD3D11UAVSetPlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_cs_uav_set_live(
    VMSVGA3DDxvk *dxvk, const VMSVGA3DD3D11CSUAVSetPlan *plan,
    const uint64_t *modified);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_ensure_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id,
    VMSVGA3DDxvkSurface *surface, const SVGACOTableDXUAViewEntry *entry,
    uint32_t array_elements);
VMSVGA3DD3D11Level vmsvga3d_d3d11_graphics_uav_bind_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t render_target_count,
    const uint32_t *render_target_ids, uint32_t depth_stencil_view_id,
    uint32_t uav_start_slot, uint32_t uav_count,
    const SVGA3dUAViewId *uav_ids,
    const SVGACOTableDXUAViewEntry *uav_entries, uint32_t cotable_count);
VMSVGA3DD3D11Level vmsvga3d_d3d11_cs_uav_bind_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t uav_count,
    const SVGA3dUAViewId *uav_ids,
    const SVGACOTableDXUAViewEntry *uav_entries, uint32_t cotable_count);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_clear_uint_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id,
    VMSVGA3DDxvkSurface *surface, const SVGACOTableDXUAViewEntry *entry,
    uint32_t array_elements, const uint32_t values[4]);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_clear_float_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id,
    VMSVGA3DDxvkSurface *surface, const SVGACOTableDXUAViewEntry *entry,
    uint32_t array_elements, const float values[4]);
VMSVGA3DD3D11Level vmsvga3d_d3d11_copy_structure_count_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId source_view_id,
    VMSVGA3DDxvkSurface *destination, uint32_t destination_byte_offset);
VMSVGA3DD3D11Level vmsvga3d_d3d11_dispatch_live(
    VMSVGA3DDxvk *dxvk, uint32_t thread_group_count_x,
    uint32_t thread_group_count_y, uint32_t thread_group_count_z);
VMSVGA3DD3D11Level vmsvga3d_d3d11_draw_indexed_instanced_indirect_live(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *args_buffer,
    uint32_t aligned_byte_offset);
VMSVGA3DD3D11Level vmsvga3d_d3d11_draw_instanced_indirect_live(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *args_buffer,
    uint32_t aligned_byte_offset);
VMSVGA3DD3D11Level vmsvga3d_d3d11_query_define_entry(
    const SVGA3dCmdDXDefineQuery *src, SVGACOTableDXQueryEntry *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_resource_policy(
    SVGA3dSurfaceAllFlags flags, bool texture_resource,
    uint32_t buffer_byte_stride, VMSVGA3DD3D11ResourcePolicy *policy);
VMSVGA3DD3D11Level vmsvga3d_d3d11_input_element(
    const SVGA3dInputElementDesc *src, VMSVGA3DD3D11InputElement *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_blend_state(
    const SVGACOTableDXBlendStateEntry *src, VMSVGA3DD3D11BlendDesc *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_depth_stencil_state(
    const SVGACOTableDXDepthStencilEntry *src,
    VMSVGA3DD3D11DepthStencilDesc *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_rasterizer_state(
    const SVGACOTableDXRasterizerStateEntry *src,
    VMSVGA3DD3D11RasterizerDesc *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_sampler_state(
    const SVGACOTableDXSamplerEntry *src, VMSVGA3DD3D11SamplerDesc *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_primitive_topology(
    SVGA3dPrimitiveType primitive, uint32_t *d3d_topology);
VMSVGA3DD3D11Level vmsvga3d_d3d11_shader_stage(
    SVGA3dShaderType shader_type, uint32_t *stage_index);
VMSVGA3DD3D11Level vmsvga3d_d3d11_query_info(
    SVGA3dQueryType type, uint32_t flags, VMSVGA3DD3D11QueryInfo *info);
VMSVGA3DD3D11Level vmsvga3d_d3d11_srv_desc(
    const SVGACOTableDXSRViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D11SRVDesc *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_rtv_desc(
    const SVGACOTableDXRTViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D11RTVDesc *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_dsv_desc(
    const SVGACOTableDXDSViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D11DSVDesc *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_desc(
    const SVGACOTableDXUAViewEntry *src, uint32_t array_elements,
    VMSVGA3DD3D11UAVDesc *dst);

#ifdef __cplusplus
}
#endif

#endif
