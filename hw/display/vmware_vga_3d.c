/*

 QEMU VMware Super Video Graphics Array 2 [SVGA-II]

 Copyright (c) 2023-2026 Christopher Eric Lentocha
 <christopherericlentocha@gmail.com>

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
#include "include/VGPU10ShaderTokens.h" // Required to be the last #include

typedef struct {
  SVGA3dSize size;
} SVGA3dCmdSize;

typedef struct vmsvga3d_context_s {
  uint32_t cid;
} VMSVGA3DContext;

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
} VMSVGA3DSurface;

struct vmsvga3d_state_s {
  VMSVGA3DContext *contexts[SVGA3D_MAX_CONTEXT_IDS];
  VMSVGA3DSurface *surfaces[SVGA3D_MAX_SURFACE_IDS];
  size_t surface_bytes;
};

static void vmsvga3d_surface_free(VMSVGA3DSurface *surface) {
  uint32_t i;

  if (surface == NULL) {
    return;
  };
  for (i = 0; i < surface->mip_count; i++) {
    g_free(surface->mips[i].data);
  };
  g_free(surface->mips);
  g_free(surface);
};

static struct vmsvga3d_state_s *
vmsvga3d_state_ensure(struct vmsvga_state_s *s) {
  if (s->svga3d == NULL) {
    s->svga3d = g_try_new0(struct vmsvga3d_state_s, 1);
  };
  return s->svga3d;
};

static void vmsvga3d_reset(struct vmsvga_state_s *s) {
  struct vmsvga3d_state_s *state = s->svga3d;
  uint32_t i;

  if (state == NULL) {
    return;
  };
  for (i = 0; i < SVGA3D_MAX_CONTEXT_IDS; i++) {
    g_free(state->contexts[i]);
  };
  for (i = 0; i < SVGA3D_MAX_SURFACE_IDS; i++) {
    vmsvga3d_surface_free(state->surfaces[i]);
  };
  g_free(state);
  s->svga3d = NULL;
};

static bool vmsvga3d_fifo_supported_command(uint32_t cmd) {
  switch (cmd) {
  case SVGA_3D_CMD_SURFACE_DEFINE:
  case SVGA_3D_CMD_SURFACE_DEFINE_V2:
  case SVGA_3D_CMD_SURFACE_DESTROY:
  case SVGA_3D_CMD_SURFACE_DMA:
  case SVGA_3D_CMD_CONTEXT_DEFINE:
  case SVGA_3D_CMD_CONTEXT_DESTROY:
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
  struct vmsvga3d_state_s *state;
  VMSVGA3DContext *context;
  SVGA3dCmdDefineContext *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    if (body->cid < SVGA3D_MAX_CONTEXT_IDS) {
      context = g_try_new0(VMSVGA3DContext, 1);
      if (context != NULL) {
        state = vmsvga3d_state_ensure(s);
        if (state != NULL) {
          context->cid = body->cid;
          g_free(state->contexts[body->cid]);
          state->contexts[body->cid] = context;
          context = NULL;
        };
        g_free(context);
      };
    };
  };
  g_free(payload);
  return true;
};

static bool vmsvga3d_handle_context_destroy(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start) {
  struct vmsvga3d_state_s *state;
  SVGA3dCmdDestroyContext *body;
  void *payload;
  uint32_t size;

  (void)cmd;
  if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
    return true;
  };
  if (size >= sizeof(*body)) {
    body = payload;
    state = s->svga3d;
    if (state != NULL && body->cid < SVGA3D_MAX_CONTEXT_IDS) {
      g_free(state->contexts[body->cid]);
      state->contexts[body->cid] = NULL;
    };
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
  if (valid && !vmsvga3d_surface_image(surface, &body->host, &image)) {
    valid = false;
  };

  for (i = 0; valid && i < box_count; i++) {
    valid = vmsvga3d_surface_dma_box(s, surface, image, &body->guest,
                                     body->transfer, &boxes[i],
                                     maximum_offset, false);
  };
  for (i = 0; valid && i < box_count; i++) {
    valid = vmsvga3d_surface_dma_box(s, surface, image, &body->guest,
                                     body->transfer, &boxes[i],
                                     maximum_offset, true);
  };

  g_free(payload);
  return true;
};

static uint32_t vmsvga3d_negotiate_hwversion(uint32_t guest_hwversion) {
  if (guest_hwversion < SVGA3D_HWVERSION_WS65_B1) {
    return 0;
  };
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
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SURFACE_COPY),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SURFACE_STRETCHBLT),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SURFACE_DMA, vmsvga3d_handle_surface_dma),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_CONTEXT_DEFINE, vmsvga3d_handle_context_define),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_CONTEXT_DESTROY, vmsvga3d_handle_context_destroy),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETTRANSFORM),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETZRANGE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETRENDERSTATE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETRENDERTARGET),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETTEXTURESTATE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETMATERIAL),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETLIGHTDATA),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETLIGHTENABLED),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETVIEWPORT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETCLIPPLANE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_CLEAR),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_PRESENT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SHADER_DEFINE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SHADER_DESTROY),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SET_SHADER),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SET_SHADER_CONST),
  VMSVGA3D_STALL(SVGA_3D_CMD_DRAW_PRIMITIVES),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SETSCISSORRECT),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_BEGIN_QUERY),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_END_QUERY),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_WAIT_FOR_QUERY),
  VMSVGA3D_STALL(SVGA_3D_CMD_PRESENT_READBACK),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN),
  VMSVGA3D_HANDLER(SVGA_3D_CMD_SURFACE_DEFINE_V2, vmsvga3d_handle_surface_define_v2),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_GENERATE_MIPMAPS),
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
  VMSVGA3D_DISCARD(SVGA_3D_CMD_SET_OTABLE_BASE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_READBACK_OTABLE),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DEFINE_GB_MOB),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_DESTROY_GB_MOB),
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
  VMSVGA3D_STALL(SVGA_3D_CMD_SET_OTABLE_BASE64),
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
  VMSVGA3D_STALL(SVGA_3D_CMD_DEFINE_GB_MOB64),
  VMSVGA3D_DISCARD(SVGA_3D_CMD_REDEFINE_GB_MOB64),
  VMSVGA3D_STALL(SVGA_3D_CMD_NOP_ERROR),
  VMSVGA3D_STALL(SVGA_3D_CMD_SET_VERTEX_STREAMS),
  VMSVGA3D_STALL(SVGA_3D_CMD_SET_VERTEX_DECLS),
  VMSVGA3D_STALL(SVGA_3D_CMD_SET_VERTEX_DIVISORS),
  VMSVGA3D_STALL(SVGA_3D_CMD_DRAW),
  VMSVGA3D_STALL(SVGA_3D_CMD_DRAW_INDEXED),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DEFINE_CONTEXT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_DESTROY_CONTEXT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_BIND_CONTEXT),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_READBACK_CONTEXT),
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
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_COTABLE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_READBACK_COTABLE),
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
  VMSVGA3D_DISCARD(SVGA_3D_CMD_GROW_OTABLE),
  VMSVGA3D_STALL(SVGA_3D_CMD_DX_GROW_COTABLE),
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

static bool vmsvga3d_fifo_command(struct vmsvga_state_s *s, uint32_t cmd,
                                   int32_t *len, uint32_t fifo_start) {
  const VMSVGA3DCommandInfo *info;

  if (cmd < SVGA_3D_CMD_LEGACY_BASE || cmd > SVGA_3D_CMD_FUTURE_MAX) {
    return false;
  };

  info = vmsvga3d_command_info(cmd);
  if (info == NULL) {
    return false;
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

static uint32_t vmsvga3d_devcap[SVGA3D_DEVCAP_MAX] = {
    [SVGA3D_DEVCAP_3D] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_LIGHTS] = 0x00000008,
    [SVGA3D_DEVCAP_MAX_TEXTURES] = 0x00000008,
    [SVGA3D_DEVCAP_MAX_CLIP_PLANES] = 0x00000008,
    [SVGA3D_DEVCAP_VERTEX_SHADER_VERSION] = 0x00000007,
    [SVGA3D_DEVCAP_VERTEX_SHADER] = 0x00000001,
    [SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION] = 0x0000000d,
    [SVGA3D_DEVCAP_FRAGMENT_SHADER] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_RENDER_TARGETS] = 0x00000008,
    [SVGA3D_DEVCAP_S23E8_TEXTURES] = 0x00000001,
    [SVGA3D_DEVCAP_S10E5_TEXTURES] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND] = 0x00000004,
    [SVGA3D_DEVCAP_D16_BUFFER_FORMAT] = 0x00000001,
    [SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT] = 0x00000001,
    [SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT] = 0x00000001,
    [SVGA3D_DEVCAP_QUERY_TYPES] = 0x00000001,
    [SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_POINT_SIZE] = 0x000000bd,
    [SVGA3D_DEVCAP_MAX_SHADER_TEXTURES] = 0x00000014,
    //        [SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH] = 0x00008000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH] = 0x00002000,
    //        [SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT] = 0x00008000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT] = 0x00002000,
    [SVGA3D_DEVCAP_MAX_VOLUME_EXTENT] = 0x00004000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT] = 0x00008000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO] = 0x00008000,
    [SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY] = 0x00000010,
    [SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT] = 0x001fffff,
    [SVGA3D_DEVCAP_MAX_VERTEX_INDEX] = 0x000fffff,
    [SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS] = 0x0000ffff,
    [SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS] = 0x0000ffff,
    [SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS] = 0x00000020,
    [SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS] = 0x00000020,
    [SVGA3D_DEVCAP_TEXTURE_OPS] = 0x03ffffff,
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
    [SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS] = 0x00000008,
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
    [SVGA3D_DEVCAP_SURFACEFMT_Z_DF16] = 0x000040c5,
    [SVGA3D_DEVCAP_SURFACEFMT_Z_DF24] = 0x000040c5,
    [SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT] = 0x000040c5,
    [SVGA3D_DEVCAP_SURFACEFMT_ATI1] = 0x00006005,
    [SVGA3D_DEVCAP_SURFACEFMT_ATI2] = 0x00006005,
    [SVGA3D_DEVCAP_DEAD1] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD8] = 0x00000000,
    [SVGA3D_DEVCAP_DEAD9] = 0x00000000,
    [SVGA3D_DEVCAP_LINE_AA] = 0x00000001,
    [SVGA3D_DEVCAP_LINE_STIPPLE] = 0x00000001,
    [SVGA3D_DEVCAP_MAX_LINE_WIDTH] = 0x0000000a,
    [SVGA3D_DEVCAP_MAX_AA_LINE_WIDTH] = 0x0000000a,
    [SVGA3D_DEVCAP_SURFACEFMT_YV12] = 0x01246000,
    [SVGA3D_DEVCAP_DEAD3] = 0x00000000,
    [SVGA3D_DEVCAP_TS_COLOR_KEY] = 0x00000001,
    [SVGA3D_DEVCAP_DEAD2] = 0x00000000,
    [SVGA3D_DEVCAP_DXCONTEXT] = 0x00000001,
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
  return index >= SVGA3D_DEVCAP_MAX ? 0 : vmsvga3d_devcap[index];
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
