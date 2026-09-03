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

#include "qemu/osdep.h"
#include "qapi/error.h"

#include "include/vmware_vga_d3d9.h"
#include "include/vmware_vga_d3d10.h"
#include "include/vmware_vga_dxvk.h"
#include "include/vmware_vga_dxvk_wsi.h"

typedef struct vmsvga3d_dxvk_d3d9_query_s VMSVGA3DDxvkD3D9Query;
typedef struct vmsvga3d_dxvk_query_s VMSVGA3DDxvkQuery;
typedef struct vmsvga3d_dxvk_state_s VMSVGA3DDxvkState;
typedef struct vmsvga3d_dxvk_shader_s VMSVGA3DDxvkShader;
typedef struct vmsvga3d_dxvk_stream_output_s VMSVGA3DDxvkStreamOutput;
typedef struct vmsvga3d_dxvk_input_layout_s VMSVGA3DDxvkInputLayout;
typedef struct vmsvga3d_dxvk_constant_buffer_s VMSVGA3DDxvkConstantBuffer;
typedef struct vmsvga3d_dxvk_view_s VMSVGA3DDxvkView;

struct vmsvga3d_dxvk_s {
  VMSVGA3DDxvkWsi *wsi;
  void *d3d9_library;
  void *d3d11_library;
  void *d3d9;
  void *d3d9_device;
  void *d3d9_pristine_state;
  VMSVGA3DDxvkD3D9Query *d3d9_queries;
  void *d3d11_device;
  void *d3d11_context;
  VMSVGA3DDxvkQuery *d3d11_queries;
  VMSVGA3DDxvkState *d3d11_states;
  VMSVGA3DDxvkShader *d3d11_shaders;
  VMSVGA3DDxvkStreamOutput *d3d11_stream_outputs;
  VMSVGA3DDxvkInputLayout *d3d11_input_layouts;
  VMSVGA3DDxvkConstantBuffer *d3d11_constant_buffers;
  VMSVGA3DDxvkView *d3d11_views;
  void *d3d11_bound_constant_buffers[SVGA3D_NUM_SHADERTYPE_DX10]
                                      [SVGA3D_DX_MAX_CONSTBUFFERS];
  bool d3d11_bound_constant_buffer_valid[SVGA3D_NUM_SHADERTYPE_DX10]
                                           [SVGA3D_DX_MAX_CONSTBUFFERS];
  void *d3d11_bound_vertex_buffers[SVGA3D_DX_MAX_VERTEXBUFFERS];
  uint32_t d3d11_bound_vertex_strides[SVGA3D_DX_MAX_VERTEXBUFFERS];
  uint32_t d3d11_bound_vertex_offsets[SVGA3D_DX_MAX_VERTEXBUFFERS];
  bool d3d11_bound_vertex_buffer_valid[SVGA3D_DX_MAX_VERTEXBUFFERS];
  void *d3d11_bound_index_buffer;
  uint32_t d3d11_bound_index_format;
  uint32_t d3d11_bound_index_offset;
  bool d3d11_bound_index_buffer_valid;
  void *d3d11_blit_constant_buffer;
  void *d3d11_blit_vertex_shader;
  void *d3d11_blit_pixel_shader;
  void *d3d11_blit_pixel_shader_srgb;
  void *d3d11_blit_sampler_state;
  void *d3d11_blit_rasterizer_state;
  void *d3d11_blit_blend_state;
  bool d3d11_blitter_initialized;
  bool ready;
};

struct vmsvga3d_dxvk_d3d9_query_s {
  uint32_t cid;
  void *query;
  VMSVGA3DDxvkD3D9Query *next;
};

struct vmsvga3d_dxvk_view_s {
  uint32_t cid;
  uint32_t view_id;
  VMSVGA3DDxvkViewKind kind;
  VMSVGA3DDxvkSurface *surface;
  void *view;
  VMSVGA3DDxvkView *next;
};

struct vmsvga3d_dxvk_query_s {
  uint32_t cid;
  uint32_t query_id;
  uint32_t d3d_query;
  uint32_t misc_flags;
  bool pending;
  void *query;
  VMSVGA3DDxvkQuery *next;
};

typedef enum vmsvga3d_dxvk_state_kind_e {
  VMSVGA3D_DXVK_STATE_BLEND = 0,
  VMSVGA3D_DXVK_STATE_DEPTH_STENCIL,
  VMSVGA3D_DXVK_STATE_RASTERIZER,
  VMSVGA3D_DXVK_STATE_SAMPLER,
} VMSVGA3DDxvkStateKind;

struct vmsvga3d_dxvk_state_s {
  uint32_t cid;
  uint32_t state_id;
  VMSVGA3DDxvkStateKind kind;
  void *state;
  VMSVGA3DDxvkState *next;
};

/* Mirror VirtualBox DXSHADER lifetime: Define creates the backend record,
 * Bind stores parsed shader information, and pipeline setup later creates
 * DXBC and the native shader.
 */
struct vmsvga3d_dxvk_shader_s {
  uint32_t cid;
  uint32_t shader_id;
  uint32_t shader_type;
  VMSVGA3DD3D10ShaderInfo info;
  bool info_valid;
  void *shader;
  uint8_t *bytecode;
  uint32_t bytecode_size;
  uint32_t stream_output_id;
  VMSVGA3DDxvkShader *next;
};

/* VirtualBox resolves a stream-output declaration only when a GS first uses
 * the SO id.  The resolved declaration is then cached per SO id until the
 * entry is redefined, destroyed, or replayed from a COTable.
 */
struct vmsvga3d_dxvk_stream_output_s {
  uint32_t cid;
  uint32_t stream_output_id;
  VMSVGA3DD3D10StreamOutputPlan plan;
  VMSVGA3DDxvkStreamOutput *next;
};

struct vmsvga3d_dxvk_input_layout_s {
  uint32_t cid;
  uint32_t layout_id;
  void *layout;
  VMSVGA3DDxvkInputLayout *next;
};

struct vmsvga3d_dxvk_constant_buffer_s {
  uint32_t cid;
  uint32_t stage_index;
  uint32_t slot;
  void *buffer;
  VMSVGA3DDxvkConstantBuffer *next;
};

/*
 * Renderer-side lifetime object for one guest SVGA3D surface. D3D9 and D3D11
 * residency are deliberately independent; crossing generations without an
 * explicit synchronization path is rejected instead of discarding GPU data.
 */
struct vmsvga3d_dxvk_surface_s {
  uint32_t sid;
  VMSVGA3DDxvk *owner;

  VMSVGA3DD3D9HostResourceType d3d9_resource_type;
  uint32_t d3d9_usage;
  uint32_t d3d9_format;
  uint32_t d3d9_length;
  void *d3d9_resource;
  void *d3d9_bounce;
  bool d3d9_resident;
  bool d3d9_has_bounce;

  VMSVGA3DD3D10CreateDesc d3d11_desc;
  void *d3d11_resource;
  bool d3d11_resident;
};

#if defined(CONFIG_LINUX) && defined(__ELF__)

#include <dlfcn.h>

#define VMSVGA3D_DXVK_D3D9_SONAME "libdxvk_d3d9.so.0"
#define VMSVGA3D_DXVK_D3D11_SONAME "libdxvk_d3d11.so.0"
#define VMSVGA3D_DXVK_WSI_ENV "DXVK_WSI_DRIVER"
#define VMSVGA3D_DXVK_WSI_VALUE "SDL2"
#define VMSVGA3D_DXVK_CONFIG_FILE_ENV "DXVK_CONFIG_FILE"
#define VMSVGA3D_DXVK_CONFIG_FILE_VALUE "/dev/null"
#define VMSVGA3D_DXVK_LOG_LEVEL_ENV "DXVK_LOG_LEVEL"
#define VMSVGA3D_DXVK_LOG_LEVEL_QUIET "none"
#define VMSVGA3D_DXVK_DEBUG_ENV "DEBUG"

#define VMSVGA3D_DXVK_VULKAN_SONAME "libvulkan.so.1"
#define VMSVGA3D_DXVK_VULKAN_API_1_3 ((1u << 22) | (3u << 12))
#define VMSVGA3D_DXVK_VK_SUCCESS 0
#define VMSVGA3D_DXVK_VK_STRUCTURE_TYPE_APPLICATION_INFO 0u
#define VMSVGA3D_DXVK_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1u
#define VMSVGA3D_DXVK_VK_KHR_SURFACE "VK_KHR_surface"
#define VMSVGA3D_DXVK_VK_EXT_HEADLESS_SURFACE "VK_EXT_headless_surface"
#define VMSVGA3D_DXVK_VK_KHR_MAINTENANCE5 "VK_KHR_maintenance5"
#define VMSVGA3D_DXVK_VK_EXT_ROBUSTNESS2 "VK_EXT_robustness2"
#define VMSVGA3D_DXVK_VK_MAX_EXTENSION_NAME_SIZE 256u
#define VMSVGA3D_DXVK_VK_PROPERTIES_BUFFER_SIZE 4096u

#define VMSVGA3D_DXVK_D3D_SDK_VERSION 32u
#define VMSVGA3D_DXVK_D3DADAPTER_DEFAULT 0u
#define VMSVGA3D_DXVK_D3DDEVTYPE_HAL 1u
#define VMSVGA3D_DXVK_D3DFMT_X8R8G8B8 22u
#define VMSVGA3D_DXVK_D3DMULTISAMPLE_NONE 0u
#define VMSVGA3D_DXVK_D3DSWAPEFFECT_DISCARD 1u
#define VMSVGA3D_DXVK_D3DCREATE_MULTITHREADED 0x00000004u
#define VMSVGA3D_DXVK_D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x00000020u
#define VMSVGA3D_DXVK_D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040u
#define VMSVGA3D_DXVK_D3DPRESENT_INTERVAL_IMMEDIATE 0x80000000u
#define VMSVGA3D_DXVK_DEVICE_DEFAULT_WIDTH 640u
#define VMSVGA3D_DXVK_DEVICE_DEFAULT_HEIGHT 480u
#define VMSVGA3D_DXVK_IUNKNOWN_ADDREF 1u
#define VMSVGA3D_DXVK_IDIRECT3D9_RELEASE 2u
#define VMSVGA3D_DXVK_IDIRECT3D9_CREATE_DEVICE 16u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE 2u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_TEXTURE 23u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_VERTEX_BUFFER 26u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_INDEX_BUFFER 27u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_RENDER_TARGET 28u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_DEPTH_STENCIL_SURFACE 29u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_UPDATE_SURFACE 30u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_RENDER_TARGET_DATA 32u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_STRETCH_RECT 34u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_OFFSCREEN_PLAIN_SURFACE 36u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_RENDER_TARGET 37u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_RENDER_TARGET 38u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_DEPTH_STENCIL_SURFACE 39u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_DEPTH_STENCIL_SURFACE 40u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_BEGIN_SCENE 41u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_END_SCENE 42u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CLEAR 43u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_TRANSFORM 44u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VIEWPORT 47u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_MATERIAL 49u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_LIGHT 51u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_LIGHT_ENABLE 53u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_CLIP_PLANE 55u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_RENDER_STATE 57u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_STATE_BLOCK 59u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_TEXTURE 65u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_TEXTURE_STAGE_STATE 67u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_SAMPLER_STATE 69u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_SCISSOR_RECT 75u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_SCISSOR_RECT 76u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_DRAW_PRIMITIVE 81u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_DRAW_INDEXED_PRIMITIVE 82u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_VERTEX_DECLARATION 86u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_DECLARATION 87u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_VERTEX_SHADER 91u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_SHADER 92u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_SHADER_CONSTANT_F 94u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_SHADER_CONSTANT_I 96u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_SHADER_CONSTANT_B 98u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_STREAM_SOURCE 100u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_STREAM_SOURCE_FREQ 102u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_INDICES 104u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_PIXEL_SHADER 106u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_PIXEL_SHADER 107u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_PIXEL_SHADER_CONSTANT_F 109u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_PIXEL_SHADER_CONSTANT_I 111u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_PIXEL_SHADER_CONSTANT_B 113u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_QUERY 118u
#define VMSVGA3D_DXVK_IDIRECT3DQUERY9_RELEASE 2u
#define VMSVGA3D_DXVK_IDIRECT3DQUERY9_ISSUE 6u
#define VMSVGA3D_DXVK_IDIRECT3DQUERY9_GET_DATA 7u
#define VMSVGA3D_DXVK_D3D_S_FALSE 1
#define VMSVGA3D_DXVK_IDIRECT3DBASETEXTURE9_SET_AUTOGEN_FILTER_TYPE 14u
#define VMSVGA3D_DXVK_IDIRECT3DBASETEXTURE9_GENERATE_MIP_SUB_LEVELS 16u
#define VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_GET_SURFACE_LEVEL 18u
#define VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_LOCK_RECT 19u
#define VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_UNLOCK_RECT 20u
#define VMSVGA3D_DXVK_IDIRECT3DSURFACE9_LOCK_RECT 13u
#define VMSVGA3D_DXVK_IDIRECT3DSURFACE9_UNLOCK_RECT 14u
#define VMSVGA3D_DXVK_IDIRECT3DBUFFER9_LOCK 11u
#define VMSVGA3D_DXVK_IDIRECT3DBUFFER9_UNLOCK 12u
#define VMSVGA3D_DXVK_IDIRECT3DSTATEBLOCK9_RELEASE 2u
#define VMSVGA3D_DXVK_IDIRECT3DSTATEBLOCK9_APPLY 5u
#define VMSVGA3D_DXVK_D3D9_RTYPE_SURFACE 1u
#define VMSVGA3D_DXVK_D3D9_RTYPE_TEXTURE 3u
#define VMSVGA3D_DXVK_D3D9_RTYPE_VERTEX_BUFFER 6u
#define VMSVGA3D_DXVK_D3D9_RTYPE_INDEX_BUFFER 7u
#define VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET 0x00000001u
#define VMSVGA3D_DXVK_D3DUSAGE_DEPTHSTENCIL 0x00000002u
#define VMSVGA3D_DXVK_D3DPOOL_DEFAULT 0u
#define VMSVGA3D_DXVK_D3DPOOL_SYSTEMMEM 2u
#define VMSVGA3D_DXVK_D3DLOCK_READONLY 0x00000010u
#define VMSVGA3D_DXVK_D3DLOCK_DISCARD 0x00002000u
#define VMSVGA3D_DXVK_D3DSBT_ALL 1u

#define VMSVGA3D_DXVK_D3D11_SDK_VERSION 7u
#define VMSVGA3D_DXVK_D3D_DRIVER_TYPE_HARDWARE 1u
#define VMSVGA3D_DXVK_D3D_FEATURE_LEVEL_11_0 0xb000u
#define VMSVGA3D_DXVK_IUNKNOWN_RELEASE 2u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BUFFER 3u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_TEXTURE1D 4u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_TEXTURE2D 5u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_TEXTURE3D 6u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_SHADER_RESOURCE_VIEW 7u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_RENDERTARGET_VIEW 9u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_DEPTH_STENCIL_VIEW 10u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_INPUT_LAYOUT 11u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_VERTEX_SHADER 12u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_GEOMETRY_SHADER 13u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_GEOMETRY_SHADER_WITH_SO 14u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_PIXEL_SHADER 15u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BLEND_STATE 20u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_DEPTH_STENCIL_STATE 21u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_RASTERIZER_STATE 22u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_SAMPLER_STATE 23u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_QUERY 24u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_PREDICATE 25u
#define VMSVGA3D_DXVK_ID3D11BUFFER_GET_DESC 10u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_CONSTANT_BUFFERS 7u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SHADER_RESOURCES 8u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SHADER 9u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SAMPLERS 10u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_SHADER 11u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW_INDEXED 12u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW 13u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_MAP 14u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_UNMAP 15u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_CONSTANT_BUFFERS 16u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_INPUT_LAYOUT 17u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_VERTEX_BUFFERS 18u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_INDEX_BUFFER 19u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW_INDEXED_INSTANCED 20u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW_INSTANCED 21u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_CONSTANT_BUFFERS 22u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_SHADER 23u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_PRIMITIVE_TOPOLOGY 24u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_SHADER_RESOURCES 25u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_SAMPLERS 26u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_BEGIN 27u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_END 28u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GET_DATA 29u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_SET_PREDICATION 30u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_SHADER_RESOURCES 31u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_SAMPLERS 32u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_RENDER_TARGETS 33u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_RENDER_TARGETS_AND_UAVS 34u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_BLEND_STATE 35u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_DEPTH_STENCIL_STATE 36u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_SO_SET_TARGETS 37u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW_AUTO 38u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_STATE 43u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_VIEWPORTS 44u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_SCISSOR_RECTS 45u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_COPY_SUBRESOURCE_REGION 46u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_COPY_RESOURCE 47u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_UPDATE_SUBRESOURCE 48u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_CLEAR_RENDERTARGET_VIEW 50u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_CLEAR_DEPTH_STENCIL_VIEW 53u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GENERATE_MIPS 54u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RESOLVE_SUBRESOURCE 57u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_GET_CONSTANT_BUFFERS 72u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_GET_SHADER_RESOURCES 73u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_GET_SHADER 74u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_GET_SAMPLERS 75u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_GET_SHADER 76u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_GET_PRIMITIVE_TOPOLOGY 83u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_GET_INPUT_LAYOUT 78u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_GET_INDEX_BUFFER 80u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_GET_SHADER 82u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_GET_RENDER_TARGETS 89u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_GET_BLEND_STATE 91u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_GET_STATE 94u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_GET_VIEWPORTS 95u
#define VMSVGA3D_DXVK_D3D11_USAGE_DEFAULT 0u
#define VMSVGA3D_DXVK_D3D11_USAGE_IMMUTABLE 1u
#define VMSVGA3D_DXVK_D3D11_USAGE_DYNAMIC 2u
#define VMSVGA3D_DXVK_D3D11_USAGE_STAGING 3u
#define VMSVGA3D_DXVK_D3D11_BIND_VERTEX_BUFFER 0x01u
#define VMSVGA3D_DXVK_D3D11_BIND_INDEX_BUFFER 0x02u
#define VMSVGA3D_DXVK_D3D11_BIND_CONSTANT_BUFFER 0x04u
#define VMSVGA3D_DXVK_D3D11_BIND_STREAM_OUTPUT 0x10u
#define VMSVGA3D_DXVK_D3D11_CPU_ACCESS_WRITE 0x00010000u
#define VMSVGA3D_DXVK_D3D11_CPU_ACCESS_READ 0x00020000u
#define VMSVGA3D_DXVK_D3D11_MAP_READ 1u
#define VMSVGA3D_DXVK_D3D11_MAP_WRITE_DISCARD 4u
#define VMSVGA3D_DXVK_D3D11_FILTER_ANISOTROPIC 0x55u
#define VMSVGA3D_DXVK_D3D11_TEXTURE_ADDRESS_WRAP 1u
#define VMSVGA3D_DXVK_D3D11_COMPARISON_ALWAYS 8u
#define VMSVGA3D_DXVK_D3D11_FILL_SOLID 3u
#define VMSVGA3D_DXVK_D3D11_CULL_NONE 1u
#define VMSVGA3D_DXVK_D3D11_BLEND_ZERO 1u
#define VMSVGA3D_DXVK_D3D11_BLEND_SRC_COLOR 3u
#define VMSVGA3D_DXVK_D3D11_BLEND_SRC_ALPHA 5u
#define VMSVGA3D_DXVK_D3D11_BLEND_OP_ADD 1u
#define VMSVGA3D_DXVK_D3D11_COLOR_WRITE_ENABLE_ALL 0x0fu
#define VMSVGA3D_DXVK_D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP 5u
#define VMSVGA3D_DXVK_D3D11_MAX_RENDER_TARGETS 8u
#define VMSVGA3D_DXVK_D3D11_MAX_VIEWPORTS 16u
#define VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_BUFFER 1u
#define VMSVGA3D_DXVK_DXGI_R32_UINT 42u
#define VMSVGA3D_DXVK_DXGI_R16_UINT 57u
#define VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_TEXTURE1D 2u
#define VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_TEXTURE2D 3u
#define VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_TEXTURE3D 4u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_BUFFER 1u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE1D 2u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE1DARRAY 3u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE2D 4u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE2DARRAY 5u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE2DMS 6u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY 7u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE3D 8u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURECUBE 9u
#define VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURECUBEARRAY 10u
#define VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_BUFFER 1u
#define VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE1D 2u
#define VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE1DARRAY 3u
#define VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE2D 4u
#define VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE2DARRAY 5u
#define VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE2DMS 6u
#define VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY 7u
#define VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE3D 8u
#define VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE1D 1u
#define VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE1DARRAY 2u
#define VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE2D 3u
#define VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE2DARRAY 4u
#define VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE2DMS 5u
#define VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY 6u

static GMutex vmsvga3d_dxvk_init_lock;

/*
 * DXVK Native exposes the D3D9 COM ABI with the host C calling convention.
 * Its current C convenience declarations omit the implicit interface pointer,
 * so use the stable vtable layout directly rather than including d3d9.h.
 */
typedef void (*VMSVGA3DDxvkComFunction)(void);
typedef void *(*VMSVGA3DDxvkDirect3DCreate9)(uint32_t sdk_version);
typedef uint32_t (*VMSVGA3DDxvkAddRef)(void *object);
typedef uint32_t (*VMSVGA3DDxvkRelease)(void *object);
typedef int32_t (*VMSVGA3DDxvkCreateDevice)(
    void *d3d9, uint32_t adapter, uint32_t device_type, void *focus_window,
    uint32_t behavior_flags, void *present_parameters, void **device);
typedef int32_t (*VMSVGA3DDxvkCreateTexture)(
    void *device, uint32_t width, uint32_t height, uint32_t levels,
    uint32_t usage, uint32_t format, uint32_t pool, void **texture,
    void **shared_handle);
typedef int32_t (*VMSVGA3DDxvkCreateVertexBuffer)(
    void *device, uint32_t length, uint32_t usage, uint32_t fvf,
    uint32_t pool, void **buffer, void **shared_handle);
typedef int32_t (*VMSVGA3DDxvkCreateIndexBuffer)(
    void *device, uint32_t length, uint32_t usage, uint32_t format,
    uint32_t pool, void **buffer, void **shared_handle);
typedef int32_t (*VMSVGA3DDxvkCreateRenderTarget)(
    void *device, uint32_t width, uint32_t height, uint32_t format,
    uint32_t multisample_type, uint32_t multisample_quality, int lockable,
    void **surface, void **shared_handle);
typedef int32_t (*VMSVGA3DDxvkCreateDepthStencilSurface)(
    void *device, uint32_t width, uint32_t height, uint32_t format,
    uint32_t multisample_type, uint32_t multisample_quality, int discard,
    void **surface, void **shared_handle);
typedef int32_t (*VMSVGA3DDxvkCreateOffscreenPlainSurface)(
    void *device, uint32_t width, uint32_t height, uint32_t format,
    uint32_t pool, void **surface, void **shared_handle);
typedef int32_t (*VMSVGA3DDxvkUpdateSurface)(
    void *device, void *source, const void *source_rect, void *destination,
    const void *destination_point);
typedef int32_t (*VMSVGA3DDxvkGetRenderTargetData)(void *device,
                                                    void *source,
                                                    void *destination);
typedef int32_t (*VMSVGA3DDxvkStretchRect)(
    void *device, void *source, const void *source_rect, void *destination,
    const void *destination_rect, uint32_t filter);
typedef int32_t (*VMSVGA3DDxvkSetRenderTarget)(void *device, uint32_t index,
                                               void *surface);
typedef int32_t (*VMSVGA3DDxvkGetRenderTarget)(void *device, uint32_t index,
                                               void **surface);
typedef int32_t (*VMSVGA3DDxvkSetDepthStencilSurface)(void *device,
                                                      void *surface);
typedef int32_t (*VMSVGA3DDxvkGetDepthStencilSurface)(void *device,
                                                      void **surface);
typedef int32_t (*VMSVGA3DDxvkClear)(void *device, uint32_t rect_count,
                                     const void *rects, uint32_t flags,
                                     uint32_t color, float depth,
                                     uint32_t stencil);
typedef int32_t (*VMSVGA3DDxvkSetScissorRect)(void *device,
                                              const void *rect);
typedef int32_t (*VMSVGA3DDxvkGetScissorRect)(void *device, void *rect);
typedef int32_t (*VMSVGA3DDxvkBeginScene)(void *device);
typedef int32_t (*VMSVGA3DDxvkEndScene)(void *device);
typedef int32_t (*VMSVGA3DDxvkSetTransform)(void *device, uint32_t type,
                                            const void *matrix);
typedef int32_t (*VMSVGA3DDxvkSetViewport)(void *device,
                                           const void *viewport);
typedef int32_t (*VMSVGA3DDxvkSetMaterial)(void *device,
                                           const void *material);
typedef int32_t (*VMSVGA3DDxvkSetLight)(void *device, uint32_t index,
                                        const void *light);
typedef int32_t (*VMSVGA3DDxvkLightEnable)(void *device, uint32_t index,
                                           int enabled);
typedef int32_t (*VMSVGA3DDxvkSetClipPlane)(void *device, uint32_t index,
                                            const float *plane);
typedef int32_t (*VMSVGA3DDxvkSetRenderState)(void *device, uint32_t state,
                                              uint32_t value);
typedef int32_t (*VMSVGA3DDxvkCreateStateBlock)(void *device, uint32_t type,
                                                void **state_block);
typedef int32_t (*VMSVGA3DDxvkStateBlockApply)(void *state_block);
typedef int32_t (*VMSVGA3DDxvkSetTexture)(void *device, uint32_t stage,
                                          void *texture);
typedef int32_t (*VMSVGA3DDxvkSetTextureStageState)(
    void *device, uint32_t stage, uint32_t state, uint32_t value);
typedef int32_t (*VMSVGA3DDxvkSetSamplerState)(
    void *device, uint32_t sampler, uint32_t state, uint32_t value);
typedef int32_t (*VMSVGA3DDxvkDrawPrimitive)(
    void *device, uint32_t primitive_type, uint32_t start_vertex,
    uint32_t primitive_count);
typedef int32_t (*VMSVGA3DDxvkDrawIndexedPrimitive)(
    void *device, uint32_t primitive_type, int32_t base_vertex_index,
    uint32_t min_vertex_index, uint32_t num_vertices, uint32_t start_index,
    uint32_t primitive_count);
typedef int32_t (*VMSVGA3DDxvkCreateVertexDeclaration)(
    void *device, const void *elements, void **declaration);
typedef int32_t (*VMSVGA3DDxvkSetVertexDeclaration)(void *device,
                                                    void *declaration);
typedef int32_t (*VMSVGA3DDxvkCreateShader)(void *device,
                                            const uint32_t *bytecode,
                                            void **shader);
typedef int32_t (*VMSVGA3DDxvkSetShader)(void *device, void *shader);
typedef int32_t (*VMSVGA3DDxvkSetShaderConstantF)(
    void *device, uint32_t reg, const float *values, uint32_t count);
typedef int32_t (*VMSVGA3DDxvkSetShaderConstantI)(
    void *device, uint32_t reg, const int32_t *values, uint32_t count);
typedef int32_t (*VMSVGA3DDxvkSetShaderConstantB)(
    void *device, uint32_t reg, const int32_t *values, uint32_t count);
typedef int32_t (*VMSVGA3DDxvkSetStreamSource)(
    void *device, uint32_t stream, void *buffer, uint32_t offset,
    uint32_t stride);
typedef int32_t (*VMSVGA3DDxvkSetStreamSourceFreq)(void *device,
                                                   uint32_t stream,
                                                   uint32_t frequency);
typedef int32_t (*VMSVGA3DDxvkSetIndices)(void *device, void *buffer);
typedef int32_t (*VMSVGA3DDxvkCreateQuery)(void *device, uint32_t type,
                                           void **query);
typedef int32_t (*VMSVGA3DDxvkQueryIssue)(void *query, uint32_t flags);
typedef int32_t (*VMSVGA3DDxvkQueryGetData)(void *query, void *data,
                                             uint32_t size, uint32_t flags);
typedef int32_t (*VMSVGA3DDxvkBufferLock)(void *buffer, uint32_t offset,
                                          uint32_t size, void **data,
                                          uint32_t flags);
typedef int32_t (*VMSVGA3DDxvkBufferUnlock)(void *buffer);
typedef int32_t (*VMSVGA3DDxvkBaseTextureSetAutoGenFilterType)(
    void *texture, uint32_t filter);
typedef void (*VMSVGA3DDxvkBaseTextureGenerateMipSubLevels)(
    void *texture);
typedef int32_t (*VMSVGA3DDxvkTextureGetSurfaceLevel)(void *texture,
                                                       uint32_t level,
                                                       void **surface);
typedef int32_t (*VMSVGA3DDxvkTextureLockRect)(void *texture, uint32_t level,
                                               void *locked_rect,
                                               const void *rect,
                                               uint32_t flags);
typedef int32_t (*VMSVGA3DDxvkTextureUnlockRect)(void *texture,
                                                 uint32_t level);
typedef int32_t (*VMSVGA3DDxvkSurfaceLockRect)(void *surface,
                                               void *locked_rect,
                                               const void *rect,
                                               uint32_t flags);
typedef int32_t (*VMSVGA3DDxvkSurfaceUnlockRect)(void *surface);

typedef struct vmsvga3d_dxvk_d3d11_buffer_desc_s {
  uint32_t byte_width;
  uint32_t usage;
  uint32_t bind_flags;
  uint32_t cpu_access_flags;
  uint32_t misc_flags;
  uint32_t structure_byte_stride;
} VMSVGA3DDxvkD3D11BufferDesc;

typedef struct vmsvga3d_dxvk_d3d11_texture1d_desc_s {
  uint32_t width;
  uint32_t mip_levels;
  uint32_t array_size;
  uint32_t format;
  uint32_t usage;
  uint32_t bind_flags;
  uint32_t cpu_access_flags;
  uint32_t misc_flags;
} VMSVGA3DDxvkD3D11Texture1DDesc;

typedef struct vmsvga3d_dxvk_d3d11_texture2d_desc_s {
  uint32_t width;
  uint32_t height;
  uint32_t mip_levels;
  uint32_t array_size;
  uint32_t format;
  struct {
    uint32_t count;
    uint32_t quality;
  } sample_desc;
  uint32_t usage;
  uint32_t bind_flags;
  uint32_t cpu_access_flags;
  uint32_t misc_flags;
} VMSVGA3DDxvkD3D11Texture2DDesc;

typedef struct vmsvga3d_dxvk_d3d11_texture3d_desc_s {
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mip_levels;
  uint32_t format;
  uint32_t usage;
  uint32_t bind_flags;
  uint32_t cpu_access_flags;
  uint32_t misc_flags;
} VMSVGA3DDxvkD3D11Texture3DDesc;

typedef struct vmsvga3d_dxvk_d3d11_srv_desc_s {
  uint32_t format;
  uint32_t view_dimension;
  uint32_t data[4];
} VMSVGA3DDxvkD3D11SRVDesc;

typedef struct vmsvga3d_dxvk_d3d11_rtv_desc_s {
  uint32_t format;
  uint32_t view_dimension;
  uint32_t data[3];
} VMSVGA3DDxvkD3D11RTVDesc;

typedef struct vmsvga3d_dxvk_d3d11_dsv_desc_s {
  uint32_t format;
  uint32_t view_dimension;
  uint32_t flags;
  uint32_t data[3];
} VMSVGA3DDxvkD3D11DSVDesc;

typedef struct vmsvga3d_dxvk_d3d11_query_desc_s {
  uint32_t query;
  uint32_t misc_flags;
} VMSVGA3DDxvkD3D11QueryDesc;

typedef struct vmsvga3d_dxvk_d3d11_input_element_desc_s {
  const char *semantic_name;
  uint32_t semantic_index;
  uint32_t format;
  uint32_t input_slot;
  uint32_t aligned_byte_offset;
  uint32_t input_slot_class;
  uint32_t instance_data_step_rate;
} VMSVGA3DDxvkD3D11InputElementDesc;

typedef int32_t (*VMSVGA3DDxvkD3D11CreateDevice)(
    void *adapter, uint32_t driver_type, void *software, uint32_t flags,
    const uint32_t *feature_levels, uint32_t feature_level_count,
    uint32_t sdk_version, void **device, uint32_t *feature_level,
    void **immediate_context);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateBuffer)(
    void *device, const VMSVGA3DDxvkD3D11BufferDesc *desc,
    const VMSVGA3DDxvkSubresourceData *initial_data, void **buffer);
