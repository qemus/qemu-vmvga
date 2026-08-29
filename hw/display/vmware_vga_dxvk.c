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
#define VMSVGA3D_DXVK_IDIRECT3D9_RELEASE 2u
#define VMSVGA3D_DXVK_IDIRECT3D9_CREATE_DEVICE 16u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_RELEASE 2u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_TEXTURE 23u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_UPDATE_TEXTURE 31u
#define VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_RENDER_TARGET_DATA 32u
#define VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_GET_SURFACE_LEVEL 18u
#define VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_LOCK_RECT 19u
#define VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_UNLOCK_RECT 20u
#define VMSVGA3D_DXVK_D3D9_RTYPE_TEXTURE 3u
#define VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET 0x00000001u
#define VMSVGA3D_DXVK_D3DLOCK_READONLY 0x00000010u

static GMutex vmsvga3d_dxvk_init_lock;

/*
 * DXVK Native exposes the D3D9 COM ABI with the host C calling convention.
 * Its current C convenience declarations omit the implicit interface pointer,
 * so use the stable vtable layout directly rather than including d3d9.h.
 */
typedef void (*VMSVGA3DDxvkComFunction)(void);
typedef void *(*VMSVGA3DDxvkDirect3DCreate9)(uint32_t sdk_version);
typedef uint32_t (*VMSVGA3DDxvkRelease)(void *object);
typedef int32_t (*VMSVGA3DDxvkCreateDevice)(
    void *d3d9, uint32_t adapter, uint32_t device_type, void *focus_window,
    uint32_t behavior_flags, void *present_parameters, void **device);
typedef int32_t (*VMSVGA3DDxvkCreateTexture)(
    void *device, uint32_t width, uint32_t height, uint32_t levels,
    uint32_t usage, uint32_t format, uint32_t pool, void **texture,
    void **shared_handle);
typedef int32_t (*VMSVGA3DDxvkUpdateTexture)(void *device, void *source,
                                             void *destination);
typedef int32_t (*VMSVGA3DDxvkGetRenderTargetData)(void *device,
                                                    void *source,
                                                    void *destination);
typedef int32_t (*VMSVGA3DDxvkTextureGetSurfaceLevel)(void *texture,
                                                       uint32_t level,
                                                       void **surface);
typedef int32_t (*VMSVGA3DDxvkTextureLockRect)(void *texture, uint32_t level,
                                               void *locked_rect,
                                               const void *rect,
                                               uint32_t flags);
typedef int32_t (*VMSVGA3DDxvkTextureUnlockRect)(void *texture,
                                                 uint32_t level);

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
  bool config_environment_set = false;
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

  vmsvga3d_dxvk_restore_wsi_environment(restore_wsi_environment);
  if (config_environment_set) {
    vmsvga3d_dxvk_restore_config_environment(saved_config_file);
  }
  g_mutex_unlock(&vmsvga3d_dxvk_init_lock);
  dxvk->ready = true;
  return dxvk;

fail:
  vmsvga3d_dxvk_restore_wsi_environment(restore_wsi_environment);
  if (config_environment_set) {
    vmsvga3d_dxvk_restore_config_environment(saved_config_file);
  }
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

bool vmsvga3d_dxvk_surface_materialize(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface,
    const struct vmsvga3d_d3d9_resource_plan_s *plan) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkCreateTexture create_texture = NULL;
  const VMSVGA3DD3D9ResourcePlan *resource_plan = plan;
  const VMSVGA3DD3D9CreateDesc *primary_desc;
  void *primary = NULL;
  void *bounce = NULL;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || resource_plan == NULL ||
      !resource_plan->primary.valid || !resource_plan->has_bounce ||
      !resource_plan->bounce.valid ||
      resource_plan->primary.resource_type != VMSVGA3D_DXVK_D3D9_RTYPE_TEXTURE ||
      resource_plan->bounce.resource_type != VMSVGA3D_DXVK_D3D9_RTYPE_TEXTURE ||
      resource_plan->primary.width == 0 || resource_plan->primary.height == 0 ||
      resource_plan->primary.levels == 0 ||
      resource_plan->bounce.levels != resource_plan->primary.levels ||
      resource_plan->bounce.width != resource_plan->primary.width ||
      resource_plan->bounce.height != resource_plan->primary.height ||
      resource_plan->bounce.format != resource_plan->primary.format) {
    return false;
  }
  if (surface->resident) {
    return surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE &&
           surface->resource != NULL && surface->bounce != NULL;
  }
  if (!vmsvga3d_dxvk_get_method(
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_CREATE_TEXTURE,
          &create_texture, sizeof(create_texture))) {
    return false;
  }
  primary_desc = &resource_plan->primary;
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

  surface->resource = primary;
  surface->bounce = bounce;
  surface->resource_type = VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE;
  surface->usage = primary_desc->usage;
  surface->resident = true;
  surface->has_bounce = true;
  return true;
#else
  (void)dxvk;
  (void)surface;
  (void)plan;
  return false;
#endif
}

