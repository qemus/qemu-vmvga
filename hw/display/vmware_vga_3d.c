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

#include "include/svga3d_caps.h"
#include "include/svga3d_cmd.h"
#include "include/svga3d_devcaps.h"
#include "include/svga3d_dx.h"
#include "include/svga3d_limits.h"
#include "include/svga3d_reg.h"
#include "include/svga3d_shaderdefs.h"
#include "include/svga3d_surfacedefs.h"
#include "include/svga3d_types.h"
#include "include/vmware_vga_3d_state.h"
#include "include/vmware_vga_d3d9.h"
#include "include/vmware_vga_dxvk.h"
#include "hw/pci/pci_device.h"
#include "system/address-spaces.h"

typedef struct {
  SVGA3dSize size;
} SVGA3dCmdSize;

/* D3D9 has 8 fixed-function stages but 21 addressable sampler slots. */
#define VMSVGA3D_MAX_TEXTURE_STAGES VMSVGA3D_D3D9_MAX_TEXTURE_STAGES
#define VMSVGA3D_MAX_SAMPLERS VMSVGA3D_D3D9_MAX_SAMPLERS

typedef struct vmsvga3d_state_value_s {
  uint32_t value;
  bool valid;
} VMSVGA3DStateValue;

/* Matches the legacy SVGA3D_DEVCAP_MAX_CLIP_PLANES value we advertise. */
#define VMSVGA3D_MAX_CLIP_PLANES 6

typedef struct vmsvga3d_transform_state_s {
  float matrix[16];
  bool valid;
} VMSVGA3DTransformState;

typedef struct vmsvga3d_material_state_s {
  SVGA3dMaterial material;
  bool valid;
} VMSVGA3DMaterialState;

typedef struct vmsvga3d_light_state_s {
  SVGA3dLightData data;
  uint32_t enabled;
  bool data_valid;
  bool enabled_valid;
} VMSVGA3DLightState;

typedef struct vmsvga3d_clip_plane_state_s {
  float plane[4];
  bool valid;
} VMSVGA3DClipPlaneState;

typedef struct vmsvga3d_shader_s {
  uint32_t shid;
  SVGA3dShaderType type;
  uint32_t bytecode_size;
  uint32_t *bytecode;
} VMSVGA3DShader;

typedef struct vmsvga3d_shader_constant_s {
  uint32_t values[4];
  bool valid;
} VMSVGA3DShaderConstant;


typedef enum vmsvga3d_query_state_e {
  VMSVGA3D_QUERY_NULL = 0,
  VMSVGA3D_QUERY_SIGNALED,
  VMSVGA3D_QUERY_BUILDING,
  VMSVGA3D_QUERY_ISSUED,
} VMSVGA3DQueryState;

typedef struct vmsvga3d_query_s {
  VMSVGA3DQueryState state;
  uint32_t result;
  bool defined;
} VMSVGA3DQuery;

typedef struct vmsvga3d_context_s {
  uint32_t cid;
  SVGA3dSurfaceImageId render_targets[SVGA3D_RT_MAX];
  SVGA3dRect viewport;
  SVGA3dRect scissor;
  VMSVGA3DStateValue render_state[SVGA3D_RS_MAX];
  VMSVGA3DStateValue texture_state[VMSVGA3D_MAX_SAMPLERS][SVGA3D_TS_MAX];
  VMSVGA3DTransformState transform[SVGA3D_TRANSFORM_MAX];
  SVGA3dZRange z_range;
  VMSVGA3DMaterialState material[SVGA3D_FACE_MAX];
  VMSVGA3DLightState light[SVGA3D_NUM_LIGHTS];
  VMSVGA3DClipPlaneState clip_plane[VMSVGA3D_MAX_CLIP_PLANES];
  VMSVGA3DShader *shader[SVGA3D_NUM_SHADERTYPE_PREDX][SVGA3D_MAX_SHADERIDS];
  uint32_t bound_shader[SVGA3D_NUM_SHADERTYPE_PREDX];
  VMSVGA3DShaderConstant shader_float[SVGA3D_NUM_SHADERTYPE_PREDX][SVGA3D_CONSTREG_MAX];
  VMSVGA3DShaderConstant shader_int[SVGA3D_NUM_SHADERTYPE_PREDX][SVGA3D_CONSTINTREG_MAX];
  VMSVGA3DShaderConstant shader_bool[SVGA3D_NUM_SHADERTYPE_PREDX][SVGA3D_CONSTBOOLREG_MAX];
  VMSVGA3DQuery occlusion;
  bool viewport_valid;
  bool scissor_valid;
  bool z_range_valid;
} VMSVGA3DContext;

#define VMSVGA3D_GBO_PAGE_SHIFT 12u
#define VMSVGA3D_GBO_PAGE_SIZE (1u << VMSVGA3D_GBO_PAGE_SHIFT)
#define VMSVGA3D_GBO_MAX_SIZE (128u * 1024u * 1024u)
#define VMSVGA3D_GBO_GPA_MASK UINT64_C(0x00000fffffffffff)

typedef struct vmsvga3d_gbo_run_s {
  uint64_t gpa;
  uint32_t pages;
} VMSVGA3DGBORun;

typedef struct vmsvga3d_gbo_s {
  SVGAMobFormat format;
  PPN64 base;
  uint32_t size;
  uint32_t page_count;
  VMSVGA3DGBORun *runs;
  uint32_t run_count;
  uint8_t *host;
  bool host_backed;
} VMSVGA3DGBO;

typedef struct vmsvga3d_mob_s {
  SVGAMobId mobid;
  VMSVGA3DGBO gbo;
} VMSVGA3DMob;

typedef struct vmsvga3d_dx_cotable_s {
  SVGAMobId mobid;
  uint32_t valid_size;
  uint32_t entry_size;
  uint32_t capacity_entries;
  uint32_t valid_entries;
} VMSVGA3DDXCOTable;

typedef struct vmsvga3d_dx_context_s {
  uint32_t cid;
  uint32_t render_target_count;
  SVGADXContextMobFormat shadow;
  VMSVGA3DDXCOTable cotables[SVGA_COTABLE_MAX];
} VMSVGA3DDXContext;

#define VMSVGA3D_MAX_MIP_LEVELS 16

typedef struct vmsvga3d_surface_image_s {
  SVGA3dSize size;
  uint32_t pitch;
  uint32_t plane_size;
  uint32_t data_size;
  uint8_t *data;
} VMSVGA3DSurfaceImage;

typedef struct vmsvga3d_surface_s {
  uint32_t sid;
  SVGA3dSurface1Flags surface_flags;
  SVGA3dSurfaceFormat format;
  SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES];
  uint32_t multisample_count;
  SVGA3dTextureFilter autogen_filter;
  uint32_t mip_count;
  size_t storage_bytes;
  VMSVGA3DSurfaceImage *mips;
  VMSVGA3DDxvkSurface *dxvk_surface;
} VMSVGA3DSurface;

struct vmsvga3d_state_s {
  VMSVGA3DContext *contexts[SVGA3D_MAX_CONTEXT_IDS];
  VMSVGA3DDXContext *dx_contexts[SVGA3D_MAX_CONTEXT_IDS];
  VMSVGA3DSurface *surfaces[SVGA3D_MAX_SURFACE_IDS];
  VMSVGA3DGBO otables[SVGA_OTABLE_MAX];
  GHashTable *mobs;
  size_t surface_bytes;
  size_t shader_bytes;
};


static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_clear(
    struct vmsvga_state_s *s, const SVGA3dCmdClear *command,
    const SVGA3dRect *rects, uint32_t rect_count);
static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_present(
    struct vmsvga_state_s *s, const SVGA3dCmdPresent *command,
    const SVGA3dCopyRect *rects, uint32_t rect_count);
static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_screen_blit(
    struct vmsvga_state_s *s,
    const SVGA3dCmdBlitSurfaceToScreen *command,
    const SVGASignedRect *clips, uint32_t clip_count);

static bool vmsvga3d_shader_type_index(SVGA3dShaderType type,
                                       uint32_t *index) {
  if (type < SVGA3D_SHADERTYPE_MIN ||
      type >= SVGA3D_SHADERTYPE_PREDX_MAX) {
    return false;
  };
  *index = (uint32_t)type - SVGA3D_SHADERTYPE_MIN;
  return true;
};

static void vmsvga3d_shader_free(VMSVGA3DShader *shader) {
  if (shader == NULL) {
    return;
  };
  g_free(shader->bytecode);
  g_free(shader);
};


static void vmsvga3d_context_free(struct vmsvga3d_state_s *state,
                                  VMSVGA3DContext *context) {
  uint32_t type;
  uint32_t shid;

  if (context == NULL) {
    return;
  };
  for (type = 0; type < SVGA3D_NUM_SHADERTYPE_PREDX; type++) {
    for (shid = 0; shid < SVGA3D_MAX_SHADERIDS; shid++) {
      VMSVGA3DShader *shader = context->shader[type][shid];

      if (shader != NULL) {
        if (state != NULL && state->shader_bytes >= shader->bytecode_size) {
          state->shader_bytes -= shader->bytecode_size;
        } else if (state != NULL) {
          state->shader_bytes = 0;
        };
        vmsvga3d_shader_free(shader);
      };
    };
  };
  g_free(context);
};

static void vmsvga3d_dx_context_free(VMSVGA3DDXContext *context) {
  g_free(context);
}

static void vmsvga3d_surface_free(VMSVGA3DSurface *surface) {
  uint32_t i;

  if (surface == NULL) {
    return;
  };
  vmsvga3d_dxvk_surface_destroy(surface->dxvk_surface);
  surface->dxvk_surface = NULL;
  for (i = 0; i < surface->mip_count; i++) {
    g_free(surface->mips[i].data);
  };
  g_free(surface->mips);
  g_free(surface);
};

static struct vmsvga3d_state_s *
vmsvga3d_state_ensure(struct vmsvga_state_s *s);

static void *vmsvga3d_dx_cotable_entry_ptr(struct vmsvga_state_s *s,
                                            uint32_t cid,
                                            SVGACOTableType type,
                                            uint32_t index);

static gpointer vmsvga3d_mob_key(SVGAMobId mobid) {
  return GUINT_TO_POINTER((guint)mobid + 1u);
}

static bool vmsvga3d_guest_memory_read(struct vmsvga_state_s *s,
                                       uint64_t gpa, void *data,
                                       size_t size) {
  if (s == NULL || (size != 0 && data == NULL)) {
    return false;
  };
  /* MOB page numbers are guest physical addresses, not PCI DMA IOVAs. */
  return address_space_read(&address_space_memory, gpa,
                            MEMTXATTRS_UNSPECIFIED, data, size) == MEMTX_OK;
}

static bool vmsvga3d_guest_memory_write(struct vmsvga_state_s *s,
                                        uint64_t gpa, const void *data,
                                        size_t size) {
  if (s == NULL || (size != 0 && data == NULL)) {
    return false;
  };
  return address_space_write(&address_space_memory, gpa,
                             MEMTXATTRS_UNSPECIFIED, data, size) == MEMTX_OK;
}

static void vmsvga3d_gbo_destroy(VMSVGA3DGBO *gbo) {
  if (gbo == NULL) {
    return;
  };
  g_free(gbo->host);
  g_free(gbo->runs);
  memset(gbo, 0, sizeof(*gbo));
}

static bool vmsvga3d_gbo_add_page(VMSVGA3DGBO *gbo, uint64_t gpa) {
  VMSVGA3DGBORun *run;

  if (gbo == NULL || gbo->run_count >= gbo->page_count) {
    return false;
  };
  gpa &= VMSVGA3D_GBO_GPA_MASK;
  if (gbo->run_count != 0) {
    run = &gbo->runs[gbo->run_count - 1];
    if (gpa == run->gpa + (uint64_t)run->pages * VMSVGA3D_GBO_PAGE_SIZE) {
      run->pages++;
      return true;
    };
  };
  run = &gbo->runs[gbo->run_count++];
  run->gpa = gpa;
  run->pages = 1;
  return true;
}

static bool vmsvga3d_gbo_read_ppn_page(struct vmsvga_state_s *s,
                                       uint64_t gpa, bool ppn64,
                                       uint8_t page[VMSVGA3D_GBO_PAGE_SIZE]) {
  struct pci_vmsvga_state_s *pci_vmsvga;

  (void)ppn64;
  if (s == NULL) {
    return false;
  };
  /* GBO page-table pages are fetched through the PCI DMA address space. */
  pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
  return pci_dma_read(PCI_DEVICE(pci_vmsvga), gpa, page,
                      VMSVGA3D_GBO_PAGE_SIZE) == MEMTX_OK;
}

static uint64_t vmsvga3d_gbo_page_ppn(const uint8_t *page, uint32_t index,
                                      bool ppn64) {
  if (ppn64) {
    uint64_t value;

    memcpy(&value, page + (size_t)index * sizeof(value), sizeof(value));
    return le64_to_cpu(value);
  } else {
    uint32_t value;

    memcpy(&value, page + (size_t)index * sizeof(value), sizeof(value));
    return le32_to_cpu(value);
  };
}

static bool vmsvga3d_gbo_create(struct vmsvga_state_s *s,
                                SVGAMobFormat format, PPN64 base,
                                uint32_t size, VMSVGA3DGBO *gbo) {
  uint32_t ppns_per_page;
  uint32_t page_index = 0;
  bool ppn64;

  if (s == NULL || gbo == NULL || size == 0 ||
      size > VMSVGA3D_GBO_MAX_SIZE) {
    return false;
  };
  if (format == SVGA3D_MOBFMT_PT64_0 ||
      format == SVGA3D_MOBFMT_PT64_1 ||
      format == SVGA3D_MOBFMT_PT64_2) {
    ppn64 = true;
  } else if (format == SVGA3D_MOBFMT_PT_0 ||
             format == SVGA3D_MOBFMT_PT_1 ||
             format == SVGA3D_MOBFMT_PT_2 ||
             format == SVGA3D_MOBFMT_RANGE) {
    ppn64 = false;
  } else {
    return false;
  };

  memset(gbo, 0, sizeof(*gbo));
  gbo->format = format;
  gbo->base = base;
  gbo->size = size;
  gbo->page_count = (size + VMSVGA3D_GBO_PAGE_SIZE - 1u) /
                    VMSVGA3D_GBO_PAGE_SIZE;
  gbo->runs = g_try_new0(VMSVGA3DGBORun, gbo->page_count);
  if (gbo->runs == NULL) {
    return false;
  };

  if (format == SVGA3D_MOBFMT_RANGE) {
    gbo->runs[0].gpa = (base << VMSVGA3D_GBO_PAGE_SHIFT) &
                       VMSVGA3D_GBO_GPA_MASK;
    gbo->runs[0].pages = gbo->page_count;
    gbo->run_count = 1;
    return true;
  };

  if (format == SVGA3D_MOBFMT_PT_0 || format == SVGA3D_MOBFMT_PT64_0) {
    if (gbo->page_count != 1) {
      vmsvga3d_gbo_destroy(gbo);
      return false;
    };
    if (!vmsvga3d_gbo_add_page(
            gbo, (base << VMSVGA3D_GBO_PAGE_SHIFT) &
                     VMSVGA3D_GBO_GPA_MASK)) {
      vmsvga3d_gbo_destroy(gbo);
      return false;
    };
    return true;
  };

  ppns_per_page = VMSVGA3D_GBO_PAGE_SIZE /
                  (ppn64 ? sizeof(uint64_t) : sizeof(uint32_t));
  if (format == SVGA3D_MOBFMT_PT_1 || format == SVGA3D_MOBFMT_PT64_1) {
    uint8_t root[VMSVGA3D_GBO_PAGE_SIZE];
    uint64_t root_gpa = base << VMSVGA3D_GBO_PAGE_SHIFT;

    if (gbo->page_count > ppns_per_page ||
        !vmsvga3d_gbo_read_ppn_page(s, root_gpa, ppn64, root)) {
      vmsvga3d_gbo_destroy(gbo);
      return false;
    };
    while (page_index < gbo->page_count) {
      uint64_t ppn = vmsvga3d_gbo_page_ppn(root, page_index, ppn64);

      if (!vmsvga3d_gbo_add_page(gbo, ppn << VMSVGA3D_GBO_PAGE_SHIFT)) {
        vmsvga3d_gbo_destroy(gbo);
        return false;
      };
      page_index++;
    };
    return true;
  };

  if ((uint64_t)gbo->page_count >
      (uint64_t)ppns_per_page * ppns_per_page) {
    vmsvga3d_gbo_destroy(gbo);
    return false;
  };
  {
    uint8_t root[VMSVGA3D_GBO_PAGE_SIZE];
    uint64_t root_gpa = base << VMSVGA3D_GBO_PAGE_SHIFT;
    uint32_t level1_count =
        (gbo->page_count + ppns_per_page - 1u) / ppns_per_page;
    uint32_t level1_index;

    if (!vmsvga3d_gbo_read_ppn_page(s, root_gpa, ppn64, root)) {
      vmsvga3d_gbo_destroy(gbo);
      return false;
    };
    for (level1_index = 0; level1_index < level1_count; level1_index++) {
      uint8_t level1[VMSVGA3D_GBO_PAGE_SIZE];
      uint64_t level1_ppn =
          vmsvga3d_gbo_page_ppn(root, level1_index, ppn64);
      uint64_t level1_gpa = level1_ppn << VMSVGA3D_GBO_PAGE_SHIFT;
      uint32_t entry;

      /* The level-1 root pointer is intentionally not masked in the PT2 path. */
      if (!vmsvga3d_gbo_read_ppn_page(s, level1_gpa, ppn64, level1)) {
        vmsvga3d_gbo_destroy(gbo);
        return false;
      };
      for (entry = 0;
           entry < ppns_per_page && page_index < gbo->page_count;
           entry++, page_index++) {
        uint64_t ppn = vmsvga3d_gbo_page_ppn(level1, entry, ppn64);

        if (!vmsvga3d_gbo_add_page(gbo,
                                    ppn << VMSVGA3D_GBO_PAGE_SHIFT)) {
          vmsvga3d_gbo_destroy(gbo);
          return false;
        };
      };
    };
  }
  return page_index == gbo->page_count;
}

static bool vmsvga3d_gbo_transfer(struct vmsvga_state_s *s,
                                  VMSVGA3DGBO *gbo, uint32_t offset,
                                  const void *src, void *dst, size_t size,
                                  bool write_guest) {
  uint64_t logical = 0;
  uint64_t current = offset;
  uint32_t run_index;

  if (s == NULL || gbo == NULL || offset > gbo->size ||
      size > (size_t)gbo->size - offset ||
      (size != 0 && ((write_guest && src == NULL) ||
                     (!write_guest && dst == NULL)))) {
    return false;
  };
  for (run_index = 0; size != 0 && run_index < gbo->run_count; run_index++) {
    uint64_t run_size =
        (uint64_t)gbo->runs[run_index].pages * VMSVGA3D_GBO_PAGE_SIZE;

    if (current >= logical + run_size) {
      logical += run_size;
      continue;
    };
    while (size != 0 && current < logical + run_size) {
      uint64_t within = current - logical;
      size_t chunk = MIN(size, (size_t)(run_size - within));
      bool ok;

      if (write_guest) {
        ok = vmsvga3d_guest_memory_write(
            s, gbo->runs[run_index].gpa + within, src, chunk);
        src = (const uint8_t *)src + chunk;
      } else {
        ok = vmsvga3d_guest_memory_read(
            s, gbo->runs[run_index].gpa + within, dst, chunk);
        dst = (uint8_t *)dst + chunk;
      };
      if (!ok) {
        return false;
      };
      current += chunk;
      size -= chunk;
    };
    logical += run_size;
  };
  return size == 0;
}

static bool vmsvga3d_gbo_read(struct vmsvga_state_s *s, VMSVGA3DGBO *gbo,
                              uint32_t offset, void *data, size_t size) {
  return vmsvga3d_gbo_transfer(s, gbo, offset, NULL, data, size, false);
}

static bool vmsvga3d_gbo_write(struct vmsvga_state_s *s, VMSVGA3DGBO *gbo,
                               uint32_t offset, const void *data,
                               size_t size) {
  return vmsvga3d_gbo_transfer(s, gbo, offset, data, NULL, size, true);
}

static bool vmsvga3d_gbo_copy(struct vmsvga_state_s *s,
                              VMSVGA3DGBO *destination,
                              VMSVGA3DGBO *source, uint32_t size) {
  uint8_t buffer[VMSVGA3D_GBO_PAGE_SIZE];
  uint32_t offset = 0;

  if (destination == NULL || source == NULL ||
      size > destination->size || size > source->size) {
    return false;
  };
  while (offset < size) {
    uint32_t chunk = MIN(size - offset, VMSVGA3D_GBO_PAGE_SIZE);

    if (!vmsvga3d_gbo_read(s, source, offset, buffer, chunk) ||
        !vmsvga3d_gbo_write(s, destination, offset, buffer, chunk)) {
      return false;
    };
    offset += chunk;
  };
  return true;
}

static bool vmsvga3d_gbo_backing_create(struct vmsvga_state_s *s,
                                        VMSVGA3DGBO *gbo,
                                        uint32_t valid_size) {
  if (s == NULL || gbo == NULL) {
    return false;
  };
  if (!gbo->host_backed) {
    gbo->host = g_try_malloc0(gbo->size);
  };
  if (gbo->host == NULL) {
    return false;
  };
  valid_size = MIN(valid_size, gbo->size);
  if (!vmsvga3d_gbo_read(s, gbo, 0, gbo->host, valid_size)) {
    g_free(gbo->host);
    gbo->host = NULL;
    gbo->host_backed = false;
    return false;
  };
  gbo->host_backed = true;
  return true;
}

static void vmsvga3d_gbo_backing_delete(VMSVGA3DGBO *gbo) {
  if (gbo == NULL || !gbo->host_backed) {
    return;
  };
  g_free(gbo->host);
  gbo->host = NULL;
  gbo->host_backed = false;
}

static bool vmsvga3d_gbo_backing_write(struct vmsvga_state_s *s,
                                       VMSVGA3DGBO *gbo) {
  return gbo != NULL && gbo->host_backed &&
         vmsvga3d_gbo_write(s, gbo, 0, gbo->host, gbo->size);
}

static void *vmsvga3d_gbo_backing_ptr(VMSVGA3DGBO *gbo, uint32_t offset) {
  if (gbo == NULL || !gbo->host_backed || offset > gbo->size) {
    return NULL;
  };
  return gbo->host + offset;
}

static bool vmsvga3d_otable_index_valid(const VMSVGA3DGBO *table,
                                        uint32_t index,
                                        uint32_t entry_size) {
  return table != NULL && table->runs != NULL && entry_size != 0 &&
         index < table->size / entry_size;
}

static bool vmsvga3d_otable_read(struct vmsvga_state_s *s,
                                 SVGAOTableType type, uint32_t index,
                                 uint32_t entry_size, void *data,
                                 uint32_t data_size) {
  struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
  VMSVGA3DGBO *table;

  if (state == NULL || type >= SVGA_OTABLE_MAX || data_size > entry_size) {
    return false;
  };
  table = &state->otables[type];
  return vmsvga3d_otable_index_valid(table, index, entry_size) &&
         vmsvga3d_gbo_read(s, table, index * entry_size, data, data_size);
}

static bool vmsvga3d_otable_write(struct vmsvga_state_s *s,
                                  SVGAOTableType type, uint32_t index,
                                  uint32_t entry_size, const void *data,
                                  uint32_t data_size) {
  struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
  VMSVGA3DGBO *table;

  if (state == NULL || type >= SVGA_OTABLE_MAX || data_size > entry_size) {
    return false;
  };
  table = &state->otables[type];
  return vmsvga3d_otable_index_valid(table, index, entry_size) &&
         vmsvga3d_gbo_write(s, table, index * entry_size, data, data_size);
}

