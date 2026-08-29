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
 * D3D10/D3D10.1 names below are used where their public numeric ABI is shared
 * with D3D11.  No Direct3D headers are included here.  Low-level translators
 * may classify data as requiring a later feature level, but command-facing
 * vGPU10 helpers own only D3D10/D3D10.1 guest state.  D3D11-only guest commands
 * and state belong to the D3D11 module.
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

typedef struct vmsvga3d_d3d10_box_s {
  uint32_t left;
  uint32_t top;
  uint32_t front;
  uint32_t right;
  uint32_t bottom;
  uint32_t back;
} VMSVGA3DD3D10Box;

typedef struct vmsvga3d_d3d10_copy_region_plan_s {
  SVGA3dCopyBox clipped_box;
  uint32_t destination_x;
  uint32_t destination_y;
  uint32_t destination_z;
  VMSVGA3DD3D10Box source_box;
} VMSVGA3DD3D10CopyRegionPlan;

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

/*
 * ShaderInfo is an owning object after shader_parse(): rewritten_bytecode must
 * be released exactly once with shader_release().  Semantic-name pointers are
 * borrowed and must not be freed.  Do not copy an owning ShaderInfo by value.
 * Output objects passed to shader_blob_info()/shader_parse() must not already
 * own rewritten bytecode.
 */
