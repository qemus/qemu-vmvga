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

#ifndef HW_DISPLAY_VMWARE_VGA_D3D10_H
#define HW_DISPLAY_VMWARE_VGA_D3D10_H

#include <stdbool.h>
#include <stdint.h>

#include "svga3d_dx.h"
#include "svga3d_types.h"

/*
 * Pure vGPU10 -> Direct3D 10.x translation.
 *
 * No D3D headers are included here.  Numeric fields use the public D3D10 /
 * D3D10.1 / DXGI ABI values so the eventual native bridge can live in C++ and
 * static_assert these structures and constants against its SDK headers.
 *
 * A translation may identify a later minimum API level.  This is useful
 * because VMware's DX protocol was extended in place after the original
 * vGPU10 generation; the D3D11 translator can reuse the same protocol input
 * without making this D3D10 module silently accept unrepresentable state.
 */

typedef enum vmsvga3d_d3d10_level_e {
  VMSVGA3D_D3D10_LEVEL_INVALID = 0,
  VMSVGA3D_D3D10_LEVEL_10_0,
  VMSVGA3D_D3D10_LEVEL_10_1,
  VMSVGA3D_D3D10_LEVEL_11_0,
  VMSVGA3D_D3D10_LEVEL_11_1,
} VMSVGA3DD3D10Level;

typedef struct vmsvga3d_d3d10_format_s {
  uint32_t dxgi_format;
  VMSVGA3DD3D10Level min_level;
} VMSVGA3DD3D10Format;

#define VMSVGA3D_D3D10_INPUT_SEMANTIC "ATTRIB"

typedef struct vmsvga3d_d3d10_input_element_s {
  uint32_t semantic_index;
  uint32_t format;
  uint32_t input_slot;
  uint32_t aligned_byte_offset;
  uint32_t input_slot_class;
  uint32_t instance_data_step_rate;
} VMSVGA3DD3D10InputElement;

typedef struct vmsvga3d_d3d10_rt_blend_s {
  uint32_t blend_enable;
  uint32_t src_blend;
  uint32_t dest_blend;
  uint32_t blend_op;
  uint32_t src_blend_alpha;
  uint32_t dest_blend_alpha;
  uint32_t blend_op_alpha;
  uint8_t write_mask;
} VMSVGA3DD3D10RTBlend;

typedef struct vmsvga3d_d3d10_blend_desc_s {
  uint32_t alpha_to_coverage_enable;
  uint32_t independent_blend_enable;
  VMSVGA3DD3D10RTBlend render_target[SVGA3D_DX_MAX_RENDER_TARGETS];
} VMSVGA3DD3D10BlendDesc;

typedef struct vmsvga3d_d3d10_stencil_face_s {
  uint32_t fail_op;
  uint32_t depth_fail_op;
  uint32_t pass_op;
  uint32_t func;
} VMSVGA3DD3D10StencilFace;

typedef struct vmsvga3d_d3d10_depth_stencil_desc_s {
  uint32_t depth_enable;
  uint32_t depth_write_mask;
  uint32_t depth_func;
  uint32_t stencil_enable;
  uint8_t stencil_read_mask;
  uint8_t stencil_write_mask;
  VMSVGA3DD3D10StencilFace front_face;
  VMSVGA3DD3D10StencilFace back_face;
} VMSVGA3DD3D10DepthStencilDesc;

typedef struct vmsvga3d_d3d10_rasterizer_desc_s {
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
} VMSVGA3DD3D10RasterizerDesc;

typedef struct vmsvga3d_d3d10_sampler_desc_s {
  uint32_t filter;
  uint32_t address_u;
  uint32_t address_v;
  uint32_t address_w;
  float mip_lod_bias;
  uint32_t max_anisotropy;
  uint32_t comparison_func;
  float border_color[4];
  float min_lod;
  float max_lod;
} VMSVGA3DD3D10SamplerDesc;

