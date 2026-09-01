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
  const uint32_t max_pages =
      MIN(VMSVGA_GMR_MAX_PAGES, UINT32_MAX / VMSVGA_GMR_PAGE_SIZE);
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
      /*
       * Match VirtualBox's GMR1 accounting quirk: the register advertises
       * VMSVGA_GMR_MAX_PAGES, but its byte total is 32-bit, so one GMR is
       * effectively limited to UINT32_MAX / 4K pages.
       */
      if (pages > max_pages || num_pages > max_pages - pages) {
        goto invalid;
      };

      {
        uint64_t gpa = (uint64_t)ppn << VMSVGA_GMR_PAGE_SHIFT;
        struct vmsvga_gmr_run_s *previous =
            num_runs != 0 ? &runs[num_runs - 1] : NULL;

        if (previous != NULL &&
            previous->gpa +
                    (uint64_t)previous->num_pages * VMSVGA_GMR_PAGE_SIZE ==
                gpa) {
          /* Adjacent descriptors describe one continuous guest range. */
          previous->num_pages += pages;
        } else {
          if (num_runs == run_capacity) {
            run_capacity = run_capacity == 0 ? 16 : run_capacity * 2;
            runs = g_renew(struct vmsvga_gmr_run_s, runs, run_capacity);
          };

          runs[num_runs].gpa = gpa;
          runs[num_runs].num_pages = pages;
          num_runs++;
        };
      };
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
    if (vmsvga_trace_flight_enabled()) {
      fprintf(stderr, "VMVGA-GMR-DIAG unmap id=%u\n", gmr_id);
    };
    vmsvga_gmr_destroy(s, gmr_id);
    return true;
  };

  if (!vmsvga_gmr_parse(s, descriptor_ppn, &new_gmr)) {
    return false;
  };

  /* Commit only after the complete replacement descriptor list is valid. */
  vmsvga_gmr_destroy(s, gmr_id);
  s->gmrs[gmr_id] = new_gmr;
  if (vmsvga_trace_flight_enabled()) {
    uint64_t first_gpa = new_gmr->num_runs != 0 ? new_gmr->runs[0].gpa : 0;
    uint32_t first_pages =
        new_gmr->num_runs != 0 ? new_gmr->runs[0].num_pages : 0;

    fprintf(stderr,
            "VMVGA-GMR-DIAG map id=%u descriptor_ppn=0x%08x runs=%u "
            "pages=%" PRIu64 " first_gpa=0x%" PRIx64 " first_pages=%u\n",
            gmr_id, descriptor_ppn, new_gmr->num_runs, new_gmr->num_pages,
            first_gpa, first_pages);
  };
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
    uint8_t *vram = vmsvga_svga_vram_ptr(s);

    if (write_to_gmr) {
      memcpy(vram + offset, src, size);
      vmsvga_mark_vram_dirty_range(s, offset, size);
    } else {
      memcpy(dst, vram + offset, size);
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
 * VMware SVGA Screen Object support.
 *
 * Cubey currently implements one screen (ID 0). With the original
 * SVGA_FIFO_CAP_SCREEN_OBJECT capability the backingStore fields are optional.
 * Parse and validate them, but keep the Screen base layer host-owned so guest
 * memory is accessed only by explicit FIFO DMA commands.
 */

#define VMSVGA_SCREEN_V1_ID 0u
#define VMSVGA_SCREEN_V1_STRUCT_SIZE \
  ((uint32_t)offsetof(SVGAScreenObject, backingStore))
#define VMSVGA_SCREEN_BACKING_STRUCT_SIZE \
  ((uint32_t)offsetof(SVGAScreenObject, cloneCount))
#define VMSVGA_SCREEN_FULL_STRUCT_SIZE ((uint32_t)sizeof(SVGAScreenObject))

#define VMSVGA_ANNOTATION_NONE 0u
#define VMSVGA_ANNOTATION_FILL 1u
#define VMSVGA_ANNOTATION_COPY 2u

#define VMSVGA_SCREEN_REJECT(fmt, ...)                                    \
  do {                                                                     \
    if (vmsvga_trace_flight_enabled()) {                                   \
      fprintf(stderr, "VMVGA-SCREEN-REJECT " fmt "\n", ##__VA_ARGS__); \
    }                                                                      \
  } while (0)

static uint32_t vmsvga_gmr_diag_hash(const uint8_t *data, size_t size) {
  uint32_t hash = 2166136261u;
  size_t i;

  for (i = 0; i < size; i++) {
    hash ^= data[i];
    hash *= 16777619u;
  };
  return hash;
};

static uint32_t vmsvga_gmr_diag_hash_extend(uint32_t hash,
                                             const uint8_t *data,
                                             size_t size) {
  size_t i;

  for (i = 0; i < size; i++) {
    hash ^= data[i];
    hash *= 16777619u;
  };
  return hash;
};

static bool vmsvga_gmr_diag_hash_rows(const uint8_t *data, uint32_t stride,
                                       size_t row_bytes, uint32_t height,
                                       uint32_t *hash_out) {
  uint32_t hash = 2166136261u;
  uint32_t row;

  if (data == NULL || hash_out == NULL || row_bytes == 0 || height == 0 ||
      stride < row_bytes) {
    return false;
  };
  for (row = 0; row < height; row++) {
    hash = vmsvga_gmr_diag_hash_extend(hash, data + (uint64_t)row * stride,
                                       row_bytes);
  };
  *hash_out = hash;
  return true;
};

static void vmsvga_screen_trace_present_snapshot(struct vmsvga_state_s *s,
                                                  uint64_t seq,
                                                  const char *reason) {
  DisplaySurface *surface;
  uint8_t *vram;
  uint8_t *front;
  uint32_t front_hash;
  uint32_t backing_hash;
  uint32_t mirror_hash;
  int64_t front_bar1_offset;
  uint32_t front_stride;
  uint32_t front_width;
  uint32_t front_height;
  uint32_t front_bypp;
  bool front_hash_valid;
  bool backing_hash_valid;
  bool mirror_hash_valid;

  if (!vmsvga_trace_flight_enabled()) {
    return;
  };
  if (seq > 16 && (seq & 63) != 0) {
    return;
  };

  front = NULL;
  front_hash = 0;
  backing_hash = 0;
  mirror_hash = 0;
  front_bar1_offset = -1;
  front_stride = 0;
  front_width = 0;
  front_height = 0;
  front_bypp = 0;
  front_hash_valid = false;
  backing_hash_valid = false;
  mirror_hash_valid = false;
  vram = vmsvga_svga_vram_ptr(s);
  surface = qemu_console_surface(s->vga.con);
  if (surface != NULL && surface_data(surface) != NULL &&
      surface_width(surface) > 0 && surface_height(surface) > 0 &&
      surface_stride(surface) > 0 && surface_bits_per_pixel(surface) > 0) {
    uintptr_t front_addr;
    uintptr_t vram_addr;
    uint64_t front_bytes;

    front = surface_data(surface);
    front_width = surface_width(surface);
    front_height = surface_height(surface);
    front_stride = surface_stride(surface);
    front_bypp = (surface_bits_per_pixel(surface) + 7) / 8;
    front_bytes = (uint64_t)front_stride * front_height;
    front_addr = (uintptr_t)front;
    vram_addr = (uintptr_t)vram;
    if (front_addr >= vram_addr && front_addr - vram_addr < s->vga.vram_size) {
      uint64_t offset = front_addr - vram_addr;
      front_bar1_offset = (int64_t)offset;
      if (front_bytes > s->vga.vram_size - offset) {
        front_bytes = 0;
      };
    };
    if (front_bytes != 0 && front_bypp != 0 &&
        (uint64_t)front_width * front_bypp <= front_stride) {
      front_hash_valid = vmsvga_gmr_diag_hash_rows(
          front, front_stride, (size_t)front_width * front_bypp, front_height,
          &front_hash);
    };
  };

  if (s->screen_defined && s->screen_backing_valid &&
      s->screen_backing_gmr_id == SVGA_GMR_FRAMEBUFFER &&
      s->screen_width != 0 && s->screen_height != 0 &&
      s->screen_backing_pitch >= (uint64_t)s->screen_width * 4 &&
      s->screen_backing_offset < s->vga.vram_size &&
      (uint64_t)s->screen_backing_pitch * s->screen_height <=
          s->vga.vram_size - s->screen_backing_offset) {
    backing_hash_valid = vmsvga_gmr_diag_hash_rows(
        vram + s->screen_backing_offset, s->screen_backing_pitch,
        (size_t)s->screen_width * 4, s->screen_height, &backing_hash);
  };

  if (s->screen_base != NULL && s->screen_width != 0 &&
      s->screen_height != 0 &&
      s->screen_stride >= (uint64_t)s->screen_width * 4 &&
      (uint64_t)s->screen_stride * s->screen_height <= s->screen_base_size) {
    mirror_hash_valid = vmsvga_gmr_diag_hash_rows(
        s->screen_base, s->screen_stride, (size_t)s->screen_width * 4,
        s->screen_height, &mirror_hash);
  };

  fprintf(stderr,
          "VMVGA-PRESENT seq=%" PRIu64 " reason=%s frontend=%p "
          "frontend-bar1-off=%" PRId64 " frontend-size=%ux%u/%u "
          "frontend-hash=0x%08x frontend-hash-valid=%u "
          "backing=%u:0x%08x/%u backing-hash=0x%08x "
          "backing-hash-valid=%u mirror=%p mirror-hash=0x%08x "
          "mirror-hash-valid=%u gmrfb=%u:0x%08x/%u "
          "handoff=%u bound=%u\n",
          seq, reason, (void *)front, front_bar1_offset, front_width,
          front_height, front_stride, front_hash, front_hash_valid,
          s->screen_backing_valid ? s->screen_backing_gmr_id : SVGA_GMR_NULL,
          s->screen_backing_valid ? s->screen_backing_offset : 0,
          s->screen_backing_valid ? s->screen_backing_pitch : 0,
          backing_hash, backing_hash_valid, (void *)s->screen_base, mirror_hash,
          mirror_hash_valid, s->gmrfb_defined ? s->gmrfb_gmr_id : SVGA_GMR_NULL,
          s->gmrfb_defined ? s->gmrfb_offset : 0,
          s->gmrfb_defined ? s->gmrfb_bytes_per_line : 0,
          s->screen_handoff_active, s->svga_surface_bound);
};

static void vmsvga_screen_base_clear(struct vmsvga_state_s *s) {
  g_clear_pointer(&s->screen_base, g_free);
  s->screen_base_size = 0;
  s->screen_stride = 0;
}

static bool vmsvga_screen_base_resize(struct vmsvga_state_s *s,
                                      uint32_t width, uint32_t height,
                                      uint32_t stride) {
  uint64_t row_bytes = (uint64_t)width * 4;
  uint64_t size64 = (uint64_t)stride * height;
  size_t size;
  uint8_t *new_base;

  if (row_bytes > UINT32_MAX || stride < row_bytes ||
      size64 == 0 || size64 > SIZE_MAX) {
    return false;
  }
  size = (size_t)size64;
  if (s->screen_base != NULL && s->screen_defined &&
      s->screen_width == width && s->screen_height == height &&
      s->screen_stride == stride && s->screen_base_size == size) {
    return true;
  }

  new_base = g_try_malloc0(size);
  if (new_base == NULL) {
    return false;
  }
  if (s->screen_base != NULL && s->screen_defined &&
      s->screen_stride != 0) {
    uint32_t copy_width = MIN(s->screen_width, width);
    uint32_t copy_height = MIN(s->screen_height, height);
    size_t copy_bytes = (size_t)copy_width * 4;
    uint32_t row;

    for (row = 0; row < copy_height; row++) {
      memcpy(new_base + (size_t)row * stride,
             s->screen_base + (size_t)row * s->screen_stride, copy_bytes);
    }
  }

  g_free(s->screen_base);
  s->screen_base = new_base;
  s->screen_base_size = size;
  s->screen_stride = stride;
  return true;
}

static bool vmsvga_screen_backing_validate(struct vmsvga_state_s *s,
                                           uint32_t width, uint32_t height,
                                           uint32_t gmr_id, uint32_t offset,
                                           uint32_t pitch) {
  uint64_t row_bytes = (uint64_t)width * 4;
  uint64_t size = (uint64_t)pitch * height;

  if (gmr_id != SVGA_GMR_FRAMEBUFFER || width == 0 || height == 0 ||
      row_bytes > UINT32_MAX || pitch < row_bytes || size == 0 ||
      size > SIZE_MAX ||
      !vmsvga_gmr_validate_range(s, gmr_id, offset, (size_t)size)) {
    return false;
  }
  return true;
}

static bool vmsvga_screen_base_layer_storage(struct vmsvga_state_s *s,
                                             uint8_t **base, size_t *size,
                                             uint32_t *stride) {
  uint64_t required;

  if (!s->screen_defined || base == NULL || size == NULL || stride == NULL) {
    return false;
  }

  if (s->screen_backing_valid) {
    if (!vmsvga_screen_backing_validate(
            s, s->screen_width, s->screen_height,
            s->screen_backing_gmr_id, s->screen_backing_offset,
            s->screen_backing_pitch) ||
        s->screen_backing_gmr_id != SVGA_GMR_FRAMEBUFFER) {
      return false;
    }
    *base = vmsvga_svga_vram_ptr(s) + s->screen_backing_offset;
    *size = s->vga.vram_size - s->screen_backing_offset;
    *stride = s->screen_backing_pitch;
    return true;
  }

  required = (uint64_t)s->screen_stride * s->screen_height;
  if (s->screen_base == NULL ||
      s->screen_stride != (uint64_t)s->screen_width * 4 ||
      required == 0 || required > s->screen_base_size) {
    return false;
  }
  *base = s->screen_base;
  *size = s->screen_base_size;
  *stride = s->screen_stride;
  return true;
}

static bool vmsvga_screen_storage(struct vmsvga_state_s *s,
                                  uint8_t **base, size_t *size,
                                  uint32_t *stride) {
  /*
   * The Screen Object base layer lives in its advertised backingStore.  The
   * host mirror is only a temporary scanout shield while the legacy firmware
   * layout is being replaced by the first guest Screen presentation.
   */
  if (s->screen_defined && s->screen_backing_valid &&
      s->screen_handoff_active) {
    if (!vmsvga_screen_base_resize(s, s->screen_width, s->screen_height,
                                   s->screen_backing_pitch)) {
      return false;
    }
    *base = s->screen_base;
    *size = s->screen_base_size;
    *stride = s->screen_stride;
    return true;
  }
  return vmsvga_screen_base_layer_storage(s, base, size, stride);
}

static inline void vmsvga_screen_mark_dirty(struct vmsvga_state_s *s,
                                            uint32_t x, uint32_t y,
                                            uint32_t width,
                                            uint32_t height) {
  if (s->screen_backing_valid &&
      s->screen_backing_gmr_id == SVGA_GMR_FRAMEBUFFER) {
    vmsvga_mark_vram_dirty_rect(s, s->screen_backing_offset,
                                s->screen_backing_pitch, 4,
                                x, y, width, height);
  }
}

static void vmsvga_screen_reset(struct vmsvga_state_s *s) {
  vmsvga_screen_base_clear(s);
  s->screen_defined = false;
  s->screen_flags = 0;
  s->screen_width = 0;
  s->screen_height = 0;
  s->screen_root_x = 0;
  s->screen_root_y = 0;
  s->screen_backing_valid = false;
  s->screen_handoff_active = false;
  s->screen_backing_gmr_id = SVGA_GMR_NULL;
  s->screen_backing_offset = 0;
  s->screen_backing_pitch = 0;
  s->screen_clone_count = 0;
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
                                 int32_t root_y, bool backing_present,
                                 uint32_t backing_gmr_id,
                                 uint32_t backing_offset,
                                 uint32_t backing_pitch,
                                 uint32_t clone_count) {
  uint64_t stride;
  uint64_t size;
  uint32_t screen_stride;
  bool handoff_active;
  DisplaySurface *surface;
  uint32_t supported_flags = SVGA_SCREEN_MUST_BE_SET |
                             SVGA_SCREEN_IS_PRIMARY |
                             SVGA_SCREEN_FULLSCREEN_HINT;

  if (id != VMSVGA_SCREEN_V1_ID || !(flags & SVGA_SCREEN_MUST_BE_SET) ||
      (flags & ~supported_flags) != 0 || width == 0 || height == 0 ||
      width > VMSVGA_MAX_WIDTH || height > VMSVGA_MAX_HEIGHT) {
    VMSVGA_SCREEN_REJECT(
        "define reason=parameters id=%u flags=0x%08x size=%ux%u root=%d,%d",
        id, flags, width, height, root_x, root_y);
    return false;
  }

  stride = (uint64_t)width * 4;
  size = stride * height;
  if (stride > UINT32_MAX || size == 0 || size > SIZE_MAX) {
    VMSVGA_SCREEN_REJECT(
        "define reason=base-size id=%u size=%ux%u stride=%" PRIu64
        " bytes=%" PRIu64,
        id, width, height, stride, size);
    return false;
  }
  /*
   * VirtualBox treats a zero Screen backing pitch as a tightly packed 32-bpp
   * scanline even though the backing-store form normally carries an explicit
   * pitch. Preserve that device quirk before validating the BAR1 range.
   */
  if (backing_present && backing_pitch == 0) {
    backing_pitch = (uint32_t)stride;
  }
  if (backing_present &&
      !vmsvga_screen_backing_validate(s, width, height, backing_gmr_id,
                                      backing_offset, backing_pitch)) {
    VMSVGA_SCREEN_REJECT(
        "define reason=backing id=%u gmr=%u offset=0x%08x pitch=%u "
        "size=%ux%u",
        id, backing_gmr_id, backing_offset, backing_pitch, width, height);
    return false;
  }

  screen_stride = backing_present ? backing_pitch : (uint32_t)stride;
  surface = qemu_console_surface(s->vga.con);
  handoff_active =
      backing_present &&
      (surface == NULL || surface_width(surface) != width ||
       surface_height(surface) != height ||
       surface_bits_per_pixel(surface) != 32 ||
       surface_stride(surface) != screen_stride ||
       surface_data(surface) !=
           vmsvga_svga_vram_ptr(s) + (size_t)backing_offset);

  if ((!backing_present || handoff_active) &&
      !vmsvga_screen_base_resize(s, width, height, screen_stride)) {
    VMSVGA_SCREEN_REJECT(
        "define reason=base-allocation id=%u size=%ux%u stride=%u",
        id, width, height, screen_stride);
    return false;
  }
  if (handoff_active && s->screen_base != NULL) {
    memset(s->screen_base, 0, s->screen_base_size);
  }

  s->screen_backing_valid = backing_present;
  /*
   * Protect a genuine scanout transition. Same-mode Screen redefines can bind
   * immediately only when the frontend already points at the same backingStore;
   * a new BAR1 offset still needs the first present to populate the handoff.
   */
  s->screen_handoff_active = handoff_active;
  if (handoff_active) {
    /*
     * Damage queued for the old scanout must not make update_display bind the
     * freshly allocated transition mirror before the first Screen present has
     * populated it.
     */
    s->damage_count = 0;
  }
  s->screen_backing_gmr_id = backing_present ? backing_gmr_id : SVGA_GMR_NULL;
  s->screen_backing_offset = backing_present ? backing_offset : 0;
  s->screen_backing_pitch = backing_present ? backing_pitch : 0;

  s->screen_defined = true;
  s->screen_flags = flags;
  s->screen_width = width;
  s->screen_height = height;
  s->screen_root_x = root_x;
  s->screen_root_y = root_y;
  s->screen_clone_count = clone_count;
  s->screen_stride = screen_stride;

  /* The active tuple also describes the Screen Object scanout surface. */
  s->active_valid = true;
  s->active_width = width;
  s->active_height = height;
  s->active_depth = 32;
  s->active_stride = s->screen_stride;
  s->new_width = width;
  s->new_height = height;
  s->new_depth = 32;
  s->svga_surface_bound = false;
  s->invalidated = true;

  if (vmsvga_trace_flight_enabled()) {
    fprintf(stderr,
            "VMVGA-SCREEN-HANDOFF phase=define deferred=%u old=%dx%d/%d/%d "
            "new=%ux%u/32/%u mirror=%p backing=%u:0x%08x\n",
            s->screen_handoff_active,
            surface != NULL ? surface_width(surface) : 0,
            surface != NULL ? surface_height(surface) : 0,
            surface != NULL ? surface_bits_per_pixel(surface) : 0,
            surface != NULL ? surface_stride(surface) : 0,
            width, height, screen_stride, (void *)s->screen_base,
            s->screen_backing_valid ? s->screen_backing_gmr_id :
                                      SVGA_GMR_NULL,
            s->screen_backing_valid ? s->screen_backing_offset : 0);
  }

  /*
   * During a layout transition keep the old frontend surface until FIFO
   * processing has populated the new Screen base layer. vmsvga_update_display
   * will bind the mirror if only partial presents arrive. A complete Screen
   * present ends the handoff and binds BAR1 directly.
   */
  if (!s->screen_handoff_active) {
    vmsvga_check_size(s);
  }

  if (vmsvga_trace_flight_enabled()) {
    fprintf(stderr,
            "VMVGA-SCREEN-BACKING id=%u present=%u gmr=%u offset=0x%08x "
            "pitch=%u clone=%u size=%ux%u handoff=%u\n",
            id, s->screen_backing_valid,
            s->screen_backing_valid ? s->screen_backing_gmr_id :
                                      SVGA_GMR_NULL,
            s->screen_backing_valid ? s->screen_backing_offset : 0,
            s->screen_backing_valid ? s->screen_backing_pitch : 0,
            clone_count, width, height, s->screen_handoff_active);
  }
  if (vmsvga_trace_flight_enabled()) {
    s->trace_now.screen_defines++;
    s->trace_activity_seq++;
  };
  VMVGA_TRACE_LOCAL(VMVGA_TRACE_STATE,
                     "SCREEN_DEFINE id=%u flags=0x%08x width=%u height=%u "
                     "root=%d,%d stride=%u backing=%u:%08x clone=%u",
                     id, flags, width, height, root_x, root_y,
                     s->active_stride,
                     s->screen_backing_valid ? s->screen_backing_gmr_id :
                                               SVGA_GMR_NULL,
                     s->screen_backing_valid ? s->screen_backing_offset : 0,
                     clone_count);
  return true;
}

static bool vmsvga_screen_destroy(struct vmsvga_state_s *s,
                                  uint32_t screen_id) {
  if (screen_id != VMSVGA_SCREEN_V1_ID) {
    VMSVGA_SCREEN_REJECT("destroy reason=screen-id id=%u", screen_id);
    return false;
  }
  if (!s->screen_defined) {
    vmsvga_screen_base_clear(s);
    return true;
  }
  vmsvga_screen_base_clear(s);
  s->screen_defined = false;
  s->screen_flags = 0;
  s->screen_width = 0;
  s->screen_height = 0;
  s->screen_root_x = 0;
  s->screen_root_y = 0;
  s->screen_backing_valid = false;
  s->screen_handoff_active = false;
  s->screen_backing_gmr_id = SVGA_GMR_NULL;
  s->screen_backing_offset = 0;
  s->screen_backing_pitch = 0;
  s->screen_clone_count = 0;
  s->screen_stride = 0;
  s->screen_annotation_type = VMSVGA_ANNOTATION_NONE;
  s->damage_count = 0;
  s->svga_surface_bound = false;
  s->invalidated = true;
  if (vmsvga_trace_flight_enabled()) {
    s->trace_now.screen_destroys++;
    s->trace_activity_seq++;
  };
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
  bool trace_flight;
  bool source_changed;

  if (!vmsvga_screen_format_decode(format, &bpp, &depth, &bypp) ||
      bytes_per_line == 0 || gmr_id == SVGA_GMR_NULL) {
    VMSVGA_SCREEN_REJECT(
        "gmrfb reason=parameters gmr=%u offset=0x%08x pitch=%u format=0x%08x",
        gmr_id, offset, bytes_per_line, format);
    return false;
  }
  (void)bpp;
  (void)depth;
  (void)bypp;
  /* Validate the starting byte now; each blit validates its full row range. */
  if (!vmsvga_gmr_validate_range(s, gmr_id, offset, 0)) {
    VMSVGA_SCREEN_REJECT(
        "gmrfb reason=gmr-range gmr=%u offset=0x%08x pitch=%u format=0x%08x",
        gmr_id, offset, bytes_per_line, format);
    return false;
  }

  trace_flight = vmsvga_trace_flight_enabled();
  source_changed = trace_flight &&
                   (!s->gmrfb_defined || s->gmrfb_gmr_id != gmr_id ||
                    s->gmrfb_bytes_per_line != bytes_per_line ||
                    s->gmrfb_format != format);
  s->gmrfb_defined = true;
  s->gmrfb_gmr_id = gmr_id;
  s->gmrfb_offset = offset;
  s->gmrfb_bytes_per_line = bytes_per_line;
  s->gmrfb_format = format;
  if (trace_flight) {
    s->trace_now.gmrfb_defines++;
    s->trace_activity_seq++;
    if (s->trace_now.gmrfb_defines <= 24 || source_changed ||
        (s->trace_now.gmrfb_defines & 63) == 0) {
      fprintf(stderr,
              "VMVGA-GMR-DIAG gmrfb seq=%" PRIu64 " source=%s gmr=%u "
              "offset=0x%08x pitch=%u format=0x%08x bpp=%u depth=%u\n",
              s->trace_now.gmrfb_defines,
              gmr_id == SVGA_GMR_FRAMEBUFFER ? "framebuffer" : "gmr-v1",
              gmr_id, offset, bytes_per_line, format, bpp, depth);
    };
  };
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

static bool vmsvga_screen_is_direct_self_present(
    const struct vmsvga_state_s *s, int64_t source_x, int64_t source_y,
    int64_t left, int64_t top, uint32_t bypp) {
  return s->screen_backing_valid &&
         s->screen_backing_gmr_id == SVGA_GMR_FRAMEBUFFER &&
         s->gmrfb_gmr_id == SVGA_GMR_FRAMEBUFFER &&
         s->screen_backing_offset == s->gmrfb_offset &&
         s->screen_backing_pitch == s->gmrfb_bytes_per_line &&
         bypp == 4 && source_x == left && source_y == top;
}

static bool vmsvga_screen_is_direct_self_readback(
    const struct vmsvga_state_s *s, const SVGASignedPoint *dest_origin,
    const SVGASignedRect *src_rect, uint32_t bypp) {
  return s->screen_backing_valid &&
         s->screen_backing_gmr_id == SVGA_GMR_FRAMEBUFFER &&
         s->gmrfb_gmr_id == SVGA_GMR_FRAMEBUFFER &&
         s->screen_backing_offset == s->gmrfb_offset &&
         s->screen_backing_pitch == s->gmrfb_bytes_per_line &&
         bypp == 4 && dest_origin->x == src_rect->left &&
         dest_origin->y == src_rect->top;
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
  uint8_t *screen_base;
  size_t screen_size;
  uint32_t screen_stride;
  uint8_t *mirror_base = NULL;
  uint32_t mirror_stride = 0;
  bool trace_blit;
  uint32_t trace_before_hash;
  uint32_t trace_source_hash;
  uint32_t trace_after_hash;
  uint32_t trace_source_nonzero_rows;
  uint32_t trace_changed_rows;
  bool self_present;

  if (!vmsvga_screen_base_layer_storage(
          s, &screen_base, &screen_size, &screen_stride) ||
      !s->gmrfb_defined ||
      !vmsvga_screen_format_decode(s->gmrfb_format, &bpp, &depth, &bypp) ||
      right <= left || bottom <= top) {
    VMSVGA_SCREEN_REJECT(
        "blit-gmrfb-to-screen reason=state-or-rect screen=%d gmrfb=%d "
        "src=%d,%d dst=%d,%d-%d,%d format=0x%08x",
        s->screen_defined, s->gmrfb_defined, src_x, src_y, dst_left, dst_top,
        dst_right, dst_bottom, s->gmrfb_format);
    return false;
  }
  (void)screen_size;

  if (s->screen_handoff_active) {
    if (!vmsvga_screen_base_resize(s, s->screen_width, s->screen_height,
                                   s->screen_backing_pitch)) {
      VMSVGA_SCREEN_REJECT(
          "blit-gmrfb-to-screen reason=handoff-mirror size=%ux%u pitch=%u",
          s->screen_width, s->screen_height, s->screen_backing_pitch);
      return false;
    }
    mirror_base = s->screen_base;
    mirror_stride = s->screen_stride;
  }

  if (left < 0) { source_x -= left; left = 0; }
  if (top < 0) { source_y -= top; top = 0; }
  right = MIN(right, (int64_t)s->screen_width);
  bottom = MIN(bottom, (int64_t)s->screen_height);
  if (right <= left || bottom <= top) {
    return true;
  }
  if (source_x < 0 || source_y < 0) {
    VMSVGA_SCREEN_REJECT(
        "blit-gmrfb-to-screen reason=negative-source src=%" PRId64 ",%" PRId64
        " dst=%" PRId64 ",%" PRId64 "-%" PRId64 ",%" PRId64,
        source_x, source_y, left, top, right, bottom);
    return false;
  }

  width = (uint32_t)(right - left);
  height = (uint32_t)(bottom - top);
  self_present = vmsvga_screen_is_direct_self_present(
      s, source_x, source_y, left, top, bypp);
  trace_blit = vmsvga_trace_flight_enabled() &&
               (s->trace_now.gmrfb_to_screen < 16 ||
                ((s->trace_now.gmrfb_to_screen + 1) & 63) == 0);
  if (trace_blit) {
    trace_before_hash = 2166136261u;
    trace_source_hash = 2166136261u;
    trace_after_hash = 2166136261u;
    trace_source_nonzero_rows = 0;
    trace_changed_rows = 0;
  };
  for (row = 0; row < height; row++) {
    uint32_t gmr_offset;
    size_t row_bytes;
    uint8_t *dst = screen_base +
                   (uint64_t)(top + row) * screen_stride +
                   (uint64_t)left * 4;
    uint8_t *mirror_dst =
        mirror_base != NULL
            ? mirror_base + (uint64_t)(top + row) * mirror_stride +
                                (uint64_t)left * 4
            : NULL;

    if (!vmsvga_screen_gmrfb_row_offset(s, (int32_t)source_x,
                                        (int32_t)(source_y + row), width,
                                        bypp, &gmr_offset, &row_bytes)) {
      VMSVGA_SCREEN_REJECT(
          "blit-gmrfb-to-screen reason=row-range row=%u src=%" PRId64 ",%"
          PRId64 " width=%u", row, source_x, source_y + row, width);
      return false;
    }
    if (row_bytes > sizeof(s->blit_scratch)) {
      VMSVGA_SCREEN_REJECT(
          "blit-gmrfb-to-screen reason=row-too-wide row=%u bytes=%zu limit=%zu",
          row, row_bytes, sizeof(s->blit_scratch));
      return false;
    }
    if (!vmsvga_gmr_read(s, s->gmrfb_gmr_id, gmr_offset,
                         s->blit_scratch, row_bytes)) {
      VMSVGA_SCREEN_REJECT(
          "blit-gmrfb-to-screen reason=gmr-read row=%u gmr=%u offset=0x%08x "
          "bytes=%zu",
          row, s->gmrfb_gmr_id, gmr_offset, row_bytes);
      return false;
    }
    if (trace_blit) {
      uint8_t *trace_dst = mirror_dst != NULL ? mirror_dst : dst;
      uint32_t before_row_hash =
          vmsvga_gmr_diag_hash(trace_dst, (size_t)width * 4);
      size_t i;

      trace_before_hash = vmsvga_gmr_diag_hash_extend(
          trace_before_hash, trace_dst, (size_t)width * 4);
      trace_source_hash = vmsvga_gmr_diag_hash_extend(
          trace_source_hash, s->blit_scratch, row_bytes);
      for (i = 0; i < row_bytes; i++) {
        if (s->blit_scratch[i] != 0) {
          trace_source_nonzero_rows++;
          break;
        }
      }

      if (self_present) {
        if (mirror_dst != NULL) {
          memcpy(mirror_dst, s->blit_scratch, (size_t)width * 4);
        }
      } else {
        vmsvga_screen_gmrfb_to_bgrx(dst, s->blit_scratch, width, bpp, depth);
        if (mirror_dst != NULL) {
          memcpy(mirror_dst, dst, (size_t)width * 4);
        }
      }
      trace_after_hash = vmsvga_gmr_diag_hash_extend(
          trace_after_hash, trace_dst, (size_t)width * 4);
      if (before_row_hash !=
          vmsvga_gmr_diag_hash(trace_dst, (size_t)width * 4)) {
        trace_changed_rows++;
      }
    } else {
      if (self_present) {
        if (mirror_dst != NULL) {
          memcpy(mirror_dst, s->blit_scratch, (size_t)width * 4);
        }
      } else {
        vmsvga_screen_gmrfb_to_bgrx(dst, s->blit_scratch, width, bpp, depth);
        if (mirror_dst != NULL) {
          memcpy(mirror_dst, dst, (size_t)width * 4);
        }
      }
    }
  }

  if (trace_blit) {
    fprintf(stderr,
            "VMVGA-SCREEN-BLIT seq=%" PRIu64 " target=%s source=%s gmr=%u "
            "rect=%" PRId64 ",%" PRId64 "-%" PRId64 ",%" PRId64 " "
            "rows=%u width=%u before=0x%08x source-hash=0x%08x "
            "after=0x%08x source-nonzero-rows=%u changed-rows=%u\n",
            s->trace_now.gmrfb_to_screen + 1,
            mirror_base != NULL ? "handoff-mirror" :
            (self_present ? "backing-self-present" : "backing"),
            s->gmrfb_gmr_id == SVGA_GMR_FRAMEBUFFER ? "framebuffer" :
                                                      "gmr-v1",
            s->gmrfb_gmr_id, left, top, right, bottom, height, width,
            trace_before_hash, trace_source_hash, trace_after_hash,
            trace_source_nonzero_rows, trace_changed_rows);
  }

  /*
   * An exact framebuffer-backed self-present is a notification, not a VRAM
   * write. VirtualBox's transfer is effectively a same-address memmove here.
   * Do not manufacture DIRTY_MEMORY_VGA bits: steady-state dirty tracking
   * must reflect real guest/renderer writes so it can recover writes which
   * become visible after the FIFO notification.
   */
  if (!self_present) {
    vmsvga_screen_mark_dirty(s, (uint32_t)left, (uint32_t)top,
                             width, height);
  }
  {
    DisplaySurface *surface = qemu_console_surface(s->vga.con);
    uint8_t *scanout_base = NULL;
    size_t scanout_size = 0;
    uint32_t scanout_stride = 0;
    bool scanout_bound =
        s->svga_surface_bound && surface != NULL &&
        vmsvga_screen_storage(s, &scanout_base, &scanout_size,
                              &scanout_stride) &&
        surface_data(surface) == scanout_base;

    (void)scanout_size;
    (void)scanout_stride;
    if (scanout_bound) {
      if (vmsvga_trace_flight_enabled()) {
        uint64_t seq = s->trace_now.gmrfb_to_screen + 1;

        s->trace_now.damage_rects++;
        s->trace_activity_seq++;
        if (seq <= 16 || (seq & 63) == 0) {
          fprintf(stderr,
                  "VMVGA-SCREEN-DAMAGE seq=%" PRIu64 " surface=%p data=%p "
                  "size=%dx%d stride=%d scanout=%p scanout-match=%u "
                  "rect=%" PRId64 ",%" PRId64 "-%" PRId64 ",%" PRId64
                  " handoff=%u bound=%u\n",
                  seq, (void *)surface, (void *)surface_data(surface),
                  surface_width(surface), surface_height(surface),
                  surface_stride(surface), (void *)scanout_base,
                  surface_data(surface) == scanout_base, left, top, right,
                  bottom, s->screen_handoff_active, s->svga_surface_bound);
        }
      }
      vmvga_console_update(s->vga.con, (uint32_t)left, (uint32_t)top,
                           width, height);
    } else {
      vmsvga_damage_add(s, (uint32_t)left, (uint32_t)top, width, height);
    }
  }
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
    VMSVGA_SCREEN_REJECT(
        "blit-gmrfb-to-screen reason=missing-state screen=%d src=%d rect=%d "
        "dest-id=%u",
        s->screen_defined, src_origin != NULL, dest_rect != NULL,
        dest_screen_id);
    return false;
  }
  width = (int64_t)dest_rect->right - dest_rect->left;
  height = (int64_t)dest_rect->bottom - dest_rect->top;
  if (width <= 0 || height <= 0 || width > INT32_MAX || height > INT32_MAX) {
    VMSVGA_SCREEN_REJECT(
        "blit-gmrfb-to-screen reason=dest-rect dest-id=%u rect=%d,%d-%d,%d",
        dest_screen_id, dest_rect->left, dest_rect->top, dest_rect->right,
        dest_rect->bottom);
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
      VMSVGA_SCREEN_REJECT(
          "blit-gmrfb-to-screen reason=root-overflow root=%d,%d rect=%d,%d-%d,%d",
          s->screen_root_x, s->screen_root_y, dest_rect->left, dest_rect->top,
          dest_rect->right, dest_rect->bottom);
      return false;
    }
    local_left = (int32_t)left;
    local_top = (int32_t)top;
    local_right = (int32_t)right;
    local_bottom = (int32_t)bottom;
  } else {
    VMSVGA_SCREEN_REJECT("blit-gmrfb-to-screen reason=screen-id dest-id=%u",
                         dest_screen_id);
    return false;
  }

  if (vmsvga_trace_flight_enabled() && s->trace_now.gmrfb_to_screen < 12) {
    fprintf(stderr,
            "VMVGA-GMR-DIAG g2s seq=%" PRIu64 " source=%s gmr=%u "
            "base=0x%08x pitch=%u format=0x%08x src=%d,%d "
            "dst-id=%u dst=%d,%d-%d,%d local=%d,%d-%d,%d\n",
            s->trace_now.gmrfb_to_screen + 1,
            s->gmrfb_gmr_id == SVGA_GMR_FRAMEBUFFER ? "framebuffer" :
                                                      "gmr-v1",
            s->gmrfb_gmr_id, s->gmrfb_offset, s->gmrfb_bytes_per_line,
            s->gmrfb_format, src_origin->x, src_origin->y, dest_screen_id,
            dest_rect->left, dest_rect->top, dest_rect->right,
            dest_rect->bottom, local_left, local_top, local_right,
            local_bottom);
  };
  ok = vmsvga_screen_blit_one_from_gmrfb(
      s, src_origin->x, src_origin->y, local_left, local_top,
      local_right, local_bottom);
  if (ok && s->screen_handoff_active &&
      local_left <= 0 && local_top <= 0 &&
      local_right >= (int32_t)s->screen_width &&
      local_bottom >= (int32_t)s->screen_height) {
    s->screen_handoff_active = false;
    s->svga_surface_bound = false;
    s->invalidated = true;
    if (vmsvga_trace_flight_enabled()) {
      fprintf(stderr,
              "VMVGA-SCREEN-HANDOFF phase=complete seq=%" PRIu64
              " size=%ux%u pitch=%u backing=%u:0x%08x\n",
              s->trace_now.gmrfb_to_screen + 1, s->screen_width,
              s->screen_height, s->screen_backing_pitch,
              s->screen_backing_gmr_id, s->screen_backing_offset);
    }
    vmsvga_check_size(s);
  }
  s->screen_annotation_type = VMSVGA_ANNOTATION_NONE;
  if (vmsvga_trace_flight_enabled()) {
    uint64_t seq = s->trace_now.gmrfb_to_screen + 1;
    if (ok) {
      vmsvga_screen_trace_present_snapshot(s, seq, "g2s");
    };
    s->trace_now.gmrfb_to_screen++;
    s->trace_activity_seq++;
  };
  return ok;
}

static bool vmsvga_screen_blit_screen_to_gmrfb(
    struct vmsvga_state_s *s, const SVGASignedPoint *dest_origin,
    const SVGASignedRect *src_rect, uint32_t src_screen_id) {
  uint32_t bpp, depth, bypp;
  int64_t width64, height64;
  uint32_t width, height, row;
  uint8_t *screen_base;
  size_t screen_size;
  uint32_t screen_stride;

  if (!vmsvga_screen_base_layer_storage(
          s, &screen_base, &screen_size, &screen_stride) ||
      !s->gmrfb_defined || dest_origin == NULL || src_rect == NULL ||
      src_screen_id != VMSVGA_SCREEN_V1_ID ||
      !vmsvga_screen_format_decode(s->gmrfb_format, &bpp, &depth, &bypp)) {
    VMSVGA_SCREEN_REJECT(
        "blit-screen-to-gmrfb reason=state-or-id screen=%d gmrfb=%d src-id=%u "
        "format=0x%08x",
        s->screen_defined, s->gmrfb_defined, src_screen_id, s->gmrfb_format);
    return false;
  }
  (void)screen_size;
  width64 = (int64_t)src_rect->right - src_rect->left;
  height64 = (int64_t)src_rect->bottom - src_rect->top;
  if (src_rect->left < 0 || src_rect->top < 0 || dest_origin->x < 0 ||
      dest_origin->y < 0 || width64 <= 0 || height64 <= 0 ||
      src_rect->right > (int32_t)s->screen_width ||
      src_rect->bottom > (int32_t)s->screen_height ||
      width64 > UINT32_MAX || height64 > UINT32_MAX) {
    VMSVGA_SCREEN_REJECT(
        "blit-screen-to-gmrfb reason=rect src=%d,%d-%d,%d dest=%d,%d "
        "screen=%ux%u",
        src_rect->left, src_rect->top, src_rect->right, src_rect->bottom,
        dest_origin->x, dest_origin->y, s->screen_width, s->screen_height);
    return false;
  }
  width = (uint32_t)width64;
  height = (uint32_t)height64;

  /*
   * When both the Screen Object and GMRFB describe the same BAR1 bytes,
   * SCREEN_TO_GMRFB is an exact self-copy. VirtualBox's framebuffer GMR
   * shortcut performs a same-address memcpy in this case. Avoid round-tripping
   * BGRX through the conversion helper, which would otherwise rewrite the
   * unused byte and mutate VRAM during a readback notification.
   */
  if (vmsvga_screen_is_direct_self_readback(s, dest_origin, src_rect, bypp)) {
    if (vmsvga_trace_flight_enabled()) {
      fprintf(stderr,
              "VMVGA-SCREEN-READBACK seq=%" PRIu64
              " target=backing-self-copy action=noop rect=%d,%d-%d,%d\n",
              s->trace_now.screen_to_gmrfb + 1, src_rect->left,
              src_rect->top, src_rect->right, src_rect->bottom);
      s->trace_now.screen_to_gmrfb++;
      s->trace_activity_seq++;
    }
    return true;
  }

  for (row = 0; row < height; row++) {
    uint32_t gmr_offset;
    size_t row_bytes;
    const uint8_t *src = screen_base +
        (uint64_t)(src_rect->top + (int32_t)row) * screen_stride +
        (uint64_t)src_rect->left * 4;
    if (!vmsvga_screen_gmrfb_row_offset(s, dest_origin->x,
                                        dest_origin->y + (int32_t)row,
                                        width, bypp, &gmr_offset,
                                        &row_bytes)) {
      VMSVGA_SCREEN_REJECT(
          "blit-screen-to-gmrfb reason=row-range row=%u dest=%d,%d width=%u",
          row, dest_origin->x, dest_origin->y + (int32_t)row, width);
      return false;
    }
    if (row_bytes > sizeof(s->blit_scratch)) {
      VMSVGA_SCREEN_REJECT(
          "blit-screen-to-gmrfb reason=row-too-wide row=%u bytes=%zu limit=%zu",
          row, row_bytes, sizeof(s->blit_scratch));
      return false;
    }
    vmsvga_screen_bgrx_to_gmrfb(s->blit_scratch, src, width, bpp, depth);
    if (!vmsvga_gmr_write(s, s->gmrfb_gmr_id, gmr_offset,
                          s->blit_scratch, row_bytes)) {
      VMSVGA_SCREEN_REJECT(
          "blit-screen-to-gmrfb reason=gmr-write row=%u gmr=%u offset=0x%08x "
          "bytes=%zu",
          row, s->gmrfb_gmr_id, gmr_offset, row_bytes);
      return false;
    }
  }

  if (vmsvga_trace_flight_enabled()) {
    s->trace_now.screen_to_gmrfb++;
    s->trace_activity_seq++;
  };
  return true;
}

static void vmsvga_screen_annotation_fill(struct vmsvga_state_s *s,
                                          uint32_t color) {
  s->screen_annotation_type = VMSVGA_ANNOTATION_FILL;
  s->screen_annotation_color = color;
  if (vmsvga_trace_flight_enabled()) {
    s->trace_now.annotation_fills++;
    s->trace_activity_seq++;
  };
}

static void vmsvga_screen_annotation_copy(struct vmsvga_state_s *s,
                                          int32_t src_x, int32_t src_y,
                                          uint32_t src_screen_id) {
  s->screen_annotation_type = VMSVGA_ANNOTATION_COPY;
  s->screen_annotation_src_x = src_x;
  s->screen_annotation_src_y = src_y;
  s->screen_annotation_src_id = src_screen_id;
  if (vmsvga_trace_flight_enabled()) {
    s->trace_now.annotation_copies++;
    s->trace_activity_seq++;
  };
}

static inline void vmsvga_screen_record_surface_to_screen(
    struct vmsvga_state_s *s) {
  if (vmsvga_trace_flight_enabled()) {
    s->trace_now.surface_to_screen++;
    s->trace_activity_seq++;
  };
}
