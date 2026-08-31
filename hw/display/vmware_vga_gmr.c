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

#define VMSVGA_GMR_PAGE_SHIFT 12
#define VMSVGA_GMR_PAGE_SIZE (1U << VMSVGA_GMR_PAGE_SHIFT)
#define VMSVGA_GMR_MAX_DESCRIPTOR_LENGTH 4096
#define VMSVGA_GMR_MAX_PAGES (UINT32_MAX / VMSVGA_GMR_PAGE_SIZE)

struct vmsvga_gmr_run_s {
  uint64_t gpa;
  uint32_t num_pages;
};

struct vmsvga_gmr_s {
  struct vmsvga_gmr_run_s *runs;
  uint32_t num_runs;
  uint64_t num_pages;
};

static void vmsvga_gmr_destroy(struct vmsvga_state_s *s, uint32_t gmr_id) {
  struct vmsvga_gmr_s *gmr;

  if (gmr_id >= ARRAY_SIZE(s->gmrs)) {
    return;
  };

  gmr = s->gmrs[gmr_id];
  if (gmr == NULL) {
    return;
  };

  g_free(gmr->runs);
  g_free(gmr);
  s->gmrs[gmr_id] = NULL;
};

static bool vmsvga_gmr_read_descriptor(struct vmsvga_state_s *s,
                                        uint64_t gpa,
                                        SVGAGuestMemDescriptor *desc) {
  struct pci_vmsvga_state_s *pci_vmsvga =
      container_of(s, struct pci_vmsvga_state_s, chip);

  return pci_dma_read(PCI_DEVICE(pci_vmsvga), gpa, desc, sizeof(*desc)) ==
         MEMTX_OK;
};

static bool vmsvga_gmr_parse(struct vmsvga_state_s *s, uint32_t descriptor_ppn,
                             struct vmsvga_gmr_s **out_gmr) {
  struct vmsvga_gmr_run_s *runs = NULL;
  uint32_t num_runs = 0;
  uint32_t run_capacity = 0;
  uint64_t num_pages = 0;
  uint64_t descriptor_page =
      (uint64_t)descriptor_ppn << VMSVGA_GMR_PAGE_SHIFT;
  uint32_t page_offset = 0;
  uint32_t descriptors_read;

  *out_gmr = NULL;

  for (descriptors_read = 0;
       descriptors_read < VMSVGA_GMR_MAX_DESCRIPTOR_LENGTH;
       descriptors_read++) {
    SVGAGuestMemDescriptor raw_desc;
    uint32_t ppn;
    uint32_t pages;

    if (page_offset > VMSVGA_GMR_PAGE_SIZE - sizeof(raw_desc)) {
      goto invalid;
    };

    if (!vmsvga_gmr_read_descriptor(s, descriptor_page + page_offset,
                                    &raw_desc)) {
      goto invalid;
    };

    ppn = le32_to_cpu(raw_desc.ppn);
    pages = le32_to_cpu(raw_desc.numPages);

    if (pages != 0) {
      if (pages > VMSVGA_GMR_MAX_PAGES ||
          num_pages > VMSVGA_GMR_MAX_PAGES - pages) {
        goto invalid;
      };

      if (num_runs == run_capacity) {
        run_capacity = run_capacity == 0 ? 16 : run_capacity * 2;
        runs = g_renew(struct vmsvga_gmr_run_s, runs, run_capacity);
      };

      runs[num_runs].gpa = (uint64_t)ppn << VMSVGA_GMR_PAGE_SHIFT;
      runs[num_runs].num_pages = pages;
      num_runs++;
      num_pages += pages;
      page_offset += sizeof(raw_desc);
      continue;
    };

    if (ppn == 0) {
      struct vmsvga_gmr_s *gmr;

      if (num_runs == 0) {
        goto invalid;
      };

      gmr = g_new0(struct vmsvga_gmr_s, 1);
      gmr->runs = runs;
      gmr->num_runs = num_runs;
      gmr->num_pages = num_pages;
      *out_gmr = gmr;
      return true;
    };

    /* A zero-length descriptor points to another descriptor page. */
    descriptor_page = (uint64_t)ppn << VMSVGA_GMR_PAGE_SHIFT;
    page_offset = 0;
  };

invalid:
  g_free(runs);
  return false;
};