typedef void (*VMSVGA3DDxvkD3D11BufferGetDesc)(
    void *buffer, VMSVGA3DDxvkD3D11BufferDesc *desc);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateTexture1D)(
    void *device, const VMSVGA3DDxvkD3D11Texture1DDesc *desc,
    const VMSVGA3DDxvkSubresourceData *initial_data, void **texture);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateTexture2D)(
    void *device, const VMSVGA3DDxvkD3D11Texture2DDesc *desc,
    const VMSVGA3DDxvkSubresourceData *initial_data, void **texture);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateTexture3D)(
    void *device, const VMSVGA3DDxvkD3D11Texture3DDesc *desc,
    const VMSVGA3DDxvkSubresourceData *initial_data, void **texture);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateShaderResourceView)(
    void *device, void *resource, const VMSVGA3DDxvkD3D11SRVDesc *desc,
    void **view);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateRenderTargetView)(
    void *device, void *resource, const VMSVGA3DDxvkD3D11RTVDesc *desc,
    void **view);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateDepthStencilView)(
    void *device, void *resource, const VMSVGA3DDxvkD3D11DSVDesc *desc,
    void **view);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateInputLayout)(
    void *device, const VMSVGA3DDxvkD3D11InputElementDesc *elements,
    uint32_t element_count, const void *shader_bytecode,
    size_t shader_bytecode_size, void **input_layout);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateShader)(
    void *device, const void *bytecode, size_t bytecode_size,
    void *class_linkage, void **shader);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateGeometryShaderWithSO)(
    void *device, const void *bytecode, size_t bytecode_size,
    const VMSVGA3DD3D10StreamOutputDecl *declarations,
    uint32_t declaration_count, const uint32_t *strides,
    uint32_t stride_count, uint32_t rasterized_stream,
    void *class_linkage, void **shader);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateBlendState)(
    void *device, const VMSVGA3DD3D10BlendDesc *desc, void **state);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateDepthStencilState)(
    void *device, const VMSVGA3DD3D10DepthStencilDesc *desc, void **state);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateRasterizerState)(
    void *device, const VMSVGA3DD3D10RasterizerDesc *desc, void **state);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateSamplerState)(
    void *device, const VMSVGA3DD3D10SamplerDesc *desc, void **state);
typedef int32_t (*VMSVGA3DDxvkD3D11CreateQuery)(
    void *device, const VMSVGA3DDxvkD3D11QueryDesc *desc, void **query);
typedef int32_t (*VMSVGA3DDxvkD3D11CreatePredicate)(
    void *device, const VMSVGA3DDxvkD3D11QueryDesc *desc, void **predicate);
typedef void (*VMSVGA3DDxvkD3D11ClearRenderTargetView)(
    void *context, void *view, const float color[4]);
typedef void (*VMSVGA3DDxvkD3D11ClearDepthStencilView)(
    void *context, void *view, uint32_t clear_flags, float depth,
    uint8_t stencil);
typedef void (*VMSVGA3DDxvkD3D11GenerateMips)(void *context, void *view);
typedef void (*VMSVGA3DDxvkD3D11OMSetRenderTargetsAndUAVs)(
    void *context, uint32_t render_target_count,
    void *const *render_target_views, void *depth_stencil_view,
    uint32_t uav_start_slot, uint32_t uav_count,
    void *const *unordered_access_views, const uint32_t *initial_counts);
typedef void (*VMSVGA3DDxvkD3D11Begin)(void *context, void *query);
typedef void (*VMSVGA3DDxvkD3D11End)(void *context, void *query);
typedef void (*VMSVGA3DDxvkD3D11SetPredication)(
    void *context, void *predicate, int32_t predicate_value);
typedef void (*VMSVGA3DDxvkD3D11SetShaderResources)(
    void *context, uint32_t start_slot, uint32_t view_count,
    void *const *shader_resource_views);
typedef void (*VMSVGA3DDxvkD3D11SetSamplers)(
    void *context, uint32_t start_slot, uint32_t sampler_count,
    void *const *samplers);
typedef void (*VMSVGA3DDxvkD3D11SetConstantBuffers)(
    void *context, uint32_t start_slot, uint32_t buffer_count,
    void *const *buffers);
typedef void (*VMSVGA3DDxvkD3D11SetShader)(
    void *context, void *shader, void *const *class_instances,
    uint32_t class_instance_count);
typedef void (*VMSVGA3DDxvkD3D11GetShader)(
    void *context, void **shader, void **class_instances,
    uint32_t *class_instance_count);
typedef void (*VMSVGA3DDxvkD3D11GetShaderResources)(
    void *context, uint32_t start_slot, uint32_t view_count, void **views);
typedef void (*VMSVGA3DDxvkD3D11GetSamplers)(
    void *context, uint32_t start_slot, uint32_t sampler_count, void **samplers);
typedef void (*VMSVGA3DDxvkD3D11GetConstantBuffers)(
    void *context, uint32_t start_slot, uint32_t buffer_count, void **buffers);
typedef void (*VMSVGA3DDxvkD3D11IAGetInputLayout)(void *context, void **layout);
typedef void (*VMSVGA3DDxvkD3D11IAGetPrimitiveTopology)(
    void *context, uint32_t *topology);
typedef void (*VMSVGA3DDxvkD3D11OMSetRenderTargets)(
    void *context, uint32_t count, void *const *views, void *depth_stencil);
typedef void (*VMSVGA3DDxvkD3D11OMGetRenderTargets)(
    void *context, uint32_t count, void **views, void **depth_stencil);
typedef void (*VMSVGA3DDxvkD3D11OMGetBlendState)(
    void *context, void **state, float blend_factor[4], uint32_t *sample_mask);
typedef void (*VMSVGA3DDxvkD3D11RSGetState)(void *context, void **state);
typedef void (*VMSVGA3DDxvkD3D11RSGetViewports)(
    void *context, uint32_t *count, SVGA3dViewport *viewports);
typedef void (*VMSVGA3DDxvkD3D11IASetInputLayout)(
    void *context, void *input_layout);
typedef void (*VMSVGA3DDxvkD3D11IASetVertexBuffers)(
    void *context, uint32_t start_slot, uint32_t buffer_count,
    void *const *buffers, const uint32_t *strides, const uint32_t *offsets);
typedef void (*VMSVGA3DDxvkD3D11IASetIndexBuffer)(
    void *context, void *buffer, uint32_t format, uint32_t offset);
typedef void (*VMSVGA3DDxvkD3D11IAGetIndexBuffer)(
    void *context, void **buffer, uint32_t *format, uint32_t *offset);
typedef void (*VMSVGA3DDxvkD3D11SOSetTargets)(
    void *context, uint32_t buffer_count, void *const *buffers,
    const uint32_t *offsets);
typedef void (*VMSVGA3DDxvkD3D11DrawIndexed)(
    void *context, uint32_t index_count, uint32_t start_index_location,
    int32_t base_vertex_location);
typedef void (*VMSVGA3DDxvkD3D11Draw)(
    void *context, uint32_t vertex_count, uint32_t start_vertex_location);
typedef void (*VMSVGA3DDxvkD3D11DrawIndexedInstanced)(
    void *context, uint32_t index_count_per_instance, uint32_t instance_count,
    uint32_t start_index_location, int32_t base_vertex_location,
    uint32_t start_instance_location);
typedef void (*VMSVGA3DDxvkD3D11DrawInstanced)(
    void *context, uint32_t vertex_count_per_instance, uint32_t instance_count,
    uint32_t start_vertex_location, uint32_t start_instance_location);
typedef void (*VMSVGA3DDxvkD3D11DrawAuto)(void *context);
typedef struct vmsvga3d_dxvk_d3d11_mapped_subresource_s {
  void *data;
  uint32_t row_pitch;
  uint32_t depth_pitch;
} VMSVGA3DDxvkD3D11MappedSubresource;
typedef int32_t (*VMSVGA3DDxvkD3D11Map)(
    void *context, void *resource, uint32_t subresource, uint32_t map_type,
    uint32_t map_flags, VMSVGA3DDxvkD3D11MappedSubresource *mapped);
typedef void (*VMSVGA3DDxvkD3D11Unmap)(
    void *context, void *resource, uint32_t subresource);
typedef void (*VMSVGA3DDxvkD3D11IASetPrimitiveTopology)(
    void *context, uint32_t topology);
typedef void (*VMSVGA3DDxvkD3D11OMSetBlendState)(
    void *context, void *blend_state, const float blend_factor[4],
    uint32_t sample_mask);
typedef void (*VMSVGA3DDxvkD3D11OMSetDepthStencilState)(
    void *context, void *depth_stencil_state, uint32_t stencil_ref);
typedef void (*VMSVGA3DDxvkD3D11RSSetState)(void *context, void *state);
typedef void (*VMSVGA3DDxvkD3D11RSSetViewports)(
    void *context, uint32_t viewport_count, const SVGA3dViewport *viewports);
typedef void (*VMSVGA3DDxvkD3D11RSSetScissorRects)(
    void *context, uint32_t rect_count, const SVGASignedRect *rects);
typedef struct vmsvga3d_dxvk_d3d11_box_s {
  uint32_t left;
  uint32_t top;
  uint32_t front;
  uint32_t right;
  uint32_t bottom;
  uint32_t back;
} VMSVGA3DDxvkD3D11Box;

typedef void (*VMSVGA3DDxvkD3D11CopySubresourceRegion)(
    void *context, void *destination, uint32_t destination_subresource,
    uint32_t destination_x, uint32_t destination_y, uint32_t destination_z,
    void *source, uint32_t source_subresource,
    const VMSVGA3DDxvkD3D11Box *source_box);
typedef void (*VMSVGA3DDxvkD3D11CopyResource)(
    void *context, void *destination, void *source);
typedef void (*VMSVGA3DDxvkD3D11ResolveSubresource)(
    void *context, void *destination, uint32_t destination_subresource,
    void *source, uint32_t source_subresource, uint32_t format);
typedef void (*VMSVGA3DDxvkD3D11UpdateSubresource)(
    void *context, void *destination, uint32_t destination_subresource,
    const VMSVGA3DDxvkD3D11Box *destination_box, const void *source_data,
    uint32_t source_row_pitch, uint32_t source_depth_pitch);
typedef int32_t (*VMSVGA3DDxvkD3D11GetData)(
    void *context, void *query, void *data, uint32_t data_size, uint32_t flags);

_Static_assert(offsetof(VMSVGA3DDxvkSubresourceData, row_pitch) ==
                   sizeof(void *),
               "D3D11_SUBRESOURCE_DATA row pitch ABI mismatch");
_Static_assert(offsetof(VMSVGA3DDxvkSubresourceData, slice_pitch) ==
                   sizeof(void *) + sizeof(uint32_t),
               "D3D11_SUBRESOURCE_DATA slice pitch ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkSubresourceData) ==
                   sizeof(void *) + 2 * sizeof(uint32_t),
               "D3D11_SUBRESOURCE_DATA ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11BufferDesc) == 24,
               "D3D11_BUFFER_DESC ABI mismatch");
_Static_assert(offsetof(VMSVGA3DDxvkD3D11MappedSubresource, row_pitch) ==
                   sizeof(void *),
               "D3D11_MAPPED_SUBRESOURCE row pitch ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11MappedSubresource) ==
                   sizeof(void *) + 2 * sizeof(uint32_t),
               "D3D11_MAPPED_SUBRESOURCE ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11Texture1DDesc) == 32,
               "D3D11_TEXTURE1D_DESC ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11Texture2DDesc) == 44,
               "D3D11_TEXTURE2D_DESC ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11Texture3DDesc) == 36,
               "D3D11_TEXTURE3D_DESC ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11SRVDesc) == 24,
               "D3D11_SHADER_RESOURCE_VIEW_DESC ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11RTVDesc) == 20,
               "D3D11_RENDER_TARGET_VIEW_DESC ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11DSVDesc) == 24,
               "D3D11_DEPTH_STENCIL_VIEW_DESC ABI mismatch");
_Static_assert(offsetof(VMSVGA3DDxvkD3D11InputElementDesc, semantic_index) ==
                   sizeof(void *),
               "D3D11_INPUT_ELEMENT_DESC semantic index ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11InputElementDesc) ==
                   sizeof(void *) + 6 * sizeof(uint32_t),
               "D3D11_INPUT_ELEMENT_DESC ABI mismatch");
_Static_assert(sizeof(VMSVGA3DD3D10BlendDesc) == 264,
               "D3D11_BLEND_DESC ABI mismatch");
_Static_assert(sizeof(VMSVGA3DD3D10DepthStencilDesc) == 52,
               "D3D11_DEPTH_STENCIL_DESC ABI mismatch");
_Static_assert(sizeof(VMSVGA3DD3D10RasterizerDesc) == 40,
               "D3D11_RASTERIZER_DESC ABI mismatch");
_Static_assert(sizeof(VMSVGA3DD3D10SamplerDesc) == 52,
               "D3D11_SAMPLER_DESC ABI mismatch");
_Static_assert(sizeof(SVGA3dViewport) == 6 * sizeof(float) &&
                   offsetof(SVGA3dViewport, x) == 0 &&
                   offsetof(SVGA3dViewport, y) == 4 &&
                   offsetof(SVGA3dViewport, width) == 8 &&
                   offsetof(SVGA3dViewport, height) == 12 &&
                   offsetof(SVGA3dViewport, minDepth) == 16 &&
                   offsetof(SVGA3dViewport, maxDepth) == 20,
               "D3D11_VIEWPORT ABI mismatch");
_Static_assert(sizeof(SVGASignedRect) == 4 * sizeof(int32_t) &&
                   offsetof(SVGASignedRect, left) == 0 &&
                   offsetof(SVGASignedRect, top) == 4 &&
                   offsetof(SVGASignedRect, right) == 8 &&
                   offsetof(SVGASignedRect, bottom) == 12,
               "D3D11_RECT ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11Box) == 24,
               "D3D11_BOX ABI mismatch");
_Static_assert(sizeof(VMSVGA3DDxvkD3D11QueryDesc) == 8,
               "D3D11_QUERY_DESC ABI mismatch");
_Static_assert(SVGA3D_NUM_SHADERTYPE_DX10 == 3,
               "vGPU10 shader stage count mismatch");
_Static_assert(SVGA3D_DX_MAX_CONSTBUFFERS == 16,
               "vGPU10 constant buffer slot count mismatch");

typedef int32_t VMSVGA3DDxvkVkResult;
typedef void *VMSVGA3DDxvkVkInstance;
typedef void *VMSVGA3DDxvkVkPhysicalDevice;

typedef struct vmsvga3d_dxvk_vk_application_info_s {
  uint32_t s_type;
  const void *next;
  const char *application_name;
  uint32_t application_version;
  const char *engine_name;
  uint32_t engine_version;
  uint32_t api_version;
} VMSVGA3DDxvkVkApplicationInfo;

typedef struct vmsvga3d_dxvk_vk_instance_create_info_s {
  uint32_t s_type;
  const void *next;
  uint32_t flags;
  const VMSVGA3DDxvkVkApplicationInfo *application_info;
  uint32_t enabled_layer_count;
  const char *const *enabled_layer_names;
  uint32_t enabled_extension_count;
  const char *const *enabled_extension_names;
} VMSVGA3DDxvkVkInstanceCreateInfo;

typedef struct vmsvga3d_dxvk_vk_extension_properties_s {
  char extension_name[VMSVGA3D_DXVK_VK_MAX_EXTENSION_NAME_SIZE];
  uint32_t spec_version;
} VMSVGA3DDxvkVkExtensionProperties;

typedef union vmsvga3d_dxvk_vk_properties_buffer_u {
  max_align_t alignment;
  uint8_t bytes[VMSVGA3D_DXVK_VK_PROPERTIES_BUFFER_SIZE];
} VMSVGA3DDxvkVkPropertiesBuffer;

typedef VMSVGA3DDxvkVkResult (*VMSVGA3DDxvkVkEnumerateInstanceVersion)(
    uint32_t *api_version);
typedef VMSVGA3DDxvkVkResult
    (*VMSVGA3DDxvkVkEnumerateInstanceExtensionProperties)(
        const char *layer_name, uint32_t *property_count,
        VMSVGA3DDxvkVkExtensionProperties *properties);
typedef VMSVGA3DDxvkVkResult (*VMSVGA3DDxvkVkCreateInstance)(
    const VMSVGA3DDxvkVkInstanceCreateInfo *create_info,
    const void *allocator, VMSVGA3DDxvkVkInstance *instance);
typedef void (*VMSVGA3DDxvkVkDestroyInstance)(
    VMSVGA3DDxvkVkInstance instance, const void *allocator);
typedef VMSVGA3DDxvkVkResult (*VMSVGA3DDxvkVkEnumeratePhysicalDevices)(
    VMSVGA3DDxvkVkInstance instance, uint32_t *physical_device_count,
    VMSVGA3DDxvkVkPhysicalDevice *physical_devices);
typedef void (*VMSVGA3DDxvkVkGetPhysicalDeviceProperties)(
    VMSVGA3DDxvkVkPhysicalDevice physical_device, void *properties);
typedef VMSVGA3DDxvkVkResult
    (*VMSVGA3DDxvkVkEnumerateDeviceExtensionProperties)(
        VMSVGA3DDxvkVkPhysicalDevice physical_device, const char *layer_name,
        uint32_t *property_count,
        VMSVGA3DDxvkVkExtensionProperties *properties);

typedef struct vmsvga3d_dxvk_present_parameters_s {
  uint32_t back_buffer_width;
  uint32_t back_buffer_height;
  uint32_t back_buffer_format;
  uint32_t back_buffer_count;
  uint32_t multisample_type;
  uint32_t multisample_quality;
  uint32_t swap_effect;
  void *device_window;
  int32_t windowed;
  int32_t enable_auto_depth_stencil;
  uint32_t auto_depth_stencil_format;
  uint32_t flags;
  uint32_t fullscreen_refresh_rate;
  uint32_t presentation_interval;
} VMSVGA3DDxvkPresentParameters;

typedef struct vmsvga3d_dxvk_locked_rect_s {
  int32_t pitch;
  void *bits;
} VMSVGA3DDxvkLockedRect;

static VMSVGA3DDxvkComFunction *vmsvga3d_dxvk_vtable(void *object) {
  VMSVGA3DDxvkComFunction *vtable = NULL;

  if (object != NULL) {
    memcpy(&vtable, object, sizeof(vtable));
  }
  return vtable;
}

static VMSVGA3DDxvkComFunction vmsvga3d_dxvk_vtable_entry(
    void *object, uint32_t index) {
  VMSVGA3DDxvkComFunction *vtable = vmsvga3d_dxvk_vtable(object);

  return vtable != NULL ? vtable[index] : NULL;
}

static void vmsvga3d_dxvk_release(void *object, uint32_t index) {
  VMSVGA3DDxvkComFunction entry;
  VMSVGA3DDxvkRelease release = NULL;

  if (object == NULL) {
    return;
  }
  entry = vmsvga3d_dxvk_vtable_entry(object, index);
  memcpy(&release, &entry, sizeof(release));
  if (release != NULL) {
    release(object);
  }
}

static bool vmsvga3d_dxvk_addref(void *object, uint32_t index) {
  VMSVGA3DDxvkComFunction entry;
  VMSVGA3DDxvkAddRef addref = NULL;

  if (object == NULL) {
    return false;
  }
  entry = vmsvga3d_dxvk_vtable_entry(object, index);
  if (entry == NULL) {
    return false;
  }
  memcpy(&addref, &entry, sizeof(addref));
  addref(object);
  return true;
}

static bool vmsvga3d_dxvk_succeeded(int32_t result) {
  return result >= 0;
}

static bool vmsvga3d_dxvk_get_method(void *object, uint32_t index,
                                      void *method, size_t method_size) {
  VMSVGA3DDxvkComFunction entry;

  if (object == NULL || method == NULL || method_size != sizeof(entry)) {
    return false;
  }
  entry = vmsvga3d_dxvk_vtable_entry(object, index);
  memcpy(method, &entry, method_size);
  return entry != NULL;
}

#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_d3d9_set_autogen_filter(void *texture,
                                                    uint32_t filter) {
  VMSVGA3DDxvkBaseTextureSetAutoGenFilterType set_filter = NULL;

  if (!vmsvga3d_dxvk_get_method(
          texture,
          VMSVGA3D_DXVK_IDIRECT3DBASETEXTURE9_SET_AUTOGEN_FILTER_TYPE,
          &set_filter, sizeof(set_filter))) {
    return false;
  }

  /* VirtualBox only asserts the HRESULT and continues in release builds. */
  (void)set_filter(texture, filter);
  return true;
}

static bool vmsvga3d_dxvk_d3d9_generate_mip_sublevels(void *texture,
                                                        uint32_t filter) {
  VMSVGA3DDxvkBaseTextureGenerateMipSubLevels generate_mips = NULL;

  if (!vmsvga3d_dxvk_d3d9_set_autogen_filter(texture, filter) ||
      !vmsvga3d_dxvk_get_method(
          texture,
          VMSVGA3D_DXVK_IDIRECT3DBASETEXTURE9_GENERATE_MIP_SUB_LEVELS,
          &generate_mips, sizeof(generate_mips))) {
    return false;
  }
  generate_mips(texture);
  return true;
}

static bool vmsvga3d_dxvk_capture_pristine_state(VMSVGA3DDxvk *dxvk) {
  VMSVGA3DDxvkCreateStateBlock create_state_block = NULL;
  int32_t result;

  if (dxvk == NULL || dxvk->d3d9_device == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_STATE_BLOCK,
          &create_state_block, sizeof(create_state_block))) {
    return false;
  }
  result = create_state_block(dxvk->d3d9_device, VMSVGA3D_DXVK_D3DSBT_ALL,
                              &dxvk->d3d9_pristine_state);
  return vmsvga3d_dxvk_succeeded(result) && dxvk->d3d9_pristine_state != NULL;
}

static bool vmsvga3d_dxvk_create_d3d11(VMSVGA3DDxvk *dxvk, Error **errp) {
  VMSVGA3DDxvkD3D11CreateDevice create_device = NULL;
  const uint32_t feature_levels[] = { VMSVGA3D_DXVK_D3D_FEATURE_LEVEL_11_0 };
  void *entry;
  uint32_t feature_level = 0;
  int32_t result;

  if (dxvk == NULL) {
    return false;
  }

  dxvk->d3d11_library =
      dlopen(VMSVGA3D_DXVK_D3D11_SONAME, RTLD_NOW | RTLD_LOCAL);
  if (dxvk->d3d11_library == NULL) {
    error_setg(errp, "failed to load %s: %s", VMSVGA3D_DXVK_D3D11_SONAME,
               dlerror());
    return false;
  }

  entry = dlsym(dxvk->d3d11_library, "D3D11CreateDevice");
  memcpy(&create_device, &entry, sizeof(create_device));
  if (create_device == NULL) {
    error_setg(errp, "%s has no D3D11CreateDevice entry point",
               VMSVGA3D_DXVK_D3D11_SONAME);
    return false;
  }

  result = create_device(
      NULL, VMSVGA3D_DXVK_D3D_DRIVER_TYPE_HARDWARE, NULL, 0, feature_levels,
      G_N_ELEMENTS(feature_levels), VMSVGA3D_DXVK_D3D11_SDK_VERSION,
      &dxvk->d3d11_device, &feature_level, &dxvk->d3d11_context);
  if (!vmsvga3d_dxvk_succeeded(result) || dxvk->d3d11_device == NULL ||
      dxvk->d3d11_context == NULL ||
      feature_level != VMSVGA3D_DXVK_D3D_FEATURE_LEVEL_11_0) {
    error_setg(errp, "DXVK D3D11 device creation failed (HRESULT 0x%08x)",
               (uint32_t)result);
    return false;
  }
  return true;
}
#endif

static bool vmsvga3d_dxvk_load_symbol(void *library, const char *name,
                                       void *function, size_t function_size,
                                       Error **errp) {
  void *entry;

  entry = dlsym(library, name);
  if (entry == NULL) {
    error_setg(errp, "%s has no %s entry point", VMSVGA3D_DXVK_VULKAN_SONAME,
               name);
    return false;
  }
  memcpy(function, &entry, function_size);
  return true;
}

static bool vmsvga3d_dxvk_has_extension(
    const VMSVGA3DDxvkVkExtensionProperties *properties, uint32_t count,
    const char *name) {
  uint32_t i;

  for (i = 0; i < count; i++) {
    if (strcmp(properties[i].extension_name, name) == 0) {
      return true;
    }
  }
  return false;
}

static bool vmsvga3d_dxvk_enumerate_extensions(
    VMSVGA3DDxvkVkEnumerateInstanceExtensionProperties enumerate,
    VMSVGA3DDxvkVkExtensionProperties **properties, uint32_t *count,
    Error **errp) {
  VMSVGA3DDxvkVkResult result;

  *properties = NULL;
  *count = 0;
  result = enumerate(NULL, count, NULL);
  if (result != VMSVGA3D_DXVK_VK_SUCCESS) {
    error_setg(errp, "failed to enumerate Vulkan instance extensions (%d)",
               result);
    return false;
  }
  if (*count == 0) {
    return true;
  }
  *properties = g_try_new0(VMSVGA3DDxvkVkExtensionProperties, *count);
  if (*properties == NULL) {
    error_setg(errp, "failed to allocate Vulkan instance extension list");
    return false;
  }
  result = enumerate(NULL, count, *properties);
  if (result != VMSVGA3D_DXVK_VK_SUCCESS) {
    error_setg(errp, "failed to read Vulkan instance extensions (%d)", result);
    g_free(*properties);
    *properties = NULL;
    *count = 0;
    return false;
  }
  return true;
}

static bool vmsvga3d_dxvk_device_has_required_extensions(
    VMSVGA3DDxvkVkEnumerateDeviceExtensionProperties enumerate,
    VMSVGA3DDxvkVkPhysicalDevice physical_device, Error **errp) {
  VMSVGA3DDxvkVkExtensionProperties *properties = NULL;
  uint32_t count = 0;
  VMSVGA3DDxvkVkResult result;
  bool compatible = false;

  result = enumerate(physical_device, NULL, &count, NULL);
  if (result != VMSVGA3D_DXVK_VK_SUCCESS) {
    return false;
  }
  if (count != 0) {
    properties = g_try_new0(VMSVGA3DDxvkVkExtensionProperties, count);
    if (properties == NULL) {
      error_setg(errp, "failed to allocate Vulkan device extension list");
      return false;
    }
    result = enumerate(physical_device, NULL, &count, properties);
    if (result == VMSVGA3D_DXVK_VK_SUCCESS) {
      compatible = vmsvga3d_dxvk_has_extension(
                       properties, count,
                       VMSVGA3D_DXVK_VK_KHR_MAINTENANCE5) &&
                   vmsvga3d_dxvk_has_extension(
                       properties, count, VMSVGA3D_DXVK_VK_EXT_ROBUSTNESS2);
    }
  }
  g_free(properties);
  return compatible;
}

static bool vmsvga3d_dxvk_vulkan_probe(Error **errp) {
  static const char *const required_instance_extensions[] = {
    VMSVGA3D_DXVK_VK_KHR_SURFACE,
    VMSVGA3D_DXVK_VK_EXT_HEADLESS_SURFACE,
  };
  VMSVGA3DDxvkVkEnumerateInstanceVersion enumerate_instance_version = NULL;
  VMSVGA3DDxvkVkEnumerateInstanceExtensionProperties
      enumerate_instance_extensions = NULL;
  VMSVGA3DDxvkVkCreateInstance create_instance = NULL;
  VMSVGA3DDxvkVkDestroyInstance destroy_instance = NULL;
  VMSVGA3DDxvkVkEnumeratePhysicalDevices enumerate_physical_devices = NULL;
  VMSVGA3DDxvkVkGetPhysicalDeviceProperties get_physical_device_properties =
      NULL;
  VMSVGA3DDxvkVkEnumerateDeviceExtensionProperties
      enumerate_device_extensions = NULL;
  VMSVGA3DDxvkVkExtensionProperties *instance_extensions = NULL;
  VMSVGA3DDxvkVkPhysicalDevice *physical_devices = NULL;
  VMSVGA3DDxvkVkInstance instance = NULL;
  VMSVGA3DDxvkVkApplicationInfo application_info = {
    .s_type = VMSVGA3D_DXVK_VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .application_name = "qemu-vmvga",
    .engine_name = "qemu-vmvga",
    .api_version = VMSVGA3D_DXVK_VULKAN_API_1_3,
  };
  VMSVGA3DDxvkVkInstanceCreateInfo create_info = {
    .s_type = VMSVGA3D_DXVK_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .application_info = &application_info,
    .enabled_extension_count = G_N_ELEMENTS(required_instance_extensions),
    .enabled_extension_names = required_instance_extensions,
  };
  void *vulkan;
  uint32_t api_version = 0;
  uint32_t instance_extension_count = 0;
  uint32_t physical_device_count = 0;
  uint32_t i;
  bool compatible_device = false;
  bool success = false;
  VMSVGA3DDxvkVkResult result;

  vulkan = dlopen(VMSVGA3D_DXVK_VULKAN_SONAME, RTLD_NOW | RTLD_LOCAL);
  if (vulkan == NULL) {
    error_setg(errp, "failed to load %s: %s", VMSVGA3D_DXVK_VULKAN_SONAME,
               dlerror());
    return false;
  }

#define VMSVGA3D_DXVK_LOAD_VULKAN(name, variable)                         \
  do {                                                                    \
    if (!vmsvga3d_dxvk_load_symbol(vulkan, name, &(variable),             \
                                    sizeof(variable), errp)) {             \
      goto out;                                                           \
    }                                                                     \
  } while (0)

  VMSVGA3D_DXVK_LOAD_VULKAN("vkEnumerateInstanceVersion",
                            enumerate_instance_version);
  VMSVGA3D_DXVK_LOAD_VULKAN("vkEnumerateInstanceExtensionProperties",
                            enumerate_instance_extensions);
  VMSVGA3D_DXVK_LOAD_VULKAN("vkCreateInstance", create_instance);
  VMSVGA3D_DXVK_LOAD_VULKAN("vkDestroyInstance", destroy_instance);
  VMSVGA3D_DXVK_LOAD_VULKAN("vkEnumeratePhysicalDevices",
                            enumerate_physical_devices);
  VMSVGA3D_DXVK_LOAD_VULKAN("vkGetPhysicalDeviceProperties",
                            get_physical_device_properties);
  VMSVGA3D_DXVK_LOAD_VULKAN("vkEnumerateDeviceExtensionProperties",
                            enumerate_device_extensions);

#undef VMSVGA3D_DXVK_LOAD_VULKAN

  result = enumerate_instance_version(&api_version);
  if (result != VMSVGA3D_DXVK_VK_SUCCESS ||
      api_version < VMSVGA3D_DXVK_VULKAN_API_1_3) {
    error_setg(errp, "DXVK requires a Vulkan 1.3 capable loader");
    goto out;
  }
  if (!vmsvga3d_dxvk_enumerate_extensions(
          enumerate_instance_extensions, &instance_extensions,
          &instance_extension_count, errp)) {
    goto out;
  }
  for (i = 0; i < G_N_ELEMENTS(required_instance_extensions); i++) {
    if (!vmsvga3d_dxvk_has_extension(instance_extensions,
                                     instance_extension_count,
                                     required_instance_extensions[i])) {
      error_setg(errp, "Vulkan instance extension %s is unavailable",
                 required_instance_extensions[i]);
      goto out;
    }
  }

  result = create_instance(&create_info, NULL, &instance);
  if (result != VMSVGA3D_DXVK_VK_SUCCESS || instance == NULL) {
    error_setg(errp, "failed to create Vulkan 1.3 instance (%d)", result);
    goto out;
  }
  result = enumerate_physical_devices(instance, &physical_device_count, NULL);
  if (result != VMSVGA3D_DXVK_VK_SUCCESS || physical_device_count == 0) {
    error_setg(errp, "no Vulkan physical device is available");
    goto out;
  }
  physical_devices = g_try_new0(VMSVGA3DDxvkVkPhysicalDevice,
                                physical_device_count);
  if (physical_devices == NULL) {
    error_setg(errp, "failed to allocate Vulkan physical device list");
    goto out;
  }
  result = enumerate_physical_devices(instance, &physical_device_count,
                                      physical_devices);
  if (result != VMSVGA3D_DXVK_VK_SUCCESS) {
    error_setg(errp, "failed to enumerate Vulkan physical devices (%d)",
               result);
    goto out;
  }

  for (i = 0; i < physical_device_count; i++) {
    VMSVGA3DDxvkVkPropertiesBuffer properties = { 0 };
    uint32_t device_api_version = 0;

    get_physical_device_properties(physical_devices[i], properties.bytes);
    memcpy(&device_api_version, properties.bytes, sizeof(device_api_version));
    if (device_api_version < VMSVGA3D_DXVK_VULKAN_API_1_3) {
      continue;
    }
    if (vmsvga3d_dxvk_device_has_required_extensions(
            enumerate_device_extensions, physical_devices[i], errp)) {
      compatible_device = true;
      break;
    }
    if (errp != NULL && *errp != NULL) {
      goto out;
    }
  }
  if (!compatible_device) {
    error_setg(errp,
               "no Vulkan device satisfies DXVK D3D9 requirements "
               "(Vulkan 1.3, VK_KHR_maintenance5, VK_EXT_robustness2)");
    goto out;
  }
  success = true;

out:
  g_free(physical_devices);
  g_free(instance_extensions);
  if (instance != NULL && destroy_instance != NULL) {
    destroy_instance(instance, NULL);
  }
  dlclose(vulkan);
  return success;
}