typedef struct vmsvga3d_d3d10_srv_desc_s {
  uint32_t format;
  uint32_t view_dimension;
  uint32_t most_detailed_mip;
  uint32_t mip_levels;
  uint32_t first_array_slice;
  uint32_t array_size;
  uint32_t first_element;
  uint32_t num_elements;
} VMSVGA3DD3D10SRVDesc;

typedef struct vmsvga3d_d3d10_rtv_desc_s {
  uint32_t format;
  uint32_t view_dimension;
  uint32_t mip_slice;
  uint32_t first_array_slice;
  uint32_t array_size;
  uint32_t first_w_slice;
  uint32_t w_size;
  uint32_t first_element;
  uint32_t num_elements;
} VMSVGA3DD3D10RTVDesc;

typedef struct vmsvga3d_d3d10_dsv_desc_s {
  uint32_t format;
  uint32_t view_dimension;
  uint32_t mip_slice;
  uint32_t first_array_slice;
  uint32_t array_size;
} VMSVGA3DD3D10DSVDesc;


typedef struct vmsvga3d_d3d10_resource_policy_s {
  uint32_t usage;
  uint32_t bind_flags;
  uint32_t cpu_access_flags;
  uint32_t misc_flags;
} VMSVGA3DD3D10ResourcePolicy;

typedef struct vmsvga3d_d3d10_query_info_s {
  uint32_t d3d_query;
  uint32_t svga_result_size;
  uint32_t d3d_result_size;
  bool boolean_result;
  bool predicate_hint;
} VMSVGA3DD3D10QueryInfo;

#ifdef __cplusplus
extern "C" {
#endif

VMSVGA3DD3D10Format vmsvga3d_d3d10_surface_format(SVGA3dSurfaceFormat format);
bool vmsvga3d_d3d10_is_srgb_format(uint32_t dxgi_format);
uint32_t vmsvga3d_d3d10_typeless_format(uint32_t dxgi_format);
bool vmsvga3d_d3d10_is_depth_stencil_format(uint32_t dxgi_format);
uint32_t vmsvga3d_d3d10_resource_format(SVGA3dSurfaceFormat format,
                                        SVGA3dSurfaceAllFlags flags);
VMSVGA3DD3D10Level vmsvga3d_d3d10_resource_policy(
    SVGA3dSurfaceAllFlags flags, bool texture_resource,
    VMSVGA3DD3D10ResourcePolicy *policy);
VMSVGA3DD3D10Level vmsvga3d_d3d10_input_element(
    const SVGA3dInputElementDesc *src, VMSVGA3DD3D10InputElement *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_blend_state(
    const SVGACOTableDXBlendStateEntry *src, VMSVGA3DD3D10BlendDesc *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_depth_stencil_state(
    const SVGACOTableDXDepthStencilEntry *src,
    VMSVGA3DD3D10DepthStencilDesc *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_rasterizer_state(
    const SVGACOTableDXRasterizerStateEntry *src,
    VMSVGA3DD3D10RasterizerDesc *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_sampler_state(
    const SVGACOTableDXSamplerEntry *src, VMSVGA3DD3D10SamplerDesc *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_primitive_topology(
    SVGA3dPrimitiveType primitive, uint32_t *d3d_topology);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_stage(
    SVGA3dShaderType shader_type, uint32_t *stage_index);
VMSVGA3DD3D10Level vmsvga3d_d3d10_query_info(
    SVGA3dQueryType type, uint32_t flags, VMSVGA3DD3D10QueryInfo *info);
VMSVGA3DD3D10Level vmsvga3d_d3d10_srv_desc(
    const SVGACOTableDXSRViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D10SRVDesc *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_rtv_desc(
    const SVGACOTableDXRTViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D10RTVDesc *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_dsv_desc(
    const SVGACOTableDXDSViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D10DSVDesc *dst);

#ifdef __cplusplus
}
#endif

#endif
