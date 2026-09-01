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

typedef struct vmsvga3d_dxvk_query_s VMSVGA3DDxvkQuery;
typedef struct vmsvga3d_dxvk_state_s VMSVGA3DDxvkState;
typedef struct vmsvga3d_dxvk_shader_s VMSVGA3DDxvkShader;
typedef struct vmsvga3d_dxvk_input_layout_s VMSVGA3DDxvkInputLayout;

struct vmsvga3d_dxvk_s {
  VMSVGA3DDxvkWsi *wsi;
  void *d3d9_library;
  void *d3d11_library;
  void *d3d9;
  void *d3d9_device;
  void *d3d9_pristine_state;
  void *d3d11_device;
  void *d3d11_context;
  VMSVGA3DDxvkQuery *d3d11_queries;
  VMSVGA3DDxvkState *d3d11_states;
  VMSVGA3DDxvkShader *d3d11_shaders;
  VMSVGA3DDxvkInputLayout *d3d11_input_layouts;
  bool ready;
};

typedef struct vmsvga3d_dxvk_srv_s {
  VMSVGA3DD3D10SRVDesc desc;
  void *view;
  struct vmsvga3d_dxvk_srv_s *next;
} VMSVGA3DDxvkSRV;

struct vmsvga3d_dxvk_query_s {
  uint32_t cid;
  uint32_t query_id;
  uint32_t d3d_query;
  uint32_t misc_flags;
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

/* Keep DXBC alive with the native shader for input-layout and SO creation. */
struct vmsvga3d_dxvk_shader_s {
  uint32_t cid;
  uint32_t shader_id;
  uint32_t shader_type;
  void *shader;
  uint8_t *bytecode;
  uint32_t bytecode_size;
  VMSVGA3DDxvkShader *next;
};

struct vmsvga3d_dxvk_input_layout_s {
  uint32_t cid;
  uint32_t layout_id;
  uint32_t shader_id;
  void *layout;
  VMSVGA3DDxvkInputLayout *next;
};

/*
 * Renderer-side lifetime object for one guest SVGA3D surface. D3D9 and D3D11
 * residency are deliberately independent; crossing generations without an
 * explicit synchronization path is rejected instead of discarding GPU data.
 */
struct vmsvga3d_dxvk_surface_s {
  uint32_t sid;

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
  VMSVGA3DDxvkSRV *d3d11_srvs;
  bool d3d11_resident;
};

#if defined(CONFIG_LINUX) && defined(__ELF__)

#include <dlfcn.h>
#include <sched.h>

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
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_PIXEL_SHADER 15u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_BLEND_STATE 20u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_DEPTH_STENCIL_STATE 21u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_RASTERIZER_STATE 22u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_SAMPLER_STATE 23u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_QUERY 24u
#define VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_PREDICATE 25u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_PS_SET_SAMPLERS 10u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_IA_SET_PRIMITIVE_TOPOLOGY 24u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_VS_SET_SAMPLERS 26u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_BEGIN 27u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_END 28u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GET_DATA 29u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_SET_PREDICATION 30u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GS_SET_SAMPLERS 32u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_BLEND_STATE 35u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_DEPTH_STENCIL_STATE 36u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_STATE 43u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_VIEWPORTS 44u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_RS_SET_SCISSOR_RECTS 45u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_COPY_SUBRESOURCE_REGION 46u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_COPY_RESOURCE 47u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_UPDATE_SUBRESOURCE 48u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_CLEAR_RENDERTARGET_VIEW 50u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_CLEAR_DEPTH_STENCIL_VIEW 53u
#define VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GENERATE_MIPS 54u
#define VMSVGA3D_DXVK_D3D11_RESOURCE_DIMENSION_BUFFER 1u
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
typedef int32_t (*VMSVGA3DDxvkBufferLock)(void *buffer, uint32_t offset,
                                          uint32_t size, void **data,
                                          uint32_t flags);
typedef int32_t (*VMSVGA3DDxvkBufferUnlock)(void *buffer);
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
typedef void (*VMSVGA3DDxvkD3D11Begin)(void *context, void *query);
typedef void (*VMSVGA3DDxvkD3D11End)(void *context, void *query);
typedef void (*VMSVGA3DDxvkD3D11SetPredication)(
    void *context, void *predicate, int32_t predicate_value);
typedef void (*VMSVGA3DDxvkD3D11SetSamplers)(
    void *context, uint32_t start_slot, uint32_t sampler_count,
    void *const *samplers);
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
    g_free(shader->bytecode);
    g_free(shader);
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
#if defined(CONFIG_LINUX) && defined(__ELF__)
  while (surface->d3d11_srvs != NULL) {
    VMSVGA3DDxvkSRV *srv = surface->d3d11_srvs;
    surface->d3d11_srvs = srv->next;
    if (srv->view != NULL) {
      vmsvga3d_dxvk_release(srv->view, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    g_free(srv);
  }
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
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)plan;
  return false;
#endif
}

#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_d3d11_desc_equal(
    const VMSVGA3DD3D10CreateDesc *a,
    const VMSVGA3DD3D10CreateDesc *b) {
  return a != NULL && b != NULL && a->valid == b->valid &&
         a->resource_dimension == b->resource_dimension &&
         a->byte_width == b->byte_width && a->width == b->width &&
         a->height == b->height && a->depth == b->depth &&
         a->mip_levels == b->mip_levels && a->array_size == b->array_size &&
         a->format == b->format && a->sample_count == b->sample_count &&
         a->sample_quality == b->sample_quality && a->usage == b->usage &&
         a->bind_flags == b->bind_flags &&
         a->cpu_access_flags == b->cpu_access_flags &&
         a->misc_flags == b->misc_flags;
}
#endif

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
      surface == NULL || resource_desc == NULL || !resource_desc->valid ||
      surface->d3d9_resident ||
      initial_data_count != resource_desc->initial_subresource_count ||
      (initial_data_count != 0 && initial_data == NULL)) {
    return false;
  }
  for (i = 0; i < initial_data_count; i++) {
    if (initial_data[i].data == NULL) {
      return false;
    }
  }
  if (surface->d3d11_resident) {
    return surface->d3d11_resource != NULL &&
           vmsvga3d_dxvk_d3d11_desc_equal(&surface->d3d11_desc,
                                           resource_desc);
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

static bool vmsvga3d_dxvk_d3d11_srv_desc_equal(
    const VMSVGA3DD3D10SRVDesc *a, const VMSVGA3DD3D10SRVDesc *b) {
  return a != NULL && b != NULL && a->format == b->format &&
         a->view_dimension == b->view_dimension &&
         a->most_detailed_mip == b->most_detailed_mip &&
         a->mip_levels == b->mip_levels &&
         a->first_array_slice == b->first_array_slice &&
         a->array_size == b->array_size &&
         a->first_element == b->first_element &&
         a->num_elements == b->num_elements;
}

static const VMSVGA3DDxvkSRV *vmsvga3d_dxvk_d3d11_srv_find(
    const VMSVGA3DDxvkSurface *surface,
    const VMSVGA3DD3D10SRVDesc *view_desc) {
  const VMSVGA3DDxvkSRV *srv;

  if (surface == NULL || view_desc == NULL) {
    return NULL;
  }
  for (srv = surface->d3d11_srvs; srv != NULL; srv = srv->next) {
    if (vmsvga3d_dxvk_d3d11_srv_desc_equal(&srv->desc, view_desc)) {
      return srv;
    }
  }
  return NULL;
}

bool vmsvga3d_dxvk_d3d11_shader_resource_view_exists(
    const VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_srv_desc_s *desc) {
  const VMSVGA3DD3D10SRVDesc *view_desc = desc;
  const VMSVGA3DDxvkSRV *srv;

  if (surface == NULL || view_desc == NULL || !surface->d3d11_resident ||
      surface->d3d11_resource == NULL) {
    return false;
  }
  srv = vmsvga3d_dxvk_d3d11_srv_find(surface, view_desc);
  return srv != NULL && srv->view != NULL;
}

bool vmsvga3d_dxvk_d3d11_shader_resource_view_ensure(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_srv_desc_s *desc) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateShaderResourceView create_view = NULL;
  VMSVGA3DDxvkD3D11SRVDesc native;
  const VMSVGA3DD3D10SRVDesc *view_desc = desc;
  VMSVGA3DDxvkSRV *srv;
  void *view = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      surface == NULL || !surface->d3d11_resident ||
      surface->d3d11_resource == NULL || view_desc == NULL) {
    return false;
  }
  if (vmsvga3d_dxvk_d3d11_shader_resource_view_exists(surface, view_desc)) {
    return true;
  }
  if (!vmsvga3d_dxvk_d3d11_srv_desc(view_desc, &native) ||
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

  srv = g_try_new0(VMSVGA3DDxvkSRV, 1);
  if (srv == NULL) {
    vmsvga3d_dxvk_release(view, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  srv->desc = *view_desc;
  srv->view = view;
  srv->next = surface->d3d11_srvs;
  surface->d3d11_srvs = srv;
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)desc;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_generate_mips(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_srv_desc_s *desc) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11GenerateMips generate_mips = NULL;
  const VMSVGA3DD3D10SRVDesc *view_desc = desc;
  const VMSVGA3DDxvkSRV *srv;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      surface == NULL || !surface->d3d11_resident ||
      surface->d3d11_resource == NULL || view_desc == NULL) {
    return false;
  }
  srv = vmsvga3d_dxvk_d3d11_srv_find(surface, view_desc);
  if (srv == NULL || srv->view == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GENERATE_MIPS,
          &generate_mips, sizeof(generate_mips))) {
    return false;
  }
  generate_mips(dxvk->d3d11_context, srv->view);
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)desc;
  return false;