static bool vmsvga3d_dxvk_validate_environment(Error **errp) {
  const char *name_filter = g_getenv("DXVK_FILTER_DEVICE_NAME");
  const char *uuid_filter = g_getenv("DXVK_FILTER_DEVICE_UUID");
  const char *config = g_getenv("DXVK_CONFIG");

  if ((name_filter != NULL && name_filter[0] != '\0') ||
      (uuid_filter != NULL && uuid_filter[0] != '\0')) {
    error_setg(errp,
               "DXVK device filter environment variables are not supported "
               "by the VMVGA safe runtime probe");
    return false;
  }
  if (config != NULL && strstr(config, "dxvk.deviceFilter") != NULL) {
    error_setg(errp,
               "DXVK_CONFIG device filtering is not supported by the VMVGA "
               "safe runtime probe");
    return false;
  }
  return true;
}

/*
 * DXVK reads dxvk.conf from the current directory by default.  A device
 * filter there can leave it with no adapters and make Direct3DCreate9 throw
 * across the C ABI.  Keep the QEMU renderer isolated from ambient config
 * files while its persistent device is created.
 */
static bool vmsvga3d_dxvk_set_config_environment(char **saved,
                                                  Error **errp) {
  const char *config_file = g_getenv(VMSVGA3D_DXVK_CONFIG_FILE_ENV);

  *saved = NULL;
  if (config_file != NULL) {
    size_t length = strlen(config_file) + 1;

    *saved = g_try_malloc0(length);
    if (*saved == NULL) {
      error_setg(errp, "failed to save %s", VMSVGA3D_DXVK_CONFIG_FILE_ENV);
      return false;
    }
    memcpy(*saved, config_file, length);
  }
  if (!g_setenv(VMSVGA3D_DXVK_CONFIG_FILE_ENV,
                VMSVGA3D_DXVK_CONFIG_FILE_VALUE, true)) {
    error_setg(errp, "failed to set %s=%s", VMSVGA3D_DXVK_CONFIG_FILE_ENV,
               VMSVGA3D_DXVK_CONFIG_FILE_VALUE);
    g_free(*saved);
    *saved = NULL;
    return false;
  }
  return true;
}

static void vmsvga3d_dxvk_restore_config_environment(char *saved) {
  if (saved != NULL) {
    bool restored = g_setenv(VMSVGA3D_DXVK_CONFIG_FILE_ENV, saved, true);

    (void)restored;
  } else {
    g_unsetenv(VMSVGA3D_DXVK_CONFIG_FILE_ENV);
  }
  g_free(saved);
}

static bool vmsvga3d_dxvk_environment_enabled(const char *name) {
  static const char *const enabled_values[] = {
      "y", "yes", "true", "1", "on", "enable", "enabled",
  };
  const char *value = g_getenv(name);
  const char *end;
  size_t length;
  size_t i;

  if (value == NULL) {
    return false;
  }
  while (*value != '\0' && g_ascii_isspace(*value)) {
    value++;
  }
  end = value + strlen(value);
  while (end > value && g_ascii_isspace(end[-1])) {
    end--;
  }
  length = (size_t)(end - value);

  for (i = 0; i < G_N_ELEMENTS(enabled_values); i++) {
    size_t enabled_length = strlen(enabled_values[i]);

    if (length == enabled_length &&
        g_ascii_strncasecmp(value, enabled_values[i], length) == 0) {
      return true;
    }
  }
  return false;
}

/*
 * DXVK 2.7.1 captures DXVK_LOG_LEVEL when its Logger singleton is created.
 * Silence the native DXVK startup dump unless the DEBUG env variable is set.
 */
static bool vmsvga3d_dxvk_set_log_environment(char **saved, bool *restore,
                                               Error **errp) {
  const char *log_level;

  *saved = NULL;
  *restore = false;
  if (vmsvga3d_dxvk_environment_enabled(VMSVGA3D_DXVK_DEBUG_ENV)) {
    return true;
  }

  log_level = g_getenv(VMSVGA3D_DXVK_LOG_LEVEL_ENV);
  if (log_level != NULL) {
    size_t length = strlen(log_level) + 1;

    *saved = g_try_malloc0(length);
    if (*saved == NULL) {
      error_setg(errp, "failed to save %s", VMSVGA3D_DXVK_LOG_LEVEL_ENV);
      return false;
    }
    memcpy(*saved, log_level, length);
  }
  if (!g_setenv(VMSVGA3D_DXVK_LOG_LEVEL_ENV,
                VMSVGA3D_DXVK_LOG_LEVEL_QUIET, true)) {
    error_setg(errp, "failed to set %s=%s", VMSVGA3D_DXVK_LOG_LEVEL_ENV,
               VMSVGA3D_DXVK_LOG_LEVEL_QUIET);
    g_free(*saved);
    *saved = NULL;
    return false;
  }
  *restore = true;
  return true;
}

static void vmsvga3d_dxvk_restore_log_environment(char *saved, bool restore) {
  if (!restore) {
    g_free(saved);
    return;
  }
  if (saved != NULL) {
    bool restored = g_setenv(VMSVGA3D_DXVK_LOG_LEVEL_ENV, saved, true);

    (void)restored;
  } else {
    g_unsetenv(VMSVGA3D_DXVK_LOG_LEVEL_ENV);
  }
  g_free(saved);
}

static bool vmsvga3d_dxvk_set_wsi_environment(bool *restore,
                                               Error **errp) {
  const char *driver = g_getenv(VMSVGA3D_DXVK_WSI_ENV);

  *restore = false;
  if (driver != NULL && strcmp(driver, VMSVGA3D_DXVK_WSI_VALUE) != 0) {
    error_setg(errp, "DXVK headless WSI requires %s=%s, but it is already %s",
               VMSVGA3D_DXVK_WSI_ENV, VMSVGA3D_DXVK_WSI_VALUE, driver);
    return false;
  }
  if (driver == NULL) {
    if (!g_setenv(VMSVGA3D_DXVK_WSI_ENV, VMSVGA3D_DXVK_WSI_VALUE, true)) {
      error_setg(errp, "failed to set %s=%s", VMSVGA3D_DXVK_WSI_ENV,
                 VMSVGA3D_DXVK_WSI_VALUE);
      return false;
    }
    *restore = true;
  }
  return true;
}

static void vmsvga3d_dxvk_restore_wsi_environment(bool restore) {
  if (restore) {
    g_unsetenv(VMSVGA3D_DXVK_WSI_ENV);
  }
}

static bool vmsvga3d_dxvk_create_device(VMSVGA3DDxvk *dxvk,
                                         uint32_t width, uint32_t height,
                                         Error **errp) {
  VMSVGA3DDxvkComFunction create_entry;
  VMSVGA3DDxvkCreateDevice create_device = NULL;
  VMSVGA3DDxvkPresentParameters present = {
    .back_buffer_width = width,
    .back_buffer_height = height,
    .back_buffer_format = VMSVGA3D_DXVK_D3DFMT_X8R8G8B8,
    .back_buffer_count = 1,
    .multisample_type = VMSVGA3D_DXVK_D3DMULTISAMPLE_NONE,
    .multisample_quality = 0,
    .swap_effect = VMSVGA3D_DXVK_D3DSWAPEFFECT_DISCARD,
    .device_window = vmsvga3d_dxvk_wsi_window(dxvk->wsi),
    .windowed = 1,
    .enable_auto_depth_stencil = 0,
    .auto_depth_stencil_format = 0,
    .flags = 0,
    .fullscreen_refresh_rate = 0,
    .presentation_interval = VMSVGA3D_DXVK_D3DPRESENT_INTERVAL_IMMEDIATE,
  };
  uint32_t behavior_flags;
  int32_t result;

  QEMU_BUILD_BUG_ON(sizeof(void *) != 4 && sizeof(void *) != 8);
  QEMU_BUILD_BUG_ON(sizeof(VMSVGA3DDxvkPresentParameters) !=
                    (sizeof(void *) == 8 ? 64 : 56));
  QEMU_BUILD_BUG_ON(offsetof(VMSVGA3DDxvkPresentParameters, device_window) !=
                    (sizeof(void *) == 8 ? 32 : 28));
  QEMU_BUILD_BUG_ON(
      offsetof(VMSVGA3DDxvkPresentParameters, presentation_interval) !=
      (sizeof(void *) == 8 ? 60 : 52));

  create_entry = vmsvga3d_dxvk_vtable_entry(
      dxvk->d3d9, VMSVGA3D_DXVK_IDIRECT3D9_CREATE_DEVICE);
  memcpy(&create_device, &create_entry, sizeof(create_device));
  if (create_device == NULL) {
    error_setg(errp, "DXVK D3D9 interface has no CreateDevice entry point");
    return false;
  }

  behavior_flags = VMSVGA3D_DXVK_D3DCREATE_MULTITHREADED |
                   VMSVGA3D_DXVK_D3DCREATE_HARDWARE_VERTEXPROCESSING;
  result = create_device(dxvk->d3d9, VMSVGA3D_DXVK_D3DADAPTER_DEFAULT,
                         VMSVGA3D_DXVK_D3DDEVTYPE_HAL,
                         present.device_window, behavior_flags, &present,
                         &dxvk->d3d9_device);
  if (result < 0 || dxvk->d3d9_device == NULL) {
    if (dxvk->d3d9_device != NULL) {
      vmsvga3d_dxvk_release(dxvk->d3d9_device,
                            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
      dxvk->d3d9_device = NULL;
    }
    behavior_flags = VMSVGA3D_DXVK_D3DCREATE_MULTITHREADED |
                     VMSVGA3D_DXVK_D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    result = create_device(dxvk->d3d9, VMSVGA3D_DXVK_D3DADAPTER_DEFAULT,
                           VMSVGA3D_DXVK_D3DDEVTYPE_HAL,
                           present.device_window, behavior_flags, &present,
                           &dxvk->d3d9_device);
  }
  if (result < 0 || dxvk->d3d9_device == NULL) {
    if (dxvk->d3d9_device != NULL) {
      vmsvga3d_dxvk_release(dxvk->d3d9_device,
                            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
      dxvk->d3d9_device = NULL;
    }
    error_setg(errp, "DXVK D3D9 device creation failed (HRESULT 0x%08x)",
               (uint32_t)result);
    return false;
  }
  return true;
}

#endif

VMSVGA3DDxvk *vmsvga3d_dxvk_create(uint32_t width, uint32_t height,
                                    Error **errp) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkDirect3DCreate9 direct3d_create9 = NULL;
  VMSVGA3DDxvk *dxvk;
  void *create9_entry;
  char *saved_config_file = NULL;
  char *saved_log_level = NULL;
  bool config_environment_set = false;
  bool restore_log_environment = false;
  bool restore_wsi_environment = false;

  width = width != 0 ? width : VMSVGA3D_DXVK_DEVICE_DEFAULT_WIDTH;
  height = height != 0 ? height : VMSVGA3D_DXVK_DEVICE_DEFAULT_HEIGHT;

  dxvk = g_try_new0(VMSVGA3DDxvk, 1);
  if (dxvk == NULL) {
    error_setg(errp, "failed to allocate DXVK renderer state");
    return NULL;
  }
  if (!vmsvga3d_dxvk_validate_environment(errp)) {
    goto fail_unlocked;
  }
  if (!vmsvga3d_dxvk_vulkan_probe(errp)) {
    goto fail_unlocked;
  }

  /* DXVK initialization reads process-global environment variables. */
  g_mutex_lock(&vmsvga3d_dxvk_init_lock);
  if (!vmsvga3d_dxvk_set_log_environment(&saved_log_level,
                                          &restore_log_environment, errp)) {
    goto fail;
  }
  if (!vmsvga3d_dxvk_set_config_environment(&saved_config_file, errp)) {
    goto fail;
  }
  config_environment_set = true;
  if (!vmsvga3d_dxvk_set_wsi_environment(&restore_wsi_environment, errp)) {
    goto fail;
  }
  dxvk->wsi = vmsvga3d_dxvk_wsi_create(width, height, errp);
  if (dxvk->wsi == NULL) {
    goto fail;
  }

  dxvk->d3d9_library = dlopen(VMSVGA3D_DXVK_D3D9_SONAME, RTLD_NOW | RTLD_LOCAL);
  if (dxvk->d3d9_library == NULL) {
    error_setg(errp, "failed to load %s: %s", VMSVGA3D_DXVK_D3D9_SONAME,
               dlerror());
    goto fail;
  }
  create9_entry = dlsym(dxvk->d3d9_library, "Direct3DCreate9");
  memcpy(&direct3d_create9, &create9_entry, sizeof(direct3d_create9));
  if (direct3d_create9 == NULL) {
    error_setg(errp, "%s has no Direct3DCreate9 entry point",
               VMSVGA3D_DXVK_D3D9_SONAME);
    goto fail;
  }
  dxvk->d3d9 = direct3d_create9(VMSVGA3D_DXVK_D3D_SDK_VERSION);
  if (dxvk->d3d9 == NULL) {
    error_setg(errp, "DXVK Direct3DCreate9 failed");
    goto fail;
  }
  if (!vmsvga3d_dxvk_create_device(dxvk, width, height, errp)) {
    goto fail;
  }
  if (!vmsvga3d_dxvk_capture_pristine_state(dxvk)) {
    error_setg(errp, "DXVK D3D9 default state-block creation failed");
    goto fail;
  }
  if (!vmsvga3d_dxvk_create_d3d11(dxvk, errp)) {
    goto fail;
  }

  vmsvga3d_dxvk_restore_wsi_environment(restore_wsi_environment);
  if (config_environment_set) {
    vmsvga3d_dxvk_restore_config_environment(saved_config_file);
  }
  vmsvga3d_dxvk_restore_log_environment(saved_log_level,
                                         restore_log_environment);
  g_mutex_unlock(&vmsvga3d_dxvk_init_lock);
  dxvk->ready = true;
  return dxvk;

fail:
  vmsvga3d_dxvk_restore_wsi_environment(restore_wsi_environment);
  if (config_environment_set) {
    vmsvga3d_dxvk_restore_config_environment(saved_config_file);
  }
  vmsvga3d_dxvk_restore_log_environment(saved_log_level,
                                         restore_log_environment);
  g_mutex_unlock(&vmsvga3d_dxvk_init_lock);
fail_unlocked:
  vmsvga3d_dxvk_destroy(dxvk);
  return NULL;
#else
  (void)width;
  (void)height;
  error_setg(errp, "DXVK D3D9/D3D11 runtime is only available on Linux ELF hosts");
  return NULL;
#endif
}

void vmsvga3d_dxvk_destroy(VMSVGA3DDxvk *dxvk) {
  if (dxvk == NULL) {
    return;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  dxvk->ready = false;
  while (dxvk->d3d11_queries != NULL) {
    VMSVGA3DDxvkQuery *query = dxvk->d3d11_queries;

    dxvk->d3d11_queries = query->next;
    if (query->query != NULL) {
      vmsvga3d_dxvk_release(query->query, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    g_free(query);
  }
  while (dxvk->d3d11_states != NULL) {
    VMSVGA3DDxvkState *state = dxvk->d3d11_states;

    dxvk->d3d11_states = state->next;
    if (state->state != NULL) {
      vmsvga3d_dxvk_release(state->state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    g_free(state);
  }
  while (dxvk->d3d11_constant_buffers != NULL) {
    VMSVGA3DDxvkConstantBuffer *buffer = dxvk->d3d11_constant_buffers;

    dxvk->d3d11_constant_buffers = buffer->next;
    if (buffer->buffer != NULL) {
      vmsvga3d_dxvk_release(buffer->buffer,
                            VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    g_free(buffer);
  }
  while (dxvk->d3d11_views != NULL) {
    VMSVGA3DDxvkView *view = dxvk->d3d11_views;

    dxvk->d3d11_views = view->next;
    if (view->view != NULL) {
      vmsvga3d_dxvk_release(view->view, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    g_free(view);
  }
  while (dxvk->d3d11_stream_outputs != NULL) {
    VMSVGA3DDxvkStreamOutput *stream_output = dxvk->d3d11_stream_outputs;

    dxvk->d3d11_stream_outputs = stream_output->next;
    g_free(stream_output);
  }
  while (dxvk->d3d11_input_layouts != NULL) {
    VMSVGA3DDxvkInputLayout *layout = dxvk->d3d11_input_layouts;

    dxvk->d3d11_input_layouts = layout->next;
    if (layout->layout != NULL) {
      vmsvga3d_dxvk_release(layout->layout, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    g_free(layout);
  }
  while (dxvk->d3d11_shaders != NULL) {
    VMSVGA3DDxvkShader *shader = dxvk->d3d11_shaders;

    dxvk->d3d11_shaders = shader->next;
    if (shader->shader != NULL) {
      vmsvga3d_dxvk_release(shader->shader, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    if (shader->info_valid) {
      vmsvga3d_d3d10_shader_release(&shader->info);
    }
    g_free(shader->bytecode);
    g_free(shader);
  }
  {
    void **objects[] = {
      &dxvk->d3d11_blit_constant_buffer,
      &dxvk->d3d11_blit_vertex_shader,
      &dxvk->d3d11_blit_pixel_shader,
      &dxvk->d3d11_blit_pixel_shader_srgb,
      &dxvk->d3d11_blit_sampler_state,
      &dxvk->d3d11_blit_rasterizer_state,
      &dxvk->d3d11_blit_blend_state,
    };
    uint32_t i;

    for (i = 0; i < G_N_ELEMENTS(objects); i++) {
      if (*objects[i] != NULL) {
        vmsvga3d_dxvk_release(*objects[i], VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
        *objects[i] = NULL;
      }
    }
  }
  if (dxvk->d3d11_context != NULL) {
    vmsvga3d_dxvk_release(dxvk->d3d11_context,
                          VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    dxvk->d3d11_context = NULL;
  }
  if (dxvk->d3d11_device != NULL) {
    vmsvga3d_dxvk_release(dxvk->d3d11_device,
                          VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    dxvk->d3d11_device = NULL;
  }
  if (dxvk->d3d11_library != NULL) {
    dlclose(dxvk->d3d11_library);
    dxvk->d3d11_library = NULL;
  }
  while (dxvk->d3d9_queries != NULL) {
    VMSVGA3DDxvkD3D9Query *query = dxvk->d3d9_queries;

    dxvk->d3d9_queries = query->next;
    if (query->query != NULL) {
      vmsvga3d_dxvk_release(query->query,
                            VMSVGA3D_DXVK_IDIRECT3DQUERY9_RELEASE);
    }
    g_free(query);
  }
  if (dxvk->d3d9_pristine_state != NULL) {
    vmsvga3d_dxvk_release(dxvk->d3d9_pristine_state,
                          VMSVGA3D_DXVK_IDIRECT3DSTATEBLOCK9_RELEASE);
    dxvk->d3d9_pristine_state = NULL;
  }
  if (dxvk->d3d9_device != NULL) {
    vmsvga3d_dxvk_release(dxvk->d3d9_device,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    dxvk->d3d9_device = NULL;
  }
  if (dxvk->d3d9 != NULL) {
    vmsvga3d_dxvk_release(dxvk->d3d9,
                          VMSVGA3D_DXVK_IDIRECT3D9_RELEASE);
    dxvk->d3d9 = NULL;
  }
  if (dxvk->d3d9_library != NULL) {
    dlclose(dxvk->d3d9_library);
    dxvk->d3d9_library = NULL;
  }
#endif
  vmsvga3d_dxvk_wsi_destroy(dxvk->wsi);
  dxvk->wsi = NULL;
  g_free(dxvk);
}

bool vmsvga3d_dxvk_ready(const VMSVGA3DDxvk *dxvk) {
  return dxvk != NULL && dxvk->ready;
}

static VMSVGA3DDxvkView *vmsvga3d_dxvk_d3d11_view_find(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkViewKind kind, uint32_t cid,
    uint32_t view_id, VMSVGA3DDxvkView ***link_out) {
  VMSVGA3DDxvkView **link;

  if (dxvk == NULL) {
    return NULL;
  }
  for (link = &dxvk->d3d11_views; *link != NULL; link = &(*link)->next) {
    VMSVGA3DDxvkView *view = *link;

    if (view->kind == kind && view->cid == cid && view->view_id == view_id) {
      if (link_out != NULL) {
        *link_out = link;
      }
      return view;
    }
  }
  return NULL;
}

static void vmsvga3d_dxvk_d3d11_view_free(VMSVGA3DDxvkView *view) {
  if (view == NULL) {
    return;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (view->view != NULL) {
    vmsvga3d_dxvk_release(view->view, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  }
#endif
  g_free(view);
}

static bool vmsvga3d_dxvk_d3d11_view_destroy(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkViewKind kind, uint32_t cid,
    uint32_t view_id) {
  VMSVGA3DDxvkView **link = NULL;
  VMSVGA3DDxvkView *view;

  if (dxvk == NULL) {
    return false;
  }
  view = vmsvga3d_dxvk_d3d11_view_find(
      dxvk, kind, cid, view_id, &link);
  if (view == NULL) {
    return true;
  }
  *link = view->next;
  vmsvga3d_dxvk_d3d11_view_free(view);
  return true;
}

static bool vmsvga3d_dxvk_d3d11_view_store(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkViewKind kind, uint32_t cid,
    uint32_t view_id, VMSVGA3DDxvkSurface *surface, void *native_view) {
  VMSVGA3DDxvkView *view;

  if (dxvk == NULL || surface == NULL || native_view == NULL ||
      vmsvga3d_dxvk_d3d11_view_find(dxvk, kind, cid, view_id, NULL) != NULL) {
    return false;
  }
  view = g_try_new0(VMSVGA3DDxvkView, 1);
  if (view == NULL) {
    return false;
  }
  view->cid = cid;
  view->view_id = view_id;
  view->kind = kind;
  view->surface = surface;
  view->view = native_view;
  view->next = dxvk->d3d11_views;
  dxvk->d3d11_views = view;
  return true;
}

static void *vmsvga3d_dxvk_d3d11_view_object(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkViewKind kind, uint32_t cid,
    uint32_t view_id) {
  VMSVGA3DDxvkView *view;

  if (view_id == SVGA3D_INVALID_ID) {
    return NULL;
  }
  view = vmsvga3d_dxvk_d3d11_view_find(
      dxvk, kind, cid, view_id, NULL);
  return view != NULL ? view->view : NULL;
}

void vmsvga3d_dxvk_d3d11_view_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid) {
  VMSVGA3DDxvkView **link;

  if (dxvk == NULL) {
    return;
  }
  link = &dxvk->d3d11_views;
  while (*link != NULL) {
    VMSVGA3DDxvkView *view = *link;

    if (view->cid != cid) {
      link = &view->next;
      continue;
    }
    *link = view->next;
    vmsvga3d_dxvk_d3d11_view_free(view);
  }
}

static void vmsvga3d_dxvk_d3d11_view_surface_destroy(
    VMSVGA3DDxvkSurface *surface) {
  VMSVGA3DDxvkView **link;
  VMSVGA3DDxvk *dxvk;

  if (surface == NULL || surface->owner == NULL) {
    return;
  }
  dxvk = surface->owner;
  link = &dxvk->d3d11_views;
  while (*link != NULL) {
    VMSVGA3DDxvkView *view = *link;

    if (view->surface != surface) {
      link = &view->next;
      continue;
    }
    *link = view->next;
    vmsvga3d_dxvk_d3d11_view_free(view);
  }
}

void vmsvga3d_dxvk_d3d11_surface_invalidate_views(
    VMSVGA3DDxvkSurface *surface) {
  vmsvga3d_dxvk_d3d11_view_surface_destroy(surface);
}

void vmsvga3d_dxvk_d3d11_surface_visit_views(
    VMSVGA3DDxvkSurface *surface, VMSVGA3DDxvkSurfaceViewVisitor visitor,
    void *opaque) {
  VMSVGA3DDxvkView *view;

  if (surface == NULL || surface->owner == NULL || visitor == NULL) {
    return;
  }
  for (view = surface->owner->d3d11_views; view != NULL; view = view->next) {
    if (view->surface == surface) {
      visitor(opaque, view->kind, view->cid, view->view_id);
    }
  }
}

bool vmsvga3d_dxvk_d3d11_surface_resident(
    const VMSVGA3DDxvkSurface *surface) {
  return surface != NULL && surface->d3d11_resident;
}

VMSVGA3DDxvkSurface *vmsvga3d_dxvk_surface_create(VMSVGA3DDxvk *dxvk,
                                                    uint32_t sid) {
  VMSVGA3DDxvkSurface *surface;

  if (!vmsvga3d_dxvk_ready(dxvk)) {
    return NULL;
  }
  surface = g_try_new0(VMSVGA3DDxvkSurface, 1);
  if (surface == NULL) {
    return NULL;
  }
  surface->sid = sid;
  surface->owner = dxvk;
  surface->d3d9_resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_NONE;
  return surface;
}

static void vmsvga3d_dxvk_surface_evict_d3d9(
    VMSVGA3DDxvkSurface *surface) {
  if (surface == NULL) {
    return;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (surface->d3d9_bounce != NULL) {
    vmsvga3d_dxvk_release(surface->d3d9_bounce,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    surface->d3d9_bounce = NULL;
  }
  if (surface->d3d9_resource != NULL) {
    vmsvga3d_dxvk_release(surface->d3d9_resource,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    surface->d3d9_resource = NULL;
  }
#endif
  surface->d3d9_resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_NONE;
  surface->d3d9_usage = 0;
  surface->d3d9_format = 0;
  surface->d3d9_length = 0;
  surface->d3d9_resident = false;
  surface->d3d9_has_bounce = false;
}

static void vmsvga3d_dxvk_surface_evict_d3d11(
    VMSVGA3DDxvkSurface *surface) {
  if (surface == NULL) {
    return;
  }
  vmsvga3d_dxvk_d3d11_view_surface_destroy(surface);
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (surface->d3d11_resource != NULL) {
    vmsvga3d_dxvk_release(surface->d3d11_resource,
                          VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    surface->d3d11_resource = NULL;
  }
#endif
  memset(&surface->d3d11_desc, 0, sizeof(surface->d3d11_desc));
  surface->d3d11_resident = false;
}

void vmsvga3d_dxvk_surface_evict(VMSVGA3DDxvkSurface *surface) {
  vmsvga3d_dxvk_surface_evict_d3d9(surface);
  vmsvga3d_dxvk_surface_evict_d3d11(surface);
}

void vmsvga3d_dxvk_surface_set_renderer(VMSVGA3DDxvkSurface *surface,
                                        VMSVGA3DDxvk *dxvk) {
  if (surface != NULL) {
    surface->owner = dxvk;
  }
}

void vmsvga3d_dxvk_surface_destroy(VMSVGA3DDxvkSurface *surface) {
  vmsvga3d_dxvk_surface_evict(surface);
  g_free(surface);
}

bool vmsvga3d_dxvk_surface_info(
    const VMSVGA3DDxvkSurface *surface,
    struct vmsvga3d_d3d9_transfer_surface_s *info) {
  if (surface == NULL || info == NULL) {
    return false;
  }
  info->resource_type = surface->d3d9_resource_type;
  info->usage = surface->d3d9_usage;
  info->resident = surface->d3d9_resident;
  info->has_bounce = surface->d3d9_has_bounce;
  return true;
}

#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_surface_plan_compatible(
    const VMSVGA3DDxvkSurface *surface,
    const VMSVGA3DD3D9ResourcePlan *plan) {
  if (surface == NULL || plan == NULL || !surface->d3d9_resident ||
      surface->d3d9_resource == NULL) {
    return false;
  }
  switch (plan->use) {
  case VMSVGA3D_D3D9_RESOURCE_USE_TEXTURE:
    return surface->d3d9_resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE;
  case VMSVGA3D_D3D9_RESOURCE_USE_COLOR_TARGET:
    return (surface->d3d9_resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE ||
            surface->d3d9_resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE) &&
           (surface->d3d9_usage & VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET) != 0;
  case VMSVGA3D_D3D9_RESOURCE_USE_DEPTH_TARGET:
    return (surface->d3d9_resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE ||
            surface->d3d9_resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE) &&
           (surface->d3d9_usage & VMSVGA3D_DXVK_D3DUSAGE_DEPTHSTENCIL) != 0;
  case VMSVGA3D_D3D9_RESOURCE_USE_VERTEX_BUFFER:
    return surface->d3d9_resource_type ==
               VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER &&
           surface->d3d9_length == plan->primary.length;
  case VMSVGA3D_D3D9_RESOURCE_USE_INDEX_BUFFER:
    return surface->d3d9_resource_type ==
               VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER &&
           surface->d3d9_length == plan->primary.length &&
           surface->d3d9_format == plan->primary.format;
  default:
    return false;
  }
}
#endif

bool vmsvga3d_dxvk_surface_materialize(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d9_resource_plan_s *plan) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkCreateTexture create_texture = NULL;
  VMSVGA3DDxvkCreateVertexBuffer create_vertex_buffer = NULL;
  VMSVGA3DDxvkCreateIndexBuffer create_index_buffer = NULL;
  VMSVGA3DDxvkCreateRenderTarget create_render_target = NULL;
  VMSVGA3DDxvkCreateDepthStencilSurface create_depth_stencil = NULL;
  VMSVGA3DDxvkCreateOffscreenPlainSurface create_offscreen = NULL;
  const VMSVGA3DD3D9ResourcePlan *resource_plan = plan;
  const VMSVGA3DD3D9CreateDesc *primary_desc;
  void *primary = NULL;
  void *bounce = NULL;
  int32_t result = -1;

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || resource_plan == NULL ||
      !resource_plan->primary.valid || surface->d3d11_resident) {
    return false;
  }
  if ((resource_plan->primary.resource_type ==
           VMSVGA3D_DXVK_D3D9_RTYPE_VERTEX_BUFFER ||
       resource_plan->primary.resource_type ==
           VMSVGA3D_DXVK_D3D9_RTYPE_INDEX_BUFFER) &&
      resource_plan->primary.length == 0) {
    return false;
  }
  if (resource_plan->primary.resource_type !=
          VMSVGA3D_DXVK_D3D9_RTYPE_VERTEX_BUFFER &&
      resource_plan->primary.resource_type !=
          VMSVGA3D_DXVK_D3D9_RTYPE_INDEX_BUFFER &&
      (resource_plan->primary.width == 0 ||
       resource_plan->primary.height == 0)) {
    return false;
  }
  if (vmsvga3d_dxvk_surface_plan_compatible(surface, resource_plan)) {
    return true;
  }
  if (surface->d3d9_resident) {
    vmsvga3d_dxvk_surface_evict_d3d9(surface);
  }

  primary_desc = &resource_plan->primary;
  if (primary_desc->resource_type == VMSVGA3D_DXVK_D3D9_RTYPE_TEXTURE) {
    if (primary_desc->levels == 0 || !resource_plan->has_bounce ||
        !resource_plan->bounce.valid ||
        resource_plan->bounce.resource_type !=
            VMSVGA3D_DXVK_D3D9_RTYPE_TEXTURE ||
        resource_plan->bounce.levels != primary_desc->levels ||
        resource_plan->bounce.width != primary_desc->width ||
        resource_plan->bounce.height != primary_desc->height ||
        resource_plan->bounce.format != primary_desc->format ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_TEXTURE,
            &create_texture, sizeof(create_texture))) {
      return false;
    }
    result = create_texture(
        dxvk->d3d9_device, primary_desc->width, primary_desc->height,
        primary_desc->levels, primary_desc->usage, primary_desc->format,
        primary_desc->pool, &primary, NULL);
    if ((!vmsvga3d_dxvk_succeeded(result) || primary == NULL) &&
        resource_plan->has_fallback && resource_plan->fallback.valid &&
        resource_plan->fallback.resource_type ==
            VMSVGA3D_DXVK_D3D9_RTYPE_TEXTURE &&
        resource_plan->fallback.width == resource_plan->primary.width &&
        resource_plan->fallback.height == resource_plan->primary.height &&
        resource_plan->fallback.levels == resource_plan->primary.levels &&
        resource_plan->fallback.format == resource_plan->primary.format) {
      primary = NULL;
      primary_desc = &resource_plan->fallback;
      result = create_texture(
          dxvk->d3d9_device, primary_desc->width, primary_desc->height,
          primary_desc->levels, primary_desc->usage, primary_desc->format,
          primary_desc->pool, &primary, NULL);
    }
    if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
      return false;
    }
    result = create_texture(
        dxvk->d3d9_device, resource_plan->bounce.width,
        resource_plan->bounce.height, resource_plan->bounce.levels,
        resource_plan->bounce.usage, resource_plan->bounce.format,
        resource_plan->bounce.pool, &bounce, NULL);
    if (!vmsvga3d_dxvk_succeeded(result) || bounce == NULL) {
      vmsvga3d_dxvk_release(primary, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
      return false;
    }
    surface->d3d9_resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE;
    surface->d3d9_has_bounce = true;
  } else if (primary_desc->resource_type ==
             VMSVGA3D_DXVK_D3D9_RTYPE_SURFACE) {
    if ((primary_desc->usage & VMSVGA3D_DXVK_D3DUSAGE_DEPTHSTENCIL) != 0) {
      if (!vmsvga3d_dxvk_get_method(
              dxvk->d3d9_device,
              VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_DEPTH_STENCIL_SURFACE,
              &create_depth_stencil, sizeof(create_depth_stencil))) {
        return false;
      }
      result = create_depth_stencil(
          dxvk->d3d9_device, primary_desc->width, primary_desc->height,
          primary_desc->format, primary_desc->multisample_type,
          primary_desc->multisample_quality, primary_desc->discard,
          &primary, NULL);
      if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
        return false;
      }
    } else if ((primary_desc->usage &
                VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET) != 0) {
      if (!vmsvga3d_dxvk_get_method(
              dxvk->d3d9_device,
              VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_RENDER_TARGET,
              &create_render_target, sizeof(create_render_target)) ||
          !vmsvga3d_dxvk_get_method(
              dxvk->d3d9_device,
              VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_OFFSCREEN_PLAIN_SURFACE,
              &create_offscreen, sizeof(create_offscreen))) {
        return false;
      }
      result = create_render_target(
          dxvk->d3d9_device, primary_desc->width, primary_desc->height,
          primary_desc->format, primary_desc->multisample_type,
          primary_desc->multisample_quality, primary_desc->lockable,
          &primary, NULL);
      if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
        return false;
      }
      result = create_offscreen(
          dxvk->d3d9_device, primary_desc->width, primary_desc->height,
          primary_desc->format, VMSVGA3D_DXVK_D3DPOOL_SYSTEMMEM,
          &bounce, NULL);
      if (!vmsvga3d_dxvk_succeeded(result) || bounce == NULL) {
        vmsvga3d_dxvk_release(primary,
                              VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
        return false;
      }
      surface->d3d9_has_bounce = true;
    } else {
      return false;
    }
    surface->d3d9_resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE;
  } else if (primary_desc->resource_type ==
             VMSVGA3D_DXVK_D3D9_RTYPE_VERTEX_BUFFER) {
    if (!vmsvga3d_dxvk_get_method(
            dxvk->d3d9_device,
            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_VERTEX_BUFFER,
            &create_vertex_buffer, sizeof(create_vertex_buffer))) {
      return false;
    }
    result = create_vertex_buffer(
        dxvk->d3d9_device, primary_desc->length, primary_desc->usage, 0,
        primary_desc->pool, &primary, NULL);
    if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
      return false;
    }
    surface->d3d9_resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER;
  } else if (primary_desc->resource_type ==
             VMSVGA3D_DXVK_D3D9_RTYPE_INDEX_BUFFER) {
    if (!vmsvga3d_dxvk_get_method(
            dxvk->d3d9_device,
            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_INDEX_BUFFER,
            &create_index_buffer, sizeof(create_index_buffer))) {
      return false;
    }
    result = create_index_buffer(
        dxvk->d3d9_device, primary_desc->length, primary_desc->usage,
        primary_desc->format, primary_desc->pool, &primary, NULL);
    if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
      return false;
    }
    surface->d3d9_resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER;
  } else {
    return false;
  }

  if ((resource_plan->use == VMSVGA3D_D3D9_RESOURCE_USE_COLOR_TARGET &&
       (primary_desc->usage & VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET) == 0) ||
      (resource_plan->use == VMSVGA3D_D3D9_RESOURCE_USE_DEPTH_TARGET &&
       (primary_desc->usage & VMSVGA3D_DXVK_D3DUSAGE_DEPTHSTENCIL) == 0)) {
    if (bounce != NULL) {
      vmsvga3d_dxvk_release(bounce,
                            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    }
    vmsvga3d_dxvk_release(primary,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    return false;
  }

  surface->d3d9_resource = primary;
  surface->d3d9_bounce = bounce;
  surface->d3d9_usage = primary_desc->usage;
  surface->d3d9_format = primary_desc->format;
  surface->d3d9_length = primary_desc->length;
  surface->d3d9_resident = true;

  if (resource_plan->set_autogen_filter &&
      surface->d3d9_resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE &&
      !vmsvga3d_dxvk_d3d9_set_autogen_filter(
          surface->d3d9_resource, resource_plan->autogen_filter)) {
    vmsvga3d_dxvk_surface_evict_d3d9(surface);
    return false;
  }
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)plan;
  return false;
#endif
}

bool vmsvga3d_dxvk_surface_generate_mipmaps(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t filter) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL ||
      surface->owner != dxvk || surface->d3d11_resident ||
      !surface->d3d9_resident || surface->d3d9_resource == NULL ||
      surface->d3d9_resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE) {
    return false;
  }
  return vmsvga3d_dxvk_d3d9_generate_mip_sublevels(
      surface->d3d9_resource, filter);
#else
  (void)dxvk;
  (void)surface;
  (void)filter;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_surface_materialize(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_create_desc_s *desc,
    const VMSVGA3DDxvkSubresourceData *initial_data,
    uint32_t initial_data_count) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateBuffer create_buffer = NULL;
  VMSVGA3DDxvkD3D11CreateTexture1D create_texture1d = NULL;
  VMSVGA3DDxvkD3D11CreateTexture2D create_texture2d = NULL;
  VMSVGA3DDxvkD3D11CreateTexture3D create_texture3d = NULL;
  const VMSVGA3DD3D10CreateDesc *resource_desc = desc;
  const VMSVGA3DDxvkSubresourceData *create_data = initial_data_count != 0
                                                       ? initial_data
                                                       : NULL;
  void *resource = NULL;
  int32_t result;
  uint32_t i;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      surface == NULL || surface->d3d9_resident) {
    return false;
  }

  /* VirtualBox dxEnsureResource returns an already-created backend resource
   * verbatim.  In particular, it does not revalidate that resource against
   * whatever usage policy caused a later caller to ask for it again.
   * Validate creation descriptors and initial data only when a resource
   * actually has to be created. */
  if (surface->d3d11_resident) {
    return surface->d3d11_resource != NULL;
  }
  if (resource_desc == NULL || !resource_desc->valid ||
      initial_data_count != resource_desc->initial_subresource_count ||
      (initial_data_count != 0 && initial_data == NULL)) {
    return false;
  }
  for (i = 0; i < initial_data_count; i++) {
    if (initial_data[i].data == NULL) {
      return false;
    }
  }

  switch (resource_desc->resource_dimension) {
  case VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_BUFFER: {
    VMSVGA3DDxvkD3D11BufferDesc native = {
      .byte_width = resource_desc->byte_width,
      .usage = resource_desc->usage,
      .bind_flags = resource_desc->bind_flags,
      .cpu_access_flags = resource_desc->cpu_access_flags,
      .misc_flags = resource_desc->misc_flags,
      .structure_byte_stride = 0,
    };

    if (native.byte_width == 0 ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BUFFER,
            &create_buffer, sizeof(create_buffer))) {
      return false;
    }
    result = create_buffer(dxvk->d3d11_device, &native, create_data,
                           &resource);
    break;
  }
  case VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
    VMSVGA3DDxvkD3D11Texture1DDesc native = {
      .width = resource_desc->width,
      .mip_levels = resource_desc->mip_levels,
      .array_size = resource_desc->array_size,
      .format = resource_desc->format,
      .usage = resource_desc->usage,
      .bind_flags = resource_desc->bind_flags,
      .cpu_access_flags = resource_desc->cpu_access_flags,
      .misc_flags = resource_desc->misc_flags,
    };

    if (native.width == 0 || native.mip_levels == 0 ||
        native.array_size == 0 ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_TEXTURE1D,
            &create_texture1d, sizeof(create_texture1d))) {
      return false;
    }
    result = create_texture1d(dxvk->d3d11_device, &native, create_data,
                              &resource);
    break;
  }
  case VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
    VMSVGA3DDxvkD3D11Texture2DDesc native = {
      .width = resource_desc->width,
      .height = resource_desc->height,
      .mip_levels = resource_desc->mip_levels,
      .array_size = resource_desc->array_size,
      .format = resource_desc->format,
      .sample_desc = {
        .count = resource_desc->sample_count,
        .quality = resource_desc->sample_quality,
      },
      .usage = resource_desc->usage,
      .bind_flags = resource_desc->bind_flags,
      .cpu_access_flags = resource_desc->cpu_access_flags,
      .misc_flags = resource_desc->misc_flags,
    };

    if (native.width == 0 || native.height == 0 || native.mip_levels == 0 ||
        native.array_size == 0 || native.sample_desc.count == 0 ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_TEXTURE2D,
            &create_texture2d, sizeof(create_texture2d))) {
      return false;
    }
    result = create_texture2d(dxvk->d3d11_device, &native, create_data,
                              &resource);
    break;
  }
  case VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
    VMSVGA3DDxvkD3D11Texture3DDesc native = {
      .width = resource_desc->width,
      .height = resource_desc->height,
      .depth = resource_desc->depth,
      .mip_levels = resource_desc->mip_levels,
      .format = resource_desc->format,
      .usage = resource_desc->usage,
      .bind_flags = resource_desc->bind_flags,
      .cpu_access_flags = resource_desc->cpu_access_flags,
      .misc_flags = resource_desc->misc_flags,
    };

    if (native.width == 0 || native.height == 0 || native.depth == 0 ||
        native.mip_levels == 0 ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_TEXTURE3D,
            &create_texture3d, sizeof(create_texture3d))) {
      return false;
    }
    result = create_texture3d(dxvk->d3d11_device, &native, create_data,
                              &resource);
    break;
  }
  default:
    return false;
  }

  if (!vmsvga3d_dxvk_succeeded(result) || resource == NULL) {
    return false;
  }
  surface->d3d11_resource = resource;
  surface->d3d11_desc = *resource_desc;
  surface->d3d11_resident = true;
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)desc;
  (void)initial_data;
  (void)initial_data_count;
  return false;
#endif
}

#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_d3d11_srv_desc(
    const VMSVGA3DD3D10SRVDesc *src, VMSVGA3DDxvkD3D11SRVDesc *dst) {
  if (src == NULL || dst == NULL) {
    return false;
  }
  memset(dst, 0, sizeof(*dst));
  dst->format = src->format;
  dst->view_dimension = src->view_dimension;

  switch (src->view_dimension) {
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_BUFFER:
    dst->data[0] = src->first_element;
    dst->data[1] = src->num_elements;
    break;
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE1D:
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE2D:
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE3D:
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURECUBE:
    dst->data[0] = src->most_detailed_mip;
    dst->data[1] = src->mip_levels;
    break;
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
    dst->data[0] = src->most_detailed_mip;
    dst->data[1] = src->mip_levels;
    dst->data[2] = src->first_array_slice;
    dst->data[3] = src->array_size;
    break;
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE2DMS:
    break;
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
    dst->data[0] = src->first_array_slice;
    dst->data[1] = src->array_size;
    break;
  case VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
    dst->data[0] = src->most_detailed_mip;
    dst->data[1] = src->mip_levels;
    dst->data[2] = src->first_array_slice;
    dst->data[3] = src->array_size;
    break;
  default:
    return false;
  }
  return true;
}
#endif

bool vmsvga3d_dxvk_d3d11_shader_resource_view_ensure(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_srv_desc_s *desc) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateShaderResourceView create_view = NULL;
  VMSVGA3DDxvkD3D11SRVDesc native;
  const VMSVGA3DD3D10SRVDesc *view_desc = desc;
  void *view = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      view_id == SVGA3D_INVALID_ID || surface == NULL ||
      !surface->d3d11_resident || surface->d3d11_resource == NULL) {
    return false;
  }
  if (vmsvga3d_dxvk_d3d11_view_find(
          dxvk, VMSVGA3D_DXVK_VIEW_SHADER_RESOURCE, cid, view_id, NULL) !=
      NULL) {
    return true;
  }
  if (view_desc == NULL ||
      !vmsvga3d_dxvk_d3d11_srv_desc(view_desc, &native) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_SHADER_RESOURCE_VIEW,
          &create_view, sizeof(create_view))) {
    return false;
  }
  result = create_view(dxvk->d3d11_device, surface->d3d11_resource, &native,
                       &view);
  if (!vmsvga3d_dxvk_succeeded(result) || view == NULL) {
    return false;
  }
  if (!vmsvga3d_dxvk_d3d11_view_store(
          dxvk, VMSVGA3D_DXVK_VIEW_SHADER_RESOURCE, cid, view_id,
          surface, view)) {
    vmsvga3d_dxvk_release(view, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)view_id;
  (void)surface;
  (void)desc;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_shader_resource_view_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id) {
  return vmsvga3d_dxvk_d3d11_view_destroy(
      dxvk, VMSVGA3D_DXVK_VIEW_SHADER_RESOURCE, cid, view_id);
}

bool vmsvga3d_dxvk_d3d11_generate_mips(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11GenerateMips generate_mips = NULL;
  void *view;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL) {
    return false;
  }
  view = vmsvga3d_dxvk_d3d11_view_object(
      dxvk, VMSVGA3D_DXVK_VIEW_SHADER_RESOURCE, cid, view_id);
  if (view == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GENERATE_MIPS,
          &generate_mips, sizeof(generate_mips))) {
    return false;
  }
  generate_mips(dxvk->d3d11_context, view);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)view_id;
  return false;
#endif
}