static bool vmsvga3d_otable_set_or_grow(struct vmsvga_state_s *s,
                                        SVGAOTableType type, PPN64 base,
                                        uint32_t size, uint32_t valid_size,
                                        SVGAMobFormat format, bool grow) {
  struct vmsvga3d_state_s *state = vmsvga3d_state_ensure(s);
  VMSVGA3DGBO replacement;
  VMSVGA3DGBO *current;

  if (state == NULL || type >= SVGA_OTABLE_MAX || size < valid_size) {
    return false;
  };
  current = &state->otables[type];
  if (current->size < valid_size) {
    return false;
  };
  if (size == 0) {
    vmsvga3d_gbo_destroy(current);
    return true;
  };
  if (!vmsvga3d_gbo_create(s, format, base, size, &replacement)) {
    return false;
  };
  if (grow && valid_size != 0 &&
      !vmsvga3d_gbo_copy(s, &replacement, current, valid_size)) {
    vmsvga3d_gbo_destroy(&replacement);
    return false;
  };
  vmsvga3d_gbo_destroy(current);
  *current = replacement;
  return true;
}

static void vmsvga3d_mob_free(gpointer data) {
  VMSVGA3DMob *mob = data;

  if (mob == NULL) {
    return;
  };
  vmsvga3d_gbo_destroy(&mob->gbo);
  g_free(mob);
}

static VMSVGA3DMob *vmsvga3d_mob_get(struct vmsvga_state_s *s,
                                     SVGAMobId mobid) {
  struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;

  if (state == NULL || state->mobs == NULL || mobid == SVGA3D_INVALID_ID) {
    return NULL;
  };
  return g_hash_table_lookup(state->mobs, vmsvga3d_mob_key(mobid));
}

static bool vmsvga3d_mob_define(struct vmsvga_state_s *s, SVGAMobId mobid,
                                SVGAMobFormat format, PPN64 base,
                                uint32_t size) {
  struct vmsvga3d_state_s *state = vmsvga3d_state_ensure(s);
  SVGAOTableMobEntry entry = { 0 };
  VMSVGA3DMob *mob;

  if (state == NULL || mobid == SVGA3D_INVALID_ID) {
    return false;
  };
  entry.ptDepth = cpu_to_le32(format);
  entry.sizeInBytes = cpu_to_le32(size);
  entry.base = cpu_to_le64(base);
  if (!vmsvga3d_otable_write(s, SVGA_OTABLE_MOB, mobid,
                              sizeof(SVGAOTableMobEntry), &entry,
                              sizeof(entry))) {
    return false;
  };

  mob = g_try_new0(VMSVGA3DMob, 1);
  if (mob == NULL) {
    return false;
  };
  mob->mobid = mobid;
  if (!vmsvga3d_gbo_create(s, format, base, size, &mob->gbo)) {
    g_free(mob);
    return false;
  };
  g_hash_table_replace(state->mobs, vmsvga3d_mob_key(mobid), mob);
  return true;
}

static bool vmsvga3d_mob_destroy(struct vmsvga_state_s *s,
                                 SVGAMobId mobid) {
  struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
  SVGAOTableMobEntry entry = { 0 };

  if (state == NULL || state->mobs == NULL ||
      mobid == SVGA3D_INVALID_ID) {
    return false;
  };
  (void)vmsvga3d_otable_write(s, SVGA_OTABLE_MOB, mobid,
                               sizeof(SVGAOTableMobEntry), &entry,
                               sizeof(entry));
  return g_hash_table_remove(state->mobs, vmsvga3d_mob_key(mobid));
}

static bool vmsvga3d_mob_read(struct vmsvga_state_s *s, VMSVGA3DMob *mob,
                              uint32_t offset, void *data, uint32_t size) {
  return mob != NULL &&
         vmsvga3d_gbo_read(s, &mob->gbo, offset, data, size);
}

static bool vmsvga3d_mob_write(struct vmsvga_state_s *s, VMSVGA3DMob *mob,
                               uint32_t offset, const void *data,
                               uint32_t size) {
  return mob != NULL &&
         vmsvga3d_gbo_write(s, &mob->gbo, offset, data, size);
}

static bool vmsvga3d_mob_backing_create(struct vmsvga_state_s *s,
                                        VMSVGA3DMob *mob,
                                        uint32_t valid_size) {
  return mob != NULL &&
         vmsvga3d_gbo_backing_create(s, &mob->gbo, valid_size);
}

static void vmsvga3d_mob_backing_delete(VMSVGA3DMob *mob) {
  if (mob != NULL) {
    vmsvga3d_gbo_backing_delete(&mob->gbo);
  };
}

static bool vmsvga3d_mob_backing_write(struct vmsvga_state_s *s,
                                       VMSVGA3DMob *mob) {
  return mob != NULL && vmsvga3d_gbo_backing_write(s, &mob->gbo);
}

static void *vmsvga3d_mob_backing_ptr(VMSVGA3DMob *mob, uint32_t offset) {
  return mob != NULL ? vmsvga3d_gbo_backing_ptr(&mob->gbo, offset) : NULL;
}

static struct vmsvga3d_state_s *
vmsvga3d_state_ensure(struct vmsvga_state_s *s) {
  if (s->svga3d == NULL) {
    s->svga3d = g_try_new0(struct vmsvga3d_state_s, 1);
  };
  if (s->svga3d != NULL && s->svga3d->mobs == NULL) {
    s->svga3d->mobs = g_hash_table_new_full(
        g_direct_hash, g_direct_equal, NULL, vmsvga3d_mob_free);
  };
  return s->svga3d;
};

static void vmsvga3d_renderer_realize(struct vmsvga_state_s *s) {
  Error *local_err = NULL;

  s->svga3d_capable = false;
  vmsvga3d_dxvk_destroy(s->dxvk);
  s->dxvk = vmsvga3d_dxvk_create(
      s->active_valid ? s->active_width : 0,
      s->active_valid ? s->active_height : 0, &local_err);
  if (vmsvga3d_dxvk_ready(s->dxvk)) {
    s->svga3d_capable = true;
  } else {
    vmsvga3d_dxvk_destroy(s->dxvk);
    s->dxvk = NULL;
  };
  error_free(local_err);
};

static void vmsvga3d_renderer_unrealize(struct vmsvga_state_s *s) {
  s->svga3d_capable = false;
  vmsvga3d_dxvk_destroy(s->dxvk);
  s->dxvk = NULL;
};

static void vmsvga3d_reset(struct vmsvga_state_s *s) {
  struct vmsvga3d_state_s *state = s->svga3d;
  uint32_t i;

  if (state == NULL) {
    return;
  };
  for (i = 0; i < SVGA3D_MAX_CONTEXT_IDS; i++) {
    vmsvga3d_context_free(state, state->contexts[i]);
    vmsvga3d_dx_context_free(state->dx_contexts[i]);
  };
  for (i = 0; i < SVGA3D_MAX_SURFACE_IDS; i++) {
    vmsvga3d_surface_free(state->surfaces[i]);
  };
  for (i = 0; i < SVGA_OTABLE_MAX; i++) {
    vmsvga3d_gbo_destroy(&state->otables[i]);
  };
  g_hash_table_destroy(state->mobs);
  g_free(state);
  s->svga3d = NULL;
};

static bool vmsvga3d_fifo_supported_command(uint32_t cmd) {
  switch (cmd) {
  case SVGA_3D_CMD_SURFACE_DEFINE:
  case SVGA_3D_CMD_SURFACE_DEFINE_V2:
  case SVGA_3D_CMD_SURFACE_DESTROY:
  case SVGA_3D_CMD_SURFACE_COPY:
  case SVGA_3D_CMD_SURFACE_STRETCHBLT:
  case SVGA_3D_CMD_SURFACE_DMA:
  case SVGA_3D_CMD_CONTEXT_DEFINE:
  case SVGA_3D_CMD_CONTEXT_DESTROY:
  case SVGA_3D_CMD_SETTRANSFORM:
  case SVGA_3D_CMD_SETZRANGE:
  case SVGA_3D_CMD_SETRENDERSTATE:
  case SVGA_3D_CMD_SETRENDERTARGET:
  case SVGA_3D_CMD_SETTEXTURESTATE:
  case SVGA_3D_CMD_SETMATERIAL:
  case SVGA_3D_CMD_SETLIGHTDATA:
  case SVGA_3D_CMD_SETLIGHTENABLED:
  case SVGA_3D_CMD_SETVIEWPORT:
  case SVGA_3D_CMD_SETCLIPPLANE:
  case SVGA_3D_CMD_CLEAR:
  case SVGA_3D_CMD_PRESENT:
  case SVGA_3D_CMD_PRESENT_READBACK:
  case SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN:
  case SVGA_3D_CMD_SHADER_DEFINE:
  case SVGA_3D_CMD_SHADER_DESTROY:
  case SVGA_3D_CMD_SET_SHADER:
  case SVGA_3D_CMD_SET_SHADER_CONST:
  case SVGA_3D_CMD_DRAW_PRIMITIVES:
  case SVGA_3D_CMD_SETSCISSORRECT:
  case SVGA_3D_CMD_BEGIN_QUERY:
  case SVGA_3D_CMD_END_QUERY:
  case SVGA_3D_CMD_WAIT_FOR_QUERY:
  case SVGA_3D_CMD_GENERATE_MIPMAPS:
  case SVGA_3D_CMD_SET_OTABLE_BASE:
  case SVGA_3D_CMD_SET_OTABLE_BASE64:
  case SVGA_3D_CMD_GROW_OTABLE:
  case SVGA_3D_CMD_DEFINE_GB_MOB:
  case SVGA_3D_CMD_DEFINE_GB_MOB64:
  case SVGA_3D_CMD_DESTROY_GB_MOB:
  case SVGA_3D_CMD_DX_DEFINE_CONTEXT:
  case SVGA_3D_CMD_DX_DESTROY_CONTEXT:
  case SVGA_3D_CMD_DX_BIND_CONTEXT:
  case SVGA_3D_CMD_DX_READBACK_CONTEXT:
  case SVGA_3D_CMD_DX_SET_COTABLE:
  case SVGA_3D_CMD_DX_READBACK_COTABLE:
  case SVGA_3D_CMD_DX_GROW_COTABLE:
    return true;
  default:
    return false;
  };
};

static void vmsvga3d_fifo_rewind(struct vmsvga_state_s *s, int32_t *len,
                                 uint32_t fifo_start) {
  s->fifo_stop = fifo_start;
  s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
  *len = 0;
};

static bool vmsvga3d_fifo_read_header(struct vmsvga_state_s *s,
                                      int32_t *len, uint32_t fifo_start,
                                      uint32_t *payload_size,
                                      uint32_t *payload_words,
                                      uint32_t *total_words) {
  if (*len < 2) {
    vmsvga3d_fifo_rewind(s, len, fifo_start);
    return false;
  };

  *payload_size = vmsvga_fifo_read(s);
  if ((*payload_size & (sizeof(uint32_t) - 1)) != 0) {
    vmsvga3d_fifo_rewind(s, len, fifo_start);
    return false;
  };
  *payload_words = *payload_size / sizeof(uint32_t);
  *total_words = *payload_words + 2;
  if (*len < 0 || *total_words > (uint32_t)*len) {
    vmsvga3d_fifo_rewind(s, len, fifo_start);
    return false;
  };

  return true;
};

static bool vmsvga3d_fifo_read_payload(struct vmsvga_state_s *s,
                                       int32_t *len, uint32_t fifo_start,
                                       void **payload, uint32_t *size) {
  uint32_t payload_size;
  uint32_t payload_words;
  uint32_t total_words;
  uint32_t *data = NULL;
  uint32_t i;

  *payload = NULL;
  *size = 0;
  if (!vmsvga3d_fifo_read_header(s, len, fifo_start, &payload_size,
                                  &payload_words, &total_words)) {
    return false;
  };

  if (payload_size != 0) {
    data = g_try_malloc(payload_size);
    if (data == NULL) {
      vmsvga3d_fifo_rewind(s, len, fifo_start);
      return false;
    };
    for (i = 0; i < payload_words; i++) {
      data[i] = vmsvga_fifo_read(s);
    };
  };

  *len -= (int32_t)total_words;
  *payload = data;
  *size = payload_size;
  return true;
};

static bool vmsvga3d_fifo_discard_packet(struct vmsvga_state_s *s,
                                         int32_t *len, uint32_t fifo_start) {
  uint32_t payload_size;
  uint32_t payload_words;
  uint32_t total_words;

  if (!vmsvga3d_fifo_read_header(s, len, fifo_start, &payload_size,
                                  &payload_words, &total_words)) {
    return false;
  };

  while (payload_words > 0) {
    vmsvga_fifo_read(s);
    payload_words--;
  };
  *len -= (int32_t)total_words;
  return true;
};

static bool vmsvga3d_surface_faces_valid(
    SVGA3dSurface1Flags surface_flags,
    const SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES],
    uint32_t mip_count) {
  uint32_t i;
  uint32_t levels;

  if (mip_count == 0 || face[0].numMipLevels == 0) {
    return false;
  };
  levels = face[0].numMipLevels;
  if (levels > VMSVGA3D_MAX_MIP_LEVELS) {
    return false;
  };
  if (surface_flags & SVGA3D_SURFACE_CUBEMAP) {
    if (levels > UINT32_MAX / SVGA3D_MAX_SURFACE_FACES ||
        levels * SVGA3D_MAX_SURFACE_FACES != mip_count) {
      return false;
    };
    for (i = 1; i < SVGA3D_MAX_SURFACE_FACES; i++) {
      if (face[i].numMipLevels != levels) {
        return false;
      };
    };
  } else {
    if (levels != mip_count) {
      return false;
    };
    for (i = 1; i < SVGA3D_MAX_SURFACE_FACES; i++) {
      if (face[i].numMipLevels != 0) {
        return false;
      };
    };
  };
  return true;
};

static bool vmsvga3d_surface_sizes_valid(
    SVGA3dSurface1Flags surface_flags, SVGA3dSurfaceFormat format,
    const SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES],
    const SVGA3dSize *mip_sizes, uint32_t mip_count) {
  uint32_t face_count;
  uint32_t face_index;
  uint32_t mip_index;
  uint32_t levels;
  SVGA3dSize base;
  SVGA3dSize expected;

  if (mip_count == 0) {
    return false;
  };
  levels = face[0].numMipLevels;
  face_count = surface_flags & SVGA3D_SURFACE_CUBEMAP
                   ? SVGA3D_MAX_SURFACE_FACES
                   : 1;
  base = mip_sizes[0];
  if (base.width == 0 || base.height == 0 || base.depth == 0) {
    return false;
  };
  if (format != SVGA3D_BUFFER) {
    if (surface_flags & SVGA3D_SURFACE_VOLUME) {
      if (base.width > 2048 || base.height > 2048 || base.depth > 2048) {
        return false;
      };
    } else if (surface_flags & SVGA3D_SURFACE_1D) {
      if (base.width > 16384 || base.height != 1 || base.depth != 1) {
        return false;
      };
    } else if (base.width > 16384 || base.height > 16384 || base.depth != 1) {
      return false;
    };
  };

  for (face_index = 0; face_index < face_count; face_index++) {
    uint32_t base_index = face_index * levels;

    if (mip_sizes[base_index].width != base.width ||
        mip_sizes[base_index].height != base.height ||
        mip_sizes[base_index].depth != base.depth) {
      return false;
    };
    for (mip_index = 0; mip_index < levels; mip_index++) {
      uint32_t index = base_index + mip_index;

      if (index >= mip_count) {
        return false;
      };
      expected = svga3dsurface_get_mip_size(base, mip_index);
      if (mip_sizes[index].width != expected.width ||
          mip_sizes[index].height != expected.height ||
          mip_sizes[index].depth != expected.depth) {
        return false;
      };
    };
  };
  return true;
};

static bool vmsvga3d_surface_image_layout(
    SVGA3dSurfaceFormat format, const SVGA3dSize *size,
    uint32_t multisample_count, VMSVGA3DSurfaceImage *image) {
  const struct svga3d_surface_desc *desc;
  uint64_t blocks_x;
  uint64_t blocks_y;
  uint64_t blocks_z;
  uint64_t pitch;
  uint64_t plane_size;
  uint64_t data_size;
  uint64_t samples;

  desc = svga3dsurface_get_desc(format);
  if (desc->format != format || desc->bytes_per_block == 0 ||
      desc->pitch_bytes_per_block == 0 || desc->block_size.width == 0 ||
      desc->block_size.height == 0 || desc->block_size.depth == 0) {
    return false;
  };

  blocks_x = ((uint64_t)size->width + desc->block_size.width - 1) /
             desc->block_size.width;
  blocks_y = ((uint64_t)size->height + desc->block_size.height - 1) /
             desc->block_size.height;
  blocks_z = ((uint64_t)size->depth + desc->block_size.depth - 1) /
             desc->block_size.depth;
  if (blocks_x > UINT64_MAX / desc->pitch_bytes_per_block) {
    return false;
  };
  pitch = blocks_x * desc->pitch_bytes_per_block;
  if (blocks_x > UINT64_MAX / blocks_y) {
    return false;
  };
  plane_size = blocks_x * blocks_y;
  if (plane_size > UINT64_MAX / desc->bytes_per_block) {
    return false;
  };
  plane_size *= desc->bytes_per_block;
  samples = MAX(multisample_count, 1U);
  if (plane_size > UINT64_MAX / blocks_z) {
    return false;
  };
  data_size = plane_size * blocks_z;
  if (data_size > UINT64_MAX / samples) {
    return false;
  };
  data_size *= samples;
  if (pitch == 0 || pitch > UINT32_MAX || plane_size == 0 ||
      plane_size > UINT32_MAX || data_size == 0 || data_size > UINT32_MAX) {
    return false;
  };

  image->size = *size;
  image->pitch = (uint32_t)pitch;
  image->plane_size = (uint32_t)plane_size;
  image->data_size = (uint32_t)data_size;
  return true;
};

static void vmsvga3d_surface_install(
    struct vmsvga_state_s *s, uint32_t sid,
    SVGA3dSurface1Flags surface_flags, SVGA3dSurfaceFormat format,
    const SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES],
    uint32_t multisample_count, SVGA3dTextureFilter autogen_filter,
    const SVGA3dSize *mip_sizes, uint32_t mip_count) {
  struct vmsvga3d_state_s *state;
  VMSVGA3DSurface *old_surface;
  VMSVGA3DSurface *surface;
  size_t old_bytes;
  size_t limit;
  uint64_t storage_bytes = 0;
  uint32_t i;

  if (sid >= SVGA3D_MAX_SURFACE_IDS ||
      !vmsvga3d_surface_faces_valid(surface_flags, face, mip_count) ||
      !vmsvga3d_surface_sizes_valid(surface_flags, format, face, mip_sizes,
                                    mip_count)) {
    return;
  };

  surface = g_try_new0(VMSVGA3DSurface, 1);
  if (surface == NULL) {
    return;
  };
  surface->mips = g_try_new0(VMSVGA3DSurfaceImage, mip_count);
  if (surface->mips == NULL) {
    g_free(surface);
    return;
  };
  surface->sid = sid;
  surface->surface_flags = surface_flags;
  surface->format = format;
  memcpy(surface->face, face, sizeof(surface->face));
  surface->multisample_count = multisample_count;
  surface->autogen_filter = autogen_filter;
  surface->mip_count = mip_count;

  for (i = 0; i < mip_count; i++) {
    if (!vmsvga3d_surface_image_layout(format, &mip_sizes[i],
                                       multisample_count, &surface->mips[i])) {
      vmsvga3d_surface_free(surface);
      return;
    };
    storage_bytes += surface->mips[i].data_size;
    if (storage_bytes > SIZE_MAX) {
      vmsvga3d_surface_free(surface);
      return;
    };
  };
  surface->storage_bytes = (size_t)storage_bytes;

  state = vmsvga3d_state_ensure(s);
  if (state == NULL) {
    vmsvga3d_surface_free(surface);
    return;
  };
  old_surface = state->surfaces[sid];
  old_bytes = old_surface != NULL ? old_surface->storage_bytes : 0;
  limit = vmsvga_surface_memory_size(s);
  if (surface->storage_bytes > limit || state->surface_bytes < old_bytes ||
      state->surface_bytes - old_bytes > limit - surface->storage_bytes) {
    vmsvga3d_surface_free(surface);
    return;
  };

  for (i = 0; i < mip_count; i++) {
    surface->mips[i].data = g_try_malloc0(surface->mips[i].data_size);
    if (surface->mips[i].data == NULL) {
      vmsvga3d_surface_free(surface);
      return;
    };
  };
  surface->dxvk_surface = vmsvga3d_dxvk_surface_create(s->dxvk, sid);
  if (surface->dxvk_surface == NULL) {
    vmsvga3d_surface_free(surface);
    return;
  };

  state->surface_bytes -= old_bytes;
  vmsvga3d_surface_free(old_surface);
  state->surfaces[sid] = surface;
  state->surface_bytes += surface->storage_bytes;
};

