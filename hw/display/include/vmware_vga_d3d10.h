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
 * VMware vGPU10 protocol translation for a D3D11-compatible host renderer.
 *
 * VirtualBox implements the vGPU10 protocol through its D3D11 backend rather
 * than by constraining execution to ID3D10Device.  Preserve that distinction:
 * D3D10/D3D10.1 names below are used where their public numeric ABI is shared
 * with D3D11, but guest state is translated according to VirtualBox's vGPU10
 * semantics and D3D11 realization.  No D3D headers are included here.
 *
 * A translation may identify the protocol state as requiring a later Direct3D
 * generation.  The D3D11 translator can extend those cases without making the
 * vGPU10 layer discard state merely because literal D3D10 cannot represent it.
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

typedef enum vmsvga3d_d3d10_resource_use_e {
  VMSVGA3D_D3D10_RESOURCE_USE_TEXTURE = 0,
  VMSVGA3D_D3D10_RESOURCE_USE_BUFFER,
  VMSVGA3D_D3D10_RESOURCE_USE_STREAM_OUTPUT_BUFFER,
  VMSVGA3D_D3D10_RESOURCE_USE_GENERIC_BUFFER,
} VMSVGA3DD3D10ResourceUse;

typedef struct vmsvga3d_d3d10_surface_info_s {
  SVGA3dSurfaceAllFlags surface_flags;
  SVGA3dSurfaceFormat format;
  /* Block-aligned native resource extent for compressed/block formats. */
  SVGA3dSize size;
  uint32_t mip_levels;
  /* Normalized array count; cubemaps already contain 6 elements per cube. */
  uint32_t array_elements;
  uint32_t multisample_count;
  SVGA3dTextureFilter autogen_filter;
  /* Byte size of mip 0 when this surface is used as a buffer. */
  uint32_t surface_bytes;
  bool has_initial_data;
} VMSVGA3DD3D10SurfaceInfo;

typedef struct vmsvga3d_d3d10_create_desc_s {
  bool valid;
  uint32_t resource_dimension;
  uint32_t byte_width;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mip_levels;
  uint32_t array_size;
  uint32_t format;
  uint32_t sample_count;
  uint32_t sample_quality;
  uint32_t usage;
  uint32_t bind_flags;
  uint32_t cpu_access_flags;
  uint32_t misc_flags;
  uint32_t initial_subresource_count;
} VMSVGA3DD3D10CreateDesc;

typedef struct vmsvga3d_d3d10_resource_plan_s {
  VMSVGA3DD3D10ResourceUse use;
  uint32_t requested_format;
  uint32_t resource_format;
  uint32_t dynamic_format;
  uint32_t staging_format;
  VMSVGA3DD3D10CreateDesc primary;
  VMSVGA3DD3D10CreateDesc dynamic;
  VMSVGA3DD3D10CreateDesc staging;
  bool has_dynamic;
  bool has_staging;
  bool uses_common_staging_buffer;
} VMSVGA3DD3D10ResourcePlan;

#define VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES 32u

typedef enum vmsvga3d_d3d10_shader_program_type_e {
  VMSVGA3D_D3D10_SHADER_PROGRAM_PIXEL = 0,
  VMSVGA3D_D3D10_SHADER_PROGRAM_VERTEX = 1,
  VMSVGA3D_D3D10_SHADER_PROGRAM_GEOMETRY = 2,
  VMSVGA3D_D3D10_SHADER_PROGRAM_HULL = 3,
  VMSVGA3D_D3D10_SHADER_PROGRAM_DOMAIN = 4,
  VMSVGA3D_D3D10_SHADER_PROGRAM_COMPUTE = 5,
} VMSVGA3DD3D10ShaderProgramType;

typedef enum vmsvga3d_d3d10_shader_component_type_e {
  VMSVGA3D_D3D10_SHADER_COMPONENT_UNKNOWN = 0,
  VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32 = 1,
  VMSVGA3D_D3D10_SHADER_COMPONENT_SINT32 = 2,
  VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 = 3,
} VMSVGA3DD3D10ShaderComponentType;

typedef struct vmsvga3d_d3d10_shader_semantic_s {
  const char *semantic_name;
  uint32_t semantic_index;
} VMSVGA3DD3D10ShaderSemantic;

typedef struct vmsvga3d_d3d10_shader_info_s {
  uint32_t program_type;
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t token_count;
  uint32_t bytecode_size;
  bool guest_signatures;
  bool semantics_complete;
  bool match_masks_covered;
  uint32_t input_signature_count;
  uint32_t output_signature_count;
  uint32_t patch_signature_count;
  SVGA3dDXShaderSignatureEntry
      input_signature[VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES];
  SVGA3dDXShaderSignatureEntry
      output_signature[VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES];
  SVGA3dDXShaderSignatureEntry
      patch_signature[VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES];
  VMSVGA3DD3D10ShaderSemantic
      input_semantic[VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES];
  VMSVGA3DD3D10ShaderSemantic
      output_semantic[VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES];
  VMSVGA3DD3D10ShaderSemantic
      patch_semantic[VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES];
} VMSVGA3DD3D10ShaderInfo;