static VMSVGA3DDxvkConstantBuffer *
vmsvga3d_dxvk_d3d11_constant_buffer_find(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t slot, VMSVGA3DDxvkConstantBuffer ***link_out) {
  VMSVGA3DDxvkConstantBuffer **link;

  if (dxvk == NULL) {
    return NULL;
  }
  for (link = &dxvk->d3d11_constant_buffers; *link != NULL;
       link = &(*link)->next) {
    VMSVGA3DDxvkConstantBuffer *buffer = *link;

    if (buffer->cid == cid && buffer->stage_index == stage_index &&
        buffer->slot == slot) {
      if (link_out != NULL) {
        *link_out = link;
      }
      return buffer;
    }
  }
  if (link_out != NULL) {
    *link_out = link;
  }
  return NULL;
}

bool vmsvga3d_dxvk_d3d11_constant_buffer_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t slot, const void *data, uint32_t size) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateBuffer create_buffer = NULL;
  VMSVGA3DDxvkD3D11BufferDesc desc = {
    .byte_width = size,
    .usage = VMSVGA3D_DXVK_D3D11_USAGE_DEFAULT,
    .bind_flags = VMSVGA3D_DXVK_D3D11_BIND_CONSTANT_BUFFER,
    .cpu_access_flags = 0,
    .misc_flags = 0,
    .structure_byte_stride = 0,
  };
  VMSVGA3DDxvkSubresourceData initial_data = {
    .data = data,
    .row_pitch = size,
    .slice_pitch = size,
  };
  VMSVGA3DDxvkConstantBuffer **link = NULL;
  VMSVGA3DDxvkConstantBuffer *old_buffer;
  VMSVGA3DDxvkConstantBuffer *new_buffer;
  void *buffer = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      stage_index >= SVGA3D_NUM_SHADERTYPE_DX10 ||
      slot >= SVGA3D_DX_MAX_CONSTBUFFERS || size == 0 ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BUFFER,
          &create_buffer, sizeof(create_buffer))) {
    return false;
  }
  result = create_buffer(dxvk->d3d11_device, &desc,
                         data != NULL ? &initial_data : NULL, &buffer);
  if (!vmsvga3d_dxvk_succeeded(result) || buffer == NULL) {
    return false;
  }
  new_buffer = g_try_new0(VMSVGA3DDxvkConstantBuffer, 1);
  if (new_buffer == NULL) {
    vmsvga3d_dxvk_release(buffer, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  new_buffer->cid = cid;
  new_buffer->stage_index = stage_index;
  new_buffer->slot = slot;
  new_buffer->buffer = buffer;

  old_buffer = vmsvga3d_dxvk_d3d11_constant_buffer_find(
      dxvk, cid, stage_index, slot, &link);
  if (old_buffer != NULL) {
    new_buffer->next = old_buffer->next;
    *link = new_buffer;
    if (old_buffer->buffer != NULL) {
      vmsvga3d_dxvk_release(old_buffer->buffer,
                            VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    g_free(old_buffer);
  } else {
    new_buffer->next = dxvk->d3d11_constant_buffers;
    dxvk->d3d11_constant_buffers = new_buffer;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)stage_index;
  (void)slot;
  (void)data;
  (void)size;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_constant_buffer_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t slot) {
  VMSVGA3DDxvkConstantBuffer **link = NULL;
  VMSVGA3DDxvkConstantBuffer *buffer;

  if (dxvk == NULL || stage_index >= SVGA3D_NUM_SHADERTYPE_DX10 ||
      slot >= SVGA3D_DX_MAX_CONSTBUFFERS) {
    return false;
  }
  buffer = vmsvga3d_dxvk_d3d11_constant_buffer_find(
      dxvk, cid, stage_index, slot, &link);
  if (buffer == NULL) {
    return true;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (buffer->buffer != NULL) {
    vmsvga3d_dxvk_release(buffer->buffer,
                          VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  }
#endif
  *link = buffer->next;
  g_free(buffer);
  return true;
}

bool vmsvga3d_dxvk_d3d11_set_shader_resources(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t start_slot, uint32_t view_count, const uint32_t *view_ids) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11SetShaderResources set_views = NULL;
  void *views[SVGA3D_DX_MAX_SRVIEWS] = { NULL };
  uint32_t method;
  uint32_t i;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      stage_index >= SVGA3D_NUM_SHADERTYPE_DX10 ||
      start_slot > SVGA3D_DX_MAX_SRVIEWS ||
      view_count > SVGA3D_DX_MAX_SRVIEWS - start_slot ||
      (view_count != 0 && view_ids == NULL)) {
    return false;
  }
  switch (stage_index) {
  case 0:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_SHADER_RESOURCES;
    break;
  case 1:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SHADER_RESOURCES;
    break;
  case 2:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_SHADER_RESOURCES;
    break;
  default:
    return false;
  }
  for (i = 0; i < view_count; i++) {
    if (view_ids[i] != SVGA3D_INVALID_ID) {
      views[i] = vmsvga3d_dxvk_d3d11_view_object(
          dxvk, VMSVGA3D_DXVK_VIEW_SHADER_RESOURCE, cid, view_ids[i]);
      if (views[i] == NULL) {
        return false;
      }
    }
  }
  if (view_count == 0) {
    return true;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, method, &set_views, sizeof(set_views))) {
    return false;
  }
  set_views(dxvk->d3d11_context, start_slot, view_count, views);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)stage_index;
  (void)start_slot;
  (void)view_count;
  (void)view_ids;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_constant_buffers(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t start_slot, uint32_t buffer_count) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11SetConstantBuffers set_buffers = NULL;
  void *buffers[SVGA3D_DX_MAX_CONSTBUFFERS] = { NULL };
  uint32_t method;
  uint32_t i;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      stage_index >= SVGA3D_NUM_SHADERTYPE_DX10 ||
      start_slot > SVGA3D_DX_MAX_CONSTBUFFERS ||
      buffer_count > SVGA3D_DX_MAX_CONSTBUFFERS - start_slot) {
    return false;
  }
  switch (stage_index) {
  case 0:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_CONSTANT_BUFFERS;
    break;
  case 1:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_CONSTANT_BUFFERS;
    break;
  case 2:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_CONSTANT_BUFFERS;
    break;
  default:
    return false;
  }
  for (i = 0; i < buffer_count; i++) {
    VMSVGA3DDxvkConstantBuffer *buffer =
        vmsvga3d_dxvk_d3d11_constant_buffer_find(
            dxvk, cid, stage_index, start_slot + i, NULL);

    if (buffer != NULL) {
      if (buffer->buffer == NULL) {
        return false;
      }
      buffers[i] = buffer->buffer;
    }
  }
  if (buffer_count != 0) {
    if (!vmsvga3d_dxvk_get_method(
            dxvk->d3d11_context, method,
            &set_buffers, sizeof(set_buffers))) {
      return false;
    }
    set_buffers(dxvk->d3d11_context, start_slot, buffer_count, buffers);
  }
  for (i = 0; i < buffer_count; i++) {
    dxvk->d3d11_bound_constant_buffers[stage_index][start_slot + i] =
        buffers[i];
    dxvk->d3d11_bound_constant_buffer_valid[stage_index][start_slot + i] =
        true;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)stage_index;
  (void)start_slot;
  (void)buffer_count;
  return false;
#endif
}

#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_d3d11_buffer_binding(
    const VMSVGA3DDxvkSurface *surface, uint32_t bind_flag,
    void **buffer) {
  if (surface == NULL || buffer == NULL || !surface->d3d11_resident ||
      surface->d3d11_resource == NULL ||
      !surface->d3d11_desc.valid ||
      surface->d3d11_desc.resource_dimension !=
          VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_BUFFER ||
      (surface->d3d11_desc.bind_flags & bind_flag) == 0) {
    return false;
  }
  *buffer = surface->d3d11_resource;
  return true;
}
#endif

bool vmsvga3d_dxvk_d3d11_set_vertex_buffers(
    VMSVGA3DDxvk *dxvk, uint32_t start_slot, uint32_t buffer_count,
    VMSVGA3DDxvkSurface *const *surfaces, const uint32_t *strides,
    const uint32_t *offsets) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11IASetVertexBuffers set_buffers = NULL;
  void *buffers[SVGA3D_DX_MAX_VERTEXBUFFERS] = { NULL };
  uint32_t i;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      start_slot > SVGA3D_DX_MAX_VERTEXBUFFERS ||
      buffer_count > SVGA3D_DX_MAX_VERTEXBUFFERS - start_slot ||
      (buffer_count != 0 &&
       (surfaces == NULL || strides == NULL || offsets == NULL))) {
    return false;
  }
  for (i = 0; i < buffer_count; i++) {
    if (surfaces[i] != NULL &&
        !vmsvga3d_dxvk_d3d11_buffer_binding(
            surfaces[i], VMSVGA3D_DXVK_D3D11_BIND_VERTEX_BUFFER,
            &buffers[i])) {
      return false;
    }
  }
  if (buffer_count != 0) {
    if (!vmsvga3d_dxvk_get_method(
            dxvk->d3d11_context,
            VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_VERTEX_BUFFERS,
            &set_buffers, sizeof(set_buffers))) {
      return false;
    }
    set_buffers(dxvk->d3d11_context, start_slot, buffer_count, buffers,
                strides, offsets);
  }
  for (i = 0; i < buffer_count; i++) {
    dxvk->d3d11_bound_vertex_buffers[start_slot + i] = buffers[i];
    dxvk->d3d11_bound_vertex_strides[start_slot + i] = strides[i];
    dxvk->d3d11_bound_vertex_offsets[start_slot + i] = offsets[i];
    dxvk->d3d11_bound_vertex_buffer_valid[start_slot + i] = true;
  }
  return true;
#else
  (void)dxvk;
  (void)start_slot;
  (void)buffer_count;
  (void)surfaces;
  (void)strides;
  (void)offsets;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_stream_output_targets(
    VMSVGA3DDxvk *dxvk,
    VMSVGA3DDxvkSurface *const surfaces[SVGA3D_DX_MAX_SOTARGETS],
    const uint32_t offsets[SVGA3D_DX_MAX_SOTARGETS]) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11SOSetTargets set_targets = NULL;
  void *buffers[SVGA3D_DX_MAX_SOTARGETS] = { NULL };
  uint32_t i;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      surfaces == NULL || offsets == NULL) {
    return false;
  }
  for (i = 0; i < SVGA3D_DX_MAX_SOTARGETS; i++) {
    if (surfaces[i] != NULL &&
        !vmsvga3d_dxvk_d3d11_buffer_binding(
            surfaces[i], VMSVGA3D_DXVK_D3D11_BIND_STREAM_OUTPUT,
            &buffers[i])) {
      return false;
    }
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_SO_SET_TARGETS,
          &set_targets, sizeof(set_targets))) {
    return false;
  }
  /* VirtualBox always rebinds the complete four-target SO table. */
  set_targets(dxvk->d3d11_context, SVGA3D_DX_MAX_SOTARGETS,
              buffers, offsets);
  return true;
#else
  (void)dxvk;
  (void)surfaces;
  (void)offsets;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_index_buffer(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    uint32_t format, uint32_t offset) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11IASetIndexBuffer set_buffer = NULL;
  void *buffer = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL) {
    return false;
  }
  if (surface != NULL) {
    if ((format != VMSVGA3D_DXVK_DXGI_R16_UINT &&
         format != VMSVGA3D_DXVK_DXGI_R32_UINT) ||
        !vmsvga3d_dxvk_d3d11_buffer_binding(
            surface, VMSVGA3D_DXVK_D3D11_BIND_INDEX_BUFFER, &buffer)) {
      return false;
    }
  } else {
    format = 0;
    offset = 0;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_INDEX_BUFFER,
          &set_buffer, sizeof(set_buffer))) {
    return false;
  }
  set_buffer(dxvk->d3d11_context, buffer, format, offset);
  dxvk->d3d11_bound_index_buffer = buffer;
  dxvk->d3d11_bound_index_format = format;
  dxvk->d3d11_bound_index_offset = offset;
  dxvk->d3d11_bound_index_buffer_valid = true;
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)format;
  (void)offset;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_create_immutable_index_buffer(
    VMSVGA3DDxvk *dxvk, const void *indices, uint32_t size,
    void **buffer) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateBuffer create_buffer = NULL;
  VMSVGA3DDxvkD3D11BufferDesc desc = {
    .byte_width = size,
    .usage = VMSVGA3D_DXVK_D3D11_USAGE_IMMUTABLE,
    .bind_flags = VMSVGA3D_DXVK_D3D11_BIND_INDEX_BUFFER,
  };
  VMSVGA3DDxvkSubresourceData initial_data = {
    .data = indices,
    .row_pitch = size,
    .slice_pitch = size,
  };
  int32_t result;

  if (buffer == NULL) {
    return false;
  }
  *buffer = NULL;
  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      indices == NULL || size == 0 ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BUFFER,
          &create_buffer, sizeof(create_buffer))) {
    return false;
  }
  result = create_buffer(dxvk->d3d11_device, &desc, &initial_data, buffer);
  if (!vmsvga3d_dxvk_succeeded(result) || *buffer == NULL) {
    if (*buffer != NULL) {
      vmsvga3d_dxvk_release(*buffer, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
      *buffer = NULL;
    }
    return false;
  }
  return true;
#else
  (void)dxvk;
  (void)indices;
  (void)size;
  (void)buffer;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_get_index_buffer(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkD3D11IndexBinding *binding) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11IAGetIndexBuffer get_buffer = NULL;

  if (binding == NULL) {
    return false;
  }
  memset(binding, 0, sizeof(*binding));
  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_GET_INDEX_BUFFER,
          &get_buffer, sizeof(get_buffer))) {
    return false;
  }
  get_buffer(dxvk->d3d11_context, &binding->buffer, &binding->format,
             &binding->offset);
  return true;
#else
  (void)dxvk;
  (void)binding;
  return false;
#endif
}


bool vmsvga3d_dxvk_d3d11_read_index_buffer(
    VMSVGA3DDxvk *dxvk, void *buffer, uint32_t offset, uint32_t bytes,
    void **data, uint32_t *data_bytes) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11BufferGetDesc get_desc = NULL;
  VMSVGA3DDxvkD3D11CreateBuffer create_buffer = NULL;
  VMSVGA3DDxvkD3D11CopySubresourceRegion copy_region = NULL;
  VMSVGA3DDxvkD3D11Map map = NULL;
  VMSVGA3DDxvkD3D11Unmap unmap = NULL;
  VMSVGA3DDxvkD3D11BufferDesc source_desc = {0};
  VMSVGA3DDxvkD3D11BufferDesc staging_desc = {0};
  VMSVGA3DDxvkD3D11Box source_box;
  VMSVGA3DDxvkD3D11MappedSubresource mapped = {0};
  void *staging_buffer = NULL;
  void *copy = NULL;
  int32_t result;

  if (data == NULL || data_bytes == NULL) {
    return false;
  }
  *data = NULL;
  *data_bytes = 0;
  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      dxvk->d3d11_context == NULL || buffer == NULL || bytes == 0 ||
      !vmsvga3d_dxvk_get_method(
          buffer, VMSVGA3D_DXVK_ID3D11BUFFER_GET_DESC,
          &get_desc, sizeof(get_desc)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BUFFER,
          &create_buffer, sizeof(create_buffer)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_COPY_SUBRESOURCE_REGION,
          &copy_region, sizeof(copy_region)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_MAP,
          &map, sizeof(map)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_UNMAP,
          &unmap, sizeof(unmap))) {
    return false;
  }

  get_desc(buffer, &source_desc);
  if (offset >= source_desc.byte_width ||
      bytes > source_desc.byte_width - offset) {
    return false;
  }

  copy = malloc(bytes);
  if (copy == NULL) {
    return false;
  }

  staging_desc.byte_width = bytes;
  staging_desc.usage = VMSVGA3D_DXVK_D3D11_USAGE_STAGING;
  staging_desc.cpu_access_flags =
      VMSVGA3D_DXVK_D3D11_CPU_ACCESS_WRITE |
      VMSVGA3D_DXVK_D3D11_CPU_ACCESS_READ;
  result = create_buffer(dxvk->d3d11_device, &staging_desc, NULL,
                         &staging_buffer);
  if (!vmsvga3d_dxvk_succeeded(result) || staging_buffer == NULL) {
    free(copy);
    return false;
  }

  source_box.left = offset;
  source_box.top = 0;
  source_box.front = 0;
  source_box.right = offset + bytes;
  source_box.bottom = 1;
  source_box.back = 1;
  copy_region(dxvk->d3d11_context, staging_buffer, 0, 0, 0, 0,
              buffer, 0, &source_box);

  result = map(dxvk->d3d11_context, staging_buffer, 0,
               VMSVGA3D_DXVK_D3D11_MAP_READ, 0, &mapped);
  if (!vmsvga3d_dxvk_succeeded(result) || mapped.data == NULL) {
    vmsvga3d_dxvk_release(staging_buffer, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    free(copy);
    return false;
  }

  memcpy(copy, mapped.data, bytes);
  unmap(dxvk->d3d11_context, staging_buffer, 0);
  vmsvga3d_dxvk_release(staging_buffer, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  *data = copy;
  *data_bytes = bytes;
  return true;
#else
  (void)dxvk;
  (void)buffer;
  (void)offset;
  (void)bytes;
  (void)data;
  (void)data_bytes;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_native_index_buffer(
    VMSVGA3DDxvk *dxvk, void *buffer, uint32_t format, uint32_t offset) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11IASetIndexBuffer set_buffer = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_INDEX_BUFFER,
          &set_buffer, sizeof(set_buffer))) {
    return false;
  }
  /* Temporary emulation binding: do not alter the cached guest IA state. */
  set_buffer(dxvk->d3d11_context, buffer, format, offset);
  return true;
#else
  (void)dxvk;
  (void)buffer;
  (void)format;
  (void)offset;
  return false;
#endif
}

void vmsvga3d_dxvk_d3d11_release_index_buffer(void *buffer) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  vmsvga3d_dxvk_release(buffer, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
#else
  (void)buffer;
#endif
}

bool vmsvga3d_dxvk_d3d11_draw(
    VMSVGA3DDxvk *dxvk, uint32_t vertex_count,
    uint32_t start_vertex_location) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11Draw draw = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW,
          &draw, sizeof(draw))) {
    return false;
  }
  draw(dxvk->d3d11_context, vertex_count, start_vertex_location);
  return true;
#else
  (void)dxvk;
  (void)vertex_count;
  (void)start_vertex_location;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_draw_indexed(
    VMSVGA3DDxvk *dxvk, uint32_t index_count,
    uint32_t start_index_location, int32_t base_vertex_location) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11DrawIndexed draw_indexed = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW_INDEXED,
          &draw_indexed, sizeof(draw_indexed))) {
    return false;
  }
  draw_indexed(dxvk->d3d11_context, index_count, start_index_location,
               base_vertex_location);
  return true;