static bool vmsvga_gmr_descriptor_write(struct vmsvga_state_s *s,
                                         uint32_t descriptor_ppn) {
  uint32_t gmr_id = s->gmrid;
  struct vmsvga_gmr_s *new_gmr;

  if (gmr_id >= ARRAY_SIZE(s->gmrs)) {
    return false;
  };

  if (descriptor_ppn == 0) {
    vmsvga_gmr_destroy(s, gmr_id);
    return true;
  };

  if (!vmsvga_gmr_parse(s, descriptor_ppn, &new_gmr)) {
    return false;
  };

  /* Commit only after the complete replacement descriptor list is valid. */
  vmsvga_gmr_destroy(s, gmr_id);
  s->gmrs[gmr_id] = new_gmr;
  return true;
};

bool vmsvga_gmr_validate_range(struct vmsvga_state_s *s, uint32_t gmr_id,
                               uint32_t offset, size_t size) {
  uint64_t total_size;

  if (gmr_id == SVGA_GMR_NULL) {
    return false;
  };

  if (gmr_id == SVGA_GMR_FRAMEBUFFER) {
    total_size = s->vga.vram_size;
  } else {
    struct vmsvga_gmr_s *gmr;

    if (gmr_id >= ARRAY_SIZE(s->gmrs)) {
      return false;
    };

    gmr = s->gmrs[gmr_id];
    if (gmr == NULL) {
      return false;
    };

    total_size = gmr->num_pages * VMSVGA_GMR_PAGE_SIZE;
  };

  return offset <= total_size && size <= total_size - offset;
};

static bool vmsvga_gmr_transfer(struct vmsvga_state_s *s, uint32_t gmr_id,
                                uint32_t offset, const void *src_buffer,
                                void *dst_buffer, size_t size,
                                bool write_to_gmr) {
  struct pci_vmsvga_state_s *pci_vmsvga;
  struct vmsvga_gmr_s *gmr;
  const uint8_t *src = src_buffer;
  uint8_t *dst = dst_buffer;
  uint64_t run_offset = 0;
  uint64_t current_offset = offset;
  uint32_t run_index;

  if (!vmsvga_gmr_validate_range(s, gmr_id, offset, size)) {
    return false;
  };

  if (size == 0) {
    return true;
  };

  if ((write_to_gmr && src == NULL) || (!write_to_gmr && dst == NULL)) {
    return false;
  };

  if (gmr_id == SVGA_GMR_FRAMEBUFFER) {
    if (write_to_gmr) {
      memcpy(s->vga.vram_ptr + offset, src, size);
      vmsvga_mark_vram_dirty_range(s, offset, size);
    } else {
      memcpy(dst, s->vga.vram_ptr + offset, size);
    };
    return true;
  };

  pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
  gmr = s->gmrs[gmr_id];

  for (run_index = 0; run_index < gmr->num_runs; run_index++) {
    uint64_t run_size =
        (uint64_t)gmr->runs[run_index].num_pages * VMSVGA_GMR_PAGE_SIZE;

    if (current_offset >= run_offset + run_size) {
      run_offset += run_size;
      continue;
    };

    while (size != 0 && current_offset < run_offset + run_size) {
      uint64_t within_run = current_offset - run_offset;
      size_t chunk = MIN(size, (size_t)(run_size - within_run));
      MemTxResult result;

      if (write_to_gmr) {
        result = pci_dma_write(PCI_DEVICE(pci_vmsvga),
                               gmr->runs[run_index].gpa + within_run, src,
                               chunk);
      } else {
        result = pci_dma_read(PCI_DEVICE(pci_vmsvga),
                              gmr->runs[run_index].gpa + within_run, dst,
                              chunk);
      };

      if (result != MEMTX_OK) {
        return false;
      };

      if (write_to_gmr) {
        src += chunk;
      } else {
        dst += chunk;
      };
      current_offset += chunk;
      size -= chunk;
    };

    if (size == 0) {
      return true;
    };

    run_offset += run_size;
  };

  return false;
};

