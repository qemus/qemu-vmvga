/*

 QEMU VMware Super Video Graphics Array 2 [SVGA-II]

 Copyright (c) 2007 Andrzej Zaborowski <balrog@zabor.org>

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
// #define ANY_FENCE_OFF
// #define EXPCAPS
// #define RAISE_IRQ_OFF
// #define VERBOSE
#include "qemu/osdep.h" // Required to be the first #include
#include "qapi/error.h"
#include "exec/target_page.h"
#include "trace.h"
#include "include/vmware_vga_compat.h"
#include "include/vmware_vga_gmr.h"
#include "include/includeCheck.h"
#include "include/svga_types.h"
#include "include/svga_escape.h"
#include "include/svga_overlay.h"
#include "include/svga_reg.h"
#include "include/vmware_pack_begin.h"
#include "include/vmware_pack_end.h"
#include "migration/vmstate.h"
#include "vga_int.h"
#define SVGA_CAP_ALPHA_BLEND 0x00002000
#define SVGA_CAP_GLYPH 0x00000400
#define SVGA_CAP_GLYPH_CLIPPING 0x00000800
#define SVGA_CAP_HP_CMD_QUEUE 0x20000000
#define SVGA_CAP_LEGACY_OFFSCREEN 0x00000008
#define SVGA_CAP_NO_BB_RESTRICTION 0x40000000
#define SVGA_CAP_OFFSCREEN_1 0x00001000
#define SVGA_CAP_RASTER_OP 0x00000010
#define SVGA_CAP_RECT_FILL 0x00000001
#define SVGA_CAP_RECT_PAT_FILL 0x00000004
#define SVGA_CMD_DEFINE_BITMAP 4
#define SVGA_CMD_DEFINE_BITMAP_SCANLINE 5
#define SVGA_CMD_DEFINE_PIXMAP 6
#define SVGA_CMD_DEFINE_PIXMAP_SCANLINE 7
#define SVGA_CMD_DISPLAY_CURSOR 20
#define SVGA_CMD_DRAW_GLYPH 23
#define SVGA_CMD_DRAW_GLYPH_CLIPPED 24
#define SVGA_CMD_FREE_OBJECT 12
#define SVGA_CMD_MOVE_CURSOR 21
#define SVGA_CMD_RECT_BITMAP_COPY 10
#define SVGA_CMD_RECT_BITMAP_FILL 8
#define SVGA_CMD_RECT_FILL 2
#define SVGA_CMD_RECT_PIXMAP_COPY 11
#define SVGA_CMD_RECT_PIXMAP_FILL 9
#define SVGA_CMD_RECT_ROP_BITMAP_COPY 17
#define SVGA_CMD_RECT_ROP_BITMAP_FILL 15
#define SVGA_CMD_RECT_ROP_FILL 13
#define SVGA_CMD_RECT_ROP_PIXMAP_COPY 18
#define SVGA_CMD_RECT_ROP_PIXMAP_FILL 16
#define SVGA_CMD_SURFACE_ALPHA_BLEND 28
#define SVGA_CMD_SURFACE_COPY 27
#define SVGA_CMD_SURFACE_FILL 26
#define SVGA_PALETTE_SIZE 768
#define VMSVGA_PALETTE_STORAGE_SIZE 768
#define SVGA_PIXMAP_SIZE(w, h, bpp) (((((w) * (bpp)) + 31) >> 5) * (h))
#define VMSVGA_MAX_WIDTH 8192
#define VMSVGA_MAX_HEIGHT 8192
#define VMSVGA_HOST_BITS_PER_PIXEL 32
#define VMSVGA_CURSOR_MAX_DIMENSION 512
#define VMSVGA_FIFO_SIZE (2 * 1024 * 1024)
#define VMSVGA_VGA_FB_BACKUP_SIZE (512 * 1024)
#define VMSVGA_SCRATCH_SIZE 32
#define VMSVGA_CURSOR_MAX_BYTE_SIZE \
  (VMSVGA_CURSOR_MAX_DIMENSION * VMSVGA_CURSOR_MAX_DIMENSION * 8)
#define SVGA_REG_CURSOR_MAX_BYTE_SIZE 66
#define SVGA_REG_CURSOR_MAX_DIMENSION 67
#define SVGA_REG_CURSOR_MOBID 65
#define SVGA_REG_FENCE 69
#define SVGA_REG_FENCE_GOAL 84
#define SVGA_REG_FIFO_CAPS 68
#define SVGA_REG_GBOBJECT_MEM_SIZE_KB 76
#define SVGA_REG_MSHINT 81
#define SVGA_REG_PALETTE_MAX \
  (SVGA_REG_PALETTE_MIN + SVGA_PALETTE_SIZE - 1)
#define SVGA_REG_PALETTE_MIN 1024
#define SVGA_REG_SCREENDMA 75
enum {
  VMSVGA_ROP_CLEAR = 0x00,
  VMSVGA_ROP_AND = 0x01,
  VMSVGA_ROP_AND_REVERSE = 0x02,
  VMSVGA_ROP_COPY = SVGA_ROP_COPY,
  VMSVGA_ROP_AND_INVERTED = 0x04,
  VMSVGA_ROP_NOOP = 0x05,
  VMSVGA_ROP_XOR = 0x06,
  VMSVGA_ROP_OR = 0x07,
  VMSVGA_ROP_NOR = 0x08,
  VMSVGA_ROP_EQUIV = 0x09,
  VMSVGA_ROP_INVERT = 0x0a,
  VMSVGA_ROP_OR_REVERSE = 0x0b,
  VMSVGA_ROP_COPY_INVERTED = 0x0c,
  VMSVGA_ROP_OR_INVERTED = 0x0d,
  VMSVGA_ROP_NAND = 0x0e,
  VMSVGA_ROP_SET = 0x0f,
};
enum {
  VMSVGA_BLENDOP_CLEAR = 0,
  VMSVGA_BLENDOP_SRC = 1,
  VMSVGA_BLENDOP_DST = 2,
  VMSVGA_BLENDOP_OVER = 3,
  VMSVGA_BLENDOP_OVER_REVERSE = 4,
  VMSVGA_BLENDOP_IN = 5,
  VMSVGA_BLENDOP_IN_REVERSE = 6,
  VMSVGA_BLENDOP_OUT = 7,
  VMSVGA_BLENDOP_OUT_REVERSE = 8,
  VMSVGA_BLENDOP_ATOP = 9,
  VMSVGA_BLENDOP_ATOP_REVERSE = 10,
  VMSVGA_BLENDOP_XOR = 11,
  VMSVGA_BLENDOP_ADD = 12,
  VMSVGA_BLENDOP_SATURATE = 13,
};
#define VMSVGA_BLENDFLAG_CONSTANT_SOURCE_ALPHA 0x01
#define VMSVGA_BLENDFLAG_CONSTANT_DEST_ALPHA 0x02
#define VMSVGA_BLENDFLAG_ALL                                             \
  (VMSVGA_BLENDFLAG_CONSTANT_SOURCE_ALPHA |                              \
   VMSVGA_BLENDFLAG_CONSTANT_DEST_ALPHA)
#define VMSVGA_MAX_OBJECTS 500
#define VMSVGA_MAX_CURSORS 500
#define VMSVGA_DAMAGE_RECTS 256
#define VMSVGA_DIRTY_BLOCK_PAGES 64
#define VMSVGA_PSEUDOCOLOR_ENTRIES 256
#define VMSVGA_BLIT_SCRATCH_SIZE (VMSVGA_MAX_WIDTH * 4)

enum vmsvga_object_type_e {
  VMSVGA_OBJECT_BITMAP = 1,
  VMSVGA_OBJECT_PIXMAP = 2,
};

struct vmsvga_object_s {
  uint32_t type;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t stride;
  size_t size;
  uint8_t *data;
};

/*
 * Legacy guests publish FIFO commands a DWORD at a time and may need to
 * SYNC for room before a large DEFINE_BITMAP/DEFINE_PIXMAP is complete.
 * Keep enough state to consume those payloads incrementally across FIFO runs.
 */
struct vmsvga_fifo_upload_s {
  bool active;
  bool discard;
  uint32_t type;
  uint32_t id;
  uint32_t total_words;
  uint32_t received_words;
};

#define VMSVGA_SURFACE_VERSION_1 1

struct vmsvga_surface_s {
  uint32_t descriptor_offset;
  uint32_t bpp;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t data_offset;
  uint32_t bypp;
};

struct vmsvga_pixel_s {
  uint32_t red;
  uint32_t green;
  uint32_t blue;
  uint32_t alpha;
};

struct vmsvga_damage_rect_s {
  uint32_t x;
  uint32_t y;
  uint32_t w;
  uint32_t h;
};

enum vmsvga_trace_display_path_e {
  VMSVGA_TRACE_DISPLAY_UNKNOWN = 0,
  VMSVGA_TRACE_DISPLAY_HIDDEN,
  VMSVGA_TRACE_DISPLAY_SVGA,
  VMSVGA_TRACE_DISPLAY_VGA,
};

/*
 * These VMState shadows borrow runtime buffers while saving and own the
 * VMState-allocated buffers while loading, until post_load transfers them.
 */
struct vmsvga_object_migration_s {
  bool present;
  uint32_t type;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t stride;
  uint32_t data_size;
  uint8_t *data;
};

struct vmsvga_cursor_migration_s {
  bool present;
  uint32_t width;
  uint32_t height;
  int32_t hot_x;
  int32_t hot_y;
  uint32_t pixel_count;
  uint32_t *data;
  bool raw;
  bool alpha;
  uint32_t and_mask_bpp;
  uint32_t xor_mask_bpp;
  uint32_t and_size;
  uint32_t xor_size;
  uint8_t *and_data;
  uint8_t *xor_data;
};

struct vmsvga_cursor_source_s {
  bool alpha;
  uint32_t width;
  uint32_t height;
  uint32_t hot_x;
  uint32_t hot_y;
  uint32_t and_mask_bpp;
  uint32_t xor_mask_bpp;
  uint32_t and_size;
  uint32_t xor_size;
  uint8_t *and_data;
  uint8_t *xor_data;
};