#else
  (void)dxvk;
  (void)index_count;
  (void)start_index_location;
  (void)base_vertex_location;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_draw_instanced(
    VMSVGA3DDxvk *dxvk, uint32_t vertex_count_per_instance,
    uint32_t instance_count, uint32_t start_vertex_location,
    uint32_t start_instance_location) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11DrawInstanced draw_instanced = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW_INSTANCED,
          &draw_instanced, sizeof(draw_instanced))) {
    return false;
  }
  draw_instanced(dxvk->d3d11_context, vertex_count_per_instance,
                 instance_count, start_vertex_location,
                 start_instance_location);
  return true;
#else
  (void)dxvk;
  (void)vertex_count_per_instance;
  (void)instance_count;
  (void)start_vertex_location;
  (void)start_instance_location;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_draw_indexed_instanced(
    VMSVGA3DDxvk *dxvk, uint32_t index_count_per_instance,
    uint32_t instance_count, uint32_t start_index_location,
    int32_t base_vertex_location, uint32_t start_instance_location) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11DrawIndexedInstanced draw_indexed_instanced = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW_INDEXED_INSTANCED,
          &draw_indexed_instanced, sizeof(draw_indexed_instanced))) {
    return false;
  }
  draw_indexed_instanced(dxvk->d3d11_context, index_count_per_instance,
                         instance_count, start_index_location,
                         base_vertex_location, start_instance_location);
  return true;
#else
  (void)dxvk;
  (void)index_count_per_instance;
  (void)instance_count;
  (void)start_index_location;
  (void)base_vertex_location;
  (void)start_instance_location;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_draw_auto(VMSVGA3DDxvk *dxvk) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11DrawAuto draw_auto = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW_AUTO,
          &draw_auto, sizeof(draw_auto))) {
    return false;
  }
  draw_auto(dxvk->d3d11_context);
  return true;
#else
  (void)dxvk;
  return false;
#endif
}

void vmsvga3d_dxvk_d3d11_constant_buffer_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid) {
  VMSVGA3DDxvkConstantBuffer **link;

  if (dxvk == NULL) {
    return;
  }
  link = &dxvk->d3d11_constant_buffers;
  while (*link != NULL) {
    VMSVGA3DDxvkConstantBuffer *buffer = *link;

    if (buffer->cid != cid) {
      link = &buffer->next;
      continue;
    }
    if (buffer->stage_index < SVGA3D_NUM_SHADERTYPE_DX10 &&
        buffer->slot < SVGA3D_DX_MAX_CONSTBUFFERS &&
        dxvk->d3d11_bound_constant_buffer_valid[buffer->stage_index]
                                                   [buffer->slot] &&
        dxvk->d3d11_bound_constant_buffers[buffer->stage_index]
                                              [buffer->slot] ==
            buffer->buffer) {
      dxvk->d3d11_bound_constant_buffers[buffer->stage_index]
                                            [buffer->slot] = NULL;
      dxvk->d3d11_bound_constant_buffer_valid[buffer->stage_index]
                                                 [buffer->slot] = false;
    }
#if defined(CONFIG_LINUX) && defined(__ELF__)
    if (buffer->buffer != NULL) {
      vmsvga3d_dxvk_release(buffer->buffer,
                            VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
#endif
    *link = buffer->next;
    g_free(buffer);
  }
}

static VMSVGA3DDxvkState *vmsvga3d_dxvk_d3d11_state_find(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkStateKind kind, uint32_t cid,
    uint32_t state_id, VMSVGA3DDxvkState ***link_out) {
  VMSVGA3DDxvkState **link;

  if (dxvk == NULL) {
    return NULL;
  }
  for (link = &dxvk->d3d11_states; *link != NULL; link = &(*link)->next) {
    VMSVGA3DDxvkState *state = *link;

    if (state->kind == kind && state->cid == cid &&
        state->state_id == state_id) {
      if (link_out != NULL) {
        *link_out = link;
      }
      return state;
    }
  }
  return NULL;
}

static bool vmsvga3d_dxvk_d3d11_state_store(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkStateKind kind, uint32_t cid,
    uint32_t state_id, void *native_state) {
  VMSVGA3DDxvkState *state;

  if (dxvk == NULL || native_state == NULL) {
    return false;
  }
  state = vmsvga3d_dxvk_d3d11_state_find(
      dxvk, kind, cid, state_id, NULL);
  if (state != NULL) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
    if (state->state != NULL) {
      vmsvga3d_dxvk_release(state->state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
#endif
    state->state = native_state;
    return true;
  }

  state = g_try_new0(VMSVGA3DDxvkState, 1);
  if (state == NULL) {
    return false;
  }
  state->cid = cid;
  state->state_id = state_id;
  state->kind = kind;
  state->state = native_state;
  state->next = dxvk->d3d11_states;
  dxvk->d3d11_states = state;
  return true;
}

static bool vmsvga3d_dxvk_d3d11_state_destroy(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkStateKind kind, uint32_t cid,
    uint32_t state_id) {
  VMSVGA3DDxvkState **link = NULL;
  VMSVGA3DDxvkState *state;

  if (dxvk == NULL) {
    return false;
  }
  state = vmsvga3d_dxvk_d3d11_state_find(
      dxvk, kind, cid, state_id, &link);
  if (state == NULL) {
    return true;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (state->state != NULL) {
    vmsvga3d_dxvk_release(state->state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  }
#endif
  *link = state->next;
  g_free(state);
  return true;
}

bool vmsvga3d_dxvk_d3d11_blend_state_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const struct vmsvga3d_d3d10_blend_desc_s *desc) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateBlendState create_state = NULL;
  void *state = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      desc == NULL) {
    return false;
  }
  if (vmsvga3d_dxvk_d3d11_state_find(
          dxvk, VMSVGA3D_DXVK_STATE_BLEND, cid, state_id, NULL) != NULL) {
    return true;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BLEND_STATE,
          &create_state, sizeof(create_state))) {
    return false;
  }
  result = create_state(dxvk->d3d11_device, desc, &state);
  if (!vmsvga3d_dxvk_succeeded(result) || state == NULL) {
    if (state != NULL) {
      vmsvga3d_dxvk_release(state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    return false;
  }
  if (!vmsvga3d_dxvk_d3d11_state_store(
          dxvk, VMSVGA3D_DXVK_STATE_BLEND, cid, state_id, state)) {
    vmsvga3d_dxvk_release(state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)state_id;
  (void)desc;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_blend_state_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id) {
  return vmsvga3d_dxvk_d3d11_state_destroy(
      dxvk, VMSVGA3D_DXVK_STATE_BLEND, cid, state_id);
}

bool vmsvga3d_dxvk_d3d11_depth_stencil_state_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const struct vmsvga3d_d3d10_depth_stencil_desc_s *desc) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateDepthStencilState create_state = NULL;
  void *state = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      desc == NULL) {
    return false;
  }
  if (vmsvga3d_dxvk_d3d11_state_find(
          dxvk, VMSVGA3D_DXVK_STATE_DEPTH_STENCIL, cid, state_id, NULL) !=
      NULL) {
    return true;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_DEPTH_STENCIL_STATE,
          &create_state, sizeof(create_state))) {
    return false;
  }
  result = create_state(dxvk->d3d11_device, desc, &state);
  if (!vmsvga3d_dxvk_succeeded(result) || state == NULL) {
    if (state != NULL) {
      vmsvga3d_dxvk_release(state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    return false;
  }
  if (!vmsvga3d_dxvk_d3d11_state_store(
          dxvk, VMSVGA3D_DXVK_STATE_DEPTH_STENCIL, cid, state_id, state)) {
    vmsvga3d_dxvk_release(state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)state_id;
  (void)desc;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_depth_stencil_state_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id) {
  return vmsvga3d_dxvk_d3d11_state_destroy(
      dxvk, VMSVGA3D_DXVK_STATE_DEPTH_STENCIL, cid, state_id);
}

bool vmsvga3d_dxvk_d3d11_rasterizer_state_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const struct vmsvga3d_d3d10_rasterizer_desc_s *desc) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateRasterizerState create_state = NULL;
  void *state = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      desc == NULL) {
    return false;
  }
  if (vmsvga3d_dxvk_d3d11_state_find(
          dxvk, VMSVGA3D_DXVK_STATE_RASTERIZER, cid, state_id, NULL) != NULL) {
    return true;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_RASTERIZER_STATE,
          &create_state, sizeof(create_state))) {
    return false;
  }
  result = create_state(dxvk->d3d11_device, desc, &state);
  if (!vmsvga3d_dxvk_succeeded(result) || state == NULL) {
    if (state != NULL) {
      vmsvga3d_dxvk_release(state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    return false;
  }
  if (!vmsvga3d_dxvk_d3d11_state_store(
          dxvk, VMSVGA3D_DXVK_STATE_RASTERIZER, cid, state_id, state)) {
    vmsvga3d_dxvk_release(state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)state_id;
  (void)desc;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_rasterizer_state_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id) {
  return vmsvga3d_dxvk_d3d11_state_destroy(
      dxvk, VMSVGA3D_DXVK_STATE_RASTERIZER, cid, state_id);
}

bool vmsvga3d_dxvk_d3d11_sampler_state_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const struct vmsvga3d_d3d10_sampler_desc_s *desc) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateSamplerState create_state = NULL;
  void *state = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      desc == NULL) {
    return false;
  }
  if (vmsvga3d_dxvk_d3d11_state_find(
          dxvk, VMSVGA3D_DXVK_STATE_SAMPLER, cid, state_id, NULL) != NULL) {
    return true;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_SAMPLER_STATE,
          &create_state, sizeof(create_state))) {
    return false;
  }
  result = create_state(dxvk->d3d11_device, desc, &state);
  if (!vmsvga3d_dxvk_succeeded(result) || state == NULL) {
    if (state != NULL) {
      vmsvga3d_dxvk_release(state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    return false;
  }
  if (!vmsvga3d_dxvk_d3d11_state_store(
          dxvk, VMSVGA3D_DXVK_STATE_SAMPLER, cid, state_id, state)) {
    vmsvga3d_dxvk_release(state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)state_id;
  (void)desc;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_sampler_state_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id) {
  return vmsvga3d_dxvk_d3d11_state_destroy(
      dxvk, VMSVGA3D_DXVK_STATE_SAMPLER, cid, state_id);
}

void vmsvga3d_dxvk_d3d11_state_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid) {
  VMSVGA3DDxvkState **link;

  if (dxvk == NULL) {
    return;
  }
  link = &dxvk->d3d11_states;
  while (*link != NULL) {
    VMSVGA3DDxvkState *state = *link;

    if (state->cid != cid) {
      link = &state->next;
      continue;
    }
#if defined(CONFIG_LINUX) && defined(__ELF__)
    if (state->state != NULL) {
      vmsvga3d_dxvk_release(state->state, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
#endif
    *link = state->next;
    g_free(state);
  }
}

#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_d3d11_state_object(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkStateKind kind, uint32_t cid,
    uint32_t state_id, void **native_state) {
  VMSVGA3DDxvkState *state;

  if (native_state == NULL) {
    return false;
  }
  *native_state = NULL;
  if (state_id == SVGA3D_INVALID_ID) {
    return true;
  }
  state = vmsvga3d_dxvk_d3d11_state_find(
      dxvk, kind, cid, state_id, NULL);
  if (state != NULL) {
    *native_state = state->state;
  }
  /* VirtualBox still binds NULL when lazy state creation failed. */
  return true;
}
#endif

bool vmsvga3d_dxvk_d3d11_set_render_targets(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t render_target_count,
    const uint32_t *render_target_ids, uint32_t depth_stencil_view_id,
    uint32_t uav_start_slot) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11OMSetRenderTargetsAndUAVs set_targets = NULL;
  void *render_targets[SVGA3D_MAX_RENDER_TARGETS] = { NULL };
  void *depth_stencil = NULL;
  uint32_t i;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      render_target_count > SVGA3D_MAX_RENDER_TARGETS ||
      (render_target_count != 0 && render_target_ids == NULL)) {
    return false;
  }
  for (i = 0; i < render_target_count; i++) {
    if (render_target_ids[i] != SVGA3D_INVALID_ID) {
      render_targets[i] = vmsvga3d_dxvk_d3d11_view_object(
          dxvk, VMSVGA3D_DXVK_VIEW_RENDER_TARGET, cid,
          render_target_ids[i]);
    }
  }
  if (depth_stencil_view_id != SVGA3D_INVALID_ID) {
    depth_stencil = vmsvga3d_dxvk_d3d11_view_object(
        dxvk, VMSVGA3D_DXVK_VIEW_DEPTH_STENCIL, cid,
        depth_stencil_view_id);
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_RENDER_TARGETS_AND_UAVS,
          &set_targets, sizeof(set_targets))) {
    return false;
  }

  set_targets(dxvk->d3d11_context, render_target_count, render_targets,
              depth_stencil, uav_start_slot, 0, NULL, NULL);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)render_target_count;
  (void)render_target_ids;
  (void)depth_stencil_view_id;
  (void)uav_start_slot;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_blend_state(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const float blend_factor[4], uint32_t sample_mask) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11OMSetBlendState set_state = NULL;
  void *state = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      (state_id != SVGA3D_INVALID_ID && blend_factor == NULL) ||
      !vmsvga3d_dxvk_d3d11_state_object(
          dxvk, VMSVGA3D_DXVK_STATE_BLEND, cid, state_id, &state) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_BLEND_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  set_state(dxvk->d3d11_context, state,
            state_id == SVGA3D_INVALID_ID ? NULL : blend_factor,
            state_id == SVGA3D_INVALID_ID ? 0u : sample_mask);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)state_id;
  (void)blend_factor;
  (void)sample_mask;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_depth_stencil_state(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    uint32_t stencil_ref) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11OMSetDepthStencilState set_state = NULL;
  void *state = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_d3d11_state_object(
          dxvk, VMSVGA3D_DXVK_STATE_DEPTH_STENCIL, cid, state_id, &state) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_DEPTH_STENCIL_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  set_state(dxvk->d3d11_context, state,
            state_id == SVGA3D_INVALID_ID ? 0u : stencil_ref);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)state_id;
  (void)stencil_ref;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_rasterizer_state(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11RSSetState set_state = NULL;
  void *state = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_d3d11_state_object(
          dxvk, VMSVGA3D_DXVK_STATE_RASTERIZER, cid, state_id, &state) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  set_state(dxvk->d3d11_context, state);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)state_id;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_samplers(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stage_index,
    uint32_t start_slot, uint32_t sampler_count, const uint32_t *state_ids) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11SetSamplers set_samplers = NULL;
  void *states[SVGA3D_DX_MAX_SAMPLERS] = { NULL };
  uint32_t method;
  uint32_t i;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      sampler_count > SVGA3D_DX_MAX_SAMPLERS ||
      start_slot > SVGA3D_DX_MAX_SAMPLERS ||
      sampler_count > SVGA3D_DX_MAX_SAMPLERS - start_slot ||
      (sampler_count != 0 && state_ids == NULL)) {
    return false;
  }
  switch (stage_index) {
  case 0:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_SAMPLERS;
    break;
  case 1:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SAMPLERS;
    break;
  case 2:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_SAMPLERS;
    break;
  default:
    return false;
  }
  for (i = 0; i < sampler_count; i++) {
    if (!vmsvga3d_dxvk_d3d11_state_object(
            dxvk, VMSVGA3D_DXVK_STATE_SAMPLER, cid, state_ids[i],
            &states[i])) {
      return false;
    }
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, method,
          &set_samplers, sizeof(set_samplers))) {
    return false;
  }
  set_samplers(dxvk->d3d11_context, start_slot, sampler_count, states);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)stage_index;
  (void)start_slot;
  (void)sampler_count;
  (void)state_ids;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_primitive_topology(
    VMSVGA3DDxvk *dxvk, uint32_t topology) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11IASetPrimitiveTopology set_topology = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_PRIMITIVE_TOPOLOGY,
          &set_topology, sizeof(set_topology))) {
    return false;
  }
  set_topology(dxvk->d3d11_context, topology);
  return true;
#else
  (void)dxvk;
  (void)topology;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_viewports(
    VMSVGA3DDxvk *dxvk, uint32_t viewport_count, const void *viewports) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11RSSetViewports set_viewports = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      viewport_count > SVGA3D_DX_MAX_VIEWPORTS ||
      (viewport_count != 0 && viewports == NULL) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_VIEWPORTS,
          &set_viewports, sizeof(set_viewports))) {
    return false;
  }
  set_viewports(dxvk->d3d11_context, viewport_count,
                (const SVGA3dViewport *)viewports);
  return true;
#else
  (void)dxvk;
  (void)viewport_count;
  (void)viewports;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_scissor_rects(
    VMSVGA3DDxvk *dxvk, uint32_t rect_count, const void *rects) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11RSSetScissorRects set_rects = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      rect_count > SVGA3D_DX_MAX_SCISSORRECTS ||
      (rect_count != 0 && rects == NULL) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_SCISSOR_RECTS,
          &set_rects, sizeof(set_rects))) {
    return false;
  }
  set_rects(dxvk->d3d11_context, rect_count,
            (const SVGASignedRect *)rects);
  return true;
#else
  (void)dxvk;
  (void)rect_count;
  (void)rects;
  return false;
#endif
}

typedef struct vmsvga3d_dxvk_d3d11_blit_constants_s {
  float src_scale_x;
  float src_scale_y;
  float src_offset_x;
  float src_offset_y;
  float dst_scale_x;
  float dst_scale_y;
  float dst_offset_x;
  float dst_offset_y;
} VMSVGA3DDxvkD3D11BlitConstants;

typedef struct vmsvga3d_dxvk_d3d11_blit_saved_state_s {
  uint32_t topology;
  void *input_layout;
  void *vs_constant_buffer;
  void *vs;
  void *gs;
  void *ps_srv;
  void *ps;
  void *ps_sampler;
  void *rasterizer;
  void *blend;
  float blend_factor[4];
  uint32_t sample_mask;
  void *render_targets[VMSVGA3D_DXVK_D3D11_MAX_RENDER_TARGETS];
  void *depth_stencil;
  uint32_t viewport_count;
  SVGA3dViewport viewports[VMSVGA3D_DXVK_D3D11_MAX_VIEWPORTS];
} VMSVGA3DDxvkD3D11BlitSavedState;

static void vmsvga3d_dxvk_d3d11_blit_release_saved(
    VMSVGA3DDxvkD3D11BlitSavedState *saved)
{
  void **single[] = {
    &saved->input_layout, &saved->vs_constant_buffer, &saved->vs,
    &saved->gs, &saved->ps_srv, &saved->ps, &saved->ps_sampler,
    &saved->rasterizer, &saved->blend, &saved->depth_stencil,
  };
  uint32_t i;

  for (i = 0; i < G_N_ELEMENTS(single); i++) {
    if (*single[i] != NULL) {
      vmsvga3d_dxvk_release(*single[i], VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
      *single[i] = NULL;
    }
  }
  for (i = 0; i < VMSVGA3D_DXVK_D3D11_MAX_RENDER_TARGETS; i++) {
    if (saved->render_targets[i] != NULL) {
      vmsvga3d_dxvk_release(saved->render_targets[i],
                            VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
      saved->render_targets[i] = NULL;
    }
  }
}

static bool vmsvga3d_dxvk_d3d11_blit_shader(
    VMSVGA3DDxvk *dxvk, uint32_t program_type,
    const uint32_t *tokens, uint32_t token_count, bool vertex, void **shader)
{
  VMSVGA3DD3D10ShaderInfo info = { 0 };
  VMSVGA3DD3D10ShaderDXBC dxbc = { 0 };
  VMSVGA3DDxvkD3D11CreateShader create_shader = NULL;
  uint32_t method;
  int32_t result;

  if (shader == NULL || tokens == NULL || token_count == 0) {
    return false;
  }
  info.program_type = program_type;
  info.rewritten_bytecode = (void *)tokens;
  info.rewritten_bytecode_size = token_count * sizeof(tokens[0]);
  if (vertex) {
    info.input_signature_count = 1;
    info.input_signature[0].registerIndex = 0;
    info.input_signature[0].semanticName = SVGADX_SIGNATURE_SEMANTIC_NAME_VERTEX_ID;
    info.input_signature[0].mask = 1;
    info.input_signature[0].componentType = VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32;
    info.input_semantic[0].semantic_name = "SV_VertexID";

    info.output_signature_count = 3;
    info.output_signature[0].registerIndex = 0;
    info.output_signature[0].semanticName = SVGADX_SIGNATURE_SEMANTIC_NAME_POSITION;
    info.output_signature[0].mask = 0x0f;
    info.output_signature[0].componentType = VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32;
    info.output_semantic[0].semantic_name = "SV_POSITION";
    info.output_signature[1].registerIndex = 1;
    info.output_signature[1].semanticName = SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED;
    info.output_signature[1].mask = 0x03;
    info.output_signature[1].componentType = VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32;
    info.output_semantic[1].semantic_name = "TEXCOORD";
    info.output_signature[2] = info.output_signature[1];
    info.output_signature[2].mask = 0x0c;
    info.output_semantic[2].semantic_name = "TEXCOORD";
    info.output_semantic[2].semantic_index = 1;
    method = VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_VERTEX_SHADER;
  } else {
    info.input_signature_count = 3;
    info.input_signature[0].registerIndex = 0;
    info.input_signature[0].semanticName = SVGADX_SIGNATURE_SEMANTIC_NAME_POSITION;
    info.input_signature[0].mask = 0x0f;
    info.input_signature[0].componentType = VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32;
    info.input_semantic[0].semantic_name = "SV_POSITION";
    info.input_signature[1].registerIndex = 1;
    info.input_signature[1].semanticName = SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED;
    info.input_signature[1].mask = 0x03;
    info.input_signature[1].componentType = VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32;
    info.input_semantic[1].semantic_name = "TEXCOORD";
    info.input_signature[2] = info.input_signature[1];
    info.input_signature[2].mask = 0x0c;
    info.input_semantic[2].semantic_name = "TEXCOORD";
    info.input_semantic[2].semantic_index = 1;
    info.output_signature_count = 1;
    info.output_signature[0].registerIndex = 0;
    info.output_signature[0].semanticName = SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED;
    info.output_signature[0].mask = 0x0f;
    info.output_signature[0].componentType = VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32;
    info.output_semantic[0].semantic_name = "SV_TARGET";
    method = VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_PIXEL_SHADER;
  }

  if (vmsvga3d_d3d10_shader_create_dxbc(&info, &dxbc) ==
          VMSVGA3D_D3D10_LEVEL_INVALID ||
      dxbc.data == NULL || dxbc.size == 0 ||
      !vmsvga3d_dxvk_get_method(dxvk->d3d11_device, method,
                                &create_shader, sizeof(create_shader))) {
    vmsvga3d_d3d10_shader_dxbc_release(&dxbc);
    return false;
  }
  result = create_shader(dxvk->d3d11_device, dxbc.data, dxbc.size, NULL, shader);
  vmsvga3d_d3d10_shader_dxbc_release(&dxbc);
  return vmsvga3d_dxvk_succeeded(result) && *shader != NULL;
}

static void vmsvga3d_dxvk_d3d11_blitter_reset(VMSVGA3DDxvk *dxvk)
{
  void **objects[] = {
    &dxvk->d3d11_blit_constant_buffer,
    &dxvk->d3d11_blit_vertex_shader,
    &dxvk->d3d11_blit_pixel_shader,
    &dxvk->d3d11_blit_pixel_shader_srgb,
    &dxvk->d3d11_blit_sampler_state,
    &dxvk->d3d11_blit_rasterizer_state,
    &dxvk->d3d11_blit_blend_state,
  };
  uint32_t i;

  for (i = 0; i < G_N_ELEMENTS(objects); i++) {
    if (*objects[i] != NULL) {
      vmsvga3d_dxvk_release(*objects[i], VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
      *objects[i] = NULL;
    }
  }
  dxvk->d3d11_blitter_initialized = false;
}

static bool vmsvga3d_dxvk_d3d11_blitter_init(VMSVGA3DDxvk *dxvk)
{
  static const uint32_t vs_tokens[] = {
    0x00010050,0x00000062,0x0100086a,0x04000059,0x00208e46,0x00000000,0x00000002,0x04000060,0x00101012,0x00000000,0x00000006,
    0x04000067,0x001020f2,0x00000000,0x00000001,0x03000065,0x00102032,0x00000001,0x03000065,0x001020c2,0x00000001,
    0x02000068,0x00000001,0x0a000001,0x00100032,0x00000000,0x00101006,0x00000000,0x00004002,0x00000001,0x00000002,0x00000000,0x00000000,
    0x0f000037,0x001000c2,0x00000000,0x00100406,0x00000000,0x00004002,0x00000000,0x00000000,0x3f800000,0xbf800000,0x00004002,0x00000000,0x00000000,0xbf800000,0x3f800000,
    0x0d000037,0x00100032,0x00000000,0x00100046,0x00000000,0x00208046,0x00000000,0x00000000,0x00004002,0x00000000,0x00000000,0x00000000,0x00000000,
    0x08000000,0x00102032,0x00000001,0x00100046,0x00000000,0x00208ae6,0x00000000,0x00000000,
    0x0b000032,0x00102032,0x00000000,0x00100ae6,0x00000000,0x00208046,0x00000000,0x00000001,0x00208ae6,0x00000000,0x00000001,
    0x08000036,0x001020c2,0x00000000,0x00004002,0x00000000,0x00000000,0x00000000,0x3f800000,
    0x08000036,0x001020c2,0x00000001,0x00004002,0x00000000,0x00000000,0x3f800000,0x00000000,0x0100003a,0x0100003e
  };
  static const uint32_t ps_tokens[] = {
    0x00000050,0x0000002c,0x0100086a,0x0300005a,0x00106000,0x00000000,0x04001858,0x00107000,0x00000000,0x00005555,
    0x03001062,0x00101032,0x00000001,0x03001062,0x00101042,0x00000001,0x03000065,0x001020f2,0x00000000,0x02000068,0x00000001,
    0x8b000045,0x800000c2,0x00155543,0x00100072,0x00000000,0x00101046,0x00000001,0x00107e46,0x00000000,0x00106000,0x00000000,
    0x05000036,0x00102072,0x00000000,0x00100246,0x00000000,
    0x05000036,0x00102082,0x00000000,0x0010102a,0x00000001,0x0100003a,0x0100003e
  };
  static const uint32_t ps_srgb_tokens[] = {
    0x00000050,0x0000003b,0x0100086a,0x0300005a,0x00106000,0x00000000,0x04001858,0x00107000,0x00000000,0x00005555,
    0x03001062,0x00101032,0x00000001,0x03001062,0x00101042,0x00000001,0x03000065,0x001020f2,0x00000000,0x02000068,0x00000001,
    0x8b000045,0x800000c2,0x00155543,0x00100072,0x00000000,0x00101046,0x00000001,0x00107e46,0x00000000,0x00106000,0x00000000,
    0x0500002f,0x00100072,0x00000000,0x00100246,0x00000000,
    0x0a000038,0x00100072,0x00000000,0x00100246,0x00000000,0x00004002,0x3ee8ba2f,0x3ee8ba2f,0x3ee8ba2f,0x00000000,
    0x05000019,0x00102072,0x00000000,0x00100246,0x00000000,
    0x05000036,0x00102082,0x00000000,0x0010102a,0x00000001,0x0100003a,0x0100003e
  };
  VMSVGA3DDxvkD3D11CreateBuffer create_buffer = NULL;
  VMSVGA3DDxvkD3D11CreateSamplerState create_sampler = NULL;
  VMSVGA3DDxvkD3D11CreateRasterizerState create_rasterizer = NULL;
  VMSVGA3DDxvkD3D11CreateBlendState create_blend = NULL;
  VMSVGA3DDxvkD3D11BufferDesc buffer_desc = { 0 };
  VMSVGA3DD3D10SamplerDesc sampler = { 0 };
  VMSVGA3DD3D10RasterizerDesc rasterizer = { 0 };
  VMSVGA3DD3D10BlendDesc blend = { 0 };
  uint32_t i;

  if (dxvk->d3d11_blitter_initialized) {
    return true;
  }
  if (!vmsvga3d_dxvk_get_method(dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BUFFER,
          &create_buffer, sizeof(create_buffer)) ||
      !vmsvga3d_dxvk_get_method(dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_SAMPLER_STATE,
          &create_sampler, sizeof(create_sampler)) ||
      !vmsvga3d_dxvk_get_method(dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_RASTERIZER_STATE,
          &create_rasterizer, sizeof(create_rasterizer)) ||
      !vmsvga3d_dxvk_get_method(dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BLEND_STATE,
          &create_blend, sizeof(create_blend))) {
    return false;
  }

  buffer_desc.byte_width = sizeof(VMSVGA3DDxvkD3D11BlitConstants);
  buffer_desc.usage = VMSVGA3D_DXVK_D3D11_USAGE_DYNAMIC;
  buffer_desc.bind_flags = VMSVGA3D_DXVK_D3D11_BIND_CONSTANT_BUFFER;
  buffer_desc.cpu_access_flags = VMSVGA3D_DXVK_D3D11_CPU_ACCESS_WRITE;
  if (!vmsvga3d_dxvk_succeeded(create_buffer(dxvk->d3d11_device, &buffer_desc,
                                              NULL, &dxvk->d3d11_blit_constant_buffer)) ||
      dxvk->d3d11_blit_constant_buffer == NULL ||
      !vmsvga3d_dxvk_d3d11_blit_shader(dxvk,
          VMSVGA3D_D3D10_SHADER_PROGRAM_VERTEX, vs_tokens,
          G_N_ELEMENTS(vs_tokens), true, &dxvk->d3d11_blit_vertex_shader) ||
      !vmsvga3d_dxvk_d3d11_blit_shader(dxvk,
          VMSVGA3D_D3D10_SHADER_PROGRAM_PIXEL, ps_tokens,
          G_N_ELEMENTS(ps_tokens), false, &dxvk->d3d11_blit_pixel_shader) ||
      !vmsvga3d_dxvk_d3d11_blit_shader(dxvk,
          VMSVGA3D_D3D10_SHADER_PROGRAM_PIXEL, ps_srgb_tokens,
          G_N_ELEMENTS(ps_srgb_tokens), false,
          &dxvk->d3d11_blit_pixel_shader_srgb)) {
    goto fail;
  }

  sampler.filter = VMSVGA3D_DXVK_D3D11_FILTER_ANISOTROPIC;
  sampler.address_u = VMSVGA3D_DXVK_D3D11_TEXTURE_ADDRESS_WRAP;
  sampler.address_v = VMSVGA3D_DXVK_D3D11_TEXTURE_ADDRESS_WRAP;
  sampler.address_w = VMSVGA3D_DXVK_D3D11_TEXTURE_ADDRESS_WRAP;
  sampler.max_anisotropy = 4;
  sampler.comparison_func = VMSVGA3D_DXVK_D3D11_COMPARISON_ALWAYS;
  sampler.max_lod = 0.0f;
  if (!vmsvga3d_dxvk_succeeded(create_sampler(dxvk->d3d11_device, &sampler,
                                               &dxvk->d3d11_blit_sampler_state)) ||
      dxvk->d3d11_blit_sampler_state == NULL) {
    goto fail;
  }

  rasterizer.fill_mode = VMSVGA3D_DXVK_D3D11_FILL_SOLID;
  rasterizer.cull_mode = VMSVGA3D_DXVK_D3D11_CULL_NONE;
  if (!vmsvga3d_dxvk_succeeded(create_rasterizer(dxvk->d3d11_device,
          &rasterizer, &dxvk->d3d11_blit_rasterizer_state)) ||
      dxvk->d3d11_blit_rasterizer_state == NULL) {
    goto fail;
  }

  for (i = 0; i < SVGA3D_DX_MAX_RENDER_TARGETS; i++) {
    blend.render_target[i].src_blend = VMSVGA3D_DXVK_D3D11_BLEND_SRC_COLOR;
    blend.render_target[i].dest_blend = VMSVGA3D_DXVK_D3D11_BLEND_ZERO;
    blend.render_target[i].blend_op = VMSVGA3D_DXVK_D3D11_BLEND_OP_ADD;
    blend.render_target[i].src_blend_alpha = VMSVGA3D_DXVK_D3D11_BLEND_SRC_ALPHA;
    blend.render_target[i].dest_blend_alpha = VMSVGA3D_DXVK_D3D11_BLEND_ZERO;
    blend.render_target[i].blend_op_alpha = VMSVGA3D_DXVK_D3D11_BLEND_OP_ADD;
    blend.render_target[i].write_mask = VMSVGA3D_DXVK_D3D11_COLOR_WRITE_ENABLE_ALL;
  }
  if (!vmsvga3d_dxvk_succeeded(create_blend(dxvk->d3d11_device, &blend,
                                             &dxvk->d3d11_blit_blend_state)) ||
      dxvk->d3d11_blit_blend_state == NULL) {
    goto fail;
  }
  dxvk->d3d11_blitter_initialized = true;
  return true;

fail:
  vmsvga3d_dxvk_d3d11_blitter_reset(dxvk);
  return false;
}

static bool vmsvga3d_dxvk_d3d11_blit_save(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkD3D11BlitSavedState *saved)
{
  VMSVGA3DDxvkD3D11IAGetPrimitiveTopology get_topology = NULL;
  VMSVGA3DDxvkD3D11IAGetInputLayout get_layout = NULL;
  VMSVGA3DDxvkD3D11GetConstantBuffers get_vs_cb = NULL;
  VMSVGA3DDxvkD3D11GetShader get_vs = NULL, get_gs = NULL, get_ps = NULL;
  VMSVGA3DDxvkD3D11GetShaderResources get_ps_srv = NULL;
  VMSVGA3DDxvkD3D11GetSamplers get_ps_sampler = NULL;
  VMSVGA3DDxvkD3D11RSGetState get_rs = NULL;
  VMSVGA3DDxvkD3D11OMGetBlendState get_blend = NULL;
  VMSVGA3DDxvkD3D11OMGetRenderTargets get_rt = NULL;
  VMSVGA3DDxvkD3D11RSGetViewports get_viewports = NULL;

#define GET_BLIT_METHOD(slot, fn) \
  vmsvga3d_dxvk_get_method(dxvk->d3d11_context, (slot), &(fn), sizeof(fn))
  if (!GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_GET_PRIMITIVE_TOPOLOGY, get_topology) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_GET_INPUT_LAYOUT, get_layout) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_GET_CONSTANT_BUFFERS, get_vs_cb) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_GET_SHADER, get_vs) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_GET_SHADER, get_gs) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_GET_SHADER_RESOURCES, get_ps_srv) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_GET_SHADER, get_ps) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_GET_SAMPLERS, get_ps_sampler) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_GET_STATE, get_rs) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_GET_BLEND_STATE, get_blend) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_GET_RENDER_TARGETS, get_rt) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_GET_VIEWPORTS, get_viewports)) {
    return false;
  }