bool vmsvga_gmr_read(struct vmsvga_state_s *s, uint32_t gmr_id,
                     uint32_t offset, void *buffer, size_t size) {
  return vmsvga_gmr_transfer(s, gmr_id, offset, NULL, buffer, size, false);
};

bool vmsvga_gmr_write(struct vmsvga_state_s *s, uint32_t gmr_id,
                      uint32_t offset, const void *buffer, size_t size) {
  return vmsvga_gmr_transfer(s, gmr_id, offset, buffer, NULL, size, true);
};

static void vmsvga_gmr_reset(struct vmsvga_state_s *s) {
  uint32_t gmr_id;

  for (gmr_id = 0; gmr_id < ARRAY_SIZE(s->gmrs); gmr_id++) {
    vmsvga_gmr_destroy(s, gmr_id);
  };
};

/*
 * Screen Object v1 belongs with the modern 2D/GMR transport.  Keep it in
 * this module so renderer backends (D3D9, D3D10/11, etc.) remain independent
 * of screen-object state and GMRFB handling.
 */

/*
 * VMware SVGA Screen Object v1 support.
 *
 * Screen Object v1 permits an implementation-specific number of screen IDs.
 * Cubey currently implements one screen (ID 0), backed by the existing BAR1
 * framebuffer.  This keeps the base layer coherent with QEMU's scanout and
 * the legacy SVGA3D PRESENT path without advertising SCREEN_OBJECT_2/GMR2.
 */

#define VMSVGA_SCREEN_V1_ID 0u
#define VMSVGA_SCREEN_V1_STRUCT_SIZE ((uint32_t)offsetof(SVGAScreenObject, backingStore))

#define VMSVGA_ANNOTATION_NONE 0u
#define VMSVGA_ANNOTATION_FILL 1u
#define VMSVGA_ANNOTATION_COPY 2u

static inline void vmsvga_screen_trace_activity(struct vmsvga_state_s *s) {
  vmsvga_trace_flight_activity(s);
}

static void vmsvga_screen_reset(struct vmsvga_state_s *s) {
  s->screen_defined = false;
  s->screen_flags = 0;
  s->screen_width = 0;
  s->screen_height = 0;
  s->screen_root_x = 0;
  s->screen_root_y = 0;
  s->gmrfb_defined = false;
  s->gmrfb_gmr_id = SVGA_GMR_NULL;
  s->gmrfb_offset = 0;
  s->gmrfb_bytes_per_line = 0;
  s->gmrfb_format = 0;
  s->screen_annotation_type = VMSVGA_ANNOTATION_NONE;
  s->screen_annotation_color = 0;
  s->screen_annotation_src_x = 0;
  s->screen_annotation_src_y = 0;
  s->screen_annotation_src_id = SVGA_ID_INVALID;
}

static bool vmsvga_screen_is_primary(const struct vmsvga_state_s *s,
                                     uint32_t screen_id) {
  return s->screen_defined && screen_id == VMSVGA_SCREEN_V1_ID;
}

