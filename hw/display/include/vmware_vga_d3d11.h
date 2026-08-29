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
typedef VMSVGA3DD3D10ShaderCreatePlan VMSVGA3DD3D11ShaderCreatePlan;

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

#ifdef __cplusplus
extern "C" {
#endif

VMSVGA3DD3D11Format vmsvga3d_d3d11_surface_format(SVGA3dSurfaceFormat format);
VMSVGA3DD3D11Level vmsvga3d_d3d11_shader_define_entry(
    const SVGA3dCmdDXDefineShader *src, SVGACOTableDXShaderEntry *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_shader_create_plan(
    SVGA3dShaderType type, uint32_t stream_output_id,
    VMSVGA3DD3D11ShaderCreatePlan *plan);
VMSVGA3DD3D11Level vmsvga3d_d3d11_stream_output_mob_entry(
    const SVGA3dCmdDXDefineStreamOutputWithMob *src,
    SVGACOTableDXStreamOutputEntry *dst);
VMSVGA3DD3D11Level vmsvga3d_d3d11_stream_output_bind(
    SVGACOTableDXStreamOutputEntry *entry, uint32_t mobid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes);
VMSVGA3DD3D11Level vmsvga3d_d3d11_rasterizer_define_entry(
    const SVGA3dCmdDXDefineRasterizerState_v2 *src,
    SVGACOTableDXRasterizerStateEntry *entry);
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
