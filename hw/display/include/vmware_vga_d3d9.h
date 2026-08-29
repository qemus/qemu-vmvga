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

#ifndef HW_DISPLAY_VMWARE_VGA_D3D9_H
#define HW_DISPLAY_VMWARE_VGA_D3D9_H

#include <stdbool.h>
#include <stdint.h>

#include "svga_types.h"
#include "svga_reg.h"
#include "svga3d_cmd.h"
#include "svga3d_types.h"

/*
 * This interface deliberately uses D3D9 ABI values without including D3D9
 * headers.  The future native-D3D9 bridge can therefore live in C++ while the
 * SVGA protocol layer remains plain C.  The bridge should static_assert these
 * ABI values against the D3D9 headers it is compiled with.
 */

#define VMSVGA3D_D3D9_MAX_TEXTURE_STAGES 8u
#define VMSVGA3D_D3D9_MAX_PIXEL_SAMPLERS 16u
#define VMSVGA3D_D3D9_DMAP_SAMPLER 256u
#define VMSVGA3D_D3D9_MAX_SAMPLERS 21u
#define VMSVGA3D_D3D9_DECL_END_STREAM 0xffu

#define VMSVGA3D_D3D9_MAKE_FOURCC(a, b, c, d) \
  ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | \
   ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

typedef enum vmsvga3d_d3d9_translate_result_e {
  VMSVGA3D_D3D9_TRANSLATE_INVALID = 0,
  VMSVGA3D_D3D9_TRANSLATE_IGNORE,
  VMSVGA3D_D3D9_TRANSLATE_EMIT,
} VMSVGA3DD3D9TranslateResult;

typedef struct vmsvga3d_d3d9_state_op_s {
  uint32_t state;
  uint32_t value;
} VMSVGA3DD3D9StateOp;

typedef struct vmsvga3d_d3d9_render_state_plan_s {
  uint32_t count;
  VMSVGA3DD3D9StateOp ops[2];
} VMSVGA3DD3D9RenderStatePlan;

typedef enum vmsvga3d_d3d9_texture_action_e {
  VMSVGA3D_D3D9_TEXTURE_ACTION_NONE = 0,
  VMSVGA3D_D3D9_TEXTURE_ACTION_BIND,
  VMSVGA3D_D3D9_TEXTURE_ACTION_STAGE_STATE,
  VMSVGA3D_D3D9_TEXTURE_ACTION_SAMPLER_STATE,
} VMSVGA3DD3D9TextureAction;

typedef struct vmsvga3d_d3d9_texture_state_plan_s {
  VMSVGA3DD3D9TextureAction action;
  uint32_t stage;
  uint32_t state;
  uint32_t value;
} VMSVGA3DD3D9TextureStatePlan;

typedef enum vmsvga3d_d3d9_render_target_action_e {
  VMSVGA3D_D3D9_RT_ACTION_NONE = 0,
  VMSVGA3D_D3D9_RT_ACTION_DEPTH_STENCIL,
  VMSVGA3D_D3D9_RT_ACTION_COLOR,
} VMSVGA3DD3D9RenderTargetAction;

typedef struct vmsvga3d_d3d9_render_target_plan_s {
  VMSVGA3DD3D9RenderTargetAction action;
  uint32_t color_index;
  bool unbind;
  bool restore_viewport_zrange_scissor;
} VMSVGA3DD3D9RenderTargetPlan;

typedef struct vmsvga3d_d3d9_color_s {
  float r;
  float g;
  float b;
  float a;
} VMSVGA3DD3D9Color;

typedef struct vmsvga3d_d3d9_vector_s {
  float x;
  float y;
  float z;
} VMSVGA3DD3D9Vector;

typedef struct vmsvga3d_d3d9_material_s {
  VMSVGA3DD3D9Color diffuse;
  VMSVGA3DD3D9Color ambient;
  VMSVGA3DD3D9Color specular;
  VMSVGA3DD3D9Color emissive;
  float power;
} VMSVGA3DD3D9Material;