static bool vmsvga_screen_define(struct vmsvga_state_s *s, uint32_t id,
                                 uint32_t flags, uint32_t width,
                                 uint32_t height, int32_t root_x,
                                 int32_t root_y) {
  uint64_t stride;
  uint64_t size;
  uint32_t supported_flags = SVGA_SCREEN_MUST_BE_SET |
                             SVGA_SCREEN_IS_PRIMARY |
                             SVGA_SCREEN_FULLSCREEN_HINT;

  if (id != VMSVGA_SCREEN_V1_ID || !(flags & SVGA_SCREEN_MUST_BE_SET) ||
      (flags & ~supported_flags) != 0 || width == 0 || height == 0 ||
      width > VMSVGA_MAX_WIDTH || height > VMSVGA_MAX_HEIGHT) {
    return false;
  }

  stride = (uint64_t)width * 4;
  size = stride * height;
  if (stride > UINT32_MAX || size > s->vga.vram_size) {
    return false;
  }

  s->screen_defined = true;
  s->screen_flags = flags;
  s->screen_width = width;
  s->screen_height = height;
  s->screen_root_x = root_x;
  s->screen_root_y = root_y;

  /* Screen Object v1's host-managed base layer is Cubey's BAR1 scanout. */
  s->active_valid = true;
  s->active_width = width;
  s->active_height = height;
  s->active_depth = 32;
  s->active_stride = (uint32_t)stride;
  s->new_width = width;
  s->new_height = height;
  s->new_depth = 32;
  s->svga_surface_bound = false;
  s->invalidated = true;

  s->trace_now.screen_defines++;
  vmsvga_screen_trace_activity(s);
  VMVGA_TRACE_LOCAL(VMVGA_TRACE_STATE,
                     "SCREEN_DEFINE id=%u flags=0x%08x width=%u height=%u "
                     "root=%d,%d stride=%u",
                     id, flags, width, height, root_x, root_y,
                     s->active_stride);
  return true;
}

static bool vmsvga_screen_destroy(struct vmsvga_state_s *s,
                                  uint32_t screen_id) {
  if (screen_id != VMSVGA_SCREEN_V1_ID) {
    return false;
  }
  if (!s->screen_defined) {
    return true;
  }
  s->screen_defined = false;
  s->screen_flags = 0;
  s->screen_width = 0;
  s->screen_height = 0;
  s->screen_root_x = 0;
  s->screen_root_y = 0;
  s->screen_annotation_type = VMSVGA_ANNOTATION_NONE;
  s->trace_now.screen_destroys++;
  vmsvga_screen_trace_activity(s);
  VMVGA_TRACE_LOCAL(VMVGA_TRACE_STATE, "SCREEN_DESTROY id=%u", screen_id);
  return true;
}

static bool vmsvga_screen_format_decode(uint32_t value, uint32_t *bpp,
                                        uint32_t *depth,
                                        uint32_t *bytes_per_pixel) {
  uint32_t bits = value & 0xff;
  uint32_t color_depth = (value >> 8) & 0xff;
  uint32_t reserved = value >> 16;

  if (reserved != 0) {
    return false;
  }
  if ((bits == 32 && color_depth == 24) ||
      (bits == 24 && color_depth == 24) ||
      (bits == 16 && color_depth == 16) ||
      (bits == 16 && color_depth == 15)) {
    *bpp = bits;
    *depth = color_depth;
    *bytes_per_pixel = (bits + 7) / 8;
    return true;
  }
  return false;
}

static bool vmsvga_screen_define_gmrfb(struct vmsvga_state_s *s,
                                       uint32_t gmr_id, uint32_t offset,
                                       uint32_t bytes_per_line,
                                       uint32_t format) {
  uint32_t bpp, depth, bypp;

  if (!vmsvga_screen_format_decode(format, &bpp, &depth, &bypp) ||
      bytes_per_line == 0 || gmr_id == SVGA_GMR_NULL) {
    return false;
  }
  (void)bpp;
  (void)depth;
  (void)bypp;
  /* Validate the starting byte now; each blit validates its full row range. */
  if (!vmsvga_gmr_validate_range(s, gmr_id, offset, 0)) {
    return false;
  }

  s->gmrfb_defined = true;
  s->gmrfb_gmr_id = gmr_id;
  s->gmrfb_offset = offset;
  s->gmrfb_bytes_per_line = bytes_per_line;
  s->gmrfb_format = format;
  s->trace_now.gmrfb_defines++;
  vmsvga_screen_trace_activity(s);
  VMVGA_TRACE_LOCAL(VMVGA_TRACE_STATE,
                     "GMRFB_DEFINE gmr=%u offset=0x%08x pitch=%u "
                     "format=0x%08x",
                     gmr_id, offset, bytes_per_line, format);
  return true;
}