#undef GET_BLIT_METHOD

  memset(saved, 0, sizeof(*saved));
  get_topology(dxvk->d3d11_context, &saved->topology);
  get_layout(dxvk->d3d11_context, &saved->input_layout);
  get_vs_cb(dxvk->d3d11_context, 0, 1, &saved->vs_constant_buffer);
  get_vs(dxvk->d3d11_context, &saved->vs, NULL, NULL);
  get_gs(dxvk->d3d11_context, &saved->gs, NULL, NULL);
  get_ps_srv(dxvk->d3d11_context, 0, 1, &saved->ps_srv);
  get_ps(dxvk->d3d11_context, &saved->ps, NULL, NULL);
  get_ps_sampler(dxvk->d3d11_context, 0, 1, &saved->ps_sampler);
  get_rs(dxvk->d3d11_context, &saved->rasterizer);
  get_blend(dxvk->d3d11_context, &saved->blend, saved->blend_factor,
            &saved->sample_mask);
  get_rt(dxvk->d3d11_context, VMSVGA3D_DXVK_D3D11_MAX_RENDER_TARGETS,
         saved->render_targets, &saved->depth_stencil);
  saved->viewport_count = VMSVGA3D_DXVK_D3D11_MAX_VIEWPORTS;
  get_viewports(dxvk->d3d11_context, &saved->viewport_count, saved->viewports);
  return saved->viewport_count <= VMSVGA3D_DXVK_D3D11_MAX_VIEWPORTS;
}

static bool vmsvga3d_dxvk_d3d11_blit_restore(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkD3D11BlitSavedState *saved)
{
  VMSVGA3DDxvkD3D11IASetPrimitiveTopology set_topology = NULL;
  VMSVGA3DDxvkD3D11IASetInputLayout set_layout = NULL;
  VMSVGA3DDxvkD3D11SetConstantBuffers set_vs_cb = NULL;
  VMSVGA3DDxvkD3D11SetShader set_vs = NULL, set_gs = NULL, set_ps = NULL;
  VMSVGA3DDxvkD3D11SetShaderResources set_ps_srv = NULL;
  VMSVGA3DDxvkD3D11SetSamplers set_ps_sampler = NULL;
  VMSVGA3DDxvkD3D11RSSetState set_rs = NULL;
  VMSVGA3DDxvkD3D11OMSetBlendState set_blend = NULL;
  VMSVGA3DDxvkD3D11OMSetRenderTargets set_rt = NULL;
  VMSVGA3DDxvkD3D11RSSetViewports set_viewports = NULL;
  bool have_methods;

#define GET_BLIT_METHOD(slot, fn) \
  vmsvga3d_dxvk_get_method(dxvk->d3d11_context, (slot), &(fn), sizeof(fn))
  have_methods =
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_PRIMITIVE_TOPOLOGY, set_topology) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_INPUT_LAYOUT, set_layout) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_CONSTANT_BUFFERS, set_vs_cb) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_SHADER, set_vs) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_SHADER, set_gs) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SHADER_RESOURCES, set_ps_srv) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SHADER, set_ps) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SAMPLERS, set_ps_sampler) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_STATE, set_rs) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_BLEND_STATE, set_blend) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_RENDER_TARGETS, set_rt) &&
      GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_VIEWPORTS, set_viewports);
#undef GET_BLIT_METHOD
  if (have_methods) {
    set_topology(dxvk->d3d11_context, saved->topology);
    set_layout(dxvk->d3d11_context, saved->input_layout);
    set_vs_cb(dxvk->d3d11_context, 0, 1, &saved->vs_constant_buffer);
    set_vs(dxvk->d3d11_context, saved->vs, NULL, 0);
    set_gs(dxvk->d3d11_context, saved->gs, NULL, 0);
    set_ps_srv(dxvk->d3d11_context, 0, 1, &saved->ps_srv);
    set_ps(dxvk->d3d11_context, saved->ps, NULL, 0);
    set_ps_sampler(dxvk->d3d11_context, 0, 1, &saved->ps_sampler);
    set_rs(dxvk->d3d11_context, saved->rasterizer);
    set_blend(dxvk->d3d11_context, saved->blend, saved->blend_factor,
              saved->sample_mask);
    set_rt(dxvk->d3d11_context, VMSVGA3D_DXVK_D3D11_MAX_RENDER_TARGETS,
           saved->render_targets, saved->depth_stencil);
    set_viewports(dxvk->d3d11_context, saved->viewport_count, saved->viewports);
  }
  vmsvga3d_dxvk_d3d11_blit_release_saved(saved);
  return have_methods;
}

bool vmsvga3d_dxvk_d3d11_present_blt(
    VMSVGA3DDxvk *dxvk,
    VMSVGA3DDxvkSurface *source, uint32_t source_subresource,
    uint32_t source_format, const SVGA3dBox *source_box,
    const SVGA3dSize *source_size, bool source_srgb,
    VMSVGA3DDxvkSurface *destination, uint32_t destination_subresource,
    uint32_t destination_format, const SVGA3dBox *destination_box,
    const SVGA3dSize *destination_size)
{
  VMSVGA3DDxvkD3D11CreateShaderResourceView create_srv = NULL;
  VMSVGA3DDxvkD3D11CreateRenderTargetView create_rtv = NULL;
  VMSVGA3DDxvkD3D11SetConstantBuffers set_vs_cb = NULL;
  VMSVGA3DDxvkD3D11IASetInputLayout set_layout = NULL;
  VMSVGA3DDxvkD3D11IASetPrimitiveTopology set_topology = NULL;
  VMSVGA3DDxvkD3D11SetShader set_vs = NULL, set_gs = NULL, set_ps = NULL;
  VMSVGA3DDxvkD3D11SetShaderResources set_ps_srv = NULL;
  VMSVGA3DDxvkD3D11SetSamplers set_ps_sampler = NULL;
  VMSVGA3DDxvkD3D11RSSetState set_rs = NULL;
  VMSVGA3DDxvkD3D11OMSetBlendState set_blend = NULL;
  VMSVGA3DDxvkD3D11OMSetRenderTargets set_rt = NULL;
  VMSVGA3DDxvkD3D11RSSetViewports set_viewports = NULL;
  VMSVGA3DDxvkD3D11Map map = NULL;
  VMSVGA3DDxvkD3D11Unmap unmap = NULL;
  VMSVGA3DDxvkD3D11Draw draw = NULL;
  VMSVGA3DDxvkD3D11SRVDesc srv_desc = { 0 };
  VMSVGA3DDxvkD3D11RTVDesc rtv_desc = { 0 };
  VMSVGA3DDxvkD3D11BlitSavedState saved;
  VMSVGA3DDxvkD3D11MappedSubresource mapped = { 0 };
  VMSVGA3DDxvkD3D11BlitConstants constants;
  SVGA3dViewport viewport = { 0 };
  float blend_factor[4] = { 0, 0, 0, 0 };
  void *srv = NULL;
  void *rtv = NULL;
  void *null_shader = NULL;
  int32_t result;
  bool saved_valid = false;
  bool mapped_valid = false;
  bool success = false;

  if (!vmsvga3d_dxvk_ready(dxvk) || source == NULL || destination == NULL ||
      !source->d3d11_resident || !destination->d3d11_resident ||
      source->d3d11_resource == NULL || destination->d3d11_resource == NULL ||
      source_box == NULL || destination_box == NULL || source_size == NULL ||
      destination_size == NULL || source_size->width == 0 ||
      source_size->height == 0 || destination_size->width == 0 ||
      destination_size->height == 0 ||
      !vmsvga3d_dxvk_d3d11_blitter_init(dxvk)) {
    return false;
  }

  srv_desc.format = source_format;
  srv_desc.view_dimension = VMSVGA3D_DXVK_D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.data[0] = source_subresource;
  srv_desc.data[1] = 1;
  rtv_desc.format = destination_format;
  rtv_desc.view_dimension = VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE2D;
  rtv_desc.data[0] = destination_subresource;
  if (!vmsvga3d_dxvk_get_method(dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_SHADER_RESOURCE_VIEW,
          &create_srv, sizeof(create_srv)) ||
      !vmsvga3d_dxvk_get_method(dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_RENDERTARGET_VIEW,
          &create_rtv, sizeof(create_rtv))) {
    return false;
  }
  result = create_rtv(dxvk->d3d11_device, destination->d3d11_resource,
                      &rtv_desc, &rtv);
  if (!vmsvga3d_dxvk_succeeded(result) || rtv == NULL) {
    goto out;
  }
  result = create_srv(dxvk->d3d11_device, source->d3d11_resource,
                      &srv_desc, &srv);
  if (!vmsvga3d_dxvk_succeeded(result) || srv == NULL) {
    goto out;
  }
  if (!vmsvga3d_dxvk_d3d11_blit_save(dxvk, &saved)) {
    goto out;
  }
  saved_valid = true;

#define GET_BLIT_METHOD(slot, fn) \
  vmsvga3d_dxvk_get_method(dxvk->d3d11_context, (slot), &(fn), sizeof(fn))
  if (!GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_RENDER_TARGETS, set_rt) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_CONSTANT_BUFFERS, set_vs_cb) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_INPUT_LAYOUT, set_layout) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_PRIMITIVE_TOPOLOGY, set_topology) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_SHADER, set_vs) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_SHADER, set_gs) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SHADER_RESOURCES, set_ps_srv) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SHADER, set_ps) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SAMPLERS, set_ps_sampler) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_STATE, set_rs) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_BLEND_STATE, set_blend) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_VIEWPORTS, set_viewports) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_MAP, map) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_UNMAP, unmap) ||
      !GET_BLIT_METHOD(VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_DRAW, draw)) {
    goto out;
  }
#undef GET_BLIT_METHOD

  set_rt(dxvk->d3d11_context, 1, &rtv, NULL);
  set_vs_cb(dxvk->d3d11_context, 0, 1, &dxvk->d3d11_blit_constant_buffer);
  set_layout(dxvk->d3d11_context, NULL);
  set_topology(dxvk->d3d11_context,
               VMSVGA3D_DXVK_D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  set_vs(dxvk->d3d11_context, dxvk->d3d11_blit_vertex_shader, NULL, 0);
  set_gs(dxvk->d3d11_context, null_shader, NULL, 0);
  set_ps_srv(dxvk->d3d11_context, 0, 1, &srv);
  set_ps(dxvk->d3d11_context,
         source_srgb ? dxvk->d3d11_blit_pixel_shader_srgb
                     : dxvk->d3d11_blit_pixel_shader,
         NULL, 0);
  set_ps_sampler(dxvk->d3d11_context, 0, 1, &dxvk->d3d11_blit_sampler_state);
  set_rs(dxvk->d3d11_context, dxvk->d3d11_blit_rasterizer_state);
  set_blend(dxvk->d3d11_context, dxvk->d3d11_blit_blend_state,
            blend_factor, UINT32_MAX);

  viewport.width = (float)destination_size->width;
  viewport.height = (float)destination_size->height;
  viewport.maxDepth = 1.0f;
  set_viewports(dxvk->d3d11_context, 1, &viewport);

  constants.src_scale_x = (float)source_box->w / (float)source_size->width;
  constants.src_scale_y = (float)source_box->h / (float)source_size->height;
  constants.src_offset_x = (float)source_box->x / (float)source_size->width;
  constants.src_offset_y = (float)source_box->y / (float)source_size->height;
  constants.dst_scale_x = (float)destination_box->w / (float)destination_size->width;
  constants.dst_scale_y = (float)destination_box->h / (float)destination_size->height;
  constants.dst_offset_x =
      (float)(destination_box->x * 2u + destination_box->w) /
          (float)destination_size->width - 1.0f;
  constants.dst_offset_y = -((float)(destination_box->y * 2u + destination_box->h) /
          (float)destination_size->height - 1.0f);

  result = map(dxvk->d3d11_context, dxvk->d3d11_blit_constant_buffer, 0,
               VMSVGA3D_DXVK_D3D11_MAP_WRITE_DISCARD, 0, &mapped);
  if (!vmsvga3d_dxvk_succeeded(result) || mapped.data == NULL) {
    goto out;
  }
  mapped_valid = true;
  memcpy(mapped.data, &constants, sizeof(constants));
  unmap(dxvk->d3d11_context, dxvk->d3d11_blit_constant_buffer, 0);
  mapped_valid = false;
  draw(dxvk->d3d11_context, 4, 0);
  success = true;

out:
  if (mapped_valid && unmap != NULL) {
    unmap(dxvk->d3d11_context, dxvk->d3d11_blit_constant_buffer, 0);
  }
  if (saved_valid) {
    success = vmsvga3d_dxvk_d3d11_blit_restore(dxvk, &saved) && success;
  }
  if (srv != NULL) {
    vmsvga3d_dxvk_release(srv, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  }
  if (rtv != NULL) {
    vmsvga3d_dxvk_release(rtv, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  }
  return success;
}

static VMSVGA3DDxvkShader *vmsvga3d_dxvk_d3d11_shader_find(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    VMSVGA3DDxvkShader ***link_out) {
  VMSVGA3DDxvkShader **link;

  if (dxvk == NULL) {
    return NULL;
  }
  for (link = &dxvk->d3d11_shaders; *link != NULL; link = &(*link)->next) {
    VMSVGA3DDxvkShader *shader = *link;

    if (shader->cid == cid && shader->shader_id == shader_id) {
      if (link_out != NULL) {
        *link_out = link;
      }
      return shader;
    }
  }
  return NULL;
}

static void vmsvga3d_dxvk_d3d11_shader_free(VMSVGA3DDxvkShader *shader) {
  if (shader == NULL) {
    return;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (shader->shader != NULL) {
    vmsvga3d_dxvk_release(shader->shader, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  }
#endif
  if (shader->info_valid) {
    vmsvga3d_d3d10_shader_release(&shader->info);
  }
  g_free(shader->bytecode);
  g_free(shader);
}

bool vmsvga3d_dxvk_d3d11_shader_object_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type) {
  VMSVGA3DDxvkShader *shader;

  if (dxvk == NULL || shader_type < SVGA3D_SHADERTYPE_MIN ||
      shader_type >= SVGA3D_SHADERTYPE_MAX ||
      vmsvga3d_dxvk_d3d11_shader_find(dxvk, cid, shader_id, NULL) != NULL) {
    return false;
  }
  shader = g_try_new0(VMSVGA3DDxvkShader, 1);
  if (shader == NULL) {
    return false;
  }
  shader->cid = cid;
  shader->shader_id = shader_id;
  shader->shader_type = shader_type;
  shader->stream_output_id = SVGA3D_INVALID_ID;
  shader->next = dxvk->d3d11_shaders;
  dxvk->d3d11_shaders = shader;
  return true;
}

bool vmsvga3d_dxvk_d3d11_shader_object_exists(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t *shader_type) {
  VMSVGA3DDxvkShader *shader;

  if (dxvk == NULL) {
    return false;
  }
  shader = vmsvga3d_dxvk_d3d11_shader_find(dxvk, cid, shader_id, NULL);
  if (shader == NULL) {
    return false;
  }
  if (shader_type != NULL) {
    *shader_type = shader->shader_type;
  }
  return true;
}

bool vmsvga3d_dxvk_d3d11_shader_bind_info(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    VMSVGA3DD3D10ShaderInfo *info) {
  VMSVGA3DDxvkShader *shader;

  if (dxvk == NULL || info == NULL) {
    return false;
  }
  shader = vmsvga3d_dxvk_d3d11_shader_find(dxvk, cid, shader_id, NULL);
  if (shader == NULL) {
    return false;
  }

  /* VirtualBox drops generated DXBC/native code on a successful rebind, but
   * retains the backend shader record itself.
   */
  if (shader->bytecode != NULL) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
    if (shader->shader != NULL) {
      vmsvga3d_dxvk_release(shader->shader, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
#endif
    shader->shader = NULL;
    g_free(shader->bytecode);
    shader->bytecode = NULL;
    shader->bytecode_size = 0;
  }
  if (shader->info_valid) {
    vmsvga3d_d3d10_shader_release(&shader->info);
  }
  shader->info = *info;
  shader->info_valid = true;
  memset(info, 0, sizeof(*info));
  return true;
}

bool vmsvga3d_dxvk_d3d11_shader_info_for_realize(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type, VMSVGA3DD3D10ShaderInfo **info) {
  VMSVGA3DDxvkShader *shader;

  if (dxvk == NULL || info == NULL) {
    return false;
  }
  *info = NULL;
  shader = vmsvga3d_dxvk_d3d11_shader_find(
      dxvk, cid, shader_id, NULL);
  if (shader == NULL || shader->shader_type != shader_type ||
      !shader->info_valid) {
    return false;
  }

  /* dxSetupPipeline only patches a shader while its native object is absent.
   * If an earlier Create*Shader failed after DXBC generation, the Oracle runs
   * the create-time preparation again on the next setup attempt.
   */
  if (shader->shader != NULL) {
    return true;
  }
  if (shader->bytecode != NULL) {
    g_free(shader->bytecode);
    shader->bytecode = NULL;
    shader->bytecode_size = 0;
  }
  *info = &shader->info;
  return true;
}

bool vmsvga3d_dxvk_d3d11_shader_info(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type, const VMSVGA3DD3D10ShaderInfo **info) {
  VMSVGA3DDxvkShader *shader;

  if (dxvk == NULL || info == NULL) {
    return false;
  }
  *info = NULL;
  shader = vmsvga3d_dxvk_d3d11_shader_find(dxvk, cid, shader_id, NULL);
  if (shader == NULL || shader->shader_type != shader_type ||
      !shader->info_valid) {
    return false;
  }
  *info = &shader->info;
  return true;
}

static VMSVGA3DDxvkStreamOutput *vmsvga3d_dxvk_d3d11_stream_output_find(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stream_output_id,
    VMSVGA3DDxvkStreamOutput ***link_out) {
  VMSVGA3DDxvkStreamOutput **link;

  if (dxvk == NULL) {
    return NULL;
  }
  for (link = &dxvk->d3d11_stream_outputs; *link != NULL;
       link = &(*link)->next) {
    if ((*link)->cid == cid &&
        (*link)->stream_output_id == stream_output_id) {
      if (link_out != NULL) {
        *link_out = link;
      }
      return *link;
    }
  }
  return NULL;
}

bool vmsvga3d_dxvk_d3d11_stream_output_cached(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stream_output_id,
    VMSVGA3DD3D10StreamOutputPlan *plan) {
  VMSVGA3DDxvkStreamOutput *stream_output;

  if (dxvk == NULL || plan == NULL) {
    return false;
  }
  stream_output = vmsvga3d_dxvk_d3d11_stream_output_find(
      dxvk, cid, stream_output_id, NULL);
  /* dxDefineStreamOutput uses cDeclarationEntry == 0 as its cache test.
   * Therefore an empty declaration is intentionally rebuilt every time.
   */
  if (stream_output == NULL || stream_output->plan.declaration_count == 0) {
    return false;
  }
  *plan = stream_output->plan;
  return true;
}

bool vmsvga3d_dxvk_d3d11_stream_output_cache(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stream_output_id,
    const VMSVGA3DD3D10StreamOutputPlan *plan) {
  VMSVGA3DDxvkStreamOutput *stream_output;

  if (dxvk == NULL || plan == NULL) {
    return false;
  }
  stream_output = vmsvga3d_dxvk_d3d11_stream_output_find(
      dxvk, cid, stream_output_id, NULL);
  if (stream_output == NULL) {
    stream_output = g_try_new0(VMSVGA3DDxvkStreamOutput, 1);
    if (stream_output == NULL) {
      return false;
    }
    stream_output->cid = cid;
    stream_output->stream_output_id = stream_output_id;
    stream_output->next = dxvk->d3d11_stream_outputs;
    dxvk->d3d11_stream_outputs = stream_output;
  }
  stream_output->plan = *plan;
  return true;
}

bool vmsvga3d_dxvk_d3d11_stream_output_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t stream_output_id) {
  VMSVGA3DDxvkStreamOutput **link = NULL;
  VMSVGA3DDxvkStreamOutput *stream_output;

  if (dxvk == NULL) {
    return false;
  }
  stream_output = vmsvga3d_dxvk_d3d11_stream_output_find(
      dxvk, cid, stream_output_id, &link);
  if (stream_output != NULL) {
    *link = stream_output->next;
    g_free(stream_output);
  }
  return true;
}

bool vmsvga3d_dxvk_d3d11_shader_realize(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t stream_output_id,
    const VMSVGA3DD3D10StreamOutputPlan *stream_output) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateShader create_shader = NULL;
  VMSVGA3DDxvkD3D11CreateGeometryShaderWithSO create_gs_so = NULL;
  VMSVGA3DDxvkShader *shader;
  VMSVGA3DD3D10ShaderDXBC dxbc;
  VMSVGA3DD3D10Level level;
  uint32_t method;
  void *native_shader = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL) {
    return false;
  }
  shader = vmsvga3d_dxvk_d3d11_shader_find(
      dxvk, cid, shader_id, NULL);
  if (shader == NULL || !shader->info_valid) {
    return false;
  }
  if (shader->shader != NULL) {
    return true;
  }

  switch (shader->shader_type) {
  case SVGA3D_SHADERTYPE_VS:
    method = VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_VERTEX_SHADER;
    break;
  case SVGA3D_SHADERTYPE_PS:
    method = VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_PIXEL_SHADER;
    break;
  case SVGA3D_SHADERTYPE_GS:
    method = VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_GEOMETRY_SHADER;
    break;
  default:
    return false;
  }

  /* Mirror dxSetupPipeline: DXBC belongs to the persistent shader object and
   * is generated only when the active pipeline first needs the shader.
   * B3/B4/B5 insert VirtualBox's contextual signature/resource/SO updates
   * before this realization point.
   */
  if (shader->bytecode == NULL) {
    memset(&dxbc, 0, sizeof(dxbc));
    level = vmsvga3d_d3d10_shader_create_dxbc(&shader->info, &dxbc);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID ||
        dxbc.data == NULL || dxbc.size == 0) {
      vmsvga3d_d3d10_shader_dxbc_release(&dxbc);
      return false;
    }
    shader->bytecode = g_try_malloc(dxbc.size);
    if (shader->bytecode == NULL) {
      vmsvga3d_d3d10_shader_dxbc_release(&dxbc);
      return false;
    }
    memcpy(shader->bytecode, dxbc.data, dxbc.size);
    shader->bytecode_size = dxbc.size;
    vmsvga3d_d3d10_shader_dxbc_release(&dxbc);
  }

  if (shader->shader_type == SVGA3D_SHADERTYPE_GS &&
      stream_output_id != SVGA3D_INVALID_ID) {
    if (stream_output == NULL ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device,
            VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_GEOMETRY_SHADER_WITH_SO,
            &create_gs_so, sizeof(create_gs_so))) {
      return false;
    }
    result = create_gs_so(
        dxvk->d3d11_device, shader->bytecode, shader->bytecode_size,
        stream_output->declarations, stream_output->declaration_count,
        stream_output->use_explicit_strides ? stream_output->strides : NULL,
        stream_output->stride_count, stream_output->rasterized_stream,
        NULL, &native_shader);
  } else {
    if (!vmsvga3d_dxvk_get_method(dxvk->d3d11_device, method,
                                   &create_shader, sizeof(create_shader))) {
      return false;
    }
    result = create_shader(dxvk->d3d11_device, shader->bytecode,
                           shader->bytecode_size, NULL, &native_shader);
  }
  if (!vmsvga3d_dxvk_succeeded(result) || native_shader == NULL) {
    if (native_shader != NULL) {
      vmsvga3d_dxvk_release(native_shader, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    return false;
  }

  shader->shader = native_shader;
  if (shader->shader_type == SVGA3D_SHADERTYPE_GS &&
      stream_output_id != SVGA3D_INVALID_ID) {
    shader->stream_output_id = stream_output_id;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)shader_id;
  (void)stream_output_id;
  (void)stream_output;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_shader_set(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11SetShader set_shader = NULL;
  VMSVGA3DDxvkShader *shader = NULL;
  void *native_shader = NULL;
  uint32_t method;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL) {
    return false;
  }
  switch (shader_type) {
  case SVGA3D_SHADERTYPE_VS:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_SHADER;
    break;
  case SVGA3D_SHADERTYPE_PS:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SHADER;
    break;
  case SVGA3D_SHADERTYPE_GS:
    method = VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_SHADER;
    break;
  default:
    return false;
  }

  if (shader_id != SVGA3D_INVALID_ID) {
    shader = vmsvga3d_dxvk_d3d11_shader_find(
        dxvk, cid, shader_id, NULL);
    if (shader == NULL || shader->shader_type != shader_type ||
        shader->shader == NULL) {
      return false;
    }
    native_shader = shader->shader;
  }

  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, method, &set_shader, sizeof(set_shader))) {
    return false;
  }
  /* VirtualBox calls *SetShader for every stage on every dxSetupPipeline,
   * including NULL to explicitly unbind an inactive stage.
   */
  set_shader(dxvk->d3d11_context, native_shader, NULL, 0);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)shader_id;
  (void)shader_type;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_shader_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id) {
  VMSVGA3DDxvkShader **link = NULL;
  VMSVGA3DDxvkShader *shader;

  if (dxvk == NULL) {
    return false;
  }
  shader = vmsvga3d_dxvk_d3d11_shader_find(
      dxvk, cid, shader_id, &link);
  if (shader == NULL) {
    return true;
  }
  *link = shader->next;
  vmsvga3d_dxvk_d3d11_shader_free(shader);
  return true;
}

bool vmsvga3d_dxvk_d3d11_shader_bytecode(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type, const void **bytecode, uint32_t *bytecode_size) {
  VMSVGA3DDxvkShader *shader;

  if (dxvk == NULL || bytecode == NULL || bytecode_size == NULL) {
    return false;
  }
  shader = vmsvga3d_dxvk_d3d11_shader_find(
      dxvk, cid, shader_id, NULL);
  if (shader == NULL || shader->shader_type != shader_type ||
      shader->shader == NULL || shader->bytecode == NULL ||
      shader->bytecode_size == 0) {
    return false;
  }
  *bytecode = shader->bytecode;
  *bytecode_size = shader->bytecode_size;
  return true;
}


static VMSVGA3DDxvkInputLayout *vmsvga3d_dxvk_d3d11_input_layout_find(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t layout_id,
    VMSVGA3DDxvkInputLayout ***link_out) {
  VMSVGA3DDxvkInputLayout **link;

  if (dxvk == NULL) {
    return NULL;
  }
  for (link = &dxvk->d3d11_input_layouts; *link != NULL;
       link = &(*link)->next) {
    VMSVGA3DDxvkInputLayout *layout = *link;

    if (layout->cid == cid && layout->layout_id == layout_id) {
      if (link_out != NULL) {
        *link_out = link;
      }
      return layout;
    }
  }
  return NULL;
}

static void vmsvga3d_dxvk_d3d11_input_layout_free(
    VMSVGA3DDxvkInputLayout *layout) {
  if (layout == NULL) {
    return;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (layout->layout != NULL) {
    vmsvga3d_dxvk_release(layout->layout, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  }
#endif
  g_free(layout);
}

bool vmsvga3d_dxvk_d3d11_input_layout_ensure(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t layout_id,
    uint32_t shader_id,
    const struct vmsvga3d_d3d10_input_element_s *elements,
    uint32_t element_count) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  const VMSVGA3DD3D10InputElement *translated = elements;
  VMSVGA3DDxvkD3D11InputElementDesc *native = NULL;
  VMSVGA3DDxvkD3D11CreateInputLayout create_input_layout = NULL;
  VMSVGA3DDxvkShader *shader;
  VMSVGA3DDxvkInputLayout *layout;
  void *native_layout = NULL;
  uint32_t i;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      element_count > 32u || (element_count != 0 && translated == NULL)) {
    return false;
  }
  if (vmsvga3d_dxvk_d3d11_input_layout_find(
          dxvk, cid, layout_id, NULL) != NULL) {
    return true;
  }
  shader = vmsvga3d_dxvk_d3d11_shader_find(
      dxvk, cid, shader_id, NULL);
  if (shader == NULL || shader->shader_type != SVGA3D_SHADERTYPE_VS ||
      shader->bytecode == NULL || shader->bytecode_size == 0 ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_INPUT_LAYOUT,
          &create_input_layout, sizeof(create_input_layout))) {
    return false;
  }

  if (element_count != 0) {
    native = g_try_new0(VMSVGA3DDxvkD3D11InputElementDesc, element_count);
    if (native == NULL) {
      return false;
    }
    for (i = 0; i < element_count; i++) {
      native[i].semantic_name = VMSVGA3D_D3D10_INPUT_SEMANTIC;
      native[i].semantic_index = translated[i].semantic_index;
      native[i].format = translated[i].format;
      native[i].input_slot = translated[i].input_slot;
      native[i].aligned_byte_offset = translated[i].aligned_byte_offset;
      native[i].input_slot_class = translated[i].input_slot_class;
      native[i].instance_data_step_rate =
          translated[i].instance_data_step_rate;
    }
  }

  result = create_input_layout(
      dxvk->d3d11_device, native, element_count, shader->bytecode,
      shader->bytecode_size, &native_layout);
  g_free(native);
  if (!vmsvga3d_dxvk_succeeded(result) || native_layout == NULL) {
    if (native_layout != NULL) {
      vmsvga3d_dxvk_release(native_layout, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    return false;
  }

  layout = g_try_new0(VMSVGA3DDxvkInputLayout, 1);
  if (layout == NULL) {
    vmsvga3d_dxvk_release(native_layout, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  layout->cid = cid;
  layout->layout_id = layout_id;
  layout->layout = native_layout;
  layout->next = dxvk->d3d11_input_layouts;
  dxvk->d3d11_input_layouts = layout;
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)layout_id;
  (void)shader_id;
  (void)elements;
  (void)element_count;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_input_layout(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t layout_id) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11IASetInputLayout set_layout = NULL;
  VMSVGA3DDxvkInputLayout *layout = NULL;
  void *native_layout = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL) {
    return false;
  }
  if (layout_id != SVGA3D_INVALID_ID) {
    layout = vmsvga3d_dxvk_d3d11_input_layout_find(
        dxvk, cid, layout_id, NULL);
    if (layout != NULL) {
      native_layout = layout->layout;
    }
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_INPUT_LAYOUT,
          &set_layout, sizeof(set_layout))) {
    return false;
  }
  set_layout(dxvk->d3d11_context, native_layout);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)layout_id;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_input_layout_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t layout_id) {
  VMSVGA3DDxvkInputLayout **link;

  if (dxvk == NULL) {
    return false;
  }
  link = &dxvk->d3d11_input_layouts;
  while (*link != NULL) {
    VMSVGA3DDxvkInputLayout *layout = *link;

    if (layout->cid != cid || layout->layout_id != layout_id) {
      link = &layout->next;
      continue;
    }
    *link = layout->next;
    vmsvga3d_dxvk_d3d11_input_layout_free(layout);
  }
  return true;
}