bool vmsvga3d_dxvk_surface_upload_level(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *surface, uint32_t level,
    const void *data, uint32_t row_bytes, uint32_t rows) {
#if defined(CONFIG_LINUX) && defined(__ELF__)
  VMSVGA3DDxvkTextureLockRect lock_rect = NULL;
  VMSVGA3DDxvkTextureUnlockRect unlock_rect = NULL;
  VMSVGA3DDxvkUpdateTexture update_texture = NULL;
  VMSVGA3DDxvkLockedRect locked = { 0 };
  const uint8_t *source = data;
  uint8_t *destination;
  uint32_t y;
  int32_t result;

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || !surface->resident ||
      surface->resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE ||
      surface->resource == NULL || surface->bounce == NULL || data == NULL ||
      row_bytes == 0 || rows == 0 ||
      !vmsvga3d_dxvk_get_method(
          surface->bounce, VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_LOCK_RECT,
          &lock_rect, sizeof(lock_rect)) ||
      !vmsvga3d_dxvk_get_method(
          surface->bounce, VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_UNLOCK_RECT,
          &unlock_rect, sizeof(unlock_rect)) ||
      !vmsvga3d_dxvk_get_method(
          dxvk->device, VMSVGA3D_DXVK_IDIRECT3DDEVICE9_UPDATE_TEXTURE,
          &update_texture, sizeof(update_texture))) {
    return false;
  }

  result = lock_rect(surface->bounce, level, &locked, NULL, 0);
  if (!vmsvga3d_dxvk_succeeded(result) || locked.bits == NULL ||
      locked.pitch < 0 || (uint32_t)locked.pitch < row_bytes) {
    if (vmsvga3d_dxvk_succeeded(result)) {
      unlock_rect(surface->bounce, level);
    }
    return false;
  }
  destination = locked.bits;
  for (y = 0; y < rows; y++) {
    memcpy(destination + (size_t)y * (uint32_t)locked.pitch,
           source + (size_t)y * row_bytes, row_bytes);
  }
  result = unlock_rect(surface->bounce, level);
  if (!vmsvga3d_dxvk_succeeded(result)) {
    return false;
  }
  result = update_texture(dxvk->device, surface->bounce, surface->resource);
  return vmsvga3d_dxvk_succeeded(result);
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
  VMSVGA3DDxvkTextureGetSurfaceLevel get_surface_level = NULL;
  VMSVGA3DDxvkGetRenderTargetData get_render_target_data = NULL;
  VMSVGA3DDxvkTextureLockRect lock_rect = NULL;
  VMSVGA3DDxvkTextureUnlockRect unlock_rect = NULL;
  VMSVGA3DDxvkLockedRect locked = { 0 };
  void *source_surface = NULL;
  void *bounce_surface = NULL;
  void *lock_texture;
  const uint8_t *source;
  uint8_t *destination = data;
  uint32_t y;
  int32_t result;
  bool success = false;

  if (!vmsvga3d_dxvk_ready(dxvk) || surface == NULL || !surface->resident ||
      surface->resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE ||
      surface->resource == NULL || surface->bounce == NULL || data == NULL ||
      row_bytes == 0 || rows == 0 ||
      !vmsvga3d_dxvk_get_method(
          surface->resource, VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_LOCK_RECT,
          &lock_rect, sizeof(lock_rect)) ||
      !vmsvga3d_dxvk_get_method(
          surface->resource, VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_UNLOCK_RECT,
          &unlock_rect, sizeof(unlock_rect))) {
    return false;
  }

  lock_texture = surface->resource;
  if (surface->usage & VMSVGA3D_DXVK_D3DUSAGE_RENDERTARGET) {
    if (!vmsvga3d_dxvk_get_method(
            surface->resource,
            VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_GET_SURFACE_LEVEL,
            &get_surface_level, sizeof(get_surface_level)) ||
        !vmsvga3d_dxvk_get_method(
            dxvk->device,
            VMSVGA3D_DXVK_IDIRECT3DDEVICE9_GET_RENDER_TARGET_DATA,
            &get_render_target_data, sizeof(get_render_target_data)) ||
        !vmsvga3d_dxvk_get_method(
            surface->bounce, VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_LOCK_RECT,
            &lock_rect, sizeof(lock_rect)) ||
        !vmsvga3d_dxvk_get_method(
            surface->bounce, VMSVGA3D_DXVK_IDIRECT3DTEXTURE9_UNLOCK_RECT,
            &unlock_rect, sizeof(unlock_rect))) {
      return false;
    }
    result = get_surface_level(surface->resource, level, &source_surface);
    if (!vmsvga3d_dxvk_succeeded(result) || source_surface == NULL) {
      goto out;
    }
    result = get_surface_level(surface->bounce, level, &bounce_surface);
    if (!vmsvga3d_dxvk_succeeded(result) || bounce_surface == NULL) {
      goto out;
    }
    result = get_render_target_data(dxvk->device, source_surface,
                                    bounce_surface);
    if (!vmsvga3d_dxvk_succeeded(result)) {
      goto out;
    }
    lock_texture = surface->bounce;
  }
  result = lock_rect(lock_texture, level, &locked, NULL,
                     VMSVGA3D_DXVK_D3DLOCK_READONLY);
  if (!vmsvga3d_dxvk_succeeded(result) || locked.bits == NULL ||
      locked.pitch < 0 || (uint32_t)locked.pitch < row_bytes) {
    if (vmsvga3d_dxvk_succeeded(result)) {
      unlock_rect(lock_texture, level);
    }
    goto out;
  }
  source = locked.bits;
  for (y = 0; y < rows; y++) {
    memcpy(destination + (size_t)y * row_bytes,
           source + (size_t)y * (uint32_t)locked.pitch, row_bytes);
  }
  result = unlock_rect(lock_texture, level);
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