typedef struct vmsvga3d_d3d9_light_s {
  uint32_t type;
  VMSVGA3DD3D9Color diffuse;
  VMSVGA3DD3D9Color specular;
  VMSVGA3DD3D9Color ambient;
  VMSVGA3DD3D9Vector position;
  VMSVGA3DD3D9Vector direction;
  float range;
  float falloff;
  float attenuation0;
  float attenuation1;
  float attenuation2;
  float theta;
  float phi;
} VMSVGA3DD3D9Light;

typedef struct vmsvga3d_d3d9_viewport_s {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  float min_z;
  float max_z;
} VMSVGA3DD3D9Viewport;

typedef struct vmsvga3d_d3d9_rect_s {
  int32_t left;
  int32_t top;
  int32_t right;
  int32_t bottom;
} VMSVGA3DD3D9Rect;

typedef struct vmsvga3d_d3d9_vertex_element_s {
  uint16_t stream;
  uint16_t offset;
  uint8_t type;
  uint8_t method;
  uint8_t usage;
  uint8_t usage_index;
} VMSVGA3DD3D9VertexElement;

typedef struct vmsvga3d_d3d9_vertex_stream_s {
  uint32_t surface_id;
  uint32_t source_offset;
  uint32_t stride;
  uint32_t frequency;
  uint32_t first_decl;
  uint32_t decl_count;
} VMSVGA3DD3D9VertexStream;

typedef struct vmsvga3d_d3d9_indexed_draw_s {
  int32_t base_vertex_index;
  uint32_t min_vertex_index;
  uint32_t num_vertices;
  uint32_t start_index;
  uint32_t primitive_count;
} VMSVGA3DD3D9IndexedDraw;

typedef struct vmsvga3d_d3d9_query_plan_s {
  uint32_t query_type;
  uint32_t issue_begin;
  uint32_t issue_end;
  uint32_t getdata_flags;
  uint32_t result_size;
  bool wait_until_ready;
} VMSVGA3DD3D9QueryPlan;

typedef struct vmsvga3d_d3d9_mipmap_plan_s {
  uint32_t filter;
  bool store_filter;
  bool require_associated_context;
  bool create_texture_if_missing;
  bool allow_texture;
  bool allow_cube_texture;
  bool allow_volume_texture;
  bool set_autogen_filter;
  bool filter_debug_assert;
  bool filter_failure_is_fatal;
  bool generate_after_filter_failure;
  bool generate_sublevels;
} VMSVGA3DD3D9MipmapPlan;

typedef enum vmsvga3d_d3d9_resource_use_e {
  VMSVGA3D_D3D9_RESOURCE_USE_TEXTURE = 0,
  VMSVGA3D_D3D9_RESOURCE_USE_COLOR_TARGET,
  VMSVGA3D_D3D9_RESOURCE_USE_DEPTH_TARGET,
  VMSVGA3D_D3D9_RESOURCE_USE_VERTEX_BUFFER,
  VMSVGA3D_D3D9_RESOURCE_USE_INDEX_BUFFER,
} VMSVGA3DD3D9ResourceUse;

typedef struct vmsvga3d_d3d9_resource_caps_s {
  bool supports_uyvy;
  bool supports_yuy2;
  bool supports_a8b8g8r8;
  bool supports_intz;
} VMSVGA3DD3D9ResourceCaps;

typedef struct vmsvga3d_d3d9_surface_info_s {
  SVGA3dSurface1Flags surface_flags;
  SVGA3dSurfaceFormat format;
  SVGA3dSize size;
  uint32_t mip_levels;
  uint32_t multisample_count;
  SVGA3dTextureFilter autogen_filter;
  uint32_t surface_bytes;
  uint32_t index_width;
  uint32_t multisample_quality_levels;
} VMSVGA3DD3D9SurfaceInfo;