static bool vmsvga3d_handle_surface_define(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start) {
  SVGA3dCmdDefineSurface *body;
  SVGA3dSize *mip_sizes;
  void *payload;
  uint32_t size;
  uint32_t mip_bytes;
  uint32_t mip_count;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body)) {
    g_free(payload);
    return true;
  };
  mip_bytes = size - sizeof(*body);
  if (mip_bytes % sizeof(SVGA3dSize) != 0) {
    g_free(payload);
    return true;
  };
  body = payload;
  mip_sizes = (SVGA3dSize *)(body + 1);
  mip_count = mip_bytes / sizeof(SVGA3dSize);
  vmsvga3d_surface_install(s, body->sid, body->surfaceFlags, body->format,
                           body->face, 0, 0, mip_sizes, mip_count);
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_surface_define_v2(struct vmsvga_state_s *s,
                                              uint32_t cmd, int32_t *len,
                                              uint32_t fifo_start) {
  SVGA3dCmdDefineSurface_v2 *body;
  SVGA3dSize *mip_sizes;
  void *payload;
  uint32_t size;
  uint32_t mip_bytes;
  uint32_t mip_count;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body)) {
    g_free(payload);
    return true;
  };
  mip_bytes = size - sizeof(*body);
  if (mip_bytes % sizeof(SVGA3dSize) != 0) {
    g_free(payload);
    return true;
  };
  body = payload;
  mip_sizes = (SVGA3dSize *)(body + 1);
  mip_count = mip_bytes / sizeof(SVGA3dSize);
  vmsvga3d_surface_install(s, body->sid, body->surfaceFlags, body->format,
                           body->face, body->multisampleCount,
                           body->autogenFilter, mip_sizes, mip_count);
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_surface_destroy(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start) {
  struct vmsvga3d_state_s *state;
  SVGA3dCmdDestroySurface *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    state = s->svga3d;
    if (state != NULL && body->sid < SVGA3D_MAX_SURFACE_IDS) {
      VMSVGA3DSurface *surface = state->surfaces[body->sid];

      if (surface != NULL) {
        if (state->surface_bytes >= surface->storage_bytes) {
          state->surface_bytes -= surface->storage_bytes;
        } else {
          state->surface_bytes = 0;
        };
        vmsvga3d_surface_free(surface);
        state->surfaces[body->sid] = NULL;
      };
    };
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_context_define(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start) {
  SVGA3dCmdDefineContext *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_context_define(s, body->cid);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_context_destroy(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start) {
  SVGA3dCmdDestroyContext *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_context_destroy(s, body->cid);
  };
  g_free(payload);
  return true;
};


static bool vmsvga3d_surface_image(VMSVGA3DSurface *surface,
                                   const SVGA3dSurfaceImageId *image_id,
                                   VMSVGA3DSurfaceImage **image);

static VMSVGA3DContext *vmsvga3d_context(struct vmsvga_state_s *s,
                                         uint32_t cid) {
  if (s->svga3d == NULL || cid >= SVGA3D_MAX_CONTEXT_IDS) {
    return NULL;
  };
  return s->svga3d->contexts[cid];
};

static VMSVGA3DDXContext *vmsvga3d_dx_context(struct vmsvga_state_s *s,
                                               uint32_t cid) {
  if (s->svga3d == NULL || cid >= SVGA3D_MAX_CONTEXT_IDS) {
    return NULL;
  };
  return s->svga3d->dx_contexts[cid];
};

static bool vmsvga3d_handle_set_transform(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start) {
  SVGA3dCmdSetTransform *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size == sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_transform(s, body->cid, body->type, body->matrix);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_z_range(struct vmsvga_state_s *s,
                                         uint32_t cmd, int32_t *len,
                                         uint32_t fifo_start) {
  SVGA3dCmdSetZRange *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size == sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_z_range(s, body->cid, &body->zRange);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_material(struct vmsvga_state_s *s,
                                          uint32_t cmd, int32_t *len,
                                          uint32_t fifo_start) {
  SVGA3dCmdSetMaterial *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size == sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_material(s, body->cid, body->face, &body->material);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_light_data(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start) {
  SVGA3dCmdSetLightData *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size == sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_light_data(s, body->cid, body->index, &body->data);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_light_enabled(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start) {
  SVGA3dCmdSetLightEnabled *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size == sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_light_enabled(s, body->cid, body->index,
                                          body->enabled);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_clip_plane(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start) {
  SVGA3dCmdSetClipPlane *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size == sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_clip_plane(s, body->cid, body->index, body->plane);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_render_state(struct vmsvga_state_s *s,
                                              uint32_t cmd, int32_t *len,
                                              uint32_t fifo_start) {
  SVGA3dCmdSetRenderState *body;
  SVGA3dRenderState *states;
  void *payload;
  uint32_t size;
  uint32_t count;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body) ||
      (size - sizeof(*body)) % sizeof(*states) != 0) {
    g_free(payload);
    return true;
  };
  body = payload;
  states = (SVGA3dRenderState *)(body + 1);
  count = (size - sizeof(*body)) / sizeof(*states);
  (void)vmsvga3d_state_set_render_state(s, body->cid, count, states);
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_texture_state(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start) {
  SVGA3dCmdSetTextureState *body;
  SVGA3dTextureState *states;
  void *payload;
  uint32_t size;
  uint32_t count;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body) ||
      (size - sizeof(*body)) % sizeof(*states) != 0) {
    g_free(payload);
    return true;
  };
  body = payload;
  states = (SVGA3dTextureState *)(body + 1);
  count = (size - sizeof(*body)) / sizeof(*states);
  (void)vmsvga3d_state_set_texture_state(s, body->cid, count, states);
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_render_target(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start) {
  SVGA3dCmdSetRenderTarget *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_render_target(s, body->cid, body->type,
                                         &body->target);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_viewport(struct vmsvga_state_s *s,
                                          uint32_t cmd, int32_t *len,
                                          uint32_t fifo_start) {
  SVGA3dCmdSetViewport *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_viewport(s, body->cid, &body->rect);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_scissor(struct vmsvga_state_s *s,
                                         uint32_t cmd, int32_t *len,
                                         uint32_t fifo_start) {
  SVGA3dCmdSetScissorRect *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_scissor(s, body->cid, &body->rect);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_generate_mipmaps(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start) {
  SVGA3dCmdGenerateMipmaps *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size == sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_generate_mipmaps(s, body->sid, body->filter);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_query_write_result(struct vmsvga_state_s *s,
                                          const SVGAGuestPtr *guest_result,
                                          SVGA3dQueryState state,
                                          uint32_t result) {
  SVGA3dQueryResult query_result = {
      .totalSize = sizeof(SVGA3dQueryResult),
      .state = state,
      .result32 = result,
  };

  if (guest_result == NULL) {
    return false;
  };
  return vmsvga_gmr_write(s, guest_result->gmrId, guest_result->offset,
                          &query_result, sizeof(query_result));
};

static bool vmsvga3d_handle_begin_query(struct vmsvga_state_s *s,
                                        uint32_t cmd, int32_t *len,
                                        uint32_t fifo_start) {
  SVGA3dCmdBeginQuery *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_query_begin(s, body->cid, body->type);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_end_query(struct vmsvga_state_s *s,
                                      uint32_t cmd, int32_t *len,
                                      uint32_t fifo_start) {
  SVGA3dCmdEndQuery *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_query_end(s, body->cid, body->type);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_wait_for_query(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start) {
  SVGA3dCmdWaitForQuery *body;
  VMSVGA3DQueryWaitStatus status;
  void *payload;
  uint32_t size;
  uint32_t result = 0;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    status = vmsvga3d_state_query_wait(s, body->cid, body->type, &result);
    if (status == VMSVGA3D_QUERY_WAIT_READY) {
      (void)vmsvga3d_query_write_result(s, &body->guestResult,
                                        SVGA3D_QUERYSTATE_SUCCEEDED, result);
    } else {
      /* Until the host renderer is attached an issued query cannot produce a
       * GPU result.  Match the normal failed-query result path rather than
       * fabricating a visibility count. */
      (void)vmsvga3d_query_write_result(s, &body->guestResult,
                                        SVGA3D_QUERYSTATE_FAILED, 0);
    };
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_present_readback(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start) {
  VMSVGA3DD3D9PresentReadbackPlan plan;
  VMSVGA3DD3D9AccelResult accel = VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  }
  g_free(payload);
  if (size != 0 || !vmsvga3d_d3d9_present_readback_plan(&plan)) {
    return true;
  }
  accel = vmsvga3d_d3d9_runtime_present_readback(s, &plan);
  /* The CPU PRESENT/readback path already keeps the framebuffer coherent.
   * UNAVAILABLE therefore means there is nothing GPU-resident to read back.
   * FAILED is intentionally not replayed after a potentially partial GPU
   * synchronization. */
  (void)accel;
  return true;
}

static bool vmsvga3d_handle_shader_define(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start) {
  SVGA3dCmdDefineShader *body;
  void *payload;
  uint32_t size;
  uint32_t bytecode_size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size <= sizeof(*body)) {
    g_free(payload);
    return true;
  };
  body = payload;
  bytecode_size = size - sizeof(*body);
  (void)vmsvga3d_state_shader_define(
      s, body->cid, body->shid, body->type, bytecode_size,
      (const uint32_t *)(body + 1));
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_shader_destroy(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start) {
  SVGA3dCmdDestroyShader *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size == sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_shader_destroy(s, body->cid, body->shid, body->type);
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_set_shader(struct vmsvga_state_s *s,
                                        uint32_t cmd, int32_t *len,
                                        uint32_t fifo_start) {
  SVGA3dCmdSetShader *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size == sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_state_set_shader(s, body->cid, body->type, body->shid);
  };
  g_free(payload);
  return true;
};

static uint32_t vmsvga3d_shader_const_limit(SVGA3dShaderConstType ctype) {
  switch (ctype) {
  case SVGA3D_CONST_TYPE_FLOAT:
    return SVGA3D_CONSTREG_MAX;
  case SVGA3D_CONST_TYPE_INT:
    return SVGA3D_CONSTINTREG_MAX;
  case SVGA3D_CONST_TYPE_BOOL:
    return SVGA3D_CONSTBOOLREG_MAX;
  default:
    return 0;
  };
};

static VMSVGA3DShaderConstant *
vmsvga3d_shader_const_array(VMSVGA3DContext *context, uint32_t type_index,
                            SVGA3dShaderConstType ctype) {
  switch (ctype) {
  case SVGA3D_CONST_TYPE_FLOAT:
    return context->shader_float[type_index];
  case SVGA3D_CONST_TYPE_INT:
    return context->shader_int[type_index];
  case SVGA3D_CONST_TYPE_BOOL:
    return context->shader_bool[type_index];
  default:
    return NULL;
  };
};

static bool vmsvga3d_handle_set_shader_const(struct vmsvga_state_s *s,
                                              uint32_t cmd, int32_t *len,
                                              uint32_t fifo_start) {
  SVGA3dCmdSetShaderConst *body;
  void *payload;
  uint32_t size;
  uint32_t trailing;
  uint32_t count;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body)) {
    g_free(payload);
    return true;
  };
  trailing = size - sizeof(*body);
  if (trailing % sizeof(body->values) != 0) {
    g_free(payload);
    return true;
  };
  body = payload;
  count = 1 + trailing / sizeof(body->values);
  (void)vmsvga3d_state_set_shader_const(
      s, body->cid, body->reg, body->type, body->ctype, count,
      (const uint32_t (*)[4])body->values);
  g_free(payload);
  return true;
};

static bool vmsvga3d_draw_decls_valid(const SVGA3dVertexDecl *decls,
                                       uint32_t decl_count) {
  uint32_t i;

  if (decls == NULL || decl_count == 0 ||
      decl_count > SVGA3D_MAX_VERTEX_ARRAYS) {
    return false;
  };
  for (i = 0; i < decl_count; i++) {
    if (decls[i].identity.type < SVGA3D_DECLTYPE_FLOAT1 ||
        decls[i].identity.type >= SVGA3D_DECLTYPE_MAX ||
        decls[i].identity.method < SVGA3D_DECLMETHOD_DEFAULT ||
        decls[i].identity.method > SVGA3D_DECLMETHOD_LOOKUPPRESAMPLED ||
        decls[i].identity.usage < SVGA3D_DECLUSAGE_POSITION ||
        decls[i].identity.usage >= SVGA3D_DECLUSAGE_MAX ||
        decls[i].identity.usageIndex > 15) {
      return false;
    };
  };
  return true;
};

static bool vmsvga3d_draw_ranges_valid(struct vmsvga3d_state_s *state,
                                        const SVGA3dPrimitiveRange *ranges,
                                        uint32_t range_count) {
  uint32_t i;

  if (state == NULL || ranges == NULL || range_count == 0 ||
      range_count > SVGA3D_MAX_DRAW_PRIMITIVE_RANGES) {
    return false;
  };
  for (i = 0; i < range_count; i++) {
    uint32_t sid = ranges[i].indexArray.surfaceId;

    if (ranges[i].primType <= SVGA3D_PRIMITIVE_INVALID ||
        ranges[i].primType >= SVGA3D_PRIMITIVE_PREDX_MAX) {
      return false;
    };
    if (sid == SVGA3D_INVALID_ID) {
      continue;
    };
    if (sid >= SVGA3D_MAX_SURFACE_IDS || state->surfaces[sid] == NULL ||
        (ranges[i].indexWidth != sizeof(uint16_t) &&
         ranges[i].indexWidth != sizeof(uint32_t))) {
      return false;
    };
  };
  return true;
};

static bool vmsvga3d_draw_vertex_surfaces_valid(
    struct vmsvga3d_state_s *state, const SVGA3dVertexDecl *decls,
    uint32_t decl_count) {
  uint32_t i;

  if (state == NULL || decls == NULL) {
    return false;
  };
  for (i = 0; i < decl_count; i++) {
    uint32_t sid = decls[i].array.surfaceId;

    if (sid >= SVGA3D_MAX_SURFACE_IDS || state->surfaces[sid] == NULL) {
      return false;
    };
  };
  return true;
};

static bool vmsvga3d_handle_draw_primitives(struct vmsvga_state_s *s,
                                             uint32_t cmd, int32_t *len,
                                             uint32_t fifo_start) {
  SVGA3dCmdDrawPrimitives *body;
  SVGA3dVertexDecl *decls;
  SVGA3dPrimitiveRange *ranges;
  SVGA3dVertexDivisor *divisors = NULL;
  void *payload;
  uint32_t size;
  uint64_t base_size;
  uint64_t divisor_size;
  uint32_t divisor_count = 0;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body)) {
    g_free(payload);
    return true;
  };
  body = payload;
  if (body->numVertexDecls > SVGA3D_MAX_VERTEX_ARRAYS ||
      body->numRanges > SVGA3D_MAX_DRAW_PRIMITIVE_RANGES) {
    g_free(payload);
    return true;
  };
  base_size = sizeof(*body) +
              (uint64_t)body->numVertexDecls * sizeof(SVGA3dVertexDecl) +
              (uint64_t)body->numRanges * sizeof(SVGA3dPrimitiveRange);
  divisor_size =
      (uint64_t)body->numVertexDecls * sizeof(SVGA3dVertexDivisor);
  if (base_size > UINT32_MAX ||
      (size != (uint32_t)base_size &&
       (base_size + divisor_size > UINT32_MAX ||
        size != (uint32_t)(base_size + divisor_size)))) {
    g_free(payload);
    return true;
  };
  decls = (SVGA3dVertexDecl *)(body + 1);
  ranges = (SVGA3dPrimitiveRange *)(decls + body->numVertexDecls);
  if (size != (uint32_t)base_size) {
    divisors = (SVGA3dVertexDivisor *)(ranges + body->numRanges);
    divisor_count = body->numVertexDecls;
  };
  if (vmsvga3d_state_draw_primitives(
          s, body->cid, body->numVertexDecls, decls, body->numRanges, ranges,
          divisor_count, divisors)) {
    (void)vmsvga3d_d3d9_runtime_draw_primitives(
        s, body->cid, body->numVertexDecls, decls, body->numRanges, ranges,
        divisor_count, divisors);
  }
  g_free(payload);
  return true;
};

static bool vmsvga3d_clear_rect(const SVGA3dRect *rect,
                                 const SVGA3dSize *size,
                                 SVGA3dRect *clipped) {
  uint64_t end_x;
  uint64_t end_y;

  if (rect->w == 0 || rect->h == 0 || rect->x >= size->width ||
      rect->y >= size->height) {
    return false;
  };
  end_x = MIN((uint64_t)rect->x + rect->w, (uint64_t)size->width);
  end_y = MIN((uint64_t)rect->y + rect->h, (uint64_t)size->height);
  if (end_x <= rect->x || end_y <= rect->y) {
    return false;
  };
  *clipped = *rect;
  clipped->w = (uint32_t)(end_x - rect->x);
  clipped->h = (uint32_t)(end_y - rect->y);
  return true;
};

static uint64_t vmsvga3d_bit_mask(uint32_t bits) {
  if (bits >= 64) {
    return UINT64_MAX;
  };
  if (bits == 0) {
    return 0;
  };
  return (UINT64_C(1) << bits) - 1;
};

static uint64_t vmsvga3d_scale_u8(uint32_t value, uint32_t bits) {
  uint64_t mask = vmsvga3d_bit_mask(bits);

  if (bits == 0) {
    return 0;
  };
  return ((uint64_t)value * mask + 127) / 255;
};


static uint32_t vmsvga3d_load_le(const uint8_t *data, uint32_t bytes) {
  uint32_t value = 0;
  uint32_t i;

  for (i = 0; i < bytes; i++) {
    value |= (uint32_t)data[i] << (i * 8);
  };
  return value;
};

static void vmsvga3d_store_le(uint8_t *data, uint32_t bytes,
                              uint32_t value) {
  uint32_t i;

  for (i = 0; i < bytes; i++) {
    data[i] = (uint8_t)(value >> (i * 8));
  };
};

static bool vmsvga3d_clear_color_value(SVGA3dSurfaceFormat format,
                                        uint32_t color, uint32_t *value,
                                        uint32_t *bytes) {
  const struct svga3d_surface_desc *desc = svga3dsurface_get_desc(format);
  uint64_t packed = 0;

  if (desc->format != format || desc->block_size.width != 1 ||
      desc->block_size.height != 1 || desc->block_size.depth != 1 ||
      desc->bytes_per_block == 0 || desc->bytes_per_block > sizeof(*value) ||
      !(desc->block_desc & SVGA3DBLOCKDESC_COLOR) ||
      !(desc->block_desc & SVGA3DBLOCKDESC_NORM) ||
      (desc->block_desc & (SVGA3DBLOCKDESC_COMPRESSED | SVGA3DBLOCKDESC_FP |
                           SVGA3DBLOCKDESC_PLANAR_YUV |
                           SVGA3DBLOCKDESC_2PLANAR_YUV |
                           SVGA3DBLOCKDESC_3PLANAR_YUV))) {
    return false;
  };

#define VMSVGA3D_PACK_CHANNEL(component, channel) do {                        \
    uint32_t bits = desc->bitDepth.channel;                                   \
    uint32_t shift = desc->bitOffset.channel;                                 \
    if (bits > 32 || shift >= 32 || (bits != 0 && bits > 32 - shift)) {      \
      return false;                                                            \
    };                                                                         \
    packed |= vmsvga3d_scale_u8((component), bits) << shift;                  \
  } while (0)
  VMSVGA3D_PACK_CHANNEL(color & 0xff, blue);
  VMSVGA3D_PACK_CHANNEL((color >> 8) & 0xff, green);
  VMSVGA3D_PACK_CHANNEL((color >> 16) & 0xff, red);
  VMSVGA3D_PACK_CHANNEL((color >> 24) & 0xff, alpha);
#undef VMSVGA3D_PACK_CHANNEL

  *value = (uint32_t)packed;
  *bytes = desc->bytes_per_block;
  return true;
};

static bool vmsvga3d_clear_color_image(VMSVGA3DSurface *surface,
                                        VMSVGA3DSurfaceImage *image,
                                        uint32_t color,
                                        const SVGA3dRect *rects,
                                        uint32_t rect_count,
                                        bool execute) {
  SVGA3dRect full_rect;
  uint32_t value;
  uint32_t bytes;
  uint32_t i;

  if (surface->multisample_count > 1 ||
      !vmsvga3d_clear_color_value(surface->format, color, &value, &bytes)) {
    return false;
  };
  if (rect_count == 0) {
    full_rect.x = 0;
    full_rect.y = 0;
    full_rect.w = image->size.width;
    full_rect.h = image->size.height;
    rects = &full_rect;
    rect_count = 1;
  };
  for (i = 0; i < rect_count; i++) {
    SVGA3dRect rect;
    uint32_t y;
    uint32_t x;

    if (!vmsvga3d_clear_rect(&rects[i], &image->size, &rect)) {
      continue;
    };
    if (!execute) {
      continue;
    };
    for (y = 0; y < rect.h; y++) {
      uint8_t *row = image->data + (uint64_t)(rect.y + y) * image->pitch +
                     (uint64_t)rect.x * bytes;
      for (x = 0; x < rect.w; x++) {
        vmsvga3d_store_le(row + (uint64_t)x * bytes, bytes, value);
      };
    };
  };
  return true;
};

static bool vmsvga3d_clear_depth_stencil_image(
    VMSVGA3DSurface *surface, VMSVGA3DSurfaceImage *image,
    bool clear_depth, float depth, bool clear_stencil, uint32_t stencil,
    const SVGA3dRect *rects, uint32_t rect_count, bool execute) {
  const struct svga3d_surface_desc *desc =
      svga3dsurface_get_desc(surface->format);
  uint32_t bytes;
  uint32_t depth_bits;
  uint32_t depth_shift;
  uint32_t stencil_bits;
  uint32_t stencil_shift;
  uint64_t depth_mask;
  uint64_t stencil_mask;
  uint64_t depth_value = 0;
  uint64_t stencil_value = 0;
  SVGA3dRect full_rect;
  uint32_t i;

  if (surface->multisample_count > 1 || desc->format != surface->format ||
      desc->block_size.width != 1 || desc->block_size.height != 1 ||
      desc->block_size.depth != 1 || desc->bytes_per_block == 0 ||
      desc->bytes_per_block > sizeof(uint32_t) ||
      !(desc->block_desc & SVGA3DBLOCKDESC_NORM) ||
      (desc->block_desc & SVGA3DBLOCKDESC_FP)) {
    return false;
  };
  bytes = desc->bytes_per_block;
  depth_bits = desc->bitDepth.depth;
  depth_shift = desc->bitOffset.depth;
  stencil_bits = desc->bitDepth.stencil;
  stencil_shift = desc->bitOffset.stencil;
  if ((clear_depth && (!(desc->block_desc & SVGA3DBLOCKDESC_DEPTH) ||
                       depth_bits == 0)) ||
      (clear_stencil && (!(desc->block_desc & SVGA3DBLOCKDESC_STENCIL) ||
                         stencil_bits == 0)) ||
      depth_bits > 32 || depth_shift >= 32 ||
      (depth_bits != 0 && depth_bits > 32 - depth_shift) ||
      stencil_bits > 32 || stencil_shift >= 32 ||
      (stencil_bits != 0 && stencil_bits > 32 - stencil_shift)) {
    return false;
  };
  depth_mask = vmsvga3d_bit_mask(depth_bits) << depth_shift;
  stencil_mask = vmsvga3d_bit_mask(stencil_bits) << stencil_shift;
  if (clear_depth) {
    double normalized = depth;
    uint64_t maximum = vmsvga3d_bit_mask(depth_bits);

    if (!(normalized >= 0.0)) {
      normalized = 0.0;
    } else if (normalized > 1.0) {
      normalized = 1.0;
    };
    depth_value = (uint64_t)(normalized * (double)maximum + 0.5) << depth_shift;
  };
  if (clear_stencil) {
    stencil_value = ((uint64_t)stencil & vmsvga3d_bit_mask(stencil_bits))
                    << stencil_shift;
  };

  if (rect_count == 0) {
    full_rect.x = 0;
    full_rect.y = 0;
    full_rect.w = image->size.width;
    full_rect.h = image->size.height;
    rects = &full_rect;
    rect_count = 1;
  };
  for (i = 0; i < rect_count; i++) {
    SVGA3dRect rect;
    uint32_t y;
    uint32_t x;

    if (!vmsvga3d_clear_rect(&rects[i], &image->size, &rect)) {
      continue;
    };
    if (!execute) {
      continue;
    };
    for (y = 0; y < rect.h; y++) {
      uint8_t *row = image->data + (uint64_t)(rect.y + y) * image->pitch +
                     (uint64_t)rect.x * bytes;
      for (x = 0; x < rect.w; x++) {
        uint8_t *pixel = row + (uint64_t)x * bytes;
        uint32_t value = vmsvga3d_load_le(pixel, bytes);

        if (clear_depth) {
          value = (uint32_t)(((uint64_t)value & ~depth_mask) | depth_value);
        };
        if (clear_stencil) {
          value = (uint32_t)(((uint64_t)value & ~stencil_mask) |
                             stencil_value);
        };
        vmsvga3d_store_le(pixel, bytes, value);
      };
    };
  };
  return true;
};

static bool vmsvga3d_clear_target(struct vmsvga_state_s *s,
                                   VMSVGA3DContext *context,
                                   SVGA3dRenderTargetType type,
                                   uint32_t color, float depth,
                                   uint32_t stencil,
                                   const SVGA3dRect *rects,
                                   uint32_t rect_count, bool execute) {
  SVGA3dSurfaceImageId *target = &context->render_targets[type];
  VMSVGA3DSurface *surface;
  VMSVGA3DSurfaceImage *image;

  if (target->sid == SVGA3D_INVALID_ID) {
    return true;
  };
  if (s->svga3d == NULL || target->sid >= SVGA3D_MAX_SURFACE_IDS) {
    return false;
  };
  surface = s->svga3d->surfaces[target->sid];
  if (!vmsvga3d_surface_image(surface, target, &image)) {
    return false;
  };
  if (type >= SVGA3D_RT_COLOR0 && type <= SVGA3D_RT_COLOR7) {
    return vmsvga3d_clear_color_image(surface, image, color, rects,
                                      rect_count, execute);
  };
  return vmsvga3d_clear_depth_stencil_image(
      surface, image, type == SVGA3D_RT_DEPTH, depth,
      type == SVGA3D_RT_STENCIL, stencil, rects, rect_count, execute);
};

static bool vmsvga3d_handle_clear(struct vmsvga_state_s *s,
                                  uint32_t cmd, int32_t *len,
                                  uint32_t fifo_start) {
  SVGA3dCmdClear *body;
  SVGA3dRect *rects;
  void *payload;
  uint32_t size;
  uint32_t rect_bytes;
  uint32_t rect_count;
  VMSVGA3DD3D9AccelResult accel;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body)) {
    g_free(payload);
    return true;
  };
  rect_bytes = size - sizeof(*body);
  if (rect_bytes % sizeof(SVGA3dRect) != 0) {
    g_free(payload);
    return true;
  };
  body = payload;
  rects = (SVGA3dRect *)(body + 1);
  rect_count = rect_bytes / sizeof(SVGA3dRect);
  accel = vmsvga3d_d3d9_try_clear(s, body, rects, rect_count);
  if (accel == VMSVGA3D_D3D9_ACCEL_UNAVAILABLE) {
    if (vmsvga3d_state_clear(s, body->cid, body->clearFlag, body->color,
                             body->depth, body->stencil, rect_count, rects)) {
      vmsvga3d_d3d9_runtime_sync_clear_targets_from_cpu(
          s, body->cid, body->clearFlag);
    }
  };
  g_free(payload);
  return true;
};


static bool vmsvga3d_surface_image(VMSVGA3DSurface *surface,
                                   const SVGA3dSurfaceImageId *image_id,
                                   VMSVGA3DSurfaceImage **image) {
  uint32_t levels;
  uint32_t face_count;
  uint32_t index;

  if (surface == NULL || image_id == NULL || image == NULL) {
    return false;
  };
  levels = surface->face[0].numMipLevels;
  face_count = surface->surface_flags & SVGA3D_SURFACE_CUBEMAP
                   ? SVGA3D_MAX_SURFACE_FACES
                   : 1;
  if (levels == 0 || image_id->face >= face_count ||
      image_id->mipmap >= levels) {
    return false;
  };
  index = image_id->face * levels + image_id->mipmap;
  if (index >= surface->mip_count) {
    return false;
  };
  *image = &surface->mips[index];
  return true;
};

static bool vmsvga3d_clip_dma_box(const SVGA3dCopyBox *box,
                                   const SVGA3dSize *size,
                                   SVGA3dCopyBox *clipped) {
  uint64_t end_x;
  uint64_t end_y;
  uint64_t end_z;
  uint32_t delta_x;
  uint32_t delta_y;
  uint32_t delta_z;

  if (box == NULL || size == NULL || clipped == NULL || box->w == 0 ||
      box->h == 0 || box->d == 0 || box->x >= size->width ||
      box->y >= size->height || box->z >= size->depth) {
    return false;
  };
  end_x = MIN((uint64_t)box->x + box->w, (uint64_t)size->width);
  end_y = MIN((uint64_t)box->y + box->h, (uint64_t)size->height);
  end_z = MIN((uint64_t)box->z + box->d, (uint64_t)size->depth);
  if (end_x <= box->x || end_y <= box->y || end_z <= box->z) {
    return false;
  };

  *clipped = *box;
  clipped->w = (uint32_t)(end_x - box->x);
  clipped->h = (uint32_t)(end_y - box->y);
  clipped->d = (uint32_t)(end_z - box->z);
  delta_x = clipped->x - box->x;
  delta_y = clipped->y - box->y;
  delta_z = clipped->z - box->z;
  if (box->srcx > UINT32_MAX - delta_x ||
      box->srcy > UINT32_MAX - delta_y ||
      box->srcz > UINT32_MAX - delta_z) {
    return false;
  };
  clipped->srcx = box->srcx + delta_x;
  clipped->srcy = box->srcy + delta_y;
  clipped->srcz = box->srcz + delta_z;
  return true;
};

static bool vmsvga3d_u64_add_product(uint64_t *value, uint64_t count,
                                      uint64_t stride) {
  uint64_t product;

  if (count != 0 && stride > UINT64_MAX / count) {
    return false;
  };
  product = count * stride;
  if (*value > UINT64_MAX - product) {
    return false;
  };
  *value += product;
  return true;
};

static bool vmsvga3d_d3d9_clear_context_info(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t *target_width,
    uint32_t *target_height, uint32_t *active_render_target_mask) {
  VMSVGA3DContext *context;
  VMSVGA3DSurface *surface;
  SVGA3dSurfaceImageId *color0;
  uint32_t type;
  uint32_t mask = 0;

  if (target_width == NULL || target_height == NULL ||
      active_render_target_mask == NULL) {
    return false;
  }
  context = vmsvga3d_context(s, cid);
  if (context == NULL) {
    return false;
  }
  color0 = &context->render_targets[SVGA3D_RT_COLOR0];
  if (color0->sid == SVGA3D_INVALID_ID || s->svga3d == NULL ||
      color0->sid >= SVGA3D_MAX_SURFACE_IDS) {
    return false;
  }
  surface = s->svga3d->surfaces[color0->sid];
  if (surface == NULL || surface->mip_count == 0 || surface->mips == NULL) {
    return false;
  }
  for (type = 0; type < SVGA3D_RT_MAX; type++) {
    if (context->render_targets[type].sid != SVGA3D_INVALID_ID) {
      mask |= 1u << type;
    }
  }
  /* VBox intentionally sizes the temporary clear scissor from mip 0 of
   * COLOR0's surface, even if a different mip/face is currently bound. */
  *target_width = surface->mips[0].size.width;
  *target_height = surface->mips[0].size.height;
  *active_render_target_mask = mask;
  return true;
}

static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_clear(
    struct vmsvga_state_s *s, const SVGA3dCmdClear *command,
    const SVGA3dRect *rects, uint32_t rect_count) {
  VMSVGA3DD3D9ClearPlan plan;
  uint32_t target_width;
  uint32_t target_height;
  uint32_t active_render_target_mask;

  if (command == NULL ||
      !vmsvga3d_d3d9_clear_context_info(
          s, command->cid, &target_width, &target_height,
          &active_render_target_mask) ||
      !vmsvga3d_d3d9_clear_plan(
          command->clearFlag, command->color, command->depth,
          command->stencil, rect_count, target_width, target_height,
          active_render_target_mask, &plan)) {
    return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  }
  return vmsvga3d_d3d9_runtime_clear(s, command, rects, rect_count, &plan);
}

static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_present(
    struct vmsvga_state_s *s, const SVGA3dCmdPresent *command,
    const SVGA3dCopyRect *rects, uint32_t rect_count) {
  VMSVGA3DD3D9TransferSurface surface_info;
  VMSVGA3DD3D9PresentPlan plan;
  VMSVGA3DSurface *surface;
  uint32_t width;
  uint32_t height;

  if (s == NULL || command == NULL || s->svga3d == NULL ||
      command->sid >= SVGA3D_MAX_SURFACE_IDS) {
    return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  }
  surface = s->svga3d->surfaces[command->sid];
  width = vmsvga_active_width(s);
  height = vmsvga_active_height(s);
  if (surface == NULL || width == 0 || height == 0 ||
      !vmsvga3d_d3d9_runtime_surface_info(s, surface, &surface_info) ||
      !vmsvga3d_d3d9_present_plan(&surface_info, rect_count, width, height,
                                  &plan) ||
      plan.execution != VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED) {
    return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  }
  return vmsvga3d_d3d9_runtime_present(s, command, rects, rect_count,
                                        &plan);
}

static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_screen_blit(
    struct vmsvga_state_s *s,
    const SVGA3dCmdBlitSurfaceToScreen *command,
    const SVGASignedRect *clips, uint32_t clip_count) {
#ifdef CONFIG_VMSVGA_DXVK
  VMSVGA3DD3D9TransferSurface surface_info;
  VMSVGA3DD3D9ScreenBlitPlan plan;
  VMSVGA3DSurface *surface;
  int64_t source_width;
  int64_t source_height;
  int64_t destination_width;
  int64_t destination_height;
  bool scaling;

  if (s == NULL || command == NULL || s->svga3d == NULL ||
      command->srcImage.sid >= SVGA3D_MAX_SURFACE_IDS) {
    return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  }
  surface = s->svga3d->surfaces[command->srcImage.sid];
  source_width = (int64_t)command->srcRect.right - command->srcRect.left;
  source_height = (int64_t)command->srcRect.bottom - command->srcRect.top;
  destination_width =
      (int64_t)command->destRect.right - command->destRect.left;
  destination_height =
      (int64_t)command->destRect.bottom - command->destRect.top;
  if (surface == NULL || source_width <= 0 || source_height <= 0 ||
      destination_width <= 0 || destination_height <= 0 ||
      !vmsvga3d_d3d9_runtime_surface_info(s, surface, &surface_info)) {
    return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  }
  scaling = source_width != destination_width ||
            source_height != destination_height;
  if (!vmsvga3d_d3d9_screen_blit_plan(
          &surface_info, command->destScreenId, scaling, clip_count, &plan) ||
      plan.execution != VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED) {
    return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  }
  return vmsvga3d_d3d9_runtime_screen_blit(s, command, clips, clip_count,
                                            &plan);
#else
  (void)s;
  (void)command;
  (void)clips;
  (void)clip_count;
  return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
#endif
}

static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_surface_copy(
    struct vmsvga_state_s *s, const SVGA3dCmdSurfaceCopy *command,
    const SVGA3dCopyBox *boxes, uint32_t box_count,
    const VMSVGA3DD3D9SurfaceCopyPlan *plan) {
  if (plan->execution == VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED) {
    return vmsvga3d_d3d9_runtime_surface_copy(s, command, boxes, box_count,
                                              plan);
  }
  return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
}

static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_stretch_blt(
    struct vmsvga_state_s *s, const SVGA3dCmdSurfaceStretchBlt *command,
    const VMSVGA3DD3D9StretchBltPlan *plan) {
  if (plan->execution == VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED) {
    return vmsvga3d_d3d9_runtime_stretch_blt(s, command, plan);
  }
  return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
}

static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_surface_dma(
    struct vmsvga_state_s *s, const SVGA3dCmdSurfaceDMA *command,
    const SVGA3dCopyBox *boxes, uint32_t box_count, uint32_t maximum_offset,
    const VMSVGA3DD3D9DmaPlan *plan) {
  if (plan->execution == VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED) {
    return vmsvga3d_d3d9_runtime_surface_dma(s, command, boxes, box_count,
                                             maximum_offset, plan);
  }
  return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
}

static void vmsvga3d_clip_surface_copy_box(const SVGA3dCopyBox *box,
                                             const SVGA3dSize *src_size,
                                             const SVGA3dSize *dst_size,
                                             SVGA3dCopyBox *clipped) {
  uint32_t max_width;
  uint32_t max_height;
  uint32_t max_depth;

  *clipped = *box;
  if (box->w == 0 || box->h == 0 || box->d == 0 ||
      box->srcx >= src_size->width || box->srcy >= src_size->height ||
      box->srcz >= src_size->depth || box->x >= dst_size->width ||
      box->y >= dst_size->height || box->z >= dst_size->depth) {
    clipped->w = 0;
    clipped->h = 0;
    clipped->d = 0;
    return;
  };

  max_width = MIN(src_size->width - box->srcx, dst_size->width - box->x);
  max_height = MIN(src_size->height - box->srcy, dst_size->height - box->y);
  max_depth = MIN(src_size->depth - box->srcz, dst_size->depth - box->z);
  clipped->w = MIN(box->w, max_width);
  clipped->h = MIN(box->h, max_height);
  clipped->d = MIN(box->d, max_depth);
};

static bool vmsvga3d_surface_copy_box(
    VMSVGA3DSurface *src_surface, VMSVGA3DSurfaceImage *src_image,
    VMSVGA3DSurface *dst_surface, VMSVGA3DSurfaceImage *dst_image,
    const SVGA3dCopyBox *box, uint8_t *scratch, size_t scratch_size,
    bool execute, size_t *scratch_needed) {
  const struct svga3d_surface_desc *desc;
  const struct svga3d_surface_desc *dst_desc;
  SVGA3dCopyBox clipped;
  uint32_t block_width;
  uint32_t block_height;
  uint32_t block_depth;
  uint32_t bytes_per_block;
  uint64_t blocks_x;
  uint64_t blocks_y;
  uint64_t blocks_z;
  uint64_t row_bytes;
  uint64_t src_offset = 0;
  uint64_t dst_offset = 0;
  uint64_t src_end;
  uint64_t dst_end;
  uint64_t temporary_size;
  uint64_t z;
  uint64_t y;
  bool same_image;

  if (scratch_needed != NULL) {
    *scratch_needed = 0;
  };
  if (src_surface == NULL || src_image == NULL || dst_surface == NULL ||
      dst_image == NULL || box == NULL) {
    return false;
  };

  vmsvga3d_clip_surface_copy_box(box, &src_image->size, &dst_image->size,
                                 &clipped);
  if (clipped.w == 0 || clipped.h == 0 || clipped.d == 0) {
    return true;
  };
  if (src_surface->multisample_count > 1 ||
      dst_surface->multisample_count > 1) {
    return false;
  };

  desc = svga3dsurface_get_desc(src_surface->format);
  dst_desc = svga3dsurface_get_desc(dst_surface->format);
  if (desc->format != src_surface->format ||
      dst_desc->format != dst_surface->format || desc->bytes_per_block == 0 ||
      desc->pitch_bytes_per_block == 0 ||
      desc->pitch_bytes_per_block != desc->bytes_per_block ||
      dst_desc->pitch_bytes_per_block != dst_desc->bytes_per_block ||
      desc->block_size.width != dst_desc->block_size.width ||
      desc->block_size.height != dst_desc->block_size.height ||
      desc->block_size.depth != dst_desc->block_size.depth ||
      desc->bytes_per_block != dst_desc->bytes_per_block ||
      desc->block_size.width == 0 || desc->block_size.height == 0 ||
      desc->block_size.depth == 0) {
    return false;
  };
  block_width = desc->block_size.width;
  block_height = desc->block_size.height;
  block_depth = desc->block_size.depth;
  bytes_per_block = desc->bytes_per_block;

  if (clipped.srcx % block_width != 0 ||
      clipped.srcy % block_height != 0 ||
      clipped.srcz % block_depth != 0 || clipped.x % block_width != 0 ||
      clipped.y % block_height != 0 || clipped.z % block_depth != 0) {
    return false;
  };

  blocks_x = ((uint64_t)clipped.w + block_width - 1) / block_width;
  blocks_y = ((uint64_t)clipped.h + block_height - 1) / block_height;
  blocks_z = ((uint64_t)clipped.d + block_depth - 1) / block_depth;
  if (blocks_x == 0 || blocks_y == 0 || blocks_z == 0 ||
      blocks_x > UINT64_MAX / bytes_per_block) {
    return false;
  };
  row_bytes = blocks_x * bytes_per_block;
  if (row_bytes > src_image->pitch || row_bytes > dst_image->pitch) {
    return false;
  };

  if (!vmsvga3d_u64_add_product(&src_offset,
                                 clipped.srcx / block_width,
                                 bytes_per_block) ||
      !vmsvga3d_u64_add_product(&src_offset,
                                 clipped.srcy / block_height,
                                 src_image->pitch) ||
      !vmsvga3d_u64_add_product(&src_offset,
                                 clipped.srcz / block_depth,
                                 src_image->plane_size) ||
      !vmsvga3d_u64_add_product(&dst_offset, clipped.x / block_width,
                                 bytes_per_block) ||
      !vmsvga3d_u64_add_product(&dst_offset, clipped.y / block_height,
                                 dst_image->pitch) ||
      !vmsvga3d_u64_add_product(&dst_offset, clipped.z / block_depth,
                                 dst_image->plane_size)) {
    return false;
  };

  src_end = src_offset;
  dst_end = dst_offset;
  if (!vmsvga3d_u64_add_product(&src_end, blocks_z - 1,
                                 src_image->plane_size) ||
      !vmsvga3d_u64_add_product(&src_end, blocks_y - 1,
                                 src_image->pitch) ||
      src_end > UINT64_MAX - row_bytes ||
      !vmsvga3d_u64_add_product(&dst_end, blocks_z - 1,
                                 dst_image->plane_size) ||
      !vmsvga3d_u64_add_product(&dst_end, blocks_y - 1,
                                 dst_image->pitch) ||
      dst_end > UINT64_MAX - row_bytes) {
    return false;
  };
  src_end += row_bytes;
  dst_end += row_bytes;
  if (src_end > src_image->data_size || dst_end > dst_image->data_size) {
    return false;
  };

  same_image = src_image->data == dst_image->data;
  if (same_image) {
    if (blocks_y > UINT64_MAX / blocks_z ||
        blocks_y * blocks_z > UINT64_MAX / row_bytes) {
      return false;
    };
    temporary_size = blocks_y * blocks_z * row_bytes;
    if (temporary_size > SIZE_MAX) {
      return false;
    };
    if (scratch_needed != NULL) {
      *scratch_needed = (size_t)temporary_size;
    };
    if (execute && (scratch == NULL || scratch_size < temporary_size)) {
      return false;
    };
  };

  if (!execute) {
    return true;
  };

  if (same_image) {
    size_t temporary_offset = 0;

    for (z = 0; z < blocks_z; z++) {
      for (y = 0; y < blocks_y; y++) {
        uint64_t offset = src_offset + z * src_image->plane_size +
                          y * src_image->pitch;
        memcpy(scratch + temporary_offset, src_image->data + offset,
               (size_t)row_bytes);
        temporary_offset += (size_t)row_bytes;
      };
    };
    temporary_offset = 0;
    for (z = 0; z < blocks_z; z++) {
      for (y = 0; y < blocks_y; y++) {
        uint64_t offset = dst_offset + z * dst_image->plane_size +
                          y * dst_image->pitch;
        memcpy(dst_image->data + offset, scratch + temporary_offset,
               (size_t)row_bytes);
        temporary_offset += (size_t)row_bytes;
      };
    };
  } else {
    for (z = 0; z < blocks_z; z++) {
      for (y = 0; y < blocks_y; y++) {
        uint64_t source = src_offset + z * src_image->plane_size +
                          y * src_image->pitch;
        uint64_t destination = dst_offset + z * dst_image->plane_size +
                               y * dst_image->pitch;
        memcpy(dst_image->data + destination, src_image->data + source,
               (size_t)row_bytes);
      };
    };
  };
  return true;
};

static bool vmsvga3d_handle_surface_copy(struct vmsvga_state_s *s,
                                         uint32_t cmd, int32_t *len,
                                         uint32_t fifo_start) {
  struct vmsvga3d_state_s *state;
  SVGA3dCmdSurfaceCopy *body;
  SVGA3dCopyBox *boxes;
  VMSVGA3DSurface *src_surface;
  VMSVGA3DSurface *dst_surface;
  VMSVGA3DSurfaceImage *src_image = NULL;
  VMSVGA3DSurfaceImage *dst_image = NULL;
  VMSVGA3DD3D9TransferSurface src_info;
  VMSVGA3DD3D9TransferSurface dst_info;
  VMSVGA3DD3D9SurfaceCopyPlan transfer_plan;
  VMSVGA3DD3D9AccelResult accel = VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  void *payload;
  uint32_t size;
  uint32_t box_count;
  uint32_t i;
  size_t scratch_size = 0;
  uint8_t *scratch = NULL;
  bool valid = true;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body) ||
      (size - sizeof(*body)) % sizeof(SVGA3dCopyBox) != 0) {
    g_free(payload);
    return true;
  };

  body = payload;
  boxes = (SVGA3dCopyBox *)(body + 1);
  box_count = (size - sizeof(*body)) / sizeof(*boxes);
  state = s->svga3d;
  if (state == NULL || body->src.sid >= SVGA3D_MAX_SURFACE_IDS ||
      body->dest.sid >= SVGA3D_MAX_SURFACE_IDS) {
    valid = false;
  };
  src_surface = valid ? state->surfaces[body->src.sid] : NULL;
  dst_surface = valid ? state->surfaces[body->dest.sid] : NULL;
  if (valid &&
      (!vmsvga3d_surface_image(src_surface, &body->src, &src_image) ||
       !vmsvga3d_surface_image(dst_surface, &body->dest, &dst_image) ||
       !vmsvga3d_d3d9_runtime_surface_info(s, src_surface, &src_info) ||
       !vmsvga3d_d3d9_runtime_surface_info(s, dst_surface, &dst_info) ||
       !vmsvga3d_d3d9_surface_copy_plan(&src_info, &dst_info,
                                        &transfer_plan))) {
    valid = false;
  };
  if (valid) {
    accel = vmsvga3d_d3d9_try_surface_copy(s, body, boxes, box_count,
                                            &transfer_plan);
    if (accel == VMSVGA3D_D3D9_ACCEL_FAILED) {
      valid = false;
    };
  };

  for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
              i < box_count; i++) {
    size_t needed = 0;

    valid = vmsvga3d_surface_copy_box(src_surface, src_image, dst_surface,
                                      dst_image, &boxes[i], NULL, 0, false,
                                      &needed);
    scratch_size = MAX(scratch_size, needed);
  };
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
      scratch_size != 0) {
    scratch = g_try_malloc(scratch_size);
    if (scratch == NULL) {
      valid = false;
    };
  };
  for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
              i < box_count; i++) {
    valid = vmsvga3d_surface_copy_box(src_surface, src_image, dst_surface,
                                      dst_image, &boxes[i], scratch,
                                      scratch_size, true, NULL);
  };
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
    vmsvga3d_d3d9_runtime_sync_surface_from_cpu(s, dst_surface);
  }

  g_free(scratch);
  g_free(payload);
  return true;
};