static inline uint8_t vmsvga_screen_expand5(uint32_t value) {
  return (uint8_t)((value << 3) | (value >> 2));
}

static inline uint8_t vmsvga_screen_expand6(uint32_t value) {
  return (uint8_t)((value << 2) | (value >> 4));
}

static void vmsvga_screen_gmrfb_to_bgrx(uint8_t *dst, const uint8_t *src,
                                        uint32_t pixels, uint32_t bpp,
                                        uint32_t depth) {
  uint32_t x;
  for (x = 0; x < pixels; x++, dst += 4) {
    if (bpp == 32) {
      dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 0;
      src += 4;
    } else if (bpp == 24) {
      dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 0;
      src += 3;
    } else {
      uint16_t pixel = lduw_le_p(src);
      if (depth == 16) {
        dst[0] = vmsvga_screen_expand5(pixel & 0x1f);
        dst[1] = vmsvga_screen_expand6((pixel >> 5) & 0x3f);
        dst[2] = vmsvga_screen_expand5((pixel >> 11) & 0x1f);
      } else {
        dst[0] = vmsvga_screen_expand5(pixel & 0x1f);
        dst[1] = vmsvga_screen_expand5((pixel >> 5) & 0x1f);
        dst[2] = vmsvga_screen_expand5((pixel >> 10) & 0x1f);
      }
      dst[3] = 0;
      src += 2;
    }
  }
}

static inline uint16_t vmsvga_screen_pack565(uint8_t b, uint8_t g,
                                              uint8_t r) {
  return (uint16_t)(((uint16_t)(r >> 3) << 11) |
                    ((uint16_t)(g >> 2) << 5) | (b >> 3));
}

static inline uint16_t vmsvga_screen_pack555(uint8_t b, uint8_t g,
                                              uint8_t r) {
  return (uint16_t)(((uint16_t)(r >> 3) << 10) |
                    ((uint16_t)(g >> 3) << 5) | (b >> 3));
}

static void vmsvga_screen_bgrx_to_gmrfb(uint8_t *dst, const uint8_t *src,
                                        uint32_t pixels, uint32_t bpp,
                                        uint32_t depth) {
  uint32_t x;
  for (x = 0; x < pixels; x++, src += 4) {
    if (bpp == 32) {
      dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 0;
      dst += 4;
    } else if (bpp == 24) {
      dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
      dst += 3;
    } else {
      uint16_t pixel = depth == 16
                           ? vmsvga_screen_pack565(src[0], src[1], src[2])
                           : vmsvga_screen_pack555(src[0], src[1], src[2]);
      stw_le_p(dst, pixel);
      dst += 2;
    }
  }
}

static bool vmsvga_screen_gmrfb_row_offset(struct vmsvga_state_s *s,
                                           int32_t x, int32_t y,
                                           uint32_t width, uint32_t bypp,
                                           uint32_t *offset,
                                           size_t *row_bytes) {
  uint64_t relative;
  uint64_t absolute;
  uint64_t bytes;

  if (!s->gmrfb_defined || x < 0 || y < 0) {
    return false;
  }
  bytes = (uint64_t)width * bypp;
  if ((uint64_t)(uint32_t)x * bypp > s->gmrfb_bytes_per_line ||
      bytes > s->gmrfb_bytes_per_line -
                  (uint64_t)(uint32_t)x * bypp) {
    return false;
  }
  relative = (uint64_t)(uint32_t)y * s->gmrfb_bytes_per_line +
             (uint64_t)(uint32_t)x * bypp;
  absolute = (uint64_t)s->gmrfb_offset + relative;
  if (bytes > SIZE_MAX || absolute > UINT32_MAX ||
      bytes > UINT32_MAX - absolute ||
      !vmsvga_gmr_validate_range(s, s->gmrfb_gmr_id, (uint32_t)absolute,
                                 (size_t)bytes)) {
    return false;
  }
  *offset = (uint32_t)absolute;
  *row_bytes = (size_t)bytes;
  return true;
}