typedef struct vmsvga3d_d3d10_shader_info_s {
  uint32_t program_type;
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t token_count;
  uint32_t bytecode_size;
  void *rewritten_bytecode;
  uint32_t rewritten_bytecode_size;
  uint32_t resource_declaration_count;
  uint32_t resource_declaration_offsets[SVGA3D_DX_MAX_SRVIEWS];
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

typedef enum vmsvga3d_d3d10_shader_resource_dimension_e {
  VMSVGA3D_D3D10_SHADER_RESOURCE_UNKNOWN = 0,
  VMSVGA3D_D3D10_SHADER_RESOURCE_BUFFER = 1,
  VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE1D = 2,
  VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2D = 3,
  VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2DMS = 4,
  VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE3D = 5,
  VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURECUBE = 6,
  VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE1DARRAY = 7,
  VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2DARRAY = 8,
  VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2DMSARRAY = 9,
  VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURECUBEARRAY = 10,
} VMSVGA3DD3D10ShaderResourceDimension;

typedef enum vmsvga3d_d3d10_shader_resource_return_type_e {
  VMSVGA3D_D3D10_SHADER_RETURN_NONE = 0,
  VMSVGA3D_D3D10_SHADER_RETURN_UNORM = 1,
  VMSVGA3D_D3D10_SHADER_RETURN_SNORM = 2,
  VMSVGA3D_D3D10_SHADER_RETURN_SINT = 3,
  VMSVGA3D_D3D10_SHADER_RETURN_UINT = 4,
  VMSVGA3D_D3D10_SHADER_RETURN_FLOAT = 5,
  VMSVGA3D_D3D10_SHADER_RETURN_MIXED = 6,
} VMSVGA3DD3D10ShaderResourceReturnType;

typedef struct vmsvga3d_d3d10_shader_resource_binding_s {
  uint32_t resource_dimension;
  uint32_t return_type;
  SVGA3dSurfaceFormat format;
} VMSVGA3DD3D10ShaderResourceBinding;

/*
 * ShaderDXBC owns data after shader_create_dxbc().  Release it exactly once
 * with shader_dxbc_release(); callers must not reuse an owning output object.
 */
typedef struct vmsvga3d_d3d10_shader_dxbc_s {
  void *data;
  uint32_t size;
} VMSVGA3DD3D10ShaderDXBC;

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

typedef struct vmsvga3d_d3d10_so_target_binding_s {
  SVGA3dSurfaceId sid;
  uint32_t offset;
  uint32_t size_in_bytes;
  bool active;
  bool ensure_stream_output_buffer;
} VMSVGA3DD3D10SOTargetBinding;

typedef struct vmsvga3d_d3d10_so_targets_plan_s {
  uint32_t guest_count;
  SVGA3dSurfaceId shadow_targets[SVGA3D_DX_MAX_SOTARGETS];
  VMSVGA3DD3D10SOTargetBinding bindings[SVGA3D_DX_MAX_SOTARGETS];
  uint32_t native_target_count;
  uint32_t backend_remembered_count;
  bool shadow_update;
  bool shadow_update_before_backend;
  bool shadow_stores_only_surface_ids;
  bool shadow_clears_unspecified_slots;
  bool prepare_targets_sequentially;
  bool size_in_bytes_ignored;
  bool bind_full_native_table;
  bool immediate_bind;
} VMSVGA3DD3D10SOTargetsPlan;

typedef struct vmsvga3d_d3d10_stream_output_set_plan_s {
  SVGA3dStreamOutputId stream_output_id;
  bool shadow_update;
  bool backend_set_is_noop;
  bool affects_geometry_shader_creation;
} VMSVGA3DD3D10StreamOutputSetPlan;

typedef enum vmsvga3d_d3d10_object_kind_e {
  VMSVGA3D_D3D10_OBJECT_SRV = 0,
  VMSVGA3D_D3D10_OBJECT_RTV,
  VMSVGA3D_D3D10_OBJECT_DSV,
  VMSVGA3D_D3D10_OBJECT_ELEMENT_LAYOUT,
  VMSVGA3D_D3D10_OBJECT_BLEND_STATE,
  VMSVGA3D_D3D10_OBJECT_DEPTH_STENCIL_STATE,
  VMSVGA3D_D3D10_OBJECT_RASTERIZER_STATE,
  VMSVGA3D_D3D10_OBJECT_SAMPLER_STATE,
  VMSVGA3D_D3D10_OBJECT_SHADER,
  VMSVGA3D_D3D10_OBJECT_STREAM_OUTPUT,
  VMSVGA3D_D3D10_OBJECT_QUERY,
  VMSVGA3D_D3D10_OBJECT_COUNT,
} VMSVGA3DD3D10ObjectKind;

typedef struct vmsvga3d_d3d10_object_lifecycle_plan_s {
  VMSVGA3DD3D10ObjectKind kind;
  bool define_calls_backend;
  bool define_creates_native_immediately;
  bool define_native_object_is_lazy;
  bool define_backend_expects_empty_slot;
  bool define_can_replace_existing_native_without_release;
  bool define_releases_existing_before_entry_update;
  bool backend_define_resets_existing_cached_object;
  bool define_may_create_surface_resource;
  bool define_surface_kind_depends_on_surface_format;
  uint32_t define_surface_create_kind;
  bool destroy_calls_backend;
  bool destroy_releases_native;
  bool destroy_entry_reset_before_backend;
  bool destroy_entry_reset_after_backend;
  bool destroy_result_propagates;
  bool destroy_clears_bound_references;
} VMSVGA3DD3D10ObjectLifecyclePlan;

typedef enum vmsvga3d_d3d10_cotable_replay_mode_e {
  VMSVGA3D_D3D10_COTABLE_REPLAY_VIEW = 0,
  VMSVGA3D_D3D10_COTABLE_REPLAY_ELEMENT_LAYOUT,
  VMSVGA3D_D3D10_COTABLE_REPLAY_STATE,
  VMSVGA3D_D3D10_COTABLE_REPLAY_STREAM_OUTPUT,
  VMSVGA3D_D3D10_COTABLE_REPLAY_QUERY,
  VMSVGA3D_D3D10_COTABLE_REPLAY_SHADER,
} VMSVGA3DD3D10COTableReplayMode;

typedef struct vmsvga3d_d3d10_cotable_plan_s {
  SVGACOTableType type;
  uint32_t entry_size;
  uint32_t capacity_entries;
  uint32_t valid_entries;
  uint32_t grow_copy_bytes;
  VMSVGA3DD3D10COTableReplayMode replay_mode;
  bool unbind;
  bool create_backing_store;
  bool replace_frontend_table_before_backend;
  bool grow_copies_valid_size_bytes;
  bool backend_reallocates_to_capacity;
  bool backend_preserves_prefix_up_to_valid_count;
  bool backend_zeroes_after_preserved_prefix;
  bool backend_releases_truncated_entries;
  bool skip_all_zero_entries_during_replay;
  bool state_replay_can_replace_preserved_pointer_without_release;
} VMSVGA3DD3D10COTablePlan;

typedef enum vmsvga3d_d3d10_bind_timing_e {
  VMSVGA3D_D3D10_BIND_IMMEDIATE = 0,
  VMSVGA3D_D3D10_BIND_DRAW_SETUP,
} VMSVGA3DD3D10BindTiming;

typedef struct vmsvga3d_d3d10_constant_buffer_plan_s {
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
  bool replace_only_on_create_success;
  bool preserve_old_buffer_on_create_failure;
  bool create_failure_is_success;
  bool bind_only_if_pipeline_differs;
  VMSVGA3DD3D10BindTiming bind_timing;
  VMSVGA3DD3D10CreateDesc create_desc;
} VMSVGA3DD3D10ConstantBufferPlan;

typedef struct vmsvga3d_d3d10_shader_resource_set_plan_s {
  uint32_t stage_index;
  uint32_t start_view;
  uint32_t count;
  uint32_t ids[SVGA3D_DX_MAX_SRVIEWS];
  uint32_t shadow_update_count;
  bool shadow_update_atomic;
  bool ensure_views_at_draw;
  bool bind_full_table_at_draw;
  bool bind_every_draw;
  VMSVGA3DD3D10BindTiming bind_timing;
} VMSVGA3DD3D10ShaderResourceSetPlan;

typedef struct vmsvga3d_d3d10_shader_set_plan_s {
  uint32_t stage_index;
  SVGA3dShaderId shader_id;
  bool shadow_update;
  bool unbind;
  bool create_if_missing_at_draw;
  bool patch_pixel_resources_on_create;
  bool update_vs_input_signature_on_create;
  bool match_signatures_on_create;
  bool stream_output_affects_geometry_creation;
  bool bind_every_draw;
  VMSVGA3DD3D10BindTiming bind_timing;
} VMSVGA3DD3D10ShaderSetPlan;

typedef struct vmsvga3d_d3d10_sampler_set_plan_s {
  uint32_t stage_index;
  uint32_t start_sampler;
  uint32_t count;
  uint32_t ids[SVGA3D_DX_MAX_SAMPLERS];
  uint32_t shadow_update_count;
  bool partial_shadow_update_on_failure;
  VMSVGA3DD3D10BindTiming bind_timing;
} VMSVGA3DD3D10SamplerSetPlan;

typedef struct vmsvga3d_d3d10_input_layout_set_plan_s {
  SVGA3dElementLayoutId layout_id;
  bool shadow_update;
  bool unbind;
  bool create_lazily_at_draw;
  bool requires_vertex_shader_dxbc;
  bool bind_every_draw;
  VMSVGA3DD3D10BindTiming bind_timing;
} VMSVGA3DD3D10InputLayoutSetPlan;

typedef struct vmsvga3d_d3d10_vertex_buffer_binding_s {
  SVGA3dSurfaceId sid;
  uint32_t stride;
  uint32_t offset;
  bool unbind;
  bool ensure_buffer_on_set;
} VMSVGA3DD3D10VertexBufferBinding;

typedef struct vmsvga3d_d3d10_vertex_buffer_set_plan_s {
  uint32_t start_buffer;
  uint32_t count;
  VMSVGA3DD3D10VertexBufferBinding bindings[SVGA3D_DX_MAX_VERTEXBUFFERS];
  uint32_t shadow_update_count;
  bool prepare_backend_on_set;
  bool backend_updates_sequentially;
  bool bind_from_slot_zero_at_draw;
  bool bind_only_if_pipeline_differs;
  VMSVGA3DD3D10BindTiming bind_timing;
} VMSVGA3DD3D10VertexBufferSetPlan;

typedef struct vmsvga3d_d3d10_vertex_buffer_pipeline_binding_s {
  uint32_t stride;
  uint32_t offset;
} VMSVGA3DD3D10VertexBufferPipelineBinding;

typedef struct vmsvga3d_d3d10_index_buffer_set_plan_s {
  SVGA3dSurfaceId sid;
  SVGA3dSurfaceFormat format;
  uint32_t offset;
  uint32_t backend_offset;
  uint32_t dxgi_format;
  uint32_t bytes_per_index;
  bool shadow_update;
  bool unbind;
  bool ensure_buffer_on_set;
  bool backend_reject;
  bool bind_only_if_pipeline_differs;
  VMSVGA3DD3D10BindTiming bind_timing;
} VMSVGA3DD3D10IndexBufferSetPlan;

typedef struct vmsvga3d_d3d10_viewports_set_plan_s {
  uint32_t count;
  SVGA3dViewport viewports[SVGA3D_DX_MAX_VIEWPORTS];
  bool shadow_update;
  bool preserve_unspecified_slots;
  bool immediate_bind;
} VMSVGA3DD3D10ViewportsSetPlan;

typedef struct vmsvga3d_d3d10_topology_set_plan_s {
  SVGA3dPrimitiveType topology;
  uint32_t native_topology;
  bool shadow_update;
  bool immediate_bind;
} VMSVGA3DD3D10TopologySetPlan;

typedef struct vmsvga3d_d3d10_blend_state_set_plan_s {
  SVGA3dBlendStateId blend_id;
  float blend_factor[4];
  uint32_t sample_mask;
  bool shadow_update;
  bool immediate_bind;
} VMSVGA3DD3D10BlendStateSetPlan;

typedef struct vmsvga3d_d3d10_depth_stencil_state_set_plan_s {
  SVGA3dDepthStencilStateId depth_stencil_id;
  uint32_t stencil_ref;
  bool shadow_update;
  bool immediate_bind;
} VMSVGA3DD3D10DepthStencilStateSetPlan;

typedef struct vmsvga3d_d3d10_rasterizer_state_set_plan_s {
  SVGA3dRasterizerStateId rasterizer_id;
  bool shadow_update;
  bool immediate_bind;
} VMSVGA3DD3D10RasterizerStateSetPlan;

typedef enum vmsvga3d_d3d10_pipeline_step_e {
  VMSVGA3D_D3D10_PIPELINE_UNBIND_OUTPUTS = 0,
  VMSVGA3D_D3D10_PIPELINE_CONSTANT_BUFFERS,
  VMSVGA3D_D3D10_PIPELINE_VERTEX_BUFFERS,
  VMSVGA3D_D3D10_PIPELINE_INDEX_BUFFER,
  VMSVGA3D_D3D10_PIPELINE_SHADER_RESOURCES,
  VMSVGA3D_D3D10_PIPELINE_RENDER_TARGETS,
  VMSVGA3D_D3D10_PIPELINE_SHADERS,
  VMSVGA3D_D3D10_PIPELINE_INPUT_LAYOUT,
  VMSVGA3D_D3D10_PIPELINE_STEP_COUNT,
} VMSVGA3DD3D10PipelineStep;

typedef struct vmsvga3d_d3d10_scissor_plan_s {
  uint32_t count;
  SVGASignedRect rects[SVGA3D_DX_MAX_SCISSORRECTS];
  bool shadow_update;
  bool native_layout_identical;
  bool immediate_bind;
} VMSVGA3DD3D10ScissorPlan;

typedef struct vmsvga3d_d3d10_render_targets_set_plan_s {
  SVGA3dDepthStencilViewId depth_stencil_view_id;
  uint32_t supplied_count;
  uint32_t previous_remembered_count;
  uint32_t remembered_count;
  SVGA3dRenderTargetViewId ids[SVGA3D_MAX_RENDER_TARGETS];
  uint32_t shadow_update_count;
  bool shadow_update_atomic;
  bool preserve_unspecified_slots;
  bool backend_set_is_noop;
  bool ensure_dsv_at_draw;
  bool ensure_all_rtv_slots_at_draw;
  bool bind_at_draw_setup;
} VMSVGA3DD3D10RenderTargetsSetPlan;

typedef struct vmsvga3d_d3d10_render_targets_pipeline_plan_s {
  SVGA3dDepthStencilViewId depth_stencil_view_id;
  uint32_t remembered_count;
  uint32_t native_rtv_count;
  SVGA3dRenderTargetViewId ids[SVGA3D_MAX_RENDER_TARGETS];
  bool native_count_is_number_of_valid_ids;
  bool sparse_slot_bug_preserved;
  bool use_render_targets_and_uavs_call;
  bool vgpu10_uav_count_is_zero;
} VMSVGA3DD3D10RenderTargetsPipelinePlan;

typedef struct vmsvga3d_d3d10_gen_mips_plan_s {
  SVGA3dShaderResourceViewId view_id;
  bool require_existing_backend_view;
  bool do_not_create_view_on_call;
  bool require_view_entry;
  bool require_surface;
  bool require_backend_surface;
  bool immediate_generate_mips;
  bool mark_surface_drawing_context;
} VMSVGA3DD3D10GenMipsPlan;

typedef enum vmsvga3d_d3d10_clear_kind_e {
  VMSVGA3D_D3D10_CLEAR_RENDER_TARGET = 0,
  VMSVGA3D_D3D10_CLEAR_DEPTH_STENCIL,
} VMSVGA3DD3D10ClearKind;

typedef struct vmsvga3d_d3d10_clear_rtv_plan_s {
  SVGA3dRenderTargetViewId view_id;
  float color[4];
  bool ensure_view_on_clear;
  bool immediate_clear;
} VMSVGA3DD3D10ClearRTVPlan;

typedef struct vmsvga3d_d3d10_clear_dsv_plan_s {
  SVGA3dDepthStencilViewId view_id;
  uint32_t guest_flags;
  uint32_t d3d_clear_flags;
  float depth;
  uint8_t stencil;
  bool ensure_view_on_clear;
  bool immediate_clear;
} VMSVGA3DD3D10ClearDSVPlan;

typedef enum vmsvga3d_d3d10_resource_create_kind_e {
  VMSVGA3D_D3D10_CREATE_TEXTURE = 0,
  VMSVGA3D_D3D10_CREATE_BUFFER,
} VMSVGA3DD3D10ResourceCreateKind;

typedef struct vmsvga3d_d3d10_copy_resource_plan_s {
  bool ensure_source_resource;
  bool ensure_destination_resource;
  VMSVGA3DD3D10ResourceCreateKind source_create_kind;
  VMSVGA3DD3D10ResourceCreateKind destination_create_kind;
  bool destination_create_kind_uses_source_format;
  bool issue_copy_resource;
  bool mark_destination_drawing_context;
} VMSVGA3DD3D10CopyResourcePlan;

typedef struct vmsvga3d_d3d10_copy_subresource_plan_s {
  bool ensure_source_resource;
  bool ensure_destination_resource;
  VMSVGA3DD3D10ResourceCreateKind source_create_kind;
  VMSVGA3DD3D10ResourceCreateKind destination_create_kind;
  bool destination_create_kind_uses_source_format;
  uint32_t destination_subresource;
  uint32_t source_subresource;
  VMSVGA3DD3D10CopyRegionPlan region;
  bool issue_copy_subresource_region;
  bool mark_destination_drawing_context;
} VMSVGA3DD3D10CopySubresourcePlan;

typedef enum vmsvga3d_d3d10_draw_kind_e {
  VMSVGA3D_D3D10_DRAW = 0,
  VMSVGA3D_D3D10_DRAW_INDEXED,
  VMSVGA3D_D3D10_DRAW_INSTANCED,
  VMSVGA3D_D3D10_DRAW_INDEXED_INSTANCED,
  VMSVGA3D_D3D10_DRAW_AUTO,
} VMSVGA3DD3D10DrawKind;

typedef struct vmsvga3d_d3d10_pipeline_setup_plan_s {
  uint32_t step_count;
  VMSVGA3DD3D10PipelineStep steps[VMSVGA3D_D3D10_PIPELINE_STEP_COUNT];
  bool ensure_all_shader_resource_views;
  bool wait_for_shader_resource_surfaces;
  bool bind_full_shader_resource_table_per_stage;
  bool ensure_depth_stencil_view;
  bool ensure_render_target_views;
  bool create_shaders_lazily;
  bool recreate_input_layout_from_vs_dxbc;
  bool failures_do_not_abort_draw;
} VMSVGA3DD3D10PipelineSetupPlan;

typedef struct vmsvga3d_d3d10_triangle_fan_plan_s {
  bool enabled;
  bool indexed_source;
  bool reject_count_over_65535;
  bool backend_reject;
  bool helper_failure_ignored;
  bool save_restore_index_buffer;
  bool temporary_index_buffer_is_immutable;
  bool temporary_buffer_create_failure_assert_only;
  bool generated_indices_are_u16;
  bool truncate_u32_source_indices_to_u16;
  bool source_read_offset_is_raw_start_index;
  bool ignore_bound_index_buffer_offset;
  uint32_t source_index_format;
  uint32_t source_bytes_per_index;
  uint32_t source_read_offset;
  uint32_t source_read_bytes;
  uint32_t bound_index_buffer_offset;
  uint32_t generated_index_count;
  uint32_t generated_buffer_bytes;
  uint32_t generated_start_index;
  int32_t generated_base_vertex;
  uint32_t temporary_topology;
  uint32_t restored_topology;
} VMSVGA3DD3D10TriangleFanPlan;

typedef struct vmsvga3d_d3d10_draw_plan_s {
  VMSVGA3DD3D10DrawKind requested_kind;
  VMSVGA3DD3D10DrawKind native_kind;
  SVGA3dPrimitiveType primitive;
  uint32_t native_topology;
  VMSVGA3DD3D10PipelineSetupPlan pipeline;
  uint32_t count0;
  uint32_t count1;
  uint32_t start0;
  uint32_t start1;
  int32_t base_vertex;
  bool triangle_fan_assert_only;
  bool track_render_targets_after_draw;
  bool skip_render_target_tracking_on_backend_reject;
  bool skip_render_target_tracking_on_emulation_failure;
  bool backend_reject;
  VMSVGA3DD3D10TriangleFanPlan triangle_fan;
} VMSVGA3DD3D10DrawPlan;

typedef struct vmsvga3d_d3d10_query_info_s {
  uint32_t d3d_query;
  uint32_t svga_result_size;
  uint32_t d3d_result_size;
  bool boolean_result;
  bool predicate_hint;
} VMSVGA3DD3D10QueryInfo;

#define VMSVGA3D_D3D10_QUERY_MISC_PREDICATEHINT 0x1u

typedef struct vmsvga3d_d3d10_query_execution_plan_s {
  uint32_t d3d_query;
  uint32_t misc_flags;
  uint32_t svga_result_size;
  uint32_t d3d_result_size;
  uint32_t getdata_flags;
  bool issue_begin;
  bool issue_end;
  bool get_data_after_end;
  bool wait_until_ready;
  bool retry_non_s_ok;
  bool yield_while_waiting;
  bool readback_is_noop;
} VMSVGA3DD3D10QueryExecutionPlan;

typedef struct vmsvga3d_d3d10_predication_plan_s {
  bool enabled;
  bool release_existing_query;
  bool create_predicate;
  uint32_t d3d_query;
  uint32_t misc_flags;
  bool predicate_value;
} VMSVGA3DD3D10PredicationPlan;

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
VMSVGA3DD3D10Level vmsvga3d_d3d10_index_format(
    SVGA3dSurfaceFormat format, uint32_t *dxgi_format,
    uint32_t *bytes_per_index);
VMSVGA3DD3D10Level vmsvga3d_d3d10_box(
    const SVGA3dBox *src, VMSVGA3DD3D10Box *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_copy_region_plan(
    const SVGA3dSize *src_size, const SVGA3dSize *dst_size,
    const SVGA3dCopyBox *box, VMSVGA3DD3D10CopyRegionPlan *plan);
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
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_parse(
    const void *blob, uint32_t size_in_bytes, VMSVGA3DD3D10ShaderInfo *info);
void vmsvga3d_d3d10_shader_release(VMSVGA3DD3D10ShaderInfo *info);
uint32_t vmsvga3d_d3d10_shader_resource_return_type(
    SVGA3dSurfaceFormat format);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_resource_binding(
    const SVGACOTableDXSRViewEntry *view, uint32_t array_elements,
    VMSVGA3DD3D10ShaderResourceBinding *binding);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_update_resources(
    VMSVGA3DD3D10ShaderInfo *info,
    const VMSVGA3DD3D10ShaderResourceBinding *bindings,
    uint32_t binding_count);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_create_dxbc(
    const VMSVGA3DD3D10ShaderInfo *info, VMSVGA3DD3D10ShaderDXBC *dxbc);
void vmsvga3d_d3d10_shader_dxbc_release(VMSVGA3DD3D10ShaderDXBC *dxbc);
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
VMSVGA3DD3D10Level vmsvga3d_d3d10_so_targets_plan(
    uint32_t count, const SVGA3dSoTarget *targets,
    VMSVGA3DD3D10SOTargetsPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_so_targets_restore_plan(
    const SVGA3dSurfaceId targets[SVGA3D_DX_MAX_SOTARGETS],
    VMSVGA3DD3D10SOTargetsPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_set_plan(
    SVGA3dStreamOutputId stream_output_id, uint32_t table_count,
    VMSVGA3DD3D10StreamOutputSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_destroy_entry(
    SVGACOTableDXStreamOutputEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_rtv_destroy_shadow_refs(
    SVGA3dRenderTargetViewId destroyed_id,
    SVGA3dRenderTargetViewId ids[SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS]);
VMSVGA3DD3D10Level vmsvga3d_d3d10_object_lifecycle_plan(
    VMSVGA3DD3D10ObjectKind kind,
    VMSVGA3DD3D10ObjectLifecyclePlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_cotable_plan(
    SVGACOTableType type, bool has_mob, uint32_t mob_size,
    uint32_t valid_size_in_bytes, bool grow, bool had_previous_mob,
    VMSVGA3DD3D10COTablePlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_srv_define_entry(
    const SVGA3dCmdDXDefineShaderResourceView *src,
    SVGACOTableDXSRViewEntry *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_srv_destroy_entry(
    SVGACOTableDXSRViewEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_rtv_define_entry(
    const SVGA3dCmdDXDefineRenderTargetView *src,
    SVGACOTableDXRTViewEntry *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_rtv_destroy_entry(
    SVGACOTableDXRTViewEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_dsv_define_entry(
    const SVGA3dCmdDXDefineDepthStencilView_v2 *src,
    SVGACOTableDXDSViewEntry *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_dsv_destroy_entry(
    SVGACOTableDXDSViewEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_element_layout_define_entry(
    SVGA3dElementLayoutId layout_id, uint32_t count,
    const SVGA3dInputElementDesc *descs,
    SVGACOTableDXElementLayoutEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_element_layout_destroy_entry(
    SVGACOTableDXElementLayoutEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_blend_define_entry(
    const SVGA3dCmdDXDefineBlendState *src,
    SVGACOTableDXBlendStateEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_blend_destroy_entry(
    SVGACOTableDXBlendStateEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_depth_stencil_define_entry(
    const SVGA3dCmdDXDefineDepthStencilState *src,
    SVGACOTableDXDepthStencilEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_depth_stencil_destroy_entry(
    SVGACOTableDXDepthStencilEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_rasterizer_define_entry(
    const SVGA3dCmdDXDefineRasterizerState *src,
    SVGACOTableDXRasterizerStateEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_rasterizer_destroy_entry(
    SVGACOTableDXRasterizerStateEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_sampler_define_entry(
    const SVGA3dCmdDXDefineSamplerState *src,
    SVGACOTableDXSamplerEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_sampler_destroy_entry(
    SVGACOTableDXSamplerEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_constant_buffer_plan(
    uint32_t slot, SVGA3dShaderType type, SVGA3dSurfaceId sid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes, bool surface_available,
    uint32_t surface_bytes, bool has_surface_data,
    VMSVGA3DD3D10ConstantBufferPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_resources_set_plan(
    uint32_t start_view, SVGA3dShaderType type, uint32_t count,
    const SVGA3dShaderResourceViewId *ids, uint32_t view_table_count,
    VMSVGA3DD3D10ShaderResourceSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_set_plan(
    SVGA3dShaderId shader_id, SVGA3dShaderType type,
    uint32_t shader_table_count, VMSVGA3DD3D10ShaderSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_samplers_set_plan(
    uint32_t start_sampler, SVGA3dShaderType type, uint32_t count,
    const SVGA3dSamplerId *ids, uint32_t sampler_table_count,
    VMSVGA3DD3D10SamplerSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_input_layout_set_plan(
    SVGA3dElementLayoutId layout_id, uint32_t layout_table_count,
    VMSVGA3DD3D10InputLayoutSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_vertex_buffers_set_plan(
    uint32_t start_buffer, uint32_t count, const SVGA3dVertexBuffer *buffers,
    VMSVGA3DD3D10VertexBufferSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_vertex_buffer_pipeline_binding(
    bool has_buffer, uint32_t surface_bytes, uint32_t stride, uint32_t offset,
    VMSVGA3DD3D10VertexBufferPipelineBinding *binding);
VMSVGA3DD3D10Level vmsvga3d_d3d10_index_buffer_set_plan(
    SVGA3dSurfaceId sid, SVGA3dSurfaceFormat format, uint32_t offset,
    VMSVGA3DD3D10IndexBufferSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_viewports_set_plan(
    uint32_t count, const SVGA3dViewport *viewports,
    VMSVGA3DD3D10ViewportsSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_topology_set_plan(
    SVGA3dPrimitiveType topology, VMSVGA3DD3D10TopologySetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_blend_state_set_plan(
    const SVGA3dCmdDXSetBlendState *command, uint32_t table_count,
    VMSVGA3DD3D10BlendStateSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_depth_stencil_state_set_plan(
    const SVGA3dCmdDXSetDepthStencilState *command, uint32_t table_count,
    VMSVGA3DD3D10DepthStencilStateSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_rasterizer_state_set_plan(
    SVGA3dRasterizerStateId rasterizer_id, uint32_t table_count,
    VMSVGA3DD3D10RasterizerStateSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_pipeline_setup_plan(
    VMSVGA3DD3D10PipelineSetupPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_scissor_plan(
    uint32_t count, const SVGASignedRect *rects,
    VMSVGA3DD3D10ScissorPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_render_targets_set_plan(
    SVGA3dDepthStencilViewId depth_stencil_view_id, uint32_t count,
    const SVGA3dRenderTargetViewId *ids, uint32_t dsv_table_count,
    uint32_t rtv_table_count, uint32_t previous_remembered_count,
    VMSVGA3DD3D10RenderTargetsSetPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_render_targets_pipeline_plan(
    SVGA3dDepthStencilViewId depth_stencil_view_id,
    const SVGA3dRenderTargetViewId ids[SVGA3D_MAX_RENDER_TARGETS],
    uint32_t remembered_count,
    VMSVGA3DD3D10RenderTargetsPipelinePlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_gen_mips_plan(
    SVGA3dShaderResourceViewId view_id, uint32_t srv_table_count,
    VMSVGA3DD3D10GenMipsPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_clear_rtv_plan(
    SVGA3dRenderTargetViewId view_id, const SVGA3dRGBAFloat *rgba,
    uint32_t rtv_table_count, VMSVGA3DD3D10ClearRTVPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_clear_dsv_plan(
    uint32_t flags, SVGA3dDepthStencilViewId view_id, float depth,
    uint32_t stencil, uint32_t dsv_table_count,
    VMSVGA3DD3D10ClearDSVPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_copy_resource_plan(
    SVGA3dSurfaceFormat source_format, bool source_resource_exists,
    bool destination_resource_exists, VMSVGA3DD3D10CopyResourcePlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_copy_subresource_plan(
    SVGA3dSurfaceFormat source_format, bool source_resource_exists,
    bool destination_resource_exists, uint32_t destination_subresource,
    uint32_t source_subresource, const SVGA3dSize *source_size,
    const SVGA3dSize *destination_size, const SVGA3dCopyBox *box,
    VMSVGA3DD3D10CopySubresourcePlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_plan(
    SVGA3dPrimitiveType primitive, uint32_t vertex_count,
    uint32_t start_vertex_location, VMSVGA3DD3D10DrawPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_indexed_plan(
    SVGA3dPrimitiveType primitive, uint32_t index_count,
    uint32_t start_index_location, int32_t base_vertex_location,
    VMSVGA3DD3D10DrawPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_instanced_plan(
    SVGA3dPrimitiveType primitive, uint32_t vertex_count_per_instance,
    uint32_t instance_count, uint32_t start_vertex_location,
    uint32_t start_instance_location, VMSVGA3DD3D10DrawPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_indexed_instanced_plan(
    SVGA3dPrimitiveType primitive, uint32_t index_count_per_instance,
    uint32_t instance_count, uint32_t start_index_location,
    int32_t base_vertex_location, uint32_t start_instance_location,
    VMSVGA3DD3D10DrawPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_auto_plan(
    SVGA3dPrimitiveType primitive, VMSVGA3DD3D10DrawPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_indexed_triangle_fan_plan(
    uint32_t index_count, uint32_t start_index_location,
    int32_t base_vertex_location, uint32_t bound_dxgi_format,
    uint32_t bound_index_buffer_bytes, uint32_t bound_index_buffer_offset,
    VMSVGA3DD3D10TriangleFanPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_triangle_fan_generate_u16(
    bool indexed_source, uint32_t count, uint32_t source_dxgi_format,
    const void *source_indices, uint32_t source_bytes,
    uint16_t *generated_indices, uint32_t generated_capacity,
    uint32_t *generated_count);
VMSVGA3DD3D10Level vmsvga3d_d3d10_query_info(
    SVGA3dQueryType type, uint32_t flags, VMSVGA3DD3D10QueryInfo *info);
VMSVGA3DD3D10Level vmsvga3d_d3d10_query_define_entry(
    const SVGA3dCmdDXDefineQuery *src, SVGACOTableDXQueryEntry *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_query_destroy_entry(
    SVGACOTableDXQueryEntry *entry);
VMSVGA3DD3D10Level vmsvga3d_d3d10_query_bind_entry(
    SVGACOTableDXQueryEntry *entry, uint32_t mobid);
VMSVGA3DD3D10Level vmsvga3d_d3d10_query_set_offset(
    SVGACOTableDXQueryEntry *entry, uint32_t offset);
VMSVGA3DD3D10Level vmsvga3d_d3d10_query_bind_all(
    SVGACOTableDXQueryEntry *entries, uint32_t count, uint32_t mobid);
VMSVGA3DD3D10Level vmsvga3d_d3d10_query_execution_plan(
    SVGA3dQueryType type, uint32_t flags,
    VMSVGA3DD3D10QueryExecutionPlan *plan);
VMSVGA3DD3D10Level vmsvga3d_d3d10_query_result(
    SVGA3dQueryType type, const void *d3d_result, uint32_t d3d_result_size,
    SVGADXQueryResultUnion *svga_result, uint32_t *svga_result_size);
VMSVGA3DD3D10Level vmsvga3d_d3d10_predication_plan(
    bool enabled, SVGA3dQueryType type, uint32_t flags,
    uint32_t predicate_value, VMSVGA3DD3D10PredicationPlan *plan);
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
