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

#include "include/vmware_vga_dxvk_wsi.h"

struct vmsvga3d_dxvk_wsi_s {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
};

#define VMSVGA3D_DXVK_WSI_MAGIC 0x56575349u
#define VMSVGA3D_DXVK_DEFAULT_WIDTH 640
#define VMSVGA3D_DXVK_DEFAULT_HEIGHT 480
#define VMSVGA3D_DXVK_WSI_ELF_SUPPORTED 0

/*
 * DXVK Native currently requires one of its window-system integrations even
 * when the caller only needs an off-screen renderer.  Its SDL2 integration is
 * loaded by SONAME and resolves a small, fixed ABI with dlsym().  On Linux ELF
 * hosts we satisfy that ABI without SDL: the functions below are normal QEMU
 * code, while a tiny runtime-generated, read-only ELF object exposes absolute
 * symbols that point back at those functions.  The object contains no machine
 * code and is loaded from a sealed memfd before DXVK is initialized.
 *
 * Keep this implementation host-specific.  Non-Linux builds, including QEMU
 * on Windows, compile only the unavailable stubs at the bottom of this file.
 */
#if defined(CONFIG_LINUX) && defined(__ELF__)

#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>

#include "qemu/memfd.h"

#define VMSVGA3D_DXVK_SDL_SONAME "libSDL2-2.0.so.0"
#define VMSVGA3D_DXVK_VULKAN_SONAME "libvulkan.so.1"
#define VMSVGA3D_DXVK_VK_KHR_SURFACE "VK_KHR_surface"
#define VMSVGA3D_DXVK_VK_EXT_HEADLESS_SURFACE "VK_EXT_headless_surface"
#define VMSVGA3D_DXVK_VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT \
  1000256000u
#define VMSVGA3D_DXVK_SDL_PIXELFORMAT_32BPP 0x10002004u
#define VMSVGA3D_DXVK_REFRESH_RATE 60

#if UINTPTR_MAX == UINT64_MAX
typedef Elf64_Ehdr VMSVGA3DElfEhdr;
typedef Elf64_Phdr VMSVGA3DElfPhdr;
typedef Elf64_Dyn VMSVGA3DElfDyn;
typedef Elf64_Sym VMSVGA3DElfSym;
typedef Elf64_Addr VMSVGA3DElfAddr;
#define VMSVGA3D_ELF_CLASS ELFCLASS64
#define VMSVGA3D_ELF_ST_INFO ELF64_ST_INFO
#define VMSVGA3D_DXVK_WSI_ELF_WORD_SUPPORTED 1
#elif UINTPTR_MAX == UINT32_MAX
typedef Elf32_Ehdr VMSVGA3DElfEhdr;
typedef Elf32_Phdr VMSVGA3DElfPhdr;
typedef Elf32_Dyn VMSVGA3DElfDyn;
typedef Elf32_Sym VMSVGA3DElfSym;
typedef Elf32_Addr VMSVGA3DElfAddr;
#define VMSVGA3D_ELF_CLASS ELFCLASS32
#define VMSVGA3D_ELF_ST_INFO ELF32_ST_INFO
#define VMSVGA3D_DXVK_WSI_ELF_WORD_SUPPORTED 1
#else
#define VMSVGA3D_DXVK_WSI_ELF_WORD_SUPPORTED 0
#endif

#if defined(__x86_64__)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_X86_64
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#elif defined(__i386__)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_386
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#elif defined(__aarch64__)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_AARCH64
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#elif defined(__arm__)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_ARM
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#elif defined(__powerpc64__)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_PPC64
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#elif defined(__powerpc__)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_PPC
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#elif defined(__riscv)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_RISCV
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#elif defined(__s390x__)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_S390
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#elif defined(__loongarch64) && defined(EM_LOONGARCH)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_LOONGARCH
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#elif defined(__mips__)
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE EM_MIPS
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 1
#else
#define VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED 0
#endif

#if VMSVGA3D_DXVK_WSI_ELF_WORD_SUPPORTED && \
    VMSVGA3D_DXVK_WSI_ELF_MACHINE_SUPPORTED