typedef enum vmsvga3d_d3d10_shader_create_kind_e {
  VMSVGA3D_D3D10_SHADER_CREATE_INVALID = 0,
  VMSVGA3D_D3D10_SHADER_CREATE_VERTEX,
  VMSVGA3D_D3D10_SHADER_CREATE_PIXEL,
  VMSVGA3D_D3D10_SHADER_CREATE_GEOMETRY,
  VMSVGA3D_D3D10_SHADER_CREATE_GEOMETRY_STREAM_OUTPUT,
  VMSVGA3D_D3D10_SHADER_CREATE_HULL,
  VMSVGA3D_D3D10_SHADER_CREATE_DOMAIN,
  VMSVGA3D_D3D10_SHADER_CREATE_COMPUTE,
} VMSVGA3DD3D10ShaderCreateKind;

typedef struct vmsvga3d_d3d10_shader_create_plan_s {
  VMSVGA3DD3D10ShaderCreateKind create_kind;
  uint32_t stage_index;
  uint32_t stream_output_id;
  bool use_stream_output;
} VMSVGA3DD3D10ShaderCreatePlan;

typedef struct vmsvga3d_d3d10_shader_output_semantic_s {
  uint32_t register_index;
  uint32_t mask;
  const char *semantic_name;
  uint32_t semantic_index;
} VMSVGA3DD3D10ShaderOutputSemantic;

typedef struct vmsvga3d_d3d10_stream_output_decl_s {
  uint32_t stream;
  const char *semantic_name;
  uint32_t semantic_index;
  uint8_t start_component;
  uint8_t component_count;
  uint8_t output_slot;
} VMSVGA3DD3D10StreamOutputDecl;

typedef struct vmsvga3d_d3d10_stream_output_plan_s {
  uint32_t declaration_count;
  VMSVGA3DD3D10StreamOutputDecl declarations[SVGA3D_MAX_STREAMOUT_DECLS];
  uint32_t strides[SVGA3D_DX_MAX_SOTARGETS];
  uint32_t stride_count;
  uint32_t rasterized_stream;
  bool use_explicit_strides;
  bool uses_mob;
  bool all_semantics_resolved;
} VMSVGA3DD3D10StreamOutputPlan;

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
VMSVGA3DD3D10Level vmsvga3d_d3d10_resource_plan(
    const VMSVGA3DD3D10SurfaceInfo *surface, VMSVGA3DD3D10ResourceUse use,
    VMSVGA3DD3D10ResourcePlan *plan);
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
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_define_entry(
    const SVGA3dCmdDXDefineShader *src, SVGACOTableDXShaderEntry *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_destroy_entry(
    SVGACOTableDXShaderEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_bind_entry(
    SVGACOTableDXShaderEntry *entry, uint32_t mobid, uint32_t offset_in_bytes);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_blob_info(
    const void *blob, uint32_t size_in_bytes, VMSVGA3DD3D10ShaderInfo *info);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_guest_signatures(
    const void *blob, uint32_t size_in_bytes, VMSVGA3DD3D10ShaderInfo *info);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_finalize_signatures(
    VMSVGA3DD3D10ShaderInfo *info);
uint32_t vmsvga3d_d3d10_shader_component_type_from_format(
    SVGA3dSurfaceFormat format);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_update_vs_input_signature(
    VMSVGA3DD3D10ShaderInfo *info, const SVGA3dInputElementDesc *elements,
    uint32_t element_count);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_match_signatures(
    SVGA3dShaderType type, VMSVGA3DD3D10ShaderInfo *shader,
    const VMSVGA3DD3D10ShaderInfo *vs, const VMSVGA3DD3D10ShaderInfo *hs,
    const VMSVGA3DD3D10ShaderInfo *ds, const VMSVGA3DD3D10ShaderInfo *gs,
    const VMSVGA3DD3D10ShaderInfo *ps);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_output_semantics(
    const VMSVGA3DD3D10ShaderInfo *info,
    VMSVGA3DD3D10ShaderOutputSemantic *outputs, uint32_t output_capacity,
    uint32_t *output_count);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_create_plan(
    SVGA3dShaderType type, uint32_t stream_output_id,
    VMSVGA3DD3D10ShaderCreatePlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_legacy_entry(
    const SVGA3dCmdDXDefineStreamOutput *src,
    SVGACOTableDXStreamOutputEntry *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_mob_entry(
    const SVGA3dCmdDXDefineStreamOutputWithMob *src,
    SVGACOTableDXStreamOutputEntry *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_bind(
    SVGACOTableDXStreamOutputEntry *entry, uint32_t mobid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes);
VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_plan(
    const SVGACOTableDXStreamOutputEntry *entry,
    const SVGA3dStreamOutputDeclarationEntry *declarations,
    const VMSVGA3DD3D10ShaderOutputSemantic *shader_outputs,
    uint32_t shader_output_count, VMSVGA3DD3D10StreamOutputPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_viewports(
    const SVGA3dViewport *src, uint32_t count, SVGA3dViewport *dst);
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