void vmsvga3d_dxvk_d3d11_input_layout_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid) {
  VMSVGA3DDxvkInputLayout **link;

  if (dxvk == NULL) {
    return;
  }
  link = &dxvk->d3d11_input_layouts;
  while (*link != NULL) {
    VMSVGA3DDxvkInputLayout *layout = *link;

    if (layout->cid != cid) {
      link = &layout->next;
      continue;
    }
    *link = layout->next;
    vmsvga3d_dxvk_d3d11_input_layout_free(layout);
  }
}

void vmsvga3d_dxvk_d3d11_shader_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid) {
  VMSVGA3DDxvkShader **link;

  if (dxvk == NULL) {
    return;
  }
  vmsvga3d_dxvk_d3d11_input_layout_context_destroy(dxvk, cid);
  {
    VMSVGA3DDxvkStreamOutput **so_link = &dxvk->d3d11_stream_outputs;
    while (*so_link != NULL) {
      VMSVGA3DDxvkStreamOutput *stream_output = *so_link;
      if (stream_output->cid != cid) {
        so_link = &stream_output->next;
        continue;
      }
      *so_link = stream_output->next;
      g_free(stream_output);
    }
  }
  link = &dxvk->d3d11_shaders;
  while (*link != NULL) {
    VMSVGA3DDxvkShader *shader = *link;

    if (shader->cid != cid) {
      link = &shader->next;
      continue;
    }
    *link = shader->next;
    vmsvga3d_dxvk_d3d11_shader_free(shader);
  }
}

static VMSVGA3DDxvkD3D9Query *vmsvga3d_dxvk_d3d9_query_find(
    VMSVGA3DDxvk *dxvk, uint32_t cid, VMSVGA3DDxvkD3D9Query ***link_out) {
  VMSVGA3DDxvkD3D9Query **link;

  if (dxvk == NULL) {
    return NULL;
  }
  link = &dxvk->d3d9_queries;
  while (*link != NULL) {
    if ((*link)->cid == cid) {
      if (link_out != NULL) {
        *link_out = link;
      }
      return *link;
    }
    link = &(*link)->next;
  }
  if (link_out != NULL) {
    *link_out = link;
  }
  return NULL;
}

static void vmsvga3d_dxvk_d3d9_query_delete_link(
    VMSVGA3DDxvkD3D9Query **link) {
  VMSVGA3DDxvkD3D9Query *query;

  if (link == NULL || *link == NULL) {
    return;
  }
  query = *link;
  *link = query->next;
  if (query->query != NULL) {
    vmsvga3d_dxvk_release(query->query,
                          VMSVGA3D_DXVK_IDIRECT3DQUERY9_RELEASE);
  }
  g_free(query);
}

static VMSVGA3DDxvkD3D9Query *vmsvga3d_dxvk_d3d9_query_ensure(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_type) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkCreateQuery create_query = NULL;
  VMSVGA3DDxvkD3D9Query *query;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk)) {
    return NULL;
  }
  query = vmsvga3d_dxvk_d3d9_query_find(dxvk, cid, NULL);
  if (query != NULL) {
    return query->query != NULL ? query : NULL;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_QUERY,
          &create_query, sizeof(create_query))) {
    return NULL;
  }
  query = g_try_new0(VMSVGA3DDxvkD3D9Query, 1);
  if (query == NULL) {
    return NULL;
  }
  result = create_query(dxvk->d3d9_device, query_type, &query->query);
  if (result != 0 || query->query == NULL) {
    if (query->query != NULL) {
      vmsvga3d_dxvk_release(query->query,
                            VMSVGA3D_DXVK_IDIRECT3DQUERY9_RELEASE);
    }
    g_free(query);
    return NULL;
  }
  query->cid = cid;
  query->next = dxvk->d3d9_queries;
  dxvk->d3d9_queries = query;
  return query;
#else
  (void)dxvk;
  (void)cid;
  (void)query_type;
  return NULL;
#endif
}

bool vmsvga3d_dxvk_d3d9_query_begin(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_type,
    uint32_t issue_flags) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D9Query **link = NULL;
  VMSVGA3DDxvkD3D9Query *query;
  VMSVGA3DDxvkQueryIssue issue = NULL;
  int32_t result;

  query = vmsvga3d_dxvk_d3d9_query_ensure(dxvk, cid, query_type);
  if (query == NULL) {
    return false;
  }
  (void)vmsvga3d_dxvk_d3d9_query_find(dxvk, cid, &link);
  if (!vmsvga3d_dxvk_get_method(query->query,
                                 VMSVGA3D_DXVK_IDIRECT3DQUERY9_ISSUE,
                                 &issue, sizeof(issue))) {
    vmsvga3d_dxvk_d3d9_query_delete_link(link);
    return false;
  }
  result = issue(query->query, issue_flags);
  if (result == 0) {
    return true;
  }
  vmsvga3d_dxvk_d3d9_query_delete_link(link);
  return false;
#else
  (void)dxvk;
  (void)cid;
  (void)query_type;
  (void)issue_flags;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d9_query_end(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t issue_flags) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D9Query **link = NULL;
  VMSVGA3DDxvkD3D9Query *query;
  VMSVGA3DDxvkQueryIssue issue = NULL;
  int32_t result;

  query = vmsvga3d_dxvk_d3d9_query_find(dxvk, cid, &link);
  if (query == NULL || query->query == NULL) {
    return false;
  }
  if (!vmsvga3d_dxvk_get_method(query->query,
                                 VMSVGA3D_DXVK_IDIRECT3DQUERY9_ISSUE,
                                 &issue, sizeof(issue))) {
    vmsvga3d_dxvk_d3d9_query_delete_link(link);
    return false;
  }
  result = issue(query->query, issue_flags);
  if (result == 0) {
    return true;
  }
  vmsvga3d_dxvk_d3d9_query_delete_link(link);
  return false;
#else
  (void)dxvk;
  (void)cid;
  (void)issue_flags;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d9_query_get_data(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t data_size,
    uint32_t flags, uint32_t *value) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D9Query **link = NULL;
  VMSVGA3DDxvkD3D9Query *query;
  VMSVGA3DDxvkQueryGetData get_data = NULL;
  int32_t result;
  uint32_t data = 0;

  if (value == NULL || data_size != sizeof(data)) {
    return false;
  }
  query = vmsvga3d_dxvk_d3d9_query_find(dxvk, cid, &link);
  if (query == NULL || query->query == NULL) {
    return false;
  }
  if (!vmsvga3d_dxvk_get_method(query->query,
                                 VMSVGA3D_DXVK_IDIRECT3DQUERY9_GET_DATA,
                                 &get_data, sizeof(get_data))) {
    vmsvga3d_dxvk_d3d9_query_delete_link(link);
    return false;
  }
  do {
    result = get_data(query->query, &data, data_size, flags);
  } while (result == VMSVGA3D_DXVK_D3D_S_FALSE);

  if (result != 0) {
    vmsvga3d_dxvk_d3d9_query_delete_link(link);
    return false;
  }
  *value = data;
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)data_size;
  (void)flags;
  (void)value;
  return false;
#endif
}

void vmsvga3d_dxvk_d3d9_query_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D9Query **link = NULL;

  if (vmsvga3d_dxvk_d3d9_query_find(dxvk, cid, &link) != NULL) {
    vmsvga3d_dxvk_d3d9_query_delete_link(link);
  }
#else
  (void)dxvk;
  (void)cid;
#endif
}

static VMSVGA3DDxvkQuery *vmsvga3d_dxvk_d3d11_query_find(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id,
    VMSVGA3DDxvkQuery ***link_out) {
  VMSVGA3DDxvkQuery **link;

  if (dxvk == NULL) {
    return NULL;
  }
  for (link = &dxvk->d3d11_queries; *link != NULL; link = &(*link)->next) {
    VMSVGA3DDxvkQuery *query = *link;

    if (query->cid == cid && query->query_id == query_id) {
      if (link_out != NULL) {
        *link_out = link;
      }
      return query;
    }
  }
  return NULL;
}

bool vmsvga3d_dxvk_d3d11_query_exists(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id) {
  VMSVGA3DDxvkQuery *query =
      vmsvga3d_dxvk_d3d11_query_find(dxvk, cid, query_id, NULL);

#if defined(CONFIG_LINUX) && defined(__ELF__)
  return query != NULL && query->query != NULL;
#else
  (void)query;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_query_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id) {
  VMSVGA3DDxvkQuery **link = NULL;
  VMSVGA3DDxvkQuery *query;

  if (dxvk == NULL) {
    return false;
  }
  query = vmsvga3d_dxvk_d3d11_query_find(dxvk, cid, query_id, &link);
  if (query == NULL) {
    return true;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (query->query != NULL) {
    vmsvga3d_dxvk_release(query->query, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  }
#endif
  *link = query->next;
  g_free(query);
  return true;
}

void vmsvga3d_dxvk_d3d11_query_context_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid) {
  VMSVGA3DDxvkQuery **link;

  if (dxvk == NULL) {
    return;
  }
  link = &dxvk->d3d11_queries;
  while (*link != NULL) {
    VMSVGA3DDxvkQuery *query = *link;

    if (query->cid != cid) {
      link = &query->next;
      continue;
    }
#if defined(CONFIG_LINUX) && defined(__ELF__)
    if (query->query != NULL) {
      vmsvga3d_dxvk_release(query->query, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
#endif
    *link = query->next;
    g_free(query);
  }
}

bool vmsvga3d_dxvk_d3d11_query_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id,
    uint32_t d3d_query, uint32_t misc_flags) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateQuery create_query = NULL;
  VMSVGA3DDxvkD3D11CreatePredicate create_predicate = NULL;
  VMSVGA3DDxvkD3D11QueryDesc desc = {
    .query = d3d_query,
    .misc_flags = misc_flags,
  };
  VMSVGA3DDxvkQuery *query;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      !vmsvga3d_dxvk_d3d11_query_destroy(dxvk, cid, query_id)) {
    return false;
  }
  if ((misc_flags & VMSVGA3D_D3D10_QUERY_MISC_PREDICATEHINT) != 0) {
    if (!vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device,
            VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_PREDICATE,
            &create_predicate, sizeof(create_predicate))) {
      return false;
    }
  } else if (!vmsvga3d_dxvk_get_method(
                 dxvk->d3d11_device,
                 VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_QUERY,
                 &create_query, sizeof(create_query))) {
    return false;
  }
  query = g_try_new0(VMSVGA3DDxvkQuery, 1);
  if (query == NULL) {
    return false;
  }
  if ((misc_flags & VMSVGA3D_D3D10_QUERY_MISC_PREDICATEHINT) != 0) {
    result = create_predicate(dxvk->d3d11_device, &desc, &query->query);
  } else {
    result = create_query(dxvk->d3d11_device, &desc, &query->query);
  }
  if (!vmsvga3d_dxvk_succeeded(result) || query->query == NULL) {
    if (query->query != NULL) {
      vmsvga3d_dxvk_release(query->query, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    g_free(query);
    return false;
  }
  query->cid = cid;
  query->query_id = query_id;
  query->d3d_query = d3d_query;
  query->misc_flags = misc_flags;
  query->next = dxvk->d3d11_queries;
  dxvk->d3d11_queries = query;
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)query_id;
  (void)d3d_query;
  (void)misc_flags;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_query_begin(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, bool issue_begin) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11Begin begin = NULL;
  VMSVGA3DDxvkQuery *query;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL) {
    return false;
  }
  query = vmsvga3d_dxvk_d3d11_query_find(dxvk, cid, query_id, NULL);
  if (query == NULL || query->query == NULL) {
    return false;
  }
  if (!issue_begin) {
    return true;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_BEGIN,
          &begin, sizeof(begin))) {
    return false;
  }
  begin(dxvk->d3d11_context, query->query);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)query_id;
  (void)issue_begin;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_query_end(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, bool issue_end) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11End end_query = NULL;
  VMSVGA3DDxvkQuery *query;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL) {
    return false;
  }
  query = vmsvga3d_dxvk_d3d11_query_find(dxvk, cid, query_id, NULL);
  if (query == NULL || query->query == NULL) {
    return false;
  }
  if (!issue_end) {
    return true;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_END,
          &end_query, sizeof(end_query))) {
    return false;
  }
  end_query(dxvk->d3d11_context, query->query);
  /* VBox keeps backend-pending membership separate from the guest COTable
   * state.  Predicate-hint queries deliberately never enter that list. */
  query->pending =
      (query->misc_flags & VMSVGA3D_D3D10_QUERY_MISC_PREDICATEHINT) == 0;
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)query_id;
  (void)issue_end;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_query_get_data(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, void *data,
    uint32_t data_size, uint32_t getdata_flags, bool *ready) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11GetData get_data = NULL;
  VMSVGA3DDxvkQuery *query;
  int32_t result;

  if (ready != NULL) {
    *ready = false;
  }
  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      data == NULL || data_size == 0 || ready == NULL) {
    return false;
  }
  query = vmsvga3d_dxvk_d3d11_query_find(dxvk, cid, query_id, NULL);
  if (query == NULL || query->query == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GET_DATA,
          &get_data, sizeof(get_data))) {
    return false;
  }

  result = get_data(dxvk->d3d11_context, query->query, data, data_size,
                    getdata_flags);
  if (result == 0) { /* S_OK */
    query->pending = false;
    *ready = true;
    return true;
  }
  if (result == 1) { /* S_FALSE: query is still pending. */
    return true;
  }
  query->pending = false;
  return false;
#else
  (void)dxvk;
  (void)cid;
  (void)query_id;
  (void)data;
  (void)data_size;
  (void)getdata_flags;
  (void)ready;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_query_pending(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkQuery *query =
      vmsvga3d_dxvk_d3d11_query_find(dxvk, cid, query_id, NULL);

  return query != NULL && query->query != NULL && query->pending;
#else
  (void)dxvk;
  (void)cid;
  (void)query_id;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_predication(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, bool enabled,
    bool predicate_value) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11SetPredication set_predication = NULL;
  VMSVGA3DDxvkQuery *query = NULL;
  void *predicate = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_SET_PREDICATION,
          &set_predication, sizeof(set_predication))) {
    return false;
  }
  if (enabled) {
    query = vmsvga3d_dxvk_d3d11_query_find(dxvk, cid, query_id, NULL);
    if (query == NULL || query->query == NULL) {
      return false;
    }
    /* Current VirtualBox binds the object created by DEFINE_QUERY.  In debug
     * builds it asserts that this was a predicate-hint query, but release
     * builds do not add a guest-visible validation here.
     */
    predicate = query->query;
  }

  set_predication(dxvk->d3d11_context, predicate,
                  predicate_value ? 1 : 0);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)query_id;
  (void)enabled;
  (void)predicate_value;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_copy_subresource_region(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *destination,
    uint32_t destination_subresource, uint32_t destination_x,
    uint32_t destination_y, uint32_t destination_z,
    VMSVGA3DDxvkSurface *source, uint32_t source_subresource,
    const struct vmsvga3d_d3d10_box_s *source_box) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CopySubresourceRegion copy_region = NULL;
  const VMSVGA3DD3D10Box *box = source_box;
  VMSVGA3DDxvkD3D11Box native;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      destination == NULL || source == NULL || box == NULL ||
      !destination->d3d11_resident || destination->d3d11_resource == NULL ||
      !source->d3d11_resident || source->d3d11_resource == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_COPY_SUBRESOURCE_REGION,
          &copy_region, sizeof(copy_region))) {
    return false;
  }
  native.left = box->left;
  native.top = box->top;
  native.front = box->front;
  native.right = box->right;
  native.bottom = box->bottom;
  native.back = box->back;
  copy_region(dxvk->d3d11_context, destination->d3d11_resource,
              destination_subresource, destination_x, destination_y,
              destination_z, source->d3d11_resource, source_subresource,
              &native);
  return true;
#else
  (void)dxvk;
  (void)destination;
  (void)destination_subresource;
  (void)destination_x;
  (void)destination_y;
  (void)destination_z;
  (void)source;
  (void)source_subresource;
  (void)source_box;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_copy_resource(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *destination,
    VMSVGA3DDxvkSurface *source) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CopyResource copy_resource = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      destination == NULL || source == NULL ||
      !destination->d3d11_resident || destination->d3d11_resource == NULL ||
      !source->d3d11_resident || source->d3d11_resource == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_COPY_RESOURCE,
          &copy_resource, sizeof(copy_resource))) {
    return false;
  }
  copy_resource(dxvk->d3d11_context, destination->d3d11_resource,
                source->d3d11_resource);
  return true;
#else
  (void)dxvk;
  (void)destination;
  (void)source;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_resolve_subresource(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *destination,
    uint32_t destination_subresource, VMSVGA3DDxvkSurface *source,
    uint32_t source_subresource, uint32_t format) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11ResolveSubresource resolve_subresource = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      destination == NULL || source == NULL ||
      !destination->d3d11_resident || destination->d3d11_resource == NULL ||
      !source->d3d11_resident || source->d3d11_resource == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RESOLVE_SUBRESOURCE,
          &resolve_subresource, sizeof(resolve_subresource))) {
    return false;
  }
  resolve_subresource(dxvk->d3d11_context, destination->d3d11_resource,
                      destination_subresource, source->d3d11_resource,
                      source_subresource, format);
  return true;
#else
  (void)dxvk;
  (void)destination;
  (void)destination_subresource;
  (void)source;
  (void)source_subresource;
  (void)format;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_update_subresource(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t subresource,
    const struct vmsvga3d_d3d10_box_s *box, const void *data,
    uint32_t row_pitch, uint32_t depth_pitch) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11UpdateSubresource update = NULL;
  const VMSVGA3DD3D10Box *source_box = box;
  VMSVGA3DDxvkD3D11Box native;

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || source_box == NULL ||
      data == NULL || row_pitch == 0 || depth_pitch == 0 ||
      surface->d3d9_resident) {
    return false;
  }
  if (!surface->d3d11_resident) {
    return true;
  }
  if (dxvk->d3d11_context == NULL || surface->d3d11_resource == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_UPDATE_SUBRESOURCE,
          &update, sizeof(update))) {
    return false;
  }
  native.left = source_box->left;
  native.top = source_box->top;
  native.front = source_box->front;
  native.right = source_box->right;
  native.bottom = source_box->bottom;
  native.back = source_box->back;
  update(dxvk->d3d11_context, surface->d3d11_resource, subresource, &native,
         data, row_pitch, depth_pitch);
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)subresource;
  (void)box;
  (void)data;
  (void)row_pitch;
  (void)depth_pitch;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_readback_subresource(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t subresource,
    void *data, uint32_t row_bytes, uint32_t row_pitch, uint32_t row_count,
    uint32_t depth_pitch, uint32_t depth_count) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateBuffer create_buffer = NULL;
  VMSVGA3DDxvkD3D11CreateTexture1D create_texture1d = NULL;
  VMSVGA3DDxvkD3D11CreateTexture2D create_texture2d = NULL;
  VMSVGA3DDxvkD3D11CreateTexture3D create_texture3d = NULL;
  VMSVGA3DDxvkD3D11CopySubresourceRegion copy_region = NULL;
  VMSVGA3DDxvkD3D11Map map = NULL;
  VMSVGA3DDxvkD3D11Unmap unmap = NULL;
  VMSVGA3DDxvkD3D11MappedSubresource mapped = {0};
  const VMSVGA3DD3D10CreateDesc *desc;
  void *staging = NULL;
  uint32_t mip_level;
  uint64_t max_subresources;
  uint32_t z;
  uint32_t y;
  int32_t result;
  bool success = false;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      dxvk->d3d11_context == NULL || surface == NULL || data == NULL ||
      row_bytes == 0 || row_pitch < row_bytes || row_count == 0 ||
      depth_pitch == 0 || depth_count == 0 || surface->d3d9_resident) {
    return false;
  }
  /* Like VirtualBox's map helper, a non-resident resource is already backed
   * by the CPU shadow and therefore needs no GPU readback. */
  if (!surface->d3d11_resident) {
    return true;
  }
  if (surface->d3d11_resource == NULL || !surface->d3d11_desc.valid ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_COPY_SUBRESOURCE_REGION,
          &copy_region, sizeof(copy_region)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_MAP,
          &map, sizeof(map)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_UNMAP,
          &unmap, sizeof(unmap))) {
    return false;
  }
  desc = &surface->d3d11_desc;

  switch (desc->resource_dimension) {
  case VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_BUFFER: {
    VMSVGA3DDxvkD3D11BufferDesc staging_desc = {0};
    VMSVGA3DDxvkD3D11Box source_box;

    if (subresource != 0 || row_count != 1 || depth_count != 1 ||
        row_bytes > desc->byte_width ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BUFFER,
            &create_buffer, sizeof(create_buffer))) {
      return false;
    }
    staging_desc.byte_width = row_bytes;
    staging_desc.usage = VMSVGA3D_DXVK_D3D11_USAGE_STAGING;
    staging_desc.cpu_access_flags = VMSVGA3D_DXVK_D3D11_CPU_ACCESS_READ;
    result = create_buffer(dxvk->d3d11_device, &staging_desc, NULL,
                           &staging);
    if (!vmsvga3d_dxvk_succeeded(result) || staging == NULL) {
      return false;
    }
    source_box.left = 0;
    source_box.top = 0;
    source_box.front = 0;
    source_box.right = row_bytes;
    source_box.bottom = 1;
    source_box.back = 1;
    copy_region(dxvk->d3d11_context, staging, 0, 0, 0, 0,
                surface->d3d11_resource, 0, &source_box);
    break;
  }
  case VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
    VMSVGA3DDxvkD3D11Texture1DDesc staging_desc = {0};

    max_subresources = (uint64_t)desc->mip_levels * desc->array_size;
    if (desc->mip_levels == 0 || desc->array_size == 0 ||
        subresource >= max_subresources ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_TEXTURE1D,
            &create_texture1d, sizeof(create_texture1d))) {
      return false;
    }
    mip_level = subresource % desc->mip_levels;
    staging_desc.width = MAX(1u, desc->width >> mip_level);
    staging_desc.mip_levels = 1;
    staging_desc.array_size = 1;
    staging_desc.format = desc->format;
    staging_desc.usage = VMSVGA3D_DXVK_D3D11_USAGE_STAGING;
    staging_desc.cpu_access_flags = VMSVGA3D_DXVK_D3D11_CPU_ACCESS_READ;
    result = create_texture1d(dxvk->d3d11_device, &staging_desc, NULL,
                              &staging);
    if (!vmsvga3d_dxvk_succeeded(result) || staging == NULL) {
      return false;
    }
    copy_region(dxvk->d3d11_context, staging, 0, 0, 0, 0,
                surface->d3d11_resource, subresource, NULL);
    break;
  }
  case VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
    VMSVGA3DDxvkD3D11Texture2DDesc staging_desc = {0};

    max_subresources = (uint64_t)desc->mip_levels * desc->array_size;
    if (desc->mip_levels == 0 || desc->array_size == 0 ||
        desc->sample_count != 1 ||
        subresource >= max_subresources ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_TEXTURE2D,
            &create_texture2d, sizeof(create_texture2d))) {
      return false;
    }
    mip_level = subresource % desc->mip_levels;
    staging_desc.width = MAX(1u, desc->width >> mip_level);
    staging_desc.height = MAX(1u, desc->height >> mip_level);
    staging_desc.mip_levels = 1;
    staging_desc.array_size = 1;
    staging_desc.format = desc->format;
    staging_desc.sample_desc.count = 1;
    staging_desc.usage = VMSVGA3D_DXVK_D3D11_USAGE_STAGING;
    staging_desc.cpu_access_flags = VMSVGA3D_DXVK_D3D11_CPU_ACCESS_READ;
    result = create_texture2d(dxvk->d3d11_device, &staging_desc, NULL,
                              &staging);
    if (!vmsvga3d_dxvk_succeeded(result) || staging == NULL) {
      return false;
    }
    copy_region(dxvk->d3d11_context, staging, 0, 0, 0, 0,
                surface->d3d11_resource, subresource, NULL);
    break;
  }
  case VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
    VMSVGA3DDxvkD3D11Texture3DDesc staging_desc = {0};

    if (desc->mip_levels == 0 || subresource >= desc->mip_levels ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_TEXTURE3D,
            &create_texture3d, sizeof(create_texture3d))) {
      return false;
    }
    mip_level = subresource;
    staging_desc.width = MAX(1u, desc->width >> mip_level);
    staging_desc.height = MAX(1u, desc->height >> mip_level);
    staging_desc.depth = MAX(1u, desc->depth >> mip_level);
    staging_desc.mip_levels = 1;
    staging_desc.format = desc->format;
    staging_desc.usage = VMSVGA3D_DXVK_D3D11_USAGE_STAGING;
    staging_desc.cpu_access_flags = VMSVGA3D_DXVK_D3D11_CPU_ACCESS_READ;
    result = create_texture3d(dxvk->d3d11_device, &staging_desc, NULL,
                              &staging);
    if (!vmsvga3d_dxvk_succeeded(result) || staging == NULL) {
      return false;
    }
    copy_region(dxvk->d3d11_context, staging, 0, 0, 0, 0,
                surface->d3d11_resource, subresource, NULL);
    break;
  }
  default:
    return false;
  }

  result = map(dxvk->d3d11_context, staging, 0,
               VMSVGA3D_DXVK_D3D11_MAP_READ, 0, &mapped);
  if (!vmsvga3d_dxvk_succeeded(result) || mapped.data == NULL) {
    goto out;
  }
  if (desc->resource_dimension ==
      VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_BUFFER) {
    memcpy(data, mapped.data, row_bytes);
  } else {
    if (mapped.row_pitch < row_bytes ||
        (depth_count > 1 && mapped.depth_pitch == 0)) {
      unmap(dxvk->d3d11_context, staging, 0);
      goto out;
    }
    for (z = 0; z < depth_count; z++) {
      const uint8_t *source_plane =
          (const uint8_t *)mapped.data +
          (size_t)z * (depth_count > 1 ? mapped.depth_pitch : 0);
      uint8_t *destination_plane = (uint8_t *)data + (size_t)z * depth_pitch;

      for (y = 0; y < row_count; y++) {
        memcpy(destination_plane + (size_t)y * row_pitch,
               source_plane + (size_t)y * mapped.row_pitch, row_bytes);
      }
    }
  }
  unmap(dxvk->d3d11_context, staging, 0);
  success = true;

out:
  vmsvga3d_dxvk_release(staging, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  return success;
#else
  (void)dxvk;
  (void)surface;
  (void)subresource;
  (void)data;
  (void)row_bytes;
  (void)row_pitch;
  (void)row_count;
  (void)depth_pitch;
  (void)depth_count;
  return false;
#endif
}


#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_d3d11_rtv_desc(
    const VMSVGA3DD3D10RTVDesc *src, VMSVGA3DDxvkD3D11RTVDesc *dst) {
  if (src == NULL || dst == NULL) {
    return false;
  }
  memset(dst, 0, sizeof(*dst));
  dst->format = src->format;
  dst->view_dimension = src->view_dimension;

  switch (src->view_dimension) {
  case VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_BUFFER:
    dst->data[0] = src->first_element;
    dst->data[1] = src->num_elements;
    break;
  case VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE1D:
  case VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE2D:
    dst->data[0] = src->mip_slice;
    break;
  case VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
  case VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
    dst->data[0] = src->mip_slice;
    dst->data[1] = src->first_array_slice;
    dst->data[2] = src->array_size;
    break;
  case VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE2DMS:
    break;
  case VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
    dst->data[0] = src->first_array_slice;
    dst->data[1] = src->array_size;
    break;
  case VMSVGA3D_DXVK_D3D11_RTV_DIMENSION_TEXTURE3D:
    dst->data[0] = src->mip_slice;
    dst->data[1] = src->first_w_slice;
    dst->data[2] = src->w_size;
    break;
  default:
    return false;
  }
  return true;
}
#endif

bool vmsvga3d_dxvk_d3d11_render_target_view_ensure(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_rtv_desc_s *desc) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateRenderTargetView create_view = NULL;
  VMSVGA3DDxvkD3D11RTVDesc native;
  const VMSVGA3DD3D10RTVDesc *view_desc = desc;
  void *view = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      view_id == SVGA3D_INVALID_ID || surface == NULL ||
      !surface->d3d11_resident || surface->d3d11_resource == NULL) {
    return false;
  }
  if (vmsvga3d_dxvk_d3d11_view_find(
          dxvk, VMSVGA3D_DXVK_VIEW_RENDER_TARGET, cid, view_id, NULL) !=
      NULL) {
    return true;
  }
  if (view_desc == NULL ||
      !vmsvga3d_dxvk_d3d11_rtv_desc(view_desc, &native) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_RENDERTARGET_VIEW,
          &create_view, sizeof(create_view))) {
    return false;
  }
  result = create_view(dxvk->d3d11_device, surface->d3d11_resource, &native,
                       &view);
  if (!vmsvga3d_dxvk_succeeded(result) || view == NULL) {
    return false;
  }
  if (!vmsvga3d_dxvk_d3d11_view_store(
          dxvk, VMSVGA3D_DXVK_VIEW_RENDER_TARGET, cid, view_id,
          surface, view)) {
    vmsvga3d_dxvk_release(view, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)view_id;
  (void)surface;
  (void)desc;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_render_target_view_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id) {
  return vmsvga3d_dxvk_d3d11_view_destroy(
      dxvk, VMSVGA3D_DXVK_VIEW_RENDER_TARGET, cid, view_id);
}

bool vmsvga3d_dxvk_d3d11_clear_render_target_view(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    const float color[4]) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11ClearRenderTargetView clear_view = NULL;
  void *view;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      color == NULL) {
    return false;
  }
  view = vmsvga3d_dxvk_d3d11_view_object(
      dxvk, VMSVGA3D_DXVK_VIEW_RENDER_TARGET, cid, view_id);
  if (view == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_CLEAR_RENDERTARGET_VIEW,
          &clear_view, sizeof(clear_view))) {
    return false;
  }
  clear_view(dxvk->d3d11_context, view, color);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)view_id;
  (void)color;
  return false;
#endif
}

#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_d3d11_dsv_desc(
    const VMSVGA3DD3D10DSVDesc *src, VMSVGA3DDxvkD3D11DSVDesc *dst) {
  if (src == NULL || dst == NULL) {
    return false;
  }
  memset(dst, 0, sizeof(*dst));
  dst->format = src->format;
  dst->view_dimension = src->view_dimension;

  switch (src->view_dimension) {
  case VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE1D:
  case VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE2D:
    dst->data[0] = src->mip_slice;
    break;
  case VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
  case VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
    dst->data[0] = src->mip_slice;
    dst->data[1] = src->first_array_slice;
    dst->data[2] = src->array_size;
    break;
  case VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE2DMS:
    break;
  case VMSVGA3D_DXVK_D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
    dst->data[0] = src->first_array_slice;
    dst->data[1] = src->array_size;
    break;
  default:
    return false;
  }
  return true;
}
#endif

bool vmsvga3d_dxvk_d3d11_depth_stencil_view_ensure(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_dsv_desc_s *desc) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateDepthStencilView create_view = NULL;
  VMSVGA3DDxvkD3D11DSVDesc native;
  const VMSVGA3DD3D10DSVDesc *view_desc = desc;
  void *view = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      view_id == SVGA3D_INVALID_ID || surface == NULL ||
      !surface->d3d11_resident || surface->d3d11_resource == NULL) {
    return false;
  }
  if (vmsvga3d_dxvk_d3d11_view_find(
          dxvk, VMSVGA3D_DXVK_VIEW_DEPTH_STENCIL, cid, view_id, NULL) !=
      NULL) {
    return true;
  }
  if (view_desc == NULL ||
      !vmsvga3d_dxvk_d3d11_dsv_desc(view_desc, &native) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_DEPTH_STENCIL_VIEW,
          &create_view, sizeof(create_view))) {
    return false;
  }
  result = create_view(dxvk->d3d11_device, surface->d3d11_resource, &native,
                       &view);
  if (!vmsvga3d_dxvk_succeeded(result) || view == NULL) {
    return false;
  }
  if (!vmsvga3d_dxvk_d3d11_view_store(
          dxvk, VMSVGA3D_DXVK_VIEW_DEPTH_STENCIL, cid, view_id,
          surface, view)) {
    vmsvga3d_dxvk_release(view, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)view_id;
  (void)surface;
  (void)desc;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_depth_stencil_view_destroy(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id) {
  return vmsvga3d_dxvk_d3d11_view_destroy(
      dxvk, VMSVGA3D_DXVK_VIEW_DEPTH_STENCIL, cid, view_id);
}

bool vmsvga3d_dxvk_d3d11_clear_depth_stencil_view(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t view_id,
    uint32_t clear_flags, float depth, uint8_t stencil) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11ClearDepthStencilView clear_view = NULL;
  void *view;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL) {
    return false;
  }
  view = vmsvga3d_dxvk_d3d11_view_object(
      dxvk, VMSVGA3D_DXVK_VIEW_DEPTH_STENCIL, cid, view_id);
  if (view == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_CLEAR_DEPTH_STENCIL_VIEW,
          &clear_view, sizeof(clear_view))) {
    return false;
  }
  clear_view(dxvk->d3d11_context, view, clear_flags, depth, stencil);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)view_id;
  (void)clear_flags;
  (void)depth;
  (void)stencil;
  return false;