typedef struct vmsvga3d_d3d9_create_desc_s {
  bool valid;
  uint32_t resource_type;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t levels;
  uint32_t length;
  uint32_t usage;
  uint32_t format;
  uint32_t pool;
  uint32_t multisample_type;
  uint32_t multisample_quality;
  bool lockable;
  bool discard;
  bool shared_handle;
} VMSVGA3DD3D9CreateDesc;

typedef struct vmsvga3d_d3d9_resource_plan_s {
  VMSVGA3DD3D9ResourceUse use;
  SVGA3dSurface1Flags normalized_surface_flags;
  SVGA3dSurface1Flags post_surface_flags;
  uint32_t requested_format;
  uint32_t actual_format;
  uint32_t base_usage;
  uint32_t post_usage;
  VMSVGA3DD3D9CreateDesc primary;
  VMSVGA3DD3D9CreateDesc bounce;
  VMSVGA3DD3D9CreateDesc fallback;
  VMSVGA3DD3D9CreateDesc emulated;
  VMSVGA3DD3D9CreateDesc surface_fallback;
  bool has_bounce;
  bool has_fallback;
  bool has_emulated;
  bool has_surface_fallback;
  bool stencil_as_texture;
  bool needs_format_conversion;
  bool set_autogen_filter;
  uint32_t autogen_filter;
} VMSVGA3DD3D9ResourcePlan;

typedef enum vmsvga3d_d3d9_host_resource_type_e {
  VMSVGA3D_D3D9_HOST_RESOURCE_NONE = 0,
  VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE,
  VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE,
  VMSVGA3D_D3D9_HOST_RESOURCE_CUBE_TEXTURE,
  VMSVGA3D_D3D9_HOST_RESOURCE_VOLUME_TEXTURE,
  VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER,
  VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER,
} VMSVGA3DD3D9HostResourceType;

typedef enum vmsvga3d_d3d9_execution_preference_e {
  VMSVGA3D_D3D9_EXECUTION_CPU_ONLY = 0,
  VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED,
} VMSVGA3DD3D9ExecutionPreference;

typedef struct vmsvga3d_d3d9_transfer_surface_s {
  VMSVGA3DD3D9HostResourceType resource_type;
  SVGA3dSurface1Flags surface_flags;
  uint32_t usage;
  uint32_t block_width;
  uint32_t block_height;
  uint32_t block_depth;
  uint32_t bytes_per_block;
  bool resident;
  bool has_bounce;
} VMSVGA3DD3D9TransferSurface;

typedef enum vmsvga3d_d3d9_copy_fallback_e {
  VMSVGA3D_D3D9_COPY_FALLBACK_NONE = 0,
  VMSVGA3D_D3D9_COPY_FALLBACK_READBACK_UPDATE,
  VMSVGA3D_D3D9_COPY_FALLBACK_LOCK_BOTH,
} VMSVGA3DD3D9CopyFallback;

typedef struct vmsvga3d_d3d9_surface_copy_plan_s {
  VMSVGA3DD3D9ExecutionPreference execution;
  VMSVGA3DD3D9CopyFallback gpu_failure_fallback;
  bool cpu_fallback_allowed;
  bool create_destination_texture;
  bool reject_volume_texture;
  bool require_identical_layout;
  bool flush_source;
  bool flush_destination;
  bool stretch_rect;
  uint32_t stretch_filter;
  bool lock_single_gpu_surface;
  bool lock_source_readonly;
  bool mark_cpu_destination_dirty;
  bool update_destination_texture;
  bool track_source;
  bool track_destination;
  bool skip_identical_self_copy;
  bool require_zero_z;
} VMSVGA3DD3D9SurfaceCopyPlan;