static void vmsvga3d_clip_surface_box(const SVGA3dBox *box,
                                       const SVGA3dSize *size,
                                       SVGA3dBox *clipped) {
  uint64_t end_x;
  uint64_t end_y;
  uint64_t end_z;

  *clipped = *box;
  if (box->w == 0 || box->h == 0 || box->d == 0 ||
      box->x >= size->width || box->y >= size->height ||
      box->z >= size->depth) {
    clipped->w = 0;
    clipped->h = 0;
    clipped->d = 0;
    return;
  };

  end_x = MIN((uint64_t)box->x + box->w, (uint64_t)size->width);
  end_y = MIN((uint64_t)box->y + box->h, (uint64_t)size->height);
  end_z = MIN((uint64_t)box->z + box->d, (uint64_t)size->depth);
  clipped->w = (uint32_t)(end_x - box->x);
  clipped->h = (uint32_t)(end_y - box->y);
  clipped->d = (uint32_t)(end_z - box->z);
};

static bool vmsvga3d_surface_stretchblt_point(
    VMSVGA3DSurface *src_surface, VMSVGA3DSurfaceImage *src_image,
    VMSVGA3DSurface *dst_surface, VMSVGA3DSurfaceImage *dst_image,
    const SVGA3dBox *src_box, const SVGA3dBox *dst_box,
    uint8_t *scratch, size_t scratch_size, bool execute,
    size_t *scratch_needed) {
  const struct svga3d_surface_desc *desc;
  SVGA3dBox src_clipped;
  SVGA3dBox dst_clipped;
  uint32_t bytes_per_pixel;
  uint64_t src_row_bytes;
  uint64_t src_plane_bytes;
  uint64_t temporary_size;
  uint64_t z;
  uint64_t y;
  uint64_t x;
  bool same_image;

  if (scratch_needed != NULL) {
    *scratch_needed = 0;
  };
  if (src_surface == NULL || src_image == NULL || dst_surface == NULL ||
      dst_image == NULL || src_box == NULL || dst_box == NULL) {
    return false;
  };
  if (src_surface->format != dst_surface->format ||
      src_surface->multisample_count > 1 ||
      dst_surface->multisample_count > 1) {
    return false;
  };

  vmsvga3d_clip_surface_box(src_box, &src_image->size, &src_clipped);
  vmsvga3d_clip_surface_box(dst_box, &dst_image->size, &dst_clipped);
  if (src_clipped.w == 0 || src_clipped.h == 0 || src_clipped.d == 0 ||
      dst_clipped.w == 0 || dst_clipped.h == 0 || dst_clipped.d == 0) {
    return true;
  };

  if (src_clipped.w == dst_clipped.w &&
      src_clipped.h == dst_clipped.h &&
      src_clipped.d == dst_clipped.d) {
    SVGA3dCopyBox copy = {
        .x = dst_clipped.x,
        .y = dst_clipped.y,
        .z = dst_clipped.z,
        .w = dst_clipped.w,
        .h = dst_clipped.h,
        .d = dst_clipped.d,
        .srcx = src_clipped.x,
        .srcy = src_clipped.y,
        .srcz = src_clipped.z,
    };

    return vmsvga3d_surface_copy_box(
        src_surface, src_image, dst_surface, dst_image, &copy, scratch,
        scratch_size, execute, scratch_needed);
  };

  desc = svga3dsurface_get_desc(src_surface->format);
  if (desc->format != src_surface->format || desc->bytes_per_block == 0 ||
      desc->pitch_bytes_per_block != desc->bytes_per_block ||
      desc->block_size.width != 1 || desc->block_size.height != 1 ||
      desc->block_size.depth != 1) {
    return false;
  };
  bytes_per_pixel = desc->bytes_per_block;

  if ((uint64_t)src_clipped.x * bytes_per_pixel >= src_image->pitch ||
      (uint64_t)dst_clipped.x * bytes_per_pixel >= dst_image->pitch) {
    return false;
  };
  if ((uint64_t)(src_clipped.x + src_clipped.w) * bytes_per_pixel >
          src_image->pitch ||
      (uint64_t)(dst_clipped.x + dst_clipped.w) * bytes_per_pixel >
          dst_image->pitch) {
    return false;
  };

  same_image = src_image->data == dst_image->data;
  if (same_image) {
    src_row_bytes = (uint64_t)src_clipped.w * bytes_per_pixel;
    if (src_row_bytes > SIZE_MAX ||
        src_row_bytes > UINT64_MAX / src_clipped.h) {
      return false;
    };
    src_plane_bytes = src_row_bytes * src_clipped.h;
    if (src_plane_bytes > UINT64_MAX / src_clipped.d) {
      return false;
    };
    temporary_size = src_plane_bytes * src_clipped.d;
    if (temporary_size > SIZE_MAX) {
      return false;
    };
    if (scratch_needed != NULL) {
      *scratch_needed = (size_t)temporary_size;
    };
    if (execute && (scratch == NULL || scratch_size < temporary_size)) {
      return false;
    };
  };

  if (!execute) {
    return true;
  };

  if (same_image) {
    size_t temporary_offset = 0;

    for (z = 0; z < src_clipped.d; z++) {
      for (y = 0; y < src_clipped.h; y++) {
        uint64_t src_offset =
            (uint64_t)(src_clipped.z + z) * src_image->plane_size +
            (uint64_t)(src_clipped.y + y) * src_image->pitch +
            (uint64_t)src_clipped.x * bytes_per_pixel;
        size_t row_bytes = (size_t)src_clipped.w * bytes_per_pixel;

        if (src_offset > src_image->data_size ||
            row_bytes > src_image->data_size - src_offset) {
          return false;
        };
        memcpy(scratch + temporary_offset, src_image->data + src_offset,
               row_bytes);
        temporary_offset += row_bytes;
      };
    };
  };

  for (z = 0; z < dst_clipped.d; z++) {
    uint32_t src_z = (uint32_t)(((uint64_t)z * src_clipped.d) /
                                dst_clipped.d);

    for (y = 0; y < dst_clipped.h; y++) {
      uint32_t src_y = (uint32_t)(((uint64_t)y * src_clipped.h) /
                                  dst_clipped.h);

      for (x = 0; x < dst_clipped.w; x++) {
        uint32_t src_x = (uint32_t)(((uint64_t)x * src_clipped.w) /
                                    dst_clipped.w);
        uint64_t dst_offset =
            (uint64_t)(dst_clipped.z + z) * dst_image->plane_size +
            (uint64_t)(dst_clipped.y + y) * dst_image->pitch +
            (uint64_t)(dst_clipped.x + x) * bytes_per_pixel;
        const uint8_t *source;

        if (dst_offset > dst_image->data_size ||
            bytes_per_pixel > dst_image->data_size - dst_offset) {
          return false;
        };
        if (same_image) {
          uint64_t scratch_offset =
              ((uint64_t)src_z * src_clipped.h * src_clipped.w +
               (uint64_t)src_y * src_clipped.w + src_x) *
              bytes_per_pixel;
          if (scratch_offset > scratch_size ||
              bytes_per_pixel > scratch_size - scratch_offset) {
            return false;
          };
          source = scratch + scratch_offset;
        } else {
          uint64_t src_offset =
              (uint64_t)(src_clipped.z + src_z) * src_image->plane_size +
              (uint64_t)(src_clipped.y + src_y) * src_image->pitch +
              (uint64_t)(src_clipped.x + src_x) * bytes_per_pixel;
          if (src_offset > src_image->data_size ||
              bytes_per_pixel > src_image->data_size - src_offset) {
            return false;
          };
          source = src_image->data + src_offset;
        };
        memcpy(dst_image->data + dst_offset, source, bytes_per_pixel);
      };
    };
  };
  return true;
};