static bool vmsvga_screen_blit_one_from_gmrfb(
    struct vmsvga_state_s *s, int32_t src_x, int32_t src_y,
    int32_t dst_left, int32_t dst_top, int32_t dst_right,
    int32_t dst_bottom) {
  uint32_t bpp, depth, bypp;
  int64_t left = dst_left;
  int64_t top = dst_top;
  int64_t right = dst_right;
  int64_t bottom = dst_bottom;
  int64_t source_x = src_x;
  int64_t source_y = src_y;
  uint32_t width, height, row;
  uint8_t *vram = vmsvga_svga_vram_ptr(s);

  if (!s->screen_defined || !s->gmrfb_defined ||
      !vmsvga_screen_format_decode(s->gmrfb_format, &bpp, &depth, &bypp) ||
      right <= left || bottom <= top) {
    return false;
  }

  if (left < 0) { source_x -= left; left = 0; }
  if (top < 0) { source_y -= top; top = 0; }
  right = MIN(right, (int64_t)s->screen_width);
  bottom = MIN(bottom, (int64_t)s->screen_height);
  if (right <= left || bottom <= top) {
    return true;
  }
  if (source_x < 0 || source_y < 0) {
    return false;
  }

  width = (uint32_t)(right - left);
  height = (uint32_t)(bottom - top);
  for (row = 0; row < height; row++) {
    uint32_t gmr_offset;
    size_t row_bytes;
    uint8_t *dst = vram + (uint64_t)(top + row) * s->active_stride +
                   (uint64_t)left * 4;
    if (!vmsvga_screen_gmrfb_row_offset(s, (int32_t)source_x,
                                        (int32_t)(source_y + row), width,
                                        bypp, &gmr_offset, &row_bytes) ||
        row_bytes > sizeof(s->blit_scratch) ||
        !vmsvga_gmr_read(s, s->gmrfb_gmr_id, gmr_offset,
                         s->blit_scratch, row_bytes)) {
      return false;
    }
    vmsvga_screen_gmrfb_to_bgrx(dst, s->blit_scratch, width, bpp, depth);
  }

  vmsvga_mark_vram_dirty_rect(s, 0, s->active_stride, 4,
                              (uint32_t)left, (uint32_t)top, width, height);
  vmsvga_damage_add_visible(s, (uint32_t)left, (uint32_t)top, width, height);
  return true;
}

static bool vmsvga_screen_blit_gmrfb_to_screen(
    struct vmsvga_state_s *s, const SVGASignedPoint *src_origin,
    const SVGASignedRect *dest_rect, uint32_t dest_screen_id) {
  int64_t width;
  int64_t height;
  int32_t local_left;
  int32_t local_top;
  int32_t local_right;
  int32_t local_bottom;
  bool ok;

  if (!s->screen_defined || src_origin == NULL || dest_rect == NULL) {
    return false;
  }
  width = (int64_t)dest_rect->right - dest_rect->left;
  height = (int64_t)dest_rect->bottom - dest_rect->top;
  if (width <= 0 || height <= 0 || width > INT32_MAX || height > INT32_MAX) {
    return false;
  }

  if (dest_screen_id == VMSVGA_SCREEN_V1_ID) {
    local_left = dest_rect->left;
    local_top = dest_rect->top;
    local_right = dest_rect->right;
    local_bottom = dest_rect->bottom;
  } else if (dest_screen_id == SVGA_ID_INVALID) {
    int64_t left = (int64_t)dest_rect->left - s->screen_root_x;
    int64_t top = (int64_t)dest_rect->top - s->screen_root_y;
    int64_t right = (int64_t)dest_rect->right - s->screen_root_x;
    int64_t bottom = (int64_t)dest_rect->bottom - s->screen_root_y;
    if (left < INT32_MIN || left > INT32_MAX || top < INT32_MIN ||
        top > INT32_MAX || right < INT32_MIN || right > INT32_MAX ||
        bottom < INT32_MIN || bottom > INT32_MAX) {
      return false;
    }
    local_left = (int32_t)left;
    local_top = (int32_t)top;
    local_right = (int32_t)right;
    local_bottom = (int32_t)bottom;
  } else {
    return false;
  }

  ok = vmsvga_screen_blit_one_from_gmrfb(
      s, src_origin->x, src_origin->y, local_left, local_top,
      local_right, local_bottom);
  s->screen_annotation_type = VMSVGA_ANNOTATION_NONE;
  s->trace_now.gmrfb_to_screen++;
  vmsvga_screen_trace_activity(s);
  return ok;
}