typedef struct vmsvga3d_d3d9_stretch_blt_plan_s {
  VMSVGA3DD3D9ExecutionPreference execution;
  bool cpu_fallback_allowed;
  bool require_existing_context;
  bool create_source_texture;
  bool create_destination_texture;
  bool reject_volume_texture;
  bool flush_source;
  bool flush_destination;
  bool stretch_rect;
  uint32_t filter;
  bool filter_debug_assert;
  bool require_zero_z;
  bool track_source;
  bool track_destination;
} VMSVGA3DD3D9StretchBltPlan;

typedef enum vmsvga3d_d3d9_dma_path_e {
  VMSVGA3D_D3D9_DMA_PATH_CPU_SHADOW = 0,
  VMSVGA3D_D3D9_DMA_PATH_GPU_SURFACE,
  VMSVGA3D_D3D9_DMA_PATH_BUFFER_SHADOW,
} VMSVGA3DD3D9DmaPath;

typedef struct vmsvga3d_d3d9_dma_plan_s {
  VMSVGA3DD3D9ExecutionPreference execution;
  VMSVGA3DD3D9DmaPath path;
  bool cpu_fallback_allowed;
  bool reject_volume_texture;
  bool flush_surface;
  bool use_bounce_surface;
  bool readback_render_target_first_box;
  bool lock_readonly;
  bool gmr_transfer;
  bool update_texture_after_write;
  bool track_after_update;
  bool mark_mipmap_dirty;
  bool mark_surface_dirty;
} VMSVGA3DD3D9DmaPlan;

typedef enum vmsvga3d_d3d9_shader_stage_e {
  VMSVGA3D_D3D9_SHADER_STAGE_INVALID = 0,
  VMSVGA3D_D3D9_SHADER_STAGE_VERTEX,
  VMSVGA3D_D3D9_SHADER_STAGE_PIXEL,
} VMSVGA3DD3D9ShaderStage;

typedef enum vmsvga3d_d3d9_shader_const_target_e {
  VMSVGA3D_D3D9_CONST_TARGET_INVALID = 0,
  VMSVGA3D_D3D9_CONST_TARGET_VS_FLOAT,
  VMSVGA3D_D3D9_CONST_TARGET_VS_INT,
  VMSVGA3D_D3D9_CONST_TARGET_VS_BOOL,
  VMSVGA3D_D3D9_CONST_TARGET_PS_FLOAT,
  VMSVGA3D_D3D9_CONST_TARGET_PS_INT,
  VMSVGA3D_D3D9_CONST_TARGET_PS_BOOL,
} VMSVGA3DD3D9ShaderConstTarget;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t vmsvga3d_d3d9_surface_format(SVGA3dSurfaceFormat format);
SVGA3dSurface1Flags vmsvga3d_d3d9_normalize_surface_flags(
    SVGA3dSurface1Flags flags, SVGA3dSurfaceFormat format);
uint32_t vmsvga3d_d3d9_surface_usage(SVGA3dSurface1Flags flags);
uint32_t vmsvga3d_d3d9_actual_format(
    uint32_t requested_format, const VMSVGA3DD3D9ResourceCaps *caps);
bool vmsvga3d_d3d9_resource_plan(
    const VMSVGA3DD3D9SurfaceInfo *surface, VMSVGA3DD3D9ResourceUse use,
    const VMSVGA3DD3D9ResourceCaps *caps, VMSVGA3DD3D9ResourcePlan *plan);
uint32_t vmsvga3d_d3d9_multisample_type(uint32_t sample_count);
bool vmsvga3d_d3d9_transform_type(SVGA3dTransformType type,
                                   uint32_t *d3d_transform);
void vmsvga3d_d3d9_apply_z_range(VMSVGA3DD3D9Viewport *viewport,
                                  const SVGA3dZRange *z_range);
VMSVGA3DD3D9TranslateResult
vmsvga3d_d3d9_render_state(const SVGA3dRenderState *state,
                            VMSVGA3DD3D9RenderStatePlan *plan);
