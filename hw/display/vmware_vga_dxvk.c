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
#include "include/vmware_vga_dxvk.h"
#include "include/vmware_vga_dxvk_wsi.h"

struct vmsvga3d_dxvk_s {
  VMSVGA3DDxvkWsi *wsi;
  void *library;
  void *d3d9;
  void *device;
  void *pristine_state;
  bool ready;
};

/*
 * Renderer-side lifetime object for one guest SVGA3D surface.  Concrete D3D9
 * resources are deliberately created lazily when the first GPU use establishes
 * whether the surface is a texture, render target, depth target or buffer.
 */
struct vmsvga3d_dxvk_surface_s {
  uint32_t sid;
  VMSVGA3DD3D9HostResourceType resource_type;
  uint32_t usage;
  uint32_t format;
  uint32_t length;
  void *resource;
  void *bounce;
  bool resident;
  bool has_bounce;
};

#if defined(CONFIG_LINUX) && defined(__ELF__)

#include <dlfcn.h>

#define VMSVGA3D_DXVK_D3D9_SONAME "libdxvk_d3d9.so.0"
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

  if (dxvk == NULL || dxvk->device == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_STATE_BLOCK,
          &create_state_block, sizeof(create_state_block))) {
    return false;
  }
  result = create_state_block(dxvk->device, VMSVGA3D_DXVK_D3DSBT_ALL,
                              &dxvk->pristine_state);
  return vmsvga3d_dxvk_succeeded(result) && dxvk->pristine_state != NULL;
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
 * Silence the native DXVK startup dump for normal QEMU runs, but leave the
 * caller's logging configuration untouched when the project-wide DEBUG
 * switch is enabled.  Match the shell enabled() helper used by the project:
 * y/yes/true/1/on/enable/enabled, case-insensitively, after trimming.
 * The temporary override is restored after DXVK has initialized, so unrelated
 * code in the QEMU process does not inherit it.
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
                         &dxvk->device);
  if (result < 0 || dxvk->device == NULL) {
    if (dxvk->device != NULL) {
      vmsvga3d_dxvk_release(dxvk->device,
                            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
      dxvk->device = NULL;
    }
    behavior_flags = VMSVGA3D_DXVK_D3DCREATE_MULTITHREADED |
                     VMSVGA3D_DXVK_D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    result = create_device(dxvk->d3d9, VMSVGA3D_DXVK_D3DADAPTER_DEFAULT,
                           VMSVGA3D_DXVK_D3DDEVTYPE_HAL,
                           present.device_window, behavior_flags, &present,
                           &dxvk->device);
  }
  if (result < 0 || dxvk->device == NULL) {
    if (dxvk->device != NULL) {
      vmsvga3d_dxvk_release(dxvk->device,
                            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
      dxvk->device = NULL;
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
    error_setg(errp, "failed to allocate DXVK D3D9 runtime state");
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

  dxvk->library = dlopen(VMSVGA3D_DXVK_D3D9_SONAME, RTLD_NOW | RTLD_LOCAL);
  if (dxvk->library == NULL) {
    error_setg(errp, "failed to load %s: %s", VMSVGA3D_DXVK_D3D9_SONAME,
               dlerror());
    goto fail;
  }
  create9_entry = dlsym(dxvk->library, "Direct3DCreate9");
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
  error_setg(errp, "DXVK D3D9 runtime is only available on Linux ELF hosts");
  return NULL;
#endif
}

void vmsvga3d_dxvk_destroy(VMSVGA3DDxvk *dxvk) {
  if (dxvk == NULL) {
    return;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  dxvk->ready = false;
  if (dxvk->pristine_state != NULL) {
    vmsvga3d_dxvk_release(dxvk->pristine_state,
                          VMSVGA3D_DXVK_IDIRECT3DSTATEBLOCK9_RELEASE);
    dxvk->pristine_state = NULL;
  }
  if (dxvk->device != NULL) {
    vmsvga3d_dxvk_release(dxvk->device,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    dxvk->device = NULL;
  }
  if (dxvk->d3d9 != NULL) {
    vmsvga3d_dxvk_release(dxvk->d3d9,
                          VMSVGA3D_DXVK_IDIRECT3D9_RELEASE);
    dxvk->d3d9 = NULL;
  }
  if (dxvk->library != NULL) {
    dlclose(dxvk->library);
    dxvk->library = NULL;
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
  surface->resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_NONE;
  return surface;
}

void vmsvga3d_dxvk_surface_evict(VMSVGA3DDxvkSurface *surface) {
  if (surface == NULL) {
    return;
  }
#if defined(CONFIG_LINUX) && defined(__ELF__)
  if (surface->bounce != NULL) {
    vmsvga3d_dxvk_release(surface->bounce,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    surface->bounce = NULL;
  }
  if (surface->resource != NULL) {
    vmsvga3d_dxvk_release(surface->resource,
                          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
    surface->resource = NULL;
  }
#endif
  surface->resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_NONE;
  surface->usage = 0;
  surface->format = 0;
  surface->length = 0;
  surface->resident = false;
  surface->has_bounce = false;
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
  info->resource_type = surface->resource_type;
  info->usage = surface->usage;
  info->resident = surface->resident;
  info->has_bounce = surface->has_bounce;
  return true;
}

#if defined(CONFIG_LINUX) && defined(__ELF__)
static bool vmsvga3d_dxvk_surface_plan_compatible(
    const VMSVGA3DDxvkSurface *surface,
    const VMSVGA3DD3D9ResourcePlan *plan) {
  if (surface == NULL || plan == NULL || !surface->resident ||
      surface->resource == NULL) {
    return false;
  }
  switch (plan->use) {
  case VMSVGA3D_D3D9_RESOURCE_USE_TEXTURE:
    return surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE;
  case VMSVGA3D_D3D9_RESOURCE_USE_COLOR_TARGET:
    return (surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE ||
            surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE) &&
           (surface->usage & VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET) != 0;
  case VMSVGA3D_D3D9_RESOURCE_USE_DEPTH_TARGET:
    return (surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE ||
            surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE) &&
           (surface->usage & VMSVGA3D_DXVK_D3DUSAGE_DEPTHSTENCIL) != 0;
  case VMSVGA3D_D3D9_RESOURCE_USE_VERTEX_BUFFER:
    return surface->resource_type ==
               VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER &&
           surface->length == plan->primary.length;
  case VMSVGA3D_D3D9_RESOURCE_USE_INDEX_BUFFER:
    return surface->resource_type ==
               VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER &&
           surface->length == plan->primary.length &&
           surface->format == plan->primary.format;
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
      !resource_plan->primary.valid) {
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
  if (surface->resident) {
    vmsvga3d_dxvk_surface_evict(surface);
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
            dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_TEXTURE,
            &create_texture, sizeof(create_texture))) {
      return false;
    }
    result = create_texture(
        dxvk->device, primary_desc->width, primary_desc->height,
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
          dxvk->device, primary_desc->width, primary_desc->height,
          primary_desc->levels, primary_desc->usage, primary_desc->format,
          primary_desc->pool, &primary, NULL);
    }
    if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
      return false;
    }
    result = create_texture(
        dxvk->device, resource_plan->bounce.width,
        resource_plan->bounce.height, resource_plan->bounce.levels,
        resource_plan->bounce.usage, resource_plan->bounce.format,
        resource_plan->bounce.pool, &bounce, NULL);
    if (!vmsvga3d_dxvk_succeeded(result) || bounce == NULL) {
      vmsvga3d_dxvk_release(primary, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
      return false;
    }
    surface->resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE;
    surface->has_bounce = true;
  } else if (primary_desc->resource_type ==
             VMSVGA3D_DXVK_D3D9_RTYPE_SURFACE) {
    if ((primary_desc->usage & VMSVGA3D_DXVK_D3DUSAGE_DEPTHSTENCIL) != 0) {
      if (!vmsvga3d_dxvk_get_method(
              dxvk->device,
              VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_DEPTH_STENCIL_SURFACE,
              &create_depth_stencil, sizeof(create_depth_stencil))) {
        return false;
      }
      result = create_depth_stencil(
          dxvk->device, primary_desc->width, primary_desc->height,
          primary_desc->format, primary_desc->multisample_type,
          primary_desc->multisample_quality, primary_desc->discard,
          &primary, NULL);
      if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
        return false;
      }
    } else if ((primary_desc->usage &
                VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET) != 0) {
      if (!vmsvga3d_dxvk_get_method(
              dxvk->device,
              VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_RENDER_TARGET,
              &create_render_target, sizeof(create_render_target)) ||
          !vmsvga3d_dxvk_get_method(
              dxvk->device,
              VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_OFFSCREEN_PLAIN_SURFACE,
              &create_offscreen, sizeof(create_offscreen))) {
        return false;
      }
      result = create_render_target(
          dxvk->device, primary_desc->width, primary_desc->height,
          primary_desc->format, primary_desc->multisample_type,
          primary_desc->multisample_quality, primary_desc->lockable,
          &primary, NULL);
      if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
        return false;
      }
      result = create_offscreen(
          dxvk->device, primary_desc->width, primary_desc->height,
          primary_desc->format, VMSVGA3D_DXVK_D3DPOOL_SYSTEMMEM,
          &bounce, NULL);
      if (!vmsvga3d_dxvk_succeeded(result) || bounce == NULL) {
        vmsvga3d_dxvk_release(primary,
                              VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE);
        return false;
      }
      surface->has_bounce = true;
    } else {
      return false;
    }
    surface->resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE;
  } else if (primary_desc->resource_type ==
             VMSVGA3D_DXVK_D3D9_RTYPE_VERTEX_BUFFER) {
    if (!vmsvga3d_dxvk_get_method(
            dxvk->device,
            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_VERTEX_BUFFER,
            &create_vertex_buffer, sizeof(create_vertex_buffer))) {
      return false;
    }
    result = create_vertex_buffer(
        dxvk->device, primary_desc->length, primary_desc->usage, 0,
        primary_desc->pool, &primary, NULL);
    if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
      return false;
    }
    surface->resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER;
  } else if (primary_desc->resource_type ==
             VMSVGA3D_DXVK_D3D9_RTYPE_INDEX_BUFFER) {
    if (!vmsvga3d_dxvk_get_method(
            dxvk->device,
            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_INDEX_BUFFER,
            &create_index_buffer, sizeof(create_index_buffer))) {
      return false;
    }
    result = create_index_buffer(
        dxvk->device, primary_desc->length, primary_desc->usage,
        primary_desc->format, primary_desc->pool, &primary, NULL);
    if (!vmsvga3d_dxvk_succeeded(result) || primary == NULL) {
      return false;
    }
    surface->resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER;
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

  surface->resource = primary;
  surface->bounce = bounce;
  surface->usage = primary_desc->usage;
  surface->format = primary_desc->format;
  surface->length = primary_desc->length;
  surface->resident = true;
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)plan;
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

  if (surface == NULL || d3d_surface == NULL || !surface->resident) {
    return false;
  }
  *d3d_surface = NULL;
  object = bounce ? surface->bounce : surface->resource;
  if (object == NULL) {
    return false;
  }
  if (surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE) {
    if (!vmsvga3d_dxvk_get_method(
            object, VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_GET_SURFACE_LEVEL,
            &get_surface_level, sizeof(get_surface_level))) {
      return false;
    }
    result = get_surface_level(object, level, d3d_surface);
    return vmsvga3d_dxvk_succeeded(result) && *d3d_surface != NULL;
  }
  if (surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE &&
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

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || !surface->resident ||
      !surface->has_bounce || surface->bounce == NULL || data == NULL ||
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_UPDATE_SURFACE,
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
  result = update_surface(dxvk->device, source_surface, NULL,
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

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || !surface->resident ||
      surface->resource == NULL || data == NULL || row_bytes == 0 ||
      rows == 0 ||
      !vmsvga3d_dxvk_surface_level_acquire(surface, false, level,
                                           &source_surface)) {
    goto out;
  }

  if ((surface->usage & VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET) != 0 ||
      surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE) {
    if (!surface->has_bounce || surface->bounce == NULL ||
        !vmsvga3d_dxvk_surface_level_acquire(surface, true, level,
                                             &bounce_surface) ||
        !vmsvga3d_dxvk_get_method(
            dxvk->device,
            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_RENDER_TARGET_DATA,
            &get_render_target_data, sizeof(get_render_target_data))) {
      goto out;
    }
    result = get_render_target_data(dxvk->device, source_surface,
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

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || !surface->resident ||
      surface->resource == NULL || data == NULL || size == 0 ||
      size > surface->length ||
      (surface->resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER &&
       surface->resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER) ||
      !vmsvga3d_dxvk_get_method(
          surface->resource, VMSVGA3D_DXVK_IDIRECT3DBUFFER9_LOCK,
          &lock, sizeof(lock)) ||
      !vmsvga3d_dxvk_get_method(
          surface->resource, VMSVGA3D_DXVK_IDIRECT3DBUFFER9_UNLOCK,
          &unlock, sizeof(unlock))) {
    return false;
  }
  result = lock(surface->resource, 0, size, &destination,
                VMSVGA3D_DXVK_D3DLOCK_DISCARD);
  if (!vmsvga3d_dxvk_succeeded(result) || destination == NULL) {
    return false;
  }
  memcpy(destination, data, size);
  result = unlock(surface->resource);
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
      !source->resident || !destination->resident ||
      !vmsvga3d_dxvk_surface_level_acquire(source, false, source_level,
                                           &source_surface) ||
      !vmsvga3d_dxvk_surface_level_acquire(destination, false,
                                           destination_level,
                                           &destination_surface) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_STRETCH_RECT,
          &stretch_rect, sizeof(stretch_rect))) {
    goto out;
  }
  result = stretch_rect(dxvk->device, source_surface, source_rect,
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_RENDER_TARGET,
          &set_render_target, sizeof(set_render_target)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_RENDER_TARGET,
          &get_render_target, sizeof(get_render_target)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_DEPTH_STENCIL_SURFACE,
          &set_depth_stencil, sizeof(set_depth_stencil)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_DEPTH_STENCIL_SURFACE,
          &get_depth_stencil, sizeof(get_depth_stencil)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CLEAR,
          &clear, sizeof(clear)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_SCISSOR_RECT,
          &set_scissor, sizeof(set_scissor)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_SCISSOR_RECT,
          &get_scissor, sizeof(get_scissor))) {
    return false;
  }

  for (i = 0; i < 8; i++) {
    if (color_targets[i] != NULL) {
      highest_target = i;
    }
  }
  for (i = 0; i <= highest_target; i++) {
    result = get_render_target(dxvk->device, i, &saved_targets[i]);
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

  result = get_depth_stencil(dxvk->device, &saved_depth_stencil);
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

  result = get_scissor(dxvk->device, &saved_scissor);
  if (!vmsvga3d_dxvk_succeeded(result)) {
    goto restore;
  }
  have_saved_scissor = true;

  for (i = 0; i <= highest_target; i++) {
    result = set_render_target(dxvk->device, i, bound_targets[i]);
    if (!vmsvga3d_dxvk_succeeded(result)) {
      goto restore;
    }
  }
  result = set_depth_stencil(dxvk->device, bound_depth_stencil);
  if (!vmsvga3d_dxvk_succeeded(result)) {
    goto restore;
  }
  result = set_scissor(dxvk->device, clear_scissor);
  if (!vmsvga3d_dxvk_succeeded(result)) {
    goto restore;
  }
  result = clear(dxvk->device, rect_count, rects, flags, color, depth,
                 stencil);
  success = vmsvga3d_dxvk_succeeded(result);

restore:
  if (have_saved_scissor) {
    set_scissor(dxvk->device, &saved_scissor);
  }
  for (i = 0; i <= highest_target; i++) {
    set_render_target(dxvk->device, i, saved_targets[i]);
  }
  if (have_saved_depth) {
    set_depth_stencil(dxvk->device, saved_depth_stencil);
  } else {
    set_depth_stencil(dxvk->device, NULL);
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

  if (!vmsvga3d_dxvk_ready(dxvk) || dxvk->pristine_state == NULL ||
      !vmsvga3d_dxvk_get_method(
          dxvk->pristine_state, VMSVGA3D_DXVK_IDIRECT3DSTATEBLOCK9_APPLY,
          &apply, sizeof(apply))) {
    return false;
  }
  result = apply(dxvk->pristine_state);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_RENDER_TARGET,
          &set_render_target, sizeof(set_render_target))) {
    return false;
  }
  if (surface != NULL &&
      !vmsvga3d_dxvk_surface_level_acquire(surface, false, level,
                                           &d3d_surface)) {
    return false;
  }
  result = set_render_target(dxvk->device, index, d3d_surface);
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
          dxvk->device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_DEPTH_STENCIL_SURFACE,
          &set_depth_stencil, sizeof(set_depth_stencil))) {
    return false;
  }
  if (surface != NULL &&
      !vmsvga3d_dxvk_surface_level_acquire(surface, false, level,
                                           &d3d_surface)) {
    return false;
  }
  result = set_depth_stencil(dxvk->device, d3d_surface);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_RENDER_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  result = set_state(dxvk->device, state, value);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_TEXTURE,
          &set_texture, sizeof(set_texture))) {
    return false;
  }
  if (surface != NULL) {
    if (!surface->resident || surface->resource == NULL ||
        surface->resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE) {
      return false;
    }
    texture = surface->resource;
  }
  result = set_texture(dxvk->device, stage, texture);
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
          dxvk->device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_TEXTURE_STAGE_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  result = set_state(dxvk->device, stage, state, value);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_SAMPLER_STATE,
          &set_state, sizeof(set_state))) {
    return false;
  }
  result = set_state(dxvk->device, sampler, state, value);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_TRANSFORM,
          &set_transform, sizeof(set_transform))) {
    return false;
  }
  result = set_transform(dxvk->device, type, matrix);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VIEWPORT,
          &set_viewport, sizeof(set_viewport))) {
    return false;
  }
  result = set_viewport(dxvk->device, viewport);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_SCISSOR_RECT,
          &set_scissor, sizeof(set_scissor))) {
    return false;
  }
  result = set_scissor(dxvk->device, rect);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_MATERIAL,
          &set_material, sizeof(set_material))) {
    return false;
  }
  result = set_material(dxvk->device, material);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_LIGHT,
          &set_light, sizeof(set_light))) {
    return false;
  }
  result = set_light(dxvk->device, index, light);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_LIGHT_ENABLE,
          &light_enable, sizeof(light_enable))) {
    return false;
  }
  result = light_enable(dxvk->device, index, enabled ? 1 : 0);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_CLIP_PLANE,
          &set_clip_plane, sizeof(set_clip_plane))) {
    return false;
  }
  result = set_clip_plane(dxvk->device, index, plane);
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
  if (!vmsvga3d_dxvk_get_method(dxvk->device, method, &create_shader,
                                 sizeof(create_shader))) {
    return NULL;
  }
  result = create_shader(dxvk->device, bytecode, &shader);
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
  if (!vmsvga3d_dxvk_get_method(dxvk->device, method, &set_shader,
                                 sizeof(set_shader))) {
    return false;
  }
  result = set_shader(dxvk->device, shader);
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
  entry = vmsvga3d_dxvk_vtable_entry(dxvk->device, method);
  if (entry == NULL) {
    return false;
  }
  if (target == VMSVGA3D_D3D9_CONST_TARGET_VS_BOOL ||
      target == VMSVGA3D_D3D9_CONST_TARGET_PS_BOOL) {
    VMSVGA3DDxvkSetShaderConstantB set_constant = NULL;
    memcpy(&set_constant, &entry, sizeof(set_constant));
    boolean_value = values[0] != 0;
    result = set_constant(dxvk->device, reg, &boolean_value, 1);
  } else if (target == VMSVGA3D_D3D9_CONST_TARGET_VS_INT ||
             target == VMSVGA3D_D3D9_CONST_TARGET_PS_INT) {
    VMSVGA3DDxvkSetShaderConstantI set_constant = NULL;
    memcpy(&set_constant, &entry, sizeof(set_constant));
    memcpy(int_values, values, sizeof(int_values));
    result = set_constant(dxvk->device, reg, int_values, 1);
  } else {
    VMSVGA3DDxvkSetShaderConstantF set_constant = NULL;
    memcpy(&set_constant, &entry, sizeof(set_constant));
    memcpy(float_values, values, sizeof(float_values));
    result = set_constant(dxvk->device, reg, float_values, 1);
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
          dxvk->device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_VERTEX_DECLARATION,
          &create_declaration, sizeof(create_declaration))) {
    return NULL;
  }
  result = create_declaration(dxvk->device, elements, &declaration);
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
          dxvk->device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_VERTEX_DECLARATION,
          &set_declaration, sizeof(set_declaration))) {
    return false;
  }
  result = set_declaration(dxvk->device, declaration);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_STREAM_SOURCE,
          &set_stream, sizeof(set_stream))) {
    return false;
  }
  if (surface != NULL) {
    if (!surface->resident || surface->resource == NULL ||
        surface->resource_type !=
            VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER) {
      return false;
    }
    buffer = surface->resource;
  }
  result = set_stream(dxvk->device, stream, buffer, offset, stride);
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
          dxvk->device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_STREAM_SOURCE_FREQ,
          &set_frequency, sizeof(set_frequency))) {
    return false;
  }
  result = set_frequency(dxvk->device, stream, frequency);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_SET_INDICES,
          &set_indices, sizeof(set_indices))) {
    return false;
  }
  if (surface != NULL) {
    if (!surface->resident || surface->resource == NULL ||
        surface->resource_type !=
            VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER) {
      return false;
    }
    buffer = surface->resource;
  }
  result = set_indices(dxvk->device, buffer);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_BEGIN_SCENE,
          &begin_scene, sizeof(begin_scene))) {
    return false;
  }
  result = begin_scene(dxvk->device);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_END_SCENE,
          &end_scene, sizeof(end_scene))) {
    return false;
  }
  result = end_scene(dxvk->device);
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
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_DRAW_PRIMITIVE,
          &draw, sizeof(draw))) {
    return false;
  }
  result = draw(dxvk->device, primitive_type, start_vertex, primitive_count);
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
          dxvk->device,
          VMSVGA3D_DXVK_IDIRECT3DDEVICE9_DRAW_INDEXED_PRIMITIVE,
          &draw, sizeof(draw))) {
    return false;
  }
  result = draw(dxvk->device, primitive_type, base_vertex_index,
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