static bool vmsvga_screen_blit_screen_to_gmrfb(
    struct vmsvga_state_s *s, const SVGASignedPoint *dest_origin,
    const SVGASignedRect *src_rect, uint32_t src_screen_id) {
  uint32_t bpp, depth, bypp;
  int64_t width64, height64;
  uint32_t width, height, row;
  uint8_t *vram = vmsvga_svga_vram_ptr(s);

  if (!s->screen_defined || !s->gmrfb_defined || dest_origin == NULL ||
      src_rect == NULL || src_screen_id != VMSVGA_SCREEN_V1_ID ||
      !vmsvga_screen_format_decode(s->gmrfb_format, &bpp, &depth, &bypp)) {
    return false;
  }
  width64 = (int64_t)src_rect->right - src_rect->left;
  height64 = (int64_t)src_rect->bottom - src_rect->top;
  if (src_rect->left < 0 || src_rect->top < 0 || dest_origin->x < 0 ||
      dest_origin->y < 0 || width64 <= 0 || height64 <= 0 ||
      src_rect->right > (int32_t)s->screen_width ||
      src_rect->bottom > (int32_t)s->screen_height ||
      width64 > UINT32_MAX || height64 > UINT32_MAX) {
    return false;
  }
  width = (uint32_t)width64;
  height = (uint32_t)height64;

  for (row = 0; row < height; row++) {
    uint32_t gmr_offset;
    size_t row_bytes;
    const uint8_t *src = vram +
        (uint64_t)(src_rect->top + (int32_t)row) * s->active_stride +
        (uint64_t)src_rect->left * 4;
    if (!vmsvga_screen_gmrfb_row_offset(s, dest_origin->x,
                                        dest_origin->y + (int32_t)row,
                                        width, bypp, &gmr_offset,
                                        &row_bytes) ||
        row_bytes > sizeof(s->blit_scratch)) {
      return false;
    }
    vmsvga_screen_bgrx_to_gmrfb(s->blit_scratch, src, width, bpp, depth);
    if (!vmsvga_gmr_write(s, s->gmrfb_gmr_id, gmr_offset,
                          s->blit_scratch, row_bytes)) {
      return false;
    }
  }

  s->trace_now.screen_to_gmrfb++;
  vmsvga_screen_trace_activity(s);
  return true;
}

static void vmsvga_screen_annotation_fill(struct vmsvga_state_s *s,
                                          uint32_t color) {
  s->screen_annotation_type = VMSVGA_ANNOTATION_FILL;
  s->screen_annotation_color = color;
  s->trace_now.annotation_fills++;
  vmsvga_screen_trace_activity(s);
}

static void vmsvga_screen_annotation_copy(struct vmsvga_state_s *s,
                                          int32_t src_x, int32_t src_y,
                                          uint32_t src_screen_id) {
  s->screen_annotation_type = VMSVGA_ANNOTATION_COPY;
  s->screen_annotation_src_x = src_x;
  s->screen_annotation_src_y = src_y;
  s->screen_annotation_src_id = src_screen_id;
  s->trace_now.annotation_copies++;
  vmsvga_screen_trace_activity(s);
}

static inline void vmsvga_screen_record_surface_to_screen(
    struct vmsvga_state_s *s) {
  s->trace_now.surface_to_screen++;
  vmsvga_screen_trace_activity(s);
}