#undef VMSVGA3D_DXVK_WSI_ELF_SUPPORTED
#define VMSVGA3D_DXVK_WSI_ELF_SUPPORTED 1
#endif

#if VMSVGA3D_DXVK_WSI_ELF_SUPPORTED

typedef struct vmsvga3d_dxvk_sdl_rect_s {
    int x;
    int y;
    int w;
    int h;
} VMSVGA3DDxvkSdlRect;

typedef struct vmsvga3d_dxvk_sdl_display_mode_s {
    uint32_t format;
    int w;
    int h;
    int refresh_rate;
    void *driverdata;
} VMSVGA3DDxvkSdlDisplayMode;

typedef void *VMSVGA3DDxvkVkInstance;
typedef uint64_t VMSVGA3DDxvkVkSurface;
typedef int32_t VMSVGA3DDxvkVkResult;

typedef struct vmsvga3d_dxvk_vk_headless_surface_create_info_s {
    uint32_t sType;
    const void *pNext;
    uint32_t flags;
} VMSVGA3DDxvkVkHeadlessSurfaceCreateInfo;

typedef void (*VMSVGA3DDxvkVkVoidFunction)(void);
typedef VMSVGA3DDxvkVkVoidFunction (*VMSVGA3DDxvkVkGetInstanceProcAddr)(
    VMSVGA3DDxvkVkInstance instance, const char *name);
typedef VMSVGA3DDxvkVkResult (*VMSVGA3DDxvkVkCreateHeadlessSurface)(
    VMSVGA3DDxvkVkInstance instance,
    const VMSVGA3DDxvkVkHeadlessSurfaceCreateInfo *create_info,
    const void *allocator, VMSVGA3DDxvkVkSurface *surface);

typedef struct vmsvga3d_dxvk_wsi_symbol_s {
    const char *name;
    void (*function)(void);
} VMSVGA3DDxvkWsiSymbol;

static GMutex vmsvga3d_dxvk_wsi_lock;
static void *vmsvga3d_dxvk_wsi_shim;
static void *vmsvga3d_dxvk_wsi_vulkan;
static VMSVGA3DDxvkVkGetInstanceProcAddr vmsvga3d_dxvk_vk_get_proc_addr;
static unsigned int vmsvga3d_dxvk_wsi_users;
static uint32_t vmsvga3d_dxvk_display_width = VMSVGA3D_DXVK_DEFAULT_WIDTH;
static uint32_t vmsvga3d_dxvk_display_height = VMSVGA3D_DXVK_DEFAULT_HEIGHT;
static const char *vmsvga3d_dxvk_wsi_error = "";

static bool vmsvga3d_dxvk_wsi_valid_window(const VMSVGA3DDxvkWsi *window)
{
    return window != NULL && window->magic == VMSVGA3D_DXVK_WSI_MAGIC;
}

static void vmsvga3d_dxvk_wsi_set_error(const char *error)
{
    vmsvga3d_dxvk_wsi_error = error != NULL ? error : "";
}

static void vmsvga3d_dxvk_sdl_fill_mode(VMSVGA3DDxvkSdlDisplayMode *mode,
                                         int width, int height,
                                         int refresh_rate)
{
    mode->format = VMSVGA3D_DXVK_SDL_PIXELFORMAT_32BPP;
    mode->w = width > 0 ? width : (int)vmsvga3d_dxvk_display_width;
    mode->h = height > 0 ? height : (int)vmsvga3d_dxvk_display_height;
    mode->refresh_rate = refresh_rate > 0
        ? refresh_rate : VMSVGA3D_DXVK_REFRESH_RATE;
    mode->driverdata = NULL;
}

static VMSVGA3DDxvkSdlDisplayMode *vmsvga3d_dxvk_sdl_get_closest_display_mode(
    int display_index, const VMSVGA3DDxvkSdlDisplayMode *wanted,
    VMSVGA3DDxvkSdlDisplayMode *closest)
{
    if (display_index != 0 || wanted == NULL || closest == NULL) {
        vmsvga3d_dxvk_wsi_set_error("invalid synthetic display mode request");
        return NULL;
    }
 
    vmsvga3d_dxvk_sdl_fill_mode(closest, wanted->w, wanted->h,
                                wanted->refresh_rate);
    if (wanted->format != 0) {
        closest->format = wanted->format;
    }
 
    return closest;
}