#endif
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
      desc == NULL ||
      !vmsvga3d_dxvk_get_method(
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
      desc == NULL ||
      !vmsvga3d_dxvk_get_method(
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
      desc == NULL ||
      !vmsvga3d_dxvk_get_method(
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
      desc == NULL ||
      !vmsvga3d_dxvk_get_method(
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
  if (state == NULL || state->state == NULL) {
    return false;
  }
  *native_state = state->state;
  return true;
}
#endif

bool vmsvga3d_dxvk_d3d11_set_blend_state(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t state_id,
    const float blend_factor[4], uint32_t sample_mask) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11OMSetBlendState set_state = NULL;
  void *state = NULL;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      blend_factor == NULL ||
      !vmsvga3d_dxvk_d3d11_state_object(
          dxvk, VMSVGA3D_DXVK_STATE_BLEND, cid, state_id, &state) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_OM_SET_BLEND_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  set_state(dxvk->d3d11_context, state, blend_factor, sample_mask);
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
  set_state(dxvk->d3d11_context, state, stencil_ref);
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
  if (sampler_count == 0) {
    return true;
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

static void vmsvga3d_dxvk_d3d11_input_layout_destroy_shader(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id);

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
  g_free(shader->bytecode);
  g_free(shader);
}

bool vmsvga3d_dxvk_d3d11_shader_define(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id,
    uint32_t shader_type, const void *bytecode, uint32_t bytecode_size) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateShader create_shader = NULL;
  VMSVGA3DDxvkShader **link = NULL;
  VMSVGA3DDxvkShader *old_shader;
  VMSVGA3DDxvkShader *new_shader;
  uint32_t method;
  void *native_shader = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      bytecode == NULL || bytecode_size == 0) {
    return false;
  }
  switch (shader_type) {
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
  if (!vmsvga3d_dxvk_get_method(dxvk->d3d11_device, method,
                                 &create_shader, sizeof(create_shader))) {
    return false;
  }

  result = create_shader(dxvk->d3d11_device, bytecode, bytecode_size,
                         NULL, &native_shader);
  if (!vmsvga3d_dxvk_succeeded(result) || native_shader == NULL) {
    if (native_shader != NULL) {
      vmsvga3d_dxvk_release(native_shader, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    return false;
  }

  new_shader = g_try_new0(VMSVGA3DDxvkShader, 1);
  if (new_shader == NULL) {
    vmsvga3d_dxvk_release(native_shader, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    return false;
  }
  new_shader->bytecode = g_try_malloc(bytecode_size);
  if (new_shader->bytecode == NULL) {
    vmsvga3d_dxvk_release(native_shader, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    g_free(new_shader);
    return false;
  }
  memcpy(new_shader->bytecode, bytecode, bytecode_size);
  new_shader->cid = cid;
  new_shader->shader_id = shader_id;
  new_shader->shader_type = shader_type;
  new_shader->shader = native_shader;
  new_shader->bytecode_size = bytecode_size;

  old_shader = vmsvga3d_dxvk_d3d11_shader_find(
      dxvk, cid, shader_id, &link);
  vmsvga3d_dxvk_d3d11_input_layout_destroy_shader(dxvk, cid, shader_id);
  if (old_shader != NULL) {
    new_shader->next = old_shader->next;
    *link = new_shader;
    vmsvga3d_dxvk_d3d11_shader_free(old_shader);
  } else {
    new_shader->next = dxvk->d3d11_shaders;
    dxvk->d3d11_shaders = new_shader;
  }
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)shader_id;
  (void)shader_type;
  (void)bytecode;
  (void)bytecode_size;
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
  vmsvga3d_dxvk_d3d11_input_layout_destroy_shader(dxvk, cid, shader_id);
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
    uint32_t shader_id, VMSVGA3DDxvkInputLayout ***link_out) {
  VMSVGA3DDxvkInputLayout **link;

  if (dxvk == NULL) {
    return NULL;
  }
  for (link = &dxvk->d3d11_input_layouts; *link != NULL;
       link = &(*link)->next) {
    VMSVGA3DDxvkInputLayout *layout = *link;

    if (layout->cid == cid && layout->layout_id == layout_id &&
        layout->shader_id == shader_id) {
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

static void vmsvga3d_dxvk_d3d11_input_layout_destroy_shader(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id) {
  VMSVGA3DDxvkInputLayout **link;

  if (dxvk == NULL) {
    return;
  }
  link = &dxvk->d3d11_input_layouts;
  while (*link != NULL) {
    VMSVGA3DDxvkInputLayout *layout = *link;

    if (layout->cid != cid || layout->shader_id != shader_id) {
      link = &layout->next;
      continue;
    }
    *link = layout->next;
    vmsvga3d_dxvk_d3d11_input_layout_free(layout);
  }
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
          dxvk, cid, layout_id, shader_id, NULL) != NULL) {
    return true;
  }
  shader = vmsvga3d_dxvk_d3d11_shader_find(
      dxvk, cid, shader_id, NULL);
  if (shader == NULL || shader->shader_type != SVGA3D_SHADERTYPE_VS ||
      shader->shader == NULL || shader->bytecode == NULL ||
      shader->bytecode_size == 0 ||
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
  layout->shader_id = shader_id;
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
  VMSVGA3DDxvkD3D11QueryDesc desc = {
    .query = d3d_query,
    .misc_flags = misc_flags,
  };
  VMSVGA3DDxvkQuery *query;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      !vmsvga3d_dxvk_d3d11_query_destroy(dxvk, cid, query_id) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_QUERY,
          &create_query, sizeof(create_query))) {
    return false;
  }
  query = g_try_new0(VMSVGA3DDxvkQuery, 1);
  if (query == NULL) {
    return false;
  }
  result = create_query(dxvk->d3d11_device, &desc, &query->query);
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
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, bool issue_end,
    void *data, uint32_t data_size, uint32_t getdata_flags) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11End end_query = NULL;
  VMSVGA3DDxvkD3D11GetData get_data = NULL;
  VMSVGA3DDxvkQuery *query;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_context == NULL ||
      data == NULL || data_size == 0) {
    return false;
  }
  query = vmsvga3d_dxvk_d3d11_query_find(dxvk, cid, query_id, NULL);
  if (query == NULL || query->query == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_GET_DATA,
          &get_data, sizeof(get_data))) {
    return false;
  }
  if (issue_end) {
    if (!vmsvga3d_dxvk_get_method(
            dxvk->d3d11_context, VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_END,
            &end_query, sizeof(end_query))) {
      return false;
    }
    end_query(dxvk->d3d11_context, query->query);
  }

  do {
    result = get_data(dxvk->d3d11_context, query->query, data, data_size,
                      getdata_flags);
    if (result != 0) {
      sched_yield();
    }
  } while (result != 0);
  return true;
#else
  (void)dxvk;
  (void)cid;
  (void)query_id;
  (void)issue_end;
  (void)data;
  (void)data_size;
  (void)getdata_flags;
  return false;
#endif
}

bool vmsvga3d_dxvk_d3d11_set_predication(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t query_id, bool enabled,
    bool predicate_value) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreatePredicate create_predicate = NULL;
  VMSVGA3DDxvkD3D11SetPredication set_predication = NULL;
  VMSVGA3DDxvkD3D11QueryDesc desc;
  VMSVGA3DDxvkQuery *query = NULL;
  void *predicate = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      dxvk->d3d11_context == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_SET_PREDICATION,
          &set_predication, sizeof(set_predication))) {
    return false;
  }
  if (!enabled) {
    set_predication(dxvk->d3d11_context, NULL, 0);
    return true;
  }

  query = vmsvga3d_dxvk_d3d11_query_find(dxvk, cid, query_id, NULL);
  if (query == NULL || query->query == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device, VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_PREDICATE,
          &create_predicate, sizeof(create_predicate))) {
    return false;
  }
  desc.query = query->d3d_query;
  desc.misc_flags = query->misc_flags;
  result = create_predicate(dxvk->d3d11_device, &desc, &predicate);
  if (!vmsvga3d_dxvk_succeeded(result) || predicate == NULL) {
    if (predicate != NULL) {
      vmsvga3d_dxvk_release(predicate, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
    }
    return false;
  }

  vmsvga3d_dxvk_release(query->query, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  query->query = predicate;
  set_predication(dxvk->d3d11_context, query->query,
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

bool vmsvga3d_dxvk_d3d11_clear_render_target_view(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_rtv_desc_s *desc, const float color[4]) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateRenderTargetView create_view = NULL;
  VMSVGA3DDxvkD3D11ClearRenderTargetView clear_view = NULL;
  VMSVGA3DDxvkD3D11RTVDesc native;
  const VMSVGA3DD3D10RTVDesc *view_desc = desc;
  void *view = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      dxvk->d3d11_context == NULL || surface == NULL ||
      !surface->d3d11_resident || surface->d3d11_resource == NULL ||
      view_desc == NULL || color == NULL ||
      !vmsvga3d_dxvk_d3d11_rtv_desc(view_desc, &native) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_RENDERTARGET_VIEW,
          &create_view, sizeof(create_view)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_CLEAR_RENDERTARGET_VIEW,
          &clear_view, sizeof(clear_view))) {
    return false;
  }

  result = create_view(dxvk->d3d11_device, surface->d3d11_resource, &native,
                       &view);
  if (!vmsvga3d_dxvk_succeeded(result) || view == NULL) {
    return false;
  }
  clear_view(dxvk->d3d11_context, view, color);
  vmsvga3d_dxvk_release(view, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)desc;
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

bool vmsvga3d_dxvk_d3d11_clear_depth_stencil_view(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d10_dsv_desc_s *desc, uint32_t clear_flags,
    float depth, uint8_t stencil) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkD3D11CreateDepthStencilView create_view = NULL;
  VMSVGA3DDxvkD3D11ClearDepthStencilView clear_view = NULL;
  VMSVGA3DDxvkD3D11DSVDesc native;
  const VMSVGA3DD3D10DSVDesc *view_desc = desc;
  void *view = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->d3d11_device == NULL ||
      dxvk->d3d11_context == NULL || surface == NULL ||
      !surface->d3d11_resident || surface->d3d11_resource == NULL ||
      view_desc == NULL ||
      !vmsvga3d_dxvk_d3d11_dsv_desc(view_desc, &native) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_device,
          VMSVGA3D_DXVK_ID3D11DEVICE_CREATE_DEPTH_STENCIL_VIEW,
          &create_view, sizeof(create_view)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->d3d11_context,
          VMSVGA3D_DXVK_ID3D11DEVICECONTEXT_CLEAR_DEPTH_STENCIL_VIEW,
          &clear_view, sizeof(clear_view))) {
    return false;
  }

  result = create_view(dxvk->d3d11_device, surface->d3d11_resource, &native,
                       &view);
  if (!vmsvga3d_dxvk_succeeded(result) || view == NULL) {
    return false;
  }
  clear_view(dxvk->d3d11_context, view, clear_flags, depth, stencil);
  vmsvga3d_dxvk_release(view, VMSVGA3D_DXVK_IUNKNOWN_RELEASE);
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)desc;
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