#ifdef VERBOSE
#define VPRINT(fmt, ...)                                                       \
  printf("vmsvga (%s): %u - %s: " fmt, __FILE__, (uint32_t)time(NULL),         \
         __func__, ##__VA_ARGS__)
#else
#define VPRINT(...)
#endif

#define VMVGA_DIAG_BLIT_FAIL 1

#if VMVGA_DIAG_BLIT_FAIL
#define VMVGA_BLIT_FAIL(fmt, ...)                                             \
  do {                                                                         \
    fprintf(stderr, "VMVGA-BLIT-FAIL: " fmt "\n", ##__VA_ARGS__);            \
  } while (0)
#else
#define VMVGA_BLIT_FAIL(fmt, ...) do { } while (0)
#endif

/*
 * Overlay-local diagnostics.
 *
 * This project overlays vmware_vga.c onto an otherwise stock QEMU source
 * tree, so it cannot add generated trace events without also replacing
 * hw/display/trace-events.  Reuse QEMU's existing vmware_setmode trace event
 * as the runtime master switch, then keep the extra diagnostics local.
 *
 * Enable at runtime with:
 *   -trace "vmware_setmode"
 *
 * Add -trace "vmware_value_read" when register reads/BUSY polling are needed.
 * Categories set to 0 compile down to a constant-false branch.
 */
#define VMVGA_TRACE_STATE   0
#define VMVGA_TRACE_DRAW    0
#define VMVGA_TRACE_DIRTY   0
#define VMVGA_TRACE_ROP     0
#define VMVGA_TRACE_OBJECT  0
#define VMVGA_TRACE_STREAM  0
#define VMVGA_TRACE_FIFO    0
#define VMVGA_TRACE_3D      1
#define VMVGA_TRACE_FLIGHT  1
#define VMVGA_TRACE_QEMU    1

#define VMVGA_TRACE_CMD_MAX 256
#define VMVGA_TRACE_DEVCAP_MAX 256
#define VMVGA_TRACE_REPORT_INTERVAL_US 1000000

#define VMVGA_TRACE_LOCAL_ENABLED(category)                              \
  ((category) &&                                                         \
   trace_event_get_state_backends(TRACE_VMWARE_SETMODE))

#define VMVGA_TRACE_LOCAL(category, fmt, ...)                            \
  do {                                                                   \
    if (VMVGA_TRACE_LOCAL_ENABLED(category)) {                            \
      fprintf(stderr, "VMVGA-" fmt "\n", ##__VA_ARGS__);                 \
    };                                                                   \
  } while (0)

#if VMVGA_TRACE_QEMU
#define VMVGA_QEMU_TRACE(call) do { call; } while (0)
#else
#define VMVGA_QEMU_TRACE(call) do { } while (0)
#endif

struct vmsvga_gmr_s;
struct vmsvga3d_state_s;
struct vmsvga3d_dxvk_s;

struct vmsvga_trace_counters_s {
  uint64_t fifo_runs;
  uint64_t fifo_empty_runs;
  uint64_t fifo_cmds;
  uint64_t fifo_2d_cmds;
  uint64_t fifo_3d_cmds;
  uint64_t fifo_updates;
  uint64_t fifo_surface_2d;
  uint64_t fifo_screen_blits;
  uint64_t fifo_gmr;
  uint64_t fifo_fences;
  uint64_t fifo_stalls;
  uint64_t fifo_prefilter_stalls;
  uint64_t screen_defines;
  uint64_t screen_destroys;
  uint64_t gmrfb_defines;
  uint64_t gmrfb_to_screen;
  uint64_t screen_to_gmrfb;
  uint64_t annotation_fills;
  uint64_t annotation_copies;
  uint64_t surface_to_screen;
  uint64_t sync_writes;
  uint64_t sync_fifo_busy_zero;
  uint64_t sync_fifo_busy_one;
  uint64_t busy_reads;
  uint64_t dirty_pages;
  uint64_t damage_rects;
};

struct vmsvga_trace_cmd_counter_s {
  uint32_t cmd;
  uint64_t count;
};

struct vmsvga_trace_devcap_s {
  uint32_t index;
  uint32_t value;
};

struct vmsvga_state_s {
  uint32_t svgapalettebase[VMSVGA_PALETTE_STORAGE_SIZE];
#ifdef CONFIG_PIXMAN
  pixman_indexed_t indexed_palette;
#endif
  uint32_t enable;
  uint32_t config;
  uint32_t index;
  uint32_t scratch_size;
  uint32_t new_width;
  uint32_t new_height;
  uint32_t new_depth;
  bool active_valid;
  uint32_t active_width;
  uint32_t active_height;
  uint32_t active_depth;
  uint32_t active_stride;
  uint32_t num_gd;
  uint32_t disp_prim;
  uint32_t disp_x;
  uint32_t disp_y;
  uint32_t disp_width;
  uint32_t disp_height;
  uint32_t devcap_val;
  uint32_t gmrdesc;
  uint32_t gmrid;
  uint32_t gmrpage;
  uint32_t traces;
  uint32_t cmd_low;
  uint32_t cmd_high;
  uint32_t guest;
  uint32_t svgaid;
  uint32_t thread;
  uint32_t sync;
  uint32_t fifo_size;
  uint32_t fifo_min;
  uint32_t fifo_max;
  uint32_t fifo_next;
  uint32_t fifo_stop;
  uint32_t irq_mask;
  uint32_t irq_status;
  uint32_t display_id;
  uint32_t pitchlock;
  /* Indexed cursor registers are guest-visible pending state. The active
   * tuple changes only at the protocol commit point (CURSOR_ON), or when a
   * Cursor Bypass 3 generation is consumed. */
  uint32_t cursor;
  uint32_t cursor_x;
  uint32_t cursor_y;
  uint32_t cursor_on;
  uint32_t active_cursor;
  uint32_t active_cursor_x;
  uint32_t active_cursor_y;
  uint32_t active_cursor_on;
  uint32_t fence;
  uint32_t fence_goal;
  uint32_t fc;
  uint32_t ff;
  uint32_t *fifo;
  uint32_t scratch[VMSVGA_SCRATCH_SIZE];
  struct vmsvga_gmr_s *gmrs[64];
  /* Screen Object ID 0. Backed Screen Objects scan out directly from their
   * guest-provided BAR1 backingStore. screen_base is retained only for the
   * optional unbacked Screen Object form. */
  uint8_t *screen_base;
  size_t screen_base_size;
  uint32_t screen_stride;
  uint32_t screen_base_migration_size;
  bool screen_defined;
  uint32_t screen_flags;
  uint32_t screen_width;
  uint32_t screen_height;
  int32_t screen_root_x;
  int32_t screen_root_y;
  bool screen_backing_valid;
  uint32_t screen_backing_gmr_id;
  uint32_t screen_backing_offset;
  uint32_t screen_backing_pitch;
  uint32_t screen_clone_count;
  bool gmrfb_defined;
  uint32_t gmrfb_gmr_id;
  uint32_t gmrfb_offset;
  uint32_t gmrfb_bytes_per_line;
  uint32_t gmrfb_format;
  uint32_t screen_annotation_type;
  uint32_t screen_annotation_color;
  int32_t screen_annotation_src_x;
  int32_t screen_annotation_src_y;
  uint32_t screen_annotation_src_id;
  struct vmsvga3d_state_s *svga3d;
  struct vmsvga3d_dxvk_s *dxvk;
  /* Guest-visible 3D capability is latched once during device realization. */
  bool svga3d_capable;
  struct vmsvga_object_s *objects[VMSVGA_MAX_OBJECTS];
  struct vmsvga_fifo_upload_s fifo_upload;
  QEMUCursor *cursor_cache[VMSVGA_MAX_CURSORS];
  struct vmsvga_cursor_source_s *cursor_source[VMSVGA_MAX_CURSORS];
  struct vmsvga_object_migration_s object_migration[VMSVGA_MAX_OBJECTS];
  struct vmsvga_cursor_migration_s cursor_migration[VMSVGA_MAX_CURSORS];
  size_t object_bytes;
  uint8_t blit_scratch[VMSVGA_BLIT_SCRATCH_SIZE];
  struct vmsvga_damage_rect_s damage[VMSVGA_DAMAGE_RECTS];
  uint32_t damage_count;
  VGACommonState vga;
  bool invalidated;
  /* The host console surface is not migrated. Track whether it is currently
   * bound to the selected SVGA scanout backing rather than the VGA path. */
  bool svga_surface_bound;
  bool hidden;
  bool cursor_dirty;
  bool dirty_log_enabled;
  bool test_marker;
  bool marker_logged;
  uint32_t trace_display_path;
  /* Diagnostic-only flight recorder state; intentionally excluded from VMState. */
  struct vmsvga_trace_counters_s trace_now;
  struct vmsvga_trace_counters_s trace_last;
  struct vmsvga_trace_cmd_counter_s trace_cmd[VMVGA_TRACE_CMD_MAX];
  uint32_t trace_cmd_count;
  struct vmsvga_trace_devcap_s trace_devcap[VMVGA_TRACE_DEVCAP_MAX];
  uint32_t trace_devcap_count;
  uint64_t trace_activity_seq;
  uint64_t trace_last_report_seq;
  int64_t trace_last_report_us;
  bool trace_fifo_have_stall;
  uint32_t trace_fifo_last_stall_cmd;
  uint32_t trace_fifo_last_stall_stop;
  uint32_t trace_fifo_last_stall_stage;
  uint64_t trace_fifo_stall_repeat;
  uint64_t trace_key_reg_seen[2];
  bool trace_3d_version_seen;
  uint32_t trace_last_guest_3d_hwversion;
  uint32_t trace_last_host_3d_hwversion;
  uint32_t trace_last_revised_3d_hwversion;
  MemoryRegion fifo_ram;
  MemoryRegion legacy_vga_mem;
  uint8_t *legacy_vga_ptr;
  uint32_t legacy_vga_size;
};
DECLARE_INSTANCE_CHECKER(struct pci_vmsvga_state_s, VMWARE_SVGA, "vmware-svga")
struct pci_vmsvga_state_s {
  PCIDevice parent_obj;
  struct vmsvga_state_s chip;
  MemoryRegion io_bar;
};

static inline bool vmsvga_trace_flight_enabled(void) {
  return VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_FLIGHT);
};

static inline void vmsvga_trace_flight_activity(struct vmsvga_state_s *s) {
  if (vmsvga_trace_flight_enabled()) {
    s->trace_activity_seq++;
  };
};

static inline void vmsvga_trace_flight_reset(struct vmsvga_state_s *s) {
  memset(&s->trace_now, 0, sizeof(s->trace_now));
  memset(&s->trace_last, 0, sizeof(s->trace_last));
  memset(s->trace_cmd, 0, sizeof(s->trace_cmd));
  memset(s->trace_devcap, 0, sizeof(s->trace_devcap));
  memset(s->trace_key_reg_seen, 0, sizeof(s->trace_key_reg_seen));
  s->trace_cmd_count = 0;
  s->trace_devcap_count = 0;
  s->trace_activity_seq = 0;
  s->trace_last_report_seq = 0;
  s->trace_last_report_us = 0;
  s->trace_fifo_have_stall = false;
  s->trace_fifo_last_stall_cmd = 0;
  s->trace_fifo_last_stall_stop = 0;
  s->trace_fifo_last_stall_stage = 0;
  s->trace_fifo_stall_repeat = 0;
  s->trace_3d_version_seen = false;
  s->trace_last_guest_3d_hwversion = 0;
  s->trace_last_host_3d_hwversion = 0;
  s->trace_last_revised_3d_hwversion = 0;
};

static const char *vmsvga_trace_fifo_cmd_name(uint32_t cmd) {
  switch (cmd) {
  case SVGA_CMD_UPDATE: return "SVGA_CMD_UPDATE";
  case SVGA_CMD_RECT_FILL: return "SVGA_CMD_RECT_FILL";
  case SVGA_CMD_RECT_COPY: return "SVGA_CMD_RECT_COPY";
  case SVGA_CMD_UPDATE_VERBOSE: return "SVGA_CMD_UPDATE_VERBOSE";
  case SVGA_CMD_SURFACE_FILL: return "SVGA_CMD_SURFACE_FILL";
  case SVGA_CMD_SURFACE_COPY: return "SVGA_CMD_SURFACE_COPY";
  case SVGA_CMD_SURFACE_ALPHA_BLEND: return "SVGA_CMD_SURFACE_ALPHA_BLEND";
  case SVGA_CMD_FENCE: return "SVGA_CMD_FENCE";
  case SVGA_CMD_ESCAPE: return "SVGA_CMD_ESCAPE";
  case SVGA_CMD_DEFINE_SCREEN: return "SVGA_CMD_DEFINE_SCREEN";
  case SVGA_CMD_DESTROY_SCREEN: return "SVGA_CMD_DESTROY_SCREEN";
  case SVGA_CMD_DEFINE_GMRFB: return "SVGA_CMD_DEFINE_GMRFB";
  case SVGA_CMD_BLIT_GMRFB_TO_SCREEN: return "SVGA_CMD_BLIT_GMRFB_TO_SCREEN";
  case SVGA_CMD_BLIT_SCREEN_TO_GMRFB: return "SVGA_CMD_BLIT_SCREEN_TO_GMRFB";
  case SVGA_CMD_ANNOTATION_FILL: return "SVGA_CMD_ANNOTATION_FILL";
  case SVGA_CMD_ANNOTATION_COPY: return "SVGA_CMD_ANNOTATION_COPY";
  case SVGA_CMD_DEFINE_GMR2: return "SVGA_CMD_DEFINE_GMR2";
  case SVGA_CMD_REMAP_GMR2: return "SVGA_CMD_REMAP_GMR2";
  case SVGA_CMD_DEFINE_CURSOR: return "SVGA_CMD_DEFINE_CURSOR";
  case SVGA_CMD_DEFINE_ALPHA_CURSOR: return "SVGA_CMD_DEFINE_ALPHA_CURSOR";
  default: return NULL;
  };
};

static const char *vmsvga_trace_reg_name(uint32_t reg) {
  switch (reg) {
  case SVGA_REG_ID: return "SVGA_REG_ID";
  case SVGA_REG_CAPABILITIES: return "SVGA_REG_CAPABILITIES";
  case SVGA_REG_CAP2: return "SVGA_REG_CAP2";
  case SVGA_REG_MEM_SIZE: return "SVGA_REG_MEM_SIZE";
  case SVGA_REG_MEM_REGS: return "SVGA_REG_MEM_REGS";
  case SVGA_REG_MEMORY_SIZE: return "SVGA_REG_MEMORY_SIZE";
  case SVGA_REG_FIFO_CAPS: return "SVGA_REG_FIFO_CAPS";
  case SVGA_REG_MOB_MAX_SIZE: return "SVGA_REG_MOB_MAX_SIZE";
  case SVGA_REG_GBOBJECT_MEM_SIZE_KB: return "SVGA_REG_GBOBJECT_MEM_SIZE_KB";
  case SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB:
    return "SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB";
  case SVGA_REG_GMRS_MAX_PAGES: return "SVGA_REG_GMRS_MAX_PAGES";
  case SVGA_REG_GMR_MAX_IDS: return "SVGA_REG_GMR_MAX_IDS";
  case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:
    return "SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH";
  case SVGA_REG_SCREENDMA: return "SVGA_REG_SCREENDMA";
  default: return NULL;
  };
};

static inline void vmsvga_trace_key_reg_read(struct vmsvga_state_s *s,
                                              uint32_t reg, uint32_t value) {
  const char *name;
  uint32_t word;
  uint64_t bit;

  if (!vmsvga_trace_flight_enabled() || reg >= 128) {
    return;
  };
  name = vmsvga_trace_reg_name(reg);
  if (name == NULL) {
    return;
  };
  word = reg >> 6;
  bit = UINT64_C(1) << (reg & 63);
  if (s->trace_key_reg_seen[word] & bit) {
    return;
  };
  s->trace_key_reg_seen[word] |= bit;
  if (reg == SVGA_REG_CAPABILITIES) {
    fprintf(stderr,
            "VMVGA-CAP capabilities=0x%08x 3d=%u gmr=%u extfifo=%u "
            "irqmask=%u traces=%u\n",
            value, !!(value & SVGA_CAP_3D), !!(value & SVGA_CAP_GMR),
            !!(value & SVGA_CAP_EXTENDED_FIFO), !!(value & SVGA_CAP_IRQMASK),
            !!(value & SVGA_CAP_TRACES));
  };
  fprintf(stderr, "VMVGA-REG-FIRST read name=%s reg=%u value=0x%08x\n",
          name, reg, value);
};

static inline void vmsvga_trace_key_reg_write(struct vmsvga_state_s *s,
                                               uint32_t reg, uint32_t value) {
  const char *name = NULL;

  if (!vmsvga_trace_flight_enabled()) {
    return;
  };
  switch (reg) {
  case SVGA_REG_ENABLE: name = "SVGA_REG_ENABLE"; break;
  case SVGA_REG_CONFIG_DONE: name = "SVGA_REG_CONFIG_DONE"; break;
  case SVGA_REG_TRACES: name = "SVGA_REG_TRACES"; break;
  case SVGA_REG_GUEST_ID: name = "SVGA_REG_GUEST_ID"; break;
  default: break;
  };
  if (name != NULL) {
    fprintf(stderr, "VMVGA-REG-WRITE name=%s reg=%u value=0x%08x\n",
            name, reg, value);
  };
};

static inline void vmsvga_trace_devcap(struct vmsvga_state_s *s,
                                        uint32_t index, uint32_t value) {
  uint32_t i;

  if (!vmsvga_trace_flight_enabled()) {
    return;
  };
  for (i = 0; i < s->trace_devcap_count; i++) {
    if (s->trace_devcap[i].index == index) {
      if (s->trace_devcap[i].value != value) {
        fprintf(stderr,
                "VMVGA-DEVCAP-CHANGE index=%u old=0x%08x new=0x%08x\n",
                index, s->trace_devcap[i].value, value);
        s->trace_devcap[i].value = value;
      };
      return;
    };
  };
  if (s->trace_devcap_count < VMVGA_TRACE_DEVCAP_MAX) {
    s->trace_devcap[s->trace_devcap_count].index = index;
    s->trace_devcap[s->trace_devcap_count].value = value;
    s->trace_devcap_count++;
  };
  fprintf(stderr, "VMVGA-DEVCAP-FIRST index=%u value=0x%08x\n",
          index, value);
};

static inline void vmsvga_trace_fifo_record(struct vmsvga_state_s *s,
                                             uint32_t cmd, bool is_3d,
                                             bool supported_3d) {
  struct vmsvga_trace_cmd_counter_s *entry = NULL;
  const char *name;
  uint32_t i;

  if (!vmsvga_trace_flight_enabled()) {
    return;
  };
  s->trace_now.fifo_cmds++;
  if (is_3d) {
    s->trace_now.fifo_3d_cmds++;
  } else {
    s->trace_now.fifo_2d_cmds++;
  };
  switch (cmd) {
  case SVGA_CMD_UPDATE:
  case SVGA_CMD_UPDATE_VERBOSE:
    s->trace_now.fifo_updates++;
    break;
  case SVGA_CMD_SURFACE_FILL:
  case SVGA_CMD_SURFACE_COPY:
  case SVGA_CMD_SURFACE_ALPHA_BLEND:
    s->trace_now.fifo_surface_2d++;
    break;
  case SVGA_CMD_DEFINE_SCREEN:
  case SVGA_CMD_DESTROY_SCREEN:
  case SVGA_CMD_DEFINE_GMRFB:
  case SVGA_CMD_BLIT_GMRFB_TO_SCREEN:
  case SVGA_CMD_BLIT_SCREEN_TO_GMRFB:
  case SVGA_CMD_ANNOTATION_FILL:
  case SVGA_CMD_ANNOTATION_COPY:
    s->trace_now.fifo_screen_blits++;
    break;
  case SVGA_CMD_DEFINE_GMR2:
  case SVGA_CMD_REMAP_GMR2:
    s->trace_now.fifo_gmr++;
    break;
  case SVGA_CMD_FENCE:
    s->trace_now.fifo_fences++;
    break;
  default:
    break;
  };
  vmsvga_trace_flight_activity(s);

  for (i = 0; i < s->trace_cmd_count; i++) {
    if (s->trace_cmd[i].cmd == cmd) {
      entry = &s->trace_cmd[i];
      break;
    };
  };
  if (entry != NULL) {
    entry->count++;
    return;
  };
  if (s->trace_cmd_count < VMVGA_TRACE_CMD_MAX) {
    entry = &s->trace_cmd[s->trace_cmd_count++];
    entry->cmd = cmd;
    entry->count = 1;
  };
  name = vmsvga_trace_fifo_cmd_name(cmd);
  fprintf(stderr,
          "VMVGA-FIFO-FIRST cmd=%u name=%s class=%s supported3d=%u "
          "stop=0x%08x next=0x%08x sync=%u\n",
          cmd, name != NULL ? name : (is_3d ? "SVGA3D" : "UNKNOWN"),
          is_3d ? "3D" : "2D", supported_3d, s->fifo_stop,
          s->fifo_next, s->sync);
};

static inline void vmsvga_trace_fifo_stall(struct vmsvga_state_s *s,
                                            uint32_t stage, uint32_t cmd,
                                            uint32_t fifo_start, int32_t len,
                                            bool is_3d,
                                            bool supported_3d) {
  bool changed;
  const char *name;

  if (!vmsvga_trace_flight_enabled()) {
    return;
  };
  s->trace_now.fifo_stalls++;
  if (stage == 1) {
    s->trace_now.fifo_prefilter_stalls++;
  };
  vmsvga_trace_flight_activity(s);
  changed = !s->trace_fifo_have_stall ||
            s->trace_fifo_last_stall_cmd != cmd ||
            s->trace_fifo_last_stall_stop != fifo_start ||
            s->trace_fifo_last_stall_stage != stage;
  if (changed) {
    s->trace_fifo_have_stall = true;
    s->trace_fifo_last_stall_cmd = cmd;
    s->trace_fifo_last_stall_stop = fifo_start;
    s->trace_fifo_last_stall_stage = stage;
    s->trace_fifo_stall_repeat = 1;
  } else {
    s->trace_fifo_stall_repeat++;
  };
  if (!changed &&
      (s->trace_fifo_stall_repeat & (s->trace_fifo_stall_repeat - 1)) != 0) {
    return;
  };
  name = vmsvga_trace_fifo_cmd_name(cmd);
  fprintf(stderr,
          "VMVGA-FIFO-STALL stage=%s cmd=%u name=%s class=%s supported3d=%u "
          "stop=0x%08x next=0x%08x words=%d sync=%u repeat=%" PRIu64
          " total=%" PRIu64 " sync_writes=%" PRIu64 " busy_reads=%" PRIu64
          "\n",
          stage == 1 ? "prefilter" : "handler", cmd,
          name != NULL ? name : (is_3d ? "SVGA3D" : "UNKNOWN"),
          is_3d ? "3D" : "2D", supported_3d, fifo_start, s->fifo_next,
          len, s->sync, s->trace_fifo_stall_repeat,
          s->trace_now.fifo_stalls, s->trace_now.sync_writes,
          s->trace_now.busy_reads);
};

static inline void vmsvga_trace_flight_report(struct vmsvga_state_s *s,
                                               const char *reason,
                                               bool force) {
  struct vmsvga_trace_counters_s *n;
  struct vmsvga_trace_counters_s *l;
  int64_t now;
  uint32_t fifo_min = 0;
  uint32_t fifo_max = 0;
  uint32_t stop = 0;
  uint32_t next = 0;
  uint32_t fifo_caps = 0;
  uint32_t fifo_busy = 0;
  bool pending = false;

  if (!vmsvga_trace_flight_enabled()) {
    return;
  };
  now = g_get_monotonic_time();
  if (!force) {
    if (s->trace_activity_seq == s->trace_last_report_seq) {
      return;
    };
    if (s->trace_last_report_us != 0 &&
        now - s->trace_last_report_us < VMVGA_TRACE_REPORT_INTERVAL_US) {
      return;
    };
  };
  if (s->fifo != NULL && s->config) {
    fifo_min = le32_to_cpu(s->fifo[SVGA_FIFO_MIN]);
    fifo_max = le32_to_cpu(s->fifo[SVGA_FIFO_MAX]);
    stop = le32_to_cpu(s->fifo[SVGA_FIFO_STOP]);
    next = le32_to_cpu(s->fifo[SVGA_FIFO_NEXT_CMD]);
    pending = stop != next;
    if (fifo_min >= (uint32_t)(SVGA_FIFO_CAPABILITIES + 1) * sizeof(uint32_t)) {
      fifo_caps = le32_to_cpu(s->fifo[SVGA_FIFO_CAPABILITIES]);
    };
    if (fifo_min >= (uint32_t)(SVGA_FIFO_BUSY + 1) * sizeof(uint32_t)) {
      fifo_busy = le32_to_cpu(s->fifo[SVGA_FIFO_BUSY]);
    };
  };
  n = &s->trace_now;
  l = &s->trace_last;
  fprintf(stderr,
          "VMVGA-FLIGHT reason=%s fifo=%" PRIu64 "(+%" PRIu64 ") "
          "2d=%" PRIu64 "(+%" PRIu64 ") 3d=%" PRIu64 "(+%" PRIu64 ") "
          "update=%" PRIu64 "(+%" PRIu64 ") surface2d=%" PRIu64
          "(+%" PRIu64 ") screenblit=%" PRIu64 "(+%" PRIu64 ") "
          "gmr=%" PRIu64 "(+%" PRIu64 ") fence=%" PRIu64 "(+%" PRIu64
          ") sync=%" PRIu64 "(+%" PRIu64 ") busyread=%" PRIu64
          "(+%" PRIu64 ") stalls=%" PRIu64 "(+%" PRIu64 ") prefilter=%"
          PRIu64 "(+%" PRIu64 ") dirtypages=%" PRIu64 "(+%" PRIu64
          ") damage=%" PRIu64 "(+%" PRIu64 ") pending=%u fifo_busy=%u "
          "min=0x%08x max=0x%08x stop=0x%08x next=0x%08x "
          "fifo_caps=0x%08x sync_state=%u\n",
          reason, n->fifo_cmds, n->fifo_cmds - l->fifo_cmds,
          n->fifo_2d_cmds, n->fifo_2d_cmds - l->fifo_2d_cmds,
          n->fifo_3d_cmds, n->fifo_3d_cmds - l->fifo_3d_cmds,
          n->fifo_updates, n->fifo_updates - l->fifo_updates,
          n->fifo_surface_2d, n->fifo_surface_2d - l->fifo_surface_2d,
          n->fifo_screen_blits, n->fifo_screen_blits - l->fifo_screen_blits,
          n->fifo_gmr, n->fifo_gmr - l->fifo_gmr,
          n->fifo_fences, n->fifo_fences - l->fifo_fences,
          n->sync_writes, n->sync_writes - l->sync_writes,
          n->busy_reads, n->busy_reads - l->busy_reads,
          n->fifo_stalls, n->fifo_stalls - l->fifo_stalls,
          n->fifo_prefilter_stalls,
          n->fifo_prefilter_stalls - l->fifo_prefilter_stalls,
          n->dirty_pages, n->dirty_pages - l->dirty_pages,
          n->damage_rects, n->damage_rects - l->damage_rects,
          pending, fifo_busy, fifo_min, fifo_max, stop, next, fifo_caps,
          s->sync);
  fprintf(stderr,
          "VMVGA-SYNC-STATS fifo_busy_before_sync: zero=%" PRIu64
          " one=%" PRIu64 " unavailable=%" PRIu64 "\n",
          n->sync_fifo_busy_zero, n->sync_fifo_busy_one,
          n->sync_writes - n->sync_fifo_busy_zero - n->sync_fifo_busy_one);
  if (n->screen_defines || n->screen_destroys || n->gmrfb_defines ||
      n->gmrfb_to_screen || n->screen_to_gmrfb || n->annotation_fills ||
      n->annotation_copies || n->surface_to_screen) {
    fprintf(stderr,
            "VMVGA-SCREEN-STATS define=%" PRIu64 " destroy=%" PRIu64
            " gmrfb=%" PRIu64 " g2s=%" PRIu64 " s2g=%" PRIu64
            " annotation_fill=%" PRIu64 " annotation_copy=%" PRIu64
            " surface_to_screen=%" PRIu64 "\n",
            n->screen_defines, n->screen_destroys, n->gmrfb_defines,
            n->gmrfb_to_screen, n->screen_to_gmrfb, n->annotation_fills,
            n->annotation_copies, n->surface_to_screen);
  };
  s->trace_last = s->trace_now;
  s->trace_last_report_seq = s->trace_activity_seq;
  s->trace_last_report_us = now;
};

static inline void vmsvga_trace_flight_histogram(struct vmsvga_state_s *s,
                                                  const char *reason) {
  uint32_t i;

  if (!vmsvga_trace_flight_enabled()) {
    return;
  };
  vmsvga_trace_flight_report(s, reason, true);
  for (i = 0; i < s->trace_cmd_count; i++) {
    const char *name = vmsvga_trace_fifo_cmd_name(s->trace_cmd[i].cmd);
    fprintf(stderr, "VMVGA-FIFO-COUNT cmd=%u name=%s count=%" PRIu64 "\n",
            s->trace_cmd[i].cmd, name != NULL ? name : "SVGA3D/UNKNOWN",
            s->trace_cmd[i].count);
  };
};

/*
 * VMware SVGA keeps legacy VGA framebuffer accesses separate while SVGA is
 * enabled.  Legacy VGA state can therefore be saved/restored or updated
 * concurrently without corrupting the active SVGA framebuffer.
 */
static inline void
vmsvga_scan_vram_dirty(struct vmsvga_state_s *s,
                        const struct vmsvga_damage_rect_s *explicit_damage,
                        uint32_t explicit_count);

static inline size_t vmsvga_legacy_vga_backup_size(
    const struct vmsvga_state_s *s) {
  return MIN((size_t)VMSVGA_VGA_FB_BACKUP_SIZE,
             (size_t)s->vga.vram_size);
};

static inline size_t
vmsvga_first_svga_clear_size(struct vmsvga_state_s *s) {
  DisplaySurface *surface = qemu_console_surface(s->vga.con);
  size_t clear_size = vmsvga_legacy_vga_backup_size(s);

  /*
   * VirtualBox clears at least the legacy 512 KiB aperture on the first
   * VGA -> SVGA ownership transition.  Also discard the complete inherited
   * firmware framebuffer, using the frontend's real stride and height rather
   * than assuming a particular resolution such as 1024x768.
   */
  if (surface != NULL && surface_stride(surface) > 0 &&
      surface_height(surface) > 0) {
    uint64_t inherited_size =
        (uint64_t)surface_stride(surface) * surface_height(surface);
    clear_size = (size_t)MIN(inherited_size, (uint64_t)s->vga.vram_size);
    clear_size = MAX(clear_size, vmsvga_legacy_vga_backup_size(s));
  };

  return MIN(clear_size, (size_t)s->vga.vram_size);
};

static inline uint8_t *vmsvga_svga_vram_ptr(struct vmsvga_state_s *s) {
  return memory_region_get_ram_ptr(&s->vga.vram);
};

static void vmsvga_legacy_vga_enter(struct vmsvga_state_s *s) {
  size_t backup_size = vmsvga_legacy_vga_backup_size(s);
  size_t clear_size = vmsvga_first_svga_clear_size(s);
  uint8_t *svga_ptr = vmsvga_svga_vram_ptr(s);

  /*
   * Capture the legacy VGA framebuffer only on the first VGA -> SVGA handoff.
   * After that it remains independent while BAR1 continues to hold SVGA data.
   */
  if (s->legacy_vga_size == 0) {
    memcpy(s->legacy_vga_ptr, svga_ptr, backup_size);
    /*
     * Clear the inherited firmware framebuffer after preserving the legacy
     * VGA aperture.  The extent comes from the currently visible frontend
     * surface, so the handoff is independent of the firmware resolution.
     */
    memset(svga_ptr, 0, clear_size);
    s->legacy_vga_size = (uint32_t)backup_size;
  };
  s->vga.vram_ptr = svga_ptr;
  s->svga_surface_bound = false;
  VMVGA_TRACE_LOCAL(VMVGA_TRACE_STATE,
                     "VGA_SHADOW enter backup=%zu clear=%zu",
                     backup_size, clear_size);
};

static void vmsvga_legacy_vga_leave(struct vmsvga_state_s *s) {
  size_t size = vmsvga_legacy_vga_backup_size(s);

  /*
   * Do not copy the VGA shadow over BAR1. The two framebuffer contents are
   * independent; only select the shadow as VGACommonState's backing store.
   */
  if (s->legacy_vga_size != 0) {
    s->vga.vram_ptr = s->legacy_vga_ptr;
  } else {
    s->vga.vram_ptr = vmsvga_svga_vram_ptr(s);
  };
  s->svga_surface_bound = false;
  VMVGA_TRACE_LOCAL(VMVGA_TRACE_STATE, "VGA_SHADOW leave size=%zu", size);
};

static inline void vmsvga_clear_vram_dirty(struct vmsvga_state_s *s) {
  DirtyBitmapSnapshot *snap;
  if (!s->dirty_log_enabled || s->vga.vram_size == 0) {
    return;
  };
  snap = memory_region_snapshot_and_clear_dirty(
      &s->vga.vram, 0, s->vga.vram_size, DIRTY_MEMORY_VGA);
  g_free(snap);
};

static inline void vmsvga_set_dirty_log(struct vmsvga_state_s *s,
                                        bool enabled) {
  if (s->dirty_log_enabled == enabled) {
    return;
  };
  if (enabled) {
    vga_dirty_log_start(&s->vga);
    s->dirty_log_enabled = true;
    /* Start a new tracing interval from a clean baseline. */
    vmsvga_clear_vram_dirty(s);
  } else {
    struct vmsvga_damage_rect_s explicit_damage[VMSVGA_DAMAGE_RECTS];
    uint32_t explicit_count = s->damage_count;
    /* Preserve only writes which happened while tracing was enabled.  Do not
     * use a deferred full redraw here: that could expose later BAR1 writes
     * which occur after TRACES has already been disabled. */
    if (explicit_count != 0) {
      memcpy(explicit_damage, s->damage,
             explicit_count * sizeof(explicit_damage[0]));
    };
    if (s->active_valid) {
      vmsvga_scan_vram_dirty(s, explicit_damage, explicit_count);
    } else {
      vmsvga_clear_vram_dirty(s);
    };
    vga_dirty_log_stop(&s->vga);
    s->dirty_log_enabled = false;
  };
};

static inline bool vmsvga_effective_traces(const struct vmsvga_state_s *s) {
  return !s->enable || !s->config || !!s->traces;
};

static inline void vmsvga_update_dirty_log(struct vmsvga_state_s *s) {
  vmsvga_set_dirty_log(s, vmsvga_effective_traces(s));
};

static uint64_t vmsvga_legacy_vga_read(void *opaque, hwaddr addr,
                                       unsigned size) {
  struct vmsvga_state_s *s = opaque;
  uint8_t *vram_ptr = s->vga.vram_ptr;
  uint32_t value;

  (void)size;
  if (s->enable) {
    s->vga.vram_ptr = s->legacy_vga_ptr;
  };
  value = vga_mem_readb(&s->vga, addr);
  s->vga.vram_ptr = vram_ptr;
  return value;
};

static void vmsvga_legacy_vga_write(void *opaque, hwaddr addr, uint64_t data,
                                    unsigned size) {
  struct vmsvga_state_s *s = opaque;
  uint8_t *vram_ptr = s->vga.vram_ptr;

  (void)size;
  if (s->enable) {
    s->vga.vram_ptr = s->legacy_vga_ptr;
  };
  vga_mem_writeb(&s->vga, addr, data);
  s->vga.vram_ptr = vram_ptr;
};

static const MemoryRegionOps vmsvga_legacy_vga_ops = {
    .read = vmsvga_legacy_vga_read,
    .write = vmsvga_legacy_vga_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl =
        {
            .min_access_size = 1,
            .max_access_size = 1,
        },
};
static inline bool vmsvga_fifo_has_reg(struct vmsvga_state_s *s,
                                       uint32_t reg);
static inline void vmsvga_cursor_select(struct vmsvga_state_s *s,
                                        uint32_t id);
static inline void vmsvga_cursor_apply(struct vmsvga_state_s *s) {
  if (!s->cursor_dirty) {
    return;
  };
  if (s->enable && !s->hidden &&
      s->active_cursor_on != SVGA_CURSOR_ON_HIDE) {
    vmvga_console_mouse_set(s->vga.con, s->active_cursor_x,
                            s->active_cursor_y, SVGA_CURSOR_ON_SHOW);
  } else {
    vmvga_console_mouse_set(s->vga.con, s->active_cursor_x,
                            s->active_cursor_y, SVGA_CURSOR_ON_HIDE);
  };
  s->cursor_dirty = false;
};
static inline bool vmsvga_cursor_bypass3_fetch(struct vmsvga_state_s *s,
                                                uint32_t *generation) {
  uint32_t count;
  uint32_t last_updated;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t on = 0;
  uint32_t i;
  if (!s->enable || !s->config || s->fifo == NULL ||
      !(s->fc & SVGA_FIFO_CAP_CURSOR_BYPASS_3) ||
      !vmsvga_fifo_has_reg(s, SVGA_FIFO_CURSOR_LAST_UPDATED)) {
    return false;
  };
  count = le32_to_cpu(s->fifo[SVGA_FIFO_CURSOR_COUNT]);
  last_updated = le32_to_cpu(s->fifo[SVGA_FIFO_CURSOR_LAST_UPDATED]);
  if (count == last_updated) {
    return false;
  };
  /*
   * Cursor Bypass 3 is intentionally lock-free shared state. Read X/Y/ON
   * against one generation and retry a few times if the guest changes the
   * generation while we are sampling, matching VMware's generation protocol
   * and VirtualBox's bounded retry strategy. If all samples race, leave the
   * generation unacknowledged and retry on the next refresh.
   */
  for (i = 0; i < 4; i++) {
    uint32_t after;
    x = le32_to_cpu(s->fifo[SVGA_FIFO_CURSOR_X]);
    y = le32_to_cpu(s->fifo[SVGA_FIFO_CURSOR_Y]);
    on = le32_to_cpu(s->fifo[SVGA_FIFO_CURSOR_ON]);
    after = le32_to_cpu(s->fifo[SVGA_FIFO_CURSOR_COUNT]);
    if (after == count) {
      break;
    };
    count = after;
  };
  if (i == 4) {
    return false;
  };
  /* Bypass 3 replaces the indexed position/visibility commit with a FIFO
   * generation. There is no FIFO cursor-ID register, so consume the current
   * pending indexed ID together with that generation. */
  if (s->active_cursor != s->cursor) {
    s->active_cursor = s->cursor;
    vmsvga_cursor_select(s, s->active_cursor);
    s->cursor_dirty = true;
  };
  if (s->active_cursor_x != x || s->active_cursor_y != y ||
      s->active_cursor_on !=
          (on ? SVGA_CURSOR_ON_SHOW : SVGA_CURSOR_ON_HIDE)) {
    s->active_cursor_x = x;
    s->active_cursor_y = y;
    s->active_cursor_on = on ? SVGA_CURSOR_ON_SHOW : SVGA_CURSOR_ON_HIDE;
    s->cursor_dirty = true;
  };
  *generation = count;
  return true;
};
static void cursor_update_from_fifo(struct vmsvga_state_s *s) {
  uint32_t bypass3_generation = 0;
  bool bypass3_update;
  VPRINT("cursor_update_from_fifo was just executed\n");
  bypass3_update = vmsvga_cursor_bypass3_fetch(s, &bypass3_generation);
  vmsvga_cursor_apply(s);
  if (bypass3_update) {
    /* Acknowledge only after the sampled cursor state has been consumed. */
    s->fifo[SVGA_FIFO_CURSOR_LAST_UPDATED] =
        cpu_to_le32(bypass3_generation);
  };
};
static inline void vmsvga_damage_flush(struct vmsvga_state_s *s) {
  uint32_t i;
  if (vmsvga_trace_flight_enabled() && s->damage_count != 0) {
    s->trace_now.damage_rects += s->damage_count;
    vmsvga_trace_flight_activity(s);
  };
  for (i = 0; i < s->damage_count; i++) {
    struct vmsvga_damage_rect_s *rect = &s->damage[i];
    VMVGA_TRACE_LOCAL(VMVGA_TRACE_DRAW,
                       "DAMAGE x=%u y=%u w=%u h=%u",
                       rect->x, rect->y, rect->w, rect->h);
    vmvga_console_update(s->vga.con, rect->x, rect->y, rect->w, rect->h);
  };
  s->damage_count = 0;
};

static inline bool vmsvga_damage_merge_is_efficient(
    const struct vmsvga_damage_rect_s *rect,
    const struct vmsvga_damage_rect_s *old, uint64_t merged_width,
    uint64_t merged_height) {
  uint64_t rect_area = (uint64_t)rect->w * rect->h;
  uint64_t old_area = (uint64_t)old->w * old->h;
  uint64_t combined_area = rect_area > UINT64_MAX - old_area
                               ? UINT64_MAX
                               : rect_area + old_area;
  uint64_t merged_area;

  if (merged_width != 0 && merged_height > UINT64_MAX / merged_width) {
    return false;
  };
  merged_area = merged_width * merged_height;

  /*
   * Do not turn sparse shapes such as a four-sided moving-window outline
   * into one solid window-sized display update.  Merging remains worthwhile
   * when its bounding box is no larger than the two original rectangles.
   */
  return merged_area <= combined_area;
};

static inline void vmsvga_damage_add(struct vmsvga_state_s *s, uint32_t x,
                                     uint32_t y, uint32_t w, uint32_t h) {
  struct vmsvga_damage_rect_s rect;
  uint32_t i;
  if (s->invalidated || w == 0 || h == 0) {
    return;
  };
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
  for (i = 0; i < s->damage_count;) {
    struct vmsvga_damage_rect_s *old = &s->damage[i];
    uint64_t rect_right = (uint64_t)rect.x + rect.w;
    uint64_t rect_bottom = (uint64_t)rect.y + rect.h;
    uint64_t old_right = (uint64_t)old->x + old->w;
    uint64_t old_bottom = (uint64_t)old->y + old->h;
    bool x_overlap = (uint64_t)rect.x < old_right &&
                     (uint64_t)old->x < rect_right;
    bool y_overlap = (uint64_t)rect.y < old_bottom &&
                     (uint64_t)old->y < rect_bottom;
    bool x_close = (uint64_t)rect.x <= old_right &&
                   (uint64_t)old->x <= rect_right;
    bool y_close = (uint64_t)rect.y <= old_bottom &&
                   (uint64_t)old->y <= rect_bottom;
    uint32_t left = MIN(rect.x, old->x);
    uint32_t top = MIN(rect.y, old->y);
    uint64_t right = MAX(rect_right, old_right);
    uint64_t bottom = MAX(rect_bottom, old_bottom);
    if (((x_overlap && y_close) || (y_overlap && x_close)) &&
        vmsvga_damage_merge_is_efficient(&rect, old, right - left,
                                         bottom - top)) {
      rect.x = left;
      rect.y = top;
      rect.w = (uint32_t)(right - left);
      rect.h = (uint32_t)(bottom - top);
      s->damage_count--;
      s->damage[i] = s->damage[s->damage_count];
      continue;
    };
    i++;
  };
  if (s->damage_count == VMSVGA_DAMAGE_RECTS) {
    vmsvga_damage_flush(s);
  };
  s->damage[s->damage_count++] = rect;
};

static inline uint32_t vmsvga_bytes_per_pixel(uint32_t bpp);
static inline uint32_t vmsvga_stride(struct vmsvga_state_s *s);
static inline uint32_t vmsvga_active_width(const struct vmsvga_state_s *s) {
  return s->active_valid ? s->active_width : 0;
};
static inline uint32_t vmsvga_active_height(const struct vmsvga_state_s *s) {
  return s->active_valid ? s->active_height : 0;
};
static inline uint32_t vmsvga_active_depth(const struct vmsvga_state_s *s) {
  return s->active_valid ? s->active_depth : 0;
};

static inline bool vmsvga_verify_rect(struct vmsvga_state_s *s, uint32_t x,
                                      uint32_t y, uint32_t w, uint32_t h) {
  uint32_t surface_width_px = vmsvga_active_width(s);
  uint32_t surface_height_px = vmsvga_active_height(s);
  if (x > 8192 || w > 8192 || x > surface_width_px ||
      w > surface_width_px - x) {
    return false;
  };
  if (y > 8192 || h > 8192 || y > surface_height_px ||
      h > surface_height_px - y) {
    return false;
  };
  return true;
};
static inline bool vmsvga_verify_vram_rect(struct vmsvga_state_s *s,
                                           uint32_t x, uint32_t y, uint32_t w,
                                           uint32_t h) {
  uint32_t bypl = vmsvga_stride(s);
  uint32_t bypp = vmsvga_bytes_per_pixel(vmsvga_active_depth(s));
  uint64_t surface_width_px = vmsvga_active_width(s);
  uint64_t surface_height_px = vmsvga_active_height(s);
  uint64_t x_bytes;
  uint64_t width_bytes;
  uint64_t rows;
  if (bypl == 0 || bypp < 1 || bypp > 4) {
    return false;
  };
  x_bytes = (uint64_t)x * bypp;
  width_bytes = (uint64_t)w * bypp;
  if (x_bytes > bypl || width_bytes > (uint64_t)bypl - x_bytes) {
    return false;
  };
  if (h != 0 && (uint64_t)y < surface_height_px &&
      ((uint64_t)x > surface_width_px ||
       (uint64_t)w > surface_width_px - x)) {
    return false;
  };
  rows = (uint64_t)s->vga.vram_size / bypl;
  if ((uint64_t)y > rows || (uint64_t)h > rows - y) {
    return false;
  };
  return true;
};
static inline void vmsvga_damage_add_visible(struct vmsvga_state_s *s,
                                             uint32_t x, uint32_t y,
                                             uint32_t w, uint32_t h) {
  uint64_t surface_width_px = vmsvga_active_width(s);
  uint64_t surface_height_px = vmsvga_active_height(s);
  uint64_t right;
  uint64_t bottom;
  if ((uint64_t)x >= surface_width_px || (uint64_t)y >= surface_height_px ||
      w == 0 || h == 0) {
    return;
  };
  right = MIN((uint64_t)x + w, surface_width_px);
  bottom = MIN((uint64_t)y + h, surface_height_px);
  vmsvga_damage_add(s, x, y, (uint32_t)(right - x),
                    (uint32_t)(bottom - y));
};
static inline void vmsvga_mark_vram_dirty_range(
    struct vmsvga_state_s *s, uint64_t offset, uint64_t size) {
  uint64_t vram_size = s->vga.vram_size;
  if (size == 0 || offset >= vram_size) {
    return;
  };
  size = MIN(size, vram_size - offset);
  memory_region_set_dirty(&s->vga.vram, (hwaddr)offset, (hwaddr)size);
};
static inline void vmsvga_mark_vram_dirty_rect(
    struct vmsvga_state_s *s, uint64_t base_offset, uint32_t pitch,
    uint32_t bypp, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
  uint64_t row_offset;
  uint64_t x_offset;
  uint64_t start;
  uint64_t width_bytes;
  uint64_t tail;
  uint64_t span;
  if (pitch == 0 || bypp == 0 || w == 0 || h == 0) {
    return;
  };
  row_offset = (uint64_t)pitch * y;
  x_offset = (uint64_t)bypp * x;
  width_bytes = (uint64_t)bypp * w;
  tail = (uint64_t)pitch * (h - 1);
  if (base_offset > UINT64_MAX - row_offset ||
      base_offset + row_offset > UINT64_MAX - x_offset ||
      tail > UINT64_MAX - width_bytes) {
    return;
  };
  start = base_offset + row_offset + x_offset;
  span = tail + width_bytes;
  if (start >= s->vga.vram_size || span > s->vga.vram_size - start) {
    return;
  };
  vmsvga_mark_vram_dirty_range(s, start, span);
};
static inline void vmsvga_mark_active_rect_dirty(
    struct vmsvga_state_s *s, uint32_t x, uint32_t y, uint32_t w,
    uint32_t h) {
  vmsvga_mark_vram_dirty_rect(
      s, 0, vmsvga_stride(s),
      vmsvga_bytes_per_pixel(vmsvga_active_depth(s)), x, y, w, h);
};

static inline void vmsvga_update_rect(struct vmsvga_state_s *s, uint32_t x,
                                      uint32_t y, uint32_t w, uint32_t h) {
  uint64_t right;
  uint64_t bottom;
  if (w == 0 || h == 0 || x >= vmsvga_active_width(s) ||
      y >= vmsvga_active_height(s)) {
    return;
  };
  right = MIN((uint64_t)x + w, (uint64_t)vmsvga_active_width(s));
  bottom = MIN((uint64_t)y + h, (uint64_t)vmsvga_active_height(s));
  vmsvga_damage_add(s, x, y, (uint32_t)(right - x),
                    (uint32_t)(bottom - y));
};
static inline bool vmsvga_copy_rect(struct vmsvga_state_s *s, uint32_t x0,
                                    uint32_t y0, uint32_t x1, uint32_t y1,
                                    uint32_t w, uint32_t h) {
  VPRINT("vmsvga_copy_rect was just executed\n");
  uint8_t *vram = s->vga.vram_ptr;
  uint32_t bypl = vmsvga_stride(s);
  uint32_t bypp = vmsvga_bytes_per_pixel(vmsvga_active_depth(s));
  size_t width = (size_t)bypp * w;
  uint32_t line = h;
  uint8_t *src;
  uint8_t *dst;
  if (!vmsvga_verify_vram_rect(s, x0, y0, w, h) ||
      !vmsvga_verify_vram_rect(s, x1, y1, w, h)) {
    return false;
  };
  if (w == 0 || h == 0) {
    return true;
  };
  if (x0 == 0 && x1 == 0 && width == bypl) {
    src = vram + (size_t)bypl * y0;
    dst = vram + (size_t)bypl * y1;
    memmove(dst, src, width * h);
    vmsvga_mark_active_rect_dirty(s, x1, y1, w, h);
    vmsvga_damage_add_visible(s, x1, y1, w, h);
    return true;
  };
  if (y1 > y0) {
    src = vram + (size_t)bypp * x0 + (size_t)bypl * (y0 + h - 1);
    dst = vram + (size_t)bypp * x1 + (size_t)bypl * (y1 + h - 1);
    for (; line > 0; line--, src -= bypl, dst -= bypl) {
      memmove(dst, src, width);
    };
  } else {
    src = vram + (size_t)bypp * x0 + (size_t)bypl * y0;
    dst = vram + (size_t)bypp * x1 + (size_t)bypl * y1;
    for (; line > 0; line--, src += bypl, dst += bypl) {
      memmove(dst, src, width);
    };
  };
  vmsvga_mark_active_rect_dirty(s, x1, y1, w, h);
  vmsvga_damage_add_visible(s, x1, y1, w, h);
  return true;
};
static inline void vmsvga_fill_pattern(uint8_t *dst, size_t length,
                                       const uint8_t *pattern,
                                       uint32_t pattern_size) {
  size_t filled;
  size_t chunk;
  if (length == 0 || pattern_size == 0) {
    return;
  };
  if (pattern_size == 1) {
    memset(dst, pattern[0], length);
    return;
  };
  filled = MIN(length, (size_t)pattern_size);
  memcpy(dst, pattern, filled);
  while (filled < length) {
    chunk = MIN(filled, length - filled);
    memcpy(dst + filled, dst, chunk);
    filled += chunk;
  };
};

static inline bool vmsvga_fill_rect(struct vmsvga_state_s *s, uint32_t c,
                                    uint32_t x, uint32_t y, uint32_t w,
                                    uint32_t h) {
  VPRINT("vmsvga_fill_rect was just executed\n");
  uint32_t bypl = vmsvga_stride(s);
  uint32_t bypp = vmsvga_bytes_per_pixel(vmsvga_active_depth(s));
  size_t width = (size_t)bypp * w;
  uint32_t line = h;
  uint8_t *first;
  uint8_t *dst;
  uint8_t col[4];
  if (!vmsvga_verify_vram_rect(s, x, y, w, h) || bypp < 1 ||
      bypp > 4) {
    return false;
  };
  if (w == 0 || h == 0) {
    return true;
  };
  col[0] = c;
  col[1] = c >> 8;
  col[2] = c >> 16;
  col[3] = c >> 24;
  first = s->vga.vram_ptr + (size_t)bypp * x + (size_t)bypl * y;
  if (x == 0 && width == bypl) {
    vmsvga_fill_pattern(first, width * h, col, bypp);
    vmsvga_mark_active_rect_dirty(s, x, y, w, h);
    vmsvga_damage_add_visible(s, x, y, w, h);
    return true;
  };
  vmsvga_fill_pattern(first, width, col, bypp);
  dst = first;
  for (line = 1; line < h; line++) {
    dst += bypl;
    memcpy(dst, first, width);
  };
  vmsvga_mark_active_rect_dirty(s, x, y, w, h);
  vmsvga_damage_add_visible(s, x, y, w, h);
  return true;
};
static inline bool vmsvga_solid_rop_is_noop(uint32_t color, uint32_t bypp,
                                            uint32_t rop) {
  uint32_t mask;
  switch (bypp) {
  case 1:
    mask = UINT8_MAX;
    break;
  case 2:
    mask = UINT16_MAX;
    break;
  case 3:
    mask = 0x00ffffff;
    break;
  case 4:
    mask = UINT32_MAX;
    break;
  default:
    return false;
  };
  color &= mask;
  switch (rop) {
  case VMSVGA_ROP_AND:
  case VMSVGA_ROP_EQUIV:
  case VMSVGA_ROP_OR_INVERTED:
    return color == mask;
  case VMSVGA_ROP_AND_INVERTED:
  case VMSVGA_ROP_XOR:
  case VMSVGA_ROP_OR:
    return color == 0;
  case VMSVGA_ROP_NOOP:
    return true;
  default:
    return false;
  };
};
static inline void vmsvga_rop_invert_buffer(uint8_t *dst, size_t length) {
  size_t offset = 0;
  while (length - offset >= sizeof(uint64_t)) {
    uint64_t value;
    memcpy(&value, dst + offset, sizeof(value));
    value = ~value;
    memcpy(dst + offset, &value, sizeof(value));
    offset += sizeof(value);
  };
  while (offset < length) {
    dst[offset] = ~dst[offset];
    offset++;
  };
};
static inline void vmsvga_rop_copy_inverted_buffer(uint8_t *dst,
                                                   const uint8_t *src,
                                                   size_t length,
                                                   bool reverse) {
  size_t offset;
  if (reverse) {
    offset = length;
    while (offset >= sizeof(uint64_t)) {
      uint64_t src_value;
      uint64_t result;
      offset -= sizeof(uint64_t);
      memcpy(&src_value, src + offset, sizeof(src_value));
      result = ~src_value;
      memcpy(dst + offset, &result, sizeof(result));
    };
    while (offset > 0) {
      offset--;
      dst[offset] = ~src[offset];
    };
  } else {
    offset = 0;
    while (length - offset >= sizeof(uint64_t)) {
      uint64_t src_value;
      uint64_t result;
      memcpy(&src_value, src + offset, sizeof(src_value));
      result = ~src_value;
      memcpy(dst + offset, &result, sizeof(result));
      offset += sizeof(uint64_t);
    };
    while (offset < length) {
      dst[offset] = ~src[offset];
      offset++;
    };
  };
};
static inline void vmsvga_rop_buffer(uint8_t *dst, const uint8_t *src,
                                     size_t length, uint32_t rop,
                                     bool reverse) {
  size_t offset;
#define VMSVGA_ROP_BUFFER_LOOP(operation)                                      \
  do {                                                                         \
    if (reverse) {                                                             \
      offset = length;                                                         \
      while (offset >= sizeof(uint64_t)) {                                     \
        uint64_t src_value;                                                    \
        uint64_t dst_value;                                                    \
        uint64_t result;                                                       \
        offset -= sizeof(uint64_t);                                            \
        memcpy(&src_value, src + offset, sizeof(src_value));                   \
        memcpy(&dst_value, dst + offset, sizeof(dst_value));                   \
        (void)src_value;                                                       \
        (void)dst_value;                                                       \
        result = (operation);                                                  \
        memcpy(dst + offset, &result, sizeof(result));                         \
      };                                                                       \
      while (offset > 0) {                                                     \
        uint64_t src_value;                                                    \
        uint64_t dst_value;                                                    \
        uint8_t result;                                                        \
        offset--;                                                              \
        src_value = src[offset];                                               \
        dst_value = dst[offset];                                               \
        (void)src_value;                                                       \
        (void)dst_value;                                                       \
        result = (uint8_t)(operation);                                         \
        dst[offset] = result;                                                  \
      };                                                                       \
    } else {                                                                   \
      offset = 0;                                                              \
      while (length - offset >= sizeof(uint64_t)) {                            \
        uint64_t src_value;                                                    \
        uint64_t dst_value;                                                    \
        uint64_t result;                                                       \
        memcpy(&src_value, src + offset, sizeof(src_value));                   \
        memcpy(&dst_value, dst + offset, sizeof(dst_value));                   \
        (void)src_value;                                                       \
        (void)dst_value;                                                       \
        result = (operation);                                                  \
        memcpy(dst + offset, &result, sizeof(result));                         \
        offset += sizeof(uint64_t);                                            \
      };                                                                       \
      while (offset < length) {                                                \
        uint64_t src_value = src[offset];                                      \
        uint64_t dst_value = dst[offset];                                      \
        uint8_t result;                                                        \
        (void)src_value;                                                       \
        (void)dst_value;                                                       \
        result = (uint8_t)(operation);                                         \
        dst[offset] = result;                                                  \
        offset++;                                                              \
      };                                                                       \
    };                                                                         \
  } while (0)
  switch (rop) {
  case VMSVGA_ROP_CLEAR:
    VMSVGA_ROP_BUFFER_LOOP(0);
    break;
  case VMSVGA_ROP_AND:
    VMSVGA_ROP_BUFFER_LOOP(src_value & dst_value);
    break;
  case VMSVGA_ROP_AND_REVERSE:
    VMSVGA_ROP_BUFFER_LOOP(src_value & ~dst_value);
    break;
  case VMSVGA_ROP_COPY:
    VMSVGA_ROP_BUFFER_LOOP(src_value);
    break;
  case VMSVGA_ROP_AND_INVERTED:
    VMSVGA_ROP_BUFFER_LOOP(~src_value & dst_value);
    break;
  case VMSVGA_ROP_NOOP:
    break;
  case VMSVGA_ROP_XOR:
    VMSVGA_ROP_BUFFER_LOOP(src_value ^ dst_value);
    break;
  case VMSVGA_ROP_OR:
    VMSVGA_ROP_BUFFER_LOOP(src_value | dst_value);
    break;
  case VMSVGA_ROP_NOR:
    VMSVGA_ROP_BUFFER_LOOP(~src_value & ~dst_value);
    break;
  case VMSVGA_ROP_EQUIV:
    VMSVGA_ROP_BUFFER_LOOP(~src_value ^ dst_value);
    break;
  case VMSVGA_ROP_INVERT:
    VMSVGA_ROP_BUFFER_LOOP(~dst_value);
    break;
  case VMSVGA_ROP_OR_REVERSE:
    VMSVGA_ROP_BUFFER_LOOP(src_value | ~dst_value);
    break;
  case VMSVGA_ROP_COPY_INVERTED:
    VMSVGA_ROP_BUFFER_LOOP(~src_value);
    break;
  case VMSVGA_ROP_OR_INVERTED:
    VMSVGA_ROP_BUFFER_LOOP(~src_value | dst_value);
    break;
  case VMSVGA_ROP_NAND:
    VMSVGA_ROP_BUFFER_LOOP(~src_value | ~dst_value);
    break;
  case VMSVGA_ROP_SET:
    VMSVGA_ROP_BUFFER_LOOP(UINT64_MAX);
    break;
  };
#undef VMSVGA_ROP_BUFFER_LOOP
};
static inline void vmsvga_rop_fill_buffer(uint8_t *dst, size_t length,
                                          const uint8_t col[4], uint32_t bypp,
                                          uint32_t rop) {
  uint8_t pattern[24];
  size_t offset;
  size_t i;
  for (i = 0; i < sizeof(pattern); i++) {
    pattern[i] = col[i % bypp];
  };
#define VMSVGA_ROP_FILL_BUFFER_LOOP(operation)                                 \
  do {                                                                         \
    offset = 0;                                                                \
    while (length - offset >= sizeof(uint64_t)) {                              \
      uint64_t src_value;                                                      \
      uint64_t dst_value;                                                      \
      uint64_t result;                                                         \
      size_t pattern_offset = offset % sizeof(pattern);                        \
      memcpy(&src_value, pattern + pattern_offset, sizeof(src_value));         \
      memcpy(&dst_value, dst + offset, sizeof(dst_value));                     \
      (void)src_value;                                                         \
      (void)dst_value;                                                         \
      result = (operation);                                                    \
      memcpy(dst + offset, &result, sizeof(result));                           \
      offset += sizeof(uint64_t);                                              \
    };                                                                         \
    while (offset < length) {                                                  \
      uint64_t src_value = pattern[offset % sizeof(pattern)];                  \
      uint64_t dst_value = dst[offset];                                        \
      uint8_t result;                                                          \
      (void)src_value;                                                         \
      (void)dst_value;                                                         \
      result = (uint8_t)(operation);                                           \
      dst[offset] = result;                                                    \
      offset++;                                                                \
    };                                                                         \
  } while (0)
  switch (rop) {
  case VMSVGA_ROP_CLEAR:
    VMSVGA_ROP_FILL_BUFFER_LOOP(0);
    break;
  case VMSVGA_ROP_AND:
    VMSVGA_ROP_FILL_BUFFER_LOOP(src_value & dst_value);
    break;
  case VMSVGA_ROP_AND_REVERSE:
    VMSVGA_ROP_FILL_BUFFER_LOOP(src_value & ~dst_value);
    break;
  case VMSVGA_ROP_COPY:
    VMSVGA_ROP_FILL_BUFFER_LOOP(src_value);
    break;
  case VMSVGA_ROP_AND_INVERTED:
    VMSVGA_ROP_FILL_BUFFER_LOOP(~src_value & dst_value);
    break;
  case VMSVGA_ROP_NOOP:
    break;
  case VMSVGA_ROP_XOR:
    VMSVGA_ROP_FILL_BUFFER_LOOP(src_value ^ dst_value);
    break;
  case VMSVGA_ROP_OR:
    VMSVGA_ROP_FILL_BUFFER_LOOP(src_value | dst_value);
    break;
  case VMSVGA_ROP_NOR:
    VMSVGA_ROP_FILL_BUFFER_LOOP(~src_value & ~dst_value);
    break;
  case VMSVGA_ROP_EQUIV:
    VMSVGA_ROP_FILL_BUFFER_LOOP(~src_value ^ dst_value);
    break;
  case VMSVGA_ROP_INVERT:
    VMSVGA_ROP_FILL_BUFFER_LOOP(~dst_value);
    break;
  case VMSVGA_ROP_OR_REVERSE:
    VMSVGA_ROP_FILL_BUFFER_LOOP(src_value | ~dst_value);
    break;
  case VMSVGA_ROP_COPY_INVERTED:
    VMSVGA_ROP_FILL_BUFFER_LOOP(~src_value);
    break;
  case VMSVGA_ROP_OR_INVERTED:
    VMSVGA_ROP_FILL_BUFFER_LOOP(~src_value | dst_value);
    break;
  case VMSVGA_ROP_NAND:
    VMSVGA_ROP_FILL_BUFFER_LOOP(~src_value | ~dst_value);
    break;
  case VMSVGA_ROP_SET:
    VMSVGA_ROP_FILL_BUFFER_LOOP(UINT64_MAX);
    break;
  };
#undef VMSVGA_ROP_FILL_BUFFER_LOOP
};
static inline bool vmsvga_rop_fill_rect(struct vmsvga_state_s *s, uint32_t c,
                                        uint32_t x, uint32_t y, uint32_t w,
                                        uint32_t h, uint32_t rop) {
  VPRINT("vmsvga_rop_fill_rect was just executed\n");
  uint32_t bypl = vmsvga_stride(s);
  uint32_t bypp = vmsvga_bytes_per_pixel(vmsvga_active_depth(s));
  size_t width = (size_t)bypp * w;
  uint8_t col[4];
  uint32_t row;
  if (rop == VMSVGA_ROP_COPY) {
    return vmsvga_fill_rect(s, c, x, y, w, h);
  };
  if (!vmsvga_verify_vram_rect(s, x, y, w, h) || bypp < 1 ||
      bypp > 4 || rop > VMSVGA_ROP_SET) {
    return false;
  };
  if (w == 0 || h == 0 || rop == VMSVGA_ROP_NOOP) {
    return true;
  };
  if (vmsvga_solid_rop_is_noop(c, bypp, rop)) {
    return true;
  };
  if (rop == VMSVGA_ROP_CLEAR) {
    if (x == 0 && width == bypl) {
      memset(s->vga.vram_ptr + (size_t)bypl * y, 0, width * h);
      vmsvga_mark_active_rect_dirty(s, x, y, w, h);
      vmsvga_damage_add_visible(s, x, y, w, h);
      return true;
    };
    for (row = 0; row < h; row++) {
      uint8_t *dst = s->vga.vram_ptr + (size_t)bypp * x +
                     (size_t)bypl * (y + row);
      memset(dst, 0, width);
    };
    vmsvga_mark_active_rect_dirty(s, x, y, w, h);
    vmsvga_damage_add_visible(s, x, y, w, h);
    return true;
  };
  if (rop == VMSVGA_ROP_SET) {
    if (x == 0 && width == bypl) {
      memset(s->vga.vram_ptr + (size_t)bypl * y, 0xff, width * h);
      vmsvga_mark_active_rect_dirty(s, x, y, w, h);
      vmsvga_damage_add_visible(s, x, y, w, h);
      return true;
    };
    for (row = 0; row < h; row++) {
      uint8_t *dst = s->vga.vram_ptr + (size_t)bypp * x +
                     (size_t)bypl * (y + row);
      memset(dst, 0xff, width);
    };
    vmsvga_mark_active_rect_dirty(s, x, y, w, h);
    vmsvga_damage_add_visible(s, x, y, w, h);
    return true;
  };
  if (rop == VMSVGA_ROP_COPY_INVERTED) {
    return vmsvga_fill_rect(s, ~c, x, y, w, h);
  };
  col[0] = c;
  col[1] = c >> 8;
  col[2] = c >> 16;
  col[3] = c >> 24;
  if (x == 0 && width == bypl) {
    uint8_t *dst = s->vga.vram_ptr + (size_t)bypl * y;
    if (rop == VMSVGA_ROP_INVERT) {
      vmsvga_rop_invert_buffer(dst, width * h);
    } else {
      vmsvga_rop_fill_buffer(dst, width * h, col, bypp, rop);
    };
    vmsvga_mark_active_rect_dirty(s, x, y, w, h);
    vmsvga_damage_add_visible(s, x, y, w, h);
    return true;
  };
  for (row = 0; row < h; row++) {
    uint8_t *dst = s->vga.vram_ptr + (size_t)bypp * x +
                   (size_t)bypl * (y + row);
    if (rop == VMSVGA_ROP_INVERT) {
      vmsvga_rop_invert_buffer(dst, width);
    } else {
      vmsvga_rop_fill_buffer(dst, width, col, bypp, rop);
    };
  };
  vmsvga_mark_active_rect_dirty(s, x, y, w, h);
  vmsvga_damage_add_visible(s, x, y, w, h);
  return true;
};
static inline bool vmsvga_rop_copy_rect(struct vmsvga_state_s *s,
                                        uint32_t x0, uint32_t y0, uint32_t x1,
                                        uint32_t y1, uint32_t w, uint32_t h,
                                        uint32_t rop) {
  VPRINT("vmsvga_rop_copy_rect was just executed\n");
  uint8_t *vram = s->vga.vram_ptr;
  uint32_t bypl = vmsvga_stride(s);
  uint32_t bypp = vmsvga_bytes_per_pixel(vmsvga_active_depth(s));
  size_t width = (size_t)bypp * w;
  bool reverse_rows;
  bool reverse_columns;
  uint32_t row_count;
  if (rop == VMSVGA_ROP_COPY) {
    return vmsvga_copy_rect(s, x0, y0, x1, y1, w, h);
  };
  if (!vmsvga_verify_vram_rect(s, x0, y0, w, h) ||
      !vmsvga_verify_vram_rect(s, x1, y1, w, h) || bypp < 1 ||
      bypp > 4 || rop > VMSVGA_ROP_SET) {
    return false;
  };
  if (w == 0 || h == 0 || rop == VMSVGA_ROP_NOOP) {
    return true;
  };
  if (rop == VMSVGA_ROP_CLEAR || rop == VMSVGA_ROP_SET) {
    uint8_t value = rop == VMSVGA_ROP_CLEAR ? 0 : 0xff;
    if (x1 == 0 && width == bypl) {
      memset(vram + (size_t)bypl * y1, value, width * h);
      vmsvga_mark_active_rect_dirty(s, x1, y1, w, h);
      vmsvga_damage_add_visible(s, x1, y1, w, h);
      return true;
    };
    for (row_count = 0; row_count < h; row_count++) {
      uint8_t *dst = vram + (size_t)bypp * x1 +
                     (size_t)bypl * (y1 + row_count);
      memset(dst, value, width);
    };
    vmsvga_mark_active_rect_dirty(s, x1, y1, w, h);
    vmsvga_damage_add_visible(s, x1, y1, w, h);
    return true;
  };
  if (x0 == 0 && x1 == 0 && width == bypl) {
    uint8_t *src = vram + (size_t)bypl * y0;
    uint8_t *dst = vram + (size_t)bypl * y1;
    size_t length = width * h;
    bool reverse = y1 > y0 && y1 < y0 + h;
    if (rop == VMSVGA_ROP_INVERT) {
      vmsvga_rop_invert_buffer(dst, length);
    } else if (rop == VMSVGA_ROP_COPY_INVERTED) {
      vmsvga_rop_copy_inverted_buffer(dst, src, length, reverse);
    } else {
      vmsvga_rop_buffer(dst, src, length, rop, reverse);
    };
    vmsvga_mark_active_rect_dirty(s, x1, y1, w, h);
    vmsvga_damage_add_visible(s, x1, y1, w, h);
    return true;
  };
  reverse_rows = y1 > y0 && y1 < y0 + h;
  reverse_columns = y0 == y1 && x1 > x0 && x1 < x0 + w;
  for (row_count = 0; row_count < h; row_count++) {
    uint32_t row = reverse_rows ? h - 1 - row_count : row_count;
    uint8_t *src_row =
        vram + (size_t)bypp * x0 + (size_t)bypl * (y0 + row);
    uint8_t *dst_row =
        vram + (size_t)bypp * x1 + (size_t)bypl * (y1 + row);
    if (rop == VMSVGA_ROP_INVERT) {
      vmsvga_rop_invert_buffer(dst_row, width);
    } else if (rop == VMSVGA_ROP_COPY_INVERTED) {
      vmsvga_rop_copy_inverted_buffer(dst_row, src_row, width, reverse_columns);
    } else {
      vmsvga_rop_buffer(dst_row, src_row, width, rop, reverse_columns);
    };
  };
  vmsvga_mark_active_rect_dirty(s, x1, y1, w, h);
  vmsvga_damage_add_visible(s, x1, y1, w, h);
  return true;
};
static inline uint32_t vmsvga_vram_read_u32(struct vmsvga_state_s *s,
                                               uint32_t offset) {
  uint32_t value;
  memcpy(&value, s->vga.vram_ptr + offset, sizeof(value));
  return le32_to_cpu(value);
};
static inline void vmsvga_vram_write_u32(struct vmsvga_state_s *s,
                                         uint32_t offset, uint32_t value) {
  value = cpu_to_le32(value);
  memcpy(s->vga.vram_ptr + offset, &value, sizeof(value));
  vmsvga_mark_vram_dirty_range(s, offset, sizeof(value));
};
static inline bool vmsvga_surface_load(struct vmsvga_state_s *s,
                                       uint32_t descriptor_offset,
                                       struct vmsvga_surface_s *surface) {
  uint64_t vram_size = s->vga.vram_size;
  uint32_t size;
  uint32_t version;
  uint64_t min_pitch;
  uint64_t data_size;
  if ((descriptor_offset & 3) != 0 ||
      descriptor_offset > vram_size ||
      sizeof(uint32_t) * 10 > vram_size - descriptor_offset) {
    return false;
  };
  size = vmsvga_vram_read_u32(s, descriptor_offset + 0);
  version = vmsvga_vram_read_u32(s, descriptor_offset + 4);
  surface->descriptor_offset = descriptor_offset;
  surface->bpp = vmsvga_vram_read_u32(s, descriptor_offset + 8);
  surface->width = vmsvga_vram_read_u32(s, descriptor_offset + 12);
  surface->height = vmsvga_vram_read_u32(s, descriptor_offset + 16);
  surface->pitch = vmsvga_vram_read_u32(s, descriptor_offset + 20);
  surface->data_offset = vmsvga_vram_read_u32(s, descriptor_offset + 36);
  surface->bypp = vmsvga_bytes_per_pixel(surface->bpp);
  if (size < sizeof(uint32_t) * 10 || size > vram_size - descriptor_offset ||
      version != VMSVGA_SURFACE_VERSION_1 || surface->bypp == 0 ||
      surface->width < 1 ||
      surface->width > VMSVGA_MAX_WIDTH || surface->height < 1 ||
      surface->height > VMSVGA_MAX_HEIGHT) {
    return false;
  };
  min_pitch = (uint64_t)surface->width * surface->bypp;
  if (surface->pitch < min_pitch) {
    return false;
  };
  data_size = (uint64_t)surface->pitch * surface->height;
  if (surface->data_offset > vram_size || data_size > vram_size - surface->data_offset) {
    return false;
  };
  return true;
};
static inline bool vmsvga_surface_rect_valid(const struct vmsvga_surface_s *surface,
                                              uint32_t x, uint32_t y,
                                              uint32_t w, uint32_t h) {
  return x <= surface->width && y <= surface->height &&
         w <= surface->width - x && h <= surface->height - y;
};
static inline void vmsvga_surface_dequeue(struct vmsvga_state_s *s,
                                          const struct vmsvga_surface_s *surface) {
  uint32_t offset = surface->descriptor_offset + 28;
  uint32_t dequeued = vmsvga_vram_read_u32(s, offset);
  vmsvga_vram_write_u32(s, offset, dequeued + 1);
};
static inline void vmsvga_surface_update_display(struct vmsvga_state_s *s,
                                                 const struct vmsvga_surface_s *surface,
                                                 uint32_t x, uint32_t y,
                                                 uint32_t w, uint32_t h) {
  uint32_t display_stride = vmsvga_stride(s);
  uint32_t display_bypp = vmsvga_bytes_per_pixel(vmsvga_active_depth(s));
  uint32_t display_width = vmsvga_active_width(s);
  uint32_t display_height = vmsvga_active_height(s);
  uint64_t fb_size = (uint64_t)display_stride * display_height;
  uint64_t surface_start = surface->data_offset;
  uint64_t rect_start;
  if (w == 0 || h == 0 || surface_start >= fb_size || display_stride == 0 ||
      display_bypp == 0) {
    return;
  };
  rect_start = surface_start + (uint64_t)surface->pitch * y +
               (uint64_t)surface->bypp * x;
  if (rect_start >= fb_size) {
    return;
  };
  if (surface->pitch == display_stride && surface->bypp == display_bypp &&
      surface_start % display_bypp == 0) {
    uint64_t base_y = surface_start / display_stride;
    uint64_t base_x = (surface_start % display_stride) / display_bypp;
    uint64_t dst_x = base_x + x;
    uint64_t dst_y = base_y + y;
    if (dst_x <= display_width && dst_y <= display_height &&
        w <= display_width - dst_x && h <= display_height - dst_y) {
      vmsvga_damage_add(s, (uint32_t)dst_x, (uint32_t)dst_y, w, h);
      return;
    };
  };
  vmsvga_damage_add(s, 0, 0, display_width, display_height);
};
static inline bool vmsvga_surface_fill(struct vmsvga_state_s *s, uint32_t color,
                                       uint32_t surface_offset, uint32_t x,
                                       uint32_t y, uint32_t w, uint32_t h,
                                       uint32_t rop) {
  struct vmsvga_surface_s surface = {0};
  uint8_t col[4];
  size_t width_bytes;
  size_t span_bytes;
  uint32_t row;
  bool contiguous;
  bool valid = vmsvga_surface_load(s, surface_offset, &surface);
  if (!valid) {
    return false;
  };
  if (!vmsvga_surface_rect_valid(&surface, x, y, w, h) ||
      rop > VMSVGA_ROP_SET) {
    vmsvga_surface_dequeue(s, &surface);
    return false;
  };
  if (w == 0 || h == 0 || rop == VMSVGA_ROP_NOOP) {
    vmsvga_surface_dequeue(s, &surface);
    return true;
  };
  if (vmsvga_solid_rop_is_noop(color, surface.bypp, rop)) {
    vmsvga_surface_dequeue(s, &surface);
    return true;
  };
  width_bytes = (size_t)surface.bypp * w;
  span_bytes = width_bytes * h;
  contiguous = x == 0 && width_bytes == surface.pitch;
  col[0] = color;
  col[1] = color >> 8;
  col[2] = color >> 16;
  col[3] = color >> 24;
  if (rop == VMSVGA_ROP_COPY || rop == VMSVGA_ROP_COPY_INVERTED) {
    uint8_t inverted[4] = {~col[0], ~col[1], ~col[2], ~col[3]};
    const uint8_t *pattern = rop == VMSVGA_ROP_COPY ? col : inverted;
    uint8_t *first = s->vga.vram_ptr + surface.data_offset +
                     (size_t)surface.pitch * y + (size_t)surface.bypp * x;
    vmsvga_fill_pattern(first, contiguous ? span_bytes : width_bytes, pattern,
                        surface.bypp);
    if (!contiguous) {
      for (row = 1; row < h; row++) {
        uint8_t *dst = s->vga.vram_ptr + surface.data_offset +
                       (size_t)surface.pitch * (y + row) +
                       (size_t)surface.bypp * x;
        memcpy(dst, first, width_bytes);
      };
    };
  } else if (contiguous) {
    uint8_t *dst = s->vga.vram_ptr + surface.data_offset +
                   (size_t)surface.pitch * y;
    if (rop == VMSVGA_ROP_CLEAR) {
      memset(dst, 0, span_bytes);
    } else if (rop == VMSVGA_ROP_SET) {
      memset(dst, 0xff, span_bytes);
    } else if (rop == VMSVGA_ROP_INVERT) {
      vmsvga_rop_invert_buffer(dst, span_bytes);
    } else {
      vmsvga_rop_fill_buffer(dst, span_bytes, col, surface.bypp, rop);
    };
  } else {
    for (row = 0; row < h; row++) {
      uint8_t *dst = s->vga.vram_ptr + surface.data_offset +
                     (size_t)surface.pitch * (y + row) +
                     (size_t)surface.bypp * x;
      if (rop == VMSVGA_ROP_CLEAR) {
        memset(dst, 0, width_bytes);
      } else if (rop == VMSVGA_ROP_SET) {
        memset(dst, 0xff, width_bytes);
      } else if (rop == VMSVGA_ROP_INVERT) {
        vmsvga_rop_invert_buffer(dst, width_bytes);
      } else {
        vmsvga_rop_fill_buffer(dst, width_bytes, col, surface.bypp, rop);
      };
    };
  };
  vmsvga_mark_vram_dirty_rect(s, surface.data_offset, surface.pitch,
                               surface.bypp, x, y, w, h);
  vmsvga_surface_update_display(s, &surface, x, y, w, h);
  vmsvga_surface_dequeue(s, &surface);
  return true;
};
static inline bool vmsvga_surface_copy(struct vmsvga_state_s *s,
                                       uint32_t src_surface_offset,
                                       uint32_t dst_surface_offset,
                                       uint32_t src_x, uint32_t src_y,
                                       uint32_t dst_x, uint32_t dst_y,
                                       uint32_t w, uint32_t h, uint32_t rop) {
  struct vmsvga_surface_s src_surface = {0};
  struct vmsvga_surface_s dst_surface = {0};
  uint8_t *vram = s->vga.vram_ptr;
  size_t width_bytes;
  size_t span_bytes;
  uint64_t src_start;
  uint64_t src_end;
  uint64_t dst_start;
  uint64_t dst_end;
  bool reverse_rows = false;
  bool reverse_columns = false;
  bool src_contiguous;
  bool dst_contiguous;
  uint32_t row_count;
  bool src_valid = vmsvga_surface_load(s, src_surface_offset, &src_surface);
  bool dst_valid = vmsvga_surface_load(s, dst_surface_offset, &dst_surface);
  if (!src_valid || !dst_valid) {
    if (src_valid) {
      vmsvga_surface_dequeue(s, &src_surface);
    };
    if (dst_valid) {
      vmsvga_surface_dequeue(s, &dst_surface);
    };
    return false;
  };
  if (!vmsvga_surface_rect_valid(&src_surface, src_x, src_y, w, h) ||
      !vmsvga_surface_rect_valid(&dst_surface, dst_x, dst_y, w, h) ||
      src_surface.bpp != dst_surface.bpp || src_surface.bypp != dst_surface.bypp ||
      rop > VMSVGA_ROP_SET) {
    vmsvga_surface_dequeue(s, &src_surface);
    vmsvga_surface_dequeue(s, &dst_surface);
    return false;
  };
  if (w == 0 || h == 0 || rop == VMSVGA_ROP_NOOP) {
    vmsvga_surface_dequeue(s, &src_surface);
    vmsvga_surface_dequeue(s, &dst_surface);
    return true;
  };
  width_bytes = (size_t)src_surface.bypp * w;
  span_bytes = width_bytes * h;
  src_start = (uint64_t)src_surface.data_offset +
              (uint64_t)src_surface.pitch * src_y +
              (uint64_t)src_surface.bypp * src_x;
  src_end = (uint64_t)src_surface.data_offset +
            (uint64_t)src_surface.pitch * (src_y + h - 1) +
            (uint64_t)src_surface.bypp * src_x + width_bytes;
  dst_start = (uint64_t)dst_surface.data_offset +
              (uint64_t)dst_surface.pitch * dst_y +
              (uint64_t)dst_surface.bypp * dst_x;
  dst_end = (uint64_t)dst_surface.data_offset +
            (uint64_t)dst_surface.pitch * (dst_y + h - 1) +
            (uint64_t)dst_surface.bypp * dst_x + width_bytes;
  if (src_start < dst_end && dst_start < src_end) {
    if (src_surface.data_offset != dst_surface.data_offset ||
        src_surface.pitch != dst_surface.pitch ||
        src_surface.width != dst_surface.width ||
        src_surface.height != dst_surface.height) {
      vmsvga_surface_dequeue(s, &src_surface);
      vmsvga_surface_dequeue(s, &dst_surface);
      return false;
    };
    reverse_rows = dst_y > src_y && dst_y < src_y + h;
    reverse_columns = src_y == dst_y && dst_x > src_x && dst_x < src_x + w;
  };
  src_contiguous = src_x == 0 && width_bytes == src_surface.pitch;
  dst_contiguous = dst_x == 0 && width_bytes == dst_surface.pitch;
  if (dst_contiguous && (rop == VMSVGA_ROP_CLEAR ||
                         rop == VMSVGA_ROP_SET ||
                         rop == VMSVGA_ROP_INVERT)) {
    uint8_t *dst = vram + (size_t)dst_start;
    if (rop == VMSVGA_ROP_CLEAR) {
      memset(dst, 0, span_bytes);
    } else if (rop == VMSVGA_ROP_SET) {
      memset(dst, 0xff, span_bytes);
    } else {
      vmsvga_rop_invert_buffer(dst, span_bytes);
    };
    vmsvga_mark_vram_dirty_rect(
        s, dst_surface.data_offset, dst_surface.pitch, dst_surface.bypp,
        dst_x, dst_y, w, h);
    vmsvga_surface_update_display(s, &dst_surface, dst_x, dst_y, w, h);
    vmsvga_surface_dequeue(s, &src_surface);
    vmsvga_surface_dequeue(s, &dst_surface);
    return true;
  };
  if (src_contiguous && dst_contiguous) {
    uint8_t *src = vram + (size_t)src_start;
    uint8_t *dst = vram + (size_t)dst_start;
    bool reverse = dst_start > src_start && dst_start < src_start + span_bytes;
    if (rop == VMSVGA_ROP_COPY) {
      memmove(dst, src, span_bytes);
    } else if (rop == VMSVGA_ROP_COPY_INVERTED) {
      vmsvga_rop_copy_inverted_buffer(dst, src, span_bytes, reverse);
    } else {
      vmsvga_rop_buffer(dst, src, span_bytes, rop, reverse);
    };
    vmsvga_mark_vram_dirty_rect(
        s, dst_surface.data_offset, dst_surface.pitch, dst_surface.bypp,
        dst_x, dst_y, w, h);
    vmsvga_surface_update_display(s, &dst_surface, dst_x, dst_y, w, h);
    vmsvga_surface_dequeue(s, &src_surface);
    vmsvga_surface_dequeue(s, &dst_surface);
    return true;
  };
  for (row_count = 0; row_count < h; row_count++) {
    uint32_t row = reverse_rows ? h - 1 - row_count : row_count;
    uint8_t *src = vram + src_surface.data_offset +
                   (size_t)src_surface.pitch * (src_y + row) +
                   (size_t)src_surface.bypp * src_x;
    uint8_t *dst = vram + dst_surface.data_offset +
                   (size_t)dst_surface.pitch * (dst_y + row) +
                   (size_t)dst_surface.bypp * dst_x;
    if (rop == VMSVGA_ROP_COPY) {
      memmove(dst, src, width_bytes);
    } else if (rop == VMSVGA_ROP_CLEAR) {
      memset(dst, 0, width_bytes);
    } else if (rop == VMSVGA_ROP_SET) {
      memset(dst, 0xff, width_bytes);
    } else if (rop == VMSVGA_ROP_INVERT) {
      vmsvga_rop_invert_buffer(dst, width_bytes);
    } else if (rop == VMSVGA_ROP_COPY_INVERTED) {
      vmsvga_rop_copy_inverted_buffer(dst, src, width_bytes, reverse_columns);
    } else {
      vmsvga_rop_buffer(dst, src, width_bytes, rop, reverse_columns);
    };
  };
  vmsvga_mark_vram_dirty_rect(
      s, dst_surface.data_offset, dst_surface.pitch, dst_surface.bypp, dst_x,
      dst_y, w, h);
  vmsvga_surface_update_display(s, &dst_surface, dst_x, dst_y, w, h);
  vmsvga_surface_dequeue(s, &src_surface);
  vmsvga_surface_dequeue(s, &dst_surface);
  return true;
};
static inline uint32_t vmsvga_mul_u8(uint32_t value, uint32_t factor) {
  uint32_t product = value * factor + 128;
  return (product + (product >> 8)) >> 8;
};
static inline uint32_t vmsvga_expand_5(uint32_t value) {
  return (value << 3) | (value >> 2);
};
static inline uint32_t vmsvga_expand_6(uint32_t value) {
  return (value << 2) | (value >> 4);
};
static inline uint32_t vmsvga_pack_5(uint32_t value) {
  return (value * 31 + 127) / 255;
};
static inline uint32_t vmsvga_pack_6(uint32_t value) {
  return (value * 63 + 127) / 255;
};
static inline struct vmsvga_pixel_s
vmsvga_surface_read_pixel(const uint8_t *src, uint32_t bpp, bool opaque) {
  struct vmsvga_pixel_s pixel = {0};
  uint16_t value16;
  switch (bpp) {
  case 15:
    memcpy(&value16, src, sizeof(value16));
    value16 = le16_to_cpu(value16);
    pixel.blue = vmsvga_expand_5(value16 & 0x1f);
    pixel.green = vmsvga_expand_5((value16 >> 5) & 0x1f);
    pixel.red = vmsvga_expand_5((value16 >> 10) & 0x1f);
    pixel.alpha = 255;
    break;
  case 16:
    memcpy(&value16, src, sizeof(value16));
    value16 = le16_to_cpu(value16);
    pixel.blue = vmsvga_expand_5(value16 & 0x1f);
    pixel.green = vmsvga_expand_6((value16 >> 5) & 0x3f);
    pixel.red = vmsvga_expand_5((value16 >> 11) & 0x1f);
    pixel.alpha = 255;
    break;
  case 24:
    pixel.blue = src[0];
    pixel.green = src[1];
    pixel.red = src[2];
    pixel.alpha = 255;
    break;
  case 32:
    pixel.blue = src[0];
    pixel.green = src[1];
    pixel.red = src[2];
    pixel.alpha = opaque ? 255 : src[3];
    break;
  default:
    break;
  };
  return pixel;
};
static inline void vmsvga_surface_write_pixel(uint8_t *dst, uint32_t bpp,
                                               bool opaque,
                                               struct vmsvga_pixel_s pixel) {
  uint16_t value16;
  pixel.red = MIN(pixel.red, 255U);
  pixel.green = MIN(pixel.green, 255U);
  pixel.blue = MIN(pixel.blue, 255U);
  pixel.alpha = MIN(pixel.alpha, 255U);
  switch (bpp) {
  case 15:
    value16 = (vmsvga_pack_5(pixel.red) << 10) |
              (vmsvga_pack_5(pixel.green) << 5) |
              vmsvga_pack_5(pixel.blue);
    value16 = cpu_to_le16(value16);
    memcpy(dst, &value16, sizeof(value16));
    break;
  case 16:
    value16 = (vmsvga_pack_5(pixel.red) << 11) |
              (vmsvga_pack_6(pixel.green) << 5) |
              vmsvga_pack_5(pixel.blue);
    value16 = cpu_to_le16(value16);
    memcpy(dst, &value16, sizeof(value16));
    break;
  case 24:
    dst[0] = pixel.blue;
    dst[1] = pixel.green;
    dst[2] = pixel.red;
    break;
  case 32:
    dst[0] = pixel.blue;
    dst[1] = pixel.green;
    dst[2] = pixel.red;
    dst[3] = opaque ? 255 : pixel.alpha;
    break;
  default:
    break;
  };
};
static inline void vmsvga_surface_scale_pixel(struct vmsvga_pixel_s *pixel,
                                               uint32_t factor) {
  pixel->red = vmsvga_mul_u8(pixel->red, factor);
  pixel->green = vmsvga_mul_u8(pixel->green, factor);
  pixel->blue = vmsvga_mul_u8(pixel->blue, factor);
  pixel->alpha = vmsvga_mul_u8(pixel->alpha, factor);
};
static inline uint32_t vmsvga_blend_component(uint32_t src, uint32_t dst,
                                               uint32_t src_factor,
                                               uint32_t dst_factor) {
  return MIN(255U, vmsvga_mul_u8(src, src_factor) +
                       vmsvga_mul_u8(dst, dst_factor));
};
static inline struct vmsvga_pixel_s
vmsvga_blend_pixel(struct vmsvga_pixel_s src, struct vmsvga_pixel_s dst,
                    uint32_t blend_op) {
  struct vmsvga_pixel_s out;
  uint32_t src_factor;
  uint32_t dst_factor;
  switch (blend_op) {
  case VMSVGA_BLENDOP_CLEAR:
    src_factor = 0;
    dst_factor = 0;
    break;
  case VMSVGA_BLENDOP_SRC:
    src_factor = 255;
    dst_factor = 0;
    break;
  case VMSVGA_BLENDOP_DST:
    src_factor = 0;
    dst_factor = 255;
    break;
  case VMSVGA_BLENDOP_OVER:
    src_factor = 255;
    dst_factor = 255 - src.alpha;
    break;
  case VMSVGA_BLENDOP_OVER_REVERSE:
    src_factor = 255 - dst.alpha;
    dst_factor = 255;
    break;
  case VMSVGA_BLENDOP_IN:
    src_factor = dst.alpha;
    dst_factor = 0;
    break;
  case VMSVGA_BLENDOP_IN_REVERSE:
    src_factor = 0;
    dst_factor = src.alpha;
    break;
  case VMSVGA_BLENDOP_OUT:
    src_factor = 255 - dst.alpha;
    dst_factor = 0;
    break;
  case VMSVGA_BLENDOP_OUT_REVERSE:
    src_factor = 0;
    dst_factor = 255 - src.alpha;
    break;
  case VMSVGA_BLENDOP_ATOP:
    src_factor = dst.alpha;
    dst_factor = 255 - src.alpha;
    break;
  case VMSVGA_BLENDOP_ATOP_REVERSE:
    src_factor = 255 - dst.alpha;
    dst_factor = src.alpha;
    break;
  case VMSVGA_BLENDOP_XOR:
    src_factor = 255 - dst.alpha;
    dst_factor = 255 - src.alpha;
    break;
  case VMSVGA_BLENDOP_ADD:
    src_factor = 255;
    dst_factor = 255;
    break;
  case VMSVGA_BLENDOP_SATURATE:
    if (src.alpha == 0) {
      src_factor = 255;
    } else {
      src_factor = MIN(255U, ((255 - dst.alpha) * 255U + src.alpha / 2) /
                                 src.alpha);
    };
    dst_factor = 255;
    break;
  default:
    src_factor = 0;
    dst_factor = 255;
    break;
  };
  out.red = vmsvga_blend_component(src.red, dst.red, src_factor, dst_factor);
  out.green =
      vmsvga_blend_component(src.green, dst.green, src_factor, dst_factor);
  out.blue =
      vmsvga_blend_component(src.blue, dst.blue, src_factor, dst_factor);
  out.alpha =
      vmsvga_blend_component(src.alpha, dst.alpha, src_factor, dst_factor);
  return out;
};
static inline bool vmsvga_surface_alpha_blend(
    struct vmsvga_state_s *s, uint32_t src_surface_offset,
    uint32_t dst_surface_offset, uint32_t src_x, uint32_t src_y,
    uint32_t dst_x, uint32_t dst_y, uint32_t w, uint32_t h,
    uint32_t blend_op, uint32_t flags, uint32_t param1, uint32_t param2) {
  struct vmsvga_surface_s src_surface = {0};
  struct vmsvga_surface_s dst_surface = {0};
  uint8_t *vram = s->vga.vram_ptr;
  uint64_t src_start;
  uint64_t src_end;
  uint64_t dst_start;
  uint64_t dst_end;
  size_t src_width_bytes;
  size_t dst_width_bytes;
  bool reverse_rows = false;
  bool reverse_columns = false;
  bool src_opaque;
  bool dst_opaque;
  uint32_t row_count;
  bool src_valid = vmsvga_surface_load(s, src_surface_offset, &src_surface);
  bool dst_valid = vmsvga_surface_load(s, dst_surface_offset, &dst_surface);
  if (!src_valid || !dst_valid) {
    if (src_valid) {
      vmsvga_surface_dequeue(s, &src_surface);
    };
    if (dst_valid) {
      vmsvga_surface_dequeue(s, &dst_surface);
    };
    return false;
  };
  if (!vmsvga_surface_rect_valid(&src_surface, src_x, src_y, w, h) ||
      !vmsvga_surface_rect_valid(&dst_surface, dst_x, dst_y, w, h) ||
      src_surface.bpp < 15 || dst_surface.bpp < 15 ||
      blend_op > VMSVGA_BLENDOP_SATURATE || (flags & ~VMSVGA_BLENDFLAG_ALL) ||
      ((flags & VMSVGA_BLENDFLAG_CONSTANT_SOURCE_ALPHA) && param1 > 255) ||
      ((flags & VMSVGA_BLENDFLAG_CONSTANT_DEST_ALPHA) && param2 > 255)) {
    vmsvga_surface_dequeue(s, &src_surface);
    vmsvga_surface_dequeue(s, &dst_surface);
    return false;
  };
  if (w == 0 || h == 0 ||
      (blend_op == VMSVGA_BLENDOP_DST &&
       !(flags & VMSVGA_BLENDFLAG_CONSTANT_DEST_ALPHA))) {
    vmsvga_surface_dequeue(s, &src_surface);
    vmsvga_surface_dequeue(s, &dst_surface);
    return true;
  };
  src_opaque = src_surface.bpp != 32 || src_surface.data_offset == 0;
  dst_opaque = dst_surface.bpp != 32 || dst_surface.data_offset == 0;
  src_width_bytes = (size_t)src_surface.bypp * w;
  dst_width_bytes = (size_t)dst_surface.bypp * w;
  src_start = (uint64_t)src_surface.data_offset +
              (uint64_t)src_surface.pitch * src_y +
              (uint64_t)src_surface.bypp * src_x;
  src_end = (uint64_t)src_surface.data_offset +
            (uint64_t)src_surface.pitch * (src_y + h - 1) +
            (uint64_t)src_surface.bypp * src_x + src_width_bytes;
  dst_start = (uint64_t)dst_surface.data_offset +
              (uint64_t)dst_surface.pitch * dst_y +
              (uint64_t)dst_surface.bypp * dst_x;
  dst_end = (uint64_t)dst_surface.data_offset +
            (uint64_t)dst_surface.pitch * (dst_y + h - 1) +
            (uint64_t)dst_surface.bypp * dst_x + dst_width_bytes;
  if (src_start < dst_end && dst_start < src_end) {
    if (src_surface.data_offset != dst_surface.data_offset ||
        src_surface.pitch != dst_surface.pitch ||
        src_surface.width != dst_surface.width ||
        src_surface.height != dst_surface.height ||
        src_surface.bpp != dst_surface.bpp) {
      vmsvga_surface_dequeue(s, &src_surface);
      vmsvga_surface_dequeue(s, &dst_surface);
      return false;
    };
    reverse_rows = dst_y > src_y && dst_y < src_y + h;
    reverse_columns = src_y == dst_y && dst_x > src_x && dst_x < src_x + w;
  };
  if (blend_op == VMSVGA_BLENDOP_CLEAR) {
    static const uint8_t opaque_black[4] = {0, 0, 0, 0xff};
    for (row_count = 0; row_count < h; row_count++) {
      uint8_t *dst = vram + dst_surface.data_offset +
                     (size_t)dst_surface.pitch * (dst_y + row_count) +
                     (size_t)dst_surface.bypp * dst_x;
      if (dst_surface.bpp == 32 && dst_opaque) {
        vmsvga_fill_pattern(dst, dst_width_bytes, opaque_black, 4);
      } else {
        memset(dst, 0, dst_width_bytes);
      };
    };
    vmsvga_mark_vram_dirty_rect(
        s, dst_surface.data_offset, dst_surface.pitch, dst_surface.bypp,
        dst_x, dst_y, w, h);
    vmsvga_surface_update_display(s, &dst_surface, dst_x, dst_y, w, h);
    vmsvga_surface_dequeue(s, &src_surface);
    vmsvga_surface_dequeue(s, &dst_surface);
    return true;
  };
  if (blend_op == VMSVGA_BLENDOP_SRC && flags == 0 &&
      src_surface.bpp == dst_surface.bpp &&
      (src_surface.bpp != 32 || (!src_opaque && !dst_opaque))) {
    for (row_count = 0; row_count < h; row_count++) {
      uint32_t row = reverse_rows ? h - 1 - row_count : row_count;
      uint8_t *src = vram + src_surface.data_offset +
                     (size_t)src_surface.pitch * (src_y + row) +
                     (size_t)src_surface.bypp * src_x;
      uint8_t *dst = vram + dst_surface.data_offset +
                     (size_t)dst_surface.pitch * (dst_y + row) +
                     (size_t)dst_surface.bypp * dst_x;
      memmove(dst, src, src_width_bytes);
    };
    vmsvga_mark_vram_dirty_rect(
        s, dst_surface.data_offset, dst_surface.pitch, dst_surface.bypp,
        dst_x, dst_y, w, h);
    vmsvga_surface_update_display(s, &dst_surface, dst_x, dst_y, w, h);
    vmsvga_surface_dequeue(s, &src_surface);
    vmsvga_surface_dequeue(s, &dst_surface);
    return true;
  };
  if (blend_op == VMSVGA_BLENDOP_OVER && flags == 0 &&
      src_surface.bpp == 32 && dst_surface.bpp == 32 && !src_opaque &&
      dst_opaque) {
    for (row_count = 0; row_count < h; row_count++) {
      uint32_t row = reverse_rows ? h - 1 - row_count : row_count;
      uint8_t *src_row = vram + src_surface.data_offset +
                         (size_t)src_surface.pitch * (src_y + row) +
                         (size_t)src_surface.bypp * src_x;
      uint8_t *dst_row = vram + dst_surface.data_offset +
                         (size_t)dst_surface.pitch * (dst_y + row) +
                         (size_t)dst_surface.bypp * dst_x;
      uint32_t column_count;
      for (column_count = 0; column_count < w; column_count++) {
        uint32_t column =
            reverse_columns ? w - 1 - column_count : column_count;
        uint8_t *src = src_row + (size_t)column * 4;
        uint8_t *dst = dst_row + (size_t)column * 4;
        uint32_t inverse_alpha = 255 - src[3];
        dst[0] = MIN(255U, (uint32_t)src[0] +
                                  vmsvga_mul_u8(dst[0], inverse_alpha));
        dst[1] = MIN(255U, (uint32_t)src[1] +
                                  vmsvga_mul_u8(dst[1], inverse_alpha));
        dst[2] = MIN(255U, (uint32_t)src[2] +
                                  vmsvga_mul_u8(dst[2], inverse_alpha));
        dst[3] = 255;
      };
    };
    vmsvga_mark_vram_dirty_rect(
        s, dst_surface.data_offset, dst_surface.pitch, dst_surface.bypp,
        dst_x, dst_y, w, h);
    vmsvga_surface_update_display(s, &dst_surface, dst_x, dst_y, w, h);
    vmsvga_surface_dequeue(s, &src_surface);
    vmsvga_surface_dequeue(s, &dst_surface);
    return true;
  };
  for (row_count = 0; row_count < h; row_count++) {
    uint32_t row = reverse_rows ? h - 1 - row_count : row_count;
    uint8_t *src_row = vram + src_surface.data_offset +
                       (size_t)src_surface.pitch * (src_y + row) +
                       (size_t)src_surface.bypp * src_x;
    uint8_t *dst_row = vram + dst_surface.data_offset +
                       (size_t)dst_surface.pitch * (dst_y + row) +
                       (size_t)dst_surface.bypp * dst_x;
    uint32_t column_count;
    for (column_count = 0; column_count < w; column_count++) {
      uint32_t column = reverse_columns ? w - 1 - column_count : column_count;
      struct vmsvga_pixel_s src = vmsvga_surface_read_pixel(
          src_row + (size_t)src_surface.bypp * column, src_surface.bpp,
          src_opaque);
      struct vmsvga_pixel_s dst = vmsvga_surface_read_pixel(
          dst_row + (size_t)dst_surface.bypp * column, dst_surface.bpp,
          dst_opaque);
      struct vmsvga_pixel_s out;
      if (flags & VMSVGA_BLENDFLAG_CONSTANT_SOURCE_ALPHA) {
        vmsvga_surface_scale_pixel(&src, param1);
      };
      if (flags & VMSVGA_BLENDFLAG_CONSTANT_DEST_ALPHA) {
        vmsvga_surface_scale_pixel(&dst, param2);
      };
      out = vmsvga_blend_pixel(src, dst, blend_op);
      vmsvga_surface_write_pixel(
          dst_row + (size_t)dst_surface.bypp * column, dst_surface.bpp,
          dst_opaque, out);
    };
  };
  vmsvga_mark_vram_dirty_rect(
      s, dst_surface.data_offset, dst_surface.pitch, dst_surface.bypp, dst_x,
      dst_y, w, h);
  vmsvga_surface_update_display(s, &dst_surface, dst_x, dst_y, w, h);
  vmsvga_surface_dequeue(s, &src_surface);
  vmsvga_surface_dequeue(s, &dst_surface);
  return true;
};
static inline void vmsvga_object_destroy(struct vmsvga_object_s *object) {
  if (object == NULL) {
    return;
  };
  g_free(object->data);
  g_free(object);
};
static void vmsvga_objects_clear(struct vmsvga_state_s *s) {
  uint32_t id;
  for (id = 0; id < VMSVGA_MAX_OBJECTS; id++) {
    vmsvga_object_destroy(s->objects[id]);
    s->objects[id] = NULL;
  };
  s->object_bytes = 0;
};
/*
 * SVGA_REG_MEMORY_SIZE includes both VRAM and dedicated surface memory.
 * Keep the advertised surface-memory budget equal to the allocation limit
 * enforced for device objects.
 */
static inline size_t
vmsvga_surface_memory_size(const struct vmsvga_state_s *s) {
  return s->vga.vram_size;
};
static inline uint32_t
vmsvga_memory_size(const struct vmsvga_state_s *s) {
  uint64_t memory_size =
      (uint64_t)s->vga.vram_size + vmsvga_surface_memory_size(s);
  return memory_size > UINT32_MAX ? UINT32_MAX : (uint32_t)memory_size;
};
static inline bool vmsvga_object_layout(uint32_t type, uint32_t width,
                                        uint32_t height, uint32_t depth,
                                        uint32_t *stride_out,
                                        size_t *size_out) {
  uint64_t bits_per_line;
  uint64_t stride;
  uint64_t size;
  if (width < 1 || width > VMSVGA_MAX_WIDTH || height < 1 ||
      height > VMSVGA_MAX_HEIGHT) {
    return false;
  };
  if (type == VMSVGA_OBJECT_BITMAP) {
    depth = 1;
  } else if (type != VMSVGA_OBJECT_PIXMAP || depth < 1 || depth > 32) {
    return false;
  };
  bits_per_line = (uint64_t)width * depth;
  stride = ((bits_per_line + 31) >> 5) * sizeof(uint32_t);
  size = stride * height;
  if (stride == 0 || stride > UINT32_MAX || size == 0 || size > SIZE_MAX) {
    return false;
  };
  *stride_out = (uint32_t)stride;
  *size_out = (size_t)size;
  return true;
};
static inline struct vmsvga_object_s *
vmsvga_object_create(struct vmsvga_state_s *s, uint32_t id, uint32_t type,
                     uint32_t width, uint32_t height, uint32_t depth) {
  struct vmsvga_object_s *old;
  struct vmsvga_object_s *object;
  uint32_t stride;
  size_t size;
  size_t old_size;
  size_t limit;
  if (id >= VMSVGA_MAX_OBJECTS ||
      !vmsvga_object_layout(type, width, height, depth, &stride, &size)) {
    return NULL;
  };
  old = s->objects[id];
  old_size = old ? old->size : 0;
  limit = vmsvga_surface_memory_size(s);
  if (size > limit || s->object_bytes < old_size ||
      s->object_bytes - old_size > limit - size) {
    return NULL;
  };
  if (old != NULL && old->type == type && old->width == width &&
      old->height == height &&
      old->depth == (type == VMSVGA_OBJECT_BITMAP ? 1 : depth) &&
      old->stride == stride && old->size == size) {
    memset(old->data, 0, size);
    return old;
  };
  object = g_try_new0(struct vmsvga_object_s, 1);
  if (object == NULL) {
    return NULL;
  };
  object->data = g_try_malloc0(size);
  if (object->data == NULL) {
    g_free(object);
    return NULL;
  };
  object->type = type;
  object->width = width;
  object->height = height;
  object->depth = type == VMSVGA_OBJECT_BITMAP ? 1 : depth;
  object->stride = stride;
  object->size = size;
  s->object_bytes -= old_size;
  vmsvga_object_destroy(old);
  s->objects[id] = object;
  s->object_bytes += size;
  return object;
};
static inline struct vmsvga_object_s *
vmsvga_object_get(struct vmsvga_state_s *s, uint32_t id, uint32_t type) {
  struct vmsvga_object_s *object;
  if (id >= VMSVGA_MAX_OBJECTS) {
    return NULL;
  };
  object = s->objects[id];
  if (object == NULL || object->type != type) {
    return NULL;
  };
  return object;
};
static inline void vmsvga_object_free(struct vmsvga_state_s *s, uint32_t id) {
  struct vmsvga_object_s *object;
  if (id >= VMSVGA_MAX_OBJECTS) {
    return;
  };
  object = s->objects[id];
  if (object == NULL) {
    return;
  };
  if (s->object_bytes >= object->size) {
    s->object_bytes -= object->size;
  } else {
    s->object_bytes = 0;
  };
  vmsvga_object_destroy(object);
  s->objects[id] = NULL;
};
static inline bool vmsvga_object_matches(struct vmsvga_object_s *object,
                                         uint32_t type, uint32_t width,
                                         uint32_t height, uint32_t depth) {
  if (object == NULL || object->type != type || object->width != width ||
      object->height != height) {
    return false;
  };
  return type == VMSVGA_OBJECT_BITMAP || object->depth == depth;
};
static inline uint32_t vmsvga_object_pixel(struct vmsvga_object_s *object,
                                           uint32_t x, uint32_t y) {
  const uint8_t *row;
  uint64_t bit_offset;
  size_t byte_offset;
  uint32_t shift;
  uint64_t value = 0;
  uint32_t bytes;
  uint32_t i;
  uint64_t mask;
  if (x >= object->width || y >= object->height) {
    return 0;
  };
  row = object->data + (size_t)object->stride * y;
  if (object->type == VMSVGA_OBJECT_BITMAP) {
    return (row[x >> 3] >> (7 - (x & 7))) & 1;
  };
  bit_offset = (uint64_t)x * object->depth;
  byte_offset = (size_t)(bit_offset >> 3);
  shift = bit_offset & 7;
  bytes = (object->depth + shift + 7) >> 3;
  if (bytes > sizeof(value)) {
    bytes = sizeof(value);
  };
  if (byte_offset + bytes > object->stride) {
    bytes = object->stride - byte_offset;
  };
  for (i = 0; i < bytes; i++) {
    value |= (uint64_t)row[byte_offset + i] << (i * 8);
  };
  if (object->depth == 32) {
    mask = UINT32_MAX;
  } else {
    mask = (UINT64_C(1) << object->depth) - 1;
  };
  return (uint32_t)((value >> shift) & mask);
};
static inline bool vmsvga_object_rect_valid(struct vmsvga_object_s *object,
                                            uint32_t x, uint32_t y,
                                            uint32_t width, uint32_t height) {
  return x <= object->width && width <= object->width - x &&
         y <= object->height && height <= object->height - y;
};
static inline bool vmsvga_pixmap_compatible(struct vmsvga_state_s *s,
                                            struct vmsvga_object_s *object) {
  if (object->depth == vmsvga_active_depth(s)) {
    return true;
  };
  return (vmsvga_active_depth(s) == 32 && object->depth == 24) ||
         (vmsvga_active_depth(s) == 24 && object->depth == 32);
};
static inline void vmsvga_store_pixel(uint8_t *dst, uint32_t bypp,
                                      uint32_t pixel) {
  switch (bypp) {
  case 4:
    dst[3] = pixel >> 24;
    /* fall through */
  case 3:
    dst[2] = pixel >> 16;
    /* fall through */
  case 2:
    dst[1] = pixel >> 8;
    /* fall through */
  case 1:
    dst[0] = pixel;
    break;
  default:
    break;
  };
};
static inline bool vmsvga_mono_overlay(uint8_t *dst, const uint8_t *bitmap,
                                       uint32_t first_bit, uint32_t last_bit,
                                       uint32_t bypp, uint32_t foreground) {
  uint32_t byte_index;
  uint32_t last_byte;
  bool changed = false;
  if (first_bit >= last_bit) {
    return false;
  };
  byte_index = first_bit >> 3;
  last_byte = (last_bit - 1) >> 3;
  for (; byte_index <= last_byte; byte_index++) {
    uint32_t bit_base = byte_index << 3;
    uint32_t byte_end = MIN(8U, last_bit - bit_base);
    uint8_t bits = bitmap[byte_index];
    if (bit_base < first_bit) {
      bits &= (uint8_t)(UINT8_MAX >> (first_bit - bit_base));
    };
    if (byte_end < 8) {
      bits &= (uint8_t)(UINT8_MAX << (8 - byte_end));
    };
    while (bits != 0) {
      uint32_t bit_in_byte = 7 - __builtin_ctz((unsigned)bits);
      uint32_t bit = bit_base + bit_in_byte;
      vmsvga_store_pixel(dst + (size_t)(bit - first_bit) * bypp, bypp,
                         foreground);
      bits &= bits - 1;
      changed = true;
    };
  };
  return changed;
};
static inline void vmsvga_repeat_pattern_row(uint8_t *row,
                                             uint32_t pattern_pixels,
                                             uint32_t width, uint32_t bypp) {
  uint32_t filled = pattern_pixels;
  if (pattern_pixels == 0) {
    return;
  };
  while (filled < width) {
    uint32_t pixels = MIN(filled, width - filled);
    memcpy(row + (size_t)filled * bypp, row, (size_t)pixels * bypp);
    filled += pixels;
  };
};
static inline bool vmsvga_object_blit(struct vmsvga_state_s *s, uint32_t id,
                                      uint32_t type, bool pattern,
                                      uint32_t src_x, uint32_t src_y,
                                      uint32_t dst_x, uint32_t dst_y,
                                      uint32_t width, uint32_t height,
                                      uint32_t foreground,
                                      uint32_t background, uint32_t rop) {
  struct vmsvga_object_s *object = vmsvga_object_get(s, id, type);
  VMVGA_TRACE_LOCAL(
      VMVGA_TRACE_OBJECT,
      "OBJECT_BLIT id=%u type=%u pattern=%u src=%u,%u dst=%u,%u w=%u h=%u "
      "fg=0x%08x bg=0x%08x rop=0x%02x",
      id, type, pattern, src_x, src_y, dst_x, dst_y, width, height,
      foreground, background, rop);
  uint32_t bypl = vmsvga_stride(s);
  uint32_t bypp = vmsvga_bytes_per_pixel(vmsvga_active_depth(s));
  size_t row_bytes;
  uint8_t *source_row = s->blit_scratch;
  uint32_t row;
  uint32_t column;
  if (object == NULL) {
    if (id >= VMSVGA_MAX_OBJECTS) {
      VMVGA_BLIT_FAIL(
          "reason=INVALID_ID id=%u type=%u pattern=%u src=%u,%u dst=%u,%u "
          "w=%u h=%u rop=0x%02x active=%ux%u depth=%u stride=%u",
          id, type, pattern, src_x, src_y, dst_x, dst_y, width, height, rop,
          vmsvga_active_width(s), vmsvga_active_height(s),
          vmsvga_active_depth(s), bypl);
    } else if (s->objects[id] == NULL) {
      VMVGA_BLIT_FAIL(
          "reason=NO_OBJECT id=%u type=%u pattern=%u src=%u,%u dst=%u,%u "
          "w=%u h=%u rop=0x%02x active=%ux%u depth=%u stride=%u",
          id, type, pattern, src_x, src_y, dst_x, dst_y, width, height, rop,
          vmsvga_active_width(s), vmsvga_active_height(s),
          vmsvga_active_depth(s), bypl);
    } else {
      VMVGA_BLIT_FAIL(
          "reason=TYPE_MISMATCH id=%u requested_type=%u stored_type=%u "
          "pattern=%u src=%u,%u dst=%u,%u w=%u h=%u rop=0x%02x "
          "object=%ux%u depth=%u stride=%u active=%ux%u depth=%u stride=%u",
          id, type, s->objects[id]->type, pattern, src_x, src_y, dst_x, dst_y,
          width, height, rop, s->objects[id]->width, s->objects[id]->height,
          s->objects[id]->depth, s->objects[id]->stride,
          vmsvga_active_width(s), vmsvga_active_height(s),
          vmsvga_active_depth(s), bypl);
    };
    return false;
  };
  if (rop > VMSVGA_ROP_SET) {
    VMVGA_BLIT_FAIL(
        "reason=INVALID_ROP id=%u type=%u pattern=%u src=%u,%u dst=%u,%u "
        "w=%u h=%u rop=0x%02x object=%ux%u depth=%u stride=%u "
        "active=%ux%u depth=%u stride=%u",
        id, type, pattern, src_x, src_y, dst_x, dst_y, width, height, rop,
        object->width, object->height, object->depth, object->stride,
        vmsvga_active_width(s), vmsvga_active_height(s),
        vmsvga_active_depth(s), bypl);
    return false;
  };
  if (bypp < 1 || bypp > 4) {
    VMVGA_BLIT_FAIL(
        "reason=INVALID_BYPP id=%u type=%u pattern=%u src=%u,%u dst=%u,%u "
        "w=%u h=%u rop=0x%02x bypp=%u active=%ux%u depth=%u stride=%u",
        id, type, pattern, src_x, src_y, dst_x, dst_y, width, height, rop, bypp,
        vmsvga_active_width(s), vmsvga_active_height(s),
        vmsvga_active_depth(s), bypl);
    return false;
  };
  if (!vmsvga_verify_rect(s, dst_x, dst_y, width, height)) {
    VMVGA_BLIT_FAIL(
        "reason=INVALID_DEST id=%u type=%u pattern=%u src=%u,%u dst=%u,%u "
        "w=%u h=%u rop=0x%02x object=%ux%u depth=%u stride=%u "
        "active=%ux%u depth=%u stride=%u",
        id, type, pattern, src_x, src_y, dst_x, dst_y, width, height, rop,
        object->width, object->height, object->depth, object->stride,
        vmsvga_active_width(s), vmsvga_active_height(s),
        vmsvga_active_depth(s), bypl);
    return false;
  };
  if (width == 0 || height == 0 || rop == VMSVGA_ROP_NOOP) {
    return true;
  };
  if (!pattern && !vmsvga_object_rect_valid(object, src_x, src_y, width,
                                             height)) {
    VMVGA_BLIT_FAIL(
        "reason=INVALID_SOURCE id=%u type=%u src=%u,%u dst=%u,%u w=%u h=%u "
        "rop=0x%02x object=%ux%u depth=%u stride=%u active=%ux%u depth=%u "
        "stride=%u",
        id, type, src_x, src_y, dst_x, dst_y, width, height, rop,
        object->width, object->height, object->depth, object->stride,
        vmsvga_active_width(s), vmsvga_active_height(s),
        vmsvga_active_depth(s), bypl);
    return false;
  };
  if (type == VMSVGA_OBJECT_PIXMAP && !vmsvga_pixmap_compatible(s, object)) {
    VMVGA_BLIT_FAIL(
        "reason=INCOMPATIBLE_DEPTH id=%u type=%u pattern=%u src=%u,%u "
        "dst=%u,%u w=%u h=%u rop=0x%02x object=%ux%u depth=%u stride=%u "
        "active=%ux%u depth=%u stride=%u",
        id, type, pattern, src_x, src_y, dst_x, dst_y, width, height, rop,
        object->width, object->height, object->depth, object->stride,
        vmsvga_active_width(s), vmsvga_active_height(s),
        vmsvga_active_depth(s), bypl);
    return false;
  };
  if (type == VMSVGA_OBJECT_BITMAP &&
      vmsvga_solid_rop_is_noop(foreground, bypp, rop) &&
      vmsvga_solid_rop_is_noop(background, bypp, rop)) {
    return true;
  };
  row_bytes = (size_t)width * bypp;
  if (rop == VMSVGA_ROP_CLEAR || rop == VMSVGA_ROP_SET ||
      rop == VMSVGA_ROP_INVERT) {
    if (dst_x == 0 && row_bytes == bypl) {
      uint8_t *dst = s->vga.vram_ptr + (size_t)bypl * dst_y;
      size_t length = row_bytes * height;
      if (rop == VMSVGA_ROP_CLEAR) {
        memset(dst, 0, length);
      } else if (rop == VMSVGA_ROP_SET) {
        memset(dst, 0xff, length);
      } else {
        vmsvga_rop_invert_buffer(dst, length);
      };
      vmsvga_mark_active_rect_dirty(s, dst_x, dst_y, width, height);
      vmsvga_damage_add(s, dst_x, dst_y, width, height);
      return true;
    };
    for (row = 0; row < height; row++) {
      uint8_t *dst = s->vga.vram_ptr + (size_t)bypl * (dst_y + row) +
                     (size_t)bypp * dst_x;
      if (rop == VMSVGA_ROP_CLEAR) {
        memset(dst, 0, row_bytes);
      } else if (rop == VMSVGA_ROP_SET) {
        memset(dst, 0xff, row_bytes);
      } else {
        vmsvga_rop_invert_buffer(dst, row_bytes);
      };
    };
    vmsvga_mark_active_rect_dirty(s, dst_x, dst_y, width, height);
    vmsvga_damage_add(s, dst_x, dst_y, width, height);
    return true;
  };
  if (type == VMSVGA_OBJECT_BITMAP) {
    uint8_t background_col[4];
    background_col[0] = background;
    background_col[1] = background >> 8;
    background_col[2] = background >> 16;
    background_col[3] = background >> 24;
    for (row = 0; row < height; row++) {
      uint32_t object_y =
          pattern ? (dst_y + row) % object->height : src_y + row;
      const uint8_t *bitmap_row =
          object->data + (size_t)object->stride * object_y;
      uint8_t *dst = s->vga.vram_ptr + (size_t)bypl * (dst_y + row) +
                     (size_t)bypp * dst_x;
      if (pattern) {
        uint32_t pattern_x = dst_x % object->width;
        uint32_t pattern_pixels = MIN(width, object->width);
        uint32_t tail_pixels =
            MIN(pattern_pixels, object->width - pattern_x);
        vmsvga_fill_pattern(source_row, (size_t)pattern_pixels * bypp,
                            background_col, bypp);
        vmsvga_mono_overlay(source_row, bitmap_row, pattern_x,
                            pattern_x + tail_pixels, bypp, foreground);
        if (tail_pixels < pattern_pixels) {
          vmsvga_mono_overlay(source_row + (size_t)tail_pixels * bypp,
                              bitmap_row, 0, pattern_pixels - tail_pixels,
                              bypp, foreground);
        };
        vmsvga_repeat_pattern_row(source_row, pattern_pixels, width, bypp);
      } else {
        vmsvga_fill_pattern(source_row, row_bytes, background_col, bypp);
        vmsvga_mono_overlay(source_row, bitmap_row, src_x, src_x + width,
                            bypp, foreground);
      };
      if (rop == VMSVGA_ROP_COPY) {
        memcpy(dst, source_row, row_bytes);
      } else if (rop == VMSVGA_ROP_COPY_INVERTED) {
        vmsvga_rop_copy_inverted_buffer(dst, source_row, row_bytes, false);
      } else {
        vmsvga_rop_buffer(dst, source_row, row_bytes, rop, false);
      };
    };
    vmsvga_mark_active_rect_dirty(s, dst_x, dst_y, width, height);
    vmsvga_damage_add(s, dst_x, dst_y, width, height);
    return true;
  };
  if (type == VMSVGA_OBJECT_PIXMAP &&
      object->depth == vmsvga_active_depth(s) &&
      (object->depth == 8 || object->depth == 16 || object->depth == 24 ||
       object->depth == 32)) {
    if (!pattern && src_x == 0 && dst_x == 0 &&
        row_bytes == object->stride && row_bytes == bypl) {
      const uint8_t *src =
          object->data + (size_t)object->stride * src_y;
      uint8_t *dst = s->vga.vram_ptr + (size_t)bypl * dst_y;
      size_t length = row_bytes * height;
      if (rop == VMSVGA_ROP_COPY) {
        memcpy(dst, src, length);
      } else if (rop == VMSVGA_ROP_COPY_INVERTED) {
        vmsvga_rop_copy_inverted_buffer(dst, src, length, false);
      } else {
        vmsvga_rop_buffer(dst, src, length, rop, false);
      };
      vmsvga_mark_active_rect_dirty(s, dst_x, dst_y, width, height);
      vmsvga_damage_add(s, dst_x, dst_y, width, height);
      return true;
    };
    for (row = 0; row < height; row++) {
      uint8_t *dst = s->vga.vram_ptr + (size_t)bypl * (dst_y + row) +
                     (size_t)bypp * dst_x;
      if (pattern) {
        const uint8_t *src =
            object->data +
            (size_t)object->stride * ((dst_y + row) % object->height);
        uint32_t source_x = dst_x % object->width;
        uint32_t pattern_pixels = MIN(width, object->width);
        uint32_t tail_pixels =
            MIN(pattern_pixels, object->width - source_x);
        memcpy(source_row, src + (size_t)source_x * bypp,
               (size_t)tail_pixels * bypp);
        if (tail_pixels < pattern_pixels) {
          memcpy(source_row + (size_t)tail_pixels * bypp, src,
                 (size_t)(pattern_pixels - tail_pixels) * bypp);
        };
        vmsvga_repeat_pattern_row(source_row, pattern_pixels, width, bypp);
        if (rop == VMSVGA_ROP_COPY) {
          memcpy(dst, source_row, row_bytes);
        } else if (rop == VMSVGA_ROP_COPY_INVERTED) {
          vmsvga_rop_copy_inverted_buffer(dst, source_row, row_bytes, false);
        } else {
          vmsvga_rop_buffer(dst, source_row, row_bytes, rop, false);
        };
      } else {
        const uint8_t *src = object->data +
                             (size_t)object->stride * (src_y + row) +
                             (size_t)bypp * src_x;
        if (rop == VMSVGA_ROP_COPY) {
          memcpy(dst, src, row_bytes);
        } else if (rop == VMSVGA_ROP_COPY_INVERTED) {
          vmsvga_rop_copy_inverted_buffer(dst, src, row_bytes, false);
        } else {
          vmsvga_rop_buffer(dst, src, row_bytes, rop, false);
        };
      };
    };
    vmsvga_mark_active_rect_dirty(s, dst_x, dst_y, width, height);
    vmsvga_damage_add(s, dst_x, dst_y, width, height);
    return true;
  };
  for (row = 0; row < height; row++) {
    uint32_t object_y =
        pattern ? (dst_y + row) % object->height : src_y + row;
    uint32_t source_pixels = pattern ? MIN(width, object->width) : width;
    uint32_t pattern_x = pattern ? dst_x % object->width : 0;
    uint8_t *dst = s->vga.vram_ptr + (size_t)bypl * (dst_y + row) +
                   (size_t)bypp * dst_x;
    {
      uint32_t object_x = pattern ? pattern_x : src_x;
      for (column = 0; column < source_pixels; column++) {
        uint32_t pixel = vmsvga_object_pixel(object, object_x, object_y);
        if (type == VMSVGA_OBJECT_BITMAP) {
          pixel = pixel ? foreground : background;
        };
        vmsvga_store_pixel(source_row + (size_t)column * bypp, bypp, pixel);
        object_x++;
        if (pattern && object_x == object->width) {
          object_x = 0;
        };
      };
    };
    if (pattern) {
      vmsvga_repeat_pattern_row(source_row, source_pixels, width, bypp);
    };
    if (rop == VMSVGA_ROP_COPY) {
      memcpy(dst, source_row, row_bytes);
    } else if (rop == VMSVGA_ROP_INVERT) {
      vmsvga_rop_invert_buffer(dst, row_bytes);
    } else if (rop == VMSVGA_ROP_COPY_INVERTED) {
      vmsvga_rop_copy_inverted_buffer(dst, source_row, row_bytes, false);
    } else {
      vmsvga_rop_buffer(dst, source_row, row_bytes, rop, false);
    };
  };
  vmsvga_mark_active_rect_dirty(s, dst_x, dst_y, width, height);
  vmsvga_damage_add(s, dst_x, dst_y, width, height);
  return true;
};
static inline uint32_t vmsvga_fifo_read_raw(struct vmsvga_state_s *s);
static inline void vmsvga_fifo_read_raw_data(struct vmsvga_state_s *s,
                                             void *dst, uint32_t words) {
  uint8_t *out = dst;
  size_t bytes = (size_t)words * sizeof(uint32_t);
  while (bytes > 0) {
    size_t chunk = MIN(bytes, (size_t)(s->fifo_max - s->fifo_stop));
    if (out != NULL) {
      memcpy(out, (uint8_t *)s->fifo + s->fifo_stop, chunk);
      out += chunk;
    };
    s->fifo_stop += chunk;
    if (s->fifo_stop == s->fifo_max) {
      s->fifo_stop = s->fifo_min;
    };
    bytes -= chunk;
  };
  s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
};
static inline void vmsvga_fifo_peek_raw_data(struct vmsvga_state_s *s,
                                             size_t byte_offset, void *dst,
                                             size_t bytes) {
  uint8_t *out = dst;
  size_t ring_size = s->fifo_max - s->fifo_min;
  uint32_t pos;
  if (bytes == 0 || ring_size == 0) {
    return;
  };
  byte_offset %= ring_size;
  pos = s->fifo_stop + byte_offset;
  if (pos >= s->fifo_max) {
    pos = s->fifo_min + (pos - s->fifo_max);
  };
  while (bytes > 0) {
    size_t chunk = MIN(bytes, (size_t)(s->fifo_max - pos));
    memcpy(out, (uint8_t *)s->fifo + pos, chunk);
    out += chunk;
    pos += chunk;
    if (pos == s->fifo_max) {
      pos = s->fifo_min;
    };
    bytes -= chunk;
  };
};
static inline void vmsvga_fifo_skip(struct vmsvga_state_s *s,
                                    uint32_t words) {
  vmsvga_fifo_read_raw_data(s, NULL, words);
};
static inline void vmsvga_fifo_read_object_data(struct vmsvga_state_s *s,
                                                uint8_t *dst,
                                                uint32_t words) {
  vmsvga_fifo_read_raw_data(s, dst, words);
};
static inline void vmsvga_fifo_upload_reset(struct vmsvga_state_s *s) {
  memset(&s->fifo_upload, 0, sizeof(s->fifo_upload));
};
static inline bool vmsvga_fifo_upload_begin(struct vmsvga_state_s *s,
                                            uint32_t type, uint32_t id,
                                            uint32_t width, uint32_t height,
                                            uint32_t depth) {
  struct vmsvga_object_s *object;
  uint32_t stride;
  size_t size;
  if (!vmsvga_object_layout(type, width, height, depth, &stride, &size) ||
      size / sizeof(uint32_t) > UINT32_MAX) {
    return false;
  };
  object = vmsvga_object_create(s, id, type, width, height, depth);
  s->fifo_upload.active = true;
  s->fifo_upload.discard = object == NULL;
  s->fifo_upload.type = type;
  s->fifo_upload.id = id;
  s->fifo_upload.total_words = (uint32_t)(size / sizeof(uint32_t));
  s->fifo_upload.received_words = 0;
  VMVGA_TRACE_LOCAL(
      VMVGA_TRACE_STREAM,
      "STREAM_START type=%u id=%u width=%u height=%u depth=%u words=%u "
      "discard=%u",
      type, id, width, height, depth, s->fifo_upload.total_words,
      s->fifo_upload.discard);
  return true;
};
static inline uint32_t vmsvga_fifo_upload_consume(struct vmsvga_state_s *s,
                                                  uint32_t available_words) {
  struct vmsvga_fifo_upload_s *upload = &s->fifo_upload;
  struct vmsvga_object_s *object = NULL;
  uint32_t remaining;
  uint32_t words;
  uint32_t offset_words;
  if (!upload->active || upload->received_words > upload->total_words) {
    return 0;
  };
  remaining = upload->total_words - upload->received_words;
  words = MIN(remaining, available_words);
  if (words == 0) {
    return 0;
  };
  offset_words = upload->received_words;
  if (!upload->discard) {
    object = vmsvga_object_get(s, upload->id, upload->type);
    if (object == NULL ||
        object->size / sizeof(uint32_t) != upload->total_words) {
      upload->discard = true;
    };
  };
  if (upload->discard) {
    vmsvga_fifo_skip(s, words);
  } else {
    vmsvga_fifo_read_object_data(
        s, object->data + (size_t)offset_words * sizeof(uint32_t), words);
  };
  upload->received_words += words;
  VMVGA_TRACE_LOCAL(
      VMVGA_TRACE_STREAM,
      "STREAM_PROGRESS type=%u id=%u consumed=%u done=%u total=%u discard=%u",
      upload->type, upload->id, words, upload->received_words,
      upload->total_words, upload->discard);
  if (upload->received_words == upload->total_words) {
    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_STREAM,
        "STREAM_COMPLETE type=%u id=%u words=%u discard=%u",
        upload->type, upload->id, upload->total_words, upload->discard);
    vmsvga_fifo_upload_reset(s);
  };
  return words;
};
static inline bool vmsvga_fifo_upload_valid(struct vmsvga_state_s *s) {
  struct vmsvga_fifo_upload_s *upload = &s->fifo_upload;
  struct vmsvga_object_s *object;
  if (!upload->active) {
    return !upload->discard && upload->type == 0 && upload->id == 0 &&
           upload->total_words == 0 && upload->received_words == 0;
  };
  if ((upload->type != VMSVGA_OBJECT_BITMAP &&
       upload->type != VMSVGA_OBJECT_PIXMAP) ||
      upload->total_words == 0 ||
      upload->received_words >= upload->total_words) {
    return false;
  };
  if (upload->discard) {
    return true;
  };
  object = vmsvga_object_get(s, upload->id, upload->type);
  return object != NULL &&
         object->size / sizeof(uint32_t) == upload->total_words;
};
struct vmsvga_cursor_definition_s {
  uint32_t width;
  uint32_t height;
  uint32_t id;
  uint32_t hot_x;
  uint32_t hot_y;
  uint32_t and_mask_bpp;
  uint32_t xor_mask_bpp;
  uint32_t and_words;
  uint32_t xor_words;
  uint8_t *and_mask;
  uint8_t *xor_mask;
};
static inline void
vmsvga_cursor_definition_clear(struct vmsvga_cursor_definition_s *cursor) {
  g_clear_pointer(&cursor->and_mask, g_free);
  g_clear_pointer(&cursor->xor_mask, g_free);
  memset(cursor, 0, sizeof(*cursor));
};
static inline uint32_t vmsvga_cursor_row_bytes(uint32_t width, uint32_t bpp) {
  uint64_t storage_bpp = bpp == 15 ? 16 : bpp;
  uint64_t bits = (uint64_t)width * storage_bpp;
  return (uint32_t)(((bits + 31) >> 5) * sizeof(uint32_t));
};
static inline void vmsvga_cursor_cache_remove(struct vmsvga_state_s *s,
                                               uint32_t id) {
  if (id < VMSVGA_MAX_CURSORS && s->cursor_cache[id] != NULL) {
    vmvga_cursor_unref(s->cursor_cache[id]);
    s->cursor_cache[id] = NULL;
  };
};
static inline void vmsvga_cursor_cache_clear(struct vmsvga_state_s *s) {
  uint32_t id;
  for (id = 0; id < VMSVGA_MAX_CURSORS; id++) {
    vmsvga_cursor_cache_remove(s, id);
  };
};
static inline void vmsvga_cursor_source_free(struct vmsvga_cursor_source_s *src) {
  if (src == NULL) {
    return;
  };
  g_free(src->and_data);
  g_free(src->xor_data);
  g_free(src);
};
static inline void vmsvga_cursor_source_clear(struct vmsvga_state_s *s) {
  uint32_t id;
  for (id = 0; id < VMSVGA_MAX_CURSORS; id++) {
    vmsvga_cursor_source_free(s->cursor_source[id]);
    s->cursor_source[id] = NULL;
  };
};
static inline QEMUCursor *vmsvga_cursor_cache_get(struct vmsvga_state_s *s,
                                                  uint32_t id) {
  if (id >= VMSVGA_MAX_CURSORS) {
    return NULL;
  };
  return s->cursor_cache[id];
};
static inline void vmsvga_cursor_select(struct vmsvga_state_s *s,
                                        uint32_t id) {
  QEMUCursor *qc = vmsvga_cursor_cache_get(s, id);
  if (qc != NULL) {
    vmvga_console_set_cursor(s->vga.con, qc);
  };
};
static inline void vmsvga_cursor_commit_indexed(struct vmsvga_state_s *s) {
  bool id_changed = s->active_cursor != s->cursor;
  s->active_cursor = s->cursor;
  s->active_cursor_x = s->cursor_x;
  s->active_cursor_y = s->cursor_y;
  s->active_cursor_on = s->cursor_on;
  if (id_changed) {
    vmsvga_cursor_select(s, s->active_cursor);
  };
  /* CURSOR_ON is the indexed bypass commit point. Applying it immediately
   * preserves the asynchronous cursor-bypass contract even if the ON value
   * itself did not change. */
  s->cursor_dirty = true;
  vmsvga_cursor_apply(s);
};
static inline void vmsvga_cursor_test_flip(struct vmsvga_state_s *s,
                                             QEMUCursor *qc, uint32_t width,
                                             uint32_t height) {
  uint32_t y;
  if (!s->test_marker || width == 0 || height == 0) {
    return;
  };
  for (y = 0; y < height / 2; y++) {
    uint32_t *top = qc->data + (size_t)y * width;
    uint32_t *bottom = qc->data + (size_t)(height - 1 - y) * width;
    uint32_t x;
    for (x = 0; x < width; x++) {
      uint32_t tmp = top[x];
      top[x] = bottom[x];
      bottom[x] = tmp;
    };
  };
  if (qc->hot_y < height) {
    qc->hot_y = height - 1 - qc->hot_y;
  };
};
static inline void vmsvga_cursor_cache_put(struct vmsvga_state_s *s,
                                           uint32_t id, QEMUCursor *qc) {
  if (id >= VMSVGA_MAX_CURSORS) {
    vmvga_cursor_unref(qc);
    return;
  };
  vmsvga_cursor_cache_remove(s, id);
  s->cursor_cache[id] = qc;
  if (s->active_cursor == id) {
    vmvga_console_set_cursor(s->vga.con, qc);
  };
};
static inline uint8_t vmsvga_cursor_bit(const uint8_t *row, uint32_t x) {
  return !!(row[x >> 3] & (0x80u >> (x & 7)));
};
static inline uint32_t vmsvga_qemu_rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
};
static inline uint32_t vmsvga_cursor_color(struct vmsvga_state_s *s,
                                            const uint8_t *data,
                                            uint32_t row_bytes, uint32_t bpp,
                                            uint32_t x, uint32_t y) {
  const uint8_t *row = data + (size_t)row_bytes * y;
  switch (bpp) {
  case 1:
    return vmsvga_cursor_bit(row, x) ? 0x00ffffff : 0;
  case 8: {
    uint32_t base = (uint32_t)row[x] * 3;
    uint8_t r = s->svgapalettebase[base] & 0xff;
    uint8_t g = s->svgapalettebase[base + 1] & 0xff;
    uint8_t b = s->svgapalettebase[base + 2] & 0xff;
    return vmsvga_qemu_rgb(r, g, b);
  }
  case 15: {
    uint16_t v = (uint16_t)row[x * 2] | ((uint16_t)row[x * 2 + 1] << 8);
    uint8_t r = ((v >> 10) & 0x1f) << 3;
    uint8_t g = ((v >> 5) & 0x1f) << 3;
    uint8_t b = (v & 0x1f) << 3;
    return vmsvga_qemu_rgb(r, g, b);
  }
  case 16: {
    uint16_t v = (uint16_t)row[x * 2] | ((uint16_t)row[x * 2 + 1] << 8);
    uint8_t r = ((v >> 11) & 0x1f) << 3;
    uint8_t g = ((v >> 5) & 0x3f) << 2;
    uint8_t b = (v & 0x1f) << 3;
    return vmsvga_qemu_rgb(r, g, b);
  }
  case 24:
    return vmsvga_qemu_rgb(row[x * 3 + 2], row[x * 3 + 1], row[x * 3]);
  case 32:
    return vmsvga_qemu_rgb(row[x * 4 + 2], row[x * 4 + 1], row[x * 4]);
  default:
    return 0;
  };
};
static inline uint32_t vmsvga_cursor_raw_pixel(const uint8_t *data,
                                                uint32_t row_bytes,
                                                uint32_t bpp, uint32_t x,
                                                uint32_t y) {
  const uint8_t *row = data + (size_t)row_bytes * y;
  switch (bpp) {
  case 1:
    return vmsvga_cursor_bit(row, x);
  case 8:
    return row[x];
  case 15:
  case 16:
    return (uint32_t)row[x * 2] | ((uint32_t)row[x * 2 + 1] << 8);
  case 24:
    return (uint32_t)row[x * 3] | ((uint32_t)row[x * 3 + 1] << 8) |
           ((uint32_t)row[x * 3 + 2] << 16);
  case 32:
    return (uint32_t)row[x * 4] | ((uint32_t)row[x * 4 + 1] << 8) |
           ((uint32_t)row[x * 4 + 2] << 16) |
           ((uint32_t)row[x * 4 + 3] << 24);
  default:
    return 0;
  };
};
static inline uint32_t vmsvga_cursor_all_ones(uint32_t bpp) {
  switch (bpp) {
  case 1:
    return 1;
  case 8:
    return 0xff;
  case 15:
    return 0x7fff;
  case 16:
    return 0xffff;
  case 24:
    return 0x00ffffff;
  case 32:
    return 0xffffffff;
  default:
    return 0;
  };
};
static inline bool
vmsvga_cursor_source_palette_dependent(const struct vmsvga_cursor_source_s *src) {
  return src != NULL && !src->alpha &&
         (src->and_mask_bpp == 8 || src->xor_mask_bpp == 8);
};
static inline bool vmsvga_cursor_render_source(struct vmsvga_state_s *s,
                                                uint32_t id) {
  struct vmsvga_cursor_source_s *src;
  QEMUCursor *qc;
  uint32_t x;
  uint32_t y;
  if (id >= VMSVGA_MAX_CURSORS || (src = s->cursor_source[id]) == NULL) {
    return false;
  };
  qc = cursor_alloc(src->width, src->height);
  if (qc == NULL) {
    return false;
  };
  qc->hot_x = src->hot_x;
  qc->hot_y = src->hot_y;
  if (src->alpha) {
    uint32_t row_bytes = vmsvga_cursor_row_bytes(src->width, 32);
    for (y = 0; y < src->height; y++) {
      const uint8_t *row = src->xor_data + (size_t)row_bytes * y;
      for (x = 0; x < src->width; x++) {
        uint8_t b = row[x * 4];
        uint8_t g = row[x * 4 + 1];
        uint8_t r = row[x * 4 + 2];
        uint8_t a = row[x * 4 + 3];
        qc->data[(size_t)y * src->width + x] =
            ((uint32_t)a << 24) | vmsvga_qemu_rgb(r, g, b);
      };
    };
  } else if (src->and_mask_bpp == 1 && src->xor_mask_bpp == 1) {
    uint32_t src_and_bpl = vmsvga_cursor_row_bytes(src->width, 1);
    uint32_t src_xor_bpl = vmsvga_cursor_row_bytes(src->width, 1);
    uint32_t dst_bpl = (src->width + 7) >> 3;
    size_t size = (size_t)dst_bpl * src->height;
    uint8_t *and_tight = g_try_malloc0(size);
    uint8_t *xor_tight = g_try_malloc0(size);
    if (and_tight == NULL || xor_tight == NULL) {
      g_free(and_tight);
      g_free(xor_tight);
      vmvga_cursor_unref(qc);
      return false;
    };
    for (y = 0; y < src->height; y++) {
      memcpy(and_tight + (size_t)y * dst_bpl,
             src->and_data + (size_t)y * src_and_bpl, dst_bpl);
      memcpy(xor_tight + (size_t)y * dst_bpl,
             src->xor_data + (size_t)y * src_xor_bpl, dst_bpl);
    };
    cursor_set_mono(qc, 0xffffff, 0x000000, xor_tight, 1, and_tight);
    g_free(and_tight);
    g_free(xor_tight);
  } else {
    uint32_t and_bpl = vmsvga_cursor_row_bytes(src->width, src->and_mask_bpp);
    uint32_t xor_bpl = vmsvga_cursor_row_bytes(src->width, src->xor_mask_bpp);
    uint32_t and_ones = vmsvga_cursor_all_ones(src->and_mask_bpp);
    for (y = 0; y < src->height; y++) {
      for (x = 0; x < src->width; x++) {
        uint32_t and_raw = vmsvga_cursor_raw_pixel(
            src->and_data, and_bpl, src->and_mask_bpp, x, y);
        uint32_t xor_raw = vmsvga_cursor_raw_pixel(
            src->xor_data, xor_bpl, src->xor_mask_bpp, x, y);
        uint32_t color = vmsvga_cursor_color(
            s, src->xor_data, xor_bpl, src->xor_mask_bpp, x, y);
        uint32_t *dst = &qc->data[(size_t)y * src->width + x];
        if (and_raw == and_ones && xor_raw == 0) {
          *dst = 0;
        } else {
          /*
           * Arbitrary color AND/XOR operations can depend on the pixels
           * underneath the cursor, while QEMU's generic cursor frontend is
           * RGBA-based. Preserve the exact transparent/source-color cases;
           * background-dependent color-XOR cases use the XOR color as the
           * closest host-cursor representation. Monochrome 1/1 cursors use
           * cursor_set_mono() above, including QEMU's inversion handling.
           */
          *dst = 0xff000000u | color;
        };
      };
    };
  };
#ifdef VERBOSE
  cursor_print_ascii_art(qc, src->alpha ? "vmsvga_alpha" : "vmsvga_cursor");
#endif
  vmsvga_cursor_test_flip(s, qc, src->width, src->height);
  vmsvga_cursor_cache_put(s, id, qc);
  return true;
};
static inline bool vmsvga_cursor_source_set(
    struct vmsvga_state_s *s, const struct vmsvga_cursor_definition_s *c,
    bool alpha) {
  struct vmsvga_cursor_source_s *src;
  size_t and_size = (size_t)c->and_words * sizeof(uint32_t);
  size_t xor_size = (size_t)c->xor_words * sizeof(uint32_t);
  if (c->id >= VMSVGA_MAX_CURSORS || xor_size > UINT32_MAX ||
      and_size > UINT32_MAX) {
    return false;
  };
  src = g_try_new0(struct vmsvga_cursor_source_s, 1);
  if (src == NULL) {
    return false;
  };
  if (and_size != 0) {
    src->and_data = g_try_malloc(and_size);
    if (src->and_data == NULL) {
      vmsvga_cursor_source_free(src);
      return false;
    };
    memcpy(src->and_data, c->and_mask, and_size);
  };
  if (xor_size != 0) {
    src->xor_data = g_try_malloc(xor_size);
    if (src->xor_data == NULL) {
      vmsvga_cursor_source_free(src);
      return false;
    };
    memcpy(src->xor_data, c->xor_mask, xor_size);
  };
  src->alpha = alpha;
  src->width = c->width;
  src->height = c->height;
  src->hot_x = c->hot_x;
  src->hot_y = c->hot_y;
  src->and_mask_bpp = c->and_mask_bpp;
  src->xor_mask_bpp = c->xor_mask_bpp;
  src->and_size = and_size;
  src->xor_size = xor_size;
  vmsvga_cursor_source_free(s->cursor_source[c->id]);
  s->cursor_source[c->id] = src;
  vmsvga_cursor_cache_remove(s, c->id);
  return vmsvga_cursor_render_source(s, c->id);
};
static inline void vmsvga_cursor_palette_changed(struct vmsvga_state_s *s) {
  uint32_t id;
  for (id = 0; id < VMSVGA_MAX_CURSORS; id++) {
    if (vmsvga_cursor_source_palette_dependent(s->cursor_source[id])) {
      vmsvga_cursor_render_source(s, id);
    };
  };
};
static inline void vmsvga_cursor_define(struct vmsvga_state_s *s,
                                        struct vmsvga_cursor_definition_s *c) {
  VPRINT("vmsvga_cursor_define was just executed\n");
  if (!vmsvga_cursor_source_set(s, c, false)) {
    VPRINT("vmsvga_cursor_define failed to cache/render cursor %u\n", c->id);
  };
};
static inline void
vmsvga_rgba_cursor_define(struct vmsvga_state_s *s,
                          struct vmsvga_cursor_definition_s *c) {
  VPRINT("vmsvga_rgba_cursor_define was just executed\n");
  if (!vmsvga_cursor_source_set(s, c, true)) {
    VPRINT("vmsvga_rgba_cursor_define failed to cache/render cursor %u\n",
           c->id);
  };
};
static inline int vmsvga_fifo_length(struct vmsvga_state_s *s) {
  VPRINT("vmsvga_fifo_length was just executed\n");
  int num;
  if (!s->enable || !s->config || s->fifo == NULL) {
    return 0;
  };
  s->fifo_next = le32_to_cpu(s->fifo[SVGA_FIFO_NEXT_CMD]);
  s->fifo_stop = le32_to_cpu(s->fifo[SVGA_FIFO_STOP]);
  if (s->fifo_next == s->fifo_stop) {
    return 0;
  };
  s->fifo_min = le32_to_cpu(s->fifo[SVGA_FIFO_MIN]);
  s->fifo_max = le32_to_cpu(s->fifo[SVGA_FIFO_MAX]);
  if ((s->fifo_min | s->fifo_max | s->fifo_next | s->fifo_stop) & 3) {
    return 0;
  };
  if (s->fifo_min < sizeof(uint32_t) * 4 || s->fifo_min >= s->fifo_max ||
      s->fifo_max > s->fifo_size || s->fifo_next < s->fifo_min ||
      s->fifo_next >= s->fifo_max || s->fifo_stop < s->fifo_min ||
      s->fifo_stop >= s->fifo_max || s->fifo_max - s->fifo_min < 10 * 1024) {
    return 0;
  };
  num = (int)s->fifo_next - (int)s->fifo_stop;
  if (num < 0) {
    num += s->fifo_max - s->fifo_min;
  };
  VPRINT("fifo_min: %u, fifo_max: %u, fifo_next: %u, fifo_stop: %u, num: %d, "
         "ret: %d\n",
         s->fifo_min, s->fifo_max, s->fifo_next, s->fifo_stop, num,
         num / (int)sizeof(uint32_t));
  return num / sizeof(uint32_t);
};
static inline bool vmsvga_fifo_pending(struct vmsvga_state_s *s) {
  if (!s->enable || !s->config || s->fifo == NULL) {
    return false;
  };
  return le32_to_cpu(s->fifo[SVGA_FIFO_NEXT_CMD]) !=
         le32_to_cpu(s->fifo[SVGA_FIFO_STOP]);
};
static inline uint32_t vmsvga_fifo_read_raw(struct vmsvga_state_s *s) {
  VPRINT("vmsvga_fifo_read_raw was just executed\n");
  uint32_t cmd = s->fifo[s->fifo_stop / sizeof(uint32_t)];
  s->fifo_stop += 4;
  if (s->fifo_stop >= s->fifo_max) {
    s->fifo_stop = s->fifo_min;
  };
  s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
  VPRINT("vmsvga_fifo_read_raw: cmd: %u\n", cmd);
  return cmd;
};
static inline uint32_t vmsvga_fifo_read(struct vmsvga_state_s *s) {
  VPRINT("vmsvga_fifo_read was just executed\n");
  uint32_t ret = le32_to_cpu(vmsvga_fifo_read_raw(s));
  VPRINT("vmsvga_fifo_read: ret: %u\n", ret);
  return ret;
};
static inline void vmsvga_draw_glyph(struct vmsvga_state_s *s, uint32_t x,
                                     uint32_t y, uint32_t w, uint32_t h,
                                     uint32_t foreground, uint32_t background,
                                     bool clipped, uint32_t clip_x,
                                     uint32_t clip_y, uint32_t clip_w,
                                     uint32_t clip_h, uint32_t payload_words) {
  uint32_t bypl = vmsvga_stride(s);
  uint32_t bypp = vmsvga_bytes_per_pixel(vmsvga_active_depth(s));
  uint32_t surface_width_px = vmsvga_active_width(s);
  uint32_t surface_height_px = vmsvga_active_height(s);
  uint64_t left = x;
  uint64_t top = y;
  uint64_t right = (uint64_t)x + w;
  uint64_t bottom = (uint64_t)y + h;
  uint8_t background_col[4];
  uint8_t *scanline;
  uint32_t row;
  bool background_transparent = !clipped || background == UINT32_MAX;
  bool changed = false;
  if (clipped) {
    uint64_t clip_right = (uint64_t)clip_x + clip_w;
    uint64_t clip_bottom = (uint64_t)clip_y + clip_h;
    if (left < clip_x) {
      left = clip_x;
    };
    if (top < clip_y) {
      top = clip_y;
    };
    if (right > clip_right) {
      right = clip_right;
    };
    if (bottom > clip_bottom) {
      bottom = clip_bottom;
    };
  };
  if (right > surface_width_px) {
    right = surface_width_px;
  };
  if (bottom > surface_height_px) {
    bottom = surface_height_px;
  };
  if (w == 0 || h == 0 || w > VMSVGA_MAX_WIDTH || h > VMSVGA_MAX_HEIGHT ||
      bypp < 1 || bypp > 4 || left >= right || top >= bottom) {
    vmsvga_fifo_skip(s, payload_words);
    return;
  };
  scanline = s->blit_scratch;
  background_col[0] = background;
  background_col[1] = background >> 8;
  background_col[2] = background >> 16;
  background_col[3] = background >> 24;
  for (row = 0; row < h; row++) {
    uint64_t dst_y = (uint64_t)y + row;
    size_t row_bytes = ((size_t)w + 7) >> 3;
    uint32_t bit_shift = 0;
    vmsvga_fifo_peek_raw_data(s, (size_t)row * row_bytes, scanline,
                              row_bytes);
    if (dst_y < top || dst_y >= bottom) {
      continue;
    };
    {
      uint32_t first_x = (uint32_t)left;
      uint32_t last_x = (uint32_t)right;
      uint32_t first_bit = bit_shift + first_x - x;
      uint32_t last_bit = bit_shift + last_x - x;
      uint8_t *dst = s->vga.vram_ptr + (size_t)bypl * (uint32_t)dst_y +
                     (size_t)bypp * first_x;
      if (!background_transparent) {
        vmsvga_fill_pattern(dst, (size_t)(last_x - first_x) * bypp,
                            background_col, bypp);
        changed = true;
      };
      changed |= vmsvga_mono_overlay(dst, scanline, first_bit, last_bit, bypp,
                                     foreground);
    };
  };
  vmsvga_fifo_skip(s, payload_words);
  if (changed) {
    vmsvga_mark_active_rect_dirty(
        s, (uint32_t)left, (uint32_t)top, (uint32_t)(right - left),
        (uint32_t)(bottom - top));
    vmsvga_damage_add(s, (uint32_t)left, (uint32_t)top,
                      (uint32_t)(right - left), (uint32_t)(bottom - top));
  };
};
typedef struct {
  uint32 color;
  uint32 x;
  uint32 y;
  uint32 width;
  uint32 height;
} SVGAFifoCmdRectFill;
typedef struct {
  uint32 bitmapId;
  uint32 width;
  uint32 height;
} SVGAFifoCmdDefineBitmap;
typedef struct {
  uint32 bitmapId;
  uint32 width;
  uint32 height;
  uint32 lineNumber;
} SVGAFifoCmdDefineBitmapScanline;
typedef struct {
  uint32 pixmapId;
  uint32 width;
  uint32 height;
  uint32 depth;
} SVGAFifoCmdDefinePixmap;
typedef struct {
  uint32 pixmapId;
  uint32 width;
  uint32 height;
  uint32 depth;
  uint32 lineNumber;
} SVGAFifoCmdDefinePixmapScanline;
typedef struct {
  uint32 bitmapId;
  uint32 x;
  uint32 y;
  uint32 width;
  uint32 height;
  uint32 foreground;
  uint32 background;
} SVGAFifoCmdRectBitmapFill;
typedef struct {
  uint32 pixmapId;
  uint32 x;
  uint32 y;
  uint32 width;
  uint32 height;
} SVGAFifoCmdRectPixmapFill;
typedef struct {
  uint32 bitmapId;
  uint32 srcX;
  uint32 srcY;
  uint32 destX;
  uint32 destY;
  uint32 width;
  uint32 height;
  uint32 foreground;
  uint32 background;
} SVGAFifoCmdRectBitmapCopy;
typedef struct {
  uint32 pixmapId;
  uint32 srcX;
  uint32 srcY;
  uint32 destX;
  uint32 destY;
  uint32 width;
  uint32 height;
} SVGAFifoCmdRectPixmapCopy;
typedef struct {
  uint32 id;
} SVGAFifoCmdFreeObject;
typedef struct {
  uint32 color;
  uint32 x;
  uint32 y;
  uint32 width;
  uint32 height;
  uint32 rop;
} SVGAFifoCmdRectRopFill;
typedef struct {
  uint32 bitmapId;
  uint32 x;
  uint32 y;
  uint32 width;
  uint32 height;
  uint32 foreground;
  uint32 background;
  uint32 rop;
} SVGAFifoCmdRectRopBitmapFill;
typedef struct {
  uint32 pixmapId;
  uint32 x;
  uint32 y;
  uint32 width;
  uint32 height;
  uint32 rop;
} SVGAFifoCmdRectRopPixmapFill;
typedef struct {
  uint32 bitmapId;
  uint32 srcX;
  uint32 srcY;
  uint32 destX;
  uint32 destY;
  uint32 width;
  uint32 height;
  uint32 foreground;
  uint32 background;
  uint32 rop;
} SVGAFifoCmdRectRopBitmapCopy;
typedef struct {
  uint32 pixmapId;
  uint32 srcX;
  uint32 srcY;
  uint32 destX;
  uint32 destY;
  uint32 width;
  uint32 height;
  uint32 rop;
} SVGAFifoCmdRectRopPixmapCopy;
typedef struct {
  uint32 id;
  uint32 display;
} SVGAFifoCmdDisplayCursor;
typedef struct {
  uint32 x;
  uint32 y;
} SVGAFifoCmdMoveCursor;
typedef struct {
  uint32 x;
  uint32 y;
  uint32 w;
  uint32 h;
  uint32 fgColor;
} SVGAFifoCmdDrawGlyph;
typedef struct {
  uint32 x;
  uint32 y;
  uint32 w;
  uint32 h;
  uint32 fgColor;
  uint32 bgColor;
  uint32 clipX;
  uint32 clipY;
  uint32 clipW;
  uint32 clipH;
} SVGAFifoCmdDrawGlyphClipped;
typedef struct {
  uint32 color;
  uint32 dstSurfaceOffset;
  uint32 x;
  uint32 y;
  uint32 width;
  uint32 height;
  uint32 rop;
} SVGAFifoCmdSurfaceFill;
typedef struct {
  uint32 srcSurfaceOffset;
  uint32 dstSurfaceOffset;
  uint32 srcX;
  uint32 srcY;
  uint32 destX;
  uint32 destY;
  uint32 width;
  uint32 height;
  uint32 rop;
} SVGAFifoCmdSurfaceCopy;
typedef struct {
  uint32 srcSurfaceOffset;
  uint32 dstSurfaceOffset;
  uint32 srcX;
  uint32 srcY;
  uint32 destX;
  uint32 destY;
  uint32 width;
  uint32 height;
  uint32 blendOp;
  uint32 flags;
  uint32 param1;
  uint32 param2;
} SVGAFifoCmdSurfaceAlphaBlend;
#include "vmware_vga_gmr.c"
#include "vmware_vga_3d.c"
static void vmsvga_fifo_run(struct vmsvga_state_s *s, bool flush_damage) {
  VPRINT("vmsvga_fifo_run was just executed\n");
  int32_t len;
  uint32_t cmd;
  uint32_t fence_arg;
  uint32_t irq_status;
  uint32_t fifo_start;
  /*
   * Keep each processing pass bounded. FIFO processing itself is independent
   * of SVGA_REG_BUSY; an explicit SYNC request is tracked separately in
   * s->sync and BUSY polling merely asks us to run another bounded pass.
   */
  uint32_t maxloop = 1024;
  struct vmsvga_cursor_definition_s cursor = {0};
  len = vmsvga_fifo_length(s);
  if (vmsvga_trace_flight_enabled()) {
    s->trace_now.fifo_runs++;
  };
  if (len < 1) {
    if (vmsvga_trace_flight_enabled()) {
      s->trace_now.fifo_empty_runs++;
    };
    if (flush_damage) {
      vmsvga_damage_flush(s);
    };
    cursor_update_from_fifo(s);
    s->sync = 0;
    if (vmsvga_fifo_has_reg(s, SVGA_FIFO_BUSY)) {
      s->fifo[SVGA_FIFO_BUSY] = cpu_to_le32(0);
    };
    return;
  };
  if (vmsvga_fifo_has_reg(s, SVGA_FIFO_BUSY)) {
    s->fifo[SVGA_FIFO_BUSY] = cpu_to_le32(1);
  };
  while ((len >= 1) && maxloop > 0) {
    maxloop--;
    if (s->fifo_upload.active) {
      uint32_t consumed;
      fifo_start = s->fifo_stop;
      consumed = vmsvga_fifo_upload_consume(s, (uint32_t)len);
      len -= (int32_t)consumed;
      if (s->fifo_stop != fifo_start) {
        irq_status =
            SVGA_IRQFLAG_FIFO_PROGRESS & s->irq_mask & ~s->irq_status;
        if (irq_status) {
          s->irq_status |= irq_status;
#ifndef RAISE_IRQ_OFF
          if (irq_status & s->irq_mask) {
            struct pci_vmsvga_state_s *pci_vmsvga =
                container_of(s, struct pci_vmsvga_state_s, chip);
            pci_set_irq(PCI_DEVICE(pci_vmsvga), 1);
          };
#endif
        };
      };
      continue;
    };
    fifo_start = s->fifo_stop;
    cmd = vmsvga_fifo_read(s);
    {
      bool is_3d = cmd >= SVGA_3D_CMD_BASE && cmd < SVGA_3D_CMD_MAX;
      bool supported_3d = is_3d && vmsvga3d_fifo_supported_command(cmd);
      vmsvga_trace_fifo_record(s, cmd, is_3d, supported_3d);
    };
    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_FIFO,
        "FIFO cmd=%u stop=0x%08x next=0x%08x words=%d sync=%u",
        cmd, fifo_start, s->fifo_next, len, s->sync);
    irq_status = 0;
    /*
     * Do not pre-filter commands by numeric value here.  Extended 2D FIFO
     * commands (ESCAPE, screen objects, GMRFB, GMR2, ...) live above
     * SVGA_CMD_FENCE, while SVGA3D has its own command namespace beginning at
     * SVGA_3D_CMD_BASE.  The old `cmd > SVGA_CMD_FENCE` gate therefore
     * rejected legitimate non-3D commands before the switch below could
     * handle them.
     *
     * Unknown commands are still safe: extended 2D commands fall through to
     * the explicit switch cases below, and SVGA3D commands are validated by
     * vmsvga3d_fifo_command().  Anything neither layer recognizes is rewound
     * by the default path, preserving the existing stall semantics.
     */
    VPRINT("Unknown command %u in SVGA command FIFO\n", cmd);
    switch (cmd) {
    case SVGA_CMD_INVALID_CMD:
      if (len < 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      s->fifo_stop = fifo_start;
      s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
      len = 0;
      VPRINT("SVGA_CMD_INVALID_CMD command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_DEFINE_BITMAP: {
      uint32_t id;
      uint32_t width;
      uint32_t height;
      uint32_t consumed;
      if (len < 4) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      if (!vmsvga_fifo_upload_begin(s, VMSVGA_OBJECT_BITMAP, id, width, height,
                                    1)) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      len -= 4;
      consumed = vmsvga_fifo_upload_consume(s, (uint32_t)len);
      len -= (int32_t)consumed;
      VPRINT("SVGA_CMD_DEFINE_BITMAP command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_DEFINE_BITMAP_SCANLINE: {
      uint32_t id;
      uint32_t width;
      uint32_t height;
      uint32_t line_number;
      uint32_t stride;
      size_t size;
      uint32_t payload_words;
      struct vmsvga_object_s *object;
      if (len < 5) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      line_number = vmsvga_fifo_read(s);
      if (!vmsvga_object_layout(VMSVGA_OBJECT_BITMAP, width, height, 1,
                                &stride, &size) || stride / 4 > INT32_MAX ||
          len < 5 + (int32_t)(stride / 4)) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      payload_words = stride / 4;
      len -= 5 + (int32_t)payload_words;
      object = vmsvga_object_get(s, id, VMSVGA_OBJECT_BITMAP);
      if (line_number < height &&
          !vmsvga_object_matches(object, VMSVGA_OBJECT_BITMAP, width, height,
                                 1)) {
        object = vmsvga_object_create(s, id, VMSVGA_OBJECT_BITMAP, width,
                                      height, 1);
      };
      if (object != NULL && line_number < height) {
        vmsvga_fifo_read_object_data(
            s, object->data + (size_t)object->stride * line_number,
            payload_words);
      } else {
        vmsvga_fifo_skip(s, payload_words);
      };
      VPRINT("SVGA_CMD_DEFINE_BITMAP_SCANLINE command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_DEFINE_PIXMAP: {
      uint32_t id;
      uint32_t width;
      uint32_t height;
      uint32_t depth;
      uint32_t consumed;
      if (len < 5) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      depth = vmsvga_fifo_read(s);
      if (!vmsvga_fifo_upload_begin(s, VMSVGA_OBJECT_PIXMAP, id, width, height,
                                    depth)) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      len -= 5;
      consumed = vmsvga_fifo_upload_consume(s, (uint32_t)len);
      len -= (int32_t)consumed;
      VPRINT("SVGA_CMD_DEFINE_PIXMAP command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_DEFINE_PIXMAP_SCANLINE: {
      uint32_t id;
      uint32_t width;
      uint32_t height;
      uint32_t depth;
      uint32_t line_number;
      uint32_t stride;
      size_t size;
      uint32_t payload_words;
      struct vmsvga_object_s *object;
      if (len < 6) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      depth = vmsvga_fifo_read(s);
      line_number = vmsvga_fifo_read(s);
      if (!vmsvga_object_layout(VMSVGA_OBJECT_PIXMAP, width, height, depth,
                                &stride, &size) || stride / 4 > INT32_MAX ||
          len < 6 + (int32_t)(stride / 4)) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      payload_words = stride / 4;
      len -= 6 + (int32_t)payload_words;
      object = vmsvga_object_get(s, id, VMSVGA_OBJECT_PIXMAP);
      if (line_number < height &&
          !vmsvga_object_matches(object, VMSVGA_OBJECT_PIXMAP, width, height,
                                 depth)) {
        object = vmsvga_object_create(s, id, VMSVGA_OBJECT_PIXMAP, width,
                                      height, depth);
      };
      if (object != NULL && line_number < height) {
        vmsvga_fifo_read_object_data(
            s, object->data + (size_t)object->stride * line_number,
            payload_words);
      } else {
        vmsvga_fifo_skip(s, payload_words);
      };
      VPRINT("SVGA_CMD_DEFINE_PIXMAP_SCANLINE command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_DISPLAY_CURSOR: {
      uint32_t id;
      uint32_t display;
      if (len < (sizeof(SVGAFifoCmdDisplayCursor) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdDisplayCursor) / sizeof(uint32_t) + 1;
      id = vmsvga_fifo_read(s);
      display = vmsvga_fifo_read(s);
      display = display ? SVGA_CURSOR_ON_SHOW : SVGA_CURSOR_ON_HIDE;
      if (s->cursor != id || s->active_cursor != id ||
          s->cursor_on != display || s->active_cursor_on != display) {
        s->cursor = id;
        s->active_cursor = id;
        s->cursor_on = display;
        s->active_cursor_on = display;
        vmsvga_cursor_select(s, id);
        s->cursor_dirty = true;
      };
      VPRINT("SVGA_CMD_DISPLAY_CURSOR command %u in SVGA command FIFO\n", cmd);
      break;
    }
    case SVGA_CMD_DRAW_GLYPH: {
      uint32_t x;
      uint32_t y;
      uint32_t width;
      uint32_t height;
      uint32_t foreground;
          uint64_t row_bytes;
      uint64_t payload_bytes;
      uint64_t payload_words;
      uint64_t total_words;
      if (len < 6) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      foreground = vmsvga_fifo_read(s);
      row_bytes = ((uint64_t)width + 7) >> 3;
      payload_bytes = row_bytes * height;
      payload_words = (payload_bytes + 3) >> 2;
      total_words = 6 + payload_words;
      if (payload_words > INT32_MAX || total_words > (uint64_t)len) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= (int32_t)total_words;
      vmsvga_draw_glyph(s, x, y, width, height, foreground, UINT32_MAX, false,
                        0, 0, 0, 0, (uint32_t)payload_words);
      VPRINT("SVGA_CMD_DRAW_GLYPH command %u in SVGA command FIFO\n", cmd);
      break;
    }
    case SVGA_CMD_DRAW_GLYPH_CLIPPED: {
      uint32_t x;
      uint32_t y;
      uint32_t width;
      uint32_t height;
      uint32_t foreground;
      uint32_t background;
      uint32_t clip_x;
      uint32_t clip_y;
      uint32_t clip_width;
      uint32_t clip_height;
          uint64_t row_bytes;
      uint64_t payload_bytes;
      uint64_t payload_words;
      uint64_t total_words;
      if (len < 11) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      foreground = vmsvga_fifo_read(s);
      background = vmsvga_fifo_read(s);
      clip_x = vmsvga_fifo_read(s);
      clip_y = vmsvga_fifo_read(s);
      clip_width = vmsvga_fifo_read(s);
      clip_height = vmsvga_fifo_read(s);
      row_bytes = ((uint64_t)width + 7) >> 3;
      payload_bytes = row_bytes * height;
      payload_words = (payload_bytes + 3) >> 2;
      total_words = 11 + payload_words;
      if (payload_words > INT32_MAX || total_words > (uint64_t)len) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= (int32_t)total_words;
      vmsvga_draw_glyph(s, x, y, width, height, foreground, background, true,
                        clip_x, clip_y, clip_width, clip_height,
                        (uint32_t)payload_words);
      VPRINT("SVGA_CMD_DRAW_GLYPH_CLIPPED command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_FREE_OBJECT: {
      uint32_t id;
      if (len < 2) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      len -= 2;
      VMVGA_TRACE_LOCAL(VMVGA_TRACE_OBJECT, "OBJECT_FREE id=%u", id);
      vmsvga_object_free(s, id);
      VPRINT("SVGA_CMD_FREE_OBJECT command %u in SVGA command FIFO\n", cmd);
      break;
    }
    case SVGA_CMD_MOVE_CURSOR: {
      if (len < (sizeof(SVGAFifoCmdMoveCursor) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdMoveCursor) / sizeof(uint32_t) + 1;
      {
        uint32_t x = vmsvga_fifo_read(s);
        uint32_t y = vmsvga_fifo_read(s);
        if (s->cursor_x != x || s->cursor_y != y ||
            s->active_cursor_x != x || s->active_cursor_y != y) {
          s->cursor_x = x;
          s->cursor_y = y;
          s->active_cursor_x = x;
          s->active_cursor_y = y;
          s->cursor_dirty = true;
        };
      };
      VPRINT("SVGA_CMD_MOVE_CURSOR command %u in SVGA command FIFO\n", cmd);
      break;
    }
    case SVGA_CMD_RECT_BITMAP_COPY: {
      uint32_t id, src_x, src_y, dst_x, dst_y, width, height, foreground,
          background;
      if (len < 10) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      src_x = vmsvga_fifo_read(s);
      src_y = vmsvga_fifo_read(s);
      dst_x = vmsvga_fifo_read(s);
      dst_y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      foreground = vmsvga_fifo_read(s);
      background = vmsvga_fifo_read(s);
      len -= 10;
      vmsvga_object_blit(s, id, VMSVGA_OBJECT_BITMAP, false, src_x, src_y,
                         dst_x, dst_y, width, height, foreground, background,
                         VMSVGA_ROP_COPY);
      VPRINT("SVGA_CMD_RECT_BITMAP_COPY command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_RECT_BITMAP_FILL: {
      uint32_t id, x, y, width, height, foreground, background;
      if (len < 8) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      foreground = vmsvga_fifo_read(s);
      background = vmsvga_fifo_read(s);
      len -= 8;
      vmsvga_object_blit(s, id, VMSVGA_OBJECT_BITMAP, true, 0, 0, x, y, width,
                         height, foreground, background, VMSVGA_ROP_COPY);
      VPRINT("SVGA_CMD_RECT_BITMAP_FILL command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_RECT_FILL:
      if (len < (sizeof(SVGAFifoCmdRectFill) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdRectFill) / sizeof(uint32_t) + 1;
      uint32_t SizeOfSVGAFifoCmdRectFill =
          sizeof(SVGAFifoCmdRectFill) / sizeof(uint32_t);
      while (SizeOfSVGAFifoCmdRectFill >= 1) {
        uint32_t c = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectFill -= 1;
        uint32_t x = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectFill -= 1;
        uint32_t y = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectFill -= 1;
        uint32_t w = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectFill -= 1;
        uint32_t h = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectFill -= 1;
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_DRAW,
            "RECT_FILL color=0x%08x x=%u y=%u w=%u h=%u",
            c, x, y, w, h);
        if (!vmsvga_fill_rect(s, c, x, y, w, h)) {
          VPRINT("SVGA_CMD_RECT_FILL ignored invalid rectangle\n");
        };
      };
      VPRINT("SVGA_CMD_RECT_FILL command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_RECT_PIXMAP_COPY: {
      uint32_t id, src_x, src_y, dst_x, dst_y, width, height;
      if (len < 8) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      src_x = vmsvga_fifo_read(s);
      src_y = vmsvga_fifo_read(s);
      dst_x = vmsvga_fifo_read(s);
      dst_y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      len -= 8;
      vmsvga_object_blit(s, id, VMSVGA_OBJECT_PIXMAP, false, src_x, src_y,
                         dst_x, dst_y, width, height, 0, 0, VMSVGA_ROP_COPY);
      VPRINT("SVGA_CMD_RECT_PIXMAP_COPY command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_RECT_PIXMAP_FILL: {
      uint32_t id, x, y, width, height;
      if (len < 6) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      len -= 6;
      vmsvga_object_blit(s, id, VMSVGA_OBJECT_PIXMAP, true, 0, 0, x, y, width,
                         height, 0, 0, VMSVGA_ROP_COPY);
      VPRINT("SVGA_CMD_RECT_PIXMAP_FILL command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_RECT_ROP_BITMAP_COPY: {
      uint32_t id, src_x, src_y, dst_x, dst_y, width, height, foreground,
          background, rop;
      if (len < 11) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      src_x = vmsvga_fifo_read(s);
      src_y = vmsvga_fifo_read(s);
      dst_x = vmsvga_fifo_read(s);
      dst_y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      foreground = vmsvga_fifo_read(s);
      background = vmsvga_fifo_read(s);
      rop = vmsvga_fifo_read(s);
      len -= 11;
      vmsvga_object_blit(s, id, VMSVGA_OBJECT_BITMAP, false, src_x, src_y,
                         dst_x, dst_y, width, height, foreground, background,
                         rop);
      VPRINT("SVGA_CMD_RECT_ROP_BITMAP_COPY command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_RECT_ROP_BITMAP_FILL: {
      uint32_t id, x, y, width, height, foreground, background, rop;
      if (len < 9) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      foreground = vmsvga_fifo_read(s);
      background = vmsvga_fifo_read(s);
      rop = vmsvga_fifo_read(s);
      len -= 9;
      vmsvga_object_blit(s, id, VMSVGA_OBJECT_BITMAP, true, 0, 0, x, y, width,
                         height, foreground, background, rop);
      VPRINT("SVGA_CMD_RECT_ROP_BITMAP_FILL command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_RECT_ROP_FILL:
      if (len < (sizeof(SVGAFifoCmdRectRopFill) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdRectRopFill) / sizeof(uint32_t) + 1;
      uint32_t rop_fill_color = vmsvga_fifo_read(s);
      uint32_t rop_fill_x = vmsvga_fifo_read(s);
      uint32_t rop_fill_y = vmsvga_fifo_read(s);
      uint32_t rop_fill_w = vmsvga_fifo_read(s);
      uint32_t rop_fill_h = vmsvga_fifo_read(s);
      uint32_t rop_fill_rop = vmsvga_fifo_read(s);
      VMVGA_TRACE_LOCAL(
          VMVGA_TRACE_ROP,
          "ROP_FILL color=0x%08x x=%u y=%u w=%u h=%u rop=0x%02x",
          rop_fill_color, rop_fill_x, rop_fill_y, rop_fill_w, rop_fill_h,
          rop_fill_rop);
      if (!vmsvga_rop_fill_rect(s, rop_fill_color, rop_fill_x, rop_fill_y,
                                rop_fill_w, rop_fill_h, rop_fill_rop)) {
        VPRINT("SVGA_CMD_RECT_ROP_FILL ignored invalid rectangle or ROP\n");
      };
      VPRINT("SVGA_CMD_RECT_ROP_FILL command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_RECT_ROP_PIXMAP_COPY: {
      uint32_t id, src_x, src_y, dst_x, dst_y, width, height, rop;
      if (len < 9) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      src_x = vmsvga_fifo_read(s);
      src_y = vmsvga_fifo_read(s);
      dst_x = vmsvga_fifo_read(s);
      dst_y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      rop = vmsvga_fifo_read(s);
      len -= 9;
      vmsvga_object_blit(s, id, VMSVGA_OBJECT_PIXMAP, false, src_x, src_y,
                         dst_x, dst_y, width, height, 0, 0, rop);
      VPRINT("SVGA_CMD_RECT_ROP_PIXMAP_COPY command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_RECT_ROP_PIXMAP_FILL: {
      uint32_t id, x, y, width, height, rop;
      if (len < 7) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      id = vmsvga_fifo_read(s);
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      rop = vmsvga_fifo_read(s);
      len -= 7;
      vmsvga_object_blit(s, id, VMSVGA_OBJECT_PIXMAP, true, 0, 0, x, y, width,
                         height, 0, 0, rop);
      VPRINT("SVGA_CMD_RECT_ROP_PIXMAP_FILL command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_SURFACE_ALPHA_BLEND: {
      uint32_t src_surface_offset;
      uint32_t dst_surface_offset;
      uint32_t src_x;
      uint32_t src_y;
      uint32_t dst_x;
      uint32_t dst_y;
      uint32_t width;
      uint32_t height;
      uint32_t blend_op;
      uint32_t flags;
      uint32_t param1;
      uint32_t param2;
      if (len < (sizeof(SVGAFifoCmdSurfaceAlphaBlend) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdSurfaceAlphaBlend) / sizeof(uint32_t) + 1;
      src_surface_offset = vmsvga_fifo_read(s);
      dst_surface_offset = vmsvga_fifo_read(s);
      src_x = vmsvga_fifo_read(s);
      src_y = vmsvga_fifo_read(s);
      dst_x = vmsvga_fifo_read(s);
      dst_y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      blend_op = vmsvga_fifo_read(s);
      flags = vmsvga_fifo_read(s);
      param1 = vmsvga_fifo_read(s);
      param2 = vmsvga_fifo_read(s);
      if (!vmsvga_surface_alpha_blend(
              s, src_surface_offset, dst_surface_offset, src_x, src_y, dst_x,
              dst_y, width, height, blend_op, flags, param1, param2)) {
        VPRINT("SVGA_CMD_SURFACE_ALPHA_BLEND ignored invalid surface or "
               "blend operation\n");
      };
      VPRINT("SVGA_CMD_SURFACE_ALPHA_BLEND command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_SURFACE_COPY: {
      uint32_t src_surface_offset;
      uint32_t dst_surface_offset;
      uint32_t src_x;
      uint32_t src_y;
      uint32_t dst_x;
      uint32_t dst_y;
      uint32_t width;
      uint32_t height;
      uint32_t rop;
      if (len < (sizeof(SVGAFifoCmdSurfaceCopy) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdSurfaceCopy) / sizeof(uint32_t) + 1;
      src_surface_offset = vmsvga_fifo_read(s);
      dst_surface_offset = vmsvga_fifo_read(s);
      src_x = vmsvga_fifo_read(s);
      src_y = vmsvga_fifo_read(s);
      dst_x = vmsvga_fifo_read(s);
      dst_y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      rop = vmsvga_fifo_read(s);
      if (!vmsvga_surface_copy(s, src_surface_offset, dst_surface_offset,
                               src_x, src_y, dst_x, dst_y, width, height,
                               rop)) {
        VPRINT("SVGA_CMD_SURFACE_COPY ignored invalid surface or ROP\n");
      };
      VPRINT("SVGA_CMD_SURFACE_COPY command %u in SVGA command FIFO\n", cmd);
      break;
    }
    case SVGA_CMD_SURFACE_FILL: {
      uint32_t color;
      uint32_t dst_surface_offset;
      uint32_t x;
      uint32_t y;
      uint32_t width;
      uint32_t height;
      uint32_t rop;
      if (len < (sizeof(SVGAFifoCmdSurfaceFill) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdSurfaceFill) / sizeof(uint32_t) + 1;
      color = vmsvga_fifo_read(s);
      dst_surface_offset = vmsvga_fifo_read(s);
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      rop = vmsvga_fifo_read(s);
      if (!vmsvga_surface_fill(s, color, dst_surface_offset, x, y, width,
                               height, rop)) {
        VPRINT("SVGA_CMD_SURFACE_FILL ignored invalid surface or ROP\n");
      };
      VPRINT("SVGA_CMD_SURFACE_FILL command %u in SVGA command FIFO\n", cmd);
      break;
    }
    case SVGA_CMD_UPDATE:
      if (len < (sizeof(SVGAFifoCmdUpdate) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdUpdate) / sizeof(uint32_t) + 1;
      uint32_t SizeOfSVGAFifoCmdUpdate =
          sizeof(SVGAFifoCmdUpdate) / sizeof(uint32_t);
      while (SizeOfSVGAFifoCmdUpdate >= 1) {
        uint32_t x = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdUpdate -= 1;
        uint32_t y = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdUpdate -= 1;
        uint32_t w = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdUpdate -= 1;
        uint32_t h = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdUpdate -= 1;
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_DRAW,
                           "UPDATE x=%u y=%u w=%u h=%u", x, y, w, h);
        vmsvga_update_rect(s, x, y, w, h);
      };
      VPRINT("SVGA_CMD_UPDATE command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_UPDATE_VERBOSE:
      if (len < (sizeof(SVGAFifoCmdUpdateVerbose) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdUpdateVerbose) / sizeof(uint32_t) + 1;
      uint32_t SizeOfSVGAFifoCmdUpdateVerbose =
          sizeof(SVGAFifoCmdUpdateVerbose) / sizeof(uint32_t);
      while (SizeOfSVGAFifoCmdUpdateVerbose >= 1) {
        uint32_t x = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdUpdateVerbose -= 1;
        uint32_t y = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdUpdateVerbose -= 1;
        uint32_t w = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdUpdateVerbose -= 1;
        uint32_t h = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdUpdateVerbose -= 1;
        uint32_t reason = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdUpdateVerbose -= 1;
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_DRAW,
                           "UPDATE_VERBOSE x=%u y=%u w=%u h=%u reason=%u",
                           x, y, w, h, reason);
        vmsvga_update_rect(s, x, y, w, h);
      };
      VPRINT("SVGA_CMD_UPDATE_VERBOSE command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_RECT_COPY:
      if (len < (sizeof(SVGAFifoCmdRectCopy) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdRectCopy) / sizeof(uint32_t) + 1;
      uint32_t SizeOfSVGAFifoCmdRectCopy =
          sizeof(SVGAFifoCmdRectCopy) / sizeof(uint32_t);
      while (SizeOfSVGAFifoCmdRectCopy >= 1) {
        uint32_t x0 = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectCopy -= 1;
        uint32_t y0 = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectCopy -= 1;
        uint32_t x1 = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectCopy -= 1;
        uint32_t y1 = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectCopy -= 1;
        uint32_t w = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectCopy -= 1;
        uint32_t h = vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdRectCopy -= 1;
        if (!vmsvga_copy_rect(s, x0, y0, x1, y1, w, h)) {
          VPRINT("SVGA_CMD_RECT_COPY ignored invalid rectangle\n");
        };
      };
      VPRINT("SVGA_CMD_RECT_COPY command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_DEFINE_CURSOR: {
      uint32_t header_words =
          sizeof(SVGAFifoCmdDefineCursor) / sizeof(uint32_t);
      size_t and_words;
      size_t xor_words;
      if (len < (int32_t)header_words + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      vmsvga_cursor_definition_clear(&cursor);
      cursor.id = vmsvga_fifo_read(s);
      cursor.hot_x = vmsvga_fifo_read(s);
      cursor.hot_y = vmsvga_fifo_read(s);
      cursor.width = vmsvga_fifo_read(s);
      cursor.height = vmsvga_fifo_read(s);
      cursor.and_mask_bpp = vmsvga_fifo_read(s);
      cursor.xor_mask_bpp = vmsvga_fifo_read(s);
      if (cursor.width < 1 || cursor.height < 1 ||
          cursor.width > VMSVGA_CURSOR_MAX_DIMENSION ||
          cursor.height > VMSVGA_CURSOR_MAX_DIMENSION ||
          cursor.hot_x >= cursor.width || cursor.hot_y >= cursor.height ||
          (cursor.and_mask_bpp != 1 &&
           cursor.and_mask_bpp != vmsvga_active_depth(s)) ||
          (cursor.xor_mask_bpp != 1 &&
           cursor.xor_mask_bpp != vmsvga_active_depth(s))) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("invalid SVGA_CMD_DEFINE_CURSOR command %u\n", cmd);
        break;
      };
      and_words = SVGA_PIXMAP_SIZE(cursor.width, cursor.height,
                                   cursor.and_mask_bpp);
      xor_words = SVGA_PIXMAP_SIZE(cursor.width, cursor.height,
                                   cursor.xor_mask_bpp);
      cursor.and_words = and_words;
      cursor.xor_words = xor_words;
      if ((uint64_t)header_words + and_words + xor_words + 1 >
          (uint64_t)len) {
        uint64_t command_bytes =
            ((uint64_t)header_words + and_words + xor_words + 1) *
            sizeof(uint32_t);
        uint64_t ring_bytes = s->fifo_max > s->fifo_min
                                  ? (uint64_t)s->fifo_max - s->fifo_min
                                  : 0;
        if (ring_bytes != 0 && command_bytes >= ring_bytes) {
          /* This command can never fit in the configured FIFO ring. Drop the
           * queued malformed submission instead of rewinding forever. */
          s->fifo_stop = s->fifo_next;
          s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
          len = 0;
          VPRINT("oversized SVGA_CMD_DEFINE_CURSOR command %u discarded\n",
                 cmd);
        } else {
          s->fifo_stop = fifo_start;
          s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
          len = 0;
          VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        };
        break;
      };
      cursor.and_mask = g_try_malloc((size_t)and_words * sizeof(uint32_t));
      cursor.xor_mask = g_try_malloc((size_t)xor_words * sizeof(uint32_t));
      if ((and_words != 0 && cursor.and_mask == NULL) ||
          (xor_words != 0 && cursor.xor_mask == NULL)) {
        len -= header_words + and_words + xor_words + 1;
        vmsvga_fifo_skip(s, and_words + xor_words);
        vmsvga_cursor_definition_clear(&cursor);
        VPRINT("SVGA_CMD_DEFINE_CURSOR command %u dropped: allocation failed\n",
               cmd);
        break;
      };
      len -= header_words + and_words + xor_words + 1;
      vmsvga_fifo_read_raw_data(s, cursor.and_mask, and_words);
      vmsvga_fifo_read_raw_data(s, cursor.xor_mask, xor_words);
      vmsvga_cursor_define(s, &cursor);
      vmsvga_cursor_definition_clear(&cursor);
      VPRINT("SVGA_CMD_DEFINE_CURSOR command %u in SVGA command FIFO\n", cmd);
      break;
    }
    case SVGA_CMD_DEFINE_ALPHA_CURSOR: {
      uint32_t header_words =
          sizeof(SVGAFifoCmdDefineAlphaCursor) / sizeof(uint32_t);
      size_t pixels;
      if (len < (int32_t)header_words + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      vmsvga_cursor_definition_clear(&cursor);
      cursor.id = vmsvga_fifo_read(s);
      cursor.hot_x = vmsvga_fifo_read(s);
      cursor.hot_y = vmsvga_fifo_read(s);
      cursor.width = vmsvga_fifo_read(s);
      cursor.height = vmsvga_fifo_read(s);
      cursor.and_mask_bpp = 0;
      cursor.xor_mask_bpp = 32;
      if (cursor.width < 1 || cursor.height < 1 ||
          cursor.width > VMSVGA_CURSOR_MAX_DIMENSION ||
          cursor.height > VMSVGA_CURSOR_MAX_DIMENSION ||
          cursor.hot_x >= cursor.width || cursor.hot_y >= cursor.height) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("invalid SVGA_CMD_DEFINE_ALPHA_CURSOR command %u\n", cmd);
        break;
      };
      pixels = (size_t)cursor.width * cursor.height;
      if ((uint64_t)header_words + pixels + 1 > (uint64_t)len) {
        uint64_t command_bytes =
            ((uint64_t)header_words + pixels + 1) * sizeof(uint32_t);
        uint64_t ring_bytes = s->fifo_max > s->fifo_min
                                  ? (uint64_t)s->fifo_max - s->fifo_min
                                  : 0;
        if (ring_bytes != 0 && command_bytes >= ring_bytes) {
          s->fifo_stop = s->fifo_next;
          s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
          len = 0;
          VPRINT("oversized SVGA_CMD_DEFINE_ALPHA_CURSOR command %u discarded\n",
                 cmd);
        } else {
          s->fifo_stop = fifo_start;
          s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
          len = 0;
          VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        };
        break;
      };
      cursor.and_words = 0;
      cursor.xor_words = pixels;
      cursor.xor_mask = g_try_malloc(pixels * sizeof(uint32_t));
      if (pixels != 0 && cursor.xor_mask == NULL) {
        len -= header_words + pixels + 1;
        vmsvga_fifo_skip(s, pixels);
        vmsvga_cursor_definition_clear(&cursor);
        VPRINT("SVGA_CMD_DEFINE_ALPHA_CURSOR command %u dropped: allocation failed\n",
               cmd);
        break;
      };
      len -= header_words + pixels + 1;
      vmsvga_fifo_read_raw_data(s, cursor.xor_mask, pixels);
      vmsvga_rgba_cursor_define(s, &cursor);
      vmsvga_cursor_definition_clear(&cursor);
      VPRINT("SVGA_CMD_DEFINE_ALPHA_CURSOR command %u in SVGA command FIFO\n",
             cmd);
      break;
    }
    case SVGA_CMD_FENCE: {
      uint32_t command_words =
          sizeof(SVGAFifoCmdFence) / sizeof(uint32_t);
      if (len < (int32_t)command_words + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= command_words + 1;
      fence_arg = vmsvga_fifo_read(s);
      s->fence = fence_arg;
      if (vmsvga_fifo_has_reg(s, SVGA_FIFO_FENCE)) {
        s->fifo[SVGA_FIFO_FENCE] = cpu_to_le32(fence_arg);
      };
      if (vmsvga_fifo_has_reg(s, SVGA_FIFO_FENCE_GOAL) &&
          le32_to_cpu(s->fifo[SVGA_FIFO_FENCE_GOAL]) == fence_arg) {
        irq_status |= SVGA_IRQFLAG_FENCE_GOAL;
      };
#ifndef ANY_FENCE_OFF
      irq_status |= SVGA_IRQFLAG_ANY_FENCE;
#endif
      VPRINT("SVGA_CMD_FENCE command %u in SVGA command FIFO %u\n", cmd,
             fence_arg);
      break;
    }
    case SVGA_CMD_DEFINE_GMR2:
      if (len < (sizeof(SVGAFifoCmdDefineGMR2) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdDefineGMR2) / sizeof(uint32_t) + 1;
      uint32_t SizeOfSVGAFifoCmdDefineGMR2 =
          sizeof(SVGAFifoCmdDefineGMR2) / sizeof(uint32_t);
      while (SizeOfSVGAFifoCmdDefineGMR2 >= 1) {
        vmsvga_fifo_read(s);
        SizeOfSVGAFifoCmdDefineGMR2 -= 1;
      };
      /* TODO: Model GMR2 state before advertising SVGA_CAP_GMR2. */
      VPRINT("SVGA_CMD_DEFINE_GMR2 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_REMAP_GMR2: {
      uint32_t gmr_id;
      uint32_t flags;
      uint32_t offset_pages;
      uint32_t num_pages;
      uint64_t payload_words;
      uint64_t total_words;
      if (len < (sizeof(SVGAFifoCmdRemapGMR2) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      gmr_id = vmsvga_fifo_read(s);
      flags = vmsvga_fifo_read(s);
      offset_pages = vmsvga_fifo_read(s);
      num_pages = vmsvga_fifo_read(s);
      (void)gmr_id;
      (void)offset_pages;
      if (flags & ~(SVGA_REMAP_GMR2_VIA_GMR | SVGA_REMAP_GMR2_PPN64 |
                    SVGA_REMAP_GMR2_SINGLE_PPN)) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("invalid SVGA_CMD_REMAP_GMR2 flags %u\n", flags);
        break;
      };
      if (flags & SVGA_REMAP_GMR2_VIA_GMR) {
        payload_words = sizeof(SVGAGuestPtr) / sizeof(uint32_t);
      } else {
        uint64_t entries =
            (flags & SVGA_REMAP_GMR2_SINGLE_PPN) ? 1 : num_pages;
        payload_words = entries *
                        ((flags & SVGA_REMAP_GMR2_PPN64) ? 2 : 1);
      };
      total_words = sizeof(SVGAFifoCmdRemapGMR2) / sizeof(uint32_t) + 1 +
                    payload_words;
      if (payload_words > UINT32_MAX || total_words > (uint64_t)len) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= (int32_t)total_words;
      vmsvga_fifo_skip(s, (uint32_t)payload_words);
      /* TODO: Apply the remap to GMR2 state once GMR2 is implemented. */
      VPRINT("SVGA_CMD_REMAP_GMR2 command %u in SVGA command FIFO: gmr %u, "
             "offset %u, pages %u\n",
             cmd, gmr_id, offset_pages, num_pages);
      break;
    }
    case SVGA_CMD_RECT_ROP_COPY:
      if (len < (sizeof(SVGAFifoCmdRectRopCopy) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdRectRopCopy) / sizeof(uint32_t) + 1;
      uint32_t rop_copy_x0 = vmsvga_fifo_read(s);
      uint32_t rop_copy_y0 = vmsvga_fifo_read(s);
      uint32_t rop_copy_x1 = vmsvga_fifo_read(s);
      uint32_t rop_copy_y1 = vmsvga_fifo_read(s);
      uint32_t rop_copy_w = vmsvga_fifo_read(s);
      uint32_t rop_copy_h = vmsvga_fifo_read(s);
      uint32_t rop_copy_rop = vmsvga_fifo_read(s);
      VMVGA_TRACE_LOCAL(
          VMVGA_TRACE_ROP,
          "ROP_COPY src=%u,%u dst=%u,%u w=%u h=%u rop=0x%02x",
          rop_copy_x0, rop_copy_y0, rop_copy_x1, rop_copy_y1, rop_copy_w,
          rop_copy_h, rop_copy_rop);
      if (!vmsvga_rop_copy_rect(s, rop_copy_x0, rop_copy_y0, rop_copy_x1,
                                rop_copy_y1, rop_copy_w, rop_copy_h,
                                rop_copy_rop)) {
        VPRINT("SVGA_CMD_RECT_ROP_COPY ignored invalid rectangle or ROP\n");
      };
      VPRINT("SVGA_CMD_RECT_ROP_COPY command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_ESCAPE: {
      uint32_t nsid;
      uint32_t size;
      uint64_t payload_words;
      uint64_t total_words;
      if (len < (sizeof(SVGAFifoCmdEscape) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      nsid = vmsvga_fifo_read(s);
      size = vmsvga_fifo_read(s);
      (void)nsid;
      payload_words = ((uint64_t)size + 3) >> 2;
      total_words = sizeof(SVGAFifoCmdEscape) / sizeof(uint32_t) + 1 +
                    payload_words;
      if (payload_words > UINT32_MAX || total_words > (uint64_t)len) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= (int32_t)total_words;
      vmsvga_fifo_skip(s, (uint32_t)payload_words);
      /* TODO: Dispatch supported escape namespaces before advertising ESCAPE. */
      VPRINT("SVGA_CMD_ESCAPE command %u in SVGA command FIFO: nsid %u, "
             "size %u\n",
             cmd, nsid, size);
      break;
    }
    case SVGA_CMD_DEFINE_SCREEN: {
      uint32_t struct_size;
      uint64_t screen_words;
      uint64_t total_words;
      uint32_t id, flags, width, height;
      int32_t root_x, root_y;
      uint32_t backing_gmr_id = SVGA_GMR_NULL;
      uint32_t backing_offset = 0;
      uint32_t backing_pitch = 0;
      uint32_t clone_count = 0;
      uint32_t consumed_words = 7;
      uint32_t extra_words;
      bool backing_present = false;
      if (len < 8) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      struct_size = vmsvga_fifo_read(s);
      screen_words = ((uint64_t)struct_size + 3) >> 2;
      total_words = screen_words + 1;
      if (struct_size < VMSVGA_SCREEN_V1_STRUCT_SIZE ||
          (struct_size > VMSVGA_SCREEN_V1_STRUCT_SIZE &&
           struct_size < VMSVGA_SCREEN_BACKING_STRUCT_SIZE) ||
          (struct_size > VMSVGA_SCREEN_BACKING_STRUCT_SIZE &&
           struct_size < VMSVGA_SCREEN_FULL_STRUCT_SIZE) ||
          struct_size > SVGA_CMD_MAX_DATASIZE || screen_words > UINT32_MAX ||
          total_words > (uint64_t)len) {
        if (vmsvga_trace_flight_enabled()) {
          fprintf(stderr,
                  "VMVGA-SCREEN-REJECT define reason=fifo-size size=%u "
                  "remaining=%d\n",
                  struct_size, len);
        }
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("invalid SVGA_CMD_DEFINE_SCREEN size %u\n", struct_size);
        break;
      };
      id = vmsvga_fifo_read(s);
      flags = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      root_x = (int32_t)vmsvga_fifo_read(s);
      root_y = (int32_t)vmsvga_fifo_read(s);
      if (struct_size >= VMSVGA_SCREEN_BACKING_STRUCT_SIZE) {
        backing_gmr_id = vmsvga_fifo_read(s);
        backing_offset = vmsvga_fifo_read(s);
        backing_pitch = vmsvga_fifo_read(s);
        backing_present = backing_gmr_id != SVGA_GMR_NULL;
        consumed_words += 3;
      };
      if (struct_size >= VMSVGA_SCREEN_FULL_STRUCT_SIZE) {
        clone_count = vmsvga_fifo_read(s);
        consumed_words++;
      };
      extra_words = (uint32_t)screen_words - consumed_words;
      vmsvga_fifo_skip(s, extra_words);
      len -= (int32_t)total_words;
      if (!vmsvga_screen_define(s, id, flags, width, height, root_x, root_y,
                                backing_present, backing_gmr_id,
                                backing_offset, backing_pitch, clone_count)) {
        VPRINT("SVGA_CMD_DEFINE_SCREEN rejected id=%u flags=0x%x size=%ux%u\n",
               id, flags, width, height);
      };
      break;
    }
    case SVGA_CMD_DESTROY_SCREEN: {
      uint32_t screen_id;
      if (len < (sizeof(SVGAFifoCmdDestroyScreen) / sizeof(uint32_t)) + 1) {
        if (vmsvga_trace_flight_enabled()) {
          fprintf(stderr,
                  "VMVGA-SCREEN-REJECT destroy reason=fifo-truncated remaining=%d\n",
                  len);
        }
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      len -= sizeof(SVGAFifoCmdDestroyScreen) / sizeof(uint32_t) + 1;
      screen_id = vmsvga_fifo_read(s);
      vmsvga_screen_destroy(s, screen_id);
      break;
    }
    case SVGA_CMD_DEFINE_GMRFB: {
      uint32_t gmr_id, offset, bytes_per_line, format;
      if (len < (sizeof(SVGAFifoCmdDefineGMRFB) / sizeof(uint32_t)) + 1) {
        if (vmsvga_trace_flight_enabled()) {
          fprintf(stderr,
                  "VMVGA-SCREEN-REJECT gmrfb reason=fifo-truncated remaining=%d\n",
                  len);
        }
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      len -= sizeof(SVGAFifoCmdDefineGMRFB) / sizeof(uint32_t) + 1;
      gmr_id = vmsvga_fifo_read(s);
      offset = vmsvga_fifo_read(s);
      bytes_per_line = vmsvga_fifo_read(s);
      format = vmsvga_fifo_read(s);
      if (!vmsvga_screen_define_gmrfb(s, gmr_id, offset, bytes_per_line,
                                      format)) {
        VPRINT("SVGA_CMD_DEFINE_GMRFB rejected gmr=%u offset=%u pitch=%u "
               "format=0x%x\n", gmr_id, offset, bytes_per_line, format);
      };
      break;
    }
    case SVGA_CMD_BLIT_GMRFB_TO_SCREEN: {
      SVGASignedPoint src_origin;
      SVGASignedRect dest_rect;
      uint32_t dest_screen_id;
      if (len < (sizeof(SVGAFifoCmdBlitGMRFBToScreen) / sizeof(uint32_t)) + 1) {
        if (vmsvga_trace_flight_enabled()) {
          fprintf(stderr,
                  "VMVGA-SCREEN-REJECT blit-gmrfb-to-screen "
                  "reason=fifo-truncated remaining=%d\n",
                  len);
        }
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      len -= sizeof(SVGAFifoCmdBlitGMRFBToScreen) / sizeof(uint32_t) + 1;
      src_origin.x = (int32_t)vmsvga_fifo_read(s);
      src_origin.y = (int32_t)vmsvga_fifo_read(s);
      dest_rect.left = (int32_t)vmsvga_fifo_read(s);
      dest_rect.top = (int32_t)vmsvga_fifo_read(s);
      dest_rect.right = (int32_t)vmsvga_fifo_read(s);
      dest_rect.bottom = (int32_t)vmsvga_fifo_read(s);
      dest_screen_id = vmsvga_fifo_read(s);
      vmsvga_screen_blit_gmrfb_to_screen(s, &src_origin, &dest_rect,
                                         dest_screen_id);
      break;
    }
    case SVGA_CMD_BLIT_SCREEN_TO_GMRFB: {
      SVGASignedPoint dest_origin;
      SVGASignedRect src_rect;
      uint32_t src_screen_id;
      if (len < (sizeof(SVGAFifoCmdBlitScreenToGMRFB) / sizeof(uint32_t)) + 1) {
        if (vmsvga_trace_flight_enabled()) {
          fprintf(stderr,
                  "VMVGA-SCREEN-REJECT blit-screen-to-gmrfb "
                  "reason=fifo-truncated remaining=%d\n",
                  len);
        }
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      len -= sizeof(SVGAFifoCmdBlitScreenToGMRFB) / sizeof(uint32_t) + 1;
      dest_origin.x = (int32_t)vmsvga_fifo_read(s);
      dest_origin.y = (int32_t)vmsvga_fifo_read(s);
      src_rect.left = (int32_t)vmsvga_fifo_read(s);
      src_rect.top = (int32_t)vmsvga_fifo_read(s);
      src_rect.right = (int32_t)vmsvga_fifo_read(s);
      src_rect.bottom = (int32_t)vmsvga_fifo_read(s);
      src_screen_id = vmsvga_fifo_read(s);
      vmsvga_screen_blit_screen_to_gmrfb(s, &dest_origin, &src_rect,
                                         src_screen_id);
      break;
    }
    case SVGA_CMD_ANNOTATION_FILL: {
      uint32_t color;
      if (len < (sizeof(SVGAFifoCmdAnnotationFill) / sizeof(uint32_t)) + 1) {
        if (vmsvga_trace_flight_enabled()) {
          fprintf(stderr,
                  "VMVGA-SCREEN-REJECT annotation-fill "
                  "reason=fifo-truncated remaining=%d\n",
                  len);
        }
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      len -= sizeof(SVGAFifoCmdAnnotationFill) / sizeof(uint32_t) + 1;
      color = vmsvga_fifo_read(s);
      vmsvga_screen_annotation_fill(s, color);
      break;
    }
    case SVGA_CMD_ANNOTATION_COPY: {
      int32_t src_x, src_y;
      uint32_t src_screen_id;
      if (len < (sizeof(SVGAFifoCmdAnnotationCopy) / sizeof(uint32_t)) + 1) {
        if (vmsvga_trace_flight_enabled()) {
          fprintf(stderr,
                  "VMVGA-SCREEN-REJECT annotation-copy "
                  "reason=fifo-truncated remaining=%d\n",
                  len);
        }
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        break;
      };
      len -= sizeof(SVGAFifoCmdAnnotationCopy) / sizeof(uint32_t) + 1;
      src_x = (int32_t)vmsvga_fifo_read(s);
      src_y = (int32_t)vmsvga_fifo_read(s);
      src_screen_id = vmsvga_fifo_read(s);
      vmsvga_screen_annotation_copy(s, src_x, src_y, src_screen_id);
      break;
    }
    case SVGA_CMD_FRONT_ROP_FILL:
      if (len < (sizeof(SVGAFifoCmdFrontRopFill) / sizeof(uint32_t)) + 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      len -= sizeof(SVGAFifoCmdFrontRopFill) / sizeof(uint32_t) + 1;
      vmsvga_fifo_read(s);
      uint32_t front_rop_x = vmsvga_fifo_read(s);
      uint32_t front_rop_y = vmsvga_fifo_read(s);
      uint32_t front_rop_w = vmsvga_fifo_read(s);
      uint32_t front_rop_h = vmsvga_fifo_read(s);
      uint32_t front_rop_rop = vmsvga_fifo_read(s);
      if (front_rop_rop == VMSVGA_ROP_COPY &&
          vmsvga_verify_rect(s, front_rop_x, front_rop_y, front_rop_w,
                             front_rop_h)) {
        vmsvga_update_rect(s, front_rop_x, front_rop_y, front_rop_w,
                           front_rop_h);
      } else {
        VPRINT("SVGA_CMD_FRONT_ROP_FILL ignored invalid rectangle or ROP\n");
      };
      VPRINT("SVGA_CMD_FRONT_ROP_FILL command %u in SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_CMD_DEAD:
      if (len < 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      s->fifo_stop = fifo_start;
      s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
      len = 0;
      VPRINT("SVGA_CMD_DEAD command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_DEAD_2:
      if (len < 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      s->fifo_stop = fifo_start;
      s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
      len = 0;
      VPRINT("SVGA_CMD_DEAD_2 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_NOP:
      if (len < 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      s->fifo_stop = fifo_start;
      s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
      len = 0;
      VPRINT("SVGA_CMD_NOP command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_NOP_ERROR:
      if (len < 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      s->fifo_stop = fifo_start;
      s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
      len = 0;
      VPRINT("SVGA_CMD_NOP_ERROR command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_MAX:
      if (len < 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      s->fifo_stop = fifo_start;
      s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
      len = 0;
      VPRINT("SVGA_CMD_MAX command %u in SVGA command FIFO\n", cmd);
      break;
    default:
      if (vmsvga3d_fifo_command(s, cmd, &len, fifo_start)) {
        break;
      };
      if (len < 1) {
        s->fifo_stop = fifo_start;
        s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
        len = 0;
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        break;
      };
      s->fifo_stop = fifo_start;
      s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
      len = 0;
      VPRINT("default command %u in SVGA command FIFO\n", cmd);
      break;
    };
    if (s->fifo_stop == fifo_start && len == 0) {
      bool is_3d = cmd >= SVGA_3D_CMD_BASE && cmd < SVGA_3D_CMD_MAX;
      bool supported_3d = is_3d && vmsvga3d_fifo_supported_command(cmd);
      VMVGA_TRACE_LOCAL(
          VMVGA_TRACE_STATE,
          "FIFO_STALL cmd=%u stop=0x%08x next=0x%08x words=%d sync=%u",
          cmd, fifo_start, s->fifo_next, len, s->sync);
      vmsvga_trace_fifo_stall(s, 2, cmd, fifo_start, len, is_3d,
                              supported_3d);
    };
    if (s->fifo_stop != fifo_start) {
      irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS;
    };
    irq_status &= s->irq_mask;
    irq_status &= ~s->irq_status;
    if (irq_status) {
      s->irq_status |= irq_status;
#ifndef RAISE_IRQ_OFF
      if (irq_status & s->irq_mask) {
        struct pci_vmsvga_state_s *pci_vmsvga =
            container_of(s, struct pci_vmsvga_state_s, chip);
        pci_set_irq(PCI_DEVICE(pci_vmsvga), 1);
      };
#endif
    };
  };
  if (flush_damage) {
    vmsvga_damage_flush(s);
  };
  vmsvga_cursor_definition_clear(&cursor);
  cursor_update_from_fifo(s);
  if (vmsvga_fifo_pending(s)) {
    /*
     * Pending work includes both fairness-budget exhaustion and a command
     * which was rewound/stalled. In either case the FIFO is not drained, so
     * preserve an explicit SYNC and keep the FIFO-side busy hint asserted.
     */
    if (vmsvga_fifo_has_reg(s, SVGA_FIFO_BUSY)) {
      s->fifo[SVGA_FIFO_BUSY] = cpu_to_le32(1);
    };
  } else {
    s->sync = 0;
    if (vmsvga_fifo_has_reg(s, SVGA_FIFO_BUSY)) {
      s->fifo[SVGA_FIFO_BUSY] = cpu_to_le32(0);
    };
  };
};
static uint32_t vmsvga_index_read(void *opaque, uint32_t address) {
  VPRINT("vmsvga_index_read was just executed\n");
  struct vmsvga_state_s *s = opaque;
  VPRINT("vmsvga_index_read %u %u\n", address, s->index);
  return s->index;
};
static void vmsvga_index_write(void *opaque, uint32_t address, uint32_t index) {
  VPRINT("vmsvga_index_write was just executed\n");
  struct vmsvga_state_s *s = opaque;
  VPRINT("vmsvga_index_write %u %u\n", address, index);
  s->index = index;
};
static pixman_format_code_t vmsvga_pixman_format(uint32_t bpp) {
#define PIXMAN_FORMAT(bpp, type, a, r, g, b)                                   \
  (((bpp) << 24) | ((type) << 16) | ((a) << 12) | ((r) << 8) | ((g) << 4) |    \
   ((b)))
  switch (bpp) {
  case 4:
    return PIXMAN_FORMAT(4, 2, 0, 1, 2, 1);
  case 8:
#ifdef CONFIG_PIXMAN
    return PIXMAN_c8;
#else
    return PIXMAN_FORMAT(8, 2, 0, 3, 3, 2);
#endif
  case 15:
    return PIXMAN_FORMAT(16, 2, 0, 5, 5, 5);
  case 16:
    return PIXMAN_FORMAT(16, 2, 0, 5, 6, 5);
  case 24:
    return PIXMAN_FORMAT(24, 2, 0, 8, 8, 8);
  case 32:
    return PIXMAN_FORMAT(32, 2, 0, 8, 8, 8);
  default:
    return qemu_default_pixman_format(bpp, true);
  }
}
static inline uint32_t vmsvga_bytes_per_pixel(uint32_t bpp) {
  switch (bpp) {
  case 8:
    return 1;
  case 15:
  case 16:
    return 2;
  case 24:
    return 3;
  case 32:
    return 4;
  default:
    return 0;
  };
};
static inline bool vmsvga_mode_valid(struct vmsvga_state_s *s, uint32_t width,
                                     uint32_t height, uint32_t depth,
                                     uint32_t pitchlock) {
  uint32_t bypp = vmsvga_bytes_per_pixel(depth);
  uint64_t min_stride;
  uint64_t stride;
  uint64_t size;
  if (width < 1 || width > VMSVGA_MAX_WIDTH || height < 1 ||
      height > VMSVGA_MAX_HEIGHT || bypp == 0) {
    return false;
  };
  min_stride = (uint64_t)width * bypp;
  stride = pitchlock ? pitchlock : min_stride;
  if (stride < min_stride || stride > UINT32_MAX) {
    return false;
  };
  size = stride * height;
  return size <= s->vga.vram_size;
};
static inline bool vmsvga_pending_pitch_valid(struct vmsvga_state_s *s,
                                               uint32_t pitch) {
  uint32_t bypp = vmsvga_bytes_per_pixel(s->new_depth);
  uint64_t min_pitch;
  if (pitch == 0 || bypp == 0 || s->new_width == 0 || s->new_height == 0) {
    return false;
  };
  min_pitch = (uint64_t)s->new_width * bypp;
  return pitch >= min_pitch &&
         (uint64_t)pitch * s->new_height <= s->vga.vram_size;
};
static inline uint32_t vmsvga_pending_stride(struct vmsvga_state_s *s) {
  uint64_t natural =
      (uint64_t)s->new_width * vmsvga_bytes_per_pixel(s->new_depth);
  if (vmsvga_pending_pitch_valid(s, s->pitchlock)) {
    return s->pitchlock;
  };
  if (s->config && vmsvga_fifo_has_reg(s, SVGA_FIFO_PITCHLOCK)) {
    uint32_t fifo_pitch = le32_to_cpu(s->fifo[SVGA_FIFO_PITCHLOCK]);
    if (vmsvga_pending_pitch_valid(s, fifo_pitch)) {
      return fifo_pitch;
    };
  };
  return natural <= UINT32_MAX ? (uint32_t)natural : 0;
};
static inline bool vmsvga_pending_mode_valid(struct vmsvga_state_s *s) {
  uint32_t stride = vmsvga_pending_stride(s);
  return stride != 0 &&
         vmsvga_mode_valid(s, s->new_width, s->new_height, s->new_depth,
                           stride);
};
static inline uint32_t vmsvga_stride(struct vmsvga_state_s *s) {
  return s->active_valid ? s->active_stride : 0;
};
static inline bool vmsvga_try_commit_mode(struct vmsvga_state_s *s) {
  uint32_t stride;
  bool changed;
  if (!vmsvga_pending_mode_valid(s)) {
    return false;
  };
  stride = vmsvga_pending_stride(s);
  changed = !s->active_valid || s->active_width != s->new_width ||
            s->active_height != s->new_height ||
            s->active_depth != s->new_depth || s->active_stride != stride;
  s->active_valid = true;
  s->active_width = s->new_width;
  s->active_height = s->new_height;
  s->active_depth = s->new_depth;
  s->active_stride = stride;
  if (changed) {
    s->invalidated = true;
    VMVGA_TRACE_LOCAL(VMVGA_TRACE_STATE,
                       "MODE_COMMIT width=%u height=%u depth=%u stride=%u",
                       s->active_width, s->active_height, s->active_depth,
                       s->active_stride);
  };
  return true;
};
static inline void vmsvga_add_uncovered_dirty_span(
    struct vmsvga_state_s *s,
    const struct vmsvga_damage_rect_s *explicit_damage,
    uint32_t explicit_count, uint32_t x, uint32_t y, uint32_t w) {
  uint64_t left = x;
  uint64_t right = (uint64_t)x + w;
  while (left < right) {
    uint64_t covered_end = left;
    uint64_t next_covered = right;
    uint32_t i;
    for (i = 0; i < explicit_count; i++) {
      const struct vmsvga_damage_rect_s *rect = &explicit_damage[i];
      uint64_t rect_right = (uint64_t)rect->x + rect->w;
      uint64_t rect_bottom = (uint64_t)rect->y + rect->h;
      if ((uint64_t)y < rect->y || (uint64_t)y >= rect_bottom ||
          rect_right <= left || (uint64_t)rect->x >= right) {
        continue;
      };
      if ((uint64_t)rect->x <= left) {
        covered_end = MAX(covered_end, MIN(rect_right, right));
      } else {
        next_covered = MIN(next_covered, (uint64_t)rect->x);
      };
    };
    if (covered_end > left) {
      left = covered_end;
      continue;
    };
    vmsvga_damage_add(s, (uint32_t)left, y,
                      (uint32_t)(next_covered - left), 1);
    left = next_covered;
  };
};
static inline void
vmsvga_scan_vram_dirty(struct vmsvga_state_s *s,
                        const struct vmsvga_damage_rect_s *explicit_damage,
                        uint32_t explicit_count) {
  DirtyBitmapSnapshot *snap;
  uint32_t bypp = vmsvga_bytes_per_pixel(vmsvga_active_depth(s));
  uint32_t stride = vmsvga_stride(s);
  uint32_t row_bytes;
  hwaddr block_addr;
  hwaddr block_size;
  hwaddr page_addr;
  hwaddr visible_size;
  bool trace_dirty = VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_DIRTY);
  bool trace_range_open = false;
  hwaddr trace_range_start = 0;
  hwaddr trace_range_end = 0;
  if (!s->dirty_log_enabled) {
    return;
  };
  if (bypp == 0 || stride == 0 || vmsvga_active_width(s) == 0 ||
      vmsvga_active_height(s) == 0) {
    return;
  };
  row_bytes = vmsvga_active_width(s) * bypp;
  visible_size = (hwaddr)stride * vmsvga_active_height(s);
  if (visible_size == 0 || visible_size > s->vga.vram_size) {
    return;
  };
  snap = memory_region_snapshot_and_clear_dirty(
      &s->vga.vram, 0, visible_size, DIRTY_MEMORY_VGA);
  if (snap == NULL) {
    return;
  };
  if (!s->invalidated && memory_region_snapshot_get_dirty(
                            &s->vga.vram, snap, 0, visible_size)) {
    block_size = (hwaddr)TARGET_PAGE_SIZE * VMSVGA_DIRTY_BLOCK_PAGES;
    for (block_addr = 0; block_addr < visible_size;
         block_addr += block_size) {
      hwaddr block_end = MIN(block_addr + block_size, visible_size);
      if (!memory_region_snapshot_get_dirty(&s->vga.vram, snap, block_addr,
                                            block_end - block_addr)) {
        continue;
      };
      for (page_addr = block_addr; page_addr < block_end;
           page_addr += TARGET_PAGE_SIZE) {
        hwaddr page_end =
            MIN(page_addr + (hwaddr)TARGET_PAGE_SIZE, block_end);
        uint32_t first_y;
        uint32_t last_y;
        uint32_t y;
        if (!memory_region_snapshot_get_dirty(&s->vga.vram, snap, page_addr,
                                              page_end - page_addr)) {
          continue;
        };
        if (vmsvga_trace_flight_enabled()) {
          s->trace_now.dirty_pages++;
          vmsvga_trace_flight_activity(s);
        };
        if (trace_dirty) {
          if (trace_range_open && page_addr != trace_range_end) {
            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_DIRTY,
                "DIRTY pages=[0x%" PRIx64 ",0x%" PRIx64
                ") y=%u..%u stride=%u bypp=%u",
                (uint64_t)trace_range_start, (uint64_t)trace_range_end,
                (uint32_t)(trace_range_start / stride),
                (uint32_t)((trace_range_end - 1) / stride), stride, bypp);
            trace_range_open = false;
          };
          if (!trace_range_open) {
            trace_range_start = page_addr;
            trace_range_open = true;
          };
          trace_range_end = page_end;
        };
        first_y = page_addr / stride;
        last_y = (page_end - 1) / stride;
        for (y = first_y; y <= last_y; y++) {
          hwaddr row_addr = (hwaddr)y * stride;
          hwaddr start = MAX(page_addr, row_addr);
          hwaddr end = MIN(page_end, row_addr + row_bytes);
          uint32_t byte_x;
          uint32_t byte_end;
          uint32_t x;
          uint32_t end_x;
          if (start >= end) {
            continue;
          };
          byte_x = start - row_addr;
          byte_end = end - row_addr;
          x = byte_x / bypp;
          end_x = MIN((byte_end + bypp - 1) / bypp,
                      vmsvga_active_width(s));
          vmsvga_add_uncovered_dirty_span(
              s, explicit_damage, explicit_count, x, y, end_x - x);
        };
      };
    };
  };
  if (trace_range_open) {
    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_DIRTY,
        "DIRTY pages=[0x%" PRIx64 ",0x%" PRIx64
        ") y=%u..%u stride=%u bypp=%u",
        (uint64_t)trace_range_start, (uint64_t)trace_range_end,
        (uint32_t)(trace_range_start / stride),
        (uint32_t)((trace_range_end - 1) / stride), stride, bypp);
  };
  g_free(snap);
};
static inline void vmsvga_set_fifo_capabilities(struct vmsvga_state_s *s) {
#ifdef EXPCAPS
  s->ff = 0xffffffff;
  s->fc = 0xffffffff;
#else
  s->ff = SVGA_FIFO_FLAG_NONE;
  s->fc = SVGA_FIFO_CAP_FENCE | SVGA_FIFO_CAP_ACCELFRONT |
          SVGA_FIFO_CAP_PITCHLOCK | SVGA_FIFO_CAP_CURSOR_BYPASS_3 |
          SVGA_FIFO_CAP_RESERVE | SVGA_FIFO_CAP_SCREEN_OBJECT;
#endif
};
static inline bool vmsvga_fifo_has_reg(struct vmsvga_state_s *s,
                                       uint32_t reg) {
  uint32_t fifo_min;
  if (s->fifo == NULL) {
    return false;
  };
  fifo_min = le32_to_cpu(s->fifo[SVGA_FIFO_MIN]);
  return fifo_min <= s->fifo_size &&
         (uint64_t)(reg + 1) * sizeof(uint32_t) <= fifo_min;
};
static inline void vmsvga_publish_fifo_registers(struct vmsvga_state_s *s) {
  if (s->fifo == NULL) {
    return;
  };
  if (vmsvga_fifo_has_reg(s, SVGA_FIFO_CAPABILITIES)) {
    uint32_t fifo_caps = s->fc;
#ifndef EXPCAPS
    if (!vmsvga_fifo_has_reg(s, SVGA_FIFO_CURSOR_LAST_UPDATED)) {
      fifo_caps &= ~SVGA_FIFO_CAP_CURSOR_BYPASS_3;
    };
    if (!vmsvga_fifo_has_reg(s, SVGA_FIFO_RESERVED)) {
      fifo_caps &= ~SVGA_FIFO_CAP_RESERVE;
    };
#endif
    s->fifo[SVGA_FIFO_CAPABILITIES] = cpu_to_le32(fifo_caps);
  };
  if (vmsvga_fifo_has_reg(s, SVGA_FIFO_FLAGS)) {
    s->fifo[SVGA_FIFO_FLAGS] = cpu_to_le32(s->ff);
  };
  if (vmsvga_fifo_has_reg(s, SVGA_FIFO_3D_HWVERSION)) {
    s->fifo[SVGA_FIFO_3D_HWVERSION] = cpu_to_le32(
        s->svga3d_capable ? vmsvga3d_host_hwversion() : 0);
  };
  vmsvga3d_publish_fifo_caps(s);
  if (vmsvga_fifo_has_reg(s, SVGA_FIFO_FENCE)) {
    s->fifo[SVGA_FIFO_FENCE] = cpu_to_le32(s->fence);
  };
  if (vmsvga_trace_flight_enabled()) {
    uint32_t fifo_min = le32_to_cpu(s->fifo[SVGA_FIFO_MIN]);
    uint32_t fifo_max = le32_to_cpu(s->fifo[SVGA_FIFO_MAX]);
    uint32_t fifo_caps = vmsvga_fifo_has_reg(s, SVGA_FIFO_CAPABILITIES)
                             ? le32_to_cpu(s->fifo[SVGA_FIFO_CAPABILITIES])
                             : 0;
    uint32_t guest_hw = vmsvga_fifo_has_reg(s, SVGA_FIFO_GUEST_3D_HWVERSION)
                            ? le32_to_cpu(s->fifo[SVGA_FIFO_GUEST_3D_HWVERSION])
                            : 0;
    uint32_t host_hw = vmsvga_fifo_has_reg(s, SVGA_FIFO_3D_HWVERSION)
                           ? le32_to_cpu(s->fifo[SVGA_FIFO_3D_HWVERSION])
                           : 0;
    uint32_t revised_hw = vmsvga_fifo_has_reg(s, SVGA_FIFO_3D_HWVERSION_REVISED)
                              ? le32_to_cpu(s->fifo[SVGA_FIFO_3D_HWVERSION_REVISED])
                              : 0;
    uint32_t caps_len = vmsvga_fifo_has_reg(s, SVGA_FIFO_3D_CAPS_LAST)
                            ? le32_to_cpu(s->fifo[SVGA_FIFO_3D_CAPS])
                            : 0;
    uint32_t caps_type = vmsvga_fifo_has_reg(s, SVGA_FIFO_3D_CAPS_LAST)
                             ? le32_to_cpu(s->fifo[SVGA_FIFO_3D_CAPS + 1])
                             : 0;
    fprintf(stderr,
            "VMVGA-NEGOTIATE capable=%u fifo_min=0x%08x fifo_max=0x%08x "
            "fifo_caps=0x%08x guest3d=0x%08x host3d=0x%08x revised3d=0x%08x "
            "caps_len=%u caps_type=%u fc=0x%08x ff=0x%08x\n",
            s->svga3d_capable, fifo_min, fifo_max, fifo_caps, guest_hw,
            host_hw, revised_hw, caps_len, caps_type, s->fc, s->ff);
    s->trace_3d_version_seen = true;
    s->trace_last_guest_3d_hwversion = guest_hw;
    s->trace_last_host_3d_hwversion = host_hw;
    s->trace_last_revised_3d_hwversion = revised_hw;
  };
};
static inline void vmsvga_fifo_set_busy(struct vmsvga_state_s *s,
                                        bool busy) {
  if (vmsvga_fifo_has_reg(s, SVGA_FIFO_BUSY)) {
    s->fifo[SVGA_FIFO_BUSY] = cpu_to_le32(busy ? 1 : 0);
  };
};
static inline void vmsvga_fifo_discard_pending(struct vmsvga_state_s *s) {
  uint32_t min;
  uint32_t max;
  uint32_t next;
  if (s->fifo == NULL) {
    return;
  };
  min = le32_to_cpu(s->fifo[SVGA_FIFO_MIN]);
  max = le32_to_cpu(s->fifo[SVGA_FIFO_MAX]);
  next = le32_to_cpu(s->fifo[SVGA_FIFO_NEXT_CMD]);
  if (((min | max | next) & 3) == 0 &&
      min >= sizeof(uint32_t) * 4 && min < max && max <= s->fifo_size &&
      next >= min && next < max) {
    s->fifo_stop = next;
    s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(next);
  };
  s->sync = 0;
  vmsvga_fifo_upload_reset(s);
  vmsvga_fifo_set_busy(s, false);
};
static inline void vmsvga_trace_3d_versions(struct vmsvga_state_s *s) {
  uint32_t guest_hw;
  uint32_t host_hw;
  uint32_t revised_hw;

  if (!vmsvga_trace_flight_enabled() || s->fifo == NULL || !s->config) {
    return;
  };
  guest_hw = vmsvga_fifo_has_reg(s, SVGA_FIFO_GUEST_3D_HWVERSION)
                 ? le32_to_cpu(s->fifo[SVGA_FIFO_GUEST_3D_HWVERSION])
                 : 0;
  host_hw = vmsvga_fifo_has_reg(s, SVGA_FIFO_3D_HWVERSION)
                ? le32_to_cpu(s->fifo[SVGA_FIFO_3D_HWVERSION])
                : 0;
  revised_hw = vmsvga_fifo_has_reg(s, SVGA_FIFO_3D_HWVERSION_REVISED)
                   ? le32_to_cpu(s->fifo[SVGA_FIFO_3D_HWVERSION_REVISED])
                   : 0;
  if (!s->trace_3d_version_seen ||
      guest_hw != s->trace_last_guest_3d_hwversion ||
      host_hw != s->trace_last_host_3d_hwversion ||
      revised_hw != s->trace_last_revised_3d_hwversion) {
    fprintf(stderr,
            "VMVGA-3D-VERSION guest=0x%08x host=0x%08x revised=0x%08x "
            "capable=%u\n",
            guest_hw, host_hw, revised_hw, s->svga3d_capable);
    s->trace_3d_version_seen = true;
    s->trace_last_guest_3d_hwversion = guest_hw;
    s->trace_last_host_3d_hwversion = host_hw;
    s->trace_last_revised_3d_hwversion = revised_hw;
  };
};
#ifdef CONFIG_PIXMAN
static inline void vmsvga_palette_update_entry(struct vmsvga_state_s *s,
                                                uint32_t entry) {
  uint32_t base = entry * 3;
  uint32_t red = s->svgapalettebase[base] & 0xff;
  uint32_t green = s->svgapalettebase[base + 1] & 0xff;
  uint32_t blue = s->svgapalettebase[base + 2] & 0xff;
  s->indexed_palette.rgba[entry] =
      0xff000000u | (red << 16) | (green << 8) | blue;
};
static inline void vmsvga_palette_rebuild(struct vmsvga_state_s *s) {
  uint32_t entry;
  memset(&s->indexed_palette, 0, sizeof(s->indexed_palette));
  s->indexed_palette.color = true;
  for (entry = 0; entry < VMSVGA_PSEUDOCOLOR_ENTRIES; entry++) {
    vmsvga_palette_update_entry(s, entry);
  };
};
#endif
static inline void vmsvga_check_size(struct vmsvga_state_s *s) {
  VPRINT("vmsvga_check_size was just executed\n");
  DisplaySurface *surface = qemu_console_surface(s->vga.con);
  uint8_t *scanout;
  uint32_t stride;
  if (!s->active_valid) {
    return;
  };
  if (s->screen_defined) {
    size_t scanout_size;
    uint32_t screen_stride;

    if (!vmsvga_screen_storage(s, &scanout, &scanout_size, &screen_stride) ||
        screen_stride != s->active_stride) {
      return;
    };
    (void)scanout_size;
  } else {
    scanout = vmsvga_svga_vram_ptr(s);
  };
  stride = s->active_stride;
  if (!s->svga_surface_bound ||
      s->active_width != surface_width(surface) ||
      s->active_height != surface_height(surface) ||
      stride != surface_stride(surface) ||
      s->active_depth != surface_bits_per_pixel(surface) ||
      surface_data(surface) != scanout) {
    pixman_format_code_t format = vmsvga_pixman_format(s->active_depth);
#ifdef VERBOSE
    int old_width = surface_width(surface);
    int old_height = surface_height(surface);
    int old_depth = surface_bits_per_pixel(surface);
    int old_stride = surface_stride(surface);
#endif
    VMVGA_QEMU_TRACE(trace_vmware_setmode(s->active_width, s->active_height,
                                           s->active_depth));
    surface = qemu_create_displaysurface_from(
        s->active_width, s->active_height, format, stride, scanout);
#ifdef CONFIG_PIXMAN
    if (s->active_depth == 8) {
      pixman_image_set_indexed(surface->image, &s->indexed_palette);
    };
#endif
    VPRINT("vmsvga_check_size: old_width: %u, old_height: %u, old_depth: %u, "
           "old_stride: %u, active_width: %u, active_height: %u, "
           "active_depth: %u, new_format: %u, active_stride: %u\n",
           old_width, old_height, old_depth, old_stride, s->active_width,
           s->active_height, s->active_depth, format, stride);
    vmvga_console_set_surface(s->vga.con, surface);
    s->svga_surface_bound = true;
    s->invalidated = true;
    VMVGA_TRACE_LOCAL(VMVGA_TRACE_STATE,
                       "SCANOUT source=%s width=%u height=%u depth=%u stride=%u",
                       s->screen_defined ? "screen" : "bar1", s->active_width,
                       s->active_height, s->active_depth, stride);
  };
};
static inline uint32_t vmsvga_read_width(struct vmsvga_state_s *s) {
  DisplaySurface *surface;
  if (s->new_width != 0) {
    return s->new_width;
  };
  surface = qemu_console_surface(s->vga.con);
  return surface && surface_width(surface) > 0 ? surface_width(surface) : 1024;
};
static inline uint32_t vmsvga_read_height(struct vmsvga_state_s *s) {
  DisplaySurface *surface;
  if (s->new_height != 0) {
    return s->new_height;
  };
  surface = qemu_console_surface(s->vga.con);
  return surface && surface_height(surface) > 0 ? surface_height(surface) : 768;
};
static uint32_t vmsvga_value_read(void *opaque, uint32_t address) {
  VPRINT("vmsvga_value_read was just executed\n");
  uint32_t ret;
  uint32_t caps;
  struct vmsvga_state_s *s = opaque;
  struct pci_vmsvga_state_s *pci_vmsvga =
      container_of(s, struct pci_vmsvga_state_s, chip);
  if (s->index >= SVGA_REG_PALETTE_MIN && s->index <= SVGA_REG_PALETTE_MAX) {
    ret = s->svgapalettebase[s->index - SVGA_REG_PALETTE_MIN];
    VMVGA_QEMU_TRACE(trace_vmware_palette_read(s->index, ret));
    return ret;
  };
  if (s->index >= SVGA_SCRATCH_BASE &&
      s->index < SVGA_SCRATCH_BASE + s->scratch_size) {
    ret = s->scratch[s->index - SVGA_SCRATCH_BASE];
    VMVGA_QEMU_TRACE(trace_vmware_scratch_read(s->index, ret));
    return ret;
  };
  VPRINT("Unknown register %u\n", s->index);
  switch (s->index) {
  case SVGA_REG_FENCE_GOAL:
    ret = 0;
    VPRINT("SVGA_REG_FENCE_GOAL register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_ID:
    ret = s->svgaid;
    VPRINT("SVGA_REG_ID register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_ENABLE:
    ret = s->enable | (s->hidden ? SVGA_REG_ENABLE_HIDE : 0);
    VPRINT("SVGA_REG_ENABLE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_WIDTH:
    ret = vmsvga_read_width(s);
    VPRINT("SVGA_REG_WIDTH register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_HEIGHT:
    ret = vmsvga_read_height(s);
    VPRINT("SVGA_REG_HEIGHT register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MAX_WIDTH:
    ret = VMSVGA_MAX_WIDTH;
    VPRINT("SVGA_REG_MAX_WIDTH register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MAX_HEIGHT:
    ret = VMSVGA_MAX_HEIGHT;
    VPRINT("SVGA_REG_MAX_HEIGHT register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_SCREENTARGET_MAX_WIDTH:
    ret = VMSVGA_MAX_WIDTH;
    VPRINT(
        "SVGA_REG_SCREENTARGET_MAX_WIDTH register %u with the return of %u\n",
        s->index, ret);
    break;
  case SVGA_REG_SCREENTARGET_MAX_HEIGHT:
    ret = VMSVGA_MAX_HEIGHT;
    VPRINT(
        "SVGA_REG_SCREENTARGET_MAX_HEIGHT register %u with the return of %u\n",
        s->index, ret);
    break;
  case SVGA_REG_BITS_PER_PIXEL:
    ret = s->new_depth ? s->new_depth : VMSVGA_HOST_BITS_PER_PIXEL;
    VPRINT("SVGA_REG_BITS_PER_PIXEL register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_HOST_BITS_PER_PIXEL:
    ret = VMSVGA_HOST_BITS_PER_PIXEL;
    VPRINT("SVGA_REG_HOST_BITS_PER_PIXEL register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DEPTH:
    ret = s->new_depth == 32 ? 24 : (s->new_depth ? s->new_depth : 24);
    VPRINT("SVGA_REG_DEPTH register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_PSEUDOCOLOR:
    if (s->new_depth == 8) {
      ret = 1;
    } else {
      ret = 0;
    };
    VPRINT("SVGA_REG_PSEUDOCOLOR register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_RED_MASK:
    if (s->new_depth == 8) {
      ret = 0x00000007;
    } else if (s->new_depth == 15) {
      ret = 0x00007c00;
    } else if (s->new_depth == 16) {
      ret = 0x0000f800;
    } else {
      ret = 0x00ff0000;
    };
    VPRINT("SVGA_REG_RED_MASK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_GREEN_MASK:
    if (s->new_depth == 8) {
      ret = 0x00000038;
    } else if (s->new_depth == 15) {
      ret = 0x000003e0;
    } else if (s->new_depth == 16) {
      ret = 0x000007e0;
    } else {
      ret = 0x0000ff00;
    };
    VPRINT("SVGA_REG_GREEN_MASK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_BLUE_MASK:
    if (s->new_depth == 8) {
      ret = 0x000000c0;
    } else if (s->new_depth == 15) {
      ret = 0x0000001f;
    } else if (s->new_depth == 16) {
      ret = 0x0000001f;
    } else {
      ret = 0x000000ff;
    };
    VPRINT("SVGA_REG_BLUE_MASK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_BYTES_PER_LINE:
    if (vmsvga_pending_mode_valid(s)) {
      ret = vmsvga_pending_stride(s);
    } else if (s->active_valid) {
      ret = vmsvga_stride(s);
    } else {
      DisplaySurface *surface = qemu_console_surface(s->vga.con);
      ret = surface != NULL && surface_stride(surface) > 0
                ? surface_stride(surface)
                : 0;
    };
    VPRINT("SVGA_REG_BYTES_PER_LINE register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_FB_START:
    ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 1);
    VPRINT("SVGA_REG_FB_START register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_FB_OFFSET:
    ret = 0;
    VPRINT("SVGA_REG_FB_OFFSET register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_BLANK_SCREEN_TARGETS:
    ret = 0;
    VPRINT("SVGA_REG_BLANK_SCREEN_TARGETS register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_VRAM_SIZE:
    ret = s->vga.vram_size;
    VPRINT("SVGA_REG_VRAM_SIZE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_FB_SIZE:
    ret = s->vga.vram_size;
    VPRINT("SVGA_REG_FB_SIZE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MOB_MAX_SIZE:
    ret = 0;
    VPRINT("SVGA_REG_MOB_MAX_SIZE register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_GBOBJECT_MEM_SIZE_KB:
    ret = (uint32_t)(vmsvga_surface_memory_size(s) / 1024);
    VPRINT("SVGA_REG_GBOBJECT_MEM_SIZE_KB register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB:
    ret = (uint32_t)(vmsvga_surface_memory_size(s) / 1024);
    VPRINT("SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_MSHINT:
    ret = 0;
    VPRINT("SVGA_REG_MSHINT register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM:
    ret = s->vga.vram_size;
    VPRINT("SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM register %u with the return "
           "of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_CAPABILITIES:
#ifdef EXPCAPS
    caps = 0xffffffff;
#else
    caps = SVGA_CAP_RECT_FILL | SVGA_CAP_RECT_COPY | SVGA_CAP_RECT_PAT_FILL |
           SVGA_CAP_LEGACY_OFFSCREEN | SVGA_CAP_RASTER_OP | SVGA_CAP_CURSOR |
           SVGA_CAP_CURSOR_BYPASS | SVGA_CAP_CURSOR_BYPASS_2 |
           SVGA_CAP_ALPHA_CURSOR | SVGA_CAP_GLYPH | SVGA_CAP_GLYPH_CLIPPING |
           SVGA_CAP_OFFSCREEN_1 | SVGA_CAP_ALPHA_BLEND | SVGA_CAP_3D |
           SVGA_CAP_GMR | SVGA_CAP_EXTENDED_FIFO |
           SVGA_CAP_PITCHLOCK | SVGA_CAP_IRQMASK | SVGA_CAP_TRACES;
#ifdef CONFIG_PIXMAN
    caps |= SVGA_CAP_8BIT_EMULATION;
#endif
#endif
    if (!s->svga3d_capable) {
      caps &= ~SVGA_CAP_3D;
    };
    ret = caps;
    VPRINT("SVGA_REG_CAPABILITIES register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_CAP2:
#ifdef EXPCAPS
    ret = 0xffffffff;
#else
    ret = SVGA_CAP2_NONE;
#endif
    VPRINT("SVGA_REG_CAP2 register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_MEM_START:
    ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 2);
    VPRINT("SVGA_REG_MEM_START register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MEM_SIZE:
    ret = s->fifo_size;
    VPRINT("SVGA_REG_MEM_SIZE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CONFIG_DONE:
    ret = s->config;
    VPRINT("SVGA_REG_CONFIG_DONE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_SYNC:
    ret = 0;
    VPRINT("SVGA_REG_SYNC register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_BUSY:
    if (vmsvga_trace_flight_enabled()) {
      s->trace_now.busy_reads++;
      vmsvga_trace_flight_activity(s);
    };
    if (s->sync) {
      vmsvga_try_commit_mode(s);
      vmsvga_fifo_run(s, false);
    };
    ret = s->sync;
    /* vmware_value_read already traces this register when enabled. */
    VPRINT("SVGA_REG_BUSY register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_GUEST_ID:
    ret = s->guest;
    VPRINT("SVGA_REG_GUEST_ID register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CURSOR_ID:
    ret = s->cursor;
    VPRINT("SVGA_REG_CURSOR_ID register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CURSOR_X:
    ret = s->cursor_x;
    VPRINT("SVGA_REG_CURSOR_X register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CURSOR_Y:
    ret = s->cursor_y;
    VPRINT("SVGA_REG_CURSOR_Y register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CURSOR_ON:
    ret = s->cursor_on;
    VPRINT("SVGA_REG_CURSOR_ON register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_SCRATCH_SIZE:
    ret = s->scratch_size;
    VPRINT("SVGA_REG_SCRATCH_SIZE register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_MEM_REGS:
    ret = SVGA_FIFO_NUM_REGS;
    VPRINT("SVGA_REG_MEM_REGS register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_NUM_DISPLAYS:
    ret = 1;
    VPRINT("SVGA_REG_NUM_DISPLAYS register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_PITCHLOCK:
    ret = s->pitchlock;
    VPRINT("SVGA_REG_PITCHLOCK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_IRQMASK:
    ret = s->irq_mask;
    VPRINT("SVGA_REG_IRQMASK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_NUM_GUEST_DISPLAYS:
    ret = s->num_gd;
    VPRINT("SVGA_REG_NUM_GUEST_DISPLAYS register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_ID:
    ret = s->display_id;
    VPRINT("SVGA_REG_DISPLAY_ID register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_DISPLAY_IS_PRIMARY:
    ret = s->display_id == 0 ? s->disp_prim : 0;
    VPRINT("SVGA_REG_DISPLAY_IS_PRIMARY register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_POSITION_X:
    ret = s->display_id == 0 ? s->disp_x : 0;
    VPRINT("SVGA_REG_DISPLAY_POSITION_X register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_POSITION_Y:
    ret = s->display_id == 0 ? s->disp_y : 0;
    VPRINT("SVGA_REG_DISPLAY_POSITION_Y register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_WIDTH:
    ret = s->display_id == 0
              ? (s->disp_width != 0 ? s->disp_width : vmsvga_read_width(s))
              : 0;
    VPRINT("SVGA_REG_DISPLAY_WIDTH register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_HEIGHT:
    ret = s->display_id == 0
              ? (s->disp_height != 0 ? s->disp_height : vmsvga_read_height(s))
              : 0;
    VPRINT("SVGA_REG_DISPLAY_HEIGHT register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_GMRS_MAX_PAGES:
    ret = 196608;
    VPRINT("SVGA_REG_GMRS_MAX_PAGES register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_GMR_ID:
    ret = s->gmrid;
    VPRINT("SVGA_REG_GMR_ID register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_GMR_MAX_IDS:
    ret = (uint32_t)ARRAY_SIZE(s->gmrs);
    VPRINT("SVGA_REG_GMR_MAX_IDS register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:
    ret = VMSVGA_GMR_MAX_DESCRIPTOR_LENGTH;
    VPRINT("SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_TRACES:
    ret = s->traces;
    VPRINT("SVGA_REG_TRACES register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_COMMAND_LOW:
    ret = s->cmd_low;
    VPRINT("SVGA_REG_COMMAND_LOW register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_COMMAND_HIGH:
    ret = s->cmd_high;
    VPRINT("SVGA_REG_COMMAND_HIGH register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DEV_CAP:
    ret = s->devcap_val;
    VPRINT("SVGA_REG_DEV_CAP register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MEMORY_SIZE:
    ret = vmsvga_memory_size(s);
    VPRINT("SVGA_REG_MEMORY_SIZE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_SCREENDMA:
    ret = 1;
    VPRINT("SVGA_REG_SCREENDMA register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_FENCE:
    ret = 0;
    VPRINT("SVGA_REG_FENCE register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_FIFO_CAPS:
    ret = 0;
    VPRINT("SVGA_REG_FIFO_CAPS register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CURSOR_MAX_DIMENSION:
    ret = VMSVGA_CURSOR_MAX_DIMENSION;
    VPRINT("SVGA_REG_CURSOR_MAX_DIMENSION register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_CURSOR_MAX_BYTE_SIZE:
    ret = VMSVGA_CURSOR_MAX_BYTE_SIZE;
    VPRINT("SVGA_REG_CURSOR_MAX_BYTE_SIZE register %u with the return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_CURSOR_MOBID:
    ret = -1;
    VPRINT("SVGA_REG_CURSOR_MOBID register %u with the return of %u\n",
           s->index, ret);
    break;
  default:
    ret = 0;
    VPRINT("default register %u with the return of %u\n", s->index, ret);
    break;
  };
  vmsvga_trace_key_reg_read(s, s->index, ret);
  if (s->index != SVGA_REG_BUSY) {
    VMVGA_QEMU_TRACE(trace_vmware_value_read(s->index, ret));
  };
  return ret;
};
static void vmsvga_value_write(void *opaque, uint32_t address, uint32_t value) {
  VPRINT("vmsvga_value_write was just executed\n");
  struct vmsvga_state_s *s = opaque;
  if (s->index >= SVGA_REG_PALETTE_MIN && s->index <= SVGA_REG_PALETTE_MAX) {
    uint32_t palette_offset = s->index - SVGA_REG_PALETTE_MIN;
    VMVGA_QEMU_TRACE(trace_vmware_palette_write(s->index, value));
    s->svgapalettebase[palette_offset] = value & 0xff;
#ifdef CONFIG_PIXMAN
    vmsvga_palette_update_entry(s, palette_offset / 3);
    if (vmsvga_active_depth(s) == 8) {
      s->invalidated = true;
    };
#endif
    vmsvga_cursor_palette_changed(s);
    return;
  };
  if (s->index >= SVGA_SCRATCH_BASE &&
      s->index < SVGA_SCRATCH_BASE + s->scratch_size) {
    VMVGA_QEMU_TRACE(trace_vmware_scratch_write(s->index, value));
    s->scratch[s->index - SVGA_SCRATCH_BASE] = value;
    return;
  };
  if (s->index != SVGA_REG_SYNC) {
    VMVGA_QEMU_TRACE(trace_vmware_value_write(s->index, value));
  };
  VPRINT("Unknown register %u with the value of %u\n", s->index, value);
  switch (s->index) {
  case SVGA_REG_ID:
    if (value == SVGA_ID_0 || value == SVGA_ID_1 || value == SVGA_ID_2) {
      s->svgaid = value;
    };
    VPRINT("SVGA_REG_ID register %u with the value of %u\n", s->index, value);
    break;
  case SVGA_REG_FENCE_GOAL:
    VPRINT("SVGA_REG_FENCE_GOAL register %u ignored value %u\n", s->index,
           value);
    break;
  case SVGA_REG_ENABLE: {
    bool was_enabled = s->enable;
    bool was_hidden = s->hidden;
    bool enabled = !!(value & SVGA_REG_ENABLE_ENABLE);
    if (!was_enabled && enabled) {
      vmsvga_legacy_vga_enter(s);
    } else if (was_enabled && !enabled) {
      /* VGA needs dirty logging before selecting its isolated framebuffer. */
      vmsvga_set_dirty_log(s, true);
      vmsvga_fifo_discard_pending(s);
      vmsvga_legacy_vga_leave(s);
    };
    s->enable = enabled;
    s->hidden = s->enable && !!(value & SVGA_REG_ENABLE_HIDE);
    if (s->enable) {
      vmsvga_try_commit_mode(s);
    };
    vmsvga_update_dirty_log(s);
    if (was_hidden != s->hidden) {
      s->cursor_dirty = true;
      cursor_update_from_fifo(s);
    };
    s->invalidated = true;
    /* vmware_value_write already traces this register when enabled. */
    VPRINT("SVGA_REG_ENABLE register %u with the value of %u\n", s->index,
           value);
    break;
  };
  case SVGA_REG_WIDTH:
    if (value >= 1 && value <= VMSVGA_MAX_WIDTH) {
      s->new_width = value;
      vmsvga_try_commit_mode(s);
    };
    VPRINT("SVGA_REG_WIDTH register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_HEIGHT:
    if (value >= 1 && value <= VMSVGA_MAX_HEIGHT) {
      s->new_height = value;
      vmsvga_try_commit_mode(s);
    };
    VPRINT("SVGA_REG_HEIGHT register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_BITS_PER_PIXEL:
#ifdef CONFIG_PIXMAN
    if (value == 8 || value == VMSVGA_HOST_BITS_PER_PIXEL) {
#else
    if (value == VMSVGA_HOST_BITS_PER_PIXEL) {
#endif
      s->new_depth = value;
      vmsvga_try_commit_mode(s);
    };
    VPRINT("SVGA_REG_BITS_PER_PIXEL register %u with the value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_CONFIG_DONE: {
    bool was_config = s->config;
    s->config = !!value;
    if (s->config) {
      vmsvga_publish_fifo_registers(s);
      if (!was_config) {
        vmsvga_fifo_set_busy(s, false);
      };
      vmsvga_try_commit_mode(s);
    } else if (was_config) {
      s->sync = 0;
      vmsvga_fifo_upload_reset(s);
      vmsvga_fifo_set_busy(s, false);
    };
    vmsvga_update_dirty_log(s);
    /* vmware_value_write already traces this register when enabled. */
    VPRINT("SVGA_REG_CONFIG_DONE register %u with the value of %u\n", s->index,
           value);
    break;
  };
  case SVGA_REG_SYNC:
    if (vmsvga_trace_flight_enabled()) {
      s->trace_now.sync_writes++;
      if (s->fifo != NULL && s->config &&
          vmsvga_fifo_has_reg(s, SVGA_FIFO_BUSY)) {
        if (le32_to_cpu(s->fifo[SVGA_FIFO_BUSY])) {
          s->trace_now.sync_fifo_busy_one++;
        } else {
          s->trace_now.sync_fifo_busy_zero++;
        };
      };
      vmsvga_trace_flight_activity(s);
    };
    if (s->enable && s->config) {
      vmsvga_try_commit_mode(s);
      s->sync = 1;
      vmsvga_fifo_run(s, false);
    };
    /* vmware_value_write already traces this register when enabled. */
    VPRINT("SVGA_REG_SYNC register %u with the value of %u\n", s->index, value);
    break;
  case SVGA_REG_BUSY:
    VPRINT("SVGA_REG_BUSY register %u with the value of %u\n", s->index, value);
    break;
  case SVGA_REG_GUEST_ID:
    s->guest = value;
    VPRINT("SVGA_REG_GUEST_ID register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_CURSOR_ID:
    s->cursor = value;
    VPRINT("SVGA_REG_CURSOR_ID register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_CURSOR_X:
    s->cursor_x = value;
    VPRINT("SVGA_REG_CURSOR_X register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_CURSOR_Y:
    s->cursor_y = value;
    VPRINT("SVGA_REG_CURSOR_Y register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_CURSOR_ON:
    if (value <= SVGA_CURSOR_ON_RESTORE_TO_FB) {
      s->cursor_on = value;
      vmsvga_cursor_commit_indexed(s);
    };
    VPRINT("SVGA_REG_CURSOR_ON register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_BYTES_PER_LINE:
    VPRINT("SVGA_REG_BYTES_PER_LINE register %u is read-only\n", s->index);
    break;
  case SVGA_REG_PITCHLOCK:
    if (value == 0 || value <= s->vga.vram_size) {
      s->pitchlock = value;
      vmsvga_try_commit_mode(s);
    };
    VPRINT("SVGA_REG_PITCHLOCK register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_IRQMASK: {
    s->irq_mask = value;
#ifndef RAISE_IRQ_OFF
    struct pci_vmsvga_state_s *pci_vmsvga =
        container_of(s, struct pci_vmsvga_state_s, chip);
    pci_set_irq(PCI_DEVICE(pci_vmsvga), !!(s->irq_status & s->irq_mask));
#endif
    VPRINT("SVGA_REG_IRQMASK register %u with the value of %u\n", s->index,
           value);
    break;
  }
  case SVGA_REG_NUM_GUEST_DISPLAYS:
    s->num_gd = MIN(value, 1u);
    VPRINT("SVGA_REG_NUM_GUEST_DISPLAYS register %u with the value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_IS_PRIMARY:
    if (s->display_id == 0) {
      s->disp_prim = !!value;
    };
    VPRINT("SVGA_REG_DISPLAY_IS_PRIMARY register %u with the value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_POSITION_X:
    if (s->display_id == 0) {
      s->disp_x = value;
    };
    VPRINT("SVGA_REG_DISPLAY_POSITION_X register %u with the value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_POSITION_Y:
    if (s->display_id == 0) {
      s->disp_y = value;
    };
    VPRINT("SVGA_REG_DISPLAY_POSITION_Y register %u with the value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_ID:
    if (value == 0 || value == UINT32_MAX) {
      s->display_id = value;
    };
    VPRINT("SVGA_REG_DISPLAY_ID register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_DISPLAY_WIDTH:
    if (s->display_id == 0 && value <= VMSVGA_MAX_WIDTH) {
      s->disp_width = value;
    };
    VPRINT("SVGA_REG_DISPLAY_WIDTH register %u with the value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_HEIGHT:
    if (s->display_id == 0 && value <= VMSVGA_MAX_HEIGHT) {
      s->disp_height = value;
    };
    VPRINT("SVGA_REG_DISPLAY_HEIGHT register %u with the value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_TRACES:
    s->traces = !!value;
    vmsvga_update_dirty_log(s);
    VPRINT("SVGA_REG_TRACES register %u with the value of %u\n", s->index,
           s->traces);
    break;
  case SVGA_REG_COMMAND_LOW:
    s->cmd_low = value;
    VPRINT("SVGA_REG_COMMAND_LOW register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_COMMAND_HIGH:
    s->cmd_high = value;
    VPRINT("SVGA_REG_COMMAND_HIGH register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_GMR_ID:
    s->gmrid = value;
    VPRINT("SVGA_REG_GMR_ID register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_GMR_DESCRIPTOR:
    s->gmrdesc = value;
    vmsvga_gmr_descriptor_write(s, value);
    VPRINT("SVGA_REG_GMR_DESCRIPTOR register %u with the value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DEV_CAP:
    s->devcap_val = s->svga3d_capable ? vmsvga3d_get_devcap(value) : 0;
    vmsvga_trace_devcap(s, value, s->devcap_val);
    VPRINT("SVGA_REG_DEV_CAP register %u with the value of %u\n", s->index,
           value);
    break;
  default:
    VPRINT("default register %u with the value of %u\n", s->index, value);
  };
  vmsvga_trace_key_reg_write(s, s->index, value);
  return;
};
static uint32_t vmsvga_irqstatus_read(void *opaque, uint32_t address) {
  VPRINT("vmsvga_irqstatus_read was just executed\n");
  struct vmsvga_state_s *s = opaque;
  VPRINT("vmsvga_irqstatus_read %u %u\n", address, s->irq_status);
  return s->irq_status;
};
static void vmsvga_irqstatus_write(void *opaque, uint32_t address,
                                   uint32_t data) {
  VPRINT("vmsvga_irqstatus_write was just executed\n");
  struct vmsvga_state_s *s = opaque;
  s->irq_status &= ~data;
  VPRINT("vmsvga_irqstatus_write %u %u\n", address, data);
#ifndef RAISE_IRQ_OFF
  struct pci_vmsvga_state_s *pci_vmsvga =
      container_of(s, struct pci_vmsvga_state_s, chip);
  pci_set_irq(PCI_DEVICE(pci_vmsvga), !!(s->irq_status & s->irq_mask));
#endif
};
static uint32_t vmsvga_bios_read(void *opaque, uint32_t address) {
  VPRINT("vmsvga_bios_read was just executed\n");
  (void)opaque;
  VPRINT("vmsvga_bios_read %u -> 0\n", address);
  return 0;
};
static void vmsvga_bios_write(void *opaque, uint32_t address, uint32_t data) {
  VPRINT("vmsvga_bios_write was just executed\n");
  (void)opaque;
  VPRINT("vmsvga_bios_write %u ignored value %u\n", address, data);
};
static inline void vmsvga_trace_display_path(
    struct vmsvga_state_s *s, uint32_t path, bool mode_valid) {
  const char *name;
  if (s->trace_display_path == path) {
    return;
  };
  s->trace_display_path = path;
  switch (path) {
  case VMSVGA_TRACE_DISPLAY_HIDDEN:
    name = "hidden";
    break;
  case VMSVGA_TRACE_DISPLAY_SVGA:
    name = "svga";
    break;
  case VMSVGA_TRACE_DISPLAY_VGA:
    name = "vga";
    break;
  default:
    name = "unknown";
    break;
  };
  VMVGA_TRACE_LOCAL(
      VMVGA_TRACE_STATE,
      "DISPLAY path=%s enable=%u config=%u hidden=%u mode_valid=%u",
      name, s->enable, s->config, s->hidden, mode_valid);
};
static VMVGA_GFX_UPDATE_RET vmsvga_update_display(void *opaque) {
  // VPRINT("vmsvga_update_display was just executed\n");
  struct vmsvga_state_s *s = opaque;
  bool pending_valid;
  bool scan_dirty;

  if (s->enable && !s->screen_defined) {
    vmsvga_try_commit_mode(s);
  };
  pending_valid = vmsvga_pending_mode_valid(s);

  /* FIFO execution is independent of whether the requested mode is currently
   * a complete/usable scanout tuple. */
  if (s->enable && s->config) {
    if (vmsvga_fifo_pending(s)) {
      vmsvga_fifo_run(s, false);
    } else {
      cursor_update_from_fifo(s);
    };
  } else {
    cursor_update_from_fifo(s);
  };

  if (!s->enable) {
    vmsvga_trace_display_path(s, VMSVGA_TRACE_DISPLAY_VGA, pending_valid);
    s->svga_surface_bound = false;
    VMVGA_GFX_UPDATE_FALLBACK(s);
    goto done;
  };

  if (s->hidden) {
    vmsvga_trace_display_path(s, VMSVGA_TRACE_DISPLAY_HIDDEN, pending_valid);
    s->damage_count = 0;
    goto done;
  };

  vmsvga_trace_display_path(s, VMSVGA_TRACE_DISPLAY_SVGA, pending_valid);
  if (!s->active_valid) {
    /* ENABLE still selects SVGA. Keep the previous host surface rather than
     * implicitly exposing VGA while requested registers are incomplete. */
    s->damage_count = 0;
    goto done;
  };

  vmsvga_check_size(s);
  scan_dirty = !s->screen_defined && vmsvga_effective_traces(s);
  if (scan_dirty) {
    struct vmsvga_damage_rect_s explicit_damage[VMSVGA_DAMAGE_RECTS];
    uint32_t explicit_count = s->damage_count;
    if (s->test_marker && !s->marker_logged) {
      fprintf(stderr, "vmware-vga: BAR1 trace scanout active "
                      "(test marker enabled)\n");
      s->marker_logged = true;
    };
    if (explicit_count != 0) {
      memcpy(explicit_damage, s->damage,
             explicit_count * sizeof(explicit_damage[0]));
    };
    vmsvga_scan_vram_dirty(s, explicit_damage, explicit_count);
  };
  if (s->invalidated) {
    s->damage_count = 0;
    VMVGA_TRACE_LOCAL(VMVGA_TRACE_DRAW,
                       "DAMAGE_FULL x=0 y=0 w=%u h=%u",
                       s->active_width, s->active_height);
    vmvga_console_update(s->vga.con, 0, 0, s->active_width,
                         s->active_height);
    s->invalidated = false;
  } else {
    vmsvga_damage_flush(s);
  };
done:
  vmsvga_trace_3d_versions(s);
  vmsvga_trace_flight_report(s, "display", false);
  VMVGA_GFX_UPDATE_DONE();
};
static void vmsvga_reset(DeviceState *dev) {
  VPRINT("vmsvga_reset was just executed\n");
  struct pci_vmsvga_state_s *pci = VMWARE_SVGA(dev);
  struct vmsvga_state_s *s = &pci->chip;
  VMVGA_TRACE_LOCAL(
      VMVGA_TRACE_STATE,
      "RESET enable=%u config=%u hidden=%u sync=%u",
      s->enable, s->config, s->hidden, s->sync);
  if (s->trace_activity_seq != 0) {
    vmsvga_trace_flight_histogram(s, "reset");
  };
  vmsvga_trace_flight_reset(s);
  vmsvga3d_reset(s);
  vmsvga_gmr_reset(s);
  vmsvga_screen_reset(s);
  s->index = 0;
  s->scratch_size = VMSVGA_SCRATCH_SIZE;
  s->fifo_size = VMSVGA_FIFO_SIZE;
  if (s->enable) {
    /* VGA will become the selected display, so enable its dirty log before
     * discarding any pending SVGA work. */
    vmsvga_set_dirty_log(s, true);
    vmsvga_fifo_discard_pending(s);
  };
  s->vga.vram_ptr = vmsvga_svga_vram_ptr(s);
  s->enable = 0;
  s->hidden = false;
  s->config = 0;
  s->svgaid = SVGA_ID_2;
  s->new_width = 0;
  s->new_height = 0;
  s->new_depth = 32;
  s->active_valid = false;
  s->active_width = 0;
  s->active_height = 0;
  s->active_depth = 0;
  s->active_stride = 0;
  s->disp_width = 0;
  s->disp_height = 0;
  s->num_gd = 1;
  s->display_id = 0;
  s->disp_prim = 0;
  s->disp_x = 0;
  s->disp_y = 0;
  s->devcap_val = 0;
  s->gmrdesc = 0;
  s->gmrid = 0;
  s->gmrpage = 0;
  s->cmd_low = 0;
  s->cmd_high = 0;
  s->pitchlock = 0;
  s->traces = 0;
  s->guest = 0;
  s->sync = 0;
  s->irq_mask = 0;
  s->irq_status = 0;
  s->cursor = 0;
  s->cursor_x = 0;
  s->cursor_y = 0;
  s->cursor_on = SVGA_CURSOR_ON_SHOW;
  s->active_cursor = 0;
  s->active_cursor_x = 0;
  s->active_cursor_y = 0;
  s->active_cursor_on = SVGA_CURSOR_ON_SHOW;
  s->cursor_dirty = true;
  s->marker_logged = false;
  s->damage_count = 0;
  s->fence = 0;
  s->fence_goal = 0;
  s->thread = 0;
  s->invalidated = false;
  s->svga_surface_bound = false;
  s->trace_display_path = VMSVGA_TRACE_DISPLAY_UNKNOWN;
  s->legacy_vga_size = 0;
  memset(s->svgapalettebase, 0, sizeof(s->svgapalettebase));
  memset(s->legacy_vga_ptr, 0, VMSVGA_VGA_FB_BACKUP_SIZE);
  vmsvga_fifo_upload_reset(s);
  vmsvga_cursor_cache_clear(s);
  vmsvga_cursor_source_clear(s);
  vmsvga_objects_clear(s);
#ifdef CONFIG_PIXMAN
  vmsvga_palette_rebuild(s);
#endif
  vmsvga_set_fifo_capabilities(s);
  memset(s->scratch, 0, sizeof(s->scratch));
  if (s->fifo != NULL) {
    vmsvga_publish_fifo_registers(s);
    vmsvga_fifo_set_busy(s, false);
  };
  vmsvga_update_dirty_log(s);
#ifndef RAISE_IRQ_OFF
  pci_set_irq(PCI_DEVICE(pci), 0);
#endif
};
static void vmsvga_invalidate_display(void *opaque) {
  VPRINT("vmsvga_invalidate_display was just executed\n");
  struct vmsvga_state_s *s = opaque;
  if (!s->enable) {
    s->vga.hw_ops->invalidate(&s->vga);
    return;
  };
  s->invalidated = true;
};
static void vmsvga_text_update(void *opaque, uint32_t *chardata) {
  VPRINT("vmsvga_text_update was just executed\n");
  struct vmsvga_state_s *s = opaque;
  uint8_t *vram_ptr = s->vga.vram_ptr;
  if (s->vga.hw_ops->text_update) {
    if (s->enable && s->legacy_vga_size != 0) {
      s->vga.vram_ptr = s->legacy_vga_ptr;
    };
    s->vga.hw_ops->text_update(&s->vga, chardata);
    s->vga.vram_ptr = vram_ptr;
  };
};
static void vmsvga_migration_buffers_clear(struct vmsvga_state_s *s) {
  uint32_t id;
  for (id = 0; id < VMSVGA_MAX_OBJECTS; id++) {
    g_clear_pointer(&s->object_migration[id].data, g_free);
  };
  for (id = 0; id < VMSVGA_MAX_CURSORS; id++) {
    g_clear_pointer(&s->cursor_migration[id].data, g_free);
    g_clear_pointer(&s->cursor_migration[id].and_data, g_free);
    g_clear_pointer(&s->cursor_migration[id].xor_data, g_free);
  };
  memset(s->object_migration, 0, sizeof(s->object_migration));
  memset(s->cursor_migration, 0, sizeof(s->cursor_migration));
};
static int vmsvga_pre_load(void *opaque) {
  struct vmsvga_state_s *s = opaque;
  vmsvga_set_dirty_log(s, true);
  s->vga.vram_ptr = vmsvga_svga_vram_ptr(s);
  s->svga_surface_bound = false;
  s->hidden = false;
  s->legacy_vga_size = 0;
  vmsvga_screen_base_clear(s);
  s->screen_base_migration_size = 0;
  vmsvga_trace_flight_reset(s);
  memset(s->legacy_vga_ptr, 0, VMSVGA_VGA_FB_BACKUP_SIZE);
  vmsvga_fifo_upload_reset(s);
  vmsvga_migration_buffers_clear(s);
  vmsvga_cursor_cache_clear(s);
  vmsvga_cursor_source_clear(s);
  vmsvga_objects_clear(s);
  memset(s->scratch, 0, sizeof(s->scratch));
  return 0;
};
static int vmsvga_pre_save(void *opaque) {
  struct vmsvga_state_s *s = opaque;
  uint32_t id;
  s->screen_base_migration_size = 0;
  if (s->enable || s->legacy_vga_size != 0) {
    s->legacy_vga_size = (uint32_t)vmsvga_legacy_vga_backup_size(s);
  };
  if (s->screen_defined) {
    uint64_t packed_stride = (uint64_t)s->screen_width * 4;
    uint64_t stride = s->screen_backing_valid
                          ? s->screen_backing_pitch
                          : packed_stride;
    uint64_t size = stride * s->screen_height;
    if (s->screen_width == 0 || s->screen_height == 0 ||
        s->screen_width > VMSVGA_MAX_WIDTH ||
        s->screen_height > VMSVGA_MAX_HEIGHT ||
        packed_stride > UINT32_MAX || stride > UINT32_MAX ||
        size == 0 || size > UINT32_MAX ||
        s->screen_stride != stride || s->active_stride != stride) {
      return -EINVAL;
    };
    if (s->screen_backing_valid) {
      if (!vmsvga_screen_backing_validate(
              s, s->screen_width, s->screen_height,
              s->screen_backing_gmr_id, s->screen_backing_offset,
              s->screen_backing_pitch)) {
        return -EINVAL;
      };
      s->screen_base_migration_size = 0;
    } else {
      if (s->screen_base == NULL || s->screen_base_size != size ||
          stride != packed_stride) {
        return -EINVAL;
      };
      s->screen_base_migration_size = (uint32_t)size;
    };
  } else if (s->screen_base != NULL || s->screen_base_size != 0 ||
             s->screen_stride != 0 || s->screen_backing_valid ||
             s->screen_backing_gmr_id != SVGA_GMR_NULL ||
             s->screen_backing_offset != 0 || s->screen_backing_pitch != 0 ||
             s->screen_clone_count != 0) {
    return -EINVAL;
  };
  if (!vmsvga_fifo_upload_valid(s)) {
    return -EINVAL;
  };
  for (id = 0; id < VMSVGA_MAX_OBJECTS; id++) {
    struct vmsvga_object_s *object = s->objects[id];
    if (object != NULL &&
        (object->data == NULL || object->size > UINT32_MAX)) {
      return -EINVAL;
    };
  };
  for (id = 0; id < VMSVGA_MAX_CURSORS; id++) {
    QEMUCursor *cursor = s->cursor_cache[id];
    if (cursor != NULL &&
        (cursor->width < 1 ||
         cursor->width > VMSVGA_CURSOR_MAX_DIMENSION ||
         cursor->height < 1 ||
         cursor->height > VMSVGA_CURSOR_MAX_DIMENSION ||
         cursor->hot_x < 0 || cursor->hot_x >= cursor->width ||
         cursor->hot_y < 0 || cursor->hot_y >= cursor->height)) {
      return -EINVAL;
    };
  };
  memset(s->object_migration, 0, sizeof(s->object_migration));
  memset(s->cursor_migration, 0, sizeof(s->cursor_migration));
  for (id = 0; id < VMSVGA_MAX_OBJECTS; id++) {
    struct vmsvga_object_s *object = s->objects[id];
    struct vmsvga_object_migration_s *migration =
        &s->object_migration[id];
    if (object == NULL) {
      continue;
    };
    migration->present = true;
    migration->type = object->type;
    migration->width = object->width;
    migration->height = object->height;
    migration->depth = object->depth;
    migration->stride = object->stride;
    migration->data_size = object->size;
    migration->data = object->data;
  };
  for (id = 0; id < VMSVGA_MAX_CURSORS; id++) {
    QEMUCursor *cursor = s->cursor_cache[id];
    struct vmsvga_cursor_source_s *source = s->cursor_source[id];
    struct vmsvga_cursor_migration_s *migration =
        &s->cursor_migration[id];
    if (cursor == NULL && source == NULL) {
      continue;
    };
    migration->present = true;
    if (cursor != NULL) {
      migration->width = cursor->width;
      migration->height = cursor->height;
      migration->hot_x = cursor->hot_x;
      migration->hot_y = cursor->hot_y;
      migration->pixel_count =
          (uint32_t)((uint64_t)cursor->width * cursor->height);
      migration->data = cursor->data;
    };
    if (source != NULL) {
      migration->raw = true;
      migration->alpha = source->alpha;
      migration->width = source->width;
      migration->height = source->height;
      migration->hot_x = source->hot_x;
      migration->hot_y = source->hot_y;
      migration->and_mask_bpp = source->and_mask_bpp;
      migration->xor_mask_bpp = source->xor_mask_bpp;
      migration->and_size = source->and_size;
      migration->xor_size = source->xor_size;
      migration->and_data = source->and_data;
      migration->xor_data = source->xor_data;
    };
  };
  return 0;
};
static void vmsvga_post_save(void *opaque) {
  struct vmsvga_state_s *s = opaque;
  s->screen_base_migration_size = 0;
  memset(s->object_migration, 0, sizeof(s->object_migration));
  memset(s->cursor_migration, 0, sizeof(s->cursor_migration));
};
static int vmsvga_restore_objects(struct vmsvga_state_s *s) {
  size_t total = 0;
  size_t limit = vmsvga_surface_memory_size(s);
  uint32_t id;
  for (id = 0; id < VMSVGA_MAX_OBJECTS; id++) {
    struct vmsvga_object_migration_s *migration =
        &s->object_migration[id];
    struct vmsvga_object_s *object;
    uint32_t stride;
    size_t size;
    if (!migration->present) {
      if (migration->data_size != 0 || migration->data != NULL) {
        return -EINVAL;
      };
      continue;
    };
    if (migration->data == NULL ||
        !vmsvga_object_layout(migration->type, migration->width,
                              migration->height, migration->depth,
                              &stride, &size) ||
        stride != migration->stride || size != migration->data_size ||
        size > limit || total > limit - size) {
      return -EINVAL;
    };
    object = g_try_new0(struct vmsvga_object_s, 1);
    if (object == NULL) {
      return -ENOMEM;
    };
    object->type = migration->type;
    object->width = migration->width;
    object->height = migration->height;
    object->depth = migration->depth;
    object->stride = migration->stride;
    object->size = size;
    object->data = migration->data;
    migration->data = NULL;
    s->objects[id] = object;
    total += size;
  };
  s->object_bytes = total;
  return 0;
};
static int vmsvga_restore_cursors(struct vmsvga_state_s *s) {
  uint32_t id;
  for (id = 0; id < VMSVGA_MAX_CURSORS; id++) {
    struct vmsvga_cursor_migration_s *migration =
        &s->cursor_migration[id];
    if (!migration->present) {
      if (migration->pixel_count != 0 || migration->data != NULL ||
          migration->and_size != 0 || migration->xor_size != 0 ||
          migration->and_data != NULL || migration->xor_data != NULL) {
        return -EINVAL;
      };
      continue;
    };
    if (migration->raw) {
      struct vmsvga_cursor_source_s *source;
      uint32_t expected_and;
      uint32_t expected_xor;
      if (migration->width < 1 ||
          migration->width > VMSVGA_CURSOR_MAX_DIMENSION ||
          migration->height < 1 ||
          migration->height > VMSVGA_CURSOR_MAX_DIMENSION ||
          migration->hot_x < 0 ||
          (uint32_t)migration->hot_x >= migration->width ||
          migration->hot_y < 0 ||
          (uint32_t)migration->hot_y >= migration->height) {
        return -EINVAL;
      };
      expected_and = migration->alpha
                         ? 0
                         : vmsvga_cursor_row_bytes(
                               migration->width, migration->and_mask_bpp) *
                               migration->height;
      expected_xor = vmsvga_cursor_row_bytes(
                         migration->width, migration->xor_mask_bpp) *
                     migration->height;
      if (migration->and_size != expected_and ||
          migration->xor_size != expected_xor ||
          (expected_and != 0 && migration->and_data == NULL) ||
          expected_xor == 0 || migration->xor_data == NULL) {
        return -EINVAL;
      };
      source = g_try_new0(struct vmsvga_cursor_source_s, 1);
      if (source == NULL) {
        return -ENOMEM;
      };
      source->alpha = migration->alpha;
      source->width = migration->width;
      source->height = migration->height;
      source->hot_x = migration->hot_x;
      source->hot_y = migration->hot_y;
      source->and_mask_bpp = migration->and_mask_bpp;
      source->xor_mask_bpp = migration->xor_mask_bpp;
      source->and_size = migration->and_size;
      source->xor_size = migration->xor_size;
      source->and_data = migration->and_data;
      source->xor_data = migration->xor_data;
      migration->and_data = NULL;
      migration->xor_data = NULL;
      s->cursor_source[id] = source;
      g_clear_pointer(&migration->data, g_free);
      if (!vmsvga_cursor_render_source(s, id)) {
        return -ENOMEM;
      };
      continue;
    } else {
      QEMUCursor *cursor;
      uint64_t data_size = (uint64_t)migration->width * migration->height *
                           sizeof(uint32_t);
      if (migration->width < 1 ||
          migration->width > VMSVGA_CURSOR_MAX_DIMENSION ||
          migration->height < 1 ||
          migration->height > VMSVGA_CURSOR_MAX_DIMENSION ||
          migration->hot_x < 0 ||
          (uint32_t)migration->hot_x >= migration->width ||
          migration->hot_y < 0 ||
          (uint32_t)migration->hot_y >= migration->height ||
          data_size / sizeof(uint32_t) != migration->pixel_count ||
          migration->data == NULL) {
        return -EINVAL;
      };
      cursor = cursor_alloc(migration->width, migration->height);
      if (cursor == NULL) {
        return -ENOMEM;
      };
      cursor->hot_x = migration->hot_x;
      cursor->hot_y = migration->hot_y;
      memcpy(cursor->data, migration->data, data_size);
      g_clear_pointer(&migration->data, g_free);
      vmsvga_cursor_cache_put(s, id, cursor);
    };
  };
  return 0;
};
static int vmsvga_post_load(void *opaque, int version_id) {
  VPRINT("vmsvga_post_load was just executed\n");
  struct vmsvga_state_s *s = opaque;
  size_t shadow_size;
  int ret;
  (void)version_id;
  s->scratch_size = VMSVGA_SCRATCH_SIZE;
  s->fifo_size = VMSVGA_FIFO_SIZE;
  s->fifo = (uint32_t *)memory_region_get_ram_ptr(&s->fifo_ram);
  s->thread = 0;
  s->traces = !!s->traces;
  if (!s->enable) {
    s->hidden = false;
  };

  if (s->active_valid &&
      !vmsvga_mode_valid(s, s->active_width, s->active_height,
                         s->active_depth, s->active_stride)) {
    ret = -EINVAL;
    goto fail;
  };
  shadow_size = vmsvga_legacy_vga_backup_size(s);
  if (s->legacy_vga_size > shadow_size ||
      (s->enable && s->legacy_vga_size != shadow_size)) {
    ret = -EINVAL;
    goto fail;
  };

  if (s->active_cursor_on > SVGA_CURSOR_ON_RESTORE_TO_FB) {
    ret = -EINVAL;
    goto fail;
  };

  if (s->screen_defined) {
    uint64_t packed_stride = (uint64_t)s->screen_width * 4;
    uint64_t stride = s->screen_backing_valid
                          ? s->screen_backing_pitch
                          : packed_stride;
    uint64_t size = stride * s->screen_height;
    bool backing_valid =
        !s->screen_backing_valid ||
        vmsvga_screen_backing_validate(
            s, s->screen_width, s->screen_height,
            s->screen_backing_gmr_id, s->screen_backing_offset,
            s->screen_backing_pitch);
    bool storage_valid = s->screen_backing_valid
                             ? s->screen_base_migration_size == 0
                             : (stride == packed_stride &&
                                s->screen_base_migration_size == size &&
                                s->screen_base != NULL);
    if (s->screen_width == 0 || s->screen_height == 0 ||
        s->screen_width > VMSVGA_MAX_WIDTH ||
        s->screen_height > VMSVGA_MAX_HEIGHT ||
        packed_stride > UINT32_MAX || stride > UINT32_MAX ||
        size == 0 || size > UINT32_MAX || !backing_valid || !storage_valid ||
        s->active_stride != stride) {
      ret = -EINVAL;
      goto fail;
    };
    s->screen_stride = (uint32_t)stride;
    if (s->screen_backing_valid) {
      g_clear_pointer(&s->screen_base, g_free);
      s->screen_base_size = 0;
    } else {
      s->screen_base_size = (size_t)size;
    };
  } else if (s->screen_base_migration_size != 0 || s->screen_base != NULL ||
             s->screen_backing_valid ||
             s->screen_backing_gmr_id != SVGA_GMR_NULL ||
             s->screen_backing_offset != 0 || s->screen_backing_pitch != 0 ||
             s->screen_clone_count != 0) {
    ret = -EINVAL;
    goto fail;
  };
  s->screen_base_migration_size = 0;

  if (s->enable) {
    vmsvga_legacy_vga_enter(s);
  } else if (s->legacy_vga_size != 0) {
    vmsvga_legacy_vga_leave(s);
  } else {
    s->vga.vram_ptr = vmsvga_svga_vram_ptr(s);
  };
  s->cursor_dirty = true;
  s->damage_count = 0;
  s->invalidated = s->enable && s->active_valid;
  s->svga_surface_bound = false;
  s->trace_display_path = VMSVGA_TRACE_DISPLAY_UNKNOWN;
  ret = vmsvga_restore_objects(s);
  if (ret < 0) {
    goto fail;
  };
  if (!vmsvga_fifo_upload_valid(s)) {
    ret = -EINVAL;
    goto fail;
  };
  ret = vmsvga_restore_cursors(s);
  if (ret < 0) {
    goto fail;
  };
  cursor_update_from_fifo(s);
  vmsvga_migration_buffers_clear(s);
#ifdef CONFIG_PIXMAN
  vmsvga_palette_rebuild(s);
#endif
  vmsvga_set_fifo_capabilities(s);
  vmsvga_publish_fifo_registers(s);
  if (!s->enable || !s->config || !vmsvga_fifo_pending(s)) {
    s->sync = 0;
    vmsvga_fifo_set_busy(s, false);
  } else if (s->sync) {
    vmsvga_fifo_set_busy(s, true);
  };
  vmsvga_update_dirty_log(s);
#ifndef RAISE_IRQ_OFF
  struct pci_vmsvga_state_s *pci_vmsvga =
      container_of(s, struct pci_vmsvga_state_s, chip);
  pci_set_irq(PCI_DEVICE(pci_vmsvga), !!(s->irq_status & s->irq_mask));
#endif
  return 0;
fail:
  vmsvga_fifo_upload_reset(s);
  vmsvga_cursor_cache_clear(s);
  vmsvga_cursor_source_clear(s);
  vmsvga_objects_clear(s);
  vmsvga_screen_base_clear(s);
  s->screen_base_migration_size = 0;
  vmsvga_migration_buffers_clear(s);
  return ret;
};
static const VMStateDescription vmstate_vmsvga_fifo_upload = {
    .name = "vmware_vga_fifo_upload",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (const VMStateField[]){
            VMSTATE_BOOL(active, struct vmsvga_fifo_upload_s),
            VMSTATE_BOOL(discard, struct vmsvga_fifo_upload_s),
            VMSTATE_UINT32(type, struct vmsvga_fifo_upload_s),
            VMSTATE_UINT32(id, struct vmsvga_fifo_upload_s),
            VMSTATE_UINT32(total_words, struct vmsvga_fifo_upload_s),
            VMSTATE_UINT32(received_words, struct vmsvga_fifo_upload_s),
            VMSTATE_END_OF_LIST()}};
static const VMStateDescription vmstate_vmsvga_object_migration = {
    .name = "vmware_vga_object",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (const VMStateField[]){
            VMSTATE_BOOL(present, struct vmsvga_object_migration_s),
            VMSTATE_UINT32(type, struct vmsvga_object_migration_s),
            VMSTATE_UINT32(width, struct vmsvga_object_migration_s),
            VMSTATE_UINT32(height, struct vmsvga_object_migration_s),
            VMSTATE_UINT32(depth, struct vmsvga_object_migration_s),
            VMSTATE_UINT32(stride, struct vmsvga_object_migration_s),
            VMSTATE_UINT32(data_size, struct vmsvga_object_migration_s),
            VMSTATE_VBUFFER_ALLOC_UINT32(data,
                                         struct vmsvga_object_migration_s, 0,
                                         NULL, data_size),
            VMSTATE_END_OF_LIST()}};
static const VMStateDescription vmstate_vmsvga_cursor_migration = {
    .name = "vmware_vga_cursor",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (const VMStateField[]){
            VMSTATE_BOOL(present, struct vmsvga_cursor_migration_s),
            VMSTATE_UINT32(width, struct vmsvga_cursor_migration_s),
            VMSTATE_UINT32(height, struct vmsvga_cursor_migration_s),
            VMSTATE_INT32(hot_x, struct vmsvga_cursor_migration_s),
            VMSTATE_INT32(hot_y, struct vmsvga_cursor_migration_s),
            VMSTATE_UINT32(pixel_count, struct vmsvga_cursor_migration_s),
            VMSTATE_VARRAY_UINT32_ALLOC(
                data, struct vmsvga_cursor_migration_s, pixel_count, 0,
                vmstate_info_uint32, uint32_t),
            VMSTATE_BOOL(raw, struct vmsvga_cursor_migration_s),
            VMSTATE_BOOL(alpha, struct vmsvga_cursor_migration_s),
            VMSTATE_UINT32(and_mask_bpp, struct vmsvga_cursor_migration_s),
            VMSTATE_UINT32(xor_mask_bpp, struct vmsvga_cursor_migration_s),
            VMSTATE_UINT32(and_size, struct vmsvga_cursor_migration_s),
            VMSTATE_UINT32(xor_size, struct vmsvga_cursor_migration_s),
            VMSTATE_VBUFFER_ALLOC_UINT32(
                and_data, struct vmsvga_cursor_migration_s, 0, NULL, and_size),
            VMSTATE_VBUFFER_ALLOC_UINT32(
                xor_data, struct vmsvga_cursor_migration_s, 0, NULL, xor_size),
            VMSTATE_END_OF_LIST()}};
static bool vmsvga_legacy_vga_size_valid(void *opaque, int version_id) {
  struct vmsvga_state_s *s = opaque;
  size_t expected;
  (void)version_id;
  expected = vmsvga_legacy_vga_backup_size(s);
  return s->legacy_vga_size <= expected &&
         (!s->enable || s->legacy_vga_size == expected);
};

static VMStateDescription vmstate_vmware_vga_internal = {
    .name = "vmware_vga_internal",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_load = vmsvga_pre_load,
    .post_load = vmsvga_post_load,
    .pre_save = vmsvga_pre_save,
    .post_save = vmsvga_post_save,
    .fields = (const VMStateField[]){
        VMSTATE_UINT32_ARRAY(svgapalettebase, struct vmsvga_state_s,
                             VMSVGA_PALETTE_STORAGE_SIZE),
        VMSTATE_UINT32(enable, struct vmsvga_state_s),
        VMSTATE_BOOL(hidden, struct vmsvga_state_s),
        VMSTATE_UINT32(config, struct vmsvga_state_s),
        VMSTATE_UINT32(index, struct vmsvga_state_s),
        VMSTATE_UINT32(scratch_size, struct vmsvga_state_s),
        VMSTATE_UINT32_ARRAY(scratch, struct vmsvga_state_s,
                             VMSVGA_SCRATCH_SIZE),
        VMSTATE_UINT32(new_width, struct vmsvga_state_s),
        VMSTATE_UINT32(new_height, struct vmsvga_state_s),
        VMSTATE_UINT32(new_depth, struct vmsvga_state_s),
        VMSTATE_UINT32(num_gd, struct vmsvga_state_s),
        VMSTATE_UINT32(disp_prim, struct vmsvga_state_s),
        VMSTATE_UINT32(disp_x, struct vmsvga_state_s),
        VMSTATE_UINT32(disp_y, struct vmsvga_state_s),
        VMSTATE_UINT32(disp_width, struct vmsvga_state_s),
        VMSTATE_UINT32(disp_height, struct vmsvga_state_s),
        VMSTATE_UINT32(devcap_val, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrdesc, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrid, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrpage, struct vmsvga_state_s),
        VMSTATE_UINT32(traces, struct vmsvga_state_s),
        VMSTATE_UINT32(cmd_low, struct vmsvga_state_s),
        VMSTATE_UINT32(cmd_high, struct vmsvga_state_s),
        VMSTATE_UINT32(guest, struct vmsvga_state_s),
        VMSTATE_UINT32(svgaid, struct vmsvga_state_s),
        VMSTATE_UINT32(thread, struct vmsvga_state_s),
        VMSTATE_UINT32(sync, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_size, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_min, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_max, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_next, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_stop, struct vmsvga_state_s),
        VMSTATE_UINT32(irq_mask, struct vmsvga_state_s),
        VMSTATE_UINT32(irq_status, struct vmsvga_state_s),
        VMSTATE_UINT32(display_id, struct vmsvga_state_s),
        VMSTATE_UINT32(pitchlock, struct vmsvga_state_s),
        VMSTATE_UINT32(cursor, struct vmsvga_state_s),
        VMSTATE_UINT32(cursor_x, struct vmsvga_state_s),
        VMSTATE_UINT32(cursor_y, struct vmsvga_state_s),
        VMSTATE_UINT32(cursor_on, struct vmsvga_state_s),
        VMSTATE_UINT32(active_cursor, struct vmsvga_state_s),
        VMSTATE_UINT32(active_cursor_x, struct vmsvga_state_s),
        VMSTATE_UINT32(active_cursor_y, struct vmsvga_state_s),
        VMSTATE_UINT32(active_cursor_on, struct vmsvga_state_s),
        VMSTATE_UINT32(fence, struct vmsvga_state_s),
        VMSTATE_UINT32(fence_goal, struct vmsvga_state_s),
        VMSTATE_UINT32(fc, struct vmsvga_state_s),
        VMSTATE_UINT32(ff, struct vmsvga_state_s),
        VMSTATE_BOOL(active_valid, struct vmsvga_state_s),
        VMSTATE_UINT32(active_width, struct vmsvga_state_s),
        VMSTATE_UINT32(active_height, struct vmsvga_state_s),
        VMSTATE_UINT32(active_depth, struct vmsvga_state_s),
        VMSTATE_UINT32(active_stride, struct vmsvga_state_s),
        VMSTATE_UINT32(legacy_vga_size, struct vmsvga_state_s),
        VMSTATE_VALIDATE("legacy VGA shadow size",
                         vmsvga_legacy_vga_size_valid),
        VMSTATE_VBUFFER_UINT32(legacy_vga_ptr, struct vmsvga_state_s, 0, NULL,
                               legacy_vga_size),
        VMSTATE_STRUCT(fifo_upload, struct vmsvga_state_s, 0,
                       vmstate_vmsvga_fifo_upload,
                       struct vmsvga_fifo_upload_s),
        VMSTATE_STRUCT_ARRAY(object_migration, struct vmsvga_state_s,
                             VMSVGA_MAX_OBJECTS, 0,
                             vmstate_vmsvga_object_migration,
                             struct vmsvga_object_migration_s),
        VMSTATE_STRUCT_ARRAY(cursor_migration, struct vmsvga_state_s,
                             VMSVGA_MAX_CURSORS, 0,
                             vmstate_vmsvga_cursor_migration,
                             struct vmsvga_cursor_migration_s),
        VMSTATE_BOOL(screen_defined, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_flags, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_width, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_height, struct vmsvga_state_s),
        VMSTATE_INT32(screen_root_x, struct vmsvga_state_s),
        VMSTATE_INT32(screen_root_y, struct vmsvga_state_s),
        VMSTATE_BOOL(screen_backing_valid, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_backing_gmr_id, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_backing_offset, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_backing_pitch, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_clone_count, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_base_migration_size, struct vmsvga_state_s),
        VMSTATE_VBUFFER_ALLOC_UINT32(screen_base, struct vmsvga_state_s, 0,
                                     NULL, screen_base_migration_size),
        VMSTATE_BOOL(gmrfb_defined, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrfb_gmr_id, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrfb_offset, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrfb_bytes_per_line, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrfb_format, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_annotation_type, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_annotation_color, struct vmsvga_state_s),
        VMSTATE_INT32(screen_annotation_src_x, struct vmsvga_state_s),
        VMSTATE_INT32(screen_annotation_src_y, struct vmsvga_state_s),
        VMSTATE_UINT32(screen_annotation_src_id, struct vmsvga_state_s),
        VMSTATE_END_OF_LIST()}};
static VMStateDescription vmstate_vmware_vga = {
    .name = "vmware_vga",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]){
        VMSTATE_PCI_DEVICE(parent_obj, struct pci_vmsvga_state_s),
        VMSTATE_STRUCT(chip, struct pci_vmsvga_state_s, 0,
                       vmstate_vmware_vga_internal, struct vmsvga_state_s),
        VMSTATE_END_OF_LIST()}};
static GraphicHwOps vmsvga_ops = {
    .invalidate = vmsvga_invalidate_display,
    .gfx_update = vmsvga_update_display,
    .text_update = vmsvga_text_update,
};
static void vmsvga_init(DeviceState *dev, struct vmsvga_state_s *s,
                        MemoryRegion *address_space, MemoryRegion *io) {
  VPRINT("vmsvga_init was just executed\n");
  s->scratch_size = VMSVGA_SCRATCH_SIZE;
  memset(s->scratch, 0, sizeof(s->scratch));
  memset(s->cursor_cache, 0, sizeof(s->cursor_cache));
  memset(s->cursor_source, 0, sizeof(s->cursor_source));
  s->vga.con = vmvga_graphic_console_create(dev, 0, &vmsvga_ops, s);
  s->fifo_size = VMSVGA_FIFO_SIZE;
  s->hidden = false;
  s->trace_display_path = VMSVGA_TRACE_DISPLAY_UNKNOWN;
  vmsvga_trace_flight_reset(s);
  memory_region_init_ram(&s->fifo_ram, OBJECT(dev), "vmsvga.fifo",
                         s->fifo_size, &error_fatal);
  s->fifo = (uint32_t *)memory_region_get_ram_ptr(&s->fifo_ram);
  vga_common_init(&s->vga, OBJECT(dev), &error_fatal);
  vga_init(&s->vga, OBJECT(dev), address_space, io, true);
  /* vga_common_init() enables DIRTY_MEMORY_VGA logging. */
  s->dirty_log_enabled = true;
  s->legacy_vga_ptr = g_malloc0(VMSVGA_VGA_FB_BACKUP_SIZE);
  s->legacy_vga_size = 0;
  memory_region_init_io(&s->legacy_vga_mem, OBJECT(dev),
                        &vmsvga_legacy_vga_ops, s, "vmsvga.vga-lowmem",
                        0x20000);
  memory_region_set_flush_coalesced(&s->legacy_vga_mem);
  memory_region_add_subregion_overlap(address_space, 0x000a0000,
                                      &s->legacy_vga_mem, 2);
  /*
   * vga_update_memory_access() otherwise installs a chain-4 alias directly
   * onto s->vga.vram and bypasses the VMSVGA-specific VGA shadow above.
   */
  s->vga.legacy_address_space = NULL;
  VMVGA_REGISTER_VGA_VMSTATE(&s->vga);
  s->thread = 0;
  s->svgaid = SVGA_ID_2;
  s->new_width = 0;
  s->new_height = 0;
  s->new_depth = 32;
  s->active_valid = false;
  s->active_width = 0;
  s->active_height = 0;
  s->active_depth = 0;
  s->active_stride = 0;
  s->disp_width = 0;
  s->disp_height = 0;
  s->num_gd = 1;
  s->display_id = 0;
  s->disp_prim = 0;
  s->disp_x = 0;
  s->disp_y = 0;
  s->pitchlock = 0;
  s->traces = 0;
  s->guest = 0;
  s->sync = 0;
  s->cursor = 0;
  s->cursor_x = 0;
  s->cursor_y = 0;
  s->cursor_on = SVGA_CURSOR_ON_SHOW;
  s->active_cursor = 0;
  s->active_cursor_x = 0;
  s->active_cursor_y = 0;
  s->active_cursor_on = SVGA_CURSOR_ON_SHOW;
  s->cursor_dirty = true;
  s->marker_logged = false;
  s->damage_count = 0;
  s->fence = 0;
  s->fence_goal = 0;
  s->invalidated = false;
  s->svga_surface_bound = false;
  vmsvga_screen_reset(s);
  memset(s->svgapalettebase, 0, sizeof(s->svgapalettebase));
  vmsvga_fifo_upload_reset(s);
#ifdef CONFIG_PIXMAN
  vmsvga_palette_rebuild(s);
#endif
  vmsvga_set_fifo_capabilities(s);
};
static uint64_t vmsvga_io_read(void *opaque, hwaddr addr, unsigned size) {
  VPRINT("vmsvga_io_read was just executed\n");
  struct vmsvga_state_s *s = opaque;
  switch (addr) {
  case SVGA_INDEX_PORT:
    VPRINT("vmsvga_io_read SVGA_INDEX_PORT\n");
    return vmsvga_index_read(s, addr);
  case SVGA_VALUE_PORT:
    VPRINT("vmsvga_io_read SVGA_VALUE_PORT\n");
    return vmsvga_value_read(s, addr);
  case SVGA_BIOS_PORT:
    VPRINT("vmsvga_io_read SVGA_BIOS_PORT\n");
    return vmsvga_bios_read(s, addr);
  case SVGA_IRQSTATUS_PORT:
    VPRINT("vmsvga_io_read SVGA_IRQSTATUS_PORT\n");
    return vmsvga_irqstatus_read(s, addr);
  default:
    VPRINT("vmsvga_io_read default\n");
    return UINT32_MAX;
  };
};
static void vmsvga_io_write(void *opaque, hwaddr addr, uint64_t data,
                            unsigned size) {
  VPRINT("vmsvga_io_write was just executed\n");
  struct vmsvga_state_s *s = opaque;
  switch (addr) {
  case SVGA_INDEX_PORT:
    VPRINT("vmsvga_io_write SVGA_INDEX_PORT\n");
    vmsvga_index_write(s, addr, data);
    break;
  case SVGA_VALUE_PORT:
    VPRINT("vmsvga_io_write SVGA_VALUE_PORT\n");
    vmsvga_value_write(s, addr, data);
    break;
  case SVGA_BIOS_PORT:
    VPRINT("vmsvga_io_write SVGA_BIOS_PORT\n");
    vmsvga_bios_write(s, addr, data);
    break;
  case SVGA_IRQSTATUS_PORT:
    VPRINT("vmsvga_io_write SVGA_IRQSTATUS_PORT\n");
    vmsvga_irqstatus_write(s, addr, data);
    break;
  default:
    VPRINT("vmsvga_io_write default\n");
    break;
  };
};
static MemoryRegionOps vmsvga_io_ops = {
    .read = vmsvga_io_read,
    .write = vmsvga_io_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid =
        {
            .min_access_size = 4,
            .max_access_size = 4,
            .unaligned = true,
        },
    .impl =
        {
            .unaligned = true,
        },
};
static void pci_vmsvga_realize(PCIDevice *dev, Error **errp) {
  VPRINT("pci_vmsvga_realize was just executed\n");
  struct pci_vmsvga_state_s *s = VMWARE_SVGA(dev);
  dev->config[PCI_INTERRUPT_PIN] = 1;
  dev->config[PCI_INTERRUPT_LINE] = 0xff;
  dev->config[PCI_LATENCY_TIMER] = 64;
  dev->config[PCI_CACHE_LINE_SIZE] = 0x08;
  pci_set_word(dev->config + PCI_STATUS,
               PCI_STATUS_DEVSEL_MEDIUM | PCI_STATUS_FAST_BACK);
  if (pci_add_capability(dev, PCI_CAP_ID_VNDR, 0x40, 4, errp) < 0) {
    return;
  };
  memory_region_init_io(&s->io_bar, OBJECT(dev), &vmsvga_io_ops, &s->chip,
                        "vmsvga-io", 0x10);
  memory_region_set_flush_coalesced(&s->io_bar);
  pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &s->io_bar);
  vmsvga_init(DEVICE(dev), &s->chip, pci_address_space(dev),
              pci_address_space_io(dev));
  vmsvga3d_renderer_realize(&s->chip);
  pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_PREFETCH, &s->chip.vga.vram);
  pci_register_bar(dev, 2, PCI_BASE_ADDRESS_MEM_TYPE_32, &s->chip.fifo_ram);
};
static void pci_vmsvga_uninit(PCIDevice *dev) {
  struct pci_vmsvga_state_s *s = VMWARE_SVGA(dev);
  vmsvga_trace_flight_histogram(&s->chip, "uninit");
  vmsvga3d_reset(&s->chip);
  vmsvga3d_renderer_unrealize(&s->chip);
  vmsvga_screen_reset(&s->chip);
  vmsvga_migration_buffers_clear(&s->chip);
  vmsvga_cursor_cache_clear(&s->chip);
  vmsvga_cursor_source_clear(&s->chip);
  vmsvga_objects_clear(&s->chip);
  g_clear_pointer(&s->chip.legacy_vga_ptr, g_free);
};
static VMVGA_PROPERTY_QUALIFIER Property vga_vmware_properties[] = {
    DEFINE_PROP_UINT32("vgamem_mb", struct pci_vmsvga_state_s,
                       chip.vga.vram_size_mb, 128),
    VMVGA_GLOBAL_VMSTATE_PROPERTY(struct pci_vmsvga_state_s,
                                  chip.vga.global_vmstate)
    DEFINE_PROP_BOOL("x-test-marker", struct pci_vmsvga_state_s,
                     chip.test_marker, false),
    VMVGA_PROPERTY_END
};
static void vmsvga_class_init(ObjectClass *klass, VMVGA_CLASS_INIT_DATA data) {
  VPRINT("vmsvga_class_init was just executed\n");
  DeviceClass *dc = DEVICE_CLASS(klass);
  PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
  k->realize = pci_vmsvga_realize;
  k->exit = pci_vmsvga_uninit;
  k->romfile = "vgabios-vmware.bin";
  k->vendor_id = PCI_VENDOR_ID_VMWARE;
  k->device_id = PCI_DEVICE_ID_VMWARE_SVGA2;
  k->class_id = PCI_CLASS_DISPLAY_VGA;
  k->subsystem_vendor_id = PCI_VENDOR_ID_VMWARE;
  k->subsystem_id = PCI_DEVICE_ID_VMWARE_SVGA2;
  k->revision = 0x00;
  VMVGA_SET_LEGACY_RESET(dc, vmsvga_reset);
  dc->vmsd = &vmstate_vmware_vga;
  device_class_set_props(dc, vga_vmware_properties);
  dc->hotpluggable = false;
  set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
};
static TypeInfo vmsvga_info = {
    .name = "vmware-svga",
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(struct pci_vmsvga_state_s),
    .class_init = vmsvga_class_init,
    .interfaces =
        (InterfaceInfo[]){
            {INTERFACE_CONVENTIONAL_PCI_DEVICE},
            {},
        },
};
static void vmsvga_register_types(void) {
  VPRINT("vmsvga_register_types was just executed\n");
  type_register_static(&vmsvga_info);
};
type_init(vmsvga_register_types)