static int vmsvga3d_dxvk_sdl_get_current_display_mode(
    int display_index, VMSVGA3DDxvkSdlDisplayMode *mode)
{
    if (display_index != 0 || mode == NULL) {
        vmsvga3d_dxvk_wsi_set_error("invalid synthetic display");
        return -1;
    }
 
    vmsvga3d_dxvk_sdl_fill_mode(mode, 0, 0, 0);
 
    return 0;
}

static int vmsvga3d_dxvk_sdl_get_desktop_display_mode(
    int display_index, VMSVGA3DDxvkSdlDisplayMode *mode)
{
    return vmsvga3d_dxvk_sdl_get_current_display_mode(display_index, mode);
}

static int vmsvga3d_dxvk_sdl_get_display_bounds(int display_index,
                                                 VMSVGA3DDxvkSdlRect *rect)
{
    if (display_index != 0 || rect == NULL) {
        vmsvga3d_dxvk_wsi_set_error("invalid synthetic display");
        return -1;
    }
 
    rect->x = 0;
    rect->y = 0;
    rect->w = (int)vmsvga3d_dxvk_display_width;
    rect->h = (int)vmsvga3d_dxvk_display_height;
 
    return 0;
}

static int vmsvga3d_dxvk_sdl_get_display_mode(
    int display_index, int mode_index, VMSVGA3DDxvkSdlDisplayMode *mode)
{
    if (mode_index != 0) {
        return -1;
    }
 
    return vmsvga3d_dxvk_sdl_get_current_display_mode(display_index, mode);
}

static const char *vmsvga3d_dxvk_sdl_get_error(void)
{
    return vmsvga3d_dxvk_wsi_error;
}

static int vmsvga3d_dxvk_sdl_get_num_video_displays(void)
{
    return 1;
}

static int vmsvga3d_dxvk_sdl_get_window_display_index(void *window)
{
    if (!vmsvga3d_dxvk_wsi_valid_window(window)) {
        vmsvga3d_dxvk_wsi_set_error("invalid synthetic window");
        return -1;
    }
 
    return 0;
}

static int vmsvga3d_dxvk_sdl_set_window_display_mode(
    void *window, const VMSVGA3DDxvkSdlDisplayMode *mode)
{
    VMSVGA3DDxvkWsi *wsi = window;

    if (!vmsvga3d_dxvk_wsi_valid_window(wsi) || mode == NULL) {
        vmsvga3d_dxvk_wsi_set_error("invalid synthetic window mode");
        return -1;
    }
 
    vmsvga3d_dxvk_wsi_resize(
        wsi, mode->w > 0 ? (uint32_t)mode->w : wsi->width,
        mode->h > 0 ? (uint32_t)mode->h : wsi->height);
 
    return 0;
}

static int vmsvga3d_dxvk_sdl_set_window_fullscreen(void *window,
                                                    uint32_t flags)
{
    if (!vmsvga3d_dxvk_wsi_valid_window(window)) {
        vmsvga3d_dxvk_wsi_set_error("invalid synthetic window");
        return -1;
    }
 
    (void)flags;
    return 0;
}

static uint32_t vmsvga3d_dxvk_sdl_get_window_flags(void *window)
{
    if (!vmsvga3d_dxvk_wsi_valid_window(window)) {
        vmsvga3d_dxvk_wsi_set_error("invalid synthetic window");
    }
 
    return 0;
}

static void vmsvga3d_dxvk_sdl_get_window_size(void *window, int *width,
                                               int *height)
{
    VMSVGA3DDxvkWsi *wsi = window;

    if (!vmsvga3d_dxvk_wsi_valid_window(wsi)) {
        vmsvga3d_dxvk_wsi_set_error("invalid synthetic window");
        if (width != NULL) {
            *width = 0;
        }
        if (height != NULL) {
            *height = 0;
        }
        return;
    }
 
    if (width != NULL) {
        *width = (int)wsi->width;
    }
 
    if (height != NULL) {
        *height = (int)wsi->height;
    }
}