static bool vmsvga3d_handle_surface_stretchblt(struct vmsvga_state_s *s,
                                                uint32_t cmd, int32_t *len,
                                                uint32_t fifo_start) {
  struct vmsvga3d_state_s *state;
  SVGA3dCmdSurfaceStretchBlt *body;
  VMSVGA3DSurface *src_surface;
  VMSVGA3DSurface *dst_surface;
  VMSVGA3DSurfaceImage *src_image = NULL;
  VMSVGA3DSurfaceImage *dst_image = NULL;
  VMSVGA3DD3D9TransferSurface src_info;
  VMSVGA3DD3D9TransferSurface dst_info;
  VMSVGA3DD3D9StretchBltPlan transfer_plan;
  VMSVGA3DD3D9AccelResult accel = VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  void *payload;
  uint32_t size;
  size_t scratch_size = 0;
  uint8_t *scratch = NULL;
  bool valid = true;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size != sizeof(*body)) {
    g_free(payload);
    return true;
  };

  body = payload;
  state = s->svga3d;
  if (state == NULL || body->src.sid >= SVGA3D_MAX_SURFACE_IDS ||
      body->dest.sid >= SVGA3D_MAX_SURFACE_IDS) {
    valid = false;
  };
  src_surface = valid ? state->surfaces[body->src.sid] : NULL;
  dst_surface = valid ? state->surfaces[body->dest.sid] : NULL;
  if (valid &&
      (!vmsvga3d_surface_image(src_surface, &body->src, &src_image) ||
       !vmsvga3d_surface_image(dst_surface, &body->dest, &dst_image) ||
       !vmsvga3d_d3d9_runtime_surface_info(s, src_surface, &src_info) ||
       !vmsvga3d_d3d9_runtime_surface_info(s, dst_surface, &dst_info) ||
       !vmsvga3d_d3d9_stretch_blt_plan(&src_info, &dst_info, body->mode,
                                       &transfer_plan))) {
    valid = false;
  };
  if (valid) {
    accel = vmsvga3d_d3d9_try_stretch_blt(s, body, &transfer_plan);
    if (accel == VMSVGA3D_D3D9_ACCEL_FAILED) {
      valid = false;
    } else if (accel == VMSVGA3D_D3D9_ACCEL_UNAVAILABLE &&
               body->mode != SVGA3D_STRETCH_BLT_POINT) {
      valid = false;
    };
  };
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
    valid = vmsvga3d_surface_stretchblt_point(
        src_surface, src_image, dst_surface, dst_image, &body->boxSrc,
        &body->boxDest, NULL, 0, false, &scratch_size);
  };
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
      scratch_size != 0) {
    scratch = g_try_malloc(scratch_size);
    if (scratch == NULL) {
      valid = false;
    };
  };
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
    valid = vmsvga3d_surface_stretchblt_point(
        src_surface, src_image, dst_surface, dst_image, &body->boxSrc,
        &body->boxDest, scratch, scratch_size, true, NULL);
  };
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
    vmsvga3d_d3d9_runtime_sync_surface_from_cpu(s, dst_surface);
  }

  g_free(scratch);
  g_free(payload);
  return true;
};

static void vmsvga3d_clip_present_rect(const SVGA3dCopyRect *rect,
                                         const SVGA3dSize *src_size,
                                         uint32_t dst_width,
                                         uint32_t dst_height,
                                         SVGA3dCopyRect *clipped) {
  uint32_t max_width;
  uint32_t max_height;

  *clipped = *rect;
  if (rect->w == 0 || rect->h == 0 || rect->srcx >= src_size->width ||
      rect->srcy >= src_size->height || rect->x >= dst_width ||
      rect->y >= dst_height) {
    clipped->w = 0;
    clipped->h = 0;
    return;
  };

  max_width = MIN(src_size->width - rect->srcx, dst_width - rect->x);
  max_height = MIN(src_size->height - rect->srcy, dst_height - rect->y);
  clipped->w = MIN(rect->w, max_width);
  clipped->h = MIN(rect->h, max_height);
};

static bool vmsvga3d_present_format(
    VMSVGA3DSurface *surface, const struct svga3d_surface_desc **desc_out) {
  const struct svga3d_surface_desc *desc;

  if (surface == NULL || surface->mip_count == 0 ||
      surface->multisample_count > 1) {
    return false;
  };
  desc = svga3dsurface_get_desc(surface->format);
  if (desc->format != surface->format ||
      (desc->block_desc & SVGA3DBLOCKDESC_RGB_UNORM) !=
          SVGA3DBLOCKDESC_RGB_UNORM ||
      desc->block_size.width != 1 || desc->block_size.height != 1 ||
      desc->block_size.depth != 1 || desc->bytes_per_block == 0 ||
      desc->bytes_per_block > sizeof(uint64_t) ||
      desc->pitch_bytes_per_block != desc->bytes_per_block ||
      desc->bitDepth.blue == 0 || desc->bitDepth.green == 0 ||
      desc->bitDepth.red == 0 || surface->mips[0].size.depth != 1 ||
      surface->mips[0].data == NULL) {
    return false;
  };
  if (desc_out != NULL) {
    *desc_out = desc;
  };
  return true;
};

static uint8_t vmsvga3d_present_channel(uint64_t pixel, uint8_t depth,
                                         uint8_t offset) {
  uint64_t mask;
  uint64_t value;

  if (depth == 0) {
    return 0;
  };
  if (depth >= 64 || offset >= 64 || depth > 64 - offset) {
    return 0;
  };
  mask = (UINT64_C(1) << depth) - 1;
  value = (pixel >> offset) & mask;
  return (uint8_t)((value * 255 + mask / 2) / mask);
};

static uint32_t vmsvga3d_present_scanout_pixel(uint8_t red, uint8_t green,
                                                uint8_t blue,
                                                uint32_t depth) {
  switch (depth) {
  case 15:
    return ((uint32_t)(red >> 3) << 10) |
           ((uint32_t)(green >> 3) << 5) | (blue >> 3);
  case 16:
    return ((uint32_t)(red >> 3) << 11) |
           ((uint32_t)(green >> 2) << 5) | (blue >> 3);
  case 24:
  case 32:
    return (uint32_t)blue | ((uint32_t)green << 8) |
           ((uint32_t)red << 16);
  default:
    return 0;
  };
};

static uint64_t vmsvga3d_present_load_pixel(const uint8_t *src,
                                             uint32_t bytes) {
  uint64_t pixel = 0;
  uint32_t i;

  for (i = 0; i < bytes; i++) {
    pixel |= (uint64_t)src[i] << (i * 8);
  };
  return pixel;
};

static bool vmsvga3d_present_rect(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, const struct svga3d_surface_desc *desc,
    const SVGA3dCopyRect *rect, bool execute) {
  SVGA3dCopyRect clipped;
  uint32_t dst_width = vmsvga_active_width(s);
  uint32_t dst_height = vmsvga_active_height(s);
  uint32_t dst_depth = vmsvga_active_depth(s);
  uint32_t dst_bypp = vmsvga_bytes_per_pixel(dst_depth);
  uint32_t dst_pitch = vmsvga_stride(s);
  uint32_t src_bypp = desc->bytes_per_block;
  uint64_t src_offset;
  uint64_t src_end;
  uint64_t dst_offset;
  uint64_t dst_end;
  uint64_t src_row_bytes;
  uint64_t dst_row_bytes;
  uint32_t row;

  if (dst_width == 0 || dst_height == 0 || dst_pitch == 0 ||
      (dst_depth != 15 && dst_depth != 16 && dst_depth != 24 &&
       dst_depth != 32) ||
      dst_bypp == 0 || s->vga.vram_ptr == NULL ||
      surface == NULL || image == NULL || desc == NULL || rect == NULL) {
    return false;
  };

  vmsvga3d_clip_present_rect(rect, &image->size, dst_width, dst_height,
                              &clipped);
  if (clipped.w == 0 || clipped.h == 0) {
    return true;
  };

  src_row_bytes = (uint64_t)clipped.w * src_bypp;
  dst_row_bytes = (uint64_t)clipped.w * dst_bypp;
  src_offset = (uint64_t)clipped.srcy * image->pitch +
               (uint64_t)clipped.srcx * src_bypp;
  dst_offset = (uint64_t)clipped.y * dst_pitch +
               (uint64_t)clipped.x * dst_bypp;
  src_end = src_offset + (uint64_t)(clipped.h - 1) * image->pitch +
            src_row_bytes;
  dst_end = dst_offset + (uint64_t)(clipped.h - 1) * dst_pitch +
            dst_row_bytes;
  if (src_end > image->data_size || dst_end > s->vga.vram_size ||
      src_row_bytes > image->pitch || dst_row_bytes > dst_pitch) {
    return false;
  };
  if (!execute) {
    return true;
  };

  if (dst_depth == 32 &&
      (surface->format == SVGA3D_X8R8G8B8 ||
       surface->format == SVGA3D_A8R8G8B8)) {
    for (row = 0; row < clipped.h; row++) {
      const uint8_t *src = image->data + src_offset +
                           (uint64_t)row * image->pitch;
      uint8_t *dst = s->vga.vram_ptr + dst_offset +
                     (uint64_t)row * dst_pitch;
      memcpy(dst, src, (size_t)src_row_bytes);
    };
  } else if (dst_depth == 16 && surface->format == SVGA3D_R5G6B5) {
    for (row = 0; row < clipped.h; row++) {
      const uint8_t *src = image->data + src_offset +
                           (uint64_t)row * image->pitch;
      uint8_t *dst = s->vga.vram_ptr + dst_offset +
                     (uint64_t)row * dst_pitch;
      memcpy(dst, src, (size_t)src_row_bytes);
    };
  } else if (dst_depth == 15 &&
             (surface->format == SVGA3D_X1R5G5B5 ||
              surface->format == SVGA3D_A1R5G5B5)) {
    for (row = 0; row < clipped.h; row++) {
      const uint8_t *src = image->data + src_offset +
                           (uint64_t)row * image->pitch;
      uint8_t *dst = s->vga.vram_ptr + dst_offset +
                     (uint64_t)row * dst_pitch;
      memcpy(dst, src, (size_t)src_row_bytes);
    };
  } else {
    for (row = 0; row < clipped.h; row++) {
      const uint8_t *src = image->data + src_offset +
                           (uint64_t)row * image->pitch;
      uint8_t *dst = s->vga.vram_ptr + dst_offset +
                     (uint64_t)row * dst_pitch;
      uint32_t column;

      for (column = 0; column < clipped.w; column++) {
        uint64_t pixel = vmsvga3d_present_load_pixel(
            src + (size_t)column * src_bypp, src_bypp);
        uint8_t blue = vmsvga3d_present_channel(
            pixel, desc->bitDepth.blue, desc->bitOffset.blue);
        uint8_t green = vmsvga3d_present_channel(
            pixel, desc->bitDepth.green, desc->bitOffset.green);
        uint8_t red = vmsvga3d_present_channel(
            pixel, desc->bitDepth.red, desc->bitOffset.red);
        uint32_t scanout =
            vmsvga3d_present_scanout_pixel(red, green, blue, dst_depth);

        vmsvga_store_pixel(dst + (size_t)column * dst_bypp, dst_bypp,
                           scanout);
      };
    };
  };

  vmsvga_mark_active_rect_dirty(s, clipped.x, clipped.y, clipped.w,
                                clipped.h);
  vmsvga_damage_add_visible(s, clipped.x, clipped.y, clipped.w, clipped.h);
  return true;
};

static bool vmsvga3d_screen_blit_copy_rect(
    const SVGASignedRect *source, const SVGASignedRect *destination,
    const SVGASignedRect *clip, SVGA3dCopyRect *copy, bool *empty) {
  int64_t width;
  int64_t height;
  int64_t left = 0;
  int64_t top = 0;
  int64_t right;
  int64_t bottom;
  int64_t source_x;
  int64_t source_y;
  int64_t destination_x;
  int64_t destination_y;

  if (source == NULL || destination == NULL || copy == NULL || empty == NULL) {
    return false;
  }
  *empty = false;
  width = (int64_t)source->right - source->left;
  height = (int64_t)source->bottom - source->top;
  if (width <= 0 || height <= 0 ||
      width != (int64_t)destination->right - destination->left ||
      height != (int64_t)destination->bottom - destination->top) {
    return false;
  }
  right = width;
  bottom = height;
  if (clip != NULL) {
    left = MAX((int64_t)clip->left, 0);
    top = MAX((int64_t)clip->top, 0);
    right = MIN((int64_t)clip->right, width);
    bottom = MIN((int64_t)clip->bottom, height);
    if (right <= left || bottom <= top) {
      *empty = true;
      return true;
    }
  }

  source_x = (int64_t)source->left + left;
  source_y = (int64_t)source->top + top;
  destination_x = (int64_t)destination->left + left;
  destination_y = (int64_t)destination->top + top;
  width = right - left;
  height = bottom - top;

  if (source_x < 0) {
    int64_t shift = -source_x;
    source_x = 0;
    destination_x += shift;
    width -= shift;
  }
  if (destination_x < 0) {
    int64_t shift = -destination_x;
    destination_x = 0;
    source_x += shift;
    width -= shift;
  }
  if (source_y < 0) {
    int64_t shift = -source_y;
    source_y = 0;
    destination_y += shift;
    height -= shift;
  }
  if (destination_y < 0) {
    int64_t shift = -destination_y;
    destination_y = 0;
    source_y += shift;
    height -= shift;
  }
  if (width <= 0 || height <= 0) {
    *empty = true;
    return true;
  }
  if (source_x > UINT32_MAX || source_y > UINT32_MAX ||
      destination_x > UINT32_MAX || destination_y > UINT32_MAX ||
      width > UINT32_MAX || height > UINT32_MAX) {
    return false;
  }

  copy->srcx = (uint32_t)source_x;
  copy->srcy = (uint32_t)source_y;
  copy->x = (uint32_t)destination_x;
  copy->y = (uint32_t)destination_y;
  copy->w = (uint32_t)width;
  copy->h = (uint32_t)height;
  return true;
}

static bool vmsvga3d_handle_blit_surface_to_screen(
    struct vmsvga_state_s *s, uint32_t cmd, int32_t *len,
    uint32_t fifo_start) {
  struct vmsvga3d_state_s *state;
  SVGA3dCmdBlitSurfaceToScreen *body;
  SVGASignedRect *clips;
  VMSVGA3DSurface *surface;
  VMSVGA3DSurfaceImage *image;
  const struct svga3d_surface_desc *desc = NULL;
  SVGA3dCopyRect *copies = NULL;
  VMSVGA3DD3D9AccelResult accel = VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  void *payload;
  uint32_t size;
  uint32_t clip_count;
  uint32_t copy_count;
  uint32_t i;
  bool valid = true;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  }
  if (size < sizeof(*body) ||
      (size - sizeof(*body)) % sizeof(SVGASignedRect) != 0) {
    g_free(payload);
    return true;
  }

  body = payload;
  clips = (SVGASignedRect *)(body + 1);
  clip_count = (size - sizeof(*body)) / sizeof(*clips);
  state = s->svga3d;
  if (state == NULL || body->srcImage.sid >= SVGA3D_MAX_SURFACE_IDS ||
      !s->active_valid) {
    valid = false;
  }
  surface = valid ? state->surfaces[body->srcImage.sid] : NULL;
  if (valid && surface != NULL) {
    accel = vmsvga3d_d3d9_try_screen_blit(s, body, clips, clip_count);
    if (accel == VMSVGA3D_D3D9_ACCEL_FAILED) {
      valid = false;
    }
  } else {
    valid = false;
  }

  image = surface != NULL && surface->mip_count != 0 ? &surface->mips[0] : NULL;
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
    int64_t source_width =
        (int64_t)body->srcRect.right - body->srcRect.left;
    int64_t source_height =
        (int64_t)body->srcRect.bottom - body->srcRect.top;
    int64_t destination_width =
        (int64_t)body->destRect.right - body->destRect.left;
    int64_t destination_height =
        (int64_t)body->destRect.bottom - body->destRect.top;

    if (body->destScreenId != 0 || source_width <= 0 || source_height <= 0 ||
        source_width != destination_width || source_height != destination_height ||
        !vmsvga3d_present_format(surface, &desc) || image == NULL) {
      valid = false;
    }
  }

  copy_count = clip_count != 0 ? clip_count : 1;
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
    copies = g_try_new0(SVGA3dCopyRect, copy_count);
    if (copies == NULL) {
      valid = false;
    }
  }
  for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
              i < copy_count; i++) {
    bool empty;
    const SVGASignedRect *clip = clip_count != 0 ? &clips[i] : NULL;

    if (!vmsvga3d_screen_blit_copy_rect(&body->srcRect, &body->destRect,
                                         clip, &copies[i], &empty)) {
      valid = false;
    } else if (empty) {
      copies[i].w = 0;
      copies[i].h = 0;
    }
  }
  for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
              i < copy_count; i++) {
    if (copies[i].w != 0 && copies[i].h != 0) {
      valid = vmsvga3d_present_rect(s, surface, image, desc, &copies[i], false);
    }
  }
  for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
              i < copy_count; i++) {
    if (copies[i].w != 0 && copies[i].h != 0) {
      valid = vmsvga3d_present_rect(s, surface, image, desc, &copies[i], true);
    }
  }

  g_free(copies);
  g_free(payload);
  return true;
}

static bool vmsvga3d_handle_present(struct vmsvga_state_s *s,
                                     uint32_t cmd, int32_t *len,
                                     uint32_t fifo_start) {
  struct vmsvga3d_state_s *state;
  SVGA3dCmdPresent *body;
  SVGA3dCopyRect *rects;
  SVGA3dCopyRect full_screen_rect;
  VMSVGA3DSurface *surface;
  VMSVGA3DSurfaceImage *image;
  const struct svga3d_surface_desc *desc = NULL;
  void *payload;
  uint32_t size;
  uint32_t rect_count;
  uint32_t i;
  VMSVGA3DD3D9AccelResult accel = VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  bool valid = true;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body) ||
      (size - sizeof(*body)) % sizeof(SVGA3dCopyRect) != 0) {
    g_free(payload);
    return true;
  };

  body = payload;
  rects = (SVGA3dCopyRect *)(body + 1);
  rect_count = (size - sizeof(*body)) / sizeof(*rects);
  state = s->svga3d;
  if (state == NULL || body->sid >= SVGA3D_MAX_SURFACE_IDS ||
      !s->active_valid) {
    valid = false;
  };
  surface = valid ? state->surfaces[body->sid] : NULL;
  if (valid && surface != NULL) {
    accel = vmsvga3d_d3d9_try_present(s, body, rects, rect_count);
    if (accel == VMSVGA3D_D3D9_ACCEL_FAILED) {
      valid = false;
    }
  } else {
    valid = false;
  };
  image = surface != NULL && surface->mip_count != 0 ? &surface->mips[0] : NULL;
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
      rect_count == 0) {
    memset(&full_screen_rect, 0, sizeof(full_screen_rect));
    full_screen_rect.w = vmsvga_active_width(s);
    full_screen_rect.h = vmsvga_active_height(s);
    rects = &full_screen_rect;
    rect_count = 1;
  };
  if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
      (!vmsvga3d_present_format(surface, &desc) || image == NULL)) {
    valid = false;
  };

  for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
              i < rect_count; i++) {
    valid = vmsvga3d_present_rect(s, surface, image, desc, &rects[i], false);
  };
  for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
              i < rect_count; i++) {
    valid = vmsvga3d_present_rect(s, surface, image, desc, &rects[i], true);
  };

  g_free(payload);
  return true;
};