#endif
}

#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_surface_level_acquire(
    const VMSVGA3DDxvkSurface *surface, bool bounce, uint32_t level,
    void **d3d_surface) {
  VMSVGA3DDxvkTextureGetSurfaceLevel get_surface_level = NULL;
  void *object;
  int32_t result;

  if (surface == NULL || d3d_surface == NULL || !surface->d3d9_resident) {
    return false;
  }
  *d3d_surface = NULL;
  object = bounce ? surface->d3d9_bounce : surface->d3d9_resource;
  if (object == NULL) {
    return false;
  }
  if (surface->d3d9_resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE) {
    if (!vmsvga3d_dxvk_get_method(
            object, VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_GET_SURFACE_LEVEL,
            &get_surface_level, sizeof(get_surface_level))) {
      return false;
    }
    result = get_surface_level(object, level, d3d_surface);
    return vmsvga3d_dxvk_succeeded(result) && *d3d_surface != NULL;
  }
  if (surface->d3d9_resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE &&
      level == 0 &&
      vmsvga3d_dxvk_addref(object, VMSVGA3D_DXVK_IUNKNOWN_ADDREF)) {
    *d3d_surface = object;
    return true;
  }
  return false;
}
#endif

bool vmsvga3d_dxvk_surface_upload_level(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t level,
    const void *data, uint32_t row_bytes, uint32_t rows) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSurfaceLockRect lock_rect = NULL;
  VMSVGA3DDxvkSurfaceUnlockRect unlock_rect = NULL;
  VMSVGA3DDxvkUpdateSurface update_surface = NULL;
  VMSVGA3DDxvkLockedRect locked = { 0 };
  void *source_surface = NULL;
  void *destination_surface = NULL;
  const uint8_t *source = data;
  uint8_t *destination;
  uint32_t y;
  int32_t result;
  bool success = false;

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || !surface->d3d9_resident ||
      !surface->d3d9_has_bounce || surface->d3d9_bounce == NULL || data == NULL ||
      row_bytes == 0 || rows == 0 ||
      !vmsvga3d_dxvk_surface_level_acquire(surface, true, level,
                                           &source_surface) ||
      !vmsvga3d_dxvk_surface_level_acquire(surface, false, level,
                                           &destination_surface) ||
      !vmsvga3d_dxvk_get_method(
          source_surface, VMSVGA3D_DXVK_IDIRECT3DSURFACE9_LOCK_RECT,
          &lock_rect, sizeof(lock_rect)) ||
      !vmsvga3d_dxvk_get_method(
          source_surface, VMSVGA3D_DXVK_IDIRECT3DSURFACE9_UNLOCK_RECT,
          &unlock_rect, sizeof(unlock_rect)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_UPDATE_SURFACE,
          &update_surface, sizeof(update_surface))) {
    goto out;
  }

  result = lock_rect(source_surface, &locked, NULL, 0);
  if (!vmsvga3d_dxvk_succeeded(result) || locked.bits == NULL ||
      locked.pitch < 0 || (uint32_t)locked.pitch < row_bytes) {
    if (vmsvga3d_dxvk_succeeded(result)) {
      unlock_rect(source_surface);
    }
    goto out;
  }
  destination = locked.bits;
  for (y = 0; y < rows; y++) {
    memcpy(destination + (size_t)y * (uint32_t)locked.pitch,
           source + (size_t)y * row_bytes, row_bytes);
  }
  result = unlock_rect(source_surface);
  if (!vmsvga3d_dxvk_succeeded(result)) {
    goto out;
  }
  result = update_surface(dxvk->d3d9_device, source_surface, NULL,
                          destination_surface, NULL);
  success = vmsvga3d_dxvk_succeeded(result);

out:
  if (destination_surface != NULL) {
    vmsvga3d_dxvk_release(destination_surface,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  if (source_surface != NULL) {
    vmsvga3d_dxvk_release(source_surface,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  return success;
#else
  (void)dxvk;
  (void)surface;
  (void)level;
  (void)data;
  (void)row_bytes;
  (void)rows;
  return false;
#endif
}

bool vmsvga3d_dxvk_surface_readback_level(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t level,
    void *data, uint32_t row_bytes, uint32_t rows) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkGetRenderTargetData get_render_target_data = NULL;
  VMSVGA3DDxvkSurfaceLockRect lock_rect = NULL;
  VMSVGA3DDxvkSurfaceUnlockRect unlock_rect = NULL;
  VMSVGA3DDxvkLockedRect locked = { 0 };
  void *source_surface = NULL;
  void *bounce_surface = NULL;
  void *lock_surface = NULL;
  const uint8_t *source;
  uint8_t *destination = data;
  uint32_t y;
  int32_t result;
  bool success = false;

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || !surface->d3d9_resident ||
      surface->d3d9_resource == NULL || data == NULL || row_bytes == 0 ||
      rows == 0 ||
      !vmsvga3d_dxvk_surface_level_acquire(surface, false, level,
                                           &source_surface)) {
    goto out;
  }

  if ((surface->d3d9_usage & VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET) != 0 ||
      surface->d3d9_resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE) {
    if (!surface->d3d9_has_bounce || surface->d3d9_bounce == NULL ||
        !vmsvga3d_dxvk_surface_level_acquire(surface, true, level,
                                             &bounce_surface) ||
        !vmsvga3d_dxvk_get_method(
            dxvk->d3d9_device,
            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_RENDER_TARGET_DATA,
            &get_render_target_data, sizeof(get_render_target_data))) {
      goto out;
    }
    result = get_render_target_data(dxvk->d3d9_device, source_surface,
                                    bounce_surface);
    if (!vmsvga3d_dxvk_succeeded(result)) {
      goto out;
    }
    lock_surface = bounce_surface;
  } else {
    lock_surface = source_surface;
  }

  if (!vmsvga3d_dxvk_get_method(
          lock_surface, VMSVGA3D_DXVK_IDIRECT3DSURFACE9_LOCK_RECT,
          &lock_rect, sizeof(lock_rect)) ||
      !vmsvga3d_dxvk_get_method(
          lock_surface, VMSVGA3D_DXVK_IDIRECT3DSURFACE9_UNLOCK_RECT,
          &unlock_rect, sizeof(unlock_rect))) {
    goto out;
  }
  result = lock_rect(lock_surface, &locked, NULL,
                     VMSVGA3D_DXVK_D3DLOCK_READONLY);
  if (!vmsvga3d_dxvk_succeeded(result) || locked.bits == NULL ||
      locked.pitch < 0 || (uint32_t)locked.pitch < row_bytes) {
    if (vmsvga3d_dxvk_succeeded(result)) {
      unlock_rect(lock_surface);
    }
    goto out;
  }
  source = locked.bits;
  for (y = 0; y < rows; y++) {
    memcpy(destination + (size_t)y * row_bytes,
           source + (size_t)y * (uint32_t)locked.pitch, row_bytes);
  }
  result = unlock_rect(lock_surface);
  success = vmsvga3d_dxvk_succeeded(result);

out:
  if (bounce_surface != NULL) {
    vmsvga3d_dxvk_release(bounce_surface,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  if (source_surface != NULL) {
    vmsvga3d_dxvk_release(source_surface,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  return success;
#else
  (void)dxvk;
  (void)surface;
  (void)level;
  (void)data;
  (void)row_bytes;
  (void)rows;
  return false;
#endif
}

bool vmsvga3d_dxvk_surface_upload_buffer(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const void *data, uint32_t size) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkBufferLock lock = NULL;
  VMSVGA3DDxvkBufferUnlock unlock = NULL;
  void *destination = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || !surface->d3d9_resident ||
      surface->d3d9_resource == NULL || data == NULL || size == 0 ||
      size > surface->d3d9_length ||
      (surface->d3d9_resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER &&
       surface->d3d9_resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER) ||
      !vmsvga3d_dxvk_get_method(
          surface->d3d9_resource, VMSVGA3D_DXVK_IDIRECT3DBUFFER9_LOCK,
          &lock, sizeof(lock)) ||
      !vmsvga3d_dxvk_get_method(
          surface->d3d9_resource, VMSVGA3D_DXVK_IDIRECT3DBUFFER9_UNLOCK,
          &unlock, sizeof(unlock))) {
    return false;
  }
  result = lock(surface->d3d9_resource, 0, size, &destination,
                VMSVGA3D_DXVK_D3DLOCK_DISCARD);
  if (!vmsvga3d_dxvk_succeeded(result) || destination == NULL) {
    return false;
  }
  memcpy(destination, data, size);
  result = unlock(surface->d3d9_resource);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)surface;
  (void)data;
  (void)size;
  return false;
#endif
}

bool vmsvga3d_dxvk_surface_stretch_rect(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *source,
    uint32_t source_level, const struct vmsvga3d_d3d9_rect_s *source_rect,
    VMSVGA3DDxvkSurface *destination, uint32_t destination_level,
    const struct vmsvga3d_d3d9_rect_s *destination_rect, uint32_t filter) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkStretchRect stretch_rect = NULL;
  void *source_surface = NULL;
  void *destination_surface = NULL;
  int32_t result;
  bool success = false;

  if (!vmsvga3d_dxvk_ready(dxvk) || source == NULL || destination == NULL ||
      !source->d3d9_resident || !destination->d3d9_resident ||
      !vmsvga3d_dxvk_surface_level_acquire(source, false, source_level,
                                           &source_surface) ||
      !vmsvga3d_dxvk_surface_level_acquire(destination, false,
                                           destination_level,
                                           &destination_surface) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_STRETCH_RECT,
          &stretch_rect, sizeof(stretch_rect))) {
    goto out;
  }
  result = stretch_rect(dxvk->d3d9_device, source_surface, source_rect,
                        destination_surface, destination_rect, filter);
  success = vmsvga3d_dxvk_succeeded(result);

out:
  if (destination_surface != NULL) {
    vmsvga3d_dxvk_release(destination_surface,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  if (source_surface != NULL) {
    vmsvga3d_dxvk_release(source_surface,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  return success;
#else
  (void)dxvk;
  (void)source;
  (void)source_level;
  (void)source_rect;
  (void)destination;
  (void)destination_level;
  (void)destination_rect;
  (void)filter;
  return false;
#endif
}

bool vmsvga3d_dxvk_clear(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *const color_targets[8],
    const uint32_t color_levels[8], VMSVGA3DDxvkSurface *depth_stencil,
    uint32_t depth_stencil_level,
    const struct vmsvga3d_d3d9_rect_s *rects, uint32_t rect_count,
    const struct vmsvga3d_d3d9_rect_s *clear_scissor, uint32_t flags,
    uint32_t color, float depth, uint32_t stencil) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetRenderTarget set_render_target = NULL;
  VMSVGA3DDxvkGetRenderTarget get_render_target = NULL;
  VMSVGA3DDxvkSetDepthStencilSurface set_depth_stencil = NULL;
  VMSVGA3DDxvkGetDepthStencilSurface get_depth_stencil = NULL;
  VMSVGA3DDxvkClear clear = NULL;
  VMSVGA3DDxvkSetScissorRect set_scissor = NULL;
  VMSVGA3DDxvkGetScissorRect get_scissor = NULL;
  void *saved_targets[8] = { 0 };
  void *bound_targets[8] = { 0 };
  void *saved_depth_stencil = NULL;
  void *bound_depth_stencil = NULL;
  VMSVGA3DD3D9Rect saved_scissor;
  uint32_t highest_target = 0;
  uint32_t i;
  int32_t result;
  bool have_saved_depth = false;
  bool have_saved_scissor = false;
  bool success = false;

  if (!vmsvga3d_dxvk_ready(dxvk) || color_targets == NULL ||
      color_levels == NULL || color_targets[0] == NULL ||
      (rect_count != 0 && rects == NULL) ||
      clear_scissor == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_RENDER_TARGET,
          &set_render_target, sizeof(set_render_target)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_RENDER_TARGET,
          &get_render_target, sizeof(get_render_target)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_DEPTH_STENCIL_SURFACE,
          &set_depth_stencil, sizeof(set_depth_stencil)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_DEPTH_STENCIL_SURFACE,
          &get_depth_stencil, sizeof(get_depth_stencil)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CLEAR,
          &clear, sizeof(clear)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_SCISSOR_RECT,
          &set_scissor, sizeof(set_scissor)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_SCISSOR_RECT,
          &get_scissor, sizeof(get_scissor))) {
    return false;
  }

  for (i = 0; i < 8; i++) {
    if (color_targets[i] != NULL) {
      highest_target = i;
    }
  }
  for (i = 0; i <= highest_target; i++) {
    result = get_render_target(dxvk->d3d9_device, i, &saved_targets[i]);
    if (!vmsvga3d_dxvk_succeeded(result)) {
      saved_targets[i] = NULL;
      if (i == 0) {
        goto restore;
      }
    }
    if (color_targets[i] != NULL &&
        !vmsvga3d_dxvk_surface_level_acquire(
            color_targets[i], false, color_levels[i], &bound_targets[i])) {
      goto restore;
    }
  }

  result = get_depth_stencil(dxvk->d3d9_device, &saved_depth_stencil);
  have_saved_depth = vmsvga3d_dxvk_succeeded(result);
  if (!have_saved_depth) {
    saved_depth_stencil = NULL;
  }
  if (depth_stencil != NULL &&
      !vmsvga3d_dxvk_surface_level_acquire(
          depth_stencil, false, depth_stencil_level,
          &bound_depth_stencil)) {
    goto restore;
  }

  result = get_scissor(dxvk->d3d9_device, &saved_scissor);
  if (!vmsvga3d_dxvk_succeeded(result)) {
    goto restore;
  }
  have_saved_scissor = true;

  for (i = 0; i <= highest_target; i++) {
    result = set_render_target(dxvk->d3d9_device, i, bound_targets[i]);
    if (!vmsvga3d_dxvk_succeeded(result)) {
      goto restore;
    }
  }
  result = set_depth_stencil(dxvk->d3d9_device, bound_depth_stencil);
  if (!vmsvga3d_dxvk_succeeded(result)) {
    goto restore;
  }
  result = set_scissor(dxvk->d3d9_device, clear_scissor);
  if (!vmsvga3d_dxvk_succeeded(result)) {
    goto restore;
  }
  result = clear(dxvk->d3d9_device, rect_count, rects, flags, color, depth,
                 stencil);
  success = vmsvga3d_dxvk_succeeded(result);

restore:
  if (have_saved_scissor) {
    set_scissor(dxvk->d3d9_device, &saved_scissor);
  }
  for (i = 0; i <= highest_target; i++) {
    set_render_target(dxvk->d3d9_device, i, saved_targets[i]);
  }
  if (have_saved_depth) {
    set_depth_stencil(dxvk->d3d9_device, saved_depth_stencil);
  } else {
    set_depth_stencil(dxvk->d3d9_device, NULL);
  }
  for (i = 0; i < 8; i++) {
    if (bound_targets[i] != NULL) {
      vmsvga3d_dxvk_release(bound_targets[i],
                            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    }
    if (saved_targets[i] != NULL) {
      vmsvga3d_dxvk_release(saved_targets[i],
                            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    }
  }
  if (bound_depth_stencil != NULL) {
    vmsvga3d_dxvk_release(bound_depth_stencil,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  if (saved_depth_stencil != NULL) {
    vmsvga3d_dxvk_release(saved_depth_stencil,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  return success;
#else
  (void)dxvk;
  (void)color_targets;
  (void)color_levels;
  (void)depth_stencil;
  (void)depth_stencil_level;
  (void)rects;
  (void)rect_count;
  (void)clear_scissor;
  (void)flags;
  (void)color;
  (void)depth;
  (void)stencil;
  return false;
#endif
}

bool vmsvga3d_dxvk_reset_state(VMSVGA3DDxvk *dxvk) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkStateBlockApply apply = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d9_pristine_state == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_pristine_state, VMSVGA3D_DXVK_IDIRECT3DSTATEBLOCK9_APPLY,
          &apply, sizeof(apply))) {
    return false;
  }
  result = apply(dxvk->d3d9_pristine_state);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_render_target(VMSVGA3DDxvk *dxvk, uint32_t index,
                                     VMSVGA3DDxvkSurface *surface,
                                     uint32_t level) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetRenderTarget set_render_target = NULL;
  void *d3d_surface = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_RENDER_TARGET,
          &set_render_target, sizeof(set_render_target))) {
    return false;
  }
  if (surface != NULL &&
      !vmsvga3d_dxvk_surface_level_acquire(surface, false, level,
                                           &d3d_surface)) {
    return false;
  }
  result = set_render_target(dxvk->d3d9_device, index, d3d_surface);
  if (d3d_surface != NULL) {
    vmsvga3d_dxvk_release(d3d_surface,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)index;
  (void)surface;
  (void)level;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_depth_stencil(VMSVGA3DDxvk *dxvk,
                                     VMSVGA3DDxvkSurface *surface,
                                     uint32_t level) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetDepthStencilSurface set_depth_stencil = NULL;
  void *d3d_surface = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_DEPTH_STENCIL_SURFACE,
          &set_depth_stencil, sizeof(set_depth_stencil))) {
    return false;
  }
  if (surface != NULL &&
      !vmsvga3d_dxvk_surface_level_acquire(surface, false, level,
                                           &d3d_surface)) {
    return false;
  }
  result = set_depth_stencil(dxvk->d3d9_device, d3d_surface);
  if (d3d_surface != NULL) {
    vmsvga3d_dxvk_release(d3d_surface,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
  }
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)surface;
  (void)level;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_render_state(VMSVGA3DDxvk *dxvk, uint32_t state,
                                    uint32_t value) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetRenderState set_state = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_RENDER_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  result = set_state(dxvk->d3d9_device, state, value);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)state;
  (void)value;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_texture(VMSVGA3DDxvk *dxvk, uint32_t stage,
                               VMSVGA3DDxvkSurface *surface) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetTexture set_texture = NULL;
  void *texture = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_TEXTURE,
          &set_texture, sizeof(set_texture))) {
    return false;
  }
  if (surface != NULL) {
    if (!surface->d3d9_resident || surface->d3d9_resource == NULL ||
        surface->d3d9_resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE) {
      return false;
    }
    texture = surface->d3d9_resource;
  }
  result = set_texture(dxvk->d3d9_device, stage, texture);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)stage;
  (void)surface;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_texture_stage_state(VMSVGA3DDxvk *dxvk,
                                           uint32_t stage, uint32_t state,
                                           uint32_t value) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetTextureStageState set_state = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_TEXTURE_STAGE_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  result = set_state(dxvk->d3d9_device, stage, state, value);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)stage;
  (void)state;
  (void)value;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_sampler_state(VMSVGA3DDxvk *dxvk, uint32_t sampler,
                                     uint32_t state, uint32_t value) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetSamplerState set_state = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_SAMPLER_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  result = set_state(dxvk->d3d9_device, sampler, state, value);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)sampler;
  (void)state;
  (void)value;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_transform(VMSVGA3DDxvk *dxvk, uint32_t type,
                                 const float matrix[16]) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetTransform set_transform = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || matrix == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_TRANSFORM,
          &set_transform, sizeof(set_transform))) {
    return false;
  }
  result = set_transform(dxvk->d3d9_device, type, matrix);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)type;
  (void)matrix;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_viewport(
    VMSVGA3DDxvk *dxvk, const struct vmsvga3d_d3d9_viewport_s *viewport) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetViewport set_viewport = NULL;
  int32_t result;

  QEMU_BUILD_BUG_ON(sizeof(VMSVGA3DD3D9Viewport) != 24);
  if (!vmsvga3d_dxvk_ready(dxvk) || viewport == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VIEWPORT,
          &set_viewport, sizeof(set_viewport))) {
    return false;
  }
  result = set_viewport(dxvk->d3d9_device, viewport);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)viewport;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_scissor(
    VMSVGA3DDxvk *dxvk, const struct vmsvga3d_d3d9_rect_s *rect) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetScissorRect set_scissor = NULL;
  int32_t result;

  QEMU_BUILD_BUG_ON(sizeof(VMSVGA3DD3D9Rect) != 16);
  if (!vmsvga3d_dxvk_ready(dxvk) || rect == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_SCISSOR_RECT,
          &set_scissor, sizeof(set_scissor))) {
    return false;
  }
  result = set_scissor(dxvk->d3d9_device, rect);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)rect;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_material(
    VMSVGA3DDxvk *dxvk, const struct vmsvga3d_d3d9_material_s *material) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetMaterial set_material = NULL;
  int32_t result;

  QEMU_BUILD_BUG_ON(sizeof(VMSVGA3DD3D9Material) != 68);
  if (!vmsvga3d_dxvk_ready(dxvk) || material == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_MATERIAL,
          &set_material, sizeof(set_material))) {
    return false;
  }
  result = set_material(dxvk->d3d9_device, material);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)material;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_light(
    VMSVGA3DDxvk *dxvk, uint32_t index,
    const struct vmsvga3d_d3d9_light_s *light) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetLight set_light = NULL;
  int32_t result;

  QEMU_BUILD_BUG_ON(sizeof(VMSVGA3DD3D9Light) != 104);
  if (!vmsvga3d_dxvk_ready(dxvk) || light == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_LIGHT,
          &set_light, sizeof(set_light))) {
    return false;
  }
  result = set_light(dxvk->d3d9_device, index, light);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)index;
  (void)light;
  return false;
#endif
}

bool vmsvga3d_dxvk_light_enable(VMSVGA3DDxvk *dxvk, uint32_t index,
                                bool enabled) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkLightEnable light_enable = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_LIGHT_ENABLE,
          &light_enable, sizeof(light_enable))) {
    return false;
  }
  result = light_enable(dxvk->d3d9_device, index, enabled ? 1 : 0);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)index;
  (void)enabled;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_clip_plane(VMSVGA3DDxvk *dxvk, uint32_t index,
                                  const float plane[4]) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetClipPlane set_clip_plane = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || plane == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_CLIP_PLANE,
          &set_clip_plane, sizeof(set_clip_plane))) {
    return false;
  }
  result = set_clip_plane(dxvk->d3d9_device, index, plane);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)index;
  (void)plane;
  return false;
#endif
}

void *vmsvga3d_dxvk_shader_create(VMSVGA3DDxvk *dxvk, uint32_t stage,
                                  const uint32_t *bytecode) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkCreateShader create_shader = NULL;
  uint32_t method;
  void *shader = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || bytecode == NULL) {
    return NULL;
  }
  if (stage == VMSVGA3D_D3D9_SHADER_STAGE_VERTEX) {
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_VERTEX_SHADER;
  } else if (stage == VMSVGA3D_D3D9_SHADER_STAGE_PIXEL) {
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_PIXEL_SHADER;
  } else {
    return NULL;
  }
  if (!vmsvga3d_dxvk_get_method(dxvk->d3d9_device, method, &create_shader,
                                 sizeof(create_shader))) {
    return NULL;
  }
  result = create_shader(dxvk->d3d9_device, bytecode, &shader);
  return vmsvga3d_dxvk_succeeded(result) ? shader : NULL;
#else
  (void)dxvk;
  (void)stage;
  (void)bytecode;
  return NULL;
#endif
}

void vmsvga3d_dxvk_shader_destroy(void *shader) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  vmsvga3d_dxvk_release(shader, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
#else
  (void)shader;
#endif
}

bool vmsvga3d_dxvk_shader_bind(VMSVGA3DDxvk *dxvk, uint32_t stage,
                               void *shader) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetShader set_shader = NULL;
  uint32_t method;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk)) {
    return false;
  }
  if (stage == VMSVGA3D_D3D9_SHADER_STAGE_VERTEX) {
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_SHADER;
  } else if (stage == VMSVGA3D_D3D9_SHADER_STAGE_PIXEL) {
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_PIXEL_SHADER;
  } else {
    return false;
  }
  if (!vmsvga3d_dxvk_get_method(dxvk->d3d9_device, method, &set_shader,
                                 sizeof(set_shader))) {
    return false;
  }
  result = set_shader(dxvk->d3d9_device, shader);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)stage;
  (void)shader;
  return false;
#endif
}

bool vmsvga3d_dxvk_shader_constant(VMSVGA3DDxvk *dxvk, uint32_t target,
                                   uint32_t reg, const uint32_t values[4]) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkComFunction entry;
  int32_t result;
  uint32_t method;
  int32_t boolean_value;
  int32_t int_values[4];
  float float_values[4];

  if (!vmsvga3d_dxvk_ready(dxvk) || values == NULL) {
    return false;
  }
  switch (target) {
  case VMSVGA3D_D3D9_CONST_TARGET_VS_FLOAT:
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_SHADER_CONSTANT_F;
    break;
  case VMSVGA3D_D3D9_CONST_TARGET_VS_INT:
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_SHADER_CONSTANT_I;
    break;
  case VMSVGA3D_D3D9_CONST_TARGET_VS_BOOL:
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_SHADER_CONSTANT_B;
    break;
  case VMSVGA3D_D3D9_CONST_TARGET_PS_FLOAT:
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_PIXEL_SHADER_CONSTANT_F;
    break;
  case VMSVGA3D_D3D9_CONST_TARGET_PS_INT:
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_PIXEL_SHADER_CONSTANT_I;
    break;
  case VMSVGA3D_D3D9_CONST_TARGET_PS_BOOL:
    method = VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_PIXEL_SHADER_CONSTANT_B;
    break;
  default:
    return false;
  }
  entry = vmsvga3d_dxvk_vtable_entry(dxvk->d3d9_device, method);
  if (entry == NULL) {
    return false;
  }
  if (target == VMSVGA3D_D3D9_CONST_TARGET_VS_BOOL ||
      target == VMSVGA3D_D3D9_CONST_TARGET_PS_BOOL) {
    VMSVGA3DDxvkSetShaderConstantB set_constant = NULL;
    memcpy(&set_constant, &entry, sizeof(set_constant));
    boolean_value = values[0] != 0;
    result = set_constant(dxvk->d3d9_device, reg, &boolean_value, 1);
  } else if (target == VMSVGA3D_D3D9_CONST_TARGET_VS_INT ||
             target == VMSVGA3D_D3D9_CONST_TARGET_PS_INT) {
    VMSVGA3DDxvkSetShaderConstantI set_constant = NULL;
    memcpy(&set_constant, &entry, sizeof(set_constant));
    memcpy(int_values, values, sizeof(int_values));
    result = set_constant(dxvk->d3d9_device, reg, int_values, 1);
  } else {
    VMSVGA3DDxvkSetShaderConstantF set_constant = NULL;
    memcpy(&set_constant, &entry, sizeof(set_constant));
    memcpy(float_values, values, sizeof(float_values));
    result = set_constant(dxvk->d3d9_device, reg, float_values, 1);
  }
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)target;
  (void)reg;
  (void)values;
  return false;
#endif
}

void *vmsvga3d_dxvk_vertex_declaration_create(
    VMSVGA3DDxvk *dxvk,
    const struct vmsvga3d_d3d9_vertex_element_s *elements) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkCreateVertexDeclaration create_declaration = NULL;
  void *declaration = NULL;
  int32_t result;

  QEMU_BUILD_BUG_ON(sizeof(VMSVGA3DD3D9VertexElement) != 8);
  if (!vmsvga3d_dxvk_ready(dxvk) || elements == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_VERTEX_DECLARATION,
          &create_declaration, sizeof(create_declaration))) {
    return NULL;
  }
  result = create_declaration(dxvk->d3d9_device, elements, &declaration);
  return vmsvga3d_dxvk_succeeded(result) ? declaration : NULL;
#else
  (void)dxvk;
  (void)elements;
  return NULL;
#endif
}

void vmsvga3d_dxvk_vertex_declaration_destroy(void *declaration) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  vmsvga3d_dxvk_release(declaration,
                        VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
#else
  (void)declaration;
#endif
}

bool vmsvga3d_dxvk_vertex_declaration_bind(VMSVGA3DDxvk *dxvk,
                                           void *declaration) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetVertexDeclaration set_declaration = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_DECLARATION,
          &set_declaration, sizeof(set_declaration))) {
    return false;
  }
  result = set_declaration(dxvk->d3d9_device, declaration);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)declaration;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_stream_source(VMSVGA3DDxvk *dxvk, uint32_t stream,
                                     VMSVGA3DDxvkSurface *surface,
                                     uint32_t offset, uint32_t stride) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetStreamSource set_stream = NULL;
  void *buffer = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_STREAM_SOURCE,
          &set_stream, sizeof(set_stream))) {
    return false;
  }
  if (surface != NULL) {
    if (!surface->d3d9_resident || surface->d3d9_resource == NULL ||
        surface->d3d9_resource_type !=
            VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER) {
      return false;
    }
    buffer = surface->d3d9_resource;
  }
  result = set_stream(dxvk->d3d9_device, stream, buffer, offset, stride);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)stream;
  (void)surface;
  (void)offset;
  (void)stride;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_stream_frequency(VMSVGA3DDxvk *dxvk,
                                        uint32_t stream,
                                        uint32_t frequency) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetStreamSourceFreq set_frequency = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_STREAM_SOURCE_FREQ,
          &set_frequency, sizeof(set_frequency))) {
    return false;
  }
  result = set_frequency(dxvk->d3d9_device, stream, frequency);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)stream;
  (void)frequency;
  return false;
#endif
}

bool vmsvga3d_dxvk_set_indices(VMSVGA3DDxvk *dxvk,
                               VMSVGA3DDxvkSurface *surface) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkSetIndices set_indices = NULL;
  void *buffer = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_INDICES,
          &set_indices, sizeof(set_indices))) {
    return false;
  }
  if (surface != NULL) {
    if (!surface->d3d9_resident || surface->d3d9_resource == NULL ||
        surface->d3d9_resource_type !=
            VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER) {
      return false;
    }
    buffer = surface->d3d9_resource;
  }
  result = set_indices(dxvk->d3d9_device, buffer);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)surface;
  return false;
#endif
}

bool vmsvga3d_dxvk_begin_scene(VMSVGA3DDxvk *dxvk) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkBeginScene begin_scene = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_BEGIN_SCENE,
          &begin_scene, sizeof(begin_scene))) {
    return false;
  }
  result = begin_scene(dxvk->d3d9_device);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  return false;
#endif
}

bool vmsvga3d_dxvk_end_scene(VMSVGA3DDxvk *dxvk) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkEndScene end_scene = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_END_SCENE,
          &end_scene, sizeof(end_scene))) {
    return false;
  }
  result = end_scene(dxvk->d3d9_device);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  return false;
#endif
}

bool vmsvga3d_dxvk_draw_primitive(VMSVGA3DDxvk *dxvk,
                                  uint32_t primitive_type,
                                  uint32_t start_vertex,
                                  uint32_t primitive_count) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkDrawPrimitive draw = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_DRAW_PRIMITIVE,
          &draw, sizeof(draw))) {
    return false;
  }
  result = draw(dxvk->d3d9_device, primitive_type, start_vertex, primitive_count);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)primitive_type;
  (void)start_vertex;
  (void)primitive_count;
  return false;
#endif
}

bool vmsvga3d_dxvk_draw_indexed_primitive(
    VMSVGA3DDxvk *dxvk, uint32_t primitive_type, int32_t base_vertex_index,
    uint32_t min_vertex_index, uint32_t num_vertices, uint32_t start_index,
    uint32_t primitive_count) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkDrawIndexedPrimitive draw = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d9_device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_DRAW_INDEXED_PRIMITIVE,
          &draw, sizeof(draw))) {
    return false;
  }
  result = draw(dxvk->d3d9_device, primitive_type, base_vertex_index,
                min_vertex_index, num_vertices, start_index,
                primitive_count);
  return vmsvga3d_dxvk_succeeded(result);
#else
  (void)dxvk;
  (void)primitive_type;
  (void)base_vertex_index;
  (void)min_vertex_index;
  (void)num_vertices;
  (void)start_index;
  (void)primitive_count;
  return false;
#endif
}