static void vmsvga3d_dxvk_sdl_set_window_size(void *window, int width,
                                               int height)
{
    VMSVGA3DDxvkWsi *wsi = window;

    if (!vmsvga3d_dxvk_wsi_valid_window(wsi)) {
        vmsvga3d_dxvk_wsi_set_error("invalid synthetic window");
        return;
    }
 
    vmsvga3d_dxvk_wsi_resize(
        wsi, width > 0 ? (uint32_t)width : wsi->width,
        height > 0 ? (uint32_t)height : wsi->height);
}

static int vmsvga3d_dxvk_sdl_vulkan_load_library(const char *path)
{
    void *get_proc_addr;

    if (path != NULL && path[0] != '\0') {
        vmsvga3d_dxvk_wsi_set_error("custom Vulkan loader paths are unsupported");
        return -1;
    }

    g_mutex_lock(&vmsvga3d_dxvk_wsi_lock);
    if (vmsvga3d_dxvk_wsi_vulkan == NULL) {
        vmsvga3d_dxvk_wsi_vulkan =
            dlopen(VMSVGA3D_DXVK_VULKAN_SONAME, RTLD_NOW | RTLD_LOCAL);
        if (vmsvga3d_dxvk_wsi_vulkan == NULL) {
            g_mutex_unlock(&vmsvga3d_dxvk_wsi_lock);
            vmsvga3d_dxvk_wsi_set_error("failed to load the Vulkan loader");
            return -1;
        }
        get_proc_addr = dlsym(vmsvga3d_dxvk_wsi_vulkan,
                              "vkGetInstanceProcAddr");
        memcpy(&vmsvga3d_dxvk_vk_get_proc_addr, &get_proc_addr,
               sizeof(vmsvga3d_dxvk_vk_get_proc_addr));
        if (vmsvga3d_dxvk_vk_get_proc_addr == NULL) {
            dlclose(vmsvga3d_dxvk_wsi_vulkan);
            vmsvga3d_dxvk_wsi_vulkan = NULL;
            g_mutex_unlock(&vmsvga3d_dxvk_wsi_lock);
            vmsvga3d_dxvk_wsi_set_error("Vulkan loader has no vkGetInstanceProcAddr");
            return -1;
        }
    }
 
    g_mutex_unlock(&vmsvga3d_dxvk_wsi_lock);
    return 0;
}

static int vmsvga3d_dxvk_sdl_vulkan_get_instance_extensions(
    void *window, unsigned int *count, const char **names)
{
    static const char *extensions[] = {
        VMSVGA3D_DXVK_VK_KHR_SURFACE,
        VMSVGA3D_DXVK_VK_EXT_HEADLESS_SURFACE,
    };
 
    const unsigned int extension_count = G_N_ELEMENTS(extensions);

    (void)window;
    if (count == NULL) {
        vmsvga3d_dxvk_wsi_set_error("Vulkan extension count is NULL");
        return 0;
    }
 
    if (names == NULL) {
        *count = extension_count;
        return 1;
    }
 
    if (*count < extension_count) {
        *count = extension_count;
        vmsvga3d_dxvk_wsi_set_error("Vulkan extension array is too small");
        return 0;
    }
 
    memcpy(names, extensions, sizeof(extensions));
    *count = extension_count;
 
    return 1;
}