static bool vmsvga3d_surface_dma_box(struct vmsvga_state_s *s,
                                     VMSVGA3DSurface *surface,
                                     VMSVGA3DSurfaceImage *image,
                                     const SVGAGuestImage *guest,
                                     SVGA3dTransferType transfer,
                                     const SVGA3dCopyBox *box,
                                     uint32_t maximum_offset,
                                     bool execute) {
  const struct svga3d_surface_desc *desc;
  SVGA3dCopyBox clipped;
  uint32_t block_width;
  uint32_t block_height;
  uint32_t block_depth;
  uint32_t bytes_per_block;
  uint32_t host_block_x;
  uint32_t host_block_y;
  uint32_t host_block_z;
  uint32_t guest_block_x;
  uint32_t guest_block_y;
  uint32_t guest_block_z;
  uint32_t blocks_x;
  uint32_t blocks_y;
  uint32_t blocks_z;
  uint32_t guest_pitch;
  uint64_t guest_plane_pitch;
  uint64_t row_bytes;
  uint32_t z;
  uint32_t y;

  if (!vmsvga3d_clip_dma_box(box, &image->size, &clipped)) {
    return true;
  };
  if (surface->multisample_count > 1) {
    return false;
  };

  desc = svga3dsurface_get_desc(surface->format);
  if (desc->format != surface->format || desc->bytes_per_block == 0 ||
      desc->block_size.width == 0 || desc->block_size.height == 0 ||
      desc->block_size.depth == 0) {
    return false;
  };
  block_width = desc->block_size.width;
  block_height = desc->block_size.height;
  block_depth = desc->block_size.depth;
  bytes_per_block = desc->bytes_per_block;

  if (clipped.x % block_width != 0 || clipped.y % block_height != 0 ||
      clipped.z % block_depth != 0 || clipped.srcx % block_width != 0 ||
      clipped.srcy % block_height != 0 ||
      clipped.srcz % block_depth != 0) {
    return false;
  };

  host_block_x = clipped.x / block_width;
  host_block_y = clipped.y / block_height;
  host_block_z = clipped.z / block_depth;
  guest_block_x = clipped.srcx / block_width;
  guest_block_y = clipped.srcy / block_height;
  guest_block_z = clipped.srcz / block_depth;
  blocks_x = (clipped.w + block_width - 1) / block_width;
  blocks_y = (clipped.h + block_height - 1) / block_height;
  blocks_z = (clipped.d + block_depth - 1) / block_depth;
  row_bytes = (uint64_t)blocks_x * bytes_per_block;
  if (row_bytes == 0 || row_bytes > UINT32_MAX) {
    return false;
  };

  guest_pitch = guest->pitch != 0 ? guest->pitch : image->pitch;
  if (guest_pitch == 0 ||
      ((block_width > 1 || block_height > 1 || block_depth > 1) &&
       guest->pitch != 0 && guest_pitch != image->pitch)) {
    return false;
  };
  if ((uint64_t)guest_block_x * bytes_per_block + row_bytes > guest_pitch) {
    return false;
  };
  guest_plane_pitch = (uint64_t)image->size.height * guest_pitch;

  for (z = 0; z < blocks_z; z++) {
    for (y = 0; y < blocks_y; y++) {
      uint64_t guest_relative = 0;
      uint64_t guest_offset;
      uint64_t host_offset = 0;

      if (!vmsvga3d_u64_add_product(&guest_relative, guest_block_x,
                                     bytes_per_block) ||
          !vmsvga3d_u64_add_product(&guest_relative,
                                     (uint64_t)guest_block_y + y,
                                     guest_pitch) ||
          !vmsvga3d_u64_add_product(&guest_relative,
                                     (uint64_t)guest_block_z + z,
                                     guest_plane_pitch) ||
          guest_relative > UINT64_MAX - guest->ptr.offset ||
          !vmsvga3d_u64_add_product(&host_offset, host_block_x,
                                     bytes_per_block) ||
          !vmsvga3d_u64_add_product(&host_offset,
                                     (uint64_t)host_block_y + y,
                                     image->pitch) ||
          !vmsvga3d_u64_add_product(&host_offset,
                                     (uint64_t)host_block_z + z,
                                     image->plane_size)) {
        return false;
      };
      guest_offset = (uint64_t)guest->ptr.offset + guest_relative;

      if (guest_relative > maximum_offset ||
          row_bytes > (uint64_t)maximum_offset - guest_relative ||
          guest_offset > UINT32_MAX || host_offset > image->data_size ||
          row_bytes > (uint64_t)image->data_size - host_offset ||
          !vmsvga_gmr_validate_range(s, guest->ptr.gmrId,
                                     (uint32_t)guest_offset,
                                     (size_t)row_bytes)) {
        return false;
      };
      if (!execute) {
        continue;
      };
      if (transfer == SVGA3D_WRITE_HOST_VRAM) {
        if (!vmsvga_gmr_read(s, guest->ptr.gmrId, (uint32_t)guest_offset,
                             image->data + host_offset, (size_t)row_bytes)) {
          return false;
        };
      } else if (transfer == SVGA3D_READ_HOST_VRAM) {
        if (!vmsvga_gmr_write(s, guest->ptr.gmrId, (uint32_t)guest_offset,
                              image->data + host_offset, (size_t)row_bytes)) {
          return false;
        };
      } else {
        return false;
      };
    };
  };
  return true;
};

static bool vmsvga3d_handle_surface_dma(struct vmsvga_state_s *s,
                                        uint32_t cmd, int32_t *len,
                                        uint32_t fifo_start) {
  struct vmsvga3d_state_s *state;
  SVGA3dCmdSurfaceDMA *body;
  SVGA3dCopyBox *boxes;
  SVGA3dCmdSurfaceDMASuffix *suffix = NULL;
  VMSVGA3DSurface *surface;
  VMSVGA3DSurfaceImage *image;
  VMSVGA3DD3D9TransferSurface surface_info;
  VMSVGA3DD3D9DmaPlan transfer_plan;
  VMSVGA3DD3D9AccelResult accel = VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
  void *payload;
  uint32_t size;
  uint32_t trailing;
  uint32_t box_bytes;
  uint32_t box_count;
  uint32_t maximum_offset = UINT32_MAX;
  uint32_t i;
  bool valid = true;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size < sizeof(*body)) {
    g_free(payload);
    return true;
  };

  body = payload;
  trailing = size - sizeof(*body);
  if (trailing % sizeof(SVGA3dCopyBox) == sizeof(SVGA3dCmdSurfaceDMASuffix)) {
    suffix = (SVGA3dCmdSurfaceDMASuffix *)((uint8_t *)payload + size -
                                            sizeof(*suffix));
    if (suffix->suffixSize != sizeof(*suffix) || suffix->flags.reserved != 0) {
      valid = false;
    } else {
      maximum_offset = suffix->maximumOffset;
      trailing -= sizeof(*suffix);
    };
  } else if (trailing % sizeof(SVGA3dCopyBox) != 0) {
    valid = false;
  };

  box_bytes = valid ? trailing : 0;
  box_count = box_bytes / sizeof(SVGA3dCopyBox);
  boxes = (SVGA3dCopyBox *)(body + 1);
  state = s->svga3d;
  if (valid && (body->transfer != SVGA3D_WRITE_HOST_VRAM &&
                body->transfer != SVGA3D_READ_HOST_VRAM)) {
    valid = false;
  };
  if (valid && (state == NULL || body->host.sid >= SVGA3D_MAX_SURFACE_IDS)) {
    valid = false;
  };
  surface = valid ? state->surfaces[body->host.sid] : NULL;
  if (valid &&
      (!vmsvga3d_surface_image(surface, &body->host, &image) ||
       !vmsvga3d_d3d9_runtime_surface_info(s, surface, &surface_info) ||
       !vmsvga3d_d3d9_dma_plan(&surface_info, body->transfer, true,
                               &transfer_plan))) {
    valid = false;
  };

  for (i = 0; valid && i < box_count; i++) {
    valid = vmsvga3d_surface_dma_box(s, surface, image, &body->guest,
                                     body->transfer, &boxes[i],
                                     maximum_offset, false);
  };
  if (valid) {
    accel = vmsvga3d_d3d9_try_surface_dma(
        s, body, boxes, box_count, maximum_offset, &transfer_plan);
    if (accel == VMSVGA3D_D3D9_ACCEL_FAILED) {
      valid = false;
    };
  };
  for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
              i < box_count; i++) {
    valid = vmsvga3d_surface_dma_box(s, surface, image, &body->guest,
                                     body->transfer, &boxes[i],
                                     maximum_offset, true);
  };

  g_free(payload);
  return true;
};

static uint32_t vmsvga3d_host_hwversion(void) {
  return SVGA3D_HWVERSION_WS65_B1;
};

typedef bool (*VMSVGA3DCommandHandler)(struct vmsvga_state_s *s,
                                       uint32_t cmd, int32_t *len,
                                       uint32_t fifo_start);

typedef enum {
  VMSVGA3D_COMMAND_STALL,
  VMSVGA3D_COMMAND_DISCARD,
} VMSVGA3DCommandAction;

typedef struct {
  uint32_t command;
  VMSVGA3DCommandAction action;
  VMSVGA3DCommandHandler handler;
  const char *name;
} VMSVGA3DCommandInfo;

#include "vmware_vga_dxvk_wsi.c"
#include "vmware_vga_dxvk.c"
#define VMSVGA3D_D3D9_RUNTIME_INTEGRATION 1
#include "vmware_vga_d3d9.c"
#undef VMSVGA3D_D3D9_RUNTIME_INTEGRATION
#include "vmware_vga_d3d10.c"
#include "vmware_vga_3d_state.c"

#if defined(__GNUC__)
#define VMSVGA3D_DX_COMMAND_UNUSED __attribute__((unused))
#else
#define VMSVGA3D_DX_COMMAND_UNUSED
#endif