VMSVGA3DD3D9TranslateResult
vmsvga3d_d3d9_render_target(SVGA3dRenderTargetType type,
                             const SVGA3dSurfaceImageId *target,
                             VMSVGA3DD3D9RenderTargetPlan *plan);
uint32_t vmsvga3d_d3d9_sampler_index(uint32_t stage);
VMSVGA3DD3D9TranslateResult
vmsvga3d_d3d9_texture_state(const SVGA3dTextureState *state,
                             VMSVGA3DD3D9TextureStatePlan *plan);
bool vmsvga3d_d3d9_material(SVGA3dFace face,
                             const SVGA3dMaterial *material,
                             VMSVGA3DD3D9Material *d3d_material);
bool vmsvga3d_d3d9_light(const SVGA3dLightData *light,
                          VMSVGA3DD3D9Light *d3d_light);
void vmsvga3d_d3d9_apply_viewport(VMSVGA3DD3D9Viewport *viewport,
                                   const SVGA3dRect *rect);
void vmsvga3d_d3d9_rect(const SVGA3dRect *rect,
                         VMSVGA3DD3D9Rect *d3d_rect);
uint32_t vmsvga3d_d3d9_clear_flags(SVGA3dClearFlag flags);
bool vmsvga3d_d3d9_vertex_element(const SVGA3dVertexArrayIdentity *identity,
                                   uint32_t stream, uint32_t offset,
                                   VMSVGA3DD3D9VertexElement *element);
bool vmsvga3d_d3d9_vertex_layout(
    const SVGA3dVertexDecl *decls, uint32_t decl_count,
    const SVGA3dVertexDivisor *divisors, uint32_t divisor_count,
    VMSVGA3DD3D9VertexElement *elements, uint32_t element_capacity,
    VMSVGA3DD3D9VertexStream *streams, uint32_t stream_capacity,
    uint32_t *stream_count);
uint32_t vmsvga3d_d3d9_index_format(uint32_t index_width);
bool vmsvga3d_d3d9_indexed_draw(const SVGA3dVertexDecl *first_decl,
                                 const SVGA3dPrimitiveRange *range,
                                 uint32_t vertex_buffer_bytes,
                                 VMSVGA3DD3D9IndexedDraw *draw);
uint32_t vmsvga3d_d3d9_texture_filter(SVGA3dTextureFilter filter);
bool vmsvga3d_d3d9_mipmap_plan(SVGA3dTextureFilter filter,
                                 VMSVGA3DD3D9MipmapPlan *plan);
bool vmsvga3d_d3d9_surface_copy_plan(
    const VMSVGA3DD3D9TransferSurface *source,
    const VMSVGA3DD3D9TransferSurface *destination,
    VMSVGA3DD3D9SurfaceCopyPlan *plan);
bool vmsvga3d_d3d9_stretch_blt_plan(
    const VMSVGA3DD3D9TransferSurface *source,
    const VMSVGA3DD3D9TransferSurface *destination,
    SVGA3dStretchBltMode mode, VMSVGA3DD3D9StretchBltPlan *plan);
bool vmsvga3d_d3d9_dma_plan(
    const VMSVGA3DD3D9TransferSurface *surface,
    SVGA3dTransferType transfer, bool first_box,
    VMSVGA3DD3D9DmaPlan *plan);
bool vmsvga3d_d3d9_query_plan(SVGA3dQueryType type,
                               VMSVGA3DD3D9QueryPlan *plan);
bool vmsvga3d_d3d9_primitive_type(SVGA3dPrimitiveType type,
                                   uint32_t primitive_count,
                                   uint32_t *d3d_primitive);
VMSVGA3DD3D9ShaderStage vmsvga3d_d3d9_shader_stage(SVGA3dShaderType type);
VMSVGA3DD3D9ShaderConstTarget
vmsvga3d_d3d9_shader_const_target(SVGA3dShaderType type,
                                   SVGA3dShaderConstType ctype);

#ifdef __cplusplus
}
#endif

#endif