static int vmsvga3d_dxvk_sdl_vulkan_create_surface(
    void *window, VMSVGA3DDxvkVkInstance instance,
    VMSVGA3DDxvkVkSurface *surface)
{
    VMSVGA3DDxvkVkHeadlessSurfaceCreateInfo create_info = {
        .sType = VMSVGA3D_DXVK_VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
    };
    VMSVGA3DDxvkVkCreateHeadlessSurface create_surface = NULL;
    VMSVGA3DDxvkVkVoidFunction create_surface_ptr;
    VMSVGA3DDxvkVkResult result;

    if (!vmsvga3d_dxvk_wsi_valid_window(window) || instance == NULL ||
        surface == NULL) {
        vmsvga3d_dxvk_wsi_set_error("invalid headless Vulkan surface request");
        return 0;
    }
 
    if (vmsvga3d_dxvk_vk_get_proc_addr == NULL &&
        vmsvga3d_dxvk_sdl_vulkan_load_library(NULL) != 0) {
        return 0;
    }
 
    create_surface_ptr = vmsvga3d_dxvk_vk_get_proc_addr(
        instance, "vkCreateHeadlessSurfaceEXT");
 
    memcpy(&create_surface, &create_surface_ptr, sizeof(create_surface));
    if (create_surface == NULL) {
        vmsvga3d_dxvk_wsi_set_error("Vulkan device has no VK_EXT_headless_surface");
        return 0;
    }
 
    result = create_surface(instance, &create_info, NULL, surface);
    if (result != 0) {
        vmsvga3d_dxvk_wsi_set_error("vkCreateHeadlessSurfaceEXT failed");
        return 0;
    }
 
    return 1;
}

static const VMSVGA3DDxvkWsiSymbol vmsvga3d_dxvk_wsi_symbols[] = {
    { "SDL_GetClosestDisplayMode",
        (void (*)(void))vmsvga3d_dxvk_sdl_get_closest_display_mode },
    { "SDL_GetCurrentDisplayMode",
        (void (*)(void))vmsvga3d_dxvk_sdl_get_current_display_mode },
    { "SDL_GetDesktopDisplayMode",
        (void (*)(void))vmsvga3d_dxvk_sdl_get_desktop_display_mode },
    { "SDL_GetDisplayBounds",
        (void (*)(void))vmsvga3d_dxvk_sdl_get_display_bounds },
    { "SDL_GetDisplayMode",
        (void (*)(void))vmsvga3d_dxvk_sdl_get_display_mode },
    { "SDL_GetError", (void (*)(void))vmsvga3d_dxvk_sdl_get_error },
    { "SDL_GetNumVideoDisplays",
        (void (*)(void))vmsvga3d_dxvk_sdl_get_num_video_displays },
    { "SDL_GetWindowDisplayIndex",
        (void (*)(void))vmsvga3d_dxvk_sdl_get_window_display_index },
    { "SDL_SetWindowDisplayMode",
        (void (*)(void))vmsvga3d_dxvk_sdl_set_window_display_mode },
    { "SDL_SetWindowFullscreen",
        (void (*)(void))vmsvga3d_dxvk_sdl_set_window_fullscreen },
    { "SDL_GetWindowFlags",
        (void (*)(void))vmsvga3d_dxvk_sdl_get_window_flags },
    { "SDL_GetWindowSize",
        (void (*)(void))vmsvga3d_dxvk_sdl_get_window_size },
    { "SDL_SetWindowSize",
        (void (*)(void))vmsvga3d_dxvk_sdl_set_window_size },
    { "SDL_Vulkan_CreateSurface",
        (void (*)(void))vmsvga3d_dxvk_sdl_vulkan_create_surface },
    { "SDL_Vulkan_GetInstanceExtensions",
        (void (*)(void))vmsvga3d_dxvk_sdl_vulkan_get_instance_extensions },
    { "SDL_Vulkan_LoadLibrary",
        (void (*)(void))vmsvga3d_dxvk_sdl_vulkan_load_library },
};