static bool vmsvga3d_dx_shadow_command(struct vmsvga_state_s *s,
                                       uint32_t cid, uint32_t cmd,
                                       const void *payload, uint32_t size) {
  switch (cmd) {
  case SVGA_3D_CMD_DX_INVALIDATE_CONTEXT:
    if (size < sizeof(SVGA3dCmdDXInvalidateContext)) {
      return false;
    };
    /* The packet cid is ignored; command-buffer metadata selects the context. */
    return vmsvga3d_state_dx_context_invalidate(s, cid);

  case SVGA_3D_CMD_DX_SET_SINGLE_CONSTANT_BUFFER: {
    SVGA3dCmdDXSetSingleConstantBuffer command;
    VMSVGA3DD3D10ConstantBufferPlan plan;
    VMSVGA3DD3D10Level level;
    VMSVGA3DSurface *surface = NULL;
    bool surface_available = false;
    uint32_t surface_bytes = 0;
    bool has_surface_data = false;

    if (vmsvga3d_dx_context(s, cid) == NULL || size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    if (command.sid != SVGA_ID_INVALID && s->svga3d != NULL &&
        command.sid < SVGA3D_MAX_SURFACE_IDS) {
      surface = s->svga3d->surfaces[command.sid];
      if (surface != NULL && surface->mip_count != 0 &&
          surface->mips != NULL) {
        surface_available = true;
        surface_bytes = surface->mips[0].data_size;
        has_surface_data = surface->mips[0].data != NULL;
      };
    };
    memset(&plan, 0, sizeof(plan));
    level = vmsvga3d_d3d10_constant_buffer_plan(
        command.slot, command.type, command.sid, command.offsetInBytes,
        command.sizeInBytes, surface_available, surface_bytes,
        has_surface_data, &plan);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
      /* VBox updates the context shadow before backend surface/range checks. */
      if (plan.shadow_update) {
        (void)vmsvga3d_state_dx_apply_constant_buffer(s, cid, &plan);
      };
      return false;
    };
    return vmsvga3d_state_dx_apply_constant_buffer(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_SHADER_RESOURCES: {
    SVGA3dCmdDXSetShaderResources command;
    SVGA3dShaderResourceViewId ids[SVGA3D_DX_MAX_SRVIEWS];
    VMSVGA3DD3D10ShaderResourceSetPlan plan;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    const uint32_t header_size = sizeof(command);
    uint32_t count;

    if (context == NULL || size < header_size) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    count = (size - header_size) / sizeof(ids[0]);
    if (count > SVGA3D_DX_MAX_SRVIEWS) {
      return false;
    };
    if (count != 0) {
      memcpy(ids, (const uint8_t *)payload + header_size,
             count * sizeof(ids[0]));
    };
    return vmsvga3d_d3d10_shader_resources_set_plan(
               command.startView, command.type, count,
               count != 0 ? ids : NULL,
               context->cotables[SVGA_COTABLE_SRVIEW].valid_entries,
               &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_shader_resources(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_SHADER: {
    SVGA3dCmdDXSetShader command;
    VMSVGA3DD3D10ShaderSetPlan plan;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

    if (context == NULL || size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    return vmsvga3d_d3d10_shader_set_plan(
               command.shaderId, command.type,
               context->cotables[SVGA_COTABLE_DXSHADER].valid_entries,
               &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_shader(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_SAMPLERS: {
    SVGA3dCmdDXSetSamplers command;
    SVGA3dSamplerId ids[SVGA3D_DX_MAX_SAMPLERS];
    VMSVGA3DD3D10SamplerSetPlan plan;
    VMSVGA3DD3D10Level level;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    const uint32_t header_size = sizeof(command);
    uint32_t count;

    if (context == NULL || size < header_size) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    count = (size - header_size) / sizeof(ids[0]);
    if (count > SVGA3D_DX_MAX_SAMPLERS) {
      return false;
    };
    if (count != 0) {
      memcpy(ids, (const uint8_t *)payload + header_size,
             count * sizeof(ids[0]));
    };
    memset(&plan, 0, sizeof(plan));
    level = vmsvga3d_d3d10_samplers_set_plan(
        command.startSampler, command.type, count,
        count != 0 ? ids : NULL,
        context->cotables[SVGA_COTABLE_SAMPLER].valid_entries, &plan);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
      /* VBox validates and writes samplers sequentially. */
      if (plan.partial_shadow_update_on_failure &&
          plan.shadow_update_count != 0) {
        (void)vmsvga3d_state_dx_apply_samplers(s, cid, &plan);
      };
      return false;
    };
    return vmsvga3d_state_dx_apply_samplers(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_INPUT_LAYOUT: {
    SVGA3dCmdDXSetInputLayout command;
    VMSVGA3DD3D10InputLayoutSetPlan plan;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

    if (context == NULL || size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    return vmsvga3d_d3d10_input_layout_set_plan(
               command.elementLayoutId,
               context->cotables[SVGA_COTABLE_ELEMENTLAYOUT].valid_entries,
               &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_input_layout(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_VERTEX_BUFFERS: {
    SVGA3dCmdDXSetVertexBuffers command;
    SVGA3dVertexBuffer buffers[SVGA3D_DX_MAX_VERTEXBUFFERS];
    VMSVGA3DD3D10VertexBufferSetPlan plan;
    const uint32_t header_size = sizeof(command);
    uint32_t count;

    if (size < header_size) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    count = (size - header_size) / sizeof(buffers[0]);
    if (count > SVGA3D_DX_MAX_VERTEXBUFFERS) {
      return false;
    };
    if (count != 0) {
      memcpy(buffers, (const uint8_t *)payload + header_size,
             count * sizeof(buffers[0]));
    };
    return vmsvga3d_d3d10_vertex_buffers_set_plan(
               command.startBuffer, count, count != 0 ? buffers : NULL,
               &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_vertex_buffers(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_INDEX_BUFFER: {
    SVGA3dCmdDXSetIndexBuffer command;
    VMSVGA3DD3D10IndexBufferSetPlan plan;
    VMSVGA3DD3D10Level level;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    level = vmsvga3d_d3d10_index_buffer_set_plan(
        command.sid, command.format, command.offset, &plan);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
      /* VBox updates the context shadow before the backend rejects format. */
      if (plan.backend_reject && plan.shadow_update) {
        (void)vmsvga3d_state_dx_apply_index_buffer(s, cid, &plan);
      };
      return false;
    };
    return vmsvga3d_state_dx_apply_index_buffer(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_TOPOLOGY: {
    SVGA3dCmdDXSetTopology command;
    VMSVGA3DD3D10TopologySetPlan plan;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    return vmsvga3d_d3d10_topology_set_plan(command.topology, &plan) !=
               VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_topology(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_VIEWPORTS: {
    SVGA3dViewport viewports[SVGA3D_DX_MAX_VIEWPORTS];
    VMSVGA3DD3D10ViewportsSetPlan plan;
    const uint32_t header_size = sizeof(SVGA3dCmdDXSetViewports);
    uint32_t count;

    if (size < header_size) {
      return false;
    };
    count = (size - header_size) / sizeof(viewports[0]);
    if (count > SVGA3D_DX_MAX_VIEWPORTS) {
      return false;
    };
    if (count != 0) {
      memcpy(viewports, (const uint8_t *)payload + header_size,
             count * sizeof(viewports[0]));
    };
    return vmsvga3d_d3d10_viewports_set_plan(
               count, count != 0 ? viewports : NULL, &plan) !=
               VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_viewports(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_SCISSORRECTS: {
    SVGASignedRect rects[SVGA3D_DX_MAX_SCISSORRECTS];
    VMSVGA3DD3D10ScissorPlan plan;
    const uint32_t header_size = sizeof(SVGA3dCmdDXSetScissorRects);
    uint32_t count;

    if (size < header_size) {
      return false;
    };
    count = (size - header_size) / sizeof(rects[0]);
    if (count > SVGA3D_DX_MAX_SCISSORRECTS) {
      return false;
    };
    if (count != 0) {
      memcpy(rects, (const uint8_t *)payload + header_size,
             count * sizeof(rects[0]));
    };
    return vmsvga3d_d3d10_scissor_plan(
               count, count != 0 ? rects : NULL, &plan) !=
               VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_scissors(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_RENDERTARGETS: {
    SVGA3dCmdDXSetRenderTargets command;
    SVGA3dRenderTargetViewId ids[SVGA3D_MAX_RENDER_TARGETS];
    VMSVGA3DD3D10RenderTargetsSetPlan plan;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    const uint32_t header_size = sizeof(command);
    uint32_t count;

    if (context == NULL || size < header_size) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    count = (size - header_size) / sizeof(ids[0]);
    if (count > SVGA3D_MAX_RENDER_TARGETS) {
      return false;
    };
    if (count != 0) {
      memcpy(ids, (const uint8_t *)payload + header_size,
             count * sizeof(ids[0]));
    };
    return vmsvga3d_d3d10_render_targets_set_plan(
               command.depthStencilViewId, count,
               count != 0 ? ids : NULL,
               context->cotables[SVGA_COTABLE_DSVIEW].valid_entries,
               context->cotables[SVGA_COTABLE_RTVIEW].valid_entries,
               context->render_target_count, &plan) !=
               VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_render_targets(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_SOTARGETS: {
    SVGA3dSoTarget targets[SVGA3D_DX_MAX_SOTARGETS];
    VMSVGA3DD3D10SOTargetsPlan plan;
    const uint32_t header_size = sizeof(SVGA3dCmdDXSetSOTargets);
    uint32_t count;

    if (size < header_size) {
      return false;
    };
    count = (size - header_size) / sizeof(targets[0]);
    if (count > SVGA3D_DX_MAX_SOTARGETS) {
      return false;
    };
    if (count != 0) {
      memcpy(targets, (const uint8_t *)payload + header_size,
             count * sizeof(targets[0]));
    };
    return vmsvga3d_d3d10_so_targets_plan(
               count, count != 0 ? targets : NULL, &plan) !=
               VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_so_targets(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_STREAMOUTPUT: {
    SVGA3dCmdDXSetStreamOutput command;
    VMSVGA3DD3D10StreamOutputSetPlan plan;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

    if (context == NULL || size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    return vmsvga3d_d3d10_stream_output_set_plan(
               command.soid,
               context->cotables[SVGA_COTABLE_STREAMOUTPUT].valid_entries,
               &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_stream_output(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_DEFINE_BLEND_STATE: {
    SVGA3dCmdDXDefineBlendState command;
    SVGACOTableDXBlendStateEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_BLENDSTATE, command.blendId);
    return entry != NULL &&
           vmsvga3d_d3d10_blend_define_entry(&command, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_BLEND_STATE: {
    SVGA3dCmdDXDestroyBlendState command;
    SVGACOTableDXBlendStateEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_BLENDSTATE, command.blendId);
    return entry != NULL &&
           vmsvga3d_d3d10_blend_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_STATE: {
    SVGA3dCmdDXDefineDepthStencilState command;
    SVGACOTableDXDepthStencilEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DEPTHSTENCIL, command.depthStencilId);
    return entry != NULL &&
           vmsvga3d_d3d10_depth_stencil_define_entry(&command, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_STATE: {
    SVGA3dCmdDXDestroyDepthStencilState command;
    SVGACOTableDXDepthStencilEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DEPTHSTENCIL, command.depthStencilId);
    return entry != NULL &&
           vmsvga3d_d3d10_depth_stencil_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DEFINE_RASTERIZER_STATE: {
    SVGA3dCmdDXDefineRasterizerState command;
    SVGACOTableDXRasterizerStateEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_RASTERIZERSTATE, command.rasterizerId);
    return entry != NULL &&
           vmsvga3d_d3d10_rasterizer_define_entry(&command, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_RASTERIZER_STATE: {
    SVGA3dCmdDXDestroyRasterizerState command;
    SVGACOTableDXRasterizerStateEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_RASTERIZERSTATE, command.rasterizerId);
    return entry != NULL &&
           vmsvga3d_d3d10_rasterizer_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DEFINE_SAMPLER_STATE: {
    SVGA3dCmdDXDefineSamplerState command;
    SVGACOTableDXSamplerEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_SAMPLER, command.samplerId);
    return entry != NULL &&
           vmsvga3d_d3d10_sampler_define_entry(&command, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_SAMPLER_STATE: {
    SVGA3dCmdDXDestroySamplerState command;
    SVGACOTableDXSamplerEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_SAMPLER, command.samplerId);
    return entry != NULL &&
           vmsvga3d_d3d10_sampler_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_SET_BLEND_STATE: {
    SVGA3dCmdDXSetBlendState command;
    VMSVGA3DD3D10BlendStateSetPlan plan;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

    if (context == NULL || size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    return vmsvga3d_d3d10_blend_state_set_plan(
               &command,
               context->cotables[SVGA_COTABLE_BLENDSTATE].valid_entries,
               &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_blend_state(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_DEPTHSTENCIL_STATE: {
    SVGA3dCmdDXSetDepthStencilState command;
    VMSVGA3DD3D10DepthStencilStateSetPlan plan;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

    if (context == NULL || size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    return vmsvga3d_d3d10_depth_stencil_state_set_plan(
               &command,
               context->cotables[SVGA_COTABLE_DEPTHSTENCIL].valid_entries,
               &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_depth_stencil_state(s, cid, &plan);
  }

  case SVGA_3D_CMD_DX_SET_RASTERIZER_STATE: {
    SVGA3dCmdDXSetRasterizerState command;
    VMSVGA3DD3D10RasterizerStateSetPlan plan;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

    if (context == NULL || size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    return vmsvga3d_d3d10_rasterizer_state_set_plan(
               command.rasterizerId,
               context->cotables[SVGA_COTABLE_RASTERIZERSTATE].valid_entries,
               &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_state_dx_apply_rasterizer_state(s, cid, &plan);
  }

  default:
    return false;
  }
}

static SVGACBStatus VMSVGA3D_DX_COMMAND_UNUSED
vmsvga3d_dx_command_buffer_process(struct vmsvga_state_s *s, uint32_t cid,
                                   const void *commands, uint32_t size,
                                   uint32_t *error_offset) {
  const uint8_t *bytes = commands;
  uint32_t offset = 0;

  if (error_offset == NULL || (size != 0 && commands == NULL)) {
    return SVGA_CB_STATUS_COMMAND_ERROR;
  };
  *error_offset = 0;

  while (offset < size) {
    const uint32_t command_offset = offset;
    uint32_t cmd;
    uint32_t payload_size;
    uint32_t remaining = size - offset;

    if (remaining < sizeof(cmd)) {
      *error_offset = command_offset;
      return SVGA_CB_STATUS_COMMAND_ERROR;
    };
    memcpy(&cmd, bytes + offset, sizeof(cmd));
    offset += sizeof(cmd);
    remaining -= sizeof(cmd);

    if (remaining < sizeof(payload_size)) {
      *error_offset = command_offset;
      return SVGA_CB_STATUS_COMMAND_ERROR;
    };
    memcpy(&payload_size, bytes + offset, sizeof(payload_size));
    offset += sizeof(payload_size);
    remaining -= sizeof(payload_size);

    if (payload_size > remaining ||
        !vmsvga3d_dx_shadow_command(s, cid, cmd, bytes + offset,
                                    payload_size)) {
      *error_offset = command_offset;
      return SVGA_CB_STATUS_COMMAND_ERROR;
    };
    offset += payload_size;
  };

  *error_offset = offset;
  return SVGA_CB_STATUS_COMPLETED;
}

#undef VMSVGA3D_DX_COMMAND_UNUSED

static bool vmsvga3d_dx_context_entry_read(
    struct vmsvga_state_s *s, uint32_t cid,
    SVGAOTableDXContextEntry *entry) {
  SVGAOTableDXContextEntry raw;

  if (entry == NULL ||
      !vmsvga3d_otable_read(s, SVGA_OTABLE_DXCONTEXT, cid,
                            sizeof(raw), &raw, sizeof(raw))) {
    return false;
  };
  entry->cid = le32_to_cpu(raw.cid);
  entry->mobid = le32_to_cpu(raw.mobid);
  return true;
}

static bool vmsvga3d_dx_context_entry_write(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGAOTableDXContextEntry *entry) {
  SVGAOTableDXContextEntry raw;

  if (entry == NULL) {
    return false;
  };
  raw.cid = cpu_to_le32(entry->cid);
  raw.mobid = cpu_to_le32(entry->mobid);
  return vmsvga3d_otable_write(s, SVGA_OTABLE_DXCONTEXT, cid,
                               sizeof(raw), &raw, sizeof(raw));
}

static uint32_t vmsvga3d_dx_cotable_entry_size(SVGACOTableType type) {
  switch (type) {
  case SVGA_COTABLE_RTVIEW:
    return sizeof(SVGACOTableDXRTViewEntry);
  case SVGA_COTABLE_DSVIEW:
    return sizeof(SVGACOTableDXDSViewEntry);
  case SVGA_COTABLE_SRVIEW:
    return sizeof(SVGACOTableDXSRViewEntry);
  case SVGA_COTABLE_ELEMENTLAYOUT:
    return sizeof(SVGACOTableDXElementLayoutEntry);
  case SVGA_COTABLE_BLENDSTATE:
    return sizeof(SVGACOTableDXBlendStateEntry);
  case SVGA_COTABLE_DEPTHSTENCIL:
    return sizeof(SVGACOTableDXDepthStencilEntry);
  case SVGA_COTABLE_RASTERIZERSTATE:
    return sizeof(SVGACOTableDXRasterizerStateEntry);
  case SVGA_COTABLE_SAMPLER:
    return sizeof(SVGACOTableDXSamplerEntry);
  case SVGA_COTABLE_STREAMOUTPUT:
    return sizeof(SVGACOTableDXStreamOutputEntry);
  case SVGA_COTABLE_DXQUERY:
    return VMSVGA3D_D3D10_QUERY_COTABLE_ENTRY_SIZE;
  case SVGA_COTABLE_DXSHADER:
    return sizeof(SVGACOTableDXShaderEntry);
  case SVGA_COTABLE_UAVIEW:
    return sizeof(SVGACOTableDXUAViewEntry);
  case SVGA_COTABLE_MAX:
    break;
  };
  return 0;
}

static bool vmsvga3d_dx_cotable_set_or_grow(
    struct vmsvga_state_s *s, uint32_t cid, SVGAMobId mobid,
    SVGACOTableType type, uint32_t valid_size, bool grow) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  VMSVGA3DDXCOTable *binding;
  VMSVGA3DMob *mob;
  VMSVGA3DMob *old_mob;
  uint32_t entry_size;
  uint32_t capacity_entries;
  uint32_t valid_entries;
  uint32_t grow_copy_bytes;

  if (context == NULL || type >= SVGA_COTABLE_MAX) {
    return false;
  };
  entry_size = vmsvga3d_dx_cotable_entry_size(type);
  if (entry_size == 0) {
    return false;
  };

  binding = &context->cotables[type];
  mob = vmsvga3d_mob_get(s, mobid);
  old_mob = vmsvga3d_mob_get(s, binding->mobid);
  if (mob != NULL) {
    if (valid_size > mob->gbo.size ||
        !vmsvga3d_mob_backing_create(s, mob, valid_size)) {
      return false;
    };
    capacity_entries = mob->gbo.size / entry_size;
    valid_entries = valid_size / entry_size;
  } else {
    valid_size = 0;
    capacity_entries = 0;
    valid_entries = 0;
    vmsvga3d_mob_backing_delete(old_mob);
  };

  grow_copy_bytes =
      grow && binding->mobid != SVGA3D_INVALID_ID && valid_entries != 0
          ? valid_size
          : 0;
  if (grow_copy_bytes != 0) {
    const void *source = vmsvga3d_mob_backing_ptr(old_mob, 0);
    void *destination = vmsvga3d_mob_backing_ptr(mob, 0);

    if (source == NULL || destination == NULL) {
      vmsvga3d_mob_backing_delete(mob);
      return false;
    };
    memcpy(destination, source, grow_copy_bytes);
  };

  binding->mobid = mob != NULL ? mob->mobid : SVGA3D_INVALID_ID;
  binding->valid_size = valid_size;
  binding->entry_size = entry_size;
  binding->capacity_entries = capacity_entries;
  binding->valid_entries = valid_entries;
  return true;
}

static void *vmsvga3d_dx_cotable_entry_ptr(struct vmsvga_state_s *s,
                                            uint32_t cid,
                                            SVGACOTableType type,
                                            uint32_t index) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  VMSVGA3DDXCOTable *binding;
  VMSVGA3DMob *mob;

  if (context == NULL || type >= SVGA_COTABLE_MAX) {
    return NULL;
  };
  binding = &context->cotables[type];
  if (index >= binding->valid_entries || binding->entry_size == 0) {
    return NULL;
  };
  mob = vmsvga3d_mob_get(s, binding->mobid);
  if (mob == NULL) {
    return NULL;
  };
  return vmsvga3d_mob_backing_ptr(mob, index * binding->entry_size);
}

static bool vmsvga3d_dx_cotable_readback(struct vmsvga_state_s *s,
                                         uint32_t cid,
                                         SVGACOTableType type) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  VMSVGA3DMob *mob;

  if (context == NULL || type >= SVGA_COTABLE_MAX) {
    return false;
  };
  mob = vmsvga3d_mob_get(s, context->cotables[type].mobid);
  return vmsvga3d_mob_backing_write(s, mob);
}

static bool vmsvga3d_dx_context_define_backed(struct vmsvga_state_s *s,
                                               uint32_t cid) {
  SVGAOTableDXContextEntry entry = {
      .cid = cid,
      .mobid = SVGA3D_INVALID_ID,
  };

  return vmsvga3d_dx_context_entry_write(s, cid, &entry) &&
         vmsvga3d_state_dx_context_define(s, cid);
}

static bool vmsvga3d_dx_context_destroy_backed(struct vmsvga_state_s *s,
                                                uint32_t cid) {
  SVGAOTableDXContextEntry entry = { 0 };

  (void)vmsvga3d_dx_context_entry_write(s, cid, &entry);
  return vmsvga3d_state_dx_context_destroy(s, cid);
}

static bool vmsvga3d_dx_context_bind_backed(
    struct vmsvga_state_s *s, const SVGA3dCmdDXBindContext *command) {
  SVGAOTableDXContextEntry entry;
  SVGADXContextMobFormat saved;
  const SVGADXContextMobFormat *valid_contents = NULL;
  VMSVGA3DMob *mob;

  if (command == NULL) {
    return false;
  };
  if (command->mobid != SVGA3D_INVALID_ID) {
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;

    if (state == NULL ||
        !vmsvga3d_otable_index_valid(&state->otables[SVGA_OTABLE_MOB],
                                     command->mobid,
                                     sizeof(SVGAOTableMobEntry))) {
      return false;
    };
  };
  if (!vmsvga3d_dx_context_entry_read(s, command->cid, &entry)) {
    return false;
  };

  if (command->mobid != entry.mobid && entry.mobid != SVGA3D_INVALID_ID) {
    if (!vmsvga3d_state_dx_context_readback(s, command->cid, &saved)) {
      return false;
    };
    mob = vmsvga3d_mob_get(s, entry.mobid);
    if (mob != NULL &&
        !vmsvga3d_mob_write(s, mob, 0, &saved, sizeof(saved))) {
      return false;
    };
  };

  if (command->mobid != SVGA3D_INVALID_ID) {
    if (command->validContents) {
      mob = vmsvga3d_mob_get(s, command->mobid);
      if (mob != NULL &&
          vmsvga3d_mob_read(s, mob, 0, &saved, sizeof(saved))) {
        valid_contents = &saved;
      };
    };
    (void)vmsvga3d_state_dx_context_bind(s, command->cid, valid_contents);
  };

  entry.mobid = command->mobid;
  return vmsvga3d_dx_context_entry_write(s, command->cid, &entry);
}

static bool vmsvga3d_dx_context_readback_backed(struct vmsvga_state_s *s,
                                                 uint32_t cid) {
  SVGAOTableDXContextEntry entry;
  SVGADXContextMobFormat contents;
  VMSVGA3DMob *mob;

  if (!vmsvga3d_dx_context_entry_read(s, cid, &entry)) {
    return false;
  };
  if (entry.mobid == SVGA3D_INVALID_ID) {
    return true;
  };
  mob = vmsvga3d_mob_get(s, entry.mobid);
  if (mob == NULL) {
    return true;
  };
  return vmsvga3d_state_dx_context_readback(s, cid, &contents) &&
         vmsvga3d_mob_write(s, mob, 0, &contents, sizeof(contents));
}

#define VMSVGA3D_WIRE_SET_OTABLE_BASE64_SIZE 24u
#define VMSVGA3D_WIRE_GROW_OTABLE_SIZE 24u
#define VMSVGA3D_WIRE_DEFINE_GB_MOB64_SIZE 20u

static bool vmsvga3d_handle_set_otable_base(struct vmsvga_state_s *s,
                                             uint32_t cmd, int32_t *len,
                                             uint32_t fifo_start) {
  void *payload;
  uint32_t size;

  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (cmd == SVGA_3D_CMD_SET_OTABLE_BASE &&
      size >= sizeof(SVGA3dCmdSetOTableBase)) {
    const SVGA3dCmdSetOTableBase *body = payload;

    (void)vmsvga3d_otable_set_or_grow(
        s, body->type, body->baseAddress, body->sizeInBytes,
        body->validSizeInBytes, body->ptDepth, false);
  } else if (cmd == SVGA_3D_CMD_SET_OTABLE_BASE64 &&
             size >= VMSVGA3D_WIRE_SET_OTABLE_BASE64_SIZE) {
    const uint8_t *body = payload;

    (void)vmsvga3d_otable_set_or_grow(
        s, (SVGAOTableType)ldl_le_p(body), ldq_le_p(body + 4),
        ldl_le_p(body + 12), ldl_le_p(body + 16),
        (SVGAMobFormat)ldl_le_p(body + 20), false);
  };
  g_free(payload);
  return true;
}

static bool vmsvga3d_handle_grow_otable(struct vmsvga_state_s *s,
                                        uint32_t cmd, int32_t *len,
                                        uint32_t fifo_start) {
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= VMSVGA3D_WIRE_GROW_OTABLE_SIZE) {
    const uint8_t *body = payload;

    (void)vmsvga3d_otable_set_or_grow(
        s, (SVGAOTableType)ldl_le_p(body), ldq_le_p(body + 4),
        ldl_le_p(body + 12), ldl_le_p(body + 16),
        (SVGAMobFormat)ldl_le_p(body + 20), true);
  };
  g_free(payload);
  return true;
}

static bool vmsvga3d_handle_define_gb_mob(struct vmsvga_state_s *s,
                                          uint32_t cmd, int32_t *len,
                                          uint32_t fifo_start) {
  void *payload;
  uint32_t size;

  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (cmd == SVGA_3D_CMD_DEFINE_GB_MOB &&
      size >= sizeof(SVGA3dCmdDefineGBMob)) {
    const SVGA3dCmdDefineGBMob *body = payload;

    (void)vmsvga3d_mob_define(s, body->mobid, body->ptDepth,
                              body->base, body->sizeInBytes);
  } else if (cmd == SVGA_3D_CMD_DEFINE_GB_MOB64 &&
             size >= VMSVGA3D_WIRE_DEFINE_GB_MOB64_SIZE) {
    const uint8_t *body = payload;

    (void)vmsvga3d_mob_define(
        s, ldl_le_p(body), (SVGAMobFormat)ldl_le_p(body + 4),
        ldq_le_p(body + 8), ldl_le_p(body + 16));
  };
  g_free(payload);
  return true;
}

static bool vmsvga3d_handle_destroy_gb_mob(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start) {
  SVGA3dCmdDestroyGBMob *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    (void)vmsvga3d_mob_destroy(s, body->mobid);
  };
  g_free(payload);
  return true;
}

static bool vmsvga3d_handle_dx_context_lifecycle(
    struct vmsvga_state_s *s, uint32_t cmd, int32_t *len,
    uint32_t fifo_start) {
  void *payload;
  uint32_t size;

  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  switch (cmd) {
  case SVGA_3D_CMD_DX_DEFINE_CONTEXT:
    if (size >= sizeof(SVGA3dCmdDXDefineContext)) {
      const SVGA3dCmdDXDefineContext *body = payload;
      (void)vmsvga3d_dx_context_define_backed(s, body->cid);
    };
    break;
  case SVGA_3D_CMD_DX_DESTROY_CONTEXT:
    if (size >= sizeof(SVGA3dCmdDXDestroyContext)) {
      const SVGA3dCmdDXDestroyContext *body = payload;
      (void)vmsvga3d_dx_context_destroy_backed(s, body->cid);
    };
    break;
  case SVGA_3D_CMD_DX_BIND_CONTEXT:
    if (size >= sizeof(SVGA3dCmdDXBindContext)) {
      (void)vmsvga3d_dx_context_bind_backed(s, payload);
    };
    break;
  case SVGA_3D_CMD_DX_READBACK_CONTEXT:
    if (size >= sizeof(SVGA3dCmdDXReadbackContext)) {
      const SVGA3dCmdDXReadbackContext *body = payload;
      (void)vmsvga3d_dx_context_readback_backed(s, body->cid);
    };
    break;
  default:
    break;
  };
  g_free(payload);
  return true;
}

static bool vmsvga3d_handle_dx_cotable(struct vmsvga_state_s *s,
                                       uint32_t cmd, int32_t *len,
                                       uint32_t fifo_start) {
  void *payload;
  uint32_t size;

  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  switch (cmd) {
  case SVGA_3D_CMD_DX_SET_COTABLE:
    if (size >= sizeof(SVGA3dCmdDXSetCOTable)) {
      const SVGA3dCmdDXSetCOTable *body = payload;
      (void)vmsvga3d_dx_cotable_set_or_grow(
          s, body->cid, body->mobid, body->type,
          body->validSizeInBytes, false);
    };
    break;
  case SVGA_3D_CMD_DX_GROW_COTABLE:
    if (size >= sizeof(SVGA3dCmdDXGrowCOTable)) {
      const SVGA3dCmdDXGrowCOTable *body = payload;
      (void)vmsvga3d_dx_cotable_set_or_grow(
          s, body->cid, body->mobid, body->type,
          body->validSizeInBytes, true);
    };
    break;
  case SVGA_3D_CMD_DX_READBACK_COTABLE:
    if (size >= sizeof(SVGA3dCmdDXReadbackCOTable)) {
      const SVGA3dCmdDXReadbackCOTable *body = payload;
      (void)vmsvga3d_dx_cotable_readback(s, body->cid, body->type);
    };
    break;
  default:
    break;
  };
  g_free(payload);
  return true;
}

#define VMSVGA3D_STALL(cmd) \
  { (cmd), VMSVGA3D_COMMAND_STALL, NULL, #cmd }
#define VMSVGA3D_DISCARD(cmd) \
  { (cmd), VMSVGA3D_COMMAND_DISCARD, NULL, #cmd }
#define VMSVGA3D_HANDLER(cmd, fn) \
  { (cmd), VMSVGA3D_COMMAND_DISCARD, (fn), #cmd }

static const VMSVGA3DCommandInfo vmsvga3d_commands[] = {
  VMSVGA3D_STALL(SVGA_3D_CMD_LEGACY_BASE),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SURFACE_DEFINE, vmsvga3d_handle_surface_define),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SURFACE_DESTROY, vmsvga3d_handle_surface_destroy),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SURFACE_COPY, vmsvga3d_handle_surface_copy),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SURFACE_STRETCHBLT, vmsvga3d_handle_surface_stretchblt),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SURFACE_DMA, vmsvga3d_handle_surface_dma),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_CONTEXT_DEFINE, vmsvga3d_handle_context_define),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_CONTEXT_DESTROY, vmsvga3d_handle_context_destroy),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETTRANSFORM, vmsvga3d_handle_set_transform),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETZRANGE, vmsvga3d_handle_set_z_range),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETRENDERSTATE, vmsvga3d_handle_set_render_state),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETRENDERTARGET, vmsvga3d_handle_set_render_target),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETTEXTURESTATE, vmsvga3d_handle_set_texture_state),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETMATERIAL, vmsvga3d_handle_set_material),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETLIGHTDATA, vmsvga3d_handle_set_light_data),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETLIGHTENABLED, vmsvga3d_handle_set_light_enabled),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETVIEWPORT, vmsvga3d_handle_set_viewport),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETCLIPPLANE, vmsvga3d_handle_set_clip_plane),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_CLEAR, vmsvga3d_handle_clear),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_PRESENT, vmsvga3d_handle_present),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SHADER_DEFINE, vmsvga3d_handle_shader_define),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SHADER_DESTROY, vmsvga3d_handle_shader_destroy),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SET_SHADER, vmsvga3d_handle_set_shader),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SET_SHADER_CONST, vmsvga3d_handle_set_shader_const),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DRAW_PRIMITIVES, vmsvga3d_handle_draw_primitives),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SETSCISSORRECT, vmsvga3d_handle_set_scissor),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_BEGIN_QUERY, vmsvga3d_handle_begin_query),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_END_QUERY, vmsvga3d_handle_end_query),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_WAIT_FOR_QUERY, vmsvga3d_handle_wait_for_query),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_PRESENT_READBACK,
                   vmsvga3d_handle_present_readback),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN,
                   vmsvga3d_handle_blit_surface_to_screen),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SURFACE_DEFINE_V2, vmsvga3d_handle_surface_define_v2),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_GENERATE_MIPMAPS, vmsvga3d_handle_generate_mipmaps),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD4),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD5),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD6),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD7),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD8),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD9),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD10),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD11),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_ACTIVATE_SURFACE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DEACTIVATE_SURFACE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SCREEN_DMA),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD1),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD2),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD12),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD13),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD14),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD15),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD16),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD17),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SET_OTABLE_BASE, vmsvga3d_handle_set_otable_base),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_READBACK_OTABLE),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_MOB, vmsvga3d_handle_define_gb_mob),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DESTROY_GB_MOB, vmsvga3d_handle_destroy_gb_mob),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEAD3),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_UPDATE_GB_MOB_MAPPING),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DEFINE_GB_SURFACE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DESTROY_GB_SURFACE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_BIND_GB_SURFACE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_COND_BIND_GB_SURFACE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_UPDATE_GB_IMAGE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_UPDATE_GB_SURFACE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_READBACK_GB_IMAGE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_READBACK_GB_SURFACE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_INVALIDATE_GB_IMAGE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_INVALIDATE_GB_SURFACE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DEFINE_GB_CONTEXT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DESTROY_GB_CONTEXT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_BIND_GB_CONTEXT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_READBACK_GB_CONTEXT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_INVALIDATE_GB_CONTEXT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DEFINE_GB_SHADER),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DESTROY_GB_SHADER),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_BIND_GB_SHADER),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SET_OTABLE_BASE64, vmsvga3d_handle_set_otable_base),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_BEGIN_GB_QUERY),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_END_GB_QUERY),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_WAIT_FOR_GB_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_NOP),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_ENABLE_GART),
  VMSVGA3D_STALL(SVGA_3D_CMD_DISABLE_GART),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_MAP_MOB_INTO_GART),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_UNMAP_GART_RANGE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DEFINE_GB_SCREENTARGET),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DESTROY_GB_SCREENTARGET),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_BIND_GB_SCREENTARGET),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_UPDATE_GB_SCREENTARGET),
  VMSVGA3D_STALL(SVGA_3D_CMD_READBACK_GB_IMAGE_PARTIAL),
  VMSVGA3D_STALL(SVGA_3D_CMD_INVALIDATE_GB_IMAGE_PARTIAL),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SET_GB_SHADERCONSTS_INLINE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_GB_SCREEN_DMA),
  VMSVGA3D_STALL(SVGA_3D_CMD_BIND_GB_SURFACE_WITH_PITCH),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_GB_MOB_FENCE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DEFINE_GB_SURFACE_V2),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_MOB64, vmsvga3d_handle_define_gb_mob),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_REDEFINE_GB_MOB64),
  VMSVGA3D_STALL(SVGA_3D_CMD_NOP_ERROR),
  VMSVGA3D_STALL(SVGA_3D_CMD_SET_VERTEX_STREAMS),
  VMSVGA3D_STALL(SVGA_3D_CMD_SET_VERTEX_DECLS),
  VMSVGA3D_STALL(SVGA_3D_CMD_SET_VERTEX_DIVISORS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DRAW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DRAW_INDEXED),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DX_DEFINE_CONTEXT, vmsvga3d_handle_dx_context_lifecycle),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DX_DESTROY_CONTEXT, vmsvga3d_handle_dx_context_lifecycle),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DX_BIND_CONTEXT, vmsvga3d_handle_dx_context_lifecycle),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DX_READBACK_CONTEXT, vmsvga3d_handle_dx_context_lifecycle),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_INVALIDATE_CONTEXT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_SINGLE_CONSTANT_BUFFER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_SHADER_RESOURCES),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_SHADER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_SAMPLERS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DRAW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DRAW_INDEXED),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DRAW_INSTANCED),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DRAW_AUTO),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_INPUT_LAYOUT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_VERTEX_BUFFERS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_INDEX_BUFFER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_TOPOLOGY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_RENDERTARGETS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_BLEND_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_DEPTHSTENCIL_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_RASTERIZER_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BIND_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_QUERY_OFFSET),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BEGIN_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_END_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_READBACK_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_PREDICATION),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_SOTARGETS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_VIEWPORTS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_SCISSORRECTS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_CLEAR_RENDERTARGET_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_CLEAR_DEPTHSTENCIL_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_PRED_COPY_REGION),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_PRED_COPY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_PRESENTBLT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_GENMIPS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_UPDATE_SUBRESOURCE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_READBACK_SUBRESOURCE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_INVALIDATE_SUBRESOURCE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_SHADERRESOURCE_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_SHADERRESOURCE_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_RENDERTARGET_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_RENDERTARGET_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_ELEMENTLAYOUT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_ELEMENTLAYOUT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_BLEND_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_BLEND_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_RASTERIZER_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_RASTERIZER_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_SAMPLER_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_SAMPLER_STATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_SHADER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_SHADER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BIND_SHADER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_STREAMOUTPUT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_STREAMOUTPUT),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DX_SET_COTABLE, vmsvga3d_handle_dx_cotable),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DX_READBACK_COTABLE, vmsvga3d_handle_dx_cotable),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BUFFER_COPY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_TRANSFER_FROM_BUFFER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SURFACE_COPY_AND_READBACK),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_MOVE_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BIND_ALL_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_READBACK_ALL_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_PRED_TRANSFER_FROM_BUFFER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_MOB_FENCE_64),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BIND_ALL_SHADER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_HINT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BUFFER_UPDATE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_VS_CONSTANT_BUFFER_OFFSET),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_PS_CONSTANT_BUFFER_OFFSET),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_GS_CONSTANT_BUFFER_OFFSET),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_HS_CONSTANT_BUFFER_OFFSET),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_DS_CONSTANT_BUFFER_OFFSET),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_CS_CONSTANT_BUFFER_OFFSET),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_COND_BIND_ALL_SHADER),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SCREEN_COPY),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_GROW_OTABLE, vmsvga3d_handle_grow_otable),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_DX_GROW_COTABLE, vmsvga3d_handle_dx_cotable),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_INTRA_SURFACE_COPY),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DEFINE_GB_SURFACE_V3),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_RESOLVE_COPY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_PRED_RESOLVE_COPY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_PRED_CONVERT_REGION),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_PRED_CONVERT),
  VMSVGA3D_STALL(SVGA_3D_CMD_WHOLE_SURFACE_COPY),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_UA_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_UA_VIEW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_CLEAR_UA_VIEW_UINT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_CLEAR_UA_VIEW_FLOAT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_COPY_STRUCTURE_COUNT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_UA_VIEWS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED_INDIRECT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DRAW_INSTANCED_INDIRECT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DISPATCH),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DISPATCH_INDIRECT),
  VMSVGA3D_STALL(SVGA_3D_CMD_WRITE_ZERO_SURFACE),
  VMSVGA3D_STALL(SVGA_3D_CMD_HINT_ZERO_SURFACE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_TRANSFER_TO_BUFFER),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_STRUCTURE_COUNT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_BITBLT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_TRANSBLT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_STRETCHBLT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_COLORFILL),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_ALPHABLEND),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_CLEARTYPEBLEND),
  VMSVGA3D_STALL(SVGA_3D_CMD_DEFINE_GB_SURFACE_V4),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_CS_UA_VIEWS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_MIN_LOD),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW_V2),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT_WITH_MOB),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_SHADER_IFACE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BIND_STREAMOUTPUT),
  VMSVGA3D_STALL(SVGA_3D_CMD_SURFACE_STRETCHBLT_NON_MS_TO_MS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BIND_SHADER_IFACE),
  VMSVGA3D_STALL(SVGA_3D_CMD_MAX),
  VMSVGA3D_STALL(SVGA_3D_CMD_FUTURE_MAX),
};

