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