static size_t vmsvga3d_dxvk_wsi_align(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static VMSVGA3DElfAddr vmsvga3d_dxvk_wsi_function_address(
    void (*function)(void))
{
    VMSVGA3DElfAddr address = 0;

    QEMU_BUILD_BUG_ON(sizeof(function) > sizeof(address));
    memcpy(&address, &function, sizeof(function));
 
    return address;
}

static bool vmsvga3d_dxvk_wsi_write_full(int fd, const uint8_t *data,
                                          size_t size)
{
    size_t offset = 0;

    while (offset < size) {
        ssize_t written = pwrite(fd, data + offset, size - offset, offset);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        offset += written;
    }
 
    return true;
}

static bool vmsvga3d_dxvk_wsi_build_elf(uint8_t **data, size_t *size,
                                         Error **errp)
{
    const size_t symbol_count = G_N_ELEMENTS(vmsvga3d_dxvk_wsi_symbols);
    const size_t dynsym_count = symbol_count + 1;
    const size_t dynamic_count = 7;
    const size_t phdr_count = 3;
    size_t phdr_offset = sizeof(VMSVGA3DElfEhdr);
    size_t dynamic_offset;
    size_t dynsym_offset;
    size_t dynstr_offset;
    size_t hash_offset;
    size_t dynstr_size = 1 + sizeof(VMSVGA3D_DXVK_SDL_SONAME);
    size_t hash_size;
    size_t file_size;
    size_t soname_offset = 1;
    size_t string_offset;
    uint8_t *image;
    VMSVGA3DElfEhdr *ehdr;
    VMSVGA3DElfPhdr *phdr;
    VMSVGA3DElfDyn *dynamic;
    VMSVGA3DElfSym *dynsym;
    char *dynstr;
    uint32_t *hash;
    size_t i;

    for (i = 0; i < symbol_count; i++) {
        dynstr_size += strlen(vmsvga3d_dxvk_wsi_symbols[i].name) + 1;
    }
 
    dynamic_offset = vmsvga3d_dxvk_wsi_align(
        phdr_offset + phdr_count * sizeof(VMSVGA3DElfPhdr),
        _Alignof(VMSVGA3DElfDyn));
    dynsym_offset = vmsvga3d_dxvk_wsi_align(
        dynamic_offset + dynamic_count * sizeof(VMSVGA3DElfDyn),
        _Alignof(VMSVGA3DElfSym));
    dynstr_offset = dynsym_offset + dynsym_count * sizeof(VMSVGA3DElfSym);
    hash_offset = vmsvga3d_dxvk_wsi_align(dynstr_offset + dynstr_size,
                                          sizeof(uint32_t));
    hash_size = (2 + 1 + dynsym_count) * sizeof(uint32_t);
    file_size = hash_offset + hash_size;

    image = g_try_malloc0(file_size);
    if (image == NULL) {
        error_setg(errp, "failed to allocate DXVK WSI ELF image");
        return false;
    }

    ehdr = (VMSVGA3DElfEhdr *)image;
    memcpy(ehdr->e_ident, ELFMAG, SELFMAG);
    ehdr->e_ident[EI_CLASS] = VMSVGA3D_ELF_CLASS;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
#else
    ehdr->e_ident[EI_DATA] = ELFDATA2MSB;
#endif
    ehdr->e_ident[EI_VERSION] = EV_CURRENT;
    ehdr->e_ident[EI_OSABI] = ELFOSABI_SYSV;
    ehdr->e_type = ET_DYN;
    ehdr->e_machine = VMSVGA3D_DXVK_WSI_ELF_MACHINE;
    ehdr->e_version = EV_CURRENT;
    ehdr->e_ehsize = sizeof(*ehdr);
    ehdr->e_phoff = phdr_offset;
    ehdr->e_phentsize = sizeof(VMSVGA3DElfPhdr);
    ehdr->e_phnum = phdr_count;

    phdr = (VMSVGA3DElfPhdr *)(image + phdr_offset);
    phdr[0].p_type = PT_LOAD;
    phdr[0].p_offset = 0;
    phdr[0].p_vaddr = 0;
    phdr[0].p_paddr = 0;
    phdr[0].p_filesz = file_size;
    phdr[0].p_memsz = file_size;
    phdr[0].p_flags = PF_R;
    phdr[0].p_align = 4096;

    phdr[1].p_type = PT_DYNAMIC;
    phdr[1].p_offset = dynamic_offset;
    phdr[1].p_vaddr = dynamic_offset;
    phdr[1].p_paddr = dynamic_offset;
    phdr[1].p_filesz = dynamic_count * sizeof(VMSVGA3DElfDyn);
    phdr[1].p_memsz = phdr[1].p_filesz;
    phdr[1].p_flags = PF_R;
    phdr[1].p_align = _Alignof(VMSVGA3DElfDyn);

    phdr[2].p_type = PT_GNU_STACK;
    phdr[2].p_flags = PF_R | PF_W;
    phdr[2].p_align = sizeof(void *);

    dynstr = (char *)(image + dynstr_offset);
    memcpy(dynstr + soname_offset, VMSVGA3D_DXVK_SDL_SONAME,
           sizeof(VMSVGA3D_DXVK_SDL_SONAME));
    string_offset = soname_offset + sizeof(VMSVGA3D_DXVK_SDL_SONAME);

    dynsym = (VMSVGA3DElfSym *)(image + dynsym_offset);
    for (i = 0; i < symbol_count; i++) {
        size_t name_size = strlen(vmsvga3d_dxvk_wsi_symbols[i].name) + 1;

        memcpy(dynstr + string_offset, vmsvga3d_dxvk_wsi_symbols[i].name,
               name_size);
        dynsym[i + 1].st_name = string_offset;
        dynsym[i + 1].st_info = VMSVGA3D_ELF_ST_INFO(STB_GLOBAL, STT_FUNC);
        dynsym[i + 1].st_other = STV_DEFAULT;
        dynsym[i + 1].st_shndx = SHN_ABS;
        dynsym[i + 1].st_value = vmsvga3d_dxvk_wsi_function_address(
            vmsvga3d_dxvk_wsi_symbols[i].function);
        string_offset += name_size;
    }

    hash = (uint32_t *)(image + hash_offset);
    hash[0] = 1;
    hash[1] = dynsym_count;
    hash[2] = symbol_count != 0 ? 1 : 0;
    for (i = 1; i < dynsym_count; i++) {
        hash[2 + 1 + i] = i + 1 < dynsym_count ? i + 1 : 0;
    }

    dynamic = (VMSVGA3DElfDyn *)(image + dynamic_offset);
    dynamic[0].d_tag = DT_HASH;
    dynamic[0].d_un.d_ptr = hash_offset;
    dynamic[1].d_tag = DT_STRTAB;
    dynamic[1].d_un.d_ptr = dynstr_offset;
    dynamic[2].d_tag = DT_SYMTAB;
    dynamic[2].d_un.d_ptr = dynsym_offset;
    dynamic[3].d_tag = DT_STRSZ;
    dynamic[3].d_un.d_val = dynstr_size;
    dynamic[4].d_tag = DT_SYMENT;
    dynamic[4].d_un.d_val = sizeof(VMSVGA3DElfSym);
    dynamic[5].d_tag = DT_SONAME;
    dynamic[5].d_un.d_val = soname_offset;
    dynamic[6].d_tag = DT_NULL;

    *data = image;
    *size = file_size;
 
    return true;
}

static bool vmsvga3d_dxvk_wsi_load_shim(Error **errp)
{
    uint8_t *image = NULL;
    size_t image_size = 0;
    char path[64];
    void *existing = NULL;
    void *shim;
    int fd;

#ifdef RTLD_NOLOAD
    existing = dlopen(VMSVGA3D_DXVK_SDL_SONAME,
                      RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    if (existing != NULL) {
        dlclose(existing);
        error_setg(errp, "DXVK headless WSI cannot replace an already loaded %s",
                   VMSVGA3D_DXVK_SDL_SONAME);
        return false;
    }
#endif

    if (!vmsvga3d_dxvk_wsi_build_elf(&image, &image_size, errp)) {
        return false;
    }
 
    fd = qemu_memfd_create("qemu-vmvga-dxvk-wsi", image_size, false, 0,
                           F_SEAL_SHRINK | F_SEAL_GROW, errp);
    if (fd < 0) {
        g_free(image);
        return false;
    }
 
    if (!vmsvga3d_dxvk_wsi_write_full(fd, image, image_size)) {
        error_setg_errno(errp, errno, "failed to write DXVK WSI memfd");
        close(fd);
        g_free(image);
        return false;
    }
 
    g_free(image);

    if (fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SEAL) < 0) {
        error_setg_errno(errp, errno, "failed to seal DXVK WSI memfd");
        close(fd);
        return false;
    }
 
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
 
    shim = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (shim == NULL) {
        error_setg(errp, "failed to load DXVK WSI memfd: %s", dlerror());
        close(fd);
        return false;
    }
 
    close(fd);
    vmsvga3d_dxvk_wsi_shim = shim;
 
    return true;
}

#endif

#endif

VMSVGA3DDxvkWsi *vmsvga3d_dxvk_wsi_create(uint32_t width, uint32_t height,
                                            Error **errp)
{
#if defined(CONFIG_LINUX) && defined(__ELF__) && VMSVGA3D_DXVK_WSI_ELF_SUPPORTED
    VMSVGA3DDxvkWsi *wsi;

    g_mutex_lock(&vmsvga3d_dxvk_wsi_lock);
    if (vmsvga3d_dxvk_wsi_users == 0 &&
        !vmsvga3d_dxvk_wsi_load_shim(errp)) {
        g_mutex_unlock(&vmsvga3d_dxvk_wsi_lock);
        return NULL;
    }
 
    vmsvga3d_dxvk_wsi_users++;
    g_mutex_unlock(&vmsvga3d_dxvk_wsi_lock);

    wsi = g_try_new0(VMSVGA3DDxvkWsi, 1);
    if (wsi == NULL) {
        error_setg(errp, "failed to allocate DXVK headless WSI state");
        g_mutex_lock(&vmsvga3d_dxvk_wsi_lock);
        if (--vmsvga3d_dxvk_wsi_users == 0) {
            dlclose(vmsvga3d_dxvk_wsi_shim);
            vmsvga3d_dxvk_wsi_shim = NULL;
        }
        g_mutex_unlock(&vmsvga3d_dxvk_wsi_lock);
        return NULL;
    }
 
    wsi->magic = VMSVGA3D_DXVK_WSI_MAGIC;
    vmsvga3d_dxvk_wsi_resize(wsi, width, height);
 
    return wsi;
#else
    (void)width;
    (void)height;
    error_setg(errp, "DXVK headless WSI is only available on supported "
                     "Linux ELF hosts");
    return NULL;
#endif
}

void vmsvga3d_dxvk_wsi_destroy(VMSVGA3DDxvkWsi *wsi)
{
    if (wsi == NULL) {
        return;
    }
 
#if defined(CONFIG_LINUX) && defined(__ELF__) && VMSVGA3D_DXVK_WSI_ELF_SUPPORTED
    wsi->magic = 0;
 
    g_free(wsi);
    g_mutex_lock(&vmsvga3d_dxvk_wsi_lock);
 
    if (vmsvga3d_dxvk_wsi_users > 0 && --vmsvga3d_dxvk_wsi_users == 0) {
        if (vmsvga3d_dxvk_wsi_vulkan != NULL) {
            dlclose(vmsvga3d_dxvk_wsi_vulkan);
            vmsvga3d_dxvk_wsi_vulkan = NULL;
            vmsvga3d_dxvk_vk_get_proc_addr = NULL;
        }
        if (vmsvga3d_dxvk_wsi_shim != NULL) {
            dlclose(vmsvga3d_dxvk_wsi_shim);
            vmsvga3d_dxvk_wsi_shim = NULL;
        }
    }
 
    g_mutex_unlock(&vmsvga3d_dxvk_wsi_lock);
#else
    g_free(wsi);
#endif
}

void vmsvga3d_dxvk_wsi_resize(VMSVGA3DDxvkWsi *wsi, uint32_t width,
                               uint32_t height)
{
    if (wsi == NULL) {
        return;
    }
 
    wsi->width = width != 0 ? width : VMSVGA3D_DXVK_DEFAULT_WIDTH;
    wsi->height = height != 0 ? height : VMSVGA3D_DXVK_DEFAULT_HEIGHT;
 
#if defined(CONFIG_LINUX) && defined(__ELF__) && VMSVGA3D_DXVK_WSI_ELF_SUPPORTED
    vmsvga3d_dxvk_display_width = wsi->width;
    vmsvga3d_dxvk_display_height = wsi->height;
#endif
}

void *vmsvga3d_dxvk_wsi_window(VMSVGA3DDxvkWsi *wsi)
{
    return wsi;
}