#undef VMSVGA3D_DISCARD
#undef VMSVGA3D_STALL

static const VMSVGA3DCommandInfo *vmsvga3d_command_info(uint32_t cmd) {
  size_t lo = 0;
  size_t hi = ARRAY_SIZE(vmsvga3d_commands);

  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    uint32_t current = vmsvga3d_commands[mid].command;

    if (cmd < current) {
      hi = mid;
    } else if (cmd > current) {
      lo = mid + 1;
    } else {
      return &vmsvga3d_commands[mid];
    };
  };

  return NULL;
};

static bool vmsvga3d_trace_fifo_command(uint32_t cmd) {
  switch (cmd) {
  case SVGA_3D_CMD_CONTEXT_DEFINE:
  case SVGA_3D_CMD_SURFACE_DEFINE:
  case SVGA_3D_CMD_SURFACE_DEFINE_V2:
  case SVGA_3D_CMD_DRAW_PRIMITIVES:
  case SVGA_3D_CMD_PRESENT:
    return true;
  default:
    return cmd >= SVGA_3D_CMD_DX_MIN && cmd <= SVGA_3D_CMD_DX_MAX;
  };
};

static const char *vmsvga3d_trace_command_path(uint32_t cmd) {
  if (cmd >= SVGA_3D_CMD_DX_MIN && cmd <= SVGA_3D_CMD_DX_MAX) {
    return "DX";
  };
  return "D3D9";
};

static const char *vmsvga3d_trace_command_action(
    const VMSVGA3DCommandInfo *info) {
  if (info->handler != NULL) {
    return "HANDLE";
  };
  if (info->action == VMSVGA3D_COMMAND_STALL) {
    return "STALL";
  };
  return "DISCARD";
};

static bool vmsvga3d_fifo_command(struct vmsvga_state_s *s, uint32_t cmd,
                                   int32_t *len, uint32_t fifo_start) {
  const VMSVGA3DCommandInfo *info;

  if (!s->svga3d_capable) {
    return false;
  };
  if (cmd < SVGA_3D_CMD_LEGACY_BASE || cmd > SVGA_3D_CMD_FUTURE_MAX) {
    return false;
  };

  info = vmsvga3d_command_info(cmd);
  if (info == NULL) {
    return false;
  };

  if (vmsvga3d_trace_fifo_command(cmd)) {
    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "3D-CMD path=%s name=%s id=%u action=%s fifo=0x%08x",
        vmsvga3d_trace_command_path(cmd), info->name, cmd,
        vmsvga3d_trace_command_action(info), fifo_start);
  };

  if (info->handler != NULL) {
    return info->handler(s, cmd, len, fifo_start);
  };

  if (info->action == VMSVGA3D_COMMAND_STALL) {
    vmsvga3d_fifo_rewind(s, len, fifo_start);
    VPRINT("%s command %u in SVGA command FIFO\n", info->name, cmd);
    return true;
  };

  if (!vmsvga3d_fifo_discard_packet(s, len, fifo_start)) {
    VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
    return true;
  };
  VPRINT("%s command %u in SVGA command FIFO\n", info->name, cmd);
  return true;
};

/*
 * Legacy devcaps (0..SVGA3D_DEVCAP_DEAD2) describe the intended final D3D9
 * profile.  They are deliberately not reduced merely because execution still
 * needs the future host renderer.  The capability audit tracks those gaps.
 * DXCONTEXT is separate: it enables the vGPU10 command model and remains off
 * until that protocol is deliberately wired.
 */
static uint32_t vmsvga3d_devcap[SVGA3D_DEVCAP_MAX] = {
    [SVGA3D_DEVCAP_3D] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_LIGHTS] = 0x00000008,
    [SVGA3D_DEVCAP_MAX_TEXTURES] = 0x00000008,
    [SVGA3D_DEVCAP_MAX_CLIP_PLANES] = 0x00000006,
    [SVGA3D_DEVCAP_VERTEX_SHADER_VERSION] = 0x00000007,
    [SVGA3D_DEVCAP_VERTEX_SHADER] = 0x00000001,
    [SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION] = 0x0000000d,
    [SVGA3D_DEVCAP_FRAGMENT_SHADER] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_RENDER_TARGETS] = 0x00000004,
    [SVGA3D_DEVCAP_S23E8_TEXTURES] = 0x00000001,
    [SVGA3D_DEVCAP_S10E5_TEXTURES] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND] = 0x00000004,
    [SVGA3D_DEVCAP_D16_BUFFER_FORMAT] = 0x00000001,
    [SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT] = 0x00000001,
    [SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT] = 0x00000001,
    [SVGA3D_DEVCAP_QUERY_TYPES] = 0x00000001,
    [SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_POINT_SIZE] = 0x433d0000, /* 189.0f */
    [SVGA3D_DEVCAP_MAX_SHADER_TEXTURES] = 0x00000014,
    //        [SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH] = 0x00008000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH] = 0x00002000,
    //        [SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT] = 0x00008000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT] = 0x00002000,
    [SVGA3D_DEVCAP_MAX_VOLUME_EXTENT] = 0x00002000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT] = 0x00002000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO] = 0x00002000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY] = 0x00000010,
    [SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT] = 0x001fffff,
    [SVGA3D_DEVCAP_MAX_VERTEX_INDEX] = 0x000fffff,
    [SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS] = 0x00008000,
    [SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS] = 0x00008000,
    [SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS] = 0x00000020,
    [SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS] = 0x00000020,
    [SVGA3D_DEVCAP_TEXTURE_OPS] = 0x03ffdfff, /* All except DSDT. */
    [SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8] = 0x0018ec1f,
    [SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8] = 0x0018e11f,
    [SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10] = 0x0008601f,
    [SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5] = 0x0008601f,
    [SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5] = 0x0008611f,
    [SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4] = 0x0000611f,
    [SVGA3D_DEVCAP_SURFACEFMT_R5G6B5] = 0x0018ec1f,
    [SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16] = 0x0000601f,
    [SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8] = 0x00006007,
    [SVGA3D_DEVCAP_SURFACEFMT_ALPHA8] = 0x0000601f,
    [SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8] = 0x0000601f,
    [SVGA3D_DEVCAP_SURFACEFMT_Z_D16] = 0x000040c5,
    [SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8] = 0x000040c5,
    [SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8] = 0x000040c5,
    [SVGA3D_DEVCAP_SURFACEFMT_DXT1] = 0x0000e005,
    [SVGA3D_DEVCAP_SURFACEFMT_DXT2] = 0x0000e005,
    [SVGA3D_DEVCAP_SURFACEFMT_DXT3] = 0x0000e005,
    [SVGA3D_DEVCAP_SURFACEFMT_DXT4] = 0x0000e005,
    [SVGA3D_DEVCAP_SURFACEFMT_DXT5] = 0x0000e005,
    [SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8] = 0x00014005,
    [SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10] = 0x00014007,
    [SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8] = 0x00014007,
    [SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8] = 0x00014005,
    [SVGA3D_DEVCAP_SURFACEFMT_CxV8U8] = 0x00014001,
    [SVGA3D_DEVCAP_SURFACEFMT_R_S10E5] = 0x0080601f,
    [SVGA3D_DEVCAP_SURFACEFMT_R_S23E8] = 0x0080601f,
    [SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5] = 0x0080601f,
    [SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8] = 0x0080601f,
    [SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5] = 0x0080601f,
    [SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8] = 0x0080601f,
    [SVGA3D_DEVCAP_MISSING62] = 0x00000000,
    [SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES] = 0x00000004,
    [SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS] = 0x00000004,
    [SVGA3D_DEVCAP_SURFACEFMT_V16U16] = 0x00014007,
    [SVGA3D_DEVCAP_SURFACEFMT_G16R16] = 0x0000601f,
    [SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16] = 0x0000601f,
    [SVGA3D_DEVCAP_SURFACEFMT_UYVY] = 0x01246000,
    [SVGA3D_DEVCAP_SURFACEFMT_YUY2] = 0x01246000,
    [SVGA3D_DEVCAP_DEAD4] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD5] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD7] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD6] = 0x00000000,
    [SVGA3D_DEVCAP_AUTOGENMIPMAPS] = 0x00000001,
    [SVGA3D_DEVCAP_SURFACEFMT_NV12] = 0x01246000,
    [SVGA3D_DEVCAP_SURFACEFMT_AYUV] = 0x00000000,
    [SVGA3D_DEVCAP_MAX_CONTEXT_IDS] = 0x00000100,
    [SVGA3D_DEVCAP_MAX_SURFACE_IDS] = 0x00008000,
    [SVGA3D_DEVCAP_SURFACEFMT_Z_DF16] = 0x00000000,
    [SVGA3D_DEVCAP_SURFACEFMT_Z_DF24] = 0x000040c5,
    [SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT] = 0x000040c5,
    [SVGA3D_DEVCAP_SURFACEFMT_ATI1] = 0x00000000,
    [SVGA3D_DEVCAP_SURFACEFMT_ATI2] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD1] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD8] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD9] = 0x00000000,
    [SVGA3D_DEVCAP_LINE_AA] = 0x00000001,
    [SVGA3D_DEVCAP_LINE_STIPPLE] = 0x00000000,
    [SVGA3D_DEVCAP_MAX_LINE_WIDTH] = 0x3f800000, /* 1.0f */
    [SVGA3D_DEVCAP_MAX_AA_LINE_WIDTH] = 0x3f800000, /* 1.0f */
    [SVGA3D_DEVCAP_SURFACEFMT_YV12] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD3] = 0x00000000,
    [SVGA3D_DEVCAP_TS_COLOR_KEY] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD2] = 0x00000000,
    [SVGA3D_DEVCAP_DXCONTEXT] = 0x00000000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_ARRAY_SIZE] = 0x00000000,
    [SVGA3D_DEVCAP_DX_MAX_VERTEXBUFFERS] = 0x00000010,
    [SVGA3D_DEVCAP_DX_MAX_CONSTANT_BUFFERS] = 0x0000000f,
    [SVGA3D_DEVCAP_DX_PROVOKING_VERTEX] = 0x00000001,
    [SVGA3D_DEVCAP_DXFMT_X8R8G8B8] = 0x000002f7,
    [SVGA3D_DEVCAP_DXFMT_A8R8G8B8] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R5G6B5] = 0x000002f7,
    [SVGA3D_DEVCAP_DXFMT_X1R5G5B5] = 0x000000f7,
    [SVGA3D_DEVCAP_DXFMT_A1R5G5B5] = 0x000000f7,
    [SVGA3D_DEVCAP_DXFMT_A4R4G4B4] = 0x000000f7,
    [SVGA3D_DEVCAP_DXFMT_Z_D32] = 0x00000009,
    [SVGA3D_DEVCAP_DXFMT_Z_D16] = 0x0000026b,
    [SVGA3D_DEVCAP_DXFMT_Z_D24S8] = 0x0000026b,
    [SVGA3D_DEVCAP_DXFMT_Z_D15S1] = 0x0000000b,
    [SVGA3D_DEVCAP_DXFMT_LUMINANCE8] = 0x000000f7,
    [SVGA3D_DEVCAP_DXFMT_LUMINANCE4_ALPHA4] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_LUMINANCE16] = 0x000000f7,
    [SVGA3D_DEVCAP_DXFMT_LUMINANCE8_ALPHA8] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_DXT1] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_DXT2] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_DXT3] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_DXT4] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_DXT5] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_BUMPU8V8] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BUMPL6V5U5] = 0x00000000,
    [SVGA3D_DEVCAP_DXFMT_BUMPX8L8V8U8] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD1] = 0x00000000,
    [SVGA3D_DEVCAP_DXFMT_ARGB_S10E5] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_ARGB_S23E8] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_A2R10G10B10] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_V8U8] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_Q8W8V8U8] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_CxV8U8] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_X8L8V8U8] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_A2W10V10U10] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_ALPHA8] = 0x000000f7,
    [SVGA3D_DEVCAP_DXFMT_R_S10E5] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R_S23E8] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_RG_S10E5] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_RG_S23E8] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_BUFFER] = 0x00000001,
    [SVGA3D_DEVCAP_DXFMT_Z_D24X8] = 0x0000026b,
    [SVGA3D_DEVCAP_DXFMT_V16U16] = 0x000001e3,
    [SVGA3D_DEVCAP_DXFMT_G16R16] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_A16B16G16R16] = 0x000001f7,
    [SVGA3D_DEVCAP_DXFMT_UYVY] = 0x00000001,
    [SVGA3D_DEVCAP_DXFMT_YUY2] = 0x00000041,
    [SVGA3D_DEVCAP_DXFMT_NV12] = 0x00000041,
    [SVGA3D_DEVCAP_FORMAT_DEAD2] = 0x00000000,
    [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_SINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R32G32B32_TYPELESS] = 0x000000e1,
    [SVGA3D_DEVCAP_DXFMT_R32G32B32_FLOAT] = 0x000001e3,
    [SVGA3D_DEVCAP_DXFMT_R32G32B32_UINT] = 0x000001e3,
    [SVGA3D_DEVCAP_DXFMT_R32G32B32_SINT] = 0x000001e3,
    [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R32G32_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_R32G32_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R32G32_SINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R32G8X24_TYPELESS] = 0x00000261,
    [SVGA3D_DEVCAP_DXFMT_D32_FLOAT_S8X24_UINT] = 0x00000269,
    [SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_X32_G8X24_UINT] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_R10G10B10A2_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R11G11B10_FLOAT] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM_SRGB] = 0x000002f7,
    [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R16G16_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_R16G16_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R16G16_SINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R32_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_D32_FLOAT] = 0x00000269,
    [SVGA3D_DEVCAP_DXFMT_R32_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R32_SINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R24G8_TYPELESS] = 0x00000261,
    [SVGA3D_DEVCAP_DXFMT_D24_UNORM_S8_UINT] = 0x00000269,
    [SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_X24_G8_UINT] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_R8G8_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_R8G8_UNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R8G8_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R8G8_SINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R16_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_R16_UNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R16_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R16_SNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R16_SINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R8_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_R8_UNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R8_UINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_R8_SNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R8_SINT] = 0x000003e7,
    [SVGA3D_DEVCAP_DXFMT_P8] = 0x00000001,
    [SVGA3D_DEVCAP_DXFMT_R9G9B9E5_SHAREDEXP] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_R8G8_B8G8_UNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_G8R8_G8B8_UNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC1_TYPELESS] = 0x000000e1,
    [SVGA3D_DEVCAP_DXFMT_BC1_UNORM_SRGB] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC2_TYPELESS] = 0x000000e1,
    [SVGA3D_DEVCAP_DXFMT_BC2_UNORM_SRGB] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC3_TYPELESS] = 0x000000e1,
    [SVGA3D_DEVCAP_DXFMT_BC3_UNORM_SRGB] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC4_TYPELESS] = 0x000000e1,
    [SVGA3D_DEVCAP_DXFMT_ATI1] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_BC4_SNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC5_TYPELESS] = 0x000000e1,
    [SVGA3D_DEVCAP_DXFMT_ATI2] = 0x00000063,
    [SVGA3D_DEVCAP_DXFMT_BC5_SNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_R10G10B10_XR_BIAS_A2_UNORM] = 0x00000045,
    [SVGA3D_DEVCAP_DXFMT_B8G8R8A8_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM_SRGB] = 0x000002f7,
    [SVGA3D_DEVCAP_DXFMT_B8G8R8X8_TYPELESS] = 0x000002e1,
    [SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM_SRGB] = 0x000002f7,
    [SVGA3D_DEVCAP_DXFMT_Z_DF16] = 0x0000006b,
    [SVGA3D_DEVCAP_DXFMT_Z_DF24] = 0x0000006b,
    [SVGA3D_DEVCAP_DXFMT_Z_D24S8_INT] = 0x0000006b,
    [SVGA3D_DEVCAP_DXFMT_YV12] = 0x00000001,
    [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_FLOAT] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_FLOAT] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R32G32_FLOAT] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R16G16_FLOAT] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R16G16_UNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R16G16_SNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R32_FLOAT] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R8G8_SNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_R16_FLOAT] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_D16_UNORM] = 0x00000269,
    [SVGA3D_DEVCAP_DXFMT_A8_UNORM] = 0x000002f7,
    [SVGA3D_DEVCAP_DXFMT_BC1_UNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC2_UNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC3_UNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_B5G6R5_UNORM] = 0x000002f7,
    [SVGA3D_DEVCAP_DXFMT_B5G5R5A1_UNORM] = 0x000002f7,
    [SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM] = 0x000003f7,
    [SVGA3D_DEVCAP_DXFMT_BC4_UNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC5_UNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_SM41] = 0x00000001,
    [SVGA3D_DEVCAP_MULTISAMPLE_2X] = 0x00000001,
    [SVGA3D_DEVCAP_MULTISAMPLE_4X] = 0x00000001,
    [SVGA3D_DEVCAP_MS_FULL_QUALITY] = 0x00000001,
    [SVGA3D_DEVCAP_LOGICOPS] = 0x00000001,
    [SVGA3D_DEVCAP_LOGIC_BLENDOPS] = 0x00000001,
    [SVGA3D_DEVCAP_DEAD12] = 0x00000000,
    [SVGA3D_DEVCAP_DXFMT_BC6H_TYPELESS] = 0x000000e1,
    [SVGA3D_DEVCAP_DXFMT_BC6H_UF16] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC6H_SF16] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC7_TYPELESS] = 0x000000e1,
    [SVGA3D_DEVCAP_DXFMT_BC7_UNORM] = 0x000000e3,
    [SVGA3D_DEVCAP_DXFMT_BC7_UNORM_SRGB] = 0x000000e3,
    [SVGA3D_DEVCAP_DEAD13] = 0x00000000,
    [SVGA3D_DEVCAP_SM5] = 0x00000001,
    [SVGA3D_DEVCAP_MULTISAMPLE_8X] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_FORCED_SAMPLE_COUNT] = 0x00000010,
    [SVGA3D_DEVCAP_GL43] = 0x00000001};

static uint32_t vmsvga3d_get_devcap(uint32_t index) {
  /* Keep the guest-visible profile strictly legacy VGPU9/D3D9.  D3D10/11
   * translators may exist internally, but DXCONTEXT and all later devcaps
   * remain hidden until their FIFO command model is actually exposed. */
  if (index >= SVGA3D_DEVCAP_DXCONTEXT) {
    return 0;
  }
  return vmsvga3d_devcap[index];
};

static void vmsvga3d_publish_fifo_caps(struct vmsvga_state_s *s) {
  uint32_t *caps;
  uint32_t length;
  uint32_t i;

  if (!vmsvga_fifo_has_reg(s, SVGA_FIFO_3D_CAPS_LAST)) {
    return;
  };

  caps = &s->fifo[SVGA_FIFO_3D_CAPS];
  memset(caps, 0, SVGA_FIFO_3D_CAPS_SIZE * sizeof(*caps));
  if (!s->svga3d_capable) {
    return;
  };

  length = (sizeof(SVGA3dCapsRecordHeader) +
            SVGA3D_DEVCAP_DEAD1 * sizeof(SVGA3dCapPair)) /
           sizeof(uint32_t);
  caps[0] = cpu_to_le32(length);
  caps[1] = cpu_to_le32(SVGA3DCAPS_RECORD_DEVCAPS);

  for (i = 0; i < SVGA3D_DEVCAP_DEAD1; i++) {
    caps[2 + i * 2] = cpu_to_le32(i);
    caps[3 + i * 2] = cpu_to_le32(vmsvga3d_get_devcap(i));
  };
};
