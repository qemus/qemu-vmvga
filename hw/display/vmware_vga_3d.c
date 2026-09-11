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

#pragma pack(push, 1)
#include "include/svga3d_caps.h"
#include "include/svga3d_cmd.h"
#include "include/svga3d_devcaps.h"
#include "include/svga3d_dx.h"
#include "include/svga3d_limits.h"
#include "include/svga3d_reg.h"
#include "include/svga3d_shaderdefs.h"
#include "include/svga3d_surfacedefs.h"
#include "include/svga3d_types.h"
#pragma pack(pop)
#include "include/vmware_vga_3d_state.h"
#include "include/vmware_vga_vgpu9.h"
#include "include/vmware_vga_vgpu10.h"
#include "include/vmware_vga_dxvk.h"
#include "hw/pci/pci_device.h"
#include "qemu/timer.h"
#include "system/address-spaces.h"

_Static_assert(sizeof(SVGA3dCmdDefineGBSurface) == 36,
               "SVGA3dCmdDefineGBSurface wire size");
_Static_assert(sizeof(SVGA3dCmdDefineGBSurface_v2) == 44,
               "SVGA3dCmdDefineGBSurface_v2 wire size");
_Static_assert(sizeof(SVGA3dCmdDefineGBSurface_v3) == 52,
               "SVGA3dCmdDefineGBSurface_v3 wire size");
_Static_assert(sizeof(SVGA3dCmdDefineGBSurface_v4) == 56,
               "SVGA3dCmdDefineGBSurface_v4 wire size");
_Static_assert(sizeof(SVGA3dCmdSetOTableBase64) == 24,
               "SVGA3dCmdSetOTableBase64 wire size");
_Static_assert(sizeof(SVGA3dCmdGrowOTable) == 24,
               "SVGA3dCmdGrowOTable wire size");
_Static_assert(sizeof(SVGA3dCmdDefineGBMob64) == 20,
               "SVGA3dCmdDefineGBMob64 wire size");
_Static_assert(sizeof(SVGA3dCmdRedefineGBMob64) == 20,
               "SVGA3dCmdRedefineGBMob64 wire size");
_Static_assert(sizeof(SVGA3dCmdBeginGBQuery) == 8,
               "SVGA3dCmdBeginGBQuery wire size");
_Static_assert(sizeof(SVGA3dCmdEndGBQuery) == 16,
               "SVGA3dCmdEndGBQuery wire size");
_Static_assert(sizeof(SVGA3dCmdWaitForGBQuery) == 16,
               "SVGA3dCmdWaitForGBQuery wire size");
_Static_assert(sizeof(SVGA3dQueryResult) == 12,
               "SVGA3dQueryResult wire size");
_Static_assert(sizeof(SVGA3dCmdDXPresentBlt) == 68,
               "SVGA3dCmdDXPresentBlt wire size");

typedef struct {
    SVGA3dSize size;
} SVGA3dCmdSize;

static void vmsvga3d_surface_destroy_view_live(
    void *opaque, VMSVGA3DDxvkViewKind kind, uint32_t cid, uint32_t view_id);
static void vmsvga3d_d3d9_gb_query_timer_cb(void *opaque);

/* D3D9 has 8 fixed-function stages but 21 addressable sampler slots. */
#define VMSVGA3D_MAX_TEXTURE_STAGES VMSVGA3D_D3D9_MAX_TEXTURE_STAGES
#define VMSVGA3D_MAX_SAMPLERS VMSVGA3D_D3D9_MAX_SAMPLERS
#define VMSVGA3D_GB_QUERY_POLL_INTERVAL_MS 1

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
    SVGA3dVertexElement vertex_decls[SVGA3D_MAX_VERTEX_ARRAYS];
    SVGA3dVertexStream vertex_streams[SVGA3D_MAX_VERTEX_ARRAYS];
    SVGA3dVertexDivisor vertex_divisors[SVGA3D_MAX_VERTEX_ARRAYS];
    uint32_t num_vertex_decls;
    uint32_t num_vertex_streams;
    uint32_t num_vertex_divisors;
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
/* The VMware Win7 KMD sizes the GART backing MOB at one 64-bit entry per
 * 4 KiB aperture page. */
#define VMSVGA3D_GART_ENTRY_SIZE 8u

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

typedef struct vmsvga3d_gb_query_s {
    uint64_t token;
    uint32_t cid;
    SVGA3dQueryType type;
    SVGAMobId mobid;
    uint32_t offset;
    uint32_t query_cookie;
    VMSVGA3DMob *mob;
    struct vmsvga3d_gb_query_s *next;
} VMSVGA3DGBQuery;

typedef struct vmsvga3d_gart_page_s {
    uint64_t gpa;
    SVGAMobId mobid;
    uint32_t mob_page;
    MemoryRegion alias;
    bool alias_active;
} VMSVGA3DGARTPage;

typedef struct vmsvga3d_dx_cotable_s {
    SVGAMobId mobid;
    uint32_t valid_size;
    uint32_t entry_size;
    uint32_t capacity_entries;
    uint32_t valid_entries;
    uint8_t *host;
    uint32_t host_size;
} VMSVGA3DDXCOTable;

#define VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET      UINT64_C(0x00000001)
#define VMSVGA3D_DX_CTX_F_STATE_CSTARGET          UINT64_C(0x00000002)
#define VMSVGA3D_DX_CTX_F_STATE_INPUTLAYOUT       UINT64_C(0x00000010)
#define VMSVGA3D_DX_CTX_F_STATE_TOPOLOGY          UINT64_C(0x00000020)
#define VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE        UINT64_C(0x00000080)
#define VMSVGA3D_DX_CTX_F_STATE_DEPTHSTENCILSTATE UINT64_C(0x00000100)
#define VMSVGA3D_DX_CTX_F_STATE_VIEWPORT          UINT64_C(0x00000400)
#define VMSVGA3D_DX_CTX_F_STATE_SCISSORRECT       UINT64_C(0x00000800)
#define VMSVGA3D_DX_CTX_F_STATE_RASTERIZERSTATE   UINT64_C(0x00001000)
#define VMSVGA3D_DX_CTX_F_STATE_INDEXBUFFER       UINT64_C(0x00002000)
#define VMSVGA3D_DX_CTX_F_STATE_VERTEXBUFFER      UINT64_C(0x00004000)
#define VMSVGA3D_DX_CTX_F_STATE_SAMPLER_VS        UINT64_C(0x00010000)
#define VMSVGA3D_DX_CTX_F_STATE_SAMPLER_PS        UINT64_C(0x00020000)
#define VMSVGA3D_DX_CTX_F_STATE_SAMPLER_GS        UINT64_C(0x00040000)
#define VMSVGA3D_DX_CTX_F_STATE_SAMPLER_HS        UINT64_C(0x00080000)
#define VMSVGA3D_DX_CTX_F_STATE_SAMPLER_DS        UINT64_C(0x00100000)
#define VMSVGA3D_DX_CTX_F_STATE_SAMPLER_CS        UINT64_C(0x00200000)
#define VMSVGA3D_DX_CTX_F_STATE_SRV_VS            UINT64_C(0x01000000)
#define VMSVGA3D_DX_CTX_F_STATE_SRV_PS            UINT64_C(0x02000000)
#define VMSVGA3D_DX_CTX_F_STATE_SRV_GS            UINT64_C(0x04000000)
#define VMSVGA3D_DX_CTX_F_STATE_SRV_HS            UINT64_C(0x08000000)
#define VMSVGA3D_DX_CTX_F_STATE_SRV_DS            UINT64_C(0x10000000)
#define VMSVGA3D_DX_CTX_F_STATE_SRV_CS            UINT64_C(0x20000000)

#define VMSVGA3D_DX_CTX_F_STATE_SAMPLERS \
  (VMSVGA3D_DX_CTX_F_STATE_SAMPLER_VS | \
   VMSVGA3D_DX_CTX_F_STATE_SAMPLER_PS | \
   VMSVGA3D_DX_CTX_F_STATE_SAMPLER_GS | \
   VMSVGA3D_DX_CTX_F_STATE_SAMPLER_HS | \
   VMSVGA3D_DX_CTX_F_STATE_SAMPLER_DS | \
   VMSVGA3D_DX_CTX_F_STATE_SAMPLER_CS)
#define VMSVGA3D_DX_CTX_F_STATE_SRVS \
  (VMSVGA3D_DX_CTX_F_STATE_SRV_VS | \
   VMSVGA3D_DX_CTX_F_STATE_SRV_PS | \
   VMSVGA3D_DX_CTX_F_STATE_SRV_GS | \
   VMSVGA3D_DX_CTX_F_STATE_SRV_HS | \
   VMSVGA3D_DX_CTX_F_STATE_SRV_DS | \
   VMSVGA3D_DX_CTX_F_STATE_SRV_CS)
#define VMSVGA3D_DX_CTX_F_STATE_ALL \
  (VMSVGA3D_DX_CTX_F_STATE_INPUTLAYOUT | \
   VMSVGA3D_DX_CTX_F_STATE_TOPOLOGY | \
   VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET | \
   VMSVGA3D_DX_CTX_F_STATE_CSTARGET | \
   VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE | \
   VMSVGA3D_DX_CTX_F_STATE_DEPTHSTENCILSTATE | \
   VMSVGA3D_DX_CTX_F_STATE_VIEWPORT | \
   VMSVGA3D_DX_CTX_F_STATE_SCISSORRECT | \
   VMSVGA3D_DX_CTX_F_STATE_RASTERIZERSTATE | \
   VMSVGA3D_DX_CTX_F_STATE_INDEXBUFFER | \
   VMSVGA3D_DX_CTX_F_STATE_VERTEXBUFFER | \
   VMSVGA3D_DX_CTX_F_STATE_SAMPLERS | \
   VMSVGA3D_DX_CTX_F_STATE_SRVS)

typedef struct vmsvga3d_dx_context_s {
    uint32_t cid;
    uint32_t render_target_count;
    uint32_t stream_output_target_count;
    uint64_t renderer_dirty;
    uint32_t vertex_buffer_max_bound;
    uint64_t vertex_buffer_modified;
    uint32_t constant_buffer_max_bound[SVGA3D_NUM_SHADERTYPE];
    uint32_t constant_buffer_start_slot[SVGA3D_NUM_SHADERTYPE];
    uint32_t constant_buffer_num_buffers[SVGA3D_NUM_SHADERTYPE];
    uint32_t shader_resource_max_bound[SVGA3D_NUM_SHADERTYPE];
    uint64_t shader_resource_modified[SVGA3D_NUM_SHADERTYPE][2];
    uint32_t uav_max_bound;
    uint32_t cs_uav_max_bound;
    uint64_t cs_uav_modified[(SVGA3D_DX11_1_MAX_UAVIEWS + 63u) / 64u];
    SVGADXContextMobFormat shadow;
    VMSVGA3DDXCOTable cotables[SVGA_COTABLE_MAX];
} VMSVGA3DDXContext;

#define VMSVGA3D_MAX_MIP_LEVELS 16
#define VMSVGA3D_SCREEN_TARGET_DAMAGE_RECTS 32

typedef struct vmsvga3d_surface_image_s {
    SVGA3dSize size;
    uint32_t pitch;
    uint32_t plane_size;
    uint32_t data_size;
    uint8_t *data;
} VMSVGA3DSurfaceImage;

typedef struct vmsvga3d_surface_s {
    uint32_t sid;
    SVGA3dSurfaceAllFlags surface_flags;
    SVGA3dSurfaceFormat format;
    SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES];
    uint32_t multisample_count;
    SVGA3dMSPattern multisample_pattern;
    uint32_t multisample_quality;
    SVGA3dTextureFilter autogen_filter;
    uint32_t buffer_byte_stride;
    uint32_t array_elements;
    uint32_t mip_count;
    size_t storage_bytes;
    VMSVGA3DSurfaceImage *mips;
    VMSVGA3DDxvkSurface *dxvk_surface;
    bool screen_target_content_valid;
    bool legacy_active;
    /* Diagnostic-only legacy presentation bookkeeping.  These fields are
     * touched only while the local vmware_setmode trace gate is enabled. */
    uint64_t trace_vgpu9_write_count;
    uint64_t trace_vgpu9_draw_count;
    uint64_t trace_vgpu9_copy_count;
    uint64_t trace_vgpu9_full_copy_count;
    uint64_t trace_vgpu9_dma_write_count;
    uint64_t trace_vgpu9_clear_count;
    uint64_t trace_vgpu9_stretch_count;
    uint64_t trace_vgpu9_present_count;
    uint64_t trace_vgpu9_last_present_write_count;
    uint64_t trace_vgpu9_last_write_3d_cmd;
    uint64_t trace_vgpu9_last_present_3d_cmd;
    uint32_t trace_vgpu9_last_write_kind;
    uint32_t trace_vgpu9_last_write_cid;
} VMSVGA3DSurface;

/* vmware_vga_vgpu10.c is included below after the legacy command handlers.
 * SurfaceCopy needs the same vGPU10 resource materializer when either side
 * has already become D3D11-resident, matching the VirtualBox D3D11 backend.
 */
static bool vmsvga3d_d3d10_copy_surface_materialize_live(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DD3D10ResourceCreateKind create_kind);
static bool vmsvga3d_surface_readback_to_shadow(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, uint32_t subresource);
static bool vmsvga3d_clear_readback_targets(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dClearFlag clear_flags);
static bool vmsvga3d_screen_target_flush_live(struct vmsvga_state_s *s);
static bool vmsvga3d_screen_target_quiesce_live(struct vmsvga_state_s *s);

struct vmsvga3d_state_s {
    VMSVGA3DContext *contexts[SVGA3D_MAX_CONTEXT_IDS];
    VMSVGA3DDXContext *dx_contexts[SVGA3D_MAX_CONTEXT_IDS];
    VMSVGA3DSurface *surfaces[SVGA3D_MAX_SURFACE_IDS];
    VMSVGA3DGBO otables[SVGA_OTABLE_MAX];
    GHashTable *mobs;
    VMSVGA3DGBQuery *gb_queries;
    QEMUTimer *gb_query_timer;
    VMSVGA3DGARTPage *gart_pages;
    SVGAMobId gart_mobid;
    uint32_t gart_page_count;
    bool gart_enabled;
    uint32_t active_dx_context_id;
    uint32_t active_screen_target_sid;
    uint32_t screen_target_dirty_sid;
    uint32_t screen_target_dirty_count;
    SVGA3dRect screen_target_dirty_rects[VMSVGA3D_SCREEN_TARGET_DAMAGE_RECTS];
    bool dx_context_ever_defined;
    uint64_t trace_vgpu9_present_seq;
    uint32_t trace_vgpu9_last_present_sid;
    uint64_t trace_vgpu9_last_present_3d_cmd;
    size_t surface_bytes;
    size_t shader_bytes;
};

static bool vmsvga3d_gb_shader_materialize(struct vmsvga_state_s *s,
                                             uint32_t cid, uint32_t shid,
                                             SVGA3dShaderType type);

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
                                       uint32_t *index)
{
    if (type < SVGA3D_SHADERTYPE_MIN ||
        type >= SVGA3D_SHADERTYPE_PREDX_MAX) {
        return false;
    }

    *index = (uint32_t)type - SVGA3D_SHADERTYPE_MIN;

    return true;
}

static void vmsvga3d_shader_free(VMSVGA3DShader *shader)
{
    if (shader == NULL) {
        return;
    }

    g_free(shader->bytecode);
    g_free(shader);
}

static void vmsvga3d_context_free(struct vmsvga3d_state_s *state,
                                  VMSVGA3DContext *context)
{
    uint32_t type;
    uint32_t shid;

    if (context == NULL) {
        return;
    }

    for (type = 0; type < SVGA3D_NUM_SHADERTYPE_PREDX; type++) {
        for (shid = 0; shid < SVGA3D_MAX_SHADERIDS; shid++) {

            VMSVGA3DShader *shader = context->shader[type][shid];

            if (shader != NULL) {
                if (state != NULL && state->shader_bytes >= shader->bytecode_size) {
                    state->shader_bytes -= shader->bytecode_size;
                } else if (state != NULL) {
                    state->shader_bytes = 0;
                }
                vmsvga3d_shader_free(shader);
            }
        }
    }

    g_free(context);
}

static void vmsvga3d_dx_context_free(VMSVGA3DDXContext *context)
{
    uint32_t type;

    if (context == NULL) {
        return;
    }

    for (type = 0; type < SVGA_COTABLE_MAX; type++) {
        g_free(context->cotables[type].host);
    }

    g_free(context);
}

static void vmsvga3d_surface_free(VMSVGA3DSurface *surface)
{
    uint32_t i;

    if (surface == NULL) {
        return;
    }

    vmsvga3d_dxvk_surface_destroy(surface->dxvk_surface);
    surface->dxvk_surface = NULL;

    for (i = 0; i < surface->mip_count; i++) {
        g_free(surface->mips[i].data);
    }

    g_free(surface->mips);
    g_free(surface);
}

static void vmsvga3d_surface_clear_legacy_bindings(
    struct vmsvga3d_state_s *state, uint32_t sid)
{
    uint32_t cid;

    if (state == NULL) {
        return;
    }

    for (cid = 0; cid < SVGA3D_MAX_CONTEXT_IDS; cid++) {
        VMSVGA3DContext *context = state->contexts[cid];
        uint32_t i;

        if (context == NULL) {
            continue;
        }

        for (i = 0; i < VMSVGA3D_MAX_SAMPLERS; i++) {
            VMSVGA3DStateValue *binding =
                &context->texture_state[i][SVGA3D_TS_BIND_TEXTURE];

            if (binding->valid && binding->value == sid) {
                binding->value = SVGA3D_INVALID_ID;
            }
        }

        for (i = 0; i < SVGA3D_RT_MAX; i++) {
            if (context->render_targets[i].sid == sid) {
                context->render_targets[i].sid = SVGA3D_INVALID_ID;
            }
        }
    }
}

static struct vmsvga3d_state_s *
vmsvga3d_state_ensure(struct vmsvga_state_s *s);
static void vmsvga3d_gart_disable_live(struct vmsvga_state_s *s);

static void *vmsvga3d_dx_cotable_entry_ptr(struct vmsvga_state_s *s,
                                            uint32_t cid,
                                            SVGACOTableType type,
                                            uint32_t index);

static gpointer vmsvga3d_mob_key(SVGAMobId mobid)
{
    return GUINT_TO_POINTER((guint)mobid + 1u);
}

static void vmsvga3d_gb_query_unlink(
    struct vmsvga_state_s *s, VMSVGA3DGBQuery **link,
    bool cancel_native, const char *reason)
{
    VMSVGA3DGBQuery *query;

    if (s == NULL || s->svga3d == NULL || link == NULL || *link == NULL) {
        return;
    }

    query = *link;
    *link = query->next;
    if (cancel_native) {
        vmsvga3d_dxvk_d3d9_gb_query_cancel(s->dxvk, query->token);
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-QUERY phase=cancel cid=%u type=%u token=%" PRIu64
            " mobid=%u offset=0x%08x reason=%s",
            query->cid, (uint32_t)query->type, query->token,
            query->mobid, query->offset,
            reason != NULL ? reason : "unknown");
    }
    g_free(query);

    if (s->svga3d->gb_queries == NULL &&
        s->svga3d->gb_query_timer != NULL) {
        timer_del(s->svga3d->gb_query_timer);
    }
}

static void vmsvga3d_gb_query_cancel_context(
    struct vmsvga_state_s *s, uint32_t cid, const char *reason)
{
    VMSVGA3DGBQuery **link;

    if (s == NULL || s->svga3d == NULL) {
        return;
    }

    link = &s->svga3d->gb_queries;
    while (*link != NULL) {
        if ((*link)->cid != cid) {
            link = &(*link)->next;
            continue;
        }
        vmsvga3d_gb_query_unlink(s, link, true, reason);
    }
}

static void vmsvga3d_gb_query_cancel_mob(
    struct vmsvga_state_s *s, SVGAMobId mobid, const char *reason)
{
    VMSVGA3DGBQuery **link;

    if (s == NULL || s->svga3d == NULL) {
        return;
    }

    link = &s->svga3d->gb_queries;
    while (*link != NULL) {
        if ((*link)->mobid != mobid) {
            link = &(*link)->next;
            continue;
        }
        vmsvga3d_gb_query_unlink(s, link, true, reason);
    }
}

static void vmsvga3d_gb_query_cancel_target(
    struct vmsvga_state_s *s, SVGAMobId mobid, uint32_t offset,
    const char *reason)
{
    VMSVGA3DGBQuery **link;

    if (s == NULL || s->svga3d == NULL) {
        return;
    }

    link = &s->svga3d->gb_queries;
    while (*link != NULL) {
        if ((*link)->mobid != mobid || (*link)->offset != offset) {
            link = &(*link)->next;
            continue;
        }
        vmsvga3d_gb_query_unlink(s, link, true, reason);
    }
}

static void vmsvga3d_gb_query_cancel_all(
    struct vmsvga_state_s *s, const char *reason)
{
    if (s == NULL || s->svga3d == NULL) {
        return;
    }

    while (s->svga3d->gb_queries != NULL) {
        vmsvga3d_gb_query_unlink(
            s, &s->svga3d->gb_queries, true, reason);
    }
}

static bool vmsvga3d_guest_memory_read(struct vmsvga_state_s *s,
                                       uint64_t gpa, void *data,
                                       size_t size)
{
    if (s == NULL || (size != 0 && data == NULL)) {
        return false;
    }

    /* MOB page numbers are guest physical addresses, not PCI DMA IOVAs. */
    return address_space_read(&address_space_memory, gpa,
                              MEMTXATTRS_UNSPECIFIED, data, size) == MEMTX_OK;
}

static bool vmsvga3d_guest_memory_write(struct vmsvga_state_s *s,
                                        uint64_t gpa, const void *data,
                                        size_t size)
{
    if (s == NULL || (size != 0 && data == NULL)) {
        return false;
    }

    return address_space_write(&address_space_memory, gpa,
                               MEMTXATTRS_UNSPECIFIED, data, size) == MEMTX_OK;
}

static void vmsvga3d_gbo_destroy(VMSVGA3DGBO *gbo)
{
    if (gbo == NULL) {
        return;
    }

    g_free(gbo->host);
    g_free(gbo->runs);
    memset(gbo, 0, sizeof(*gbo));
}

static bool vmsvga3d_gbo_add_page(VMSVGA3DGBO *gbo, uint64_t gpa)
{
    VMSVGA3DGBORun *run;

    if (gbo == NULL || gbo->run_count >= gbo->page_count) {
        return false;
    }

    gpa &= VMSVGA3D_GBO_GPA_MASK;
    if (gbo->run_count != 0) {
        run = &gbo->runs[gbo->run_count - 1];
        if (gpa == run->gpa + (uint64_t)run->pages * VMSVGA3D_GBO_PAGE_SIZE) {
            run->pages++;
            return true;
        }
    }

    run = &gbo->runs[gbo->run_count++];
    run->gpa = gpa;
    run->pages = 1;

    return true;
}

static bool vmsvga3d_gbo_read_ppn_page(struct vmsvga_state_s *s,
                                       uint64_t gpa, bool ppn64,
                                       uint8_t page[VMSVGA3D_GBO_PAGE_SIZE])
{
    struct pci_vmsvga_state_s *pci_vmsvga;

    (void)ppn64;
    if (s == NULL) {
        return false;
    }

    /* GBO page-table pages are fetched through the PCI DMA address space. */
    pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    return pci_dma_read(PCI_DEVICE(pci_vmsvga), gpa, page,
                        VMSVGA3D_GBO_PAGE_SIZE) == MEMTX_OK;
}

static uint64_t vmsvga3d_gbo_page_ppn(const uint8_t *page, uint32_t index,
                                      bool ppn64)
{
    if (ppn64) {
        uint64_t value;

        memcpy(&value, page + (size_t)index * sizeof(value), sizeof(value));

        return le64_to_cpu(value);
    } else {
        uint32_t value;

        memcpy(&value, page + (size_t)index * sizeof(value), sizeof(value));

        return le32_to_cpu(value);
    }
}

static bool vmsvga3d_gbo_create(struct vmsvga_state_s *s,
                                SVGAMobFormat format, PPN64 base,
                                uint32_t size, VMSVGA3DGBO *gbo)
{
    uint32_t ppns_per_page;
    uint32_t page_index = 0;
    bool ppn64;

    if (s == NULL || gbo == NULL || size > VMSVGA3D_GBO_MAX_SIZE) {
        return false;
    }

    /*
     * SVGA3D_MOBFMT_EMPTY is a real protocol state, not an invalid GBO.
     * The Win7 VMware KMD uses it as a zero-sized placeholder and later
     * REDEFINE_GB_MOB64 supplies the real backing.  Keep a logical MOB in
     * the host table with no pages so the redefine preserves object identity.
     */
    if (format == SVGA3D_MOBFMT_EMPTY) {
        if (size != 0 || base != SVGA3D_MOB_EMPTY_BASE) {
            return false;
        }
        memset(gbo, 0, sizeof(*gbo));
        gbo->format = format;
        gbo->base = base;
        return true;
    }

    if (size == 0) {
        return false;
    }

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
    }

    memset(gbo, 0, sizeof(*gbo));
    gbo->format = format;
    gbo->base = base;
    gbo->size = size;
    gbo->page_count = (size + VMSVGA3D_GBO_PAGE_SIZE - 1u) /
                      VMSVGA3D_GBO_PAGE_SIZE;
    gbo->runs = g_try_new0(VMSVGA3DGBORun, gbo->page_count);

    if (gbo->runs == NULL) {
        return false;
    }

    if (format == SVGA3D_MOBFMT_RANGE) {
        gbo->runs[0].gpa = (base << VMSVGA3D_GBO_PAGE_SHIFT) &
                           VMSVGA3D_GBO_GPA_MASK;
        gbo->runs[0].pages = gbo->page_count;
        gbo->run_count = 1;
        return true;
    }

    if (format == SVGA3D_MOBFMT_PT_0 || format == SVGA3D_MOBFMT_PT64_0) {
        if (gbo->page_count != 1) {
            vmsvga3d_gbo_destroy(gbo);
            return false;
        }
        if (!vmsvga3d_gbo_add_page(
                gbo, (base << VMSVGA3D_GBO_PAGE_SHIFT) &
                         VMSVGA3D_GBO_GPA_MASK)) {
            vmsvga3d_gbo_destroy(gbo);
            return false;
        }
        return true;
    }

    ppns_per_page = VMSVGA3D_GBO_PAGE_SIZE /
                    (ppn64 ? sizeof(uint64_t) : sizeof(uint32_t));

    if (format == SVGA3D_MOBFMT_PT_1 || format == SVGA3D_MOBFMT_PT64_1) {
        uint8_t root[VMSVGA3D_GBO_PAGE_SIZE];
        uint64_t root_gpa = base << VMSVGA3D_GBO_PAGE_SHIFT;

        if (gbo->page_count > ppns_per_page ||
            !vmsvga3d_gbo_read_ppn_page(s, root_gpa, ppn64, root)) {
            vmsvga3d_gbo_destroy(gbo);
            return false;
        }

        while (page_index < gbo->page_count) {
            uint64_t ppn = vmsvga3d_gbo_page_ppn(root, page_index, ppn64);

            if (!vmsvga3d_gbo_add_page(gbo, ppn << VMSVGA3D_GBO_PAGE_SHIFT)) {
                vmsvga3d_gbo_destroy(gbo);
                return false;
            }
            page_index++;
        }

        return true;
    }

    if ((uint64_t)gbo->page_count >
        (uint64_t)ppns_per_page * ppns_per_page) {
        vmsvga3d_gbo_destroy(gbo);
        return false;
    }
    {
        uint8_t root[VMSVGA3D_GBO_PAGE_SIZE];
        uint64_t root_gpa = base << VMSVGA3D_GBO_PAGE_SHIFT;
        uint32_t level1_count =
            (gbo->page_count + ppns_per_page - 1u) / ppns_per_page;
        uint32_t level1_index;

        if (!vmsvga3d_gbo_read_ppn_page(s, root_gpa, ppn64, root)) {
            vmsvga3d_gbo_destroy(gbo);
            return false;
        }

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
            }

            for (entry = 0;
                 entry < ppns_per_page && page_index < gbo->page_count;
                 entry++, page_index++) {
                uint64_t ppn = vmsvga3d_gbo_page_ppn(level1, entry, ppn64);

                if (!vmsvga3d_gbo_add_page(gbo,
                                            ppn << VMSVGA3D_GBO_PAGE_SHIFT)) {
                    vmsvga3d_gbo_destroy(gbo);
                    return false;
                }
            }
        }
    }

    return page_index == gbo->page_count;
}

static bool vmsvga3d_gbo_transfer(struct vmsvga_state_s *s,
                                  VMSVGA3DGBO *gbo, uint32_t offset,
                                  const void *src, void *dst, size_t size,
                                  bool write_guest)
{
    uint64_t logical = 0;
    uint64_t current = offset;
    uint32_t run_index;

    if (s == NULL || gbo == NULL || offset > gbo->size ||
        size > (size_t)gbo->size - offset ||
        (size != 0 && ((write_guest && src == NULL) ||
                       (!write_guest && dst == NULL)))) {
        return false;
    }

    for (run_index = 0; size != 0 && run_index < gbo->run_count; run_index++) {
        uint64_t run_size =
            (uint64_t)gbo->runs[run_index].pages * VMSVGA3D_GBO_PAGE_SIZE;

        if (current >= logical + run_size) {
            logical += run_size;
            continue;
        }

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
            }
            if (!ok) {
                return false;
            }
            current += chunk;
            size -= chunk;
        }
        logical += run_size;
    }

    return size == 0;
}

static bool vmsvga3d_gbo_read(struct vmsvga_state_s *s, VMSVGA3DGBO *gbo,
                              uint32_t offset, void *data, size_t size)
{
    return vmsvga3d_gbo_transfer(s, gbo, offset, NULL, data, size, false);
}

static bool vmsvga3d_gbo_write(struct vmsvga_state_s *s, VMSVGA3DGBO *gbo,
                               uint32_t offset, const void *data,
                               size_t size)
{
    return vmsvga3d_gbo_transfer(s, gbo, offset, data, NULL, size, true);
}

static bool vmsvga3d_gbo_copy(struct vmsvga_state_s *s,
                              VMSVGA3DGBO *destination,
                              VMSVGA3DGBO *source, uint32_t size)
{
    uint8_t buffer[VMSVGA3D_GBO_PAGE_SIZE];
    uint32_t offset = 0;

    if (destination == NULL || source == NULL ||
        size > destination->size || size > source->size) {
        return false;
    }

    while (offset < size) {
        uint32_t chunk = MIN(size - offset, VMSVGA3D_GBO_PAGE_SIZE);

        if (!vmsvga3d_gbo_read(s, source, offset, buffer, chunk) ||
            !vmsvga3d_gbo_write(s, destination, offset, buffer, chunk)) {
            return false;
        }
        offset += chunk;
    }

    return true;
}

static bool vmsvga3d_otable_index_valid(const VMSVGA3DGBO *table,
                                        uint32_t index,
                                        uint32_t entry_size)
{
    return table != NULL && table->runs != NULL && entry_size != 0 &&
           index < table->size / entry_size;
}

static bool vmsvga3d_otable_read(struct vmsvga_state_s *s,
                                 SVGAOTableType type, uint32_t index,
                                 uint32_t entry_size, void *data,
                                 uint32_t data_size)
{
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
    VMSVGA3DGBO *table;

    if (state == NULL || type >= SVGA_OTABLE_MAX || data_size > entry_size) {
        return false;
    }

    table = &state->otables[type];
    return vmsvga3d_otable_index_valid(table, index, entry_size) &&
           vmsvga3d_gbo_read(s, table, index * entry_size, data, data_size);
}

static bool vmsvga3d_otable_write(struct vmsvga_state_s *s,
                                  SVGAOTableType type, uint32_t index,
                                  uint32_t entry_size, const void *data,
                                  uint32_t data_size)
{
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
    VMSVGA3DGBO *table;

    if (state == NULL || type >= SVGA_OTABLE_MAX || data_size > entry_size) {
        return false;
    }

    table = &state->otables[type];
    return vmsvga3d_otable_index_valid(table, index, entry_size) &&
           vmsvga3d_gbo_write(s, table, index * entry_size, data, data_size);
}

static bool vmsvga3d_otable_set_or_grow(struct vmsvga_state_s *s,
                                        SVGAOTableType type, PPN64 base,
                                        uint32_t size, uint32_t valid_size,
                                        SVGAMobFormat format, bool grow)
{
    struct vmsvga3d_state_s *state = vmsvga3d_state_ensure(s);
    VMSVGA3DGBO replacement;
    VMSVGA3DGBO *current;

    if (state == NULL || type >= SVGA_OTABLE_MAX || size < valid_size) {
        return false;
    }

    current = &state->otables[type];
    if (grow && current->size < valid_size) {
        return false;
    }

    if (size == 0) {
        vmsvga3d_gbo_destroy(current);
        return true;
    }

    if (!vmsvga3d_gbo_create(s, format, base, size, &replacement)) {
        return false;
    }

    if (grow && valid_size != 0 &&
        !vmsvga3d_gbo_copy(s, &replacement, current, valid_size)) {
        vmsvga3d_gbo_destroy(&replacement);
        return false;
    }

    vmsvga3d_gbo_destroy(current);
    *current = replacement;
    return true;
}

static void vmsvga3d_mob_free(gpointer data)
{
    VMSVGA3DMob *mob = data;

    if (mob == NULL) {
        return;
    }

    vmsvga3d_gbo_destroy(&mob->gbo);
    g_free(mob);
}

static VMSVGA3DMob *vmsvga3d_mob_get(struct vmsvga_state_s *s,
                                     SVGAMobId mobid)
{
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;

    if (state == NULL || state->mobs == NULL || mobid == SVGA3D_INVALID_ID) {
        return NULL;
    }

    return g_hash_table_lookup(state->mobs, vmsvga3d_mob_key(mobid));
}

static bool vmsvga3d_mob_define(struct vmsvga_state_s *s, SVGAMobId mobid,
                                SVGAMobFormat format, PPN64 base,
                                uint32_t size)
{
    struct vmsvga3d_state_s *state = vmsvga3d_state_ensure(s);
    SVGAOTableMobEntry entry = { 0 };
    VMSVGA3DMob *mob;

    if (state == NULL || mobid == SVGA3D_INVALID_ID) {
        return false;
    }

    /*
     * Build the host-side GBO before publishing the MOB in the guest-visible
     * OTable.  If page-table decoding fails, leaving a valid-looking OTable
     * entry behind lets later surface binds refer to a MOB which does not
     * exist in our host-side MOB table.
     */
    mob = g_try_new0(VMSVGA3DMob, 1);
    if (mob == NULL) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "MOB define mobid=%u format=%u base=0x%016" PRIx64
            " size=%u result=REJECT reason=alloc",
            mobid, (unsigned)format, (uint64_t)base, size);
        return false;
    }

    mob->mobid = mobid;
    if (!vmsvga3d_gbo_create(s, format, base, size, &mob->gbo)) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "MOB define mobid=%u format=%u base=0x%016" PRIx64
            " size=%u result=REJECT reason=gbo-create",
            mobid, (unsigned)format, (uint64_t)base, size);
        g_free(mob);
        return false;
    }

    entry.ptDepth = cpu_to_le32(format);
    entry.sizeInBytes = cpu_to_le32(size);
    entry.base = cpu_to_le64(base);
    if (!vmsvga3d_otable_write(s, SVGA_OTABLE_MOB, mobid,
                                sizeof(SVGAOTableMobEntry), &entry,
                                sizeof(entry))) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "MOB define mobid=%u format=%u base=0x%016" PRIx64
            " size=%u result=REJECT reason=otable-write",
            mobid, (unsigned)format, (uint64_t)base, size);
        vmsvga3d_gbo_destroy(&mob->gbo);
        g_free(mob);
        return false;
    }

    vmsvga3d_gb_query_cancel_mob(s, mobid, "mob-define");
    g_hash_table_replace(state->mobs, vmsvga3d_mob_key(mobid), mob);
    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "MOB define mobid=%u format=%u base=0x%016" PRIx64
        " size=%u pages=%u runs=%u result=OK",
        mobid, (unsigned)format, (uint64_t)base, size,
        mob->gbo.page_count, mob->gbo.run_count);
    return true;
}

static bool vmsvga3d_mob_destroy(struct vmsvga_state_s *s,
                                 SVGAMobId mobid)
{
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
    SVGAOTableMobEntry entry = { 0 };

    if (state == NULL || state->mobs == NULL ||
        mobid == SVGA3D_INVALID_ID) {
        return false;
    }

    (void)vmsvga3d_otable_write(s, SVGA_OTABLE_MOB, mobid,
                                 sizeof(SVGAOTableMobEntry), &entry,
                                 sizeof(entry));

    if (state->gart_enabled && state->gart_mobid == mobid) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GART backing-mob-destroy mobid=%u action=DISABLE",
            mobid);
        vmsvga3d_gart_disable_live(s);
    }

    vmsvga3d_gb_query_cancel_mob(s, mobid, "mob-destroy");
    return g_hash_table_remove(state->mobs, vmsvga3d_mob_key(mobid));
}

static bool vmsvga3d_mob_read(struct vmsvga_state_s *s, VMSVGA3DMob *mob,
                              uint32_t offset, void *data, uint32_t size)
{
    return mob != NULL &&
           vmsvga3d_gbo_read(s, &mob->gbo, offset, data, size);
}

static bool vmsvga3d_mob_write(struct vmsvga_state_s *s, VMSVGA3DMob *mob,
                               uint32_t offset, const void *data,
                               uint32_t size)
{
    return mob != NULL &&
           vmsvga3d_gbo_write(s, &mob->gbo, offset, data, size);
}

static uint32_t vmsvga3d_gart_max_pages(const struct vmsvga_state_s *s)
{
    uint64_t pages;

    if (s == NULL) {
        return 0;
    }

    pages = (uint64_t)s->vga.vram_size / VMSVGA3D_GBO_PAGE_SIZE;
    return (uint32_t)MIN(pages, (uint64_t)UINT32_MAX);
}

static MemoryRegion *vmsvga3d_gart_bar1(struct vmsvga_state_s *s)
{
    struct pci_vmsvga_state_s *pci_vmsvga;

    if (s == NULL) {
        return NULL;
    }

    pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    return &pci_vmsvga->bar1;
}

static bool vmsvga3d_gart_resolve_page(uint64_t gpa,
                                       MemoryRegionSection *section)
{
    if (section == NULL || address_space_memory.root == NULL) {
        return false;
    }

    *section = memory_region_find(address_space_memory.root, gpa,
                                  VMSVGA3D_GBO_PAGE_SIZE);
    if (section->mr == NULL ||
        int128_get64(section->size) < VMSVGA3D_GBO_PAGE_SIZE) {
        if (section->mr != NULL) {
            memory_region_unref(section->mr);
            section->mr = NULL;
        }
        return false;
    }

    return true;
}

static void vmsvga3d_gart_page_remove_alias(struct vmsvga_state_s *s,
                                             VMSVGA3DGARTPage *page)
{
    MemoryRegion *bar1;

    if (page == NULL || !page->alias_active) {
        return;
    }

    bar1 = vmsvga3d_gart_bar1(s);
    if (bar1 != NULL) {
        memory_region_del_subregion(bar1, &page->alias);
    }
    object_unparent(OBJECT(&page->alias));
    page->alias_active = false;
}

static void vmsvga3d_gart_page_install_alias(
    struct vmsvga_state_s *s, VMSVGA3DGARTPage *page,
    uint32_t aperture_page, const MemoryRegionSection *section)
{
    struct pci_vmsvga_state_s *pci_vmsvga;
    g_autofree char *name = NULL;

    if (s == NULL || page == NULL || section == NULL || section->mr == NULL) {
        return;
    }

    pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    name = g_strdup_printf("vmsvga-gart-%u", aperture_page);
    memory_region_init_alias(&page->alias, OBJECT(pci_vmsvga), name,
                             section->mr, section->offset_within_region,
                             VMSVGA3D_GBO_PAGE_SIZE);
    memory_region_add_subregion_overlap(&pci_vmsvga->bar1,
                                        (hwaddr)aperture_page *
                                            VMSVGA3D_GBO_PAGE_SIZE,
                                        &page->alias, 1);
    page->alias_active = true;
}

static void vmsvga3d_gart_clear_pages(struct vmsvga3d_state_s *state)
{
    uint32_t page;

    if (state == NULL || state->gart_pages == NULL) {
        return;
    }

    for (page = 0; page < state->gart_page_count; page++) {
        state->gart_pages[page].gpa = 0;
        state->gart_pages[page].mobid = SVGA3D_INVALID_ID;
        state->gart_pages[page].mob_page = 0;
        state->gart_pages[page].alias_active = false;
    }
}

static void vmsvga3d_gart_disable_live(struct vmsvga_state_s *s)
{
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
    uint32_t page;

    if (state == NULL) {
        return;
    }

    if (state->gart_pages != NULL) {
        memory_region_transaction_begin();
        for (page = 0; page < state->gart_page_count; page++) {
            vmsvga3d_gart_page_remove_alias(s, &state->gart_pages[page]);
        }
        memory_region_transaction_commit();
    }

    g_free(state->gart_pages);
    state->gart_pages = NULL;
    state->gart_mobid = SVGA3D_INVALID_ID;
    state->gart_page_count = 0;
    state->gart_enabled = false;
}

static bool vmsvga3d_gart_enable_live(struct vmsvga_state_s *s,
                                      SVGAMobId mobid, uint32_t must_be_zero,
                                      uint32_t initialized)
{
    struct vmsvga3d_state_s *state = vmsvga3d_state_ensure(s);
    VMSVGA3DMob *mob;
    VMSVGA3DGARTPage *pages;
    uint32_t page_count;
    uint32_t max_pages;
    bool preserve;

    if (state == NULL || must_be_zero != 0 || initialized > 1) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, mobid);
    if (mob == NULL || mob->gbo.size < VMSVGA3D_GART_ENTRY_SIZE ||
        (mob->gbo.size % VMSVGA3D_GART_ENTRY_SIZE) != 0) {
        return false;
    }

    page_count = mob->gbo.size / VMSVGA3D_GART_ENTRY_SIZE;
    max_pages = vmsvga3d_gart_max_pages(s);
    if (page_count == 0 || page_count > max_pages) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GART enable mobid=%u pages=%u max-pages=%u result=REJECT-RANGE",
            mobid, page_count, max_pages);
        return false;
    }

    preserve = initialized != 0 && state->gart_enabled &&
               state->gart_mobid == mobid &&
               state->gart_page_count == page_count &&
               state->gart_pages != NULL;
    if (!preserve) {
        if (state->gart_pages != NULL) {
            vmsvga3d_gart_disable_live(s);
            state = vmsvga3d_state_ensure(s);
            if (state == NULL) {
                return false;
            }
        }

        pages = g_try_new0(VMSVGA3DGARTPage, page_count);
        if (pages == NULL) {
            return false;
        }

        state->gart_pages = pages;
        state->gart_page_count = page_count;
        vmsvga3d_gart_clear_pages(state);
    }

    state->gart_mobid = mobid;
    state->gart_enabled = true;

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "GART enable mobid=%u pages=%u max-pages=%u initialized=%u preserve=%u result=OK",
        mobid, page_count, max_pages, initialized, preserve ? 1u : 0u);
    return true;
}

static bool vmsvga3d_gart_map_mob_live(struct vmsvga_state_s *s,
                                       SVGAMobId mobid, uint32_t gart_offset)
{
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
    VMSVGA3DMob *mob;
    MemoryRegionSection *sections;
    uint64_t *gpas;
    uint32_t first_page;
    uint32_t mob_page = 0;
    uint32_t run_index;
    uint32_t page;
    bool ok = false;

    if (state == NULL || !state->gart_enabled || state->gart_pages == NULL ||
        (gart_offset & (VMSVGA3D_GBO_PAGE_SIZE - 1u)) != 0) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, mobid);
    if (mob == NULL) {
        return false;
    }

    first_page = gart_offset >> VMSVGA3D_GBO_PAGE_SHIFT;
    if (first_page >= state->gart_page_count ||
        mob->gbo.page_count > state->gart_page_count - first_page) {
        return false;
    }

    if (mob->gbo.page_count == 0) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GART map mobid=%u offset=0x%08x first=%u pages=0 result=OK",
            mobid, gart_offset, first_page);
        return true;
    }

    sections = g_try_new0(MemoryRegionSection, mob->gbo.page_count);
    gpas = g_try_new(uint64_t, mob->gbo.page_count);
    if (sections == NULL || gpas == NULL) {
        g_free(sections);
        g_free(gpas);
        return false;
    }

    /*
     * The VMware host owns the serialized 64-bit GART PTE format.  The guest
     * command supplies only a MOB id and aperture offset, so keep the
     * equivalent translation as host-side shadow state rather than inventing
     * guest-visible PTE bits.  Snapshot GPAs at MAP time so DESTROY_GB_MOB
     * followed by UNMAP_GART_RANGE, as used by the Win7 KMD, remains valid.
     *
     * Resolve the complete mapping before changing BAR1 so MAP is atomic from
     * the guest's point of view.  Each alias points at the actual system-memory
     * leaf for the MOB page, keeping CPU aperture accesses coherent with MOB
     * reads/writes without a software shadow copy.
     */
    for (run_index = 0; run_index < mob->gbo.run_count; run_index++) {
        const VMSVGA3DGBORun *run = &mob->gbo.runs[run_index];
        uint32_t run_page;

        for (run_page = 0; run_page < run->pages; run_page++, mob_page++) {
            uint64_t gpa = run->gpa +
                           (uint64_t)run_page * VMSVGA3D_GBO_PAGE_SIZE;

            if (mob_page >= mob->gbo.page_count ||
                !vmsvga3d_gart_resolve_page(gpa, &sections[mob_page])) {
                goto out;
            }
            gpas[mob_page] = gpa;
        }
    }

    if (mob_page != mob->gbo.page_count) {
        goto out;
    }

    memory_region_transaction_begin();
    for (page = 0; page < mob->gbo.page_count; page++) {
        VMSVGA3DGARTPage *entry = &state->gart_pages[first_page + page];

        vmsvga3d_gart_page_remove_alias(s, entry);
        vmsvga3d_gart_page_install_alias(s, entry, first_page + page,
                                         &sections[page]);
        entry->gpa = gpas[page];
        entry->mobid = mobid;
        entry->mob_page = page;
    }
    memory_region_transaction_commit();
    ok = true;

out:
    for (page = 0; page < mob->gbo.page_count; page++) {
        if (sections[page].mr != NULL) {
            memory_region_unref(sections[page].mr);
        }
    }
    g_free(sections);
    g_free(gpas);

    if (!ok) {
        return false;
    }

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "GART map mobid=%u offset=0x%08x first=%u pages=%u result=OK",
        mobid, gart_offset, first_page, mob->gbo.page_count);
    return true;
}

static bool vmsvga3d_gart_unmap_live(struct vmsvga_state_s *s,
                                     uint32_t gart_offset, uint32_t num_pages)
{
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
    uint32_t first_page;
    uint32_t page;

    if (state == NULL || !state->gart_enabled || state->gart_pages == NULL ||
        (gart_offset & (VMSVGA3D_GBO_PAGE_SIZE - 1u)) != 0) {
        return false;
    }

    first_page = gart_offset >> VMSVGA3D_GBO_PAGE_SHIFT;
    if (first_page > state->gart_page_count ||
        num_pages > state->gart_page_count - first_page) {
        return false;
    }

    memory_region_transaction_begin();
    for (page = 0; page < num_pages; page++) {
        VMSVGA3DGARTPage *entry = &state->gart_pages[first_page + page];

        vmsvga3d_gart_page_remove_alias(s, entry);
        entry->gpa = 0;
        entry->mobid = SVGA3D_INVALID_ID;
        entry->mob_page = 0;
    }
    memory_region_transaction_commit();

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "GART unmap offset=0x%08x first=%u pages=%u result=OK",
        gart_offset, first_page, num_pages);
    return true;
}

static bool vmsvga3d_gbo_page_gpa(const VMSVGA3DGBO *gbo,
                                    uint32_t page_index, uint64_t *gpa)
{
    uint32_t logical_page = 0;
    uint32_t run_index;

    if (gbo == NULL || gpa == NULL || page_index >= gbo->page_count) {
        return false;
    }

    for (run_index = 0; run_index < gbo->run_count; run_index++) {
        const VMSVGA3DGBORun *run = &gbo->runs[run_index];

        if (page_index < logical_page + run->pages) {
            *gpa = run->gpa +
                   (uint64_t)(page_index - logical_page) *
                       VMSVGA3D_GBO_PAGE_SIZE;
            return true;
        }
        logical_page += run->pages;
    }

    return false;
}

static bool vmsvga3d_mob_redefine(struct vmsvga_state_s *s,
                                  SVGAMobId mobid, SVGAMobFormat format,
                                  PPN64 base, uint32_t size)
{
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
    SVGAOTableMobEntry entry = { 0 };
    VMSVGA3DGBO replacement;
    VMSVGA3DGBO old;
    VMSVGA3DMob *mob;
    uint32_t refreshed = 0;
    uint32_t invalidated = 0;
    uint32_t page;

    if (state == NULL || mobid == SVGA3D_INVALID_ID) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, mobid);
    if (mob == NULL) {
        bool recovered;

        /*
         * The command carries a complete replacement mapping.  Recover a
         * missing host-side MOB as a fresh definition rather than preserving
         * a stale guest-visible binding with no host object behind it.  This
         * also repairs state after an earlier DEFINE whose GBO construction
         * could not be completed.
         */
        recovered = vmsvga3d_mob_define(s, mobid, format, base, size);
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "MOB redefine mobid=%u format=%u base=0x%016" PRIx64
            " size=%u missing-mob=1 recovery=%s",
            mobid, (unsigned)format, (uint64_t)base, size,
            recovered ? "DEFINE-OK" : "DEFINE-FAIL");
        return recovered;
    }

    memset(&replacement, 0, sizeof(replacement));
    if (!vmsvga3d_gbo_create(s, format, base, size, &replacement)) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "MOB redefine mobid=%u format=%u base=0x%016" PRIx64
            " size=%u result=REJECT reason=gbo-create",
            mobid, (unsigned)format, (uint64_t)base, size);
        return false;
    }

    entry.ptDepth = cpu_to_le32(format);
    entry.sizeInBytes = cpu_to_le32(size);
    entry.base = cpu_to_le64(base);
    if (!vmsvga3d_otable_write(s, SVGA_OTABLE_MOB, mobid,
                               sizeof(SVGAOTableMobEntry), &entry,
                               sizeof(entry))) {
        vmsvga3d_gbo_destroy(&replacement);
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "MOB redefine mobid=%u result=REJECT reason=otable-write",
            mobid);
        return false;
    }

    vmsvga3d_gb_query_cancel_mob(s, mobid, "mob-redefine");
    old = mob->gbo;
    mob->gbo = replacement;

    /*
     * MAP_MOB_INTO_GART stores a snapshot of each translated MOB page in the
     * host-side GART shadow.  A redefine changes that translation without a
     * new MAP command, so refresh every shadow entry which still names this
     * MOB.  Entries that referred to pages beyond a newly smaller MOB become
     * unmapped instead of retaining stale guest physical addresses.
     */
    if (state->gart_enabled && state->gart_pages != NULL) {
        MemoryRegionSection *sections =
            g_new0(MemoryRegionSection, state->gart_page_count);
        uint64_t *gpas = g_new0(uint64_t, state->gart_page_count);
        bool *resolved = g_new0(bool, state->gart_page_count);

        for (page = 0; page < state->gart_page_count; page++) {
            VMSVGA3DGARTPage *gart_page = &state->gart_pages[page];
            uint64_t gpa;

            if (gart_page->mobid != mobid) {
                continue;
            }

            if (vmsvga3d_gbo_page_gpa(&mob->gbo, gart_page->mob_page,
                                       &gpa) &&
                vmsvga3d_gart_resolve_page(gpa, &sections[page])) {
                gpas[page] = gpa;
                resolved[page] = true;
            }
        }

        memory_region_transaction_begin();
        for (page = 0; page < state->gart_page_count; page++) {
            VMSVGA3DGARTPage *gart_page = &state->gart_pages[page];

            if (gart_page->mobid != mobid) {
                continue;
            }

            vmsvga3d_gart_page_remove_alias(s, gart_page);
            if (resolved[page]) {
                vmsvga3d_gart_page_install_alias(s, gart_page, page,
                                                 &sections[page]);
                gart_page->gpa = gpas[page];
                refreshed++;
            } else {
                gart_page->gpa = 0;
                gart_page->mobid = SVGA3D_INVALID_ID;
                gart_page->mob_page = 0;
                invalidated++;
            }
        }
        memory_region_transaction_commit();

        for (page = 0; page < state->gart_page_count; page++) {
            if (sections[page].mr != NULL) {
                memory_region_unref(sections[page].mr);
            }
        }
        g_free(resolved);
        g_free(gpas);
        g_free(sections);
    }

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "MOB redefine mobid=%u old-format=%u old-base=0x%016" PRIx64
        " old-size=%u new-format=%u new-base=0x%016" PRIx64
        " new-size=%u gart-refreshed=%u gart-invalidated=%u result=OK",
        mobid, (unsigned)old.format, (uint64_t)old.base, old.size,
        (unsigned)format, (uint64_t)base, size, refreshed, invalidated);

    vmsvga3d_gbo_destroy(&old);
    return true;
}

static struct vmsvga3d_state_s *
vmsvga3d_state_ensure(struct vmsvga_state_s *s)
{
    if (s->svga3d == NULL) {
        s->svga3d = g_try_new0(struct vmsvga3d_state_s, 1);
        if (s->svga3d != NULL) {
            s->svga3d->gart_mobid = SVGA3D_INVALID_ID;
            s->svga3d->active_dx_context_id = SVGA3D_INVALID_ID;
            s->svga3d->active_screen_target_sid = SVGA3D_INVALID_ID;
            s->svga3d->screen_target_dirty_sid = SVGA3D_INVALID_ID;
            s->svga3d->trace_vgpu9_last_present_sid = SVGA3D_INVALID_ID;
        }
    }

    if (s->svga3d != NULL && s->svga3d->mobs == NULL) {
        s->svga3d->mobs = g_hash_table_new_full(
            g_direct_hash, g_direct_equal, NULL, vmsvga3d_mob_free);
    }

    return s->svga3d;
}

static void vmsvga3d_renderer_surface_renderer_set(
    struct vmsvga_state_s *s, VMSVGA3DDxvk *dxvk, bool evict)
{
    struct vmsvga3d_state_s *state = s->svga3d;
    uint32_t sid;

    if (state == NULL) {
        return;
    }

    for (sid = 0; sid < SVGA3D_MAX_SURFACE_IDS; sid++) {
        VMSVGA3DSurface *surface = state->surfaces[sid];

        if (surface == NULL || surface->dxvk_surface == NULL) {
            continue;
        }
        if (evict) {
            vmsvga3d_dxvk_surface_evict(surface->dxvk_surface);
        }
        vmsvga3d_dxvk_surface_set_renderer(surface->dxvk_surface, dxvk);
    }
}

static void vmsvga3d_renderer_realize(struct vmsvga_state_s *s)
{
    Error *local_err = NULL;

    vmsvga3d_gb_query_cancel_all(s, "renderer-realize");

    if (s->svga3d != NULL) {
        s->svga3d->active_dx_context_id = SVGA3D_INVALID_ID;
        s->svga3d->active_screen_target_sid = SVGA3D_INVALID_ID;
        s->svga3d->screen_target_dirty_sid = SVGA3D_INVALID_ID;
        s->svga3d->screen_target_dirty_count = 0;
        memset(s->svga3d->screen_target_dirty_rects, 0,
               sizeof(s->svga3d->screen_target_dirty_rects));
    }

    s->svga3d_capable = false;
    s->svga3d_dx_capable = false;

    vmsvga3d_renderer_surface_renderer_set(s, NULL, true);
    vmsvga3d_dxvk_destroy(s->dxvk);
    s->dxvk = NULL;

    if (!s->enable_3d) {
        return;
    }

    s->dxvk = vmsvga3d_dxvk_create(
        s->active_valid ? s->active_width : 0,
        s->active_valid ? s->active_height : 0, s->debug, &local_err);

    if (vmsvga3d_dxvk_ready(s->dxvk)) {
        vmsvga3d_renderer_surface_renderer_set(s, s->dxvk, false);
        s->svga3d_capable = true;
        s->svga3d_dx_capable = vmsvga3d_dxvk_d3d11_ready(s->dxvk);
    } else {
        VMSVGA3DDxvk *diagnostic_dxvk = NULL;
        Error *diagnostic_err = NULL;

        vmsvga3d_dxvk_destroy(s->dxvk);
        s->dxvk = NULL;

        fprintf(stderr,
                "VMVGA: warning: 3D acceleration was requested, but DXVK "
                "initialization failed: %s\n",
                local_err != NULL ? error_get_pretty(local_err) :
                                    "unknown error");

        if (!s->debug) {
            fprintf(stderr,
                    "VMVGA: retrying DXVK initialization with diagnostics "
                    "enabled:\n");
            diagnostic_dxvk = vmsvga3d_dxvk_create(
                s->active_valid ? s->active_width : 0,
                s->active_valid ? s->active_height : 0, true,
                &diagnostic_err);
            vmsvga3d_dxvk_destroy(diagnostic_dxvk);
            error_free(diagnostic_err);
        }
    }

    error_free(local_err);
}

static void vmsvga3d_renderer_unrealize(struct vmsvga_state_s *s)
{
    vmsvga3d_gb_query_cancel_all(s, "renderer-unrealize");

    if (s->svga3d != NULL) {
        s->svga3d->active_dx_context_id = SVGA3D_INVALID_ID;
        s->svga3d->active_screen_target_sid = SVGA3D_INVALID_ID;
        s->svga3d->screen_target_dirty_sid = SVGA3D_INVALID_ID;
        s->svga3d->screen_target_dirty_count = 0;
        memset(s->svga3d->screen_target_dirty_rects, 0,
               sizeof(s->svga3d->screen_target_dirty_rects));
    }

    s->svga3d_capable = false;
    s->svga3d_dx_capable = false;

    vmsvga3d_renderer_surface_renderer_set(s, NULL, true);
    vmsvga3d_dxvk_destroy(s->dxvk);

    s->dxvk = NULL;
}

static void vmsvga3d_reset(struct vmsvga_state_s *s)
{
    struct vmsvga3d_state_s *state = s->svga3d;
    uint32_t i;

    if (state == NULL) {
        return;
    }

    vmsvga3d_gb_query_cancel_all(s, "reset");
    if (state->gb_query_timer != NULL) {
        timer_del(state->gb_query_timer);
        timer_free(state->gb_query_timer);
        state->gb_query_timer = NULL;
    }

    /* VirtualBox destroys each legacy context's D3D9 device during reset,
     * which drops all native state references before guest resources are freed.
     * QEMU uses one shared D3D9 device, so restore its captured pristine state
     * to provide the equivalent teardown boundary. */
    (void)vmsvga3d_dxvk_reset_state(s->dxvk);

    vmsvga3d_dxvk_reset_guest_objects(s->dxvk,
                                      state->dx_context_ever_defined);

    for (i = 0; i < SVGA3D_MAX_CONTEXT_IDS; i++) {
        vmsvga3d_context_free(state, state->contexts[i]);
        vmsvga3d_dx_context_free(state->dx_contexts[i]);
    }

    for (i = 0; i < SVGA3D_MAX_SURFACE_IDS; i++) {
        vmsvga3d_surface_free(state->surfaces[i]);
    }

    for (i = 0; i < SVGA_OTABLE_MAX; i++) {
        vmsvga3d_gbo_destroy(&state->otables[i]);
    }

    vmsvga3d_gart_disable_live(s);
    g_hash_table_destroy(state->mobs);
    g_free(state);

    s->svga3d = NULL;
}

static bool vmsvga3d_fifo_supported_command(uint32_t cmd)
{
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
    case SVGA_3D_CMD_SET_GB_SHADERCONSTS_INLINE:
    case SVGA_3D_CMD_DRAW_PRIMITIVES:
    case SVGA_3D_CMD_SETSCISSORRECT:
    case SVGA_3D_CMD_BEGIN_QUERY:
    case SVGA_3D_CMD_END_QUERY:
    case SVGA_3D_CMD_WAIT_FOR_QUERY:
    case SVGA_3D_CMD_GENERATE_MIPMAPS:
    case SVGA_3D_CMD_SET_OTABLE_BASE:
    case SVGA_3D_CMD_SET_OTABLE_BASE64:
    case SVGA_3D_CMD_BEGIN_GB_QUERY:
    case SVGA_3D_CMD_END_GB_QUERY:
    case SVGA_3D_CMD_WAIT_FOR_GB_QUERY:
    case SVGA_3D_CMD_GROW_OTABLE:
    case SVGA_3D_CMD_DEFINE_GB_MOB:
    case SVGA_3D_CMD_DEFINE_GB_MOB64:
    case SVGA_3D_CMD_DESTROY_GB_MOB:
    case SVGA_3D_CMD_ENABLE_GART:
    case SVGA_3D_CMD_DISABLE_GART:
    case SVGA_3D_CMD_MAP_MOB_INTO_GART:
    case SVGA_3D_CMD_UNMAP_GART_RANGE:
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
    }
}

static void vmsvga3d_fifo_rewind(struct vmsvga_state_s *s, int32_t *len,
                                 uint32_t fifo_start)
{
    s->fifo_stop = fifo_start;
    s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
    *len = 0;
}

static bool vmsvga3d_fifo_read_header(struct vmsvga_state_s *s,
                                      int32_t *len, uint32_t fifo_start,
                                      uint32_t *payload_size,
                                      uint32_t *payload_words,
                                      uint32_t *total_words)
{
    if (*len < 2) {
        vmsvga3d_fifo_rewind(s, len, fifo_start);
        return false;
    }

    *payload_size = vmsvga_fifo_read(s);
    if ((*payload_size & (sizeof(uint32_t) - 1)) != 0) {
        vmsvga3d_fifo_rewind(s, len, fifo_start);
        return false;
    }

    *payload_words = *payload_size / sizeof(uint32_t);
    *total_words = *payload_words + 2;
    if (*len < 0 || *total_words > (uint32_t)*len) {
        vmsvga3d_fifo_rewind(s, len, fifo_start);
        return false;
    }

    return true;
}

static bool vmsvga3d_fifo_read_payload(struct vmsvga_state_s *s,
                                       int32_t *len, uint32_t fifo_start,
                                       void **payload, uint32_t *size)
{
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
    }

    if (payload_size != 0) {
        data = g_try_malloc(payload_size);
        if (data == NULL) {
            vmsvga3d_fifo_rewind(s, len, fifo_start);
            return false;
        }
        for (i = 0; i < payload_words; i++) {
            data[i] = vmsvga_fifo_read(s);
        }
    }

    *len -= (int32_t)total_words;
    *payload = data;
    *size = payload_size;

    return true;
}

static bool vmsvga3d_fifo_discard_packet(struct vmsvga_state_s *s,
                                         int32_t *len, uint32_t fifo_start)
{
    uint32_t payload_size;
    uint32_t payload_words;
    uint32_t total_words;

    if (!vmsvga3d_fifo_read_header(s, len, fifo_start, &payload_size,
                                    &payload_words, &total_words)) {
        return false;
    }

    while (payload_words > 0) {
        vmsvga_fifo_read(s);
        payload_words--;
    }

    *len -= (int32_t)total_words;

    return true;
}

static bool vmsvga3d_surface_faces_valid(
    SVGA3dSurfaceAllFlags surface_flags,
    const SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES],
    uint32_t array_elements, uint32_t mip_count)
{
    uint32_t i;
    uint32_t levels;

    if (array_elements == 0 || array_elements > SVGA3D_MAX_SURFACE_ARRAYSIZE ||
        mip_count == 0 || face[0].numMipLevels == 0) {
        return false;
    }

    levels = face[0].numMipLevels;
    if (levels > VMSVGA3D_MAX_MIP_LEVELS ||
        levels > UINT32_MAX / array_elements ||
        levels * array_elements != mip_count) {
        return false;
    }

    if (surface_flags & SVGA3D_SURFACE_CUBEMAP) {
        if (array_elements % SVGA3D_MAX_SURFACE_FACES != 0) {
            return false;
        }
        for (i = 1; i < SVGA3D_MAX_SURFACE_FACES; i++) {
            if (face[i].numMipLevels != levels) {
                return false;
            }
        }
    } else {
        for (i = 1; i < SVGA3D_MAX_SURFACE_FACES; i++) {
            if (face[i].numMipLevels != 0) {
                return false;
            }
        }
    }

    return true;
}

static bool vmsvga3d_surface_sizes_valid(
    SVGA3dSurfaceAllFlags surface_flags, SVGA3dSurfaceFormat format,
    const SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES],
    const SVGA3dSize *mip_sizes, uint32_t array_elements,
    uint32_t mip_count)
{
    uint32_t array_index;
    uint32_t mip_index;
    uint32_t levels;
    SVGA3dSize base;
    SVGA3dSize expected;

    if (mip_count == 0) {
        return false;
    }

    levels = face[0].numMipLevels;
    base = mip_sizes[0];

    if (base.width == 0 || base.height == 0 || base.depth == 0) {
        return false;
    }

    if (format != SVGA3D_BUFFER) {
        if (surface_flags & SVGA3D_SURFACE_VOLUME) {
            if (base.width > 2048 || base.height > 2048 || base.depth > 2048) {
                return false;
            }
        } else if (surface_flags & SVGA3D_SURFACE_1D) {
            if (base.width > 16384 || base.height != 1 || base.depth != 1) {
                return false;
            }
        } else if (base.width > 16384 || base.height > 16384 || base.depth != 1) {
            return false;
        }
    }

    for (array_index = 0; array_index < array_elements; array_index++) {
        uint32_t base_index = array_index * levels;

        if (mip_sizes[base_index].width != base.width ||
            mip_sizes[base_index].height != base.height ||
            mip_sizes[base_index].depth != base.depth) {
            return false;
        }

        for (mip_index = 0; mip_index < levels; mip_index++) {
            uint32_t index = base_index + mip_index;

            if (index >= mip_count) {
                return false;
            }
            expected = svga3dsurface_get_mip_size(base, mip_index);
            if (mip_sizes[index].width != expected.width ||
                mip_sizes[index].height != expected.height ||
                mip_sizes[index].depth != expected.depth) {
                return false;
            }
        }
    }

    return true;
}

static bool vmsvga3d_surface_image_layout(
    SVGA3dSurfaceFormat format, const SVGA3dSize *size,
    uint32_t multisample_count, VMSVGA3DSurfaceImage *image)
{
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
    }

    blocks_x = ((uint64_t)size->width + desc->block_size.width - 1) /
               desc->block_size.width;
    blocks_y = ((uint64_t)size->height + desc->block_size.height - 1) /
               desc->block_size.height;
    blocks_z = ((uint64_t)size->depth + desc->block_size.depth - 1) /
               desc->block_size.depth;

    if (blocks_x > UINT64_MAX / desc->pitch_bytes_per_block) {
        return false;
    }

    pitch = blocks_x * desc->pitch_bytes_per_block;
    if (blocks_x > UINT64_MAX / blocks_y) {
        return false;
    }

    plane_size = blocks_x * blocks_y;
    if (plane_size > UINT64_MAX / desc->bytes_per_block) {
        return false;
    }

    plane_size *= desc->bytes_per_block;
    samples = MAX(multisample_count, 1U);
    if (plane_size > UINT64_MAX / blocks_z) {
        return false;
    }

    data_size = plane_size * blocks_z;
    if (data_size > UINT64_MAX / samples) {
        return false;
    }

    data_size *= samples;
    if (pitch == 0 || pitch > UINT32_MAX || plane_size == 0 ||
        plane_size > UINT32_MAX || data_size == 0 || data_size > UINT32_MAX) {
        return false;
    }

    image->size = *size;
    image->pitch = (uint32_t)pitch;
    image->plane_size = (uint32_t)plane_size;
    image->data_size = (uint32_t)data_size;

    return true;
}

static void vmsvga3d_surface_install(
    struct vmsvga_state_s *s, uint32_t sid,
    SVGA3dSurfaceAllFlags surface_flags, SVGA3dSurfaceFormat format,
    const SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES],
    uint32_t multisample_count, SVGA3dMSPattern multisample_pattern,
    SVGA3dTextureFilter autogen_filter,
    uint32_t array_elements, const SVGA3dSize *mip_sizes,
    uint32_t mip_count)
{
    struct vmsvga3d_state_s *state;
    VMSVGA3DSurface *old_surface;
    VMSVGA3DSurface *surface;
    size_t old_bytes;
    size_t limit;
    uint64_t storage_bytes = 0;
    uint32_t i;

    if (sid >= SVGA3D_MAX_SURFACE_IDS) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=SID_RANGE sid=%u "
                          "format=%u flags=0x%016" PRIx64 " mips=%u arrays=%u",
                          sid, format, surface_flags, mip_count, array_elements);
        return;
    }

    if (!vmsvga3d_surface_faces_valid(surface_flags, face, array_elements,
                                       mip_count)) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=FACES sid=%u "
                          "format=%u flags=0x%016" PRIx64 " mips=%u arrays=%u",
                          sid, format, surface_flags, mip_count, array_elements);
        return;
    }

    if (!vmsvga3d_surface_sizes_valid(surface_flags, format, face, mip_sizes,
                                      array_elements, mip_count)) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=SIZES sid=%u "
                          "format=%u flags=0x%016" PRIx64 " mips=%u arrays=%u",
                          sid, format, surface_flags, mip_count, array_elements);
        return;
    }

    surface = g_try_new0(VMSVGA3DSurface, 1);
    if (surface == NULL) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=ALLOC_SURFACE sid=%u "
                          "format=%u flags=0x%016" PRIx64 " mips=%u arrays=%u",
                          sid, format, surface_flags, mip_count, array_elements);
        return;
    }

    surface->mips = g_try_new0(VMSVGA3DSurfaceImage, mip_count);
    if (surface->mips == NULL) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=ALLOC_MIPS sid=%u "
                          "format=%u flags=0x%016" PRIx64 " mips=%u arrays=%u",
                          sid, format, surface_flags, mip_count, array_elements);
        g_free(surface);
        return;
    }

    surface->sid = sid;
    surface->surface_flags = surface_flags;
    surface->format = format;
    memcpy(surface->face, face, sizeof(surface->face));
    surface->multisample_count = multisample_count;
    surface->multisample_pattern =
        multisample_pattern > SVGA3D_MS_PATTERN_MAX
            ? SVGA3D_MS_PATTERN_MAX : multisample_pattern;
    surface->autogen_filter = autogen_filter;
    surface->array_elements = array_elements;
    surface->mip_count = mip_count;

    for (i = 0; i < mip_count; i++) {
        if (!vmsvga3d_surface_image_layout(format, &mip_sizes[i],
                                           multisample_count, &surface->mips[i])) {
            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "SURFACE result=REJECT reason=IMAGE_LAYOUT sid=%u format=%u "
                "flags=0x%016" PRIx64 " mip=%u size=%ux%ux%u samples=%u",
                sid, format, surface_flags, i, mip_sizes[i].width,
                mip_sizes[i].height, mip_sizes[i].depth, multisample_count);
            vmsvga3d_surface_free(surface);
            return;
        }
        storage_bytes += surface->mips[i].data_size;
        if (storage_bytes > SIZE_MAX) {
            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "SURFACE result=REJECT reason=STORAGE_OVERFLOW sid=%u "
                "format=%u flags=0x%016" PRIx64 " mip=%u bytes=%" PRIu64,
                sid, format, surface_flags, i, storage_bytes);
            vmsvga3d_surface_free(surface);
            return;
        }
    }
    surface->storage_bytes = (size_t)storage_bytes;

    state = vmsvga3d_state_ensure(s);
    if (state == NULL) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=STATE sid=%u "
                          "format=%u flags=0x%016" PRIx64 " bytes=%zu",
                          sid, format, surface_flags, surface->storage_bytes);
        vmsvga3d_surface_free(surface);
        return;
    }

    old_surface = state->surfaces[sid];
    old_bytes = old_surface != NULL ? old_surface->storage_bytes : 0;
    limit = vmsvga_surface_memory_size(s);

    if (surface->storage_bytes > limit || state->surface_bytes < old_bytes ||
        state->surface_bytes - old_bytes > limit - surface->storage_bytes) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "SURFACE result=REJECT reason=SURFACE_MEMORY sid=%u format=%u "
            "flags=0x%016" PRIx64 " bytes=%zu current=%zu old=%zu limit=%zu",
            sid, format, surface_flags, surface->storage_bytes,
            state->surface_bytes, old_bytes, limit);
        vmsvga3d_surface_free(surface);
        return;
    }

    for (i = 0; i < mip_count; i++) {
        surface->mips[i].data = g_try_malloc0(surface->mips[i].data_size);
        if (surface->mips[i].data == NULL) {
            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "SURFACE result=REJECT reason=ALLOC_IMAGE sid=%u format=%u "
                "flags=0x%016" PRIx64 " mip=%u bytes=%u",
                sid, format, surface_flags, i, surface->mips[i].data_size);
            vmsvga3d_surface_free(surface);
            return;
        }
    }

    surface->dxvk_surface = vmsvga3d_dxvk_surface_create(s->dxvk, sid);
    if (surface->dxvk_surface == NULL) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "SURFACE result=REJECT reason=DXVK_SURFACE_CREATE sid=%u "
            "format=%u flags=0x%016" PRIx64 " bytes=%zu",
            sid, format, surface_flags, surface->storage_bytes);
        vmsvga3d_surface_free(surface);
        return;
    }

    if (old_surface != NULL && sid == state->active_screen_target_sid &&
        !vmsvga3d_screen_target_quiesce_live(s)) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=SCREEN_TARGET_PENDING "
                          "sid=%u",
                          sid);
        vmsvga3d_surface_free(surface);
        return;
    }

    state->surface_bytes -= old_bytes;
    if (old_surface != NULL) {
        /* VBox redefinition goes through SurfaceDestroy, which also removes the
         * old SID from every legacy context's active texture and render-target
         * state before the backend surface is destroyed. */
        vmsvga3d_surface_clear_legacy_bindings(state, sid);
    }

    if (old_surface != NULL && old_surface->dxvk_surface != NULL) {
        /* VBox redefinition goes through SurfaceDestroy(..., true), so realized
         * DX view COTable entries are cleared before this SID is reused. */
        vmsvga3d_dxvk_d3d11_surface_visit_views(
            old_surface->dxvk_surface, vmsvga3d_surface_destroy_view_live, s);
    }

    vmsvga3d_surface_free(old_surface);
    state->surfaces[sid] = surface;
    state->surface_bytes += surface->storage_bytes;

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "SURFACE result=OK sid=%u format=%u flags=0x%016" PRIx64 " mips=%u "
        "arrays=%u size=%ux%ux%u samples=%u bytes=%zu total=%zu redefined=%u",
        sid, format, surface_flags, mip_count, array_elements,
        mip_count != 0 ? mip_sizes[0].width : 0,
        mip_count != 0 ? mip_sizes[0].height : 0,
        mip_count != 0 ? mip_sizes[0].depth : 0, multisample_count,
        surface->storage_bytes, state->surface_bytes, old_surface != NULL);
}

static bool vmsvga3d_gb_surface_define_live(
    struct vmsvga_state_s *s, uint32_t sid,
    SVGA3dSurfaceAllFlags surface_flags, SVGA3dSurfaceFormat format,
    uint32_t num_mip_levels, uint32_t multisample_count,
    SVGA3dMSPattern multisample_pattern, uint32_t multisample_quality,
    SVGA3dTextureFilter autogen_filter, const SVGA3dSize *base_size,
    uint32_t array_size, uint32_t buffer_byte_stride)
{
    SVGAOTableSurfaceEntry entry;
    SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES] = {{0}};
    SVGA3dSize *mip_sizes;
    uint32_t array_elements;
    uint32_t mip_count;
    uint32_t array_index;
    uint32_t mip_index;

    if (s == NULL || base_size == NULL) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "GB-SURFACE result=REJECT reason=ARGS sid=%u", sid);
        return false;
    }

    if ((surface_flags & SVGA3D_SURFACE_SCREENTARGET) != 0 &&
        !s->screen_defined && s->screen_preseed_base == NULL) {
        (void)vmsvga_screen_preseed_capture(s, qemu_console_surface(s->vga.con),
                                            "gb-screentarget-surface");
    }

    multisample_quality = MIN(MAX(multisample_quality,
                                  (uint32_t)SVGA3D_MS_QUALITY_MIN),
                                  (uint32_t)SVGA3D_MS_QUALITY_MAX);

    memset(&entry, 0, sizeof(entry));
 
    entry.format = cpu_to_le32(format);
    entry.surface1Flags = cpu_to_le32((uint32_t)surface_flags);
    entry.numMipLevels = cpu_to_le32(num_mip_levels);
    entry.multisampleCount = cpu_to_le32(multisample_count);
    entry.autogenFilter = cpu_to_le32(autogen_filter);
    entry.size.width = cpu_to_le32(base_size->width);
    entry.size.height = cpu_to_le32(base_size->height);
    entry.size.depth = cpu_to_le32(base_size->depth);
    entry.mobid = cpu_to_le32(SVGA3D_INVALID_ID);
    entry.arraySize = cpu_to_le32(array_size);
    entry.surface2Flags = cpu_to_le32((uint32_t)(surface_flags >> 32));
    entry.multisamplePattern = (uint8_t)multisample_pattern;
    entry.qualityLevel = (uint8_t)multisample_quality;
    entry.bufferByteStride = cpu_to_le16((uint16_t)buffer_byte_stride);

    if (!vmsvga3d_otable_write(s, SVGA_OTABLE_SURFACE, sid, sizeof(entry),
                                &entry, sizeof(entry))) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-SURFACE result=REJECT reason=OTABLE sid=%u format=%u "
            "flags=0x%016" PRIx64 " mips=%u arrays=%u size=%ux%ux%u",
            sid, format, surface_flags, num_mip_levels, array_size,
            base_size->width, base_size->height, base_size->depth);
        return false;
    }

    if (sid >= SVGA3D_MAX_SURFACE_IDS || num_mip_levels == 0 ||
        num_mip_levels > VMSVGA3D_MAX_MIP_LEVELS ||
        array_size > SVGA3D_MAX_SURFACE_ARRAYSIZE) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-SURFACE result=REJECT reason=RANGE sid=%u format=%u "
            "mips=%u arrays=%u",
            sid, format, num_mip_levels, array_size);
        return false;
    }

    array_elements = array_size != 0
                         ? array_size
                         : ((surface_flags & SVGA3D_SURFACE_CUBEMAP) != 0
                                ? SVGA3D_MAX_SURFACE_FACES
                                : 1u);
    if (array_elements == 0 ||
        ((surface_flags & SVGA3D_SURFACE_CUBEMAP) != 0 &&
         array_elements % SVGA3D_MAX_SURFACE_FACES != 0) ||
        num_mip_levels > UINT32_MAX / array_elements) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-SURFACE result=REJECT reason=ARRAY sid=%u flags=0x%016" PRIx64
            " mips=%u arrays=%u elements=%u",
            sid, surface_flags, num_mip_levels, array_size, array_elements);
        return false;
    }
    mip_count = num_mip_levels * array_elements;

    mip_sizes = g_try_new(SVGA3dSize, mip_count);

    if (mip_sizes == NULL) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-SURFACE result=REJECT reason=ALLOC_MIPS sid=%u mips=%u",
            sid, mip_count);
        return false;
    }

    for (array_index = 0; array_index < array_elements; array_index++) {
        for (mip_index = 0; mip_index < num_mip_levels; mip_index++) {
            mip_sizes[array_index * num_mip_levels + mip_index] =
                svga3dsurface_get_mip_size(*base_size, mip_index);
        }
    }

    face[0].numMipLevels = num_mip_levels;
    if (surface_flags & SVGA3D_SURFACE_CUBEMAP) {
        for (array_index = 1; array_index < SVGA3D_MAX_SURFACE_FACES;
             array_index++) {
            face[array_index].numMipLevels = num_mip_levels;
        }
    }

    vmsvga3d_surface_install(s, sid, surface_flags, format, face,
                             multisample_count, multisample_pattern,
                             autogen_filter, array_elements,
                             mip_sizes, mip_count);
    if (sid < SVGA3D_MAX_SURFACE_IDS && s->svga3d->surfaces[sid] != NULL) {
        s->svga3d->surfaces[sid]->multisample_quality = multisample_quality;
        s->svga3d->surfaces[sid]->buffer_byte_stride =
            (uint16_t)buffer_byte_stride;
    }
    g_free(mip_sizes);

    return true;
}

static void vmsvga3d_surface_destroy_view_live(
    void *opaque, VMSVGA3DDxvkViewKind kind, uint32_t cid, uint32_t view_id)
{
    struct vmsvga_state_s *s = opaque;
    VMSVGA3DDXContext *context;
    VMSVGA3DDXCOTable *binding;
    SVGACOTableType type;
    uint32_t expected_entry_size;
    uint32_t offset;

    if (s == NULL || s->svga3d == NULL || cid >= SVGA3D_MAX_CONTEXT_IDS) {
        return;
    }

    context = s->svga3d->dx_contexts[cid];
    if (context == NULL) {
        return;
    }

    switch (kind) {
    case VMSVGA3D_DXVK_VIEW_SHADER_RESOURCE:
        type = SVGA_COTABLE_SRVIEW;
        expected_entry_size = sizeof(SVGACOTableDXSRViewEntry);
        break;
    case VMSVGA3D_DXVK_VIEW_RENDER_TARGET:
        type = SVGA_COTABLE_RTVIEW;
        expected_entry_size = sizeof(SVGACOTableDXRTViewEntry);
        break;
    case VMSVGA3D_DXVK_VIEW_DEPTH_STENCIL:
        type = SVGA_COTABLE_DSVIEW;
        expected_entry_size = sizeof(SVGACOTableDXDSViewEntry);
        break;
    case VMSVGA3D_DXVK_VIEW_UNORDERED_ACCESS:
        type = SVGA_COTABLE_UAVIEW;
        expected_entry_size = sizeof(SVGACOTableDXUAViewEntry);
        break;
    default:
        return;
    }

    binding = &context->cotables[type];
    if (view_id >= binding->capacity_entries || binding->host == NULL ||
        binding->entry_size != expected_entry_size) {
        return;
    }
    {
        uint64_t offset64 = (uint64_t)view_id * (uint64_t)binding->entry_size;

        if (offset64 > binding->host_size) {
            return;
        }
        offset = (uint32_t)offset64;
    }
    if (binding->entry_size > binding->host_size - offset) {
        return;
    }

    /* VBox clears only COTable entries for views that were actually realized
     * on this backend surface.  Unrealized definitions survive SURFACE_DESTROY.
     */
    memset(binding->host + offset, 0, binding->entry_size);
}

static void vmsvga3d_surface_destroy_live(struct vmsvga_state_s *s,
                                           uint32_t sid)
{
    struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;
    VMSVGA3DSurface *surface;

    if (state == NULL || sid >= SVGA3D_MAX_SURFACE_IDS) {
        return;
    }

    surface = state->surfaces[sid];
    if (surface == NULL) {
        return;
    }

    if (sid == state->active_screen_target_sid &&
        !vmsvga3d_screen_target_quiesce_live(s)) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE_DESTROY result=REJECT "
                          "reason=SCREEN_TARGET_PENDING sid=%u",
                          sid);
        return;
    }

    if (state->surface_bytes >= surface->storage_bytes) {
        state->surface_bytes -= surface->storage_bytes;
    } else {
        state->surface_bytes = 0;
    }

    /* Match VBox SurfaceDestroy: remove the SID from every legacy context's
     * active texture and render-target state before destroying the backend
     * surface. */
    vmsvga3d_surface_clear_legacy_bindings(state, sid);

    if (surface->dxvk_surface != NULL) {
        vmsvga3d_dxvk_d3d11_surface_visit_views(
            surface->dxvk_surface, vmsvga3d_surface_destroy_view_live, s);
    }

    vmsvga3d_surface_free(surface);
    state->surfaces[sid] = NULL;
}

static bool vmsvga3d_handle_surface_define(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start)
{
    SVGA3dCmdDefineSurface *body;
    SVGA3dSize *mip_sizes;
    void *payload;
    uint32_t size;
    uint32_t mip_bytes;
    uint32_t mip_count;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=PAYLOAD_READ "
                          "fifo=0x%08x",
                          fifo_start);
        return true;
    }

    if (size < sizeof(*body)) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=PACKET_SHORT "
                          "fifo=0x%08x bytes=%u expected=%zu",
                          fifo_start, size, sizeof(*body));
        g_free(payload);
        return true;
    }

    mip_bytes = size - sizeof(*body);
    if (mip_bytes % sizeof(SVGA3dSize) != 0) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=MIP_PAYLOAD "
                          "fifo=0x%08x bytes=%u mip_bytes=%u",
                          fifo_start, size, mip_bytes);
        g_free(payload);
        return true;
    }

    body = payload;
    mip_sizes = (SVGA3dSize *)(body + 1);
    mip_count = mip_bytes / sizeof(SVGA3dSize);
    vmsvga3d_surface_install(s, body->sid, body->surfaceFlags, body->format,
                             body->face, 0, SVGA3D_MS_PATTERN_NONE, 0,
                             (body->surfaceFlags & SVGA3D_SURFACE_CUBEMAP)
                                 ? SVGA3D_MAX_SURFACE_FACES
                                 : 1u,
                             mip_sizes, mip_count);

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_surface_define_v2(struct vmsvga_state_s *s,
                                              uint32_t cmd, int32_t *len,
                                              uint32_t fifo_start)
{
    SVGA3dCmdDefineSurface_v2 *body;
    SVGA3dSize *mip_sizes;
    void *payload;
    uint32_t size;
    uint32_t mip_bytes;
    uint32_t mip_count;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=PAYLOAD_READ_V2 "
                          "fifo=0x%08x",
                          fifo_start);
        return true;
    }

    if (size < sizeof(*body)) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=PACKET_SHORT_V2 "
                          "fifo=0x%08x bytes=%u expected=%zu",
                          fifo_start, size, sizeof(*body));
        g_free(payload);
        return true;
    }

    mip_bytes = size - sizeof(*body);
    if (mip_bytes % sizeof(SVGA3dSize) != 0) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "SURFACE result=REJECT reason=MIP_PAYLOAD_V2 "
                          "fifo=0x%08x bytes=%u mip_bytes=%u",
                          fifo_start, size, mip_bytes);
        g_free(payload);
        return true;
    }

    body = payload;
    mip_sizes = (SVGA3dSize *)(body + 1);
    mip_count = mip_bytes / sizeof(SVGA3dSize);

    vmsvga3d_surface_install(s, body->sid, body->surfaceFlags, body->format,
                             body->face, body->multisampleCount,
                             body->multisampleCount > 1
                                 ? SVGA3D_MS_PATTERN_STANDARD
                                 : SVGA3D_MS_PATTERN_NONE,
                             body->autogenFilter,
                             (body->surfaceFlags & SVGA3D_SURFACE_CUBEMAP)
                                 ? SVGA3D_MAX_SURFACE_FACES
                                 : 1u,
                             mip_sizes, mip_count);

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_surface_destroy(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start)
{
    SVGA3dCmdDestroySurface *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        vmsvga3d_surface_destroy_live(s, body->sid);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_context_define(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start)
{
    SVGA3dCmdDefineContext *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        if (vmsvga3d_state_context_define(s, body->cid)) {
            vmsvga3d_gb_query_cancel_context(s, body->cid, "context-define");
            vmsvga3d_dxvk_d3d9_query_context_destroy(s->dxvk, body->cid);
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_context_destroy(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start)
{
    SVGA3dCmdDestroyContext *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        if (vmsvga3d_state_context_destroy(s, body->cid)) {
            vmsvga3d_gb_query_cancel_context(s, body->cid, "context-destroy");
            vmsvga3d_dxvk_d3d9_query_context_destroy(s->dxvk, body->cid);
        }
    }

    g_free(payload);
    return true;
}


static bool vmsvga3d_surface_image(VMSVGA3DSurface *surface,
                                   const SVGA3dSurfaceImageId *image_id,
                                   VMSVGA3DSurfaceImage **image);

static VMSVGA3DContext *vmsvga3d_context(struct vmsvga_state_s *s,
                                         uint32_t cid)
{
    if (s->svga3d == NULL || cid >= SVGA3D_MAX_CONTEXT_IDS) {
        return NULL;
    }

    return s->svga3d->contexts[cid];
}

static VMSVGA3DDXContext *vmsvga3d_dx_context(struct vmsvga_state_s *s,
                                               uint32_t cid)
{
    if (s->svga3d == NULL || cid >= SVGA3D_MAX_CONTEXT_IDS) {
        return NULL;
    }

    return s->svga3d->dx_contexts[cid];
}

typedef enum VMSVGA3DTraceVgpu9WriteKind {
    VMSVGA3D_TRACE_VGPU9_WRITE_NONE = 0,
    VMSVGA3D_TRACE_VGPU9_WRITE_DRAW,
    VMSVGA3D_TRACE_VGPU9_WRITE_COPY,
    VMSVGA3D_TRACE_VGPU9_WRITE_DMA,
    VMSVGA3D_TRACE_VGPU9_WRITE_CLEAR,
    VMSVGA3D_TRACE_VGPU9_WRITE_STRETCH,
} VMSVGA3DTraceVgpu9WriteKind;

static const char *vmsvga3d_trace_vgpu9_write_name(uint32_t kind)
{
    switch ((VMSVGA3DTraceVgpu9WriteKind)kind) {
    case VMSVGA3D_TRACE_VGPU9_WRITE_DRAW:
        return "draw";
    case VMSVGA3D_TRACE_VGPU9_WRITE_COPY:
        return "copy";
    case VMSVGA3D_TRACE_VGPU9_WRITE_DMA:
        return "dma";
    case VMSVGA3D_TRACE_VGPU9_WRITE_CLEAR:
        return "clear";
    case VMSVGA3D_TRACE_VGPU9_WRITE_STRETCH:
        return "stretch";
    case VMSVGA3D_TRACE_VGPU9_WRITE_NONE:
    default:
        return "none";
    }
}

static bool vmsvga3d_trace_vgpu9_enabled(struct vmsvga_state_s *s)
{
    return s != NULL && s->vgpu_generation == VMSVGA_VGPU_9 &&
           VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D);
}

static bool vmsvga3d_trace_vgpu9_full_surface(struct vmsvga_state_s *s,
                                               VMSVGA3DSurface *surface)
{
    uint32_t width;
    uint32_t height;

    if (s == NULL || surface == NULL || surface->mips == NULL ||
        surface->mip_count == 0 || !s->active_valid) {
        return false;
    }

    width = s->screen_defined ? s->screen_width : s->active_width;
    height = s->screen_defined ? s->screen_height : s->active_height;
    return surface->mips[0].size.width == width &&
           surface->mips[0].size.height == height;
}

static void vmsvga3d_trace_vgpu9_surface_write(
    struct vmsvga_state_s *s, uint32_t sid, VMSVGA3DTraceVgpu9WriteKind kind,
    uint32_t cid, bool full_copy)
{
    VMSVGA3DSurface *surface;

    if (!vmsvga3d_trace_vgpu9_enabled(s) || s->svga3d == NULL ||
        sid >= SVGA3D_MAX_SURFACE_IDS) {
        return;
    }

    surface = s->svga3d->surfaces[sid];
    if (surface == NULL) {
        return;
    }

    surface->trace_vgpu9_write_count++;
    surface->trace_vgpu9_last_write_3d_cmd = s->trace_now.fifo_3d_cmds;
    surface->trace_vgpu9_last_write_kind = kind;
    surface->trace_vgpu9_last_write_cid = cid;

    switch (kind) {
    case VMSVGA3D_TRACE_VGPU9_WRITE_DRAW:
        surface->trace_vgpu9_draw_count++;
        break;
    case VMSVGA3D_TRACE_VGPU9_WRITE_COPY:
        surface->trace_vgpu9_copy_count++;
        if (full_copy) {
            surface->trace_vgpu9_full_copy_count++;
        }
        break;
    case VMSVGA3D_TRACE_VGPU9_WRITE_DMA:
        surface->trace_vgpu9_dma_write_count++;
        break;
    case VMSVGA3D_TRACE_VGPU9_WRITE_CLEAR:
        surface->trace_vgpu9_clear_count++;
        break;
    case VMSVGA3D_TRACE_VGPU9_WRITE_STRETCH:
        surface->trace_vgpu9_stretch_count++;
        break;
    case VMSVGA3D_TRACE_VGPU9_WRITE_NONE:
    default:
        break;
    }
}

static void vmsvga3d_trace_vgpu9_render_target_write(
    struct vmsvga_state_s *s, uint32_t cid, VMSVGA3DTraceVgpu9WriteKind kind)
{
    VMSVGA3DContext *context;
    uint32_t type;

    if (!vmsvga3d_trace_vgpu9_enabled(s)) {
        return;
    }

    context = vmsvga3d_context(s, cid);
    if (context == NULL) {
        return;
    }

    for (type = SVGA3D_RT_COLOR0; type <= SVGA3D_RT_COLOR3; type++) {
        uint32_t sid = context->render_targets[type].sid;

        if (sid != SVGA3D_INVALID_ID) {
            vmsvga3d_trace_vgpu9_surface_write(s, sid, kind, cid, false);
        }
    }
}

static void vmsvga3d_trace_vgpu9_present(struct vmsvga_state_s *s,
                                          uint32_t sid,
                                          const char *present_kind)
{
    struct vmsvga3d_state_s *state;
    VMSVGA3DSurface *surface;
    uint64_t pending_writes;
    uint64_t write_age;

    if (!vmsvga3d_trace_vgpu9_enabled(s) || s->svga3d == NULL ||
        sid >= SVGA3D_MAX_SURFACE_IDS) {
        return;
    }

    state = s->svga3d;
    surface = state->surfaces[sid];
    if (surface == NULL) {
        return;
    }

    pending_writes = surface->trace_vgpu9_write_count -
                     surface->trace_vgpu9_last_present_write_count;
    write_age = surface->trace_vgpu9_write_count == 0
                    ? 0
                    : s->trace_now.fifo_3d_cmds -
                          surface->trace_vgpu9_last_write_3d_cmd;

    state->trace_vgpu9_present_seq++;
    surface->trace_vgpu9_present_count++;
    fprintf(stderr,
            "VMVGA-VGPU9-PRESENT-LINK seq=%" PRIu64 " kind=%s sid=%u "
            "full=%u active=%u writes=%" PRIu64 " pending=%" PRIu64
            " draws=%" PRIu64 " copies=%" PRIu64 " fullcopies=%" PRIu64
            " dma=%" PRIu64 " clears=%" PRIu64 " stretch=%" PRIu64
            " last=%s cid=%u write-age-3d=%" PRIu64 "\n",
            state->trace_vgpu9_present_seq, present_kind, sid,
            vmsvga3d_trace_vgpu9_full_surface(s, surface),
            surface->legacy_active, surface->trace_vgpu9_write_count,
            pending_writes, surface->trace_vgpu9_draw_count,
            surface->trace_vgpu9_copy_count,
            surface->trace_vgpu9_full_copy_count,
            surface->trace_vgpu9_dma_write_count,
            surface->trace_vgpu9_clear_count,
            surface->trace_vgpu9_stretch_count,
            vmsvga3d_trace_vgpu9_write_name(surface->trace_vgpu9_last_write_kind),
            surface->trace_vgpu9_last_write_cid, write_age);

    surface->trace_vgpu9_last_present_write_count =
        surface->trace_vgpu9_write_count;
    surface->trace_vgpu9_last_present_3d_cmd = s->trace_now.fifo_3d_cmds;
    state->trace_vgpu9_last_present_sid = sid;
    state->trace_vgpu9_last_present_3d_cmd = s->trace_now.fifo_3d_cmds;
}

static void vmsvga3d_trace_vgpu9_frame_state(struct vmsvga_state_s *s,
                                               const char *reason)
{
    enum { RECENT_MAX = 6 };
    struct vmsvga3d_state_s *state;
    VMSVGA3DSurface *recent[RECENT_MAX] = { 0 };
    uint32_t tracked = 0;
    uint32_t pending_surfaces = 0;
    uint32_t full_surfaces = 0;
    uint32_t sid;
    uint32_t i;
    uint64_t last_present_age;

    if (!vmsvga3d_trace_vgpu9_enabled(s) || s->svga3d == NULL) {
        return;
    }

    state = s->svga3d;
    for (sid = 0; sid < SVGA3D_MAX_SURFACE_IDS; sid++) {
        VMSVGA3DSurface *surface = state->surfaces[sid];
        uint64_t pending;

        if (surface == NULL ||
            (surface->trace_vgpu9_write_count == 0 &&
             surface->trace_vgpu9_present_count == 0)) {
            continue;
        }

        tracked++;
        pending = surface->trace_vgpu9_write_count -
                  surface->trace_vgpu9_last_present_write_count;
        if (pending != 0) {
            pending_surfaces++;
        }
        if (vmsvga3d_trace_vgpu9_full_surface(s, surface)) {
            full_surfaces++;
        }

        for (i = 0; i < RECENT_MAX; i++) {
            if (recent[i] == NULL ||
                surface->trace_vgpu9_last_write_3d_cmd >
                    recent[i]->trace_vgpu9_last_write_3d_cmd) {
                uint32_t j;

                for (j = RECENT_MAX - 1; j > i; j--) {
                    recent[j] = recent[j - 1];
                }
                recent[i] = surface;
                break;
            }
        }
    }

    last_present_age = state->trace_vgpu9_present_seq == 0
                           ? 0
                           : s->trace_now.fifo_3d_cmds -
                                 state->trace_vgpu9_last_present_3d_cmd;
    fprintf(stderr,
            "VMVGA-VGPU9-FRAMESTATE reason=%s 3d=%" PRIu64
            " presents=%" PRIu64 " last-sid=%u last-present-age-3d=%" PRIu64
            " tracked=%u full=%u pending-surfaces=%u\n",
            reason != NULL ? reason : "unknown", s->trace_now.fifo_3d_cmds,
            state->trace_vgpu9_present_seq,
            state->trace_vgpu9_last_present_sid, last_present_age, tracked,
            full_surfaces, pending_surfaces);

    for (i = 0; i < RECENT_MAX && recent[i] != NULL; i++) {
        VMSVGA3DSurface *surface = recent[i];
        uint64_t pending = surface->trace_vgpu9_write_count -
                           surface->trace_vgpu9_last_present_write_count;
        uint64_t write_age = s->trace_now.fifo_3d_cmds -
                             surface->trace_vgpu9_last_write_3d_cmd;
        uint32_t width = surface->mips != NULL && surface->mip_count != 0
                             ? surface->mips[0].size.width
                             : 0;
        uint32_t height = surface->mips != NULL && surface->mip_count != 0
                              ? surface->mips[0].size.height
                              : 0;

        fprintf(stderr,
                "VMVGA-VGPU9-SURFACE rank=%u sid=%u size=%ux%u full=%u "
                "active=%u writes=%" PRIu64 " pending=%" PRIu64
                " presents=%" PRIu64 " last=%s cid=%u age-3d=%" PRIu64
                " draws=%" PRIu64 " copies=%" PRIu64 " fullcopies=%" PRIu64
                " dma=%" PRIu64 " clears=%" PRIu64 " stretch=%" PRIu64
                "\n",
                i, surface->sid, width, height,
                vmsvga3d_trace_vgpu9_full_surface(s, surface),
                surface->legacy_active, surface->trace_vgpu9_write_count,
                pending, surface->trace_vgpu9_present_count,
                vmsvga3d_trace_vgpu9_write_name(
                    surface->trace_vgpu9_last_write_kind),
                surface->trace_vgpu9_last_write_cid, write_age,
                surface->trace_vgpu9_draw_count,
                surface->trace_vgpu9_copy_count,
                surface->trace_vgpu9_full_copy_count,
                surface->trace_vgpu9_dma_write_count,
                surface->trace_vgpu9_clear_count,
                surface->trace_vgpu9_stretch_count);
    }
}


static bool vmsvga3d_handle_set_vertex_decls(struct vmsvga_state_s *s,
                                              uint32_t cmd, int32_t *len,
                                              uint32_t fifo_start)
{
    SVGA3dCmdSetVertexDecls *body;
    VMSVGA3DContext *context;
    void *payload;
    uint32_t size;
    uint32_t count;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        count = body->numElements;

        if (count <= SVGA3D_MAX_VERTEX_ARRAYS &&
            size == sizeof(*body) + count * sizeof(SVGA3dVertexElement)) {
            context = vmsvga3d_context(s, body->cid);
            if (context != NULL) {
                memset(context->vertex_decls, 0, sizeof(context->vertex_decls));
                memcpy(context->vertex_decls, body + 1,
                       count * sizeof(SVGA3dVertexElement));
                context->num_vertex_decls = count;
            }
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_vertex_streams(struct vmsvga_state_s *s,
                                                uint32_t cmd, int32_t *len,
                                                uint32_t fifo_start)
{
    SVGA3dCmdSetVertexStreams *body;
    VMSVGA3DContext *context;
    SVGA3dVertexStream *streams;
    void *payload;
    uint32_t size;
    uint32_t count;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        count = body->numStreams;

        if (count <= SVGA3D_MAX_VERTEX_ARRAYS &&
            size == sizeof(*body) + count * sizeof(SVGA3dVertexStream)) {
            context = vmsvga3d_context(s, body->cid);
            if (context != NULL) {
                streams = (SVGA3dVertexStream *)(body + 1);
                memset(context->vertex_streams, 0,
                       sizeof(context->vertex_streams));
                memcpy(context->vertex_streams, streams,
                       count * sizeof(SVGA3dVertexStream));
                context->num_vertex_streams = count;
            }
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_vertex_divisors(struct vmsvga_state_s *s,
                                                 uint32_t cmd, int32_t *len,
                                                 uint32_t fifo_start)
{
    SVGA3dCmdSetVertexDivisors *body;
    VMSVGA3DContext *context;
    void *payload;
    uint32_t size;
    uint32_t count;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        count = body->numDivisors;

        if (count <= SVGA3D_MAX_VERTEX_ARRAYS &&
            size == sizeof(*body) + count * sizeof(SVGA3dVertexDivisor)) {
            context = vmsvga3d_context(s, body->cid);
            if (context != NULL) {
                memset(context->vertex_divisors, 0,
                       sizeof(context->vertex_divisors));
                memcpy(context->vertex_divisors, body + 1,
                       count * sizeof(SVGA3dVertexDivisor));
                context->num_vertex_divisors = count;
            }
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_transform(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start)
{
    SVGA3dCmdSetTransform *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }
    if (size == sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_set_transform(s, body->cid, body->type, body->matrix);
    }
    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_z_range(struct vmsvga_state_s *s,
                                         uint32_t cmd, int32_t *len,
                                         uint32_t fifo_start)
{
    SVGA3dCmdSetZRange *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_set_z_range(s, body->cid, &body->zRange);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_material(struct vmsvga_state_s *s,
                                          uint32_t cmd, int32_t *len,
                                          uint32_t fifo_start)
{
    SVGA3dCmdSetMaterial *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_set_material(s, body->cid, body->face, &body->material);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_light_data(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start)
{
    SVGA3dCmdSetLightData *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_set_light_data(s, body->cid, body->index, &body->data);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_light_enabled(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start)
{
    SVGA3dCmdSetLightEnabled *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_set_light_enabled(s, body->cid, body->index,
                                              body->enabled);
    }

    g_free(payload);
    return true;
}

static const char *vmsvga3d_trace_render_state_name(uint32_t state);

static bool vmsvga3d_handle_set_clip_plane(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start)
{
    SVGA3dCmdSetClipPlane *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_set_clip_plane(s, body->cid, body->index, body->plane);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_render_state(struct vmsvga_state_s *s,
                                              uint32_t cmd, int32_t *len,
                                              uint32_t fifo_start)
{
    SVGA3dCmdSetRenderState *body;
    SVGA3dRenderState *states;
    void *payload;
    uint32_t size;
    uint32_t count;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size < sizeof(*body) ||
        (size - sizeof(*body)) % sizeof(*states) != 0) {
        g_free(payload);
        return true;
    }

    body = payload;
    states = (SVGA3dRenderState *)(body + 1);
    count = (size - sizeof(*body)) / sizeof(*states);
    if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
        uint32_t i;

        for (i = 0; i < count; i++) {
            const char *name = vmsvga3d_trace_render_state_name(states[i].state);

            if (name != NULL) {
                VMVGA_TRACE_LOCAL(
                    VMVGA_TRACE_3D,
                    "D3D9-STATE phase=set cid=%u state=%s(%u) value=0x%08x",
                    body->cid, name, states[i].state, states[i].uintValue);
            }
        }
    }
    (void)vmsvga3d_state_set_render_state(s, body->cid, count, states);
    g_free(payload);

    return true;
}

static bool vmsvga3d_handle_set_texture_state(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start)
{
    SVGA3dCmdSetTextureState *body;
    SVGA3dTextureState *states;
    void *payload;
    uint32_t size;
    uint32_t count;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size < sizeof(*body) ||
        (size - sizeof(*body)) % sizeof(*states) != 0) {
        g_free(payload);
        return true;
    }

    body = payload;
    states = (SVGA3dTextureState *)(body + 1);
    count = (size - sizeof(*body)) / sizeof(*states);

    (void)vmsvga3d_state_set_texture_state(s, body->cid, count, states);

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_render_target(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start)
{
    SVGA3dCmdSetRenderTarget *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_set_render_target(s, body->cid, body->type,
                                             &body->target);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_viewport(struct vmsvga_state_s *s,
                                          uint32_t cmd, int32_t *len,
                                          uint32_t fifo_start)
{
    SVGA3dCmdSetViewport *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_set_viewport(s, body->cid, &body->rect);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_scissor(struct vmsvga_state_s *s,
                                         uint32_t cmd, int32_t *len,
                                         uint32_t fifo_start)
{
    SVGA3dCmdSetScissorRect *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_set_scissor(s, body->cid, &body->rect);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_generate_mipmaps(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start)
{
    SVGA3dCmdGenerateMipmaps *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        VMSVGA3DSurface *surface = NULL;

        body = payload;
        if (s->svga3d != NULL && body->sid < SVGA3D_MAX_SURFACE_IDS) {
            surface = s->svga3d->surfaces[body->sid];
        }

        /* VirtualBox's D3D11 VGPU9 backend accepts GENERATE_MIPMAPS but does
         * nothing.  Do not mutate our CPU/D3D9 shadow once D3D11 owns it. */
        if (surface != NULL &&
            vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface)) {
            g_free(payload);
            return true;
        }

        if (vmsvga3d_state_generate_mipmaps(s, body->sid, body->filter) &&
            surface != NULL) {
            (void)vmsvga3d_d3d9_runtime_generate_mipmaps(
                s, surface, body->filter);
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_query_write_result(struct vmsvga_state_s *s,
                                          const SVGAGuestPtr *guest_result,
                                          SVGA3dQueryState state,
                                          uint32_t result)
{
    SVGA3dQueryResult query_result = {
          .totalSize = sizeof(SVGA3dQueryResult),
          .state = state,
          .result32 = result,
    };

    if (guest_result == NULL) {
        return false;
    }

    return vmsvga_gmr_write(s, guest_result->gmrId, guest_result->offset,
                            &query_result, sizeof(query_result));
}

static bool vmsvga3d_handle_begin_query(struct vmsvga_state_s *s,
                                        uint32_t cmd, int32_t *len,
                                        uint32_t fifo_start)
{
    SVGA3dCmdBeginQuery *body;
    void *payload;
    uint32_t size;

    (void)cmd;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        VMSVGA3DD3D9QueryPlan plan;
        VMSVGA3DContext *context;

        body = payload;
        context = vmsvga3d_context(s, body->cid);
        if (context != NULL && vmsvga3d_d3d9_query_plan(body->type, &plan)) {
            if (vmsvga3d_dxvk_d3d9_query_begin(
                    s->dxvk, body->cid, plan.query_type, plan.issue_begin)) {
                (void)vmsvga3d_state_query_begin(s, body->cid, body->type);
            } else {
                context->occlusion.defined = false;
            }
        }
        if (vmsvga_trace_flight_enabled()) {
            fprintf(stderr,
                    "VMVGA-QUERY phase=begin cid=%u type=%u context=%u "
                    "defined=%u\n",
                    body->cid, body->type, context != NULL,
                    context != NULL && context->occlusion.defined);
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_end_query(struct vmsvga_state_s *s,
                                      uint32_t cmd, int32_t *len,
                                      uint32_t fifo_start)
{
    SVGA3dCmdEndQuery *body;
    void *payload;
    uint32_t size;

    (void)cmd;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        VMSVGA3DD3D9QueryPlan plan;
        VMSVGA3DContext *context;

        body = payload;
        context = vmsvga3d_context(s, body->cid);
        if (context != NULL && context->occlusion.defined &&
            vmsvga3d_d3d9_query_plan(body->type, &plan)) {
            if (vmsvga3d_dxvk_d3d9_query_end(s->dxvk, body->cid,
                                              plan.issue_end)) {
                (void)vmsvga3d_state_query_end(s, body->cid, body->type);
            } else {
                context->occlusion.defined = false;
            }
        }
        if (vmsvga_trace_flight_enabled()) {
            fprintf(stderr,
                    "VMVGA-QUERY phase=end cid=%u type=%u context=%u "
                    "defined=%u guest=%u:0x%08x\n",
                    body->cid, body->type, context != NULL,
                    context != NULL && context->occlusion.defined,
                    body->guestResult.gmrId, body->guestResult.offset);
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_wait_for_query(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start)
{
    SVGA3dCmdWaitForQuery *body;
    VMSVGA3DQueryWaitStatus status;
    void *payload;
    uint32_t size;
    uint32_t result = 0;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        status = vmsvga3d_state_query_wait(s, body->cid, body->type, &result);
        if (status == VMSVGA3D_QUERY_WAIT_NEEDS_RENDERER) {
            VMSVGA3DD3D9QueryPlan plan;

            if (vmsvga3d_d3d9_query_plan(body->type, &plan) &&
                plan.wait_until_ready &&
                vmsvga3d_dxvk_d3d9_query_get_data(
                    s->dxvk, body->cid, plan.result_size, plan.getdata_flags,
                    &result) &&
                vmsvga3d_state_query_complete(s, body->cid, body->type, result)) {
                status = vmsvga3d_state_query_wait(s, body->cid, body->type,
                                                   &result);
            } else {
                VMSVGA3DContext *context = vmsvga3d_context(s, body->cid);

                if (context != NULL) {
                    context->occlusion.defined = false;
                }
                status = VMSVGA3D_QUERY_WAIT_FAILED;
            }
        }
        if (status == VMSVGA3D_QUERY_WAIT_READY) {
            (void)vmsvga3d_query_write_result(s, &body->guestResult,
                                              SVGA3D_QUERYSTATE_SUCCEEDED, result);
        } else {
            (void)vmsvga3d_query_write_result(s, &body->guestResult,
                                              SVGA3D_QUERYSTATE_FAILED, 0);
        }
        if (vmsvga_trace_flight_enabled()) {
            VMSVGA3DContext *context = vmsvga3d_context(s, body->cid);
            fprintf(stderr,
                    "VMVGA-QUERY phase=wait cid=%u type=%u status=%u "
                    "result=%u defined=%u guest=%u:0x%08x\n",
                    body->cid, body->type, (uint32_t)status, result,
                    context != NULL && context->occlusion.defined,
                    body->guestResult.gmrId, body->guestResult.offset);
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_gb_query_target_current(
    struct vmsvga_state_s *s, const VMSVGA3DGBQuery *query,
    SVGA3dQueryResult *guest_result)
{
    VMSVGA3DMob *mob;

    if (s == NULL || query == NULL || guest_result == NULL) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, query->mobid);
    if (mob == NULL || mob != query->mob ||
        !vmsvga3d_mob_read(s, mob, query->offset,
                           guest_result, sizeof(*guest_result))) {
        return false;
    }

    return guest_result->totalSize >= sizeof(*guest_result) &&
           guest_result->state == SVGA3D_QUERYSTATE_PENDING &&
           guest_result->queryCookie == query->query_cookie;
}

static bool vmsvga3d_gb_query_publish(
    struct vmsvga_state_s *s, const VMSVGA3DGBQuery *query,
    SVGA3dQueryState state, uint32_t result)
{
    SVGA3dQueryResult guest_result;
    VMSVGA3DMob *mob;

    if (!vmsvga3d_gb_query_target_current(s, query, &guest_result)) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, query->mobid);
    if (mob == NULL || mob != query->mob ||
        !vmsvga3d_mob_write(s, mob,
                            query->offset + offsetof(SVGA3dQueryResult, result32),
                            &result, sizeof(result))) {
        return false;
    }

    /* Publish the terminal state last so a polling guest cannot observe a
     * completed query before the result payload has reached its MOB. */
    return vmsvga3d_mob_write(
        s, mob, query->offset + offsetof(SVGA3dQueryResult, state),
        &state, sizeof(state));
}

static void vmsvga3d_d3d9_process_pending_gb_queries_filtered(
    struct vmsvga_state_s *s, bool filter, uint32_t cid,
    SVGA3dQueryType type, uint32_t flags, bool wait, const char *source)
{
    VMSVGA3DGBQuery **link;

    if (s == NULL || s->svga3d == NULL || s->dxvk == NULL) {
        return;
    }

    link = &s->svga3d->gb_queries;
    while (*link != NULL) {
        VMSVGA3DGBQuery *query = *link;
        VMSVGA3DD3D9GBQueryPollResult poll;
        SVGA3dQueryResult guest_result;
        uint32_t result = 0;

        if (filter && (query->cid != cid || query->type != type)) {
            link = &query->next;
            continue;
        }

        if (!vmsvga3d_gb_query_target_current(s, query, &guest_result)) {
            vmsvga3d_gb_query_unlink(s, link, true, "target-reused");
            continue;
        }

        poll = vmsvga3d_dxvk_d3d9_gb_query_poll(
            s->dxvk, query->token, sizeof(result), flags, wait, &result);
        if (poll == VMSVGA3D_D3D9_GB_QUERY_POLL_PENDING) {
            link = &query->next;
            continue;
        }

        if (poll == VMSVGA3D_D3D9_GB_QUERY_POLL_READY) {
            bool published = vmsvga3d_gb_query_publish(
                s, query, SVGA3D_QUERYSTATE_SUCCEEDED, result);

            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "GB-QUERY phase=complete cid=%u type=%u token=%" PRIu64
                " mobid=%u offset=0x%08x result=%u state=%s source=%s",
                query->cid, (uint32_t)query->type, query->token,
                query->mobid, query->offset, result,
                published ? "SUCCEEDED" : "STALE",
                source != NULL ? source : "unknown");
            vmsvga3d_gb_query_unlink(s, link, false, "complete");
            continue;
        }

        {
            bool published = vmsvga3d_gb_query_publish(
                s, query, SVGA3D_QUERYSTATE_FAILED, 0);

            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "GB-QUERY phase=complete cid=%u type=%u token=%" PRIu64
                " mobid=%u offset=0x%08x result=0 state=%s source=%s",
                query->cid, (uint32_t)query->type, query->token,
                query->mobid, query->offset,
                published ? "FAILED" : "STALE",
                source != NULL ? source : "unknown");
        }
        vmsvga3d_gb_query_unlink(s, link, false, "renderer-failed");
    }
}

static void vmsvga3d_d3d9_process_pending_gb_queries(
    struct vmsvga_state_s *s, const char *source)
{
    VMSVGA3DD3D9QueryPlan plan;

    if (!vmsvga3d_d3d9_query_plan(SVGA3D_QUERYTYPE_OCCLUSION, &plan)) {
        return;
    }

    /* D3DGETDATA_FLUSH asks D3D9 to submit outstanding work but remains
     * nonblocking: S_FALSE leaves the query on our pending list. */
    vmsvga3d_d3d9_process_pending_gb_queries_filtered(
        s, false, 0, SVGA3D_QUERYTYPE_OCCLUSION,
        plan.getdata_flags, false, source);
}

static void vmsvga3d_d3d9_schedule_gb_query_poll(
    struct vmsvga_state_s *s)
{
    struct vmsvga3d_state_s *state;

    if (s == NULL || (state = s->svga3d) == NULL ||
        state->gb_queries == NULL) {
        return;
    }

    if (state->gb_query_timer == NULL) {
        state->gb_query_timer = timer_new_ms(
            QEMU_CLOCK_VIRTUAL, vmsvga3d_d3d9_gb_query_timer_cb, s);
    }

    timer_mod(state->gb_query_timer,
              qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) +
              VMSVGA3D_GB_QUERY_POLL_INTERVAL_MS);
}

static void vmsvga3d_d3d9_gb_query_timer_cb(void *opaque)
{
    struct vmsvga_state_s *s = opaque;

    if (s == NULL || s->svga3d == NULL ||
        s->svga3d->gb_queries == NULL) {
        return;
    }

    vmsvga3d_d3d9_process_pending_gb_queries(s, "TIMER");
    vmsvga3d_d3d9_schedule_gb_query_poll(s);
}

static bool vmsvga3d_handle_gb_query(struct vmsvga_state_s *s,
                                     uint32_t cmd, int32_t *len,
                                     uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (cmd == SVGA_3D_CMD_BEGIN_GB_QUERY &&
        size >= sizeof(SVGA3dCmdBeginGBQuery)) {
        const SVGA3dCmdBeginGBQuery *body = payload;
        VMSVGA3DD3D9QueryPlan plan;
        bool ok = false;

        if (vmsvga3d_context(s, body->cid) != NULL &&
            vmsvga3d_d3d9_query_plan(body->type, &plan)) {
            ok = vmsvga3d_dxvk_d3d9_gb_query_begin(
                s->dxvk, body->cid, plan.query_type, plan.issue_begin);
        }
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-QUERY phase=begin cid=%u type=%u result=%s",
            body->cid, (uint32_t)body->type, ok ? "OK" : "FAIL");
    } else if (cmd == SVGA_3D_CMD_END_GB_QUERY &&
               size >= sizeof(SVGA3dCmdEndGBQuery)) {
        const SVGA3dCmdEndGBQuery *body = payload;
        VMSVGA3DD3D9QueryPlan plan;
        SVGA3dQueryResult guest_result = { 0 };
        VMSVGA3DMob *mob = vmsvga3d_mob_get(s, body->mobid);
        uint64_t token = 0;
        bool target_ok = mob != NULL &&
                         vmsvga3d_mob_read(s, mob, body->offset,
                                           &guest_result, sizeof(guest_result)) &&
                         guest_result.totalSize >= sizeof(guest_result) &&
                         guest_result.state == SVGA3D_QUERYSTATE_PENDING;
        bool end_ok = false;
        bool queued = false;

        if (target_ok) {
            /* A newly issued generation supersedes any older result which
             * named the same guest slot, even if the guest reused a cookie. */
            vmsvga3d_gb_query_cancel_target(
                s, body->mobid, body->offset, "target-reissued");
        }

        if (vmsvga3d_context(s, body->cid) != NULL &&
            vmsvga3d_d3d9_query_plan(body->type, &plan)) {
            end_ok = vmsvga3d_dxvk_d3d9_gb_query_end(
                s->dxvk, body->cid, plan.query_type, plan.issue_end, &token);
            if (end_ok && target_ok) {
                VMSVGA3DGBQuery *query = g_try_new0(VMSVGA3DGBQuery, 1);

                if (query != NULL) {
                    query->token = token;
                    query->cid = body->cid;
                    query->type = body->type;
                    query->mobid = body->mobid;
                    query->offset = body->offset;
                    query->query_cookie = guest_result.queryCookie;
                    query->mob = mob;
                    query->next = s->svga3d->gb_queries;
                    s->svga3d->gb_queries = query;
                    queued = true;
                }
            }
        }

        if (!queued) {
            if (target_ok) {
                uint32_t result = 0;
                SVGA3dQueryState failed = SVGA3D_QUERYSTATE_FAILED;

                (void)vmsvga3d_mob_write(
                    s, mob,
                    body->offset + offsetof(SVGA3dQueryResult, result32),
                    &result, sizeof(result));
                (void)vmsvga3d_mob_write(
                    s, mob, body->offset + offsetof(SVGA3dQueryResult, state),
                    &failed, sizeof(failed));
            }
            if (token != 0) {
                vmsvga3d_dxvk_d3d9_gb_query_cancel(s->dxvk, token);
            }
        }

        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-QUERY phase=end cid=%u type=%u token=%" PRIu64
            " mobid=%u offset=0x%08x cookie=0x%08x target=%u queued=%u result=%s",
            body->cid, (uint32_t)body->type, token, body->mobid, body->offset,
            target_ok ? guest_result.queryCookie : 0u,
            target_ok ? 1u : 0u, queued ? 1u : 0u,
            end_ok ? "OK" : "FAIL");

        if (queued) {
            /* Cheap probe only; do not force a submit from every END. */
            vmsvga3d_d3d9_process_pending_gb_queries_filtered(
                s, true, body->cid, body->type, 0, false, "END");
            vmsvga3d_d3d9_schedule_gb_query_poll(s);
        }
    } else if (cmd == SVGA_3D_CMD_WAIT_FOR_GB_QUERY &&
               size >= sizeof(SVGA3dCmdWaitForGBQuery)) {
        const SVGA3dCmdWaitForGBQuery *body = payload;
        VMSVGA3DD3D9QueryPlan plan;
        SVGA3dQueryResult guest_result = { 0 };
        VMSVGA3DMob *mob = vmsvga3d_mob_get(s, body->mobid);
        bool pending = mob != NULL &&
                       vmsvga3d_mob_read(s, mob, body->offset,
                                         &guest_result, sizeof(guest_result)) &&
                       guest_result.totalSize >= sizeof(guest_result) &&
                       guest_result.state == SVGA3D_QUERYSTATE_PENDING;

        if (pending && vmsvga3d_d3d9_query_plan(body->type, &plan)) {
            /* WAIT is a barrier for every ended query of this cid/type, not
             * only for the explicitly named result record. */
            vmsvga3d_d3d9_process_pending_gb_queries_filtered(
                s, true, body->cid, body->type,
                plan.getdata_flags, true, "WAIT");
        }
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-QUERY phase=wait cid=%u type=%u mobid=%u offset=0x%08x pending=%u",
            body->cid, (uint32_t)body->type, body->mobid, body->offset,
            pending ? 1u : 0u);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_shader_define(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start)
{
    SVGA3dCmdDefineShader *body;
    void *payload;
    uint32_t size;
    uint32_t bytecode_size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size <= sizeof(*body)) {
        g_free(payload);
        return true;
    }

    body = payload;
    bytecode_size = size - sizeof(*body);
    (void)vmsvga3d_state_shader_define(
        s, body->cid, body->shid, body->type, bytecode_size,
        (const uint32_t *)(body + 1));

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_shader_destroy(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start)
{
    SVGA3dCmdDestroyShader *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_state_shader_destroy(s, body->cid, body->shid, body->type);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_shader(struct vmsvga_state_s *s,
                                        uint32_t cmd, int32_t *len,
                                        uint32_t fifo_start)
{
    SVGA3dCmdSetShader *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        body = payload;
        if (!vmsvga3d_state_set_shader(s, body->cid, body->type, body->shid) &&
            body->shid != SVGA3D_INVALID_ID &&
            vmsvga3d_gb_shader_materialize(s, body->cid, body->shid, body->type)) {
            (void)vmsvga3d_state_set_shader(s, body->cid, body->type, body->shid);
        }
    }

    g_free(payload);
    return true;
}

static uint32_t vmsvga3d_shader_const_limit(SVGA3dShaderConstType ctype)
{
    switch (ctype) {
    case SVGA3D_CONST_TYPE_FLOAT:
        return SVGA3D_CONSTREG_MAX;
    case SVGA3D_CONST_TYPE_INT:
        return SVGA3D_CONSTINTREG_MAX;
    case SVGA3D_CONST_TYPE_BOOL:
        return SVGA3D_CONSTBOOLREG_MAX;
    default:
        return 0;
    }
}

static VMSVGA3DShaderConstant *
vmsvga3d_shader_const_array(VMSVGA3DContext *context, uint32_t type_index,
                            SVGA3dShaderConstType ctype)
{
    switch (ctype) {
    case SVGA3D_CONST_TYPE_FLOAT:
        return context->shader_float[type_index];
    case SVGA3D_CONST_TYPE_INT:
        return context->shader_int[type_index];
    case SVGA3D_CONST_TYPE_BOOL:
        return context->shader_bool[type_index];
    default:
        return NULL;
    }
}

static bool vmsvga3d_handle_set_shader_const(struct vmsvga_state_s *s,
                                              uint32_t cmd, int32_t *len,
                                              uint32_t fifo_start)
{
    SVGA3dCmdSetShaderConst *body;
    void *payload;
    uint32_t size;
    uint32_t trailing;
    uint32_t count;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size < sizeof(*body)) {
        g_free(payload);
        return true;
    }

    trailing = size - sizeof(*body);
    if (trailing % sizeof(body->values) != 0) {
        g_free(payload);
        return true;
    }

    body = payload;
    count = 1 + trailing / sizeof(body->values);

    (void)vmsvga3d_state_set_shader_const(
        s, body->cid, body->reg, body->type, body->ctype, count,
        (const uint32_t (*)[4])body->values);

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_set_gb_shader_consts_inline(
    struct vmsvga_state_s *s, uint32_t cmd, int32_t *len,
    uint32_t fifo_start)
{
    const SVGA3dCmdSetGBShaderConstInline *body;
    const uint8_t *values;
    void *payload;
    uint32_t size;
    uint32_t trailing;
    uint32_t count = 0;
    bool applied = false;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size < sizeof(*body)) {
        g_free(payload);
        return true;
    }

    body = payload;
    values = (const uint8_t *)(body + 1);
    trailing = size - sizeof(*body);

    /*
     * The GB inline command carries no explicit register count.  FLOAT and
     * INT constants occupy four dwords per register, while BOOL constants
     * are packed as one dword per register.  Normalize the latter to the
     * four-dword shadow representation used by the legacy state path.
     */
    if (body->constType == SVGA3D_CONST_TYPE_FLOAT ||
        body->constType == SVGA3D_CONST_TYPE_INT) {
        if (trailing != 0 && trailing % (4u * sizeof(uint32_t)) == 0) {
            count = trailing / (4u * sizeof(uint32_t));
            applied = vmsvga3d_state_set_shader_const(
                s, body->cid, body->regStart, body->shaderType,
                body->constType, count,
                (const uint32_t (*)[4])values);
        }
    } else if (body->constType == SVGA3D_CONST_TYPE_BOOL) {
        if (trailing != 0 && trailing % sizeof(uint32_t) == 0) {
            uint32_t normalized[SVGA3D_CONSTBOOLREG_MAX][4] = {{ 0 }};
            const uint32_t *packed = (const uint32_t *)values;
            uint32_t i;

            count = trailing / sizeof(uint32_t);
            if (count <= ARRAY_SIZE(normalized)) {
                for (i = 0; i < count; i++) {
                    normalized[i][0] = packed[i];
                }
                applied = vmsvga3d_state_set_shader_const(
                    s, body->cid, body->regStart, body->shaderType,
                    body->constType, count, normalized);
            }
        }
    }

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "GB-SHADER-CONSTS cid=%u reg=%u shader=%u ctype=%u count=%u "
        "result=%s",
        body->cid, body->regStart, body->shaderType, body->constType, count,
        applied ? "OK" : "IGNORED");

    g_free(payload);
    return true;
}

static bool vmsvga3d_draw_decls_valid(const SVGA3dVertexDecl *decls,
                                       uint32_t decl_count)
{
    uint32_t i;

    if (decls == NULL || decl_count == 0 ||
        decl_count > SVGA3D_MAX_VERTEX_ARRAYS) {
        return false;
    }

    for (i = 0; i < decl_count; i++) {
        if (decls[i].identity.type < SVGA3D_DECLTYPE_FLOAT1 ||
            decls[i].identity.type >= SVGA3D_DECLTYPE_MAX ||
            decls[i].identity.method < SVGA3D_DECLMETHOD_DEFAULT ||
            decls[i].identity.method > SVGA3D_DECLMETHOD_LOOKUPPRESAMPLED ||
            decls[i].identity.usage < SVGA3D_DECLUSAGE_POSITION ||
            decls[i].identity.usage >= SVGA3D_DECLUSAGE_MAX ||
            decls[i].identity.usageIndex > 15) {
            return false;
        }
    }

    return true;
}

static bool vmsvga3d_draw_ranges_valid(struct vmsvga3d_state_s *state,
                                        const SVGA3dPrimitiveRange *ranges,
                                        uint32_t range_count)
{
    uint32_t i;

    if (state == NULL || ranges == NULL || range_count == 0 ||
        range_count > SVGA3D_MAX_DRAW_PRIMITIVE_RANGES) {
        return false;
    }

    for (i = 0; i < range_count; i++) {
        uint32_t sid = ranges[i].indexArray.surfaceId;

        if (ranges[i].primType <= SVGA3D_PRIMITIVE_INVALID ||
            ranges[i].primType >= SVGA3D_PRIMITIVE_PREDX_MAX) {
            return false;
        }
        if (sid == SVGA3D_INVALID_ID) {
            continue;
        }
        if (sid >= SVGA3D_MAX_SURFACE_IDS || state->surfaces[sid] == NULL ||
            (ranges[i].indexWidth != sizeof(uint16_t) &&
             ranges[i].indexWidth != sizeof(uint32_t))) {
            return false;
        }
    }

    return true;
}

static bool vmsvga3d_draw_vertex_surfaces_valid(
    struct vmsvga3d_state_s *state, const SVGA3dVertexDecl *decls,
    uint32_t decl_count)
{
    uint32_t i;

    if (state == NULL || decls == NULL) {
        return false;
    }

    for (i = 0; i < decl_count; i++) {
        uint32_t sid = decls[i].array.surfaceId;

        if (sid >= SVGA3D_MAX_SURFACE_IDS || state->surfaces[sid] == NULL) {
            return false;
        }
    }

    return true;
}


static uint32_t vmsvga3d_build_vertex_decls(VMSVGA3DContext *context,
                                            SVGA3dVertexDecl *decls)
{
    uint32_t i;

    if (context == NULL) {
        return 0;
    }

    for (i = 0; i < MIN(context->num_vertex_decls,
                       SVGA3D_MAX_VERTEX_ARRAYS); i++) {
        SVGA3dVertexElement *element = &context->vertex_decls[i];

        memset(&decls[i], 0, sizeof(decls[i]));
        decls[i].identity.type = element->type;
        decls[i].identity.method = element->method;
        decls[i].identity.usage = element->usage;
        decls[i].identity.usageIndex = element->usageIndex;

        if (element->stream < MIN(context->num_vertex_streams,
                                      SVGA3D_MAX_VERTEX_ARRAYS)) {
            SVGA3dVertexStream *stream =
                &context->vertex_streams[element->stream];

            decls[i].array.surfaceId = stream->sid;
            decls[i].array.offset = stream->offset + element->streamOffset;
            decls[i].array.stride = stream->stride;
        } else {
            decls[i].array.surfaceId = SVGA3D_INVALID_ID;
        }
    }

    return context->num_vertex_decls;
}

static bool vmsvga3d_handle_draw(struct vmsvga_state_s *s,
                                  uint32_t cmd, int32_t *len,
                                  uint32_t fifo_start)
{
    SVGA3dCmdDraw *body;
    SVGA3dVertexDecl decls[SVGA3D_MAX_VERTEX_ARRAYS];
    SVGA3dPrimitiveRange range;
    VMSVGA3DContext *context;
    void *payload;
    uint32_t size;
    uint32_t numDecls;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        body = payload;
        context = vmsvga3d_context(s, body->cid);

        if (context != NULL) {
            numDecls = vmsvga3d_build_vertex_decls(context, decls);

            memset(&range, 0, sizeof(range));
            range.primType = body->primitiveType;
            range.primitiveCount = body->primitiveCount;
            range.indexArray.surfaceId = SVGA3D_INVALID_ID;
            range.indexBias = body->startVertexLocation;

            if (vmsvga3d_state_draw_primitives(
                    s, body->cid, numDecls, decls, 1, &range,
                    context->num_vertex_divisors, context->vertex_divisors)) {
                if (vmsvga3d_d3d9_runtime_draw_primitives(
                        s, body->cid, numDecls, decls, 1, &range,
                        context->num_vertex_divisors,
                        context->vertex_divisors) ==
                    VMSVGA3D_D3D9_ACCEL_COMPLETE) {
                    if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
                        vmsvga3d_trace_vgpu9_render_target_write(
                            s, body->cid, VMSVGA3D_TRACE_VGPU9_WRITE_DRAW);
                    }
                }
            }
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_draw_indexed(struct vmsvga_state_s *s,
                                          uint32_t cmd, int32_t *len,
                                          uint32_t fifo_start)
{
    SVGA3dCmdDrawIndexed *body;
    SVGA3dVertexDecl decls[SVGA3D_MAX_VERTEX_ARRAYS];
    SVGA3dPrimitiveRange range;
    VMSVGA3DContext *context;
    void *payload;
    uint32_t size;
    uint32_t numDecls;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size == sizeof(*body)) {
        body = payload;
        context = vmsvga3d_context(s, body->cid);

        if (context != NULL) {
            numDecls = vmsvga3d_build_vertex_decls(context, decls);

            memset(&range, 0, sizeof(range));
            range.primType = body->primitiveType;
            range.primitiveCount = body->primitiveCount;
            range.indexArray.surfaceId = body->indexBufferSid;
            range.indexArray.offset = body->indexBufferOffset;
            range.indexArray.stride = body->indexBufferStride;
            range.indexWidth = body->indexBufferStride;
            range.indexBias = body->baseVertexLocation;

            if (vmsvga3d_state_draw_primitives(
                    s, body->cid, numDecls, decls, 1, &range,
                    context->num_vertex_divisors, context->vertex_divisors)) {
                if (vmsvga3d_d3d9_runtime_draw_primitives(
                        s, body->cid, numDecls, decls, 1, &range,
                        context->num_vertex_divisors,
                        context->vertex_divisors) ==
                    VMSVGA3D_D3D9_ACCEL_COMPLETE) {
                    if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
                        vmsvga3d_trace_vgpu9_render_target_write(
                            s, body->cid, VMSVGA3D_TRACE_VGPU9_WRITE_DRAW);
                    }
                }
            }
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_draw_primitives(struct vmsvga_state_s *s,
                                             uint32_t cmd, int32_t *len,
                                             uint32_t fifo_start)
{
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
    }

    if (size < sizeof(*body)) {
        g_free(payload);
        return true;
    }

    body = payload;
    if (body->numVertexDecls > SVGA3D_MAX_VERTEX_ARRAYS ||
        body->numRanges > SVGA3D_MAX_DRAW_PRIMITIVE_RANGES) {
        g_free(payload);
        return true;
    }

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
    }

    decls = (SVGA3dVertexDecl *)(body + 1);
    ranges = (SVGA3dPrimitiveRange *)(decls + body->numVertexDecls);

    if (size != (uint32_t)base_size) {
        divisors = (SVGA3dVertexDivisor *)(ranges + body->numRanges);
        divisor_count = body->numVertexDecls;
    }

    if (vmsvga3d_state_draw_primitives(
            s, body->cid, body->numVertexDecls, decls, body->numRanges, ranges,
            divisor_count, divisors)) {
        if (vmsvga3d_d3d9_runtime_draw_primitives(
                s, body->cid, body->numVertexDecls, decls, body->numRanges,
                ranges, divisor_count, divisors) ==
            VMSVGA3D_D3D9_ACCEL_COMPLETE) {
            if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
                vmsvga3d_trace_vgpu9_render_target_write(
                    s, body->cid, VMSVGA3D_TRACE_VGPU9_WRITE_DRAW);
            }
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_clear_rect(const SVGA3dRect *rect,
                                 const SVGA3dSize *size,
                                 SVGA3dRect *clipped)
{
    uint64_t end_x;
    uint64_t end_y;

    if (rect->w == 0 || rect->h == 0 || rect->x >= size->width ||
        rect->y >= size->height) {
        return false;
    }

    end_x = MIN((uint64_t)rect->x + rect->w, (uint64_t)size->width);
    end_y = MIN((uint64_t)rect->y + rect->h, (uint64_t)size->height);
    if (end_x <= rect->x || end_y <= rect->y) {
        return false;
    }

    *clipped = *rect;
    clipped->w = (uint32_t)(end_x - rect->x);
    clipped->h = (uint32_t)(end_y - rect->y);
    return true;
}

static uint64_t vmsvga3d_bit_mask(uint32_t bits)
{
    if (bits >= 64) {
        return UINT64_MAX;
    }

    if (bits == 0) {
        return 0;
    }

    return (UINT64_C(1) << bits) - 1;
}

static uint64_t vmsvga3d_scale_u8(uint32_t value, uint32_t bits)
{
    uint64_t mask = vmsvga3d_bit_mask(bits);

    if (bits == 0) {
        return 0;
    }

    return ((uint64_t)value * mask + 127) / 255;
}

static uint32_t vmsvga3d_load_le(const uint8_t *data, uint32_t bytes)
{
    uint32_t value = 0;
    uint32_t i;

    for (i = 0; i < bytes; i++) {
        value |= (uint32_t)data[i] << (i * 8);
    }

    return value;
}

static void vmsvga3d_store_le(uint8_t *data, uint32_t bytes,
                              uint32_t value)
{
    uint32_t i;

    for (i = 0; i < bytes; i++) {
        data[i] = (uint8_t)(value >> (i * 8));
    }
}

static bool vmsvga3d_clear_color_value(SVGA3dSurfaceFormat format,
                                        uint32_t color, uint32_t *value,
                                        uint32_t *bytes)
{
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
    }

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
}

static bool vmsvga3d_clear_color_image(VMSVGA3DSurface *surface,
                                        VMSVGA3DSurfaceImage *image,
                                        uint32_t color,
                                        const SVGA3dRect *rects,
                                        uint32_t rect_count,
                                        bool execute)
{
    SVGA3dRect full_rect;
    uint32_t value;
    uint32_t bytes;
    uint32_t i;

    if (surface->multisample_count > 1 ||
        !vmsvga3d_clear_color_value(surface->format, color, &value, &bytes)) {
        return false;
    }

    if (rect_count == 0) {
        full_rect.x = 0;
        full_rect.y = 0;
        full_rect.w = image->size.width;
        full_rect.h = image->size.height;
        rects = &full_rect;
        rect_count = 1;
    }

    for (i = 0; i < rect_count; i++) {
        SVGA3dRect rect;
        uint32_t y;
        uint32_t x;

        if (!vmsvga3d_clear_rect(&rects[i], &image->size, &rect)) {
            continue;
        }

        if (!execute) {
            continue;
        }

        for (y = 0; y < rect.h; y++) {
            uint8_t *row = image->data + (uint64_t)(rect.y + y) * image->pitch +
                           (uint64_t)rect.x * bytes;
            for (x = 0; x < rect.w; x++) {
                vmsvga3d_store_le(row + (uint64_t)x * bytes, bytes, value);
            }
        }
    }

    return true;
}

static bool vmsvga3d_clear_depth_stencil_image(
    VMSVGA3DSurface *surface, VMSVGA3DSurfaceImage *image,
    bool clear_depth, float depth, bool clear_stencil, uint32_t stencil,
    const SVGA3dRect *rects, uint32_t rect_count, bool execute)
{
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
    }

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
    }

    depth_mask = vmsvga3d_bit_mask(depth_bits) << depth_shift;
    stencil_mask = vmsvga3d_bit_mask(stencil_bits) << stencil_shift;

    if (clear_depth) {
        double normalized = depth;
        uint64_t maximum = vmsvga3d_bit_mask(depth_bits);

        if (!(normalized >= 0.0)) {
            normalized = 0.0;
        } else if (normalized > 1.0) {
            normalized = 1.0;
        }
        depth_value = (uint64_t)(normalized * (double)maximum + 0.5) << depth_shift;
    }

    if (clear_stencil) {
        stencil_value = ((uint64_t)stencil & vmsvga3d_bit_mask(stencil_bits))
                        << stencil_shift;
    }

    if (rect_count == 0) {
        full_rect.x = 0;
        full_rect.y = 0;
        full_rect.w = image->size.width;
        full_rect.h = image->size.height;
        rects = &full_rect;
        rect_count = 1;
    }

    for (i = 0; i < rect_count; i++) {
        SVGA3dRect rect;
        uint32_t y;
        uint32_t x;

        if (!vmsvga3d_clear_rect(&rects[i], &image->size, &rect)) {
            continue;
        }

        if (!execute) {
            continue;
        }

        for (y = 0; y < rect.h; y++) {
            uint8_t *row = image->data + (uint64_t)(rect.y + y) * image->pitch +
                           (uint64_t)rect.x * bytes;
            for (x = 0; x < rect.w; x++) {
                uint8_t *pixel = row + (uint64_t)x * bytes;
                uint32_t value = vmsvga3d_load_le(pixel, bytes);

                if (clear_depth) {
                    value = (uint32_t)(((uint64_t)value & ~depth_mask) | depth_value);
                }
                if (clear_stencil) {
                    value = (uint32_t)(((uint64_t)value & ~stencil_mask) |
                                       stencil_value);
                }
                vmsvga3d_store_le(pixel, bytes, value);
            }
        }
    }

    return true;
}

static bool vmsvga3d_clear_target(struct vmsvga_state_s *s,
                                   VMSVGA3DContext *context,
                                   SVGA3dRenderTargetType type,
                                   uint32_t color, float depth,
                                   uint32_t stencil,
                                   const SVGA3dRect *rects,
                                   uint32_t rect_count, bool execute)
{
    SVGA3dSurfaceImageId *target = &context->render_targets[type];
    VMSVGA3DSurface *surface;
    VMSVGA3DSurfaceImage *image;

    if (target->sid == SVGA3D_INVALID_ID) {
        return true;
    }

    if (s->svga3d == NULL || target->sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    surface = s->svga3d->surfaces[target->sid];
    if (!vmsvga3d_surface_image(surface, target, &image)) {
        return false;
    }

    if (type >= SVGA3D_RT_COLOR0 && type <= SVGA3D_RT_COLOR7) {
        return vmsvga3d_clear_color_image(surface, image, color, rects,
                                          rect_count, execute);
    }

    return vmsvga3d_clear_depth_stencil_image(
        surface, image, type == SVGA3D_RT_DEPTH, depth,
        type == SVGA3D_RT_STENCIL, stencil, rects, rect_count, execute);
}

static bool vmsvga3d_clear_target_d3d11_resident(
    struct vmsvga_state_s *s, const VMSVGA3DContext *context,
    SVGA3dRenderTargetType type)
{
    const SVGA3dSurfaceImageId *target;
    VMSVGA3DSurface *surface;

    if (s == NULL || s->svga3d == NULL || context == NULL ||
        type >= SVGA3D_RT_MAX) {
        return false;
    }

    target = &context->render_targets[type];
    if (target->sid == SVGA3D_INVALID_ID ||
        target->sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    surface = s->svga3d->surfaces[target->sid];

    return surface != NULL &&
           vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface);
}

static bool vmsvga3d_clear_hits_d3d11_resident_target(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dClearFlag flags)
{
    VMSVGA3DContext *context = vmsvga3d_context(s, cid);
    uint32_t type;

    if (context == NULL) {
        return false;
    }

    if (flags & SVGA3D_CLEAR_COLOR) {
        for (type = SVGA3D_RT_COLOR0; type <= SVGA3D_RT_COLOR7; type++) {
            if (vmsvga3d_clear_target_d3d11_resident(
                    s, context, (SVGA3dRenderTargetType)type)) {
                return true;
            }
        }
    }

    if ((flags & SVGA3D_CLEAR_DEPTH) &&
        vmsvga3d_clear_target_d3d11_resident(s, context, SVGA3D_RT_DEPTH)) {
        return true;
    }

    if ((flags & SVGA3D_CLEAR_STENCIL) &&
        vmsvga3d_clear_target_d3d11_resident(s, context,
                                              SVGA3D_RT_STENCIL)) {
        return true;
    }

    return false;
}

static bool vmsvga3d_handle_clear(struct vmsvga_state_s *s,
                                  uint32_t cmd, int32_t *len,
                                  uint32_t fifo_start)
{
    SVGA3dCmdClear *body;
    SVGA3dRect *rects;
    void *payload;
    uint32_t size;
    uint32_t rect_bytes;
    uint32_t rect_count;
    VMSVGA3DD3D9AccelResult accel;
    bool wrote = false;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size < sizeof(*body)) {
        g_free(payload);
        return true;
    }

    rect_bytes = size - sizeof(*body);
    if (rect_bytes % sizeof(SVGA3dRect) != 0) {
        g_free(payload);
        return true;
    }

    body = payload;
    rects = (SVGA3dRect *)(body + 1);
    rect_count = rect_bytes / sizeof(SVGA3dRect);
    if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
        VMSVGA3DContext *context = vmsvga3d_context(s, body->cid);
        uint32_t color_sid = SVGA3D_INVALID_ID;
        uint32_t depth_sid = SVGA3D_INVALID_ID;
        uint32_t stencil_sid = SVGA3D_INVALID_ID;

        if (context != NULL) {
            color_sid = context->render_targets[SVGA3D_RT_COLOR0].sid;
            depth_sid = context->render_targets[SVGA3D_RT_DEPTH].sid;
            stencil_sid = context->render_targets[SVGA3D_RT_STENCIL].sid;
        }
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "D3D9-CLEAR cid=%u flags=0x%08x color=0x%08x depth=%g "
            "stencil=0x%08x rects=%u rt0=%u depth-sid=%u stencil-sid=%u",
            body->cid, body->clearFlag, body->color, (double)body->depth,
            body->stencil, rect_count, color_sid, depth_sid, stencil_sid);
    }

    /* The VirtualBox D3D11 VGPU9 clear callback is a successful no-op.
     * Once one of the requested render targets is D3D11-resident, running our
     * CPU fallback would create a shadow/native split that VBox never creates. */
    if (vmsvga3d_clear_hits_d3d11_resident_target(
            s, body->cid, body->clearFlag)) {
        g_free(payload);
        return true;
    }

    accel = vmsvga3d_d3d9_try_clear(s, body, rects, rect_count);
    if (accel == VMSVGA3D_D3D9_ACCEL_COMPLETE) {
        wrote = true;
    } else if (accel == VMSVGA3D_D3D9_ACCEL_UNAVAILABLE) {
        /* CPU clear must preserve pixels outside a partial clear.  Pull any
         * resident renderer contents into the canonical shadow first instead
         * of assuming a D3D9 target is still CPU-coherent. */
        if (vmsvga3d_clear_readback_targets(
                s, body->cid, body->clearFlag) &&
            vmsvga3d_state_clear(s, body->cid, body->clearFlag, body->color,
                                 body->depth, body->stencil, rect_count, rects)) {
            vmsvga3d_d3d9_runtime_sync_clear_targets_from_cpu(
                s, body->cid, body->clearFlag);
            wrote = true;
        }
    }

    if (wrote && (body->clearFlag & SVGA3D_CLEAR_COLOR)) {
        if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
            vmsvga3d_trace_vgpu9_render_target_write(
                s, body->cid, VMSVGA3D_TRACE_VGPU9_WRITE_CLEAR);
        }
    }

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "D3D9-CLEAR result cid=%u accel=%u wrote=%u",
        body->cid, (uint32_t)accel, wrote ? 1u : 0u);
    g_free(payload);
    return true;
}


static bool vmsvga3d_surface_image(VMSVGA3DSurface *surface,
                                   const SVGA3dSurfaceImageId *image_id,
                                   VMSVGA3DSurfaceImage **image)
{
    uint32_t levels;
    uint32_t face_count;
    uint32_t index;

    if (surface == NULL || image_id == NULL || image == NULL) {
        return false;
    }

    levels = surface->face[0].numMipLevels;
    face_count = surface->array_elements;
    if (levels == 0 || face_count == 0 || image_id->face >= face_count ||
        image_id->mipmap >= levels) {
        return false;
    }

    index = image_id->face * levels + image_id->mipmap;
    if (index >= surface->mip_count) {
        return false;
    }

    *image = &surface->mips[index];
    return true;
}

static bool vmsvga3d_clip_dma_box(const SVGA3dCopyBox *box,
                                   const SVGA3dSize *size,
                                   SVGA3dCopyBox *clipped)
{
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
    }

    end_x = MIN((uint64_t)box->x + box->w, (uint64_t)size->width);
    end_y = MIN((uint64_t)box->y + box->h, (uint64_t)size->height);
    end_z = MIN((uint64_t)box->z + box->d, (uint64_t)size->depth);
    if (end_x <= box->x || end_y <= box->y || end_z <= box->z) {
        return false;
    }

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
    }

    clipped->srcx = box->srcx + delta_x;
    clipped->srcy = box->srcy + delta_y;
    clipped->srcz = box->srcz + delta_z;

    return true;
}

static bool vmsvga3d_u64_add_product(uint64_t *value, uint64_t count,
                                      uint64_t stride)
{
    uint64_t product;

    if (count != 0 && stride > UINT64_MAX / count) {
        return false;
    }

    product = count * stride;
    if (*value > UINT64_MAX - product) {
        return false;
    }

    *value += product;
    return true;
}

static bool vmsvga3d_d3d9_clear_context_info(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t *target_width,
    uint32_t *target_height, uint32_t *active_render_target_mask)
{
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
    const SVGA3dRect *rects, uint32_t rect_count)
{
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
    const SVGA3dCopyRect *rects, uint32_t rect_count)
{
    VMSVGA3DD3D9TransferSurface surface_info;
    VMSVGA3DD3D9PresentPlan plan;
    VMSVGA3DSurface *surface;
    uint32_t width;
    uint32_t height;

    if (s != NULL && s->screen_defined) {
        /* Keep legacy PRESENT on the CPU path while a Screen Object is active so
         * it targets the exact guest-backed or fallback base layer. */
        return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
    }

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
    const SVGASignedRect *clips, uint32_t clip_count)
{
    if (s != NULL && s->screen_defined) {
        /* Keep this command on the CPU path while a Screen Object is active so
         * it targets the exact guest-backed or fallback base layer. */
        return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
    }
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

static const char *vmsvga3d_d3d9_accel_result_name(
    VMSVGA3DD3D9AccelResult result)
{
    switch (result) {
    case VMSVGA3D_D3D9_ACCEL_UNAVAILABLE:
        return "unavailable";
    case VMSVGA3D_D3D9_ACCEL_COMPLETE:
        return "complete";
    case VMSVGA3D_D3D9_ACCEL_FAILED:
        return "failed";
    default:
        return "invalid";
    }
}

static const char *vmsvga3d_d3d9_execution_name(
    VMSVGA3DD3D9ExecutionPreference execution)
{
    switch (execution) {
    case VMSVGA3D_D3D9_EXECUTION_CPU_ONLY:
        return "cpu";
    case VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED:
        return "gpu-preferred";
    default:
        return "invalid";
    }
}

static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_surface_copy(
    struct vmsvga_state_s *s, const SVGA3dCmdSurfaceCopy *command,
    const SVGA3dCopyBox *boxes, uint32_t box_count,
    const VMSVGA3DD3D9SurfaceCopyPlan *plan)
{
    if (plan->execution == VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED) {
        return vmsvga3d_d3d9_runtime_surface_copy(s, command, boxes, box_count,
                                                  plan);
    }

    return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
}

static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_stretch_blt(
    struct vmsvga_state_s *s, const SVGA3dCmdSurfaceStretchBlt *command,
    const VMSVGA3DD3D9StretchBltPlan *plan)
{
    if (plan->execution == VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED) {
        return vmsvga3d_d3d9_runtime_stretch_blt(s, command, plan);
    }

    return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
}

static VMSVGA3DD3D9AccelResult vmsvga3d_d3d9_try_surface_dma(
    struct vmsvga_state_s *s, const SVGA3dCmdSurfaceDMA *command,
    const SVGA3dCopyBox *boxes, uint32_t box_count, uint32_t maximum_offset,
    const VMSVGA3DD3D9DmaPlan *plan)
{
    if (plan->execution == VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED) {
        return vmsvga3d_d3d9_runtime_surface_dma(s, command, boxes, box_count,
                                                 maximum_offset, plan);
    }

    return VMSVGA3D_D3D9_ACCEL_UNAVAILABLE;
}

static void vmsvga3d_clip_surface_copy_box(const SVGA3dCopyBox *box,
                                             const SVGA3dSize *src_size,
                                             const SVGA3dSize *dst_size,
                                             SVGA3dCopyBox *clipped)
{
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
    }

    max_width = MIN(src_size->width - box->srcx, dst_size->width - box->x);
    max_height = MIN(src_size->height - box->srcy, dst_size->height - box->y);
    max_depth = MIN(src_size->depth - box->srcz, dst_size->depth - box->z);

    clipped->w = MIN(box->w, max_width);
    clipped->h = MIN(box->h, max_height);
    clipped->d = MIN(box->d, max_depth);
}

static bool vmsvga3d_surface_copy_box(
    VMSVGA3DSurface *src_surface, VMSVGA3DSurfaceImage *src_image,
    VMSVGA3DSurface *dst_surface, VMSVGA3DSurfaceImage *dst_image,
    const SVGA3dCopyBox *box, uint8_t *scratch, size_t scratch_size,
    bool execute, size_t *scratch_needed)
{
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
    }

    if (src_surface == NULL || src_image == NULL || dst_surface == NULL ||
        dst_image == NULL || box == NULL) {
        return false;
    }

    vmsvga3d_clip_surface_copy_box(box, &src_image->size, &dst_image->size,
                                   &clipped);
    if (clipped.w == 0 || clipped.h == 0 || clipped.d == 0) {
        return true;
    }

    if (src_surface->multisample_count > 1 ||
        dst_surface->multisample_count > 1) {
        return false;
    }

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
    }

    block_width = desc->block_size.width;
    block_height = desc->block_size.height;
    block_depth = desc->block_size.depth;
    bytes_per_block = desc->bytes_per_block;

    if (clipped.srcx % block_width != 0 ||
        clipped.srcy % block_height != 0 ||
        clipped.srcz % block_depth != 0 || clipped.x % block_width != 0 ||
        clipped.y % block_height != 0 || clipped.z % block_depth != 0) {
        return false;
    }

    blocks_x = ((uint64_t)clipped.w + block_width - 1) / block_width;
    blocks_y = ((uint64_t)clipped.h + block_height - 1) / block_height;
    blocks_z = ((uint64_t)clipped.d + block_depth - 1) / block_depth;

    if (blocks_x == 0 || blocks_y == 0 || blocks_z == 0 ||
        blocks_x > UINT64_MAX / bytes_per_block) {
        return false;
    }

    row_bytes = blocks_x * bytes_per_block;
    if (row_bytes > src_image->pitch || row_bytes > dst_image->pitch) {
        return false;
    }

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
    }

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
    }

    src_end += row_bytes;
    dst_end += row_bytes;

    if (src_end > src_image->data_size || dst_end > dst_image->data_size) {
        return false;
    }

    same_image = src_image->data == dst_image->data;
    if (same_image) {
        if (blocks_y > UINT64_MAX / blocks_z ||
            blocks_y * blocks_z > UINT64_MAX / row_bytes) {
            return false;
        }
        temporary_size = blocks_y * blocks_z * row_bytes;
        if (temporary_size > SIZE_MAX) {
            return false;
        }
        if (scratch_needed != NULL) {
            *scratch_needed = (size_t)temporary_size;
        }
        if (execute && (scratch == NULL || scratch_size < temporary_size)) {
            return false;
        }
    }

    if (!execute) {
        return true;
    }

    if (same_image) {
        size_t temporary_offset = 0;

        for (z = 0; z < blocks_z; z++) {
            for (y = 0; y < blocks_y; y++) {
                uint64_t offset = src_offset + z * src_image->plane_size +
                                  y * src_image->pitch;
                memcpy(scratch + temporary_offset, src_image->data + offset,
                       (size_t)row_bytes);
                temporary_offset += (size_t)row_bytes;
            }
        }
        temporary_offset = 0;
        for (z = 0; z < blocks_z; z++) {
            for (y = 0; y < blocks_y; y++) {
                uint64_t offset = dst_offset + z * dst_image->plane_size +
                                  y * dst_image->pitch;
                memcpy(dst_image->data + offset, scratch + temporary_offset,
                       (size_t)row_bytes);
                temporary_offset += (size_t)row_bytes;
            }
        }
    } else {
        for (z = 0; z < blocks_z; z++) {
            for (y = 0; y < blocks_y; y++) {
                uint64_t source = src_offset + z * src_image->plane_size +
                                  y * src_image->pitch;
                uint64_t destination = dst_offset + z * dst_image->plane_size +
                                       y * dst_image->pitch;
                memcpy(dst_image->data + destination, src_image->data + source,
                       (size_t)row_bytes);
            }
        }
    }

    return true;
}

static bool vmsvga3d_handle_surface_copy(struct vmsvga_state_s *s,
                                         uint32_t cmd, int32_t *len,
                                         uint32_t fifo_start)
{
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
    bool d3d11_copy = false;
    bool trace_3d = VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D);
    bool valid = true;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size < sizeof(*body) ||
        (size - sizeof(*body)) % sizeof(SVGA3dCopyBox) != 0) {
        g_free(payload);
        return true;
    }

    body = payload;
    boxes = (SVGA3dCopyBox *)(body + 1);
    box_count = (size - sizeof(*body)) / sizeof(*boxes);
    state = s->svga3d;

    if (state == NULL || body->src.sid >= SVGA3D_MAX_SURFACE_IDS ||
        body->dest.sid >= SVGA3D_MAX_SURFACE_IDS) {
        valid = false;
    }

    src_surface = valid ? state->surfaces[body->src.sid] : NULL;
    dst_surface = valid ? state->surfaces[body->dest.sid] : NULL;

    if (valid &&
        (!vmsvga3d_surface_image(src_surface, &body->src, &src_image) ||
         !vmsvga3d_surface_image(dst_surface, &body->dest, &dst_image))) {
        valid = false;
    }

    if (valid && vmsvga3d_dxvk_ready(s->dxvk)) {
        d3d11_copy =
            vmsvga3d_dxvk_d3d11_surface_resident(src_surface->dxvk_surface) ||
            vmsvga3d_dxvk_d3d11_surface_resident(dst_surface->dxvk_surface);
    }

    if (valid && !d3d11_copy &&
        (!vmsvga3d_d3d9_runtime_surface_info(s, src_surface, &src_info) ||
         !vmsvga3d_d3d9_runtime_surface_info(s, dst_surface, &dst_info) ||
         !vmsvga3d_d3d9_surface_copy_plan(&src_info, &dst_info,
                                          &transfer_plan))) {
        valid = false;
    }

    if (trace_3d && valid && d3d11_copy) {
        fprintf(stderr,
                "VMVGA-D3D9-COPY src=%u:%u:%u dst=%u:%u:%u boxes=%u "
                "src-active=%u dst-active=%u path=d3d11 "
                "src-d3d11=%u dst-d3d11=%u\n",
                body->src.sid, body->src.face, body->src.mipmap,
                body->dest.sid, body->dest.face, body->dest.mipmap, box_count,
                src_surface->legacy_active, dst_surface->legacy_active,
                vmsvga3d_dxvk_d3d11_surface_resident(src_surface->dxvk_surface),
                vmsvga3d_dxvk_d3d11_surface_resident(dst_surface->dxvk_surface));
    } else if (trace_3d && valid) {
        fprintf(stderr,
                "VMVGA-D3D9-COPY src=%u:%u:%u dst=%u:%u:%u boxes=%u "
                "src-active=%u dst-active=%u "
                "src-resident=%u src-bounce=%u src-kind=%u src-usage=0x%08x "
                "dst-resident=%u dst-bounce=%u dst-kind=%u dst-usage=0x%08x "
                "plan=%s lock-single=%u lock-src-ro=%u update-dst=%u\n",
                body->src.sid, body->src.face, body->src.mipmap,
                body->dest.sid, body->dest.face, body->dest.mipmap, box_count,
                src_surface->legacy_active, dst_surface->legacy_active,
                src_info.resident, src_info.has_bounce, src_info.resource_type,
                src_info.usage, dst_info.resident, dst_info.has_bounce,
                dst_info.resource_type, dst_info.usage,
                vmsvga3d_d3d9_execution_name(transfer_plan.execution),
                transfer_plan.lock_single_gpu_surface,
                transfer_plan.lock_source_readonly,
                transfer_plan.update_destination_texture);
        for (i = 0; i < box_count; i++) {
            fprintf(stderr,
                    "VMVGA-D3D9-COPY box[%u] src=%u,%u,%u dst=%u,%u,%u "
                    "size=%ux%ux%u\n",
                    i, boxes[i].srcx, boxes[i].srcy, boxes[i].srcz, boxes[i].x,
                    boxes[i].y, boxes[i].z, boxes[i].w, boxes[i].h, boxes[i].d);
        }
    }

    if (valid && !d3d11_copy) {
        accel = vmsvga3d_d3d9_try_surface_copy(s, body, boxes, box_count,
                                                &transfer_plan);
        if (trace_3d) {
            fprintf(stderr,
                    "VMVGA-D3D9-COPY result src=%u dst=%u accel=%s "
                    "fallback=%s\n",
                    body->src.sid, body->dest.sid,
                    vmsvga3d_d3d9_accel_result_name(accel),
                    accel == VMSVGA3D_D3D9_ACCEL_COMPLETE ? "none" : "cpu-shadow");
        }
        if (accel == VMSVGA3D_D3D9_ACCEL_FAILED) {
            valid = false;
        }
    }

    /* VirtualBox switches the whole operation to D3D11 as soon as either
     * surface already owns a native resource.  It then materializes the other
     * surface and executes every clipped box with CopySubresourceRegion; the
     * system-memory copies are deliberately left stale until a later readback.
     */
    if (valid && d3d11_copy) {
        VMSVGA3DD3D10ResourceCreateKind src_kind =
            src_surface->format == SVGA3D_BUFFER
                ? VMSVGA3D_D3D10_CREATE_BUFFER
                : VMSVGA3D_D3D10_CREATE_TEXTURE;
        VMSVGA3DD3D10ResourceCreateKind dst_kind =
            dst_surface->format == SVGA3D_BUFFER
                ? VMSVGA3D_D3D10_CREATE_BUFFER
                : VMSVGA3D_D3D10_CREATE_TEXTURE;
        uint32_t src_subresource =
            body->src.face * src_surface->face[0].numMipLevels +
            body->src.mipmap;
        uint32_t dst_subresource =
            body->dest.face * dst_surface->face[0].numMipLevels +
            body->dest.mipmap;

        if (!vmsvga3d_d3d10_copy_surface_materialize_live(
                s, src_surface, src_kind) ||
            !vmsvga3d_d3d10_copy_surface_materialize_live(
                s, dst_surface, dst_kind)) {
            valid = false;
        }

        for (i = 0; valid && i < box_count; i++) {
            SVGA3dCopyBox clipped;
            VMSVGA3DD3D10Box src_box;

            vmsvga3d_clip_surface_copy_box(&boxes[i], &src_image->size,
                                           &dst_image->size, &clipped);
            if (clipped.w == 0 || clipped.h == 0 || clipped.d == 0) {
                continue;
            }
            src_box.left = clipped.srcx;
            src_box.top = clipped.srcy;
            src_box.front = clipped.srcz;
            src_box.right = clipped.srcx + clipped.w;
            src_box.bottom = clipped.srcy + clipped.h;
            src_box.back = clipped.srcz + clipped.d;
            if (!vmsvga3d_dxvk_d3d11_copy_subresource_region(
                    s->dxvk, dst_surface->dxvk_surface, dst_subresource,
                    clipped.x, clipped.y, clipped.z, src_surface->dxvk_surface,
                    src_subresource, &src_box)) {
                valid = false;
            }
        }
    }

    if (valid && !d3d11_copy &&
        accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
        uint32_t src_subresource = (uint32_t)(src_image - src_surface->mips);
        uint32_t dst_subresource = (uint32_t)(dst_image - dst_surface->mips);

        /* The CPU fallback reads the source shadow and can partially overwrite
         * the destination shadow.  Synchronize both first so GPU-new pixels
         * are never replaced by stale system-memory contents. */
        valid = vmsvga3d_surface_readback_to_shadow(
            s, src_surface, src_image, src_subresource);
        if (valid && (src_surface != dst_surface || src_image != dst_image)) {
            valid = vmsvga3d_surface_readback_to_shadow(
                s, dst_surface, dst_image, dst_subresource);
        }
    }

    for (i = 0; valid && !d3d11_copy &&
                accel != VMSVGA3D_D3D9_ACCEL_COMPLETE && i < box_count; i++) {
        size_t needed = 0;

        valid = vmsvga3d_surface_copy_box(src_surface, src_image, dst_surface,
                                          dst_image, &boxes[i], NULL, 0, false,
                                          &needed);
        if (valid && needed > scratch_size) {
            uint8_t *new_scratch = g_try_realloc(scratch, needed);

            if (new_scratch == NULL) {
                valid = false;
            } else {
                scratch = new_scratch;
                scratch_size = needed;
            }
        }
        if (valid) {
            valid = vmsvga3d_surface_copy_box(src_surface, src_image, dst_surface,
                                              dst_image, &boxes[i], scratch,
                                              scratch_size, true, NULL);
        }
    }
    if (valid && !d3d11_copy &&
        accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
        vmsvga3d_d3d9_runtime_sync_surface_from_cpu(s, dst_surface);
    }

    if (valid && dst_image != NULL &&
        VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
        bool full_copy = box_count == 1 && boxes[0].x == 0 && boxes[0].y == 0 &&
                         boxes[0].z == 0 &&
                         boxes[0].w == dst_image->size.width &&
                         boxes[0].h == dst_image->size.height &&
                         boxes[0].d == dst_image->size.depth;

        vmsvga3d_trace_vgpu9_surface_write(
            s, body->dest.sid, VMSVGA3D_TRACE_VGPU9_WRITE_COPY,
            SVGA3D_INVALID_ID, full_copy);
    }

    g_free(scratch);
    g_free(payload);

    return true;
}

static void vmsvga3d_clip_surface_box(const SVGA3dBox *box,
                                       const SVGA3dSize *size,
                                       SVGA3dBox *clipped)
{
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
    }

    end_x = MIN((uint64_t)box->x + box->w, (uint64_t)size->width);
    end_y = MIN((uint64_t)box->y + box->h, (uint64_t)size->height);
    end_z = MIN((uint64_t)box->z + box->d, (uint64_t)size->depth);
    clipped->w = (uint32_t)(end_x - box->x);
    clipped->h = (uint32_t)(end_y - box->y);
    clipped->d = (uint32_t)(end_z - box->z);
}

static bool vmsvga3d_surface_stretchblt_point(
    VMSVGA3DSurface *src_surface, VMSVGA3DSurfaceImage *src_image,
    VMSVGA3DSurface *dst_surface, VMSVGA3DSurfaceImage *dst_image,
    const SVGA3dBox *src_box, const SVGA3dBox *dst_box,
    uint8_t *scratch, size_t scratch_size, bool execute,
    size_t *scratch_needed)
{
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
    }

    if (src_surface == NULL || src_image == NULL || dst_surface == NULL ||
        dst_image == NULL || src_box == NULL || dst_box == NULL) {
        return false;
    }

    if (src_surface->format != dst_surface->format ||
        src_surface->multisample_count > 1 ||
        dst_surface->multisample_count > 1) {
        return false;
    }

    vmsvga3d_clip_surface_box(src_box, &src_image->size, &src_clipped);
    vmsvga3d_clip_surface_box(dst_box, &dst_image->size, &dst_clipped);

    if (src_clipped.w == 0 || src_clipped.h == 0 || src_clipped.d == 0 ||
        dst_clipped.w == 0 || dst_clipped.h == 0 || dst_clipped.d == 0) {
        return true;
    }

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
    }

    desc = svga3dsurface_get_desc(src_surface->format);
    if (desc->format != src_surface->format || desc->bytes_per_block == 0 ||
        desc->pitch_bytes_per_block != desc->bytes_per_block ||
        desc->block_size.width != 1 || desc->block_size.height != 1 ||
        desc->block_size.depth != 1) {
        return false;
    }

    bytes_per_pixel = desc->bytes_per_block;

    if ((uint64_t)src_clipped.x * bytes_per_pixel >= src_image->pitch ||
        (uint64_t)dst_clipped.x * bytes_per_pixel >= dst_image->pitch) {
        return false;
    }

    if ((uint64_t)(src_clipped.x + src_clipped.w) * bytes_per_pixel >
            src_image->pitch ||
        (uint64_t)(dst_clipped.x + dst_clipped.w) * bytes_per_pixel >
            dst_image->pitch) {
        return false;
    }

    same_image = src_image->data == dst_image->data;
    if (same_image) {
        src_row_bytes = (uint64_t)src_clipped.w * bytes_per_pixel;
        if (src_row_bytes > SIZE_MAX ||
            src_row_bytes > UINT64_MAX / src_clipped.h) {
            return false;
        }
        src_plane_bytes = src_row_bytes * src_clipped.h;
        if (src_plane_bytes > UINT64_MAX / src_clipped.d) {
            return false;
        }
        temporary_size = src_plane_bytes * src_clipped.d;
        if (temporary_size > SIZE_MAX) {
            return false;
        }
        if (scratch_needed != NULL) {
            *scratch_needed = (size_t)temporary_size;
        }
        if (execute && (scratch == NULL || scratch_size < temporary_size)) {
            return false;
        }
    }

    if (!execute) {
        return true;
    }

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
                }
                memcpy(scratch + temporary_offset, src_image->data + src_offset,
                       row_bytes);
                temporary_offset += row_bytes;
            }
        }
    }

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
                }
                if (same_image) {
                    uint64_t scratch_offset =
                        ((uint64_t)src_z * src_clipped.h * src_clipped.w +
                         (uint64_t)src_y * src_clipped.w + src_x) *
                        bytes_per_pixel;
                    if (scratch_offset > scratch_size ||
                        bytes_per_pixel > scratch_size - scratch_offset) {
                        return false;
                    }
                    source = scratch + scratch_offset;
                } else {
                    uint64_t src_offset =
                        (uint64_t)(src_clipped.z + src_z) * src_image->plane_size +
                        (uint64_t)(src_clipped.y + src_y) * src_image->pitch +
                        (uint64_t)(src_clipped.x + src_x) * bytes_per_pixel;
                    if (src_offset > src_image->data_size ||
                        bytes_per_pixel > src_image->data_size - src_offset) {
                        return false;
                    }
                    source = src_image->data + src_offset;
                }
                memcpy(dst_image->data + dst_offset, source, bytes_per_pixel);
            }
        }
    }

    return true;
}

static bool vmsvga3d_handle_surface_stretchblt(struct vmsvga_state_s *s,
                                                uint32_t cmd, int32_t *len,
                                                uint32_t fifo_start)
{
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
    bool d3d11_stretch = false;
    bool valid = true;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size != sizeof(*body)) {
        g_free(payload);
        return true;
    }

    body = payload;
    state = s->svga3d;
    if (state == NULL || body->src.sid >= SVGA3D_MAX_SURFACE_IDS ||
        body->dest.sid >= SVGA3D_MAX_SURFACE_IDS) {
        valid = false;
    }

    src_surface = valid ? state->surfaces[body->src.sid] : NULL;
    dst_surface = valid ? state->surfaces[body->dest.sid] : NULL;
    if (valid &&
        (!vmsvga3d_surface_image(src_surface, &body->src, &src_image) ||
         !vmsvga3d_surface_image(dst_surface, &body->dest, &dst_image))) {
        valid = false;
    }

    if (valid && vmsvga3d_dxvk_ready(s->dxvk)) {
        d3d11_stretch =
            vmsvga3d_dxvk_d3d11_surface_resident(src_surface->dxvk_surface) ||
            vmsvga3d_dxvk_d3d11_surface_resident(dst_surface->dxvk_surface);
    }

    /* The VirtualBox D3D11 frontend materializes both texture resources when
     * either side already owns a hardware resource.  Its D3D11 StretchBlt
     * backend then deliberately returns success without issuing any copy.
     * Do not mutate only our CPU shadow in that case: doing so would create a
     * CPU/GPU split-brain state that VirtualBox never creates.
     */
    if (valid && d3d11_stretch) {
        if ((!vmsvga3d_dxvk_d3d11_surface_resident(src_surface->dxvk_surface) &&
             !vmsvga3d_d3d10_copy_surface_materialize_live(
                 s, src_surface, VMSVGA3D_D3D10_CREATE_TEXTURE)) ||
            (!vmsvga3d_dxvk_d3d11_surface_resident(dst_surface->dxvk_surface) &&
             !vmsvga3d_d3d10_copy_surface_materialize_live(
                 s, dst_surface, VMSVGA3D_D3D10_CREATE_TEXTURE))) {
            valid = false;
        }
    }

    if (valid && !d3d11_stretch &&
        (!vmsvga3d_d3d9_runtime_surface_info(s, src_surface, &src_info) ||
         !vmsvga3d_d3d9_runtime_surface_info(s, dst_surface, &dst_info) ||
         !vmsvga3d_d3d9_stretch_blt_plan(&src_info, &dst_info, body->mode,
                                         &transfer_plan))) {
        valid = false;
    }

    if (valid && !d3d11_stretch) {
        accel = vmsvga3d_d3d9_try_stretch_blt(s, body, &transfer_plan);
        if (accel == VMSVGA3D_D3D9_ACCEL_FAILED) {
            valid = false;
        } else if (accel == VMSVGA3D_D3D9_ACCEL_UNAVAILABLE &&
                   body->mode != SVGA3D_STRETCH_BLT_POINT) {
            valid = false;
        }
    }

    if (valid && !d3d11_stretch &&
        accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
        uint32_t src_subresource = (uint32_t)(src_image - src_surface->mips);
        uint32_t dst_subresource = (uint32_t)(dst_image - dst_surface->mips);

        valid = vmsvga3d_surface_readback_to_shadow(
            s, src_surface, src_image, src_subresource);
        if (valid && (src_surface != dst_surface || src_image != dst_image)) {
            valid = vmsvga3d_surface_readback_to_shadow(
                s, dst_surface, dst_image, dst_subresource);
        }
    }

    if (valid && !d3d11_stretch &&
        accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
        valid = vmsvga3d_surface_stretchblt_point(
            src_surface, src_image, dst_surface, dst_image, &body->boxSrc,
            &body->boxDest, NULL, 0, false, &scratch_size);
    }

    if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
        scratch_size != 0) {
        scratch = g_try_malloc(scratch_size);
        if (scratch == NULL) {
            valid = false;
        }
    }

    if (valid && !d3d11_stretch &&
        accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
        valid = vmsvga3d_surface_stretchblt_point(
            src_surface, src_image, dst_surface, dst_image, &body->boxSrc,
            &body->boxDest, scratch, scratch_size, true, NULL);
    }

    if (valid && !d3d11_stretch &&
        accel != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
        vmsvga3d_d3d9_runtime_sync_surface_from_cpu(s, dst_surface);
    }

    if (valid && !d3d11_stretch) {
        if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
            vmsvga3d_trace_vgpu9_surface_write(
                s, body->dest.sid, VMSVGA3D_TRACE_VGPU9_WRITE_STRETCH,
                SVGA3D_INVALID_ID, false);
        }
    }

    g_free(scratch);
    g_free(payload);

    return true;
}

static void vmsvga3d_clip_present_rect(const SVGA3dCopyRect *rect,
                                         const SVGA3dSize *src_size,
                                         uint32_t dst_width,
                                         uint32_t dst_height,
                                         SVGA3dCopyRect *clipped)
{
    uint32_t max_width;
    uint32_t max_height;

    *clipped = *rect;
    if (rect->w == 0 || rect->h == 0 || rect->srcx >= src_size->width ||
        rect->srcy >= src_size->height || rect->x >= dst_width ||
        rect->y >= dst_height) {
        clipped->w = 0;
        clipped->h = 0;
        return;
    }

    max_width = MIN(src_size->width - rect->srcx, dst_width - rect->x);
    max_height = MIN(src_size->height - rect->srcy, dst_height - rect->y);
    clipped->w = MIN(rect->w, max_width);
    clipped->h = MIN(rect->h, max_height);
}

static bool vmsvga3d_present_format(
    VMSVGA3DSurface *surface, const struct svga3d_surface_desc **desc_out)
{
    const struct svga3d_surface_desc *desc;

    if (surface == NULL || surface->mip_count == 0 ||
        surface->multisample_count > 1) {
        return false;
    }

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
    }

    if (desc_out != NULL) {
        *desc_out = desc;
    }

    return true;
}

static uint8_t vmsvga3d_present_channel(uint64_t pixel, uint8_t depth,
                                         uint8_t offset)
{
    uint64_t mask;
    uint64_t value;

    if (depth == 0) {
        return 0;
    }

    if (depth >= 64 || offset >= 64 || depth > 64 - offset) {
        return 0;
    }

    mask = (UINT64_C(1) << depth) - 1;
    value = (pixel >> offset) & mask;

    return (uint8_t)((value * 255 + mask / 2) / mask);
}

static uint32_t vmsvga3d_present_scanout_pixel(uint8_t red, uint8_t green,
                                                uint8_t blue,
                                                uint32_t depth)
{
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
    }
}

static uint64_t vmsvga3d_present_load_pixel(const uint8_t *src,
                                             uint32_t bytes)
{
    uint64_t pixel = 0;
    uint32_t i;

    for (i = 0; i < bytes; i++) {
        pixel |= (uint64_t)src[i] << (i * 8);
    }

    return pixel;
}

static bool vmsvga3d_present_rect_to_buffer(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, const struct svga3d_surface_desc *desc,
    const SVGA3dCopyRect *rect, uint8_t *dst_base, size_t dst_size,
    uint32_t dst_width, uint32_t dst_height, uint32_t dst_depth,
    uint32_t dst_pitch, uint64_t dirty_base_offset, bool mark_vram_dirty,
    bool execute)
{
    SVGA3dCopyRect clipped;
    uint32_t dst_bypp = vmsvga_bytes_per_pixel(dst_depth);
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
        dst_bypp == 0 || dst_base == NULL || dst_size == 0 ||
        surface == NULL || image == NULL || desc == NULL || rect == NULL) {
        return false;
    }

    vmsvga3d_clip_present_rect(rect, &image->size, dst_width, dst_height,
                                &clipped);
    if (clipped.w == 0 || clipped.h == 0) {
        return true;
    }

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

    if (src_end > image->data_size || dst_end > dst_size ||
        src_row_bytes > image->pitch || dst_row_bytes > dst_pitch) {
        return false;
    }

    if (!execute) {
        return true;
    }

    if (dst_depth == 32 &&
        (surface->format == SVGA3D_X8R8G8B8 ||
         surface->format == SVGA3D_A8R8G8B8)) {
        for (row = 0; row < clipped.h; row++) {
            const uint8_t *src = image->data + src_offset +
                                 (uint64_t)row * image->pitch;
            uint8_t *dst = dst_base + dst_offset + (uint64_t)row * dst_pitch;
            memcpy(dst, src, (size_t)src_row_bytes);
        }
    } else if (dst_depth == 16 && surface->format == SVGA3D_R5G6B5) {
        for (row = 0; row < clipped.h; row++) {
            const uint8_t *src = image->data + src_offset +
                                 (uint64_t)row * image->pitch;
            uint8_t *dst = dst_base + dst_offset + (uint64_t)row * dst_pitch;
            memcpy(dst, src, (size_t)src_row_bytes);
        }
    } else if (dst_depth == 15 &&
               (surface->format == SVGA3D_X1R5G5B5 ||
                surface->format == SVGA3D_A1R5G5B5)) {
        for (row = 0; row < clipped.h; row++) {
            const uint8_t *src = image->data + src_offset +
                                 (uint64_t)row * image->pitch;
            uint8_t *dst = dst_base + dst_offset + (uint64_t)row * dst_pitch;
            memcpy(dst, src, (size_t)src_row_bytes);
        }
    } else {
        for (row = 0; row < clipped.h; row++) {
            const uint8_t *src = image->data + src_offset +
                                 (uint64_t)row * image->pitch;
            uint8_t *dst = dst_base + dst_offset + (uint64_t)row * dst_pitch;
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
            }
        }
    }

    if (mark_vram_dirty) {
        vmsvga_mark_vram_dirty_rect(s, dirty_base_offset, dst_pitch, dst_bypp,
                                    clipped.x, clipped.y, clipped.w, clipped.h);
    }

    vmsvga_damage_add_visible(s, clipped.x, clipped.y, clipped.w, clipped.h);

    return true;
}

static bool vmsvga3d_present_rect(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, const struct svga3d_surface_desc *desc,
    const SVGA3dCopyRect *rect, bool execute)
{
    return vmsvga3d_present_rect_to_buffer(
        s, surface, image, desc, rect, s->vga.vram_ptr, s->vga.vram_size,
        vmsvga_active_width(s), vmsvga_active_height(s),
        vmsvga_active_depth(s), vmsvga_stride(s), 0, true, execute);
}

static bool vmsvga3d_present_screen_rect(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, const struct svga3d_surface_desc *desc,
    const SVGA3dCopyRect *rect, bool execute)
{
    uint8_t *screen_base;
    size_t screen_size;
    uint32_t screen_stride;
    uint64_t dirty_base = 0;
    bool mark_dirty;

    if (s == NULL ||
        !vmsvga_screen_storage(s, &screen_base, &screen_size, &screen_stride)) {
        return false;
    }

    mark_dirty = s->screen_backing_valid;

    if (mark_dirty) {
        dirty_base = s->screen_backing_offset;
    }

    return vmsvga3d_present_rect_to_buffer(
        s, surface, image, desc, rect, screen_base, screen_size,
        s->screen_width, s->screen_height, 32, screen_stride, dirty_base,
        mark_dirty, execute);
}

static bool vmsvga3d_present_screen_rect_damage_only(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, const struct svga3d_surface_desc *desc,
    const SVGA3dCopyRect *rect)
{
    SVGA3dCopyRect clipped;
    uint8_t *screen_base;
    size_t screen_size;
    uint32_t screen_stride;
    uint64_t dirty_base = 0;
    bool mark_dirty;

    if (s == NULL || surface == NULL || image == NULL || desc == NULL ||
        rect == NULL ||
        !vmsvga_screen_storage(s, &screen_base, &screen_size, &screen_stride)) {
        return false;
    }

    if (!vmsvga3d_present_rect_to_buffer(
            s, surface, image, desc, rect, screen_base, screen_size,
            s->screen_width, s->screen_height, 32, screen_stride, 0, false,
            false)) {
        return false;
    }

    vmsvga3d_clip_present_rect(rect, &image->size, s->screen_width,
                                s->screen_height, &clipped);
    if (clipped.w == 0 || clipped.h == 0) {
        return true;
    }

    mark_dirty = s->screen_backing_valid;
    if (mark_dirty) {
        dirty_base = s->screen_backing_offset;
        vmsvga_mark_vram_dirty_rect(s, dirty_base, screen_stride, 4,
                                    clipped.x, clipped.y, clipped.w,
                                    clipped.h);
    }

    vmsvga_damage_add_visible(s, clipped.x, clipped.y, clipped.w, clipped.h);
    return true;
}

static bool vmsvga3d_present_scaled_screen_rect(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, const struct svga3d_surface_desc *desc,
    const SVGASignedRect *source, const SVGASignedRect *destination,
    const SVGASignedRect *clip, bool execute)
{
    int64_t src_width, src_height, dst_width, dst_height;
    int64_t local_left = 0, local_top = 0, local_right, local_bottom;
    int64_t dst_left, dst_top, dst_right, dst_bottom;
    uint32_t dst_depth = 32;
    uint32_t dst_bypp = 4;
    uint32_t dst_pitch;
    uint32_t src_bypp;
    uint32_t y;
    uint8_t *screen_base;
    size_t screen_size;

    if (s == NULL || surface == NULL || image == NULL || desc == NULL ||
        source == NULL || destination == NULL ||
        !vmsvga_screen_storage(s, &screen_base, &screen_size, &dst_pitch)) {
        return false;
    }

    (void)screen_size;
    src_width = (int64_t)source->right - source->left;
    src_height = (int64_t)source->bottom - source->top;
    dst_width = (int64_t)destination->right - destination->left;
    dst_height = (int64_t)destination->bottom - destination->top;

    if (src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0 ||
        source->left < 0 || source->top < 0 ||
        source->right > (int32_t)image->size.width ||
        source->bottom > (int32_t)image->size.height) {
        return false;
    }

    src_bypp = desc->bytes_per_block;
    if (src_bypp == 0 || desc->block_size.width != 1 ||
        desc->block_size.height != 1 || desc->block_size.depth != 1) {
        return false;
    }

    local_right = dst_width;
    local_bottom = dst_height;
    if (clip != NULL) {
        local_left = MAX(local_left, (int64_t)clip->left);
        local_top = MAX(local_top, (int64_t)clip->top);
        local_right = MIN(local_right, (int64_t)clip->right);
        local_bottom = MIN(local_bottom, (int64_t)clip->bottom);
    }

    local_left = MAX(local_left, -(int64_t)destination->left);
    local_top = MAX(local_top, -(int64_t)destination->top);
    local_right = MIN(local_right,
                      (int64_t)s->screen_width - destination->left);
    local_bottom = MIN(local_bottom,
                       (int64_t)s->screen_height - destination->top);

    if (local_right <= local_left || local_bottom <= local_top) {
        return true;
    }

    dst_left = (int64_t)destination->left + local_left;
    dst_top = (int64_t)destination->top + local_top;
    dst_right = (int64_t)destination->left + local_right;
    dst_bottom = (int64_t)destination->top + local_bottom;

    if (dst_left < 0 || dst_top < 0 || dst_right > s->screen_width ||
        dst_bottom > s->screen_height) {
        return false;
    }

    if (!execute) {
        return true;
    }

    for (y = (uint32_t)local_top; y < (uint32_t)local_bottom; y++) {
        uint32_t src_y = (uint32_t)((int64_t)source->top +
            ((int64_t)y * src_height) / dst_height);
        uint8_t *dst = screen_base +
            (uint64_t)(destination->top + (int32_t)y) * dst_pitch +
            (uint64_t)(destination->left + (int32_t)local_left) * dst_bypp;
        uint32_t x;
        for (x = (uint32_t)local_left; x < (uint32_t)local_right; x++) {
            uint32_t src_x = (uint32_t)((int64_t)source->left +
                ((int64_t)x * src_width) / dst_width);
            uint64_t src_offset = (uint64_t)src_y * image->pitch +
                                  (uint64_t)src_x * src_bypp;
            uint64_t pixel;
            uint8_t blue, green, red;
            uint32_t scanout;
            if (src_offset + src_bypp > image->data_size) {
                return false;
            }
            pixel = vmsvga3d_present_load_pixel(image->data + src_offset, src_bypp);
            blue = vmsvga3d_present_channel(pixel, desc->bitDepth.blue,
                                            desc->bitOffset.blue);
            green = vmsvga3d_present_channel(pixel, desc->bitDepth.green,
                                             desc->bitOffset.green);
            red = vmsvga3d_present_channel(pixel, desc->bitDepth.red,
                                           desc->bitOffset.red);
            scanout = vmsvga3d_present_scanout_pixel(red, green, blue, dst_depth);
            vmsvga_store_pixel(dst, dst_bypp, scanout);
            dst += dst_bypp;
        }
    }

    vmsvga_screen_mark_dirty(s, (uint32_t)dst_left, (uint32_t)dst_top,
                             (uint32_t)(dst_right - dst_left),
                             (uint32_t)(dst_bottom - dst_top));
    vmsvga_damage_add_visible(s, (uint32_t)dst_left, (uint32_t)dst_top,
                              (uint32_t)(dst_right - dst_left),
                              (uint32_t)(dst_bottom - dst_top));

    return true;
}

static bool vmsvga3d_screen_blit_copy_rect(
    const SVGASignedRect *source, const SVGASignedRect *destination,
    const SVGASignedRect *clip, SVGA3dCopyRect *copy, bool *empty)
{
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

typedef struct vmsvga3d_screen_blit_trace_s {
    uint8_t *before;
    size_t row_bytes;
    size_t snapshot_size;
    size_t source_bytes;
    uint32_t width;
    uint32_t height;
    uint32_t source_hash;
    uint32_t screen_before_hash;
    uint32_t damage_before;
    bool enabled;
    bool captured;
    bool source_hash_valid;
} VMSVGA3DScreenBlitTrace;

static void vmsvga3d_screen_blit_trace_capture(
    struct vmsvga_state_s *s, const VMSVGA3DSurfaceImage *image,
    VMSVGA3DScreenBlitTrace *trace)
{
    uint8_t *screen_base;
    size_t screen_size;
    uint32_t screen_stride;
    uint64_t required;
    uint32_t row;

    if (!VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
        return;
    }

    memset(trace, 0, sizeof(*trace));
    trace->enabled = true;
    trace->damage_before = s != NULL ? s->damage_count : 0;
    if (image != NULL && image->data != NULL && image->data_size != 0) {
        trace->source_hash =
            vmsvga_gmr_diag_hash(image->data, image->data_size);
        trace->source_bytes = image->data_size;
        trace->source_hash_valid = true;
    }

    if (s == NULL || s->screen_width == 0 || s->screen_height == 0 ||
        !vmsvga_screen_storage(s, &screen_base, &screen_size, &screen_stride) ||
        s->screen_width > UINT32_MAX / 4) {
        return;
    }

    trace->row_bytes = (size_t)s->screen_width * 4;
    if (screen_stride < trace->row_bytes ||
        s->screen_height > SIZE_MAX / trace->row_bytes) {
        return;
    }

    required = (uint64_t)(s->screen_height - 1) * screen_stride +
               trace->row_bytes;
    trace->snapshot_size = trace->row_bytes * s->screen_height;
    if (required > screen_size || trace->snapshot_size == 0) {
        return;
    }

    trace->before = g_try_malloc(trace->snapshot_size);
    if (trace->before == NULL) {
        return;
    }

    trace->width = s->screen_width;
    trace->height = s->screen_height;
    for (row = 0; row < trace->height; row++) {
        memcpy(trace->before + (size_t)row * trace->row_bytes,
               screen_base + (uint64_t)row * screen_stride,
               trace->row_bytes);
    }
    trace->screen_before_hash =
        vmsvga_gmr_diag_hash(trace->before, trace->snapshot_size);
    trace->captured = true;
}

static void vmsvga3d_screen_blit_trace_report(
    struct vmsvga_state_s *s, uint32_t sid,
    const VMSVGA3DScreenBlitTrace *trace)
{
    uint8_t *screen_base;
    size_t screen_size;
    uint32_t screen_stride;
    uint32_t screen_after_hash;
    uint32_t region_before_hash;
    uint32_t region_after_hash;
    uint32_t min_x;
    uint32_t min_y;
    uint32_t max_x;
    uint32_t max_y;
    uint32_t changed_rows;
    uint64_t raw_changed_pixels;
    uint64_t rgb_changed_pixels;
    uint64_t required;
    uint32_t row;
    bool bbox_valid;

    if (!trace->enabled) {
        return;
    }

    screen_after_hash = 2166136261u;
    region_before_hash = 2166136261u;
    region_after_hash = 2166136261u;
    min_x = UINT32_MAX;
    min_y = UINT32_MAX;
    max_x = 0;
    max_y = 0;
    changed_rows = 0;
    raw_changed_pixels = 0;
    rgb_changed_pixels = 0;
    bbox_valid = false;

    if (!trace->captured || s == NULL || s->screen_width != trace->width ||
        s->screen_height != trace->height ||
        !vmsvga_screen_storage(s, &screen_base, &screen_size, &screen_stride) ||
        screen_stride < trace->row_bytes) {
        fprintf(stderr,
                "VMVGA-SCREEN-BLIT3D-DIFF sid=%u capture=0 "
                "source-hash=0x%08x source-valid=%u source-bytes=0x%zx "
                "damage-before=%u damage-after=%u\n",
                sid, trace->source_hash, trace->source_hash_valid,
                trace->source_bytes, trace->damage_before,
                s != NULL ? s->damage_count : 0);
        return;
    }

    required = (uint64_t)(trace->height - 1) * screen_stride +
               trace->row_bytes;
    if (required > screen_size) {
        fprintf(stderr,
                "VMVGA-SCREEN-BLIT3D-DIFF sid=%u capture=0 reason=screen-range "
                "source-hash=0x%08x source-valid=%u source-bytes=0x%zx "
                "damage-before=%u damage-after=%u\n",
                sid, trace->source_hash, trace->source_hash_valid,
                trace->source_bytes, trace->damage_before, s->damage_count);
        return;
    }

    for (row = 0; row < trace->height; row++) {
        const uint8_t *before_row =
            trace->before + (size_t)row * trace->row_bytes;
        const uint8_t *after_row = screen_base + (uint64_t)row * screen_stride;
        uint32_t column;
        bool row_rgb_changed = false;

        screen_after_hash = vmsvga_gmr_diag_hash_extend(
            screen_after_hash, after_row, trace->row_bytes);
        for (column = 0; column < trace->width; column++) {
            const uint8_t *before_pixel = before_row + (size_t)column * 4;
            const uint8_t *after_pixel = after_row + (size_t)column * 4;
            bool raw_changed =
                before_pixel[0] != after_pixel[0] ||
                before_pixel[1] != after_pixel[1] ||
                before_pixel[2] != after_pixel[2] ||
                before_pixel[3] != after_pixel[3];
            bool rgb_changed =
                before_pixel[0] != after_pixel[0] ||
                before_pixel[1] != after_pixel[1] ||
                before_pixel[2] != after_pixel[2];

            if (raw_changed) {
                raw_changed_pixels++;
            }
            if (!rgb_changed) {
                continue;
            }

            rgb_changed_pixels++;
            row_rgb_changed = true;
            bbox_valid = true;
            min_x = MIN(min_x, column);
            min_y = MIN(min_y, row);
            max_x = MAX(max_x, column);
            max_y = MAX(max_y, row);
        }
        if (row_rgb_changed) {
            changed_rows++;
        }
    }

    if (bbox_valid) {
        size_t region_row_bytes = (size_t)(max_x - min_x + 1) * 4;

        for (row = min_y; ; row++) {
            const uint8_t *before_row =
                trace->before + (size_t)row * trace->row_bytes +
                (size_t)min_x * 4;
            const uint8_t *after_row =
                screen_base + (uint64_t)row * screen_stride +
                (size_t)min_x * 4;

            region_before_hash = vmsvga_gmr_diag_hash_extend(
                region_before_hash, before_row, region_row_bytes);
            region_after_hash = vmsvga_gmr_diag_hash_extend(
                region_after_hash, after_row, region_row_bytes);
            if (row == max_y) {
                break;
            }
        }
    }

    fprintf(stderr,
            "VMVGA-SCREEN-BLIT3D-DIFF sid=%u capture=1 "
            "source-hash=0x%08x source-valid=%u source-bytes=0x%zx "
            "screen-before=0x%08x screen-after=0x%08x raw-pixels=%" PRIu64
            " rgb-pixels=%" PRIu64 " rgb-rows=%u bbox-valid=%u "
            "bbox=%u,%u-%u,%u region-before=0x%08x region-after=0x%08x "
            "damage-before=%u damage-after=%u\n",
            sid, trace->source_hash, trace->source_hash_valid,
            trace->source_bytes, trace->screen_before_hash, screen_after_hash,
            raw_changed_pixels, rgb_changed_pixels, changed_rows, bbox_valid,
            bbox_valid ? min_x : 0, bbox_valid ? min_y : 0,
            bbox_valid ? max_x + 1 : 0, bbox_valid ? max_y + 1 : 0,
            bbox_valid ? region_before_hash : 0,
            bbox_valid ? region_after_hash : 0, trace->damage_before,
            s->damage_count);
}

static void vmsvga3d_screen_blit_trace_cleanup(
    VMSVGA3DScreenBlitTrace *trace)
{
    g_free(trace->before);
    trace->before = NULL;
}

static bool vmsvga3d_handle_blit_surface_to_screen(
    struct vmsvga_state_s *s, uint32_t cmd, int32_t *len,
    uint32_t fifo_start)
{
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
    VMSVGA3DScreenBlitTrace trace_diff;
    bool trace_3d = VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D);
    bool valid = true;

    if (trace_3d) {
        memset(&trace_diff, 0, sizeof(trace_diff));
    }

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
    vmsvga_screen_record_surface_to_screen(s);
    state = s->svga3d;

    if (state == NULL || body->srcImage.sid >= SVGA3D_MAX_SURFACE_IDS ||
        !s->active_valid) {
        valid = false;
    }

    surface = valid ? state->surfaces[body->srcImage.sid] : NULL;
    if (valid && surface != NULL) {
        if (trace_3d) {
            VMSVGA3DD3D9TransferSurface surface_info;
            bool d3d9_info =
                vmsvga3d_d3d9_runtime_surface_info(s, surface, &surface_info);
            bool d3d11_resident =
                vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface);

            fprintf(stderr,
                    "VMVGA-SCREEN-BLIT3D sid=%u face=%u mip=%u "
                    "src=%d,%d-%d,%d dst-screen=%u dst=%d,%d-%d,%d clips=%u "
                    "screen-defined=%u surface-active=%u d3d9-info=%u "
                    "d3d9-resident=%u d3d9-bounce=%u d3d11-resident=%u\n",
                    body->srcImage.sid, body->srcImage.face,
                    body->srcImage.mipmap, body->srcRect.left, body->srcRect.top,
                    body->srcRect.right, body->srcRect.bottom, body->destScreenId,
                    body->destRect.left, body->destRect.top, body->destRect.right,
                    body->destRect.bottom, clip_count, s->screen_defined,
                    surface->legacy_active, d3d9_info,
                    d3d9_info ? surface_info.resident : 0,
                    d3d9_info ? surface_info.has_bounce : 0, d3d11_resident);
            for (i = 0; i < clip_count; i++) {
                fprintf(stderr,
                        "VMVGA-SCREEN-BLIT3D clip[%u]=%d,%d-%d,%d\n", i,
                        clips[i].left, clips[i].top, clips[i].right,
                        clips[i].bottom);
            }
        }

        accel = vmsvga3d_d3d9_try_screen_blit(s, body, clips, clip_count);
        if (trace_3d) {
            fprintf(stderr, "VMVGA-SCREEN-BLIT3D accel=%s\n",
                    vmsvga3d_d3d9_accel_result_name(accel));
        }
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
        bool scaling = source_width != destination_width ||
                       source_height != destination_height;

        if (!vmsvga_screen_is_primary(s, body->destScreenId) ||
            source_width <= 0 || source_height <= 0 ||
            destination_width <= 0 || destination_height <= 0 ||
            !vmsvga3d_present_format(surface, &desc) || image == NULL) {
            valid = false;
        }
        /* The CPU-backed Screen Object presenter reads image->data.  Synchronize
         * whichever renderer currently owns the surface before conversion,
         * clipping or scaling.  D3D9 and D3D11 residency are mutually exclusive,
         * so only one backend can supply authoritative pixels here.
         */
        if (valid) {
            valid = vmsvga3d_surface_readback_to_shadow(
                s, surface, image, 0);
            if (trace_3d) {
                fprintf(stderr,
                        "VMVGA-SCREEN-BLIT3D readback sid=%u result=%s\n",
                        body->srcImage.sid, valid ? "complete" : "failed");
            }
            if (trace_3d && valid) {
                vmsvga3d_screen_blit_trace_capture(s, image, &trace_diff);
            }
        }

        copy_count = clip_count != 0 ? clip_count : 1;
        if (valid && !scaling) {
            copies = g_try_new0(SVGA3dCopyRect, copy_count);
            if (copies == NULL) {
                valid = false;
            }
            for (i = 0; valid && i < copy_count; i++) {
                bool empty;
                const SVGASignedRect *clip = clip_count != 0 ? &clips[i] : NULL;
                if (!vmsvga3d_screen_blit_copy_rect(&body->srcRect, &body->destRect,
                                                     clip, &copies[i], &empty)) {
                    valid = false;
                } else if (empty) {
                    copies[i].w = 0;
                    copies[i].h = 0;
                }
                if (trace_3d && valid) {
                    fprintf(stderr,
                            "VMVGA-SCREEN-BLIT3D copy[%u] src=%u,%u "
                            "dst=%u,%u size=%ux%u empty=%u\n",
                            i, copies[i].srcx, copies[i].srcy, copies[i].x,
                            copies[i].y, copies[i].w, copies[i].h, empty);
                }
            }
            for (i = 0; valid && i < copy_count; i++) {
                if (copies[i].w != 0 && copies[i].h != 0) {
                    valid = vmsvga3d_present_screen_rect(
                        s, surface, image, desc, &copies[i], false);
                }
            }
            for (i = 0; valid && i < copy_count; i++) {
                if (copies[i].w != 0 && copies[i].h != 0) {
                    valid = vmsvga3d_present_screen_rect(
                        s, surface, image, desc, &copies[i], true);
                }
            }
        } else if (valid) {
            for (i = 0; valid && i < copy_count; i++) {
                const SVGASignedRect *clip = clip_count != 0 ? &clips[i] : NULL;
                valid = vmsvga3d_present_scaled_screen_rect(
                    s, surface, image, desc, &body->srcRect, &body->destRect, clip,
                    false);
            }
            for (i = 0; valid && i < copy_count; i++) {
                const SVGASignedRect *clip = clip_count != 0 ? &clips[i] : NULL;
                valid = vmsvga3d_present_scaled_screen_rect(
                    s, surface, image, desc, &body->srcRect, &body->destRect, clip,
                    true);
            }
        }
    } else {
        copy_count = 0;
    }

    if (trace_3d) {
        vmsvga3d_screen_blit_trace_report(s, body->srcImage.sid, &trace_diff);
        vmsvga3d_screen_blit_trace_cleanup(&trace_diff);

        fprintf(stderr,
                "VMVGA-SCREEN-BLIT3D result sid=%u valid=%u accel=%s copies=%u\n",
                body->srcImage.sid, valid,
                vmsvga3d_d3d9_accel_result_name(accel), copy_count);
    }

    if (trace_3d && valid) {
        vmsvga3d_trace_vgpu9_present(s, body->srcImage.sid, "screen-blit");
    }

    g_free(copies);
    g_free(payload);

    return true;
}

static bool vmsvga3d_handle_present(struct vmsvga_state_s *s,
                                     uint32_t cmd, int32_t *len,
                                     uint32_t fifo_start)
{
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
    }

    if (size < sizeof(*body) ||
        (size - sizeof(*body)) % sizeof(SVGA3dCopyRect) != 0) {
        g_free(payload);
        return true;
    }

    body = payload;
    rects = (SVGA3dCopyRect *)(body + 1);
    rect_count = (size - sizeof(*body)) / sizeof(*rects);
    state = s->svga3d;
    if (state == NULL || body->sid >= SVGA3D_MAX_SURFACE_IDS ||
        !s->active_valid) {
        valid = false;
    }

    surface = valid ? state->surfaces[body->sid] : NULL;
    if (valid && surface != NULL) {
        accel = vmsvga3d_d3d9_try_present(s, body, rects, rect_count);
        if (accel == VMSVGA3D_D3D9_ACCEL_FAILED) {
            valid = false;
        }
    } else {
        valid = false;
    }

    image = surface != NULL && surface->mip_count != 0 ? &surface->mips[0] : NULL;
    if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
        rect_count == 0) {
        memset(&full_screen_rect, 0, sizeof(full_screen_rect));
        if (s->screen_defined) {
            full_screen_rect.w = s->screen_width;
            full_screen_rect.h = s->screen_height;
        } else {
            full_screen_rect.w = vmsvga_active_width(s);
            full_screen_rect.h = vmsvga_active_height(s);
        }
        rects = &full_screen_rect;
        rect_count = 1;
    }

    if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
        (!vmsvga3d_present_format(surface, &desc) || image == NULL)) {
        valid = false;
    }
    /* PRESENT is translated to SurfaceBlitToScreen by VirtualBox.  The CPU
     * fallback must sample whichever renderer currently owns the surface, not
     * only an ID3D11Resource. */
    if (valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
        !vmsvga3d_surface_readback_to_shadow(s, surface, image, 0)) {
        valid = false;
    }

    for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
                i < rect_count; i++) {
        if (s->screen_defined) {
            valid = vmsvga3d_present_screen_rect(
                s, surface, image, desc, &rects[i], false);
        } else {
            valid = vmsvga3d_present_rect(
                s, surface, image, desc, &rects[i], false);
        }
    }

    for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
                i < rect_count; i++) {
        if (s->screen_defined) {
            valid = vmsvga3d_present_screen_rect(
                s, surface, image, desc, &rects[i], true);
        } else {
            valid = vmsvga3d_present_rect(
                s, surface, image, desc, &rects[i], true);
        }
    }

    if (valid) {
        if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
            vmsvga3d_trace_vgpu9_present(s, body->sid, "present");
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_surface_dma_box(struct vmsvga_state_s *s,
                                     VMSVGA3DSurface *surface,
                                     VMSVGA3DSurfaceImage *image,
                                     const SVGAGuestImage *guest,
                                     SVGA3dTransferType transfer,
                                     const SVGA3dCopyBox *box,
                                     uint32_t maximum_offset,
                                     bool execute)
{
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
    }

    if (surface->multisample_count > 1) {
        return false;
    }

    desc = svga3dsurface_get_desc(surface->format);
    if (desc->format != surface->format || desc->bytes_per_block == 0 ||
        desc->block_size.width == 0 || desc->block_size.height == 0 ||
        desc->block_size.depth == 0) {
        return false;
    }

    block_width = desc->block_size.width;
    block_height = desc->block_size.height;
    block_depth = desc->block_size.depth;
    bytes_per_block = desc->bytes_per_block;

    if (clipped.x % block_width != 0 || clipped.y % block_height != 0 ||
        clipped.z % block_depth != 0 || clipped.srcx % block_width != 0 ||
        clipped.srcy % block_height != 0 ||
        clipped.srcz % block_depth != 0) {
        return false;
    }

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
    }

    guest_pitch = guest->pitch != 0 ? guest->pitch : image->pitch;
    if (guest_pitch == 0) {
        return false;
    }

    if ((uint64_t)guest_block_x * bytes_per_block + row_bytes > guest_pitch) {
        return false;
    }

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
            }

            guest_offset = (uint64_t)guest->ptr.offset + guest_relative;

            if (guest_relative > maximum_offset ||
                row_bytes > (uint64_t)maximum_offset - guest_relative ||
                guest_offset > UINT32_MAX || host_offset > image->data_size ||
                row_bytes > (uint64_t)image->data_size - host_offset ||
                !vmsvga_gmr_validate_range(s, guest->ptr.gmrId,
                                           (uint32_t)guest_offset,
                                           (size_t)row_bytes)) {
                return false;
            }

            if (!execute) {
                continue;
            }

            if (transfer == SVGA3D_WRITE_HOST_VRAM) {
                if (!vmsvga_gmr_read(s, guest->ptr.gmrId, (uint32_t)guest_offset,
                                     image->data + host_offset, (size_t)row_bytes)) {
                    return false;
                }
            } else if (transfer == SVGA3D_READ_HOST_VRAM) {
                if (!vmsvga_gmr_write(s, guest->ptr.gmrId, (uint32_t)guest_offset,
                                      image->data + host_offset, (size_t)row_bytes)) {
                    return false;
                }
            } else {
                return false;
            }
        }
    }

    return true;
}

static bool vmsvga3d_d3d11_readback_shadow_image(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, uint32_t subresource)
{
    uint32_t row_count;
    uint32_t depth_count;

    if (s == NULL || surface == NULL || image == NULL ||
        surface->dxvk_surface == NULL || image->data == NULL ||
        image->pitch == 0 || image->plane_size == 0 || image->data_size == 0 ||
        image->plane_size % image->pitch != 0 ||
        image->data_size % image->plane_size != 0) {
        return false;
    }

    if (!vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface)) {
        return true;
    }

    row_count = image->plane_size / image->pitch;
    depth_count = image->data_size / image->plane_size;

    return vmsvga3d_dxvk_d3d11_readback_subresource(
        s->dxvk, surface->dxvk_surface, subresource, image->data, image->pitch,
        image->pitch, row_count, image->plane_size, depth_count);
}

static bool vmsvga3d_surface_dma_d3d11_upload_box(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, uint32_t subresource,
    const SVGA3dCopyBox *box)
{
    const struct svga3d_surface_desc *desc;
    SVGA3dCopyBox clipped;
    VMSVGA3DD3D10Box native_box;
    uint64_t data_offset;
    uint32_t host_block_x;
    uint32_t host_block_y;
    uint32_t host_block_z;

    if (s == NULL || surface == NULL || image == NULL || box == NULL ||
        surface->dxvk_surface == NULL || image->data == NULL) {
        return false;
    }

    if (!vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface)) {
        return true;
    }

    /* VBox's D3D11 SurfaceDMA callback rejects hardware buffer resources. */
    if (surface->format == SVGA3D_BUFFER) {
        return false;
    }

    if (!vmsvga3d_clip_dma_box(box, &image->size, &clipped)) {
        return false;
    }

    desc = svga3dsurface_get_desc(surface->format);

    if (desc->format != surface->format || desc->bytes_per_block == 0 ||
        desc->block_size.width == 0 || desc->block_size.height == 0 ||
        desc->block_size.depth == 0 ||
        clipped.x % desc->block_size.width != 0 ||
        clipped.y % desc->block_size.height != 0 ||
        clipped.z % desc->block_size.depth != 0) {
        return false;
    }

    host_block_x = clipped.x / desc->block_size.width;
    host_block_y = clipped.y / desc->block_size.height;
    host_block_z = clipped.z / desc->block_size.depth;
    data_offset = (uint64_t)host_block_x * desc->bytes_per_block +
                  (uint64_t)host_block_y * image->pitch +
                  (uint64_t)host_block_z * image->plane_size;

    if (data_offset >= image->data_size) {
        return false;
    }

    native_box.left = clipped.x;
    native_box.top = clipped.y;
    native_box.front = clipped.z;
    native_box.right = clipped.x + clipped.w;
    native_box.bottom = clipped.y + clipped.h;
    native_box.back = clipped.z + clipped.d;

    return vmsvga3d_dxvk_d3d11_update_subresource(
        s->dxvk, surface->dxvk_surface, subresource, &native_box,
        image->data + data_offset, image->pitch, image->plane_size);
}

static bool vmsvga3d_handle_surface_dma(struct vmsvga_state_s *s,
                                        uint32_t cmd, int32_t *len,
                                        uint32_t fifo_start)
{
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
    uint32_t subresource = 0;
    uint32_t i;
    bool valid = true;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size < sizeof(*body)) {
        g_free(payload);
        return true;
    }

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
        }
    } else if (trailing % sizeof(SVGA3dCopyBox) != 0) {
        valid = false;
    }

    box_bytes = valid ? trailing : 0;
    box_count = box_bytes / sizeof(SVGA3dCopyBox);
    boxes = (SVGA3dCopyBox *)(body + 1);
    state = s->svga3d;

    if (valid && (body->transfer != SVGA3D_WRITE_HOST_VRAM &&
                  body->transfer != SVGA3D_READ_HOST_VRAM)) {
        valid = false;
    }

    if (valid && (state == NULL || body->host.sid >= SVGA3D_MAX_SURFACE_IDS)) {
        valid = false;
    }

    surface = valid ? state->surfaces[body->host.sid] : NULL;

    if (valid &&
        (!vmsvga3d_surface_image(surface, &body->host, &image) ||
         !vmsvga3d_d3d9_runtime_surface_info(s, surface, &surface_info) ||
         !vmsvga3d_d3d9_dma_plan(&surface_info, body->transfer, true,
                                 &transfer_plan))) {
        valid = false;
    }

    if (valid) {
        subresource = body->host.face * surface->face[0].numMipLevels +
                      body->host.mipmap;
        if (subresource >= surface->mip_count) {
            valid = false;
        }
    }

    for (i = 0; valid && i < box_count; i++) {
        valid = vmsvga3d_surface_dma_box(s, surface, image, &body->guest,
                                         body->transfer, &boxes[i],
                                         maximum_offset, false);
    }

    if (valid) {
        accel = vmsvga3d_d3d9_try_surface_dma(
            s, body, boxes, box_count, maximum_offset, &transfer_plan);
        if (accel == VMSVGA3D_D3D9_ACCEL_FAILED) {
            valid = false;
        }
    }

    if (valid && box_count != 0 &&
        accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
        !vmsvga3d_surface_readback_to_shadow(
            s, surface, image, subresource)) {
        valid = false;
    }

    for (i = 0; valid && accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
                i < box_count; i++) {
        valid = vmsvga3d_surface_dma_box(s, surface, image, &body->guest,
                                         body->transfer, &boxes[i],
                                         maximum_offset, true);
        if (valid && body->transfer == SVGA3D_WRITE_HOST_VRAM) {
            valid = vmsvga3d_surface_dma_d3d11_upload_box(
                s, surface, image, subresource, &boxes[i]);
        }
    }

    if (valid && box_count != 0 &&
        accel != VMSVGA3D_D3D9_ACCEL_COMPLETE &&
        body->transfer == SVGA3D_WRITE_HOST_VRAM) {
        /* D3D11 boxes were updated above.  If this is a D3D9-resident
         * fallback, upload the coherent CPU shadow or evict an unsupported
         * resource so the next use rematerializes from that shadow. */
        vmsvga3d_d3d9_runtime_sync_surface_from_cpu(s, surface);
    }

    if (valid && box_count != 0 && body->transfer == SVGA3D_WRITE_HOST_VRAM) {
        if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
            vmsvga3d_trace_vgpu9_surface_write(
                s, body->host.sid, VMSVGA3D_TRACE_VGPU9_WRITE_DMA,
                SVGA3D_INVALID_ID, false);
        }
    }

    g_free(payload);
    return true;
}

static uint32_t vmsvga3d_host_hwversion(void)
{
    return SVGA3D_HWVERSION_WS8_B1;
}

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

/*
 * These helpers are defined by the included DXVK/D3D10 implementation files
 * below.  Keep declarations ahead of those definitions so QEMU's
 * -Wmissing-prototypes build remains clean for the amalgamated translation
 * unit.
 */
bool vmsvga3d_dxvk_d3d11_shader_invalidate(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t shader_id);
VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_mob_entry(
    const SVGA3dCmdDXDefineStreamOutputWithMob *src,
    SVGACOTableDXStreamOutputEntry *dst);
VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_bind_entry(
    SVGACOTableDXStreamOutputEntry *entry, uint32_t mobid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes);
static bool vmsvga3d_screen_target_mark_dirty_live(
    struct vmsvga_state_s *s, uint32_t sid, uint32_t subresource,
    const SVGA3dRect *rect, bool presentation);
static bool vmsvga3d_surface_changed_live(
    struct vmsvga_state_s *s, uint32_t sid, uint32_t subresource,
    const SVGA3dBox *box);
static bool vmsvga3d_surface_presented_live(
    struct vmsvga_state_s *s, uint32_t sid, uint32_t subresource,
    const SVGA3dBox *box);

#include "vmware_vga_dxvk_wsi.c"
#include "vmware_vga_dxvk.c"
#define VMSVGA3D_D3D9_RUNTIME_INTEGRATION 1
#include "vmware_vga_vgpu9.c"
#undef VMSVGA3D_D3D9_RUNTIME_INTEGRATION

/* Generic SVGA3D commands must synchronize the renderer that actually owns
 * a surface before they inspect or modify the canonical CPU shadow.  Keep
 * that backend selection here instead of open-coding D3D11 assumptions in
 * individual FIFO handlers. */
static bool vmsvga3d_surface_readback_to_shadow(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DSurfaceImage *image, uint32_t subresource)
{
    VMSVGA3DD3D9TransferSurface d3d9_info = {0};
    VMSVGA3DD3D9AccelResult d3d9_result;
    bool d3d9_resident;
    bool d3d11_resident;
    bool success;
    const char *backend;

    if (s == NULL || surface == NULL || image == NULL ||
        surface->mips == NULL || subresource >= surface->mip_count ||
        &surface->mips[subresource] != image) {
        return false;
    }

    d3d9_resident =
        vmsvga3d_d3d9_runtime_surface_info(s, surface, &d3d9_info) &&
        d3d9_info.resident;
    d3d11_resident =
        surface->dxvk_surface != NULL &&
        vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface);

    if (d3d9_resident && d3d11_resident) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "COHERENCE op=readback sid=%u sub=%u backend=conflict result=REJECT",
            surface->sid, subresource);
        return false;
    }

    if (d3d9_resident) {
        backend = "d3d9";
        d3d9_result = vmsvga3d_d3d9_runtime_readback_surface_image(
            s, surface, image, subresource);
        success = d3d9_result == VMSVGA3D_D3D9_ACCEL_COMPLETE;
    } else if (d3d11_resident) {
        backend = "d3d11";
        success = vmsvga3d_d3d11_readback_shadow_image(
            s, surface, image, subresource);
    } else {
        backend = "cpu";
        success = true;
    }

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "COHERENCE op=readback sid=%u sub=%u backend=%s result=%s",
        surface->sid, subresource, backend, success ? "OK" : "FAIL");
    return success;
}

static bool vmsvga3d_clear_readback_image(
    struct vmsvga_state_s *s, const SVGA3dSurfaceImageId *image_id)
{
    VMSVGA3DSurface *surface;
    VMSVGA3DSurfaceImage *image;
    uint32_t subresource;
    uint32_t levels;

    if (s == NULL || image_id == NULL || s->svga3d == NULL ||
        image_id->sid == SVGA3D_INVALID_ID ||
        image_id->sid >= SVGA3D_MAX_SURFACE_IDS) {
        return true;
    }

    surface = s->svga3d->surfaces[image_id->sid];
    if (!vmsvga3d_surface_image(surface, image_id, &image)) {
        return false;
    }

    levels = surface->face[0].numMipLevels;
    if (levels == 0 || image_id->face > UINT32_MAX / levels) {
        return false;
    }
    subresource = image_id->face * levels + image_id->mipmap;

    return vmsvga3d_surface_readback_to_shadow(
        s, surface, image, subresource);
}

static bool vmsvga3d_clear_readback_targets(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dClearFlag clear_flags)
{
    VMSVGA3DContext *context = vmsvga3d_context(s, cid);
    uint32_t type;

    if (context == NULL) {
        return false;
    }

    if (clear_flags & SVGA3D_CLEAR_COLOR) {
        for (type = SVGA3D_RT_COLOR0; type <= SVGA3D_RT_COLOR7; type++) {
            if (!vmsvga3d_clear_readback_image(
                    s, &context->render_targets[type])) {
                return false;
            }
        }
    }

    if ((clear_flags & SVGA3D_CLEAR_DEPTH) &&
        !vmsvga3d_clear_readback_image(
            s, &context->render_targets[SVGA3D_RT_DEPTH])) {
        return false;
    }

    if ((clear_flags & SVGA3D_CLEAR_STENCIL) &&
        (!(clear_flags & SVGA3D_CLEAR_DEPTH) ||
         context->render_targets[SVGA3D_RT_STENCIL].sid !=
             context->render_targets[SVGA3D_RT_DEPTH].sid ||
         context->render_targets[SVGA3D_RT_STENCIL].face !=
             context->render_targets[SVGA3D_RT_DEPTH].face ||
         context->render_targets[SVGA3D_RT_STENCIL].mipmap !=
             context->render_targets[SVGA3D_RT_DEPTH].mipmap) &&
        !vmsvga3d_clear_readback_image(
            s, &context->render_targets[SVGA3D_RT_STENCIL])) {
        return false;
    }

    return true;
}

#define VMSVGA3D_D3D10_RUNTIME_INTEGRATION 1
#include "vmware_vga_vgpu10.c"
#undef VMSVGA3D_D3D10_RUNTIME_INTEGRATION
#include "vmware_vga_vgpu11.c"
#include "vmware_vga_3d_state.c"

static SVGACBStatus vmsvga3d_device_command_buffer_process(
    const void *commands, uint32_t size, uint32_t *error_offset)
{
    const uint8_t *bytes = commands;
    uint32_t offset = 0;

    if (error_offset == NULL || (size != 0 && commands == NULL)) {
        return SVGA_CB_STATUS_COMMAND_ERROR;
    }
    *error_offset = 0;

    while (offset < size) {
        const uint32_t command_offset = offset;
        uint32_t cmd;

        if (size - offset < sizeof(cmd)) {
            *error_offset = command_offset;
            return SVGA_CB_STATUS_COMMAND_ERROR;
        }

        memcpy(&cmd, bytes + offset, sizeof(cmd));
        cmd = le32_to_cpu(cmd);
        offset += sizeof(cmd);

        switch (cmd) {
        case SVGA_DC_CMD_NOP:
            break;

        case SVGA_DC_CMD_START_STOP_CONTEXT: {
              SVGADCCmdStartStop command;

              if (size - offset < sizeof(command)) {
                  *error_offset = command_offset;
                  return SVGA_CB_STATUS_COMMAND_ERROR;
              }
              memcpy(&command, bytes + offset, sizeof(command));
              command.enable = le32_to_cpu(command.enable);
              command.context = (SVGACBContext)le32_to_cpu((uint32_t)command.context);
              offset += sizeof(command);
              /* QEMU executes context-0 command buffers synchronously, so there is no
               * queue object to create or destroy.  Keep VirtualBox's command/argument
               * validation while making START/STOP an otherwise successful no-op. */
              if (command.context >= SVGA_CB_CONTEXT_MAX) {
                  *error_offset = command_offset;
                  return SVGA_CB_STATUS_COMMAND_ERROR;
              }
              break;
          }

        case SVGA_DC_CMD_PREEMPT: {
              SVGADCCmdPreempt command;

              if (size - offset < sizeof(command)) {
                  *error_offset = command_offset;
                  return SVGA_CB_STATUS_COMMAND_ERROR;
              }
              memcpy(&command, bytes + offset, sizeof(command));
              command.context = (SVGACBContext)le32_to_cpu((uint32_t)command.context);
              command.ignoreIDZero = le32_to_cpu(command.ignoreIDZero);
              offset += sizeof(command);
              /* There is no asynchronous queue in this backend, therefore by the time
               * PREEMPT executes there is nothing pending to preempt. */
              if (command.context >= SVGA_CB_CONTEXT_MAX) {
                  *error_offset = command_offset;
                  return SVGA_CB_STATUS_COMMAND_ERROR;
              }
              break;
          }

        default:
            *error_offset = command_offset;
            return SVGA_CB_STATUS_COMMAND_ERROR;
        }
    }

    *error_offset = offset;
    return SVGA_CB_STATUS_COMPLETED;
}

static void vmsvga3d_command_buffer_write_status(
    struct vmsvga_state_s *s, uint64_t header_gpa, SVGACBStatus status,
    uint32_t error_offset, uint32_t processed_offset)
{
    uint32_t value;

    /* Publish result metadata before status.  The guest may poll status as
     * the completion indication and must not observe stale offset data after
     * it sees a terminal status value. */
    if (status == SVGA_CB_STATUS_COMMAND_ERROR) {
        value = cpu_to_le32(error_offset);
        (void)vmsvga3d_guest_memory_write(
            s, header_gpa + offsetof(SVGACBHeader, errorOffset),
            &value, sizeof(value));
    }

    /* Some guests read back `SVGACBHeader.offset` on completion to determine
     * how far the synchronous parser progressed.  Publish the processed offset
     * before writing the terminal status value. */
    (void)vmsvga3d_guest_memory_write(
        s, header_gpa + offsetof(SVGACBHeader, offset), &value, sizeof(value));

    value = cpu_to_le32((uint32_t)status);
    (void)vmsvga3d_guest_memory_write(
        s, header_gpa + offsetof(SVGACBHeader, status), &value, sizeof(value));
}

static void vmsvga3d_command_buffer_raise_irq(struct vmsvga_state_s *s,
                                               uint32_t flags)
{
    bool trace_flight;

    if (s == NULL || flags == 0) {
        return;
    }

    trace_flight = vmsvga_trace_flight_enabled();
    if (trace_flight) {
        fprintf(stderr,
                "VMVGA-IRQ event=cb-request flags=0x%08x mask=0x%08x "
                "status=0x%08x enabled=%u\n",
                flags, s->irq_mask, s->irq_status, !!(flags & s->irq_mask));
    }
    if ((flags & s->irq_mask) == 0) {
        return;
    }

    s->irq_status |= flags;
    if (trace_flight) {
        fprintf(stderr,
                "VMVGA-IRQ event=raise reason=command-buffer "
                "flags=0x%08x mask=0x%08x status=0x%08x line=1\n",
                flags, s->irq_mask, s->irq_status);
    }

#ifndef RAISE_IRQ_OFF
    {
        struct pci_vmsvga_state_s *pci_vmsvga =
            container_of(s, struct pci_vmsvga_state_s, chip);
        pci_set_irq(PCI_DEVICE(pci_vmsvga), 1);
    }
#endif
}

static void vmsvga3d_command_buffer_submit(struct vmsvga_state_s *s,
                                           uint32_t command_low,
                                           uint32_t command_high,
                                           bool prepend)
{
    SVGACBHeader raw;
    SVGACBHeader header;
    SVGACBStatus status = SVGA_CB_STATUS_CB_HEADER_ERROR;
    uint64_t header_gpa;
    uint8_t *commands = NULL;
    uint32_t context;
    uint32_t processed = 0;
    uint32_t irq_flags = 0;
    bool reserved_nonzero = false;
    unsigned i;

    if (s == NULL) {
        return;
    }

    header_gpa = ((uint64_t)command_high << 32) |
                 ((uint64_t)command_low & ~(uint64_t)SVGA_CB_CONTEXT_MASK);
    context = command_low & SVGA_CB_CONTEXT_MASK;

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "CB-SUBMIT kind=%s low=0x%08x high=0x%08x header=0x%016" PRIx64
        " context=%u enable=%u config=%u",
        prepend ? "PREPEND" : "COMMAND", command_low, command_high,
        header_gpa, context, s->enable, s->config);

    /* CONFIG_DONE is required for register command buffers.  Device-context
     * teardown commands may still arrive after SVGA_REG_ENABLE is cleared. */
    if (!s->config ||
        (!s->enable && context != SVGA_CB_CONTEXT_DEVICE)) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "CB-COMPLETE kind=%s header=0x%016" PRIx64
            " context=%u result=IGNORED reason=fifo-disabled",
            prepend ? "PREPEND" : "COMMAND", header_gpa, context);
        return;
    }

    if ((header_gpa & 63u) != 0) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "CB-COMPLETE kind=%s header=0x%016" PRIx64
            " context=%u result=REJECT reason=header-alignment",
            prepend ? "PREPEND" : "COMMAND", header_gpa, context);
        return;
    }

    if (!vmsvga3d_guest_memory_read(s, header_gpa, &raw, sizeof(raw))) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "CB-COMPLETE kind=%s header=0x%016" PRIx64
            " context=%u result=REJECT reason=header-read",
            prepend ? "PREPEND" : "COMMAND", header_gpa, context);
        return;
    }

    memset(&header, 0, sizeof(header));
    header.status = (SVGACBStatus)le32_to_cpu((uint32_t)raw.status);
    header.errorOffset = le32_to_cpu(raw.errorOffset);
    header.id = le64_to_cpu(raw.id);
    header.flags = (SVGACBFlags)le32_to_cpu((uint32_t)raw.flags);
    header.length = le32_to_cpu(raw.length);
    if ((header.flags & SVGA_CB_FLAG_MOB) != 0) {
        header.ptr.mob.mobid = le32_to_cpu(raw.ptr.mob.mobid);
        header.ptr.mob.mobOffset = le32_to_cpu(raw.ptr.mob.mobOffset);
    } else {
        header.ptr.pa = le64_to_cpu(raw.ptr.pa);
    }
    header.offset = le32_to_cpu(raw.offset);
    processed = header.offset;
    header.dxContext = le32_to_cpu(raw.dxContext);
    for (i = 0; i < ARRAY_SIZE(raw.mustBeZero); i++) {
        if (le32_to_cpu(raw.mustBeZero[i]) != 0) {
            reserved_nonzero = true;
            break;
        }
    }

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "CB-HEADER kind=%s header=0x%016" PRIx64
        " context=%u status=%u id=0x%016" PRIx64
        " flags=0x%08x length=%u offset=%u dxContext=%u source=%s",
        prepend ? "PREPEND" : "COMMAND", header_gpa, context,
        (unsigned)header.status, header.id, (unsigned)header.flags,
        header.length, header.offset, header.dxContext,
        (header.flags & SVGA_CB_FLAG_MOB) ? "MOB" : "PA");

    if (header.status != SVGA_CB_STATUS_NONE ||
        (header.flags & ~(SVGA_CB_FLAG_NO_IRQ | SVGA_CB_FLAG_DX_CONTEXT |
                          SVGA_CB_FLAG_MOB)) != 0 ||
        ((header.flags & SVGA_CB_FLAG_MOB) != 0 &&
         !vmsvga_guest_backed_objects_capable(s)) ||
        header.length > SVGA_CB_MAX_SIZE || header.offset > header.length ||
        reserved_nonzero) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "CB-HEADER kind=%s header=0x%016" PRIx64
            " result=REJECT status=%u flags=0x%08x length=%u offset=%u "
            "reserved=%u",
            prepend ? "PREPEND" : "COMMAND", header_gpa,
            (unsigned)header.status, (unsigned)header.flags, header.length,
            header.offset, reserved_nonzero);
        irq_flags = SVGA_IRQFLAG_ERROR | SVGA_IRQFLAG_COMMAND_BUFFER;
        goto out;
    }

    if (header.length != 0) {
        commands = g_try_malloc(header.length);
        if (commands == NULL) {
            status = SVGA_CB_STATUS_QUEUE_FULL;
            goto out;
        }

        if ((header.flags & SVGA_CB_FLAG_MOB) != 0) {
            VMSVGA3DMob *mob = vmsvga3d_mob_get(s, header.ptr.mob.mobid);

            if (mob == NULL ||
                !vmsvga3d_mob_read(s, mob, header.ptr.mob.mobOffset,
                                    commands, header.length)) {
                VMVGA_TRACE_LOCAL(
                    VMVGA_TRACE_3D,
                    "CB-SOURCE kind=%s source=MOB mobid=%u offset=0x%08x "
                    "length=%u result=REJECT",
                    prepend ? "PREPEND" : "COMMAND",
                    header.ptr.mob.mobid, header.ptr.mob.mobOffset,
                    header.length);
                irq_flags = SVGA_IRQFLAG_ERROR | SVGA_IRQFLAG_COMMAND_BUFFER;
                goto out;
            }

            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "CB-SOURCE kind=%s source=MOB mobid=%u offset=0x%08x "
                "length=%u result=OK",
                prepend ? "PREPEND" : "COMMAND",
                header.ptr.mob.mobid, header.ptr.mob.mobOffset,
                header.length);
        } else if (!vmsvga3d_guest_memory_read(s, header.ptr.pa, commands,
                                               header.length)) {
            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "CB-SOURCE kind=%s source=PA gpa=0x%016" PRIx64
                " length=%u result=REJECT",
                prepend ? "PREPEND" : "COMMAND", header.ptr.pa,
                header.length);
            irq_flags = SVGA_IRQFLAG_ERROR | SVGA_IRQFLAG_COMMAND_BUFFER;
            goto out;
        } else {
            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "CB-SOURCE kind=%s source=PA gpa=0x%016" PRIx64
                " length=%u result=OK",
                prepend ? "PREPEND" : "COMMAND", header.ptr.pa,
                header.length);
        }
    }

    if (context == SVGA_CB_CONTEXT_DEVICE) {
        uint32_t local_offset = 0;
        status = vmsvga3d_device_command_buffer_process(
            commands != NULL ? commands + header.offset : NULL,
            header.length - header.offset, &local_offset);
        processed = header.offset + local_offset;
    } else if (context < SVGA_CB_CONTEXT_MAX) {
        uint32_t local_offset = 0;
        uint32_t dx_context =
            (header.flags & SVGA_CB_FLAG_DX_CONTEXT) != 0
                ? header.dxContext
                : SVGA3D_INVALID_ID;

        /*
         * COMMAND_BUFFERS_2 carries the normal SVGA command stream.  The
         * DX_CONTEXT flag supplies metadata for DX commands in that stream;
         * it does not select a different command encoding.  Use one parser
         * for both flagged and unflagged buffers, like VirtualBox does.
         */
        status = vmsvga_command_buffer_process(
            s, dx_context,
            commands != NULL ? commands + header.offset : NULL,
            header.length - header.offset, &local_offset);
        processed = header.offset + local_offset;
    } else {
        status = SVGA_CB_STATUS_QUEUE_FULL;
    }

    if ((header.flags & SVGA_CB_FLAG_NO_IRQ) == 0) {
        irq_flags |= SVGA_IRQFLAG_COMMAND_BUFFER;
    }
    if (status == SVGA_CB_STATUS_COMMAND_ERROR) {
        irq_flags |= SVGA_IRQFLAG_ERROR;
    }

out:
    if (status != SVGA_CB_STATUS_NONE) {
        vmsvga3d_command_buffer_write_status(
            s, header_gpa, status, processed, processed);
    }

    VMVGA_TRACE_LOCAL(
        VMVGA_TRACE_3D,
        "CB-COMPLETE kind=%s header=0x%016" PRIx64
        " context=%u id=0x%016" PRIx64
        " status=%u processed=%u errorOffset=%u irq=0x%08x",
        prepend ? "PREPEND" : "COMMAND", header_gpa, context, header.id,
        (unsigned)status, processed,
        status == SVGA_CB_STATUS_COMMAND_ERROR ? processed : 0, irq_flags);

    vmsvga3d_command_buffer_raise_irq(s, irq_flags);

    g_free(commands);
}


static bool vmsvga3d_gb_context_entry_read(struct vmsvga_state_s *s,
                                             uint32_t cid,
                                             SVGAOTableContextEntry *entry)
{
    SVGAOTableContextEntry raw;

    if (entry == NULL ||
        !vmsvga3d_otable_read(s, SVGA_OTABLE_CONTEXT, cid, sizeof(raw),
                              &raw, sizeof(raw))) {
        return false;
    }

    entry->cid = le32_to_cpu(raw.cid);
    entry->mobid = le32_to_cpu(raw.mobid);
    return true;
}

static bool vmsvga3d_gb_context_entry_write(struct vmsvga_state_s *s,
                                              uint32_t cid,
                                              const SVGAOTableContextEntry *entry)
{
    SVGAOTableContextEntry raw;

    if (entry == NULL) {
        return false;
    }

    raw.cid = cpu_to_le32(entry->cid);
    raw.mobid = cpu_to_le32(entry->mobid);
    return vmsvga3d_otable_write(s, SVGA_OTABLE_CONTEXT, cid, sizeof(raw),
                                 &raw, sizeof(raw));
}

static bool vmsvga3d_gb_shader_entry_read(struct vmsvga_state_s *s,
                                            uint32_t shid,
                                            SVGAOTableShaderEntry *entry)
{
    SVGAOTableShaderEntry raw;

    if (entry == NULL ||
        !vmsvga3d_otable_read(s, SVGA_OTABLE_SHADER, shid, sizeof(raw),
                              &raw, sizeof(raw))) {
        return false;
    }

    entry->type = le32_to_cpu(raw.type);
    entry->sizeInBytes = le32_to_cpu(raw.sizeInBytes);
    entry->offsetInBytes = le32_to_cpu(raw.offsetInBytes);
    entry->mobid = le32_to_cpu(raw.mobid);
    return true;
}

static bool vmsvga3d_gb_shader_entry_write(struct vmsvga_state_s *s,
                                             uint32_t shid,
                                             const SVGAOTableShaderEntry *entry)
{
    SVGAOTableShaderEntry raw;

    if (entry == NULL) {
        return false;
    }

    raw.type = cpu_to_le32(entry->type);
    raw.sizeInBytes = cpu_to_le32(entry->sizeInBytes);
    raw.offsetInBytes = cpu_to_le32(entry->offsetInBytes);
    raw.mobid = cpu_to_le32(entry->mobid);
    return vmsvga3d_otable_write(s, SVGA_OTABLE_SHADER, shid, sizeof(raw),
                                 &raw, sizeof(raw));
}


static SVGAGBVertexElement *vmsvga3d_gb_context_vertex_decl(
    SVGAGBContextData *context, uint32_t index)
{
    if (index < ARRAY_SIZE(context->decl1)) {
        return &context->decl1[index];
    }
    index -= ARRAY_SIZE(context->decl1);
    if (index < ARRAY_SIZE(context->decl2)) {
        return &context->decl2[index];
    }
    index -= ARRAY_SIZE(context->decl2);
    if (index < ARRAY_SIZE(context->decl3)) {
        return &context->decl3[index];
    }
    return NULL;
}

static const SVGAGBVertexElement *vmsvga3d_gb_context_vertex_decl_const(
    const SVGAGBContextData *context, uint32_t index)
{
    if (index < ARRAY_SIZE(context->decl1)) {
        return &context->decl1[index];
    }
    index -= ARRAY_SIZE(context->decl1);
    if (index < ARRAY_SIZE(context->decl2)) {
        return &context->decl2[index];
    }
    index -= ARRAY_SIZE(context->decl2);
    if (index < ARRAY_SIZE(context->decl3)) {
        return &context->decl3[index];
    }
    return NULL;
}

static bool vmsvga3d_gb_vertex_element_encode(
    const SVGA3dVertexElement *src, SVGAGBVertexElement *dst)
{
    if (src == NULL || dst == NULL ||
        src->type >= SVGA3D_DECLTYPE_MAX ||
        src->method > SVGA3D_DECLMETHOD_LOOKUPPRESAMPLED ||
        src->usage >= SVGA3D_DECLUSAGE_MAX) {
        return false;
    }

    dst->streamOffset = src->streamOffset;
    dst->stream = src->stream;
    dst->type = src->type;
    dst->methodUsage = (src->method & 0x0f) | ((src->usage & 0x0f) << 4);
    dst->usageIndex = src->usageIndex;
    return true;
}

static bool vmsvga3d_gb_vertex_element_decode(
    const SVGAGBVertexElement *src, SVGA3dVertexElement *dst)
{
    uint8_t method;
    uint8_t usage;

    if (src == NULL || dst == NULL) {
        return false;
    }

    method = src->methodUsage & 0x0f;
    usage = src->methodUsage >> 4;
    if (src->type >= SVGA3D_DECLTYPE_MAX ||
        method > SVGA3D_DECLMETHOD_LOOKUPPRESAMPLED ||
        usage >= SVGA3D_DECLUSAGE_MAX) {
        return false;
    }

    dst->streamOffset = src->streamOffset;
    dst->stream = src->stream;
    dst->type = src->type;
    dst->method = method;
    dst->usage = usage;
    dst->usageIndex = src->usageIndex;
    dst->padding = 0;
    return true;
}

static bool vmsvga3d_gb_vertex_stream_encode(
    const SVGA3dVertexStream *src, SVGAGBVertexStream *dst)
{
    if (src == NULL || dst == NULL || src->stride > UINT16_MAX) {
        return false;
    }

    dst->sid = src->sid;
    dst->stride = src->stride;
    dst->offset = src->offset;
    return true;
}

static void vmsvga3d_gb_vertex_stream_decode(
    const SVGAGBVertexStream *src, SVGA3dVertexStream *dst)
{
    dst->sid = src->sid;
    dst->stride = src->stride;
    dst->offset = src->offset;
}

static const char *vmsvga3d_trace_render_state_name(uint32_t state)
{
    switch (state) {
    case SVGA3D_RS_CLIPPING:
        return "CLIPPING";
    case SVGA3D_RS_ZENABLE:
        return "ZENABLE";
    case SVGA3D_RS_ZWRITEENABLE:
        return "ZWRITEENABLE";
    case SVGA3D_RS_LIGHTINGENABLE:
        return "LIGHTINGENABLE";
    case SVGA3D_RS_CULLMODE:
        return "CULLMODE";
    case SVGA3D_RS_ZFUNC:
        return "ZFUNC";
    case SVGA3D_RS_COLORWRITEENABLE:
        return "COLORWRITEENABLE";
    default:
        return NULL;
    }
}

static void vmsvga3d_trace_gb_context_render_states(
    const char *phase, uint32_t cid, const VMSVGA3DContext *context,
    const SVGAGBContextData *contents)
{
    static const uint32_t states[] = {
        SVGA3D_RS_CLIPPING,
        SVGA3D_RS_ZENABLE,
        SVGA3D_RS_ZWRITEENABLE,
        SVGA3D_RS_LIGHTINGENABLE,
        SVGA3D_RS_CULLMODE,
        SVGA3D_RS_ZFUNC,
        SVGA3D_RS_COLORWRITEENABLE,
    };
    uint32_t i;

    if (phase == NULL || contents == NULL) {
        return;
    }

    for (i = 0; i < ARRAY_SIZE(states); i++) {
        uint32_t state = states[i];
        bool valid = context != NULL && context->render_state[state].valid;
        uint32_t cached = context != NULL ? context->render_state[state].value : 0;

        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-CONTEXT phase=%s cid=%u state=%s(%u) cached-valid=%u "
            "cached=0x%08x mob=0x%08x",
            phase, cid, vmsvga3d_trace_render_state_name(state), state,
            valid ? 1u : 0u, cached, contents->renderStates[state]);
    }
}

static bool vmsvga3d_gb_context_snapshot(struct vmsvga_state_s *s,
                                          uint32_t cid,
                                          SVGAGBContextData *out)
{
    VMSVGA3DContext *context = vmsvga3d_context(s, cid);
    uint32_t i, j;

    if (context == NULL || out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    if (context->viewport_valid) {
        out->viewport = context->viewport;
    }
    if (context->scissor_valid) {
        out->scissorRect = context->scissor;
    }
    if (context->z_range_valid) {
        out->zRange = context->z_range;
    }
    memcpy(out->renderTargets, context->render_targets,
           sizeof(out->renderTargets));

    for (i = 0; i < SVGA3D_RS_MAX; i++) {
        if (context->render_state[i].valid) {
            out->renderStates[i] = context->render_state[i].value;
        }
    }
    for (i = 0; i < MIN((uint32_t)VMSVGA3D_MAX_SAMPLERS,
                        (uint32_t)SVGA3D_NUM_TEXTURE_UNITS); i++) {
        for (j = 0; j <= SVGA3D_TS_CONSTANT; j++) {
            if (context->texture_state[i][j].valid) {
                out->textureStages[i][j] = context->texture_state[i][j].value;
            }
        }
        if (SVGA3D_TS_COLOR_KEY_ENABLE < SVGA3D_TS_MAX &&
            context->texture_state[i][SVGA3D_TS_COLOR_KEY_ENABLE].valid) {
            out->tsColorKeyEnable[i] =
                context->texture_state[i][SVGA3D_TS_COLOR_KEY_ENABLE].value;
        }
        if (SVGA3D_TS_COLOR_KEY < SVGA3D_TS_MAX &&
            context->texture_state[i][SVGA3D_TS_COLOR_KEY].valid) {
            out->tsColorKey[i] =
                context->texture_state[i][SVGA3D_TS_COLOR_KEY].value;
        }
    }
    /* The GB context stores one effective material.  Match the D3D9
     * replay priority so readback preserves the state that can affect draws. */
    if (context->material[SVGA3D_FACE_FRONT].valid) {
        out->material.face = SVGA3D_FACE_FRONT;
        out->material.material =
            context->material[SVGA3D_FACE_FRONT].material;
    } else if (context->material[SVGA3D_FACE_FRONT_BACK].valid) {
        out->material.face = SVGA3D_FACE_FRONT_BACK;
        out->material.material =
            context->material[SVGA3D_FACE_FRONT_BACK].material;
    } else if (context->material[SVGA3D_FACE_BACK].valid) {
        out->material.face = SVGA3D_FACE_BACK;
        out->material.material =
            context->material[SVGA3D_FACE_BACK].material;
    }

    for (i = 0; i < SVGA3D_TRANSFORM_MAX; i++) {
        if (context->transform[i].valid) {
            memcpy(out->matrices[i], context->transform[i].matrix,
                   sizeof(out->matrices[i]));
        }
    }
    for (i = 0; i < SVGA3D_NUM_CLIPPLANES && i < VMSVGA3D_MAX_CLIP_PLANES; i++) {
        if (context->clip_plane[i].valid) {
            memcpy(out->clipPlanes[i], context->clip_plane[i].plane,
                   sizeof(out->clipPlanes[i]));
        }
    }
    for (i = 0; i < SVGA3D_NUM_LIGHTS; i++) {
        if (context->light[i].enabled_valid) {
            out->lightEnabled[i] = context->light[i].enabled;
        }
        if (context->light[i].data_valid) {
            out->lightData[i] = context->light[i].data;
        }
    }
    for (i = 0; i < SVGA3D_NUM_SHADERTYPE_PREDX; i++) {
        out->shaders[i] = context->bound_shader[i];
        for (j = 0; j < SVGA3D_CONSTBOOLREG_MAX; j++) {
            if (context->shader_bool[i][j].valid &&
                context->shader_bool[i][j].values[0] != 0) {
                if (i == 0) {
                    out->vShaderBValues |= (uint16_t)(1u << j);
                } else {
                    out->pShaderBValues |= (uint16_t)(1u << j);
                }
            }
        }
        for (j = 0; j < SVGA3D_CONSTINTREG_MAX; j++) {
            if (context->shader_int[i][j].valid) {
                if (i == 0) {
                    memcpy(out->vShaderIValues[j].value,
                           context->shader_int[i][j].values,
                           sizeof(out->vShaderIValues[j].value));
                } else {
                    memcpy(out->pShaderIValues[j].value,
                           context->shader_int[i][j].values,
                           sizeof(out->pShaderIValues[j].value));
                }
            }
        }
        for (j = 0; j < SVGA3D_CONSTREG_MAX; j++) {
            if (context->shader_float[i][j].valid) {
                if (i == 0) {
                    memcpy(out->vShaderFValues[j].value,
                           context->shader_float[i][j].values,
                           sizeof(out->vShaderFValues[j].value));
                } else {
                    memcpy(out->pShaderFValues[j].value,
                           context->shader_float[i][j].values,
                           sizeof(out->pShaderFValues[j].value));
                }
            }
        }
    }
    out->numVertexDecls = MIN(context->num_vertex_decls,
                              (uint32_t)SVGA3D_MAX_VERTEX_ARRAYS);
    out->numVertexStreams = MIN(context->num_vertex_streams,
                                (uint32_t)SVGA3D_MAX_VERTEX_ARRAYS);
    out->numVertexDivisors = MIN(context->num_vertex_divisors,
                                 (uint32_t)SVGA3D_MAX_VERTEX_ARRAYS);
    for (i = 0; i < out->numVertexDecls; i++) {
        SVGAGBVertexElement *dst = vmsvga3d_gb_context_vertex_decl(out, i);

        if (dst == NULL ||
            !vmsvga3d_gb_vertex_element_encode(&context->vertex_decls[i],
                                                dst)) {
            return false;
        }
    }
    for (i = 0; i < out->numVertexStreams; i++) {
        if (!vmsvga3d_gb_vertex_stream_encode(&context->vertex_streams[i],
                                               &out->streams[i])) {
            return false;
        }
    }
    for (i = 0; i < out->numVertexDivisors; i++) {
        out->divisors[i] = context->vertex_divisors[i];
    }

    out->occQueryActive = context->occlusion.defined &&
                          context->occlusion.state == VMSVGA3D_QUERY_BUILDING;
    out->occQueryValue = context->occlusion.result;
    if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
        vmsvga3d_trace_gb_context_render_states("snapshot", cid, context, out);
    }
    return true;
}

static bool vmsvga3d_gb_context_restore(struct vmsvga_state_s *s,
                                         uint32_t cid,
                                         const SVGAGBContextData *in)
{
    VMSVGA3DContext *context;
    SVGA3dVertexElement vertex_decls[SVGA3D_MAX_VERTEX_ARRAYS] = { 0 };
    SVGA3dVertexStream vertex_streams[SVGA3D_MAX_VERTEX_ARRAYS] = { 0 };
    uint32_t num_vertex_decls;
    uint32_t num_vertex_streams;
    uint32_t num_vertex_divisors;
    uint32_t i, j;

    if (in == NULL) {
        return false;
    }

    num_vertex_decls = MIN(in->numVertexDecls,
                           (uint32_t)SVGA3D_MAX_VERTEX_ARRAYS);
    num_vertex_streams = MIN(in->numVertexStreams,
                             (uint32_t)SVGA3D_MAX_VERTEX_ARRAYS);
    num_vertex_divisors = MIN(in->numVertexDivisors,
                              (uint32_t)SVGA3D_MAX_VERTEX_ARRAYS);
    for (i = 0; i < num_vertex_decls; i++) {
        const SVGAGBVertexElement *src =
            vmsvga3d_gb_context_vertex_decl_const(in, i);

        if (src == NULL ||
            !vmsvga3d_gb_vertex_element_decode(src, &vertex_decls[i])) {
            return false;
        }
    }
    for (i = 0; i < num_vertex_streams; i++) {
        vmsvga3d_gb_vertex_stream_decode(&in->streams[i],
                                          &vertex_streams[i]);
    }

    if (!vmsvga3d_state_context_define(s, cid)) {
        return false;
    }
    context = vmsvga3d_context(s, cid);
    if (context == NULL) {
        return false;
    }

    context->viewport = in->viewport;
    context->viewport_valid = true;
    context->scissor = in->scissorRect;
    context->scissor_valid = true;
    context->z_range = in->zRange;
    context->z_range_valid = true;
    memcpy(context->render_targets, in->renderTargets,
           sizeof(context->render_targets));

    for (i = 0; i < SVGA3D_RS_MAX; i++) {
        context->render_state[i].value = in->renderStates[i];
        context->render_state[i].valid = true;
    }
    for (i = 0; i < MIN((uint32_t)VMSVGA3D_MAX_SAMPLERS,
                        (uint32_t)SVGA3D_NUM_TEXTURE_UNITS); i++) {
        for (j = 0; j <= SVGA3D_TS_CONSTANT; j++) {
            context->texture_state[i][j].value = in->textureStages[i][j];
            context->texture_state[i][j].valid = true;
        }
        if (SVGA3D_TS_COLOR_KEY_ENABLE < SVGA3D_TS_MAX) {
            context->texture_state[i][SVGA3D_TS_COLOR_KEY_ENABLE].value =
                in->tsColorKeyEnable[i];
            context->texture_state[i][SVGA3D_TS_COLOR_KEY_ENABLE].valid = true;
        }
        if (SVGA3D_TS_COLOR_KEY < SVGA3D_TS_MAX) {
            context->texture_state[i][SVGA3D_TS_COLOR_KEY].value =
                in->tsColorKey[i];
            context->texture_state[i][SVGA3D_TS_COLOR_KEY].valid = true;
        }
    }
    if (in->material.face == SVGA3D_FACE_FRONT ||
        in->material.face == SVGA3D_FACE_BACK ||
        in->material.face == SVGA3D_FACE_FRONT_BACK) {
        context->material[in->material.face].material = in->material.material;
        context->material[in->material.face].valid = true;
    }

    for (i = 0; i < SVGA3D_TRANSFORM_MAX; i++) {
        memcpy(context->transform[i].matrix, in->matrices[i],
               sizeof(context->transform[i].matrix));
        context->transform[i].valid = true;
    }
    for (i = 0; i < SVGA3D_NUM_CLIPPLANES && i < VMSVGA3D_MAX_CLIP_PLANES; i++) {
        memcpy(context->clip_plane[i].plane, in->clipPlanes[i],
               sizeof(context->clip_plane[i].plane));
        context->clip_plane[i].valid = true;
    }
    for (i = 0; i < SVGA3D_NUM_LIGHTS; i++) {
        context->light[i].enabled = in->lightEnabled[i];
        context->light[i].enabled_valid = true;
        context->light[i].data = in->lightData[i];
        context->light[i].data_valid = true;
    }
    for (i = 0; i < SVGA3D_NUM_SHADERTYPE_PREDX; i++) {
        context->bound_shader[i] = in->shaders[i];
        for (j = 0; j < SVGA3D_CONSTBOOLREG_MAX; j++) {
            memset(context->shader_bool[i][j].values, 0,
                   sizeof(context->shader_bool[i][j].values));
            context->shader_bool[i][j].values[0] =
                ((i == 0 ? in->vShaderBValues : in->pShaderBValues) >> j) & 1u;
            context->shader_bool[i][j].valid = true;
        }
        for (j = 0; j < SVGA3D_CONSTINTREG_MAX; j++) {
            context->shader_int[i][j].valid = true;
            if (i == 0) {
                memcpy(context->shader_int[i][j].values,
                       in->vShaderIValues[j].value,
                       sizeof(context->shader_int[i][j].values));
            } else {
                memcpy(context->shader_int[i][j].values,
                       in->pShaderIValues[j].value,
                       sizeof(context->shader_int[i][j].values));
            }
        }
        for (j = 0; j < SVGA3D_CONSTREG_MAX; j++) {
            context->shader_float[i][j].valid = true;
            if (i == 0) {
                memcpy(context->shader_float[i][j].values,
                       in->vShaderFValues[j].value,
                       sizeof(context->shader_float[i][j].values));
            } else {
                memcpy(context->shader_float[i][j].values,
                       in->pShaderFValues[j].value,
                       sizeof(context->shader_float[i][j].values));
            }
        }
    }
    context->occlusion.defined = in->occQueryActive != 0;
    context->occlusion.state = in->occQueryActive ? VMSVGA3D_QUERY_BUILDING
                                                   : VMSVGA3D_QUERY_NULL;
    context->occlusion.result = in->occQueryValue;
    context->num_vertex_decls = num_vertex_decls;
    context->num_vertex_streams = num_vertex_streams;
    context->num_vertex_divisors = num_vertex_divisors;
    memcpy(context->vertex_decls, vertex_decls, sizeof(vertex_decls));
    memcpy(context->vertex_streams, vertex_streams, sizeof(vertex_streams));
    for (i = 0; i < num_vertex_divisors; i++) {
        context->vertex_divisors[i] = in->divisors[i];
    }

    if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
        vmsvga3d_trace_gb_context_render_states("restore", cid, context, in);
    }
    return true;
}

static bool vmsvga3d_gb_shader_materialize(struct vmsvga_state_s *s,
                                             uint32_t cid, uint32_t shid,
                                             SVGA3dShaderType type)
{
    SVGAOTableShaderEntry entry;
    VMSVGA3DMob *mob;
    uint32_t *bytecode;
    bool ok;

    if (!vmsvga3d_gb_shader_entry_read(s, shid, &entry) ||
        entry.type != type || entry.mobid == SVGA3D_INVALID_ID ||
        entry.sizeInBytes == 0 ||
        entry.sizeInBytes > SVGA3D_MAX_SHADER_MEMORY_BYTES ||
        entry.sizeInBytes % sizeof(uint32_t) != 0) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, entry.mobid);
    if (mob == NULL || entry.offsetInBytes > mob->gbo.size ||
        entry.sizeInBytes > mob->gbo.size - entry.offsetInBytes) {
        return false;
    }

    bytecode = g_try_malloc(entry.sizeInBytes);
    if (bytecode == NULL) {
        return false;
    }

    ok = vmsvga3d_mob_read(s, mob, entry.offsetInBytes, bytecode,
                            entry.sizeInBytes) &&
         vmsvga3d_state_shader_define(s, cid, shid, type,
                                      entry.sizeInBytes, bytecode);
    g_free(bytecode);
    return ok;
}

static bool vmsvga3d_dx_context_entry_read(
    struct vmsvga_state_s *s, uint32_t cid,
    SVGAOTableDXContextEntry *entry)
{
    SVGAOTableDXContextEntry raw;

    if (entry == NULL ||
        !vmsvga3d_otable_read(s, SVGA_OTABLE_DXCONTEXT, cid,
                              sizeof(raw), &raw, sizeof(raw))) {
        return false;
    }

    entry->cid = le32_to_cpu(raw.cid);
    entry->mobid = le32_to_cpu(raw.mobid);

    return true;
}

static bool vmsvga3d_dx_context_entry_write(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGAOTableDXContextEntry *entry)
{
    SVGAOTableDXContextEntry raw;

    if (entry == NULL) {
        return false;
    }

    raw.cid = cpu_to_le32(entry->cid);
    raw.mobid = cpu_to_le32(entry->mobid);

    return vmsvga3d_otable_write(s, SVGA_OTABLE_DXCONTEXT, cid,
                                 sizeof(raw), &raw, sizeof(raw));
}

static uint32_t vmsvga3d_dx_cotable_entry_size(SVGACOTableType type)
{
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
    }
    return 0;
}

static bool vmsvga3d_dx_cotable_set_or_grow(
    struct vmsvga_state_s *s, uint32_t cid, SVGAMobId mobid,
    SVGACOTableType type, uint32_t valid_size, bool grow)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    VMSVGA3DDXCOTable *binding;
    VMSVGA3DMob *mob;
    VMSVGA3DMob *old_mob;
    uint8_t *new_host = NULL;
    uint32_t entry_size;
    uint32_t capacity_entries;
    uint32_t valid_entries;
    uint32_t old_capacity_entries;
    uint32_t copy_bytes = 0;

    if (context == NULL || type >= SVGA_COTABLE_MAX) {
        return false;
    }

    entry_size = vmsvga3d_dx_cotable_entry_size(type);
    if (entry_size == 0) {
        return false;
    }

    binding = &context->cotables[type];
    old_capacity_entries = binding->capacity_entries;
    mob = vmsvga3d_mob_get(s, mobid);
    old_mob = vmsvga3d_mob_get(s, binding->mobid);

    /* Match VirtualBox: a nonexistent MOB unbinds the current COTable. */
    if (mob != NULL) {
        if (valid_size > mob->gbo.size) {
            return false;
        }
        if (grow) {
            uint32_t old_mob_size = old_mob != NULL ? old_mob->gbo.size : 0;

            valid_size = MIN(valid_size, old_mob_size);
        }

        capacity_entries = MIN(mob->gbo.size / entry_size,
                               (uint32_t)SVGA_COTABLE_MAX_IDS);
        valid_entries = MIN(valid_size / entry_size, capacity_entries);
        valid_size = valid_entries * entry_size;

        new_host = g_try_malloc0(mob->gbo.size);
        if (new_host == NULL && mob->gbo.size != 0) {
            return false;
        }

        if (grow) {
            copy_bytes = MIN(valid_size,
                             binding->capacity_entries * binding->entry_size);
            if (copy_bytes != 0) {
                if (binding->host == NULL || copy_bytes > binding->host_size) {
                    g_free(new_host);
                    return false;
                }
                memcpy(new_host, binding->host, copy_bytes);
            }
        } else if (valid_size != 0) {
            if (vmsvga3d_mob_read(s, mob, 0, new_host, valid_size)) {
                vmsvga3d_d3d10_cotable_sanitize_live(
                    type, new_host, valid_entries, capacity_entries);
            } else {
                memset(new_host, 0, valid_size);
            }
        }
    } else {
        valid_size = 0;
        capacity_entries = 0;
        valid_entries = 0;
    }

    {
        uint8_t *old_host = binding->host;

        binding->mobid = mob != NULL ? mob->mobid : SVGA3D_INVALID_ID;
        binding->valid_size = valid_size;
        binding->entry_size = entry_size;
        binding->capacity_entries = capacity_entries;
        binding->valid_entries = valid_entries;
        binding->host = new_host;
        binding->host_size = mob != NULL ? mob->gbo.size : 0;
        g_free(old_host);
    }

    return vmsvga3d_d3d10_state_cotable_replay_live(
        s, cid, type, old_capacity_entries, valid_entries, grow);
}

static void *vmsvga3d_dx_cotable_entry_ptr(struct vmsvga_state_s *s,
                                            uint32_t cid,
                                            SVGACOTableType type,
                                            uint32_t index)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    VMSVGA3DDXCOTable *binding;
    uint32_t offset;

    if (context == NULL || type >= SVGA_COTABLE_MAX) {
        return NULL;
    }

    binding = &context->cotables[type];
    if (index >= binding->capacity_entries || binding->entry_size == 0 ||
        binding->host == NULL) {
        return NULL;
    }

    offset = index * binding->entry_size;
    if (offset > binding->host_size ||
        binding->entry_size > binding->host_size - offset) {
        return NULL;
    }

    return binding->host + offset;
}

static bool vmsvga3d_dx_cotable_readback(struct vmsvga_state_s *s,
                                         uint32_t cid,
                                         SVGACOTableType type)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    VMSVGA3DDXCOTable *binding;
    VMSVGA3DMob *mob;
    uint32_t write_size;

    if (context == NULL || type >= SVGA_COTABLE_MAX) {
        return false;
    }

    binding = &context->cotables[type];
    mob = vmsvga3d_mob_get(s, binding->mobid);
    if (mob == NULL || binding->host == NULL || binding->entry_size == 0) {
        return false;
    }

    write_size = binding->capacity_entries * binding->entry_size;
    if (write_size > binding->host_size) {
        return false;
    }

    write_size = MIN(write_size, mob->gbo.size);

    return vmsvga3d_mob_write(s, mob, 0, binding->host, write_size);
}

static bool vmsvga3d_dx_context_define_backed(struct vmsvga_state_s *s,
                                               uint32_t cid)
{
    SVGAOTableDXContextEntry entry = {
          .cid = cid,
          .mobid = SVGA3D_INVALID_ID,
    };

    if (s != NULL && s->svga3d != NULL &&
        cid < SVGA3D_MAX_CONTEXT_IDS && s->svga3d->dx_contexts[cid] != NULL) {
        vmsvga3d_dxvk_d3d11_flush(s->dxvk);
    }

    if (!vmsvga3d_dx_context_entry_write(s, cid, &entry) ||
        !vmsvga3d_state_dx_context_define(s, cid)) {
        return false;
    }

    s->svga3d->dx_context_ever_defined = true;

    if (s->svga3d->active_dx_context_id == cid) {
        s->svga3d->active_dx_context_id = SVGA3D_INVALID_ID;
    }

    vmsvga3d_dxvk_d3d11_query_context_destroy(s->dxvk, cid);
    vmsvga3d_dxvk_d3d11_state_context_destroy(s->dxvk, cid);
    vmsvga3d_dxvk_d3d11_constant_buffer_context_destroy(s->dxvk, cid);
    vmsvga3d_dxvk_d3d11_view_context_destroy(s->dxvk, cid);
    vmsvga3d_dxvk_d3d11_shader_context_destroy(s->dxvk, cid);

    return true;
}

static bool vmsvga3d_dx_context_destroy_backed(struct vmsvga_state_s *s,
                                                uint32_t cid)
{
    SVGAOTableDXContextEntry entry = { 0 };

    (void)vmsvga3d_dx_context_entry_write(s, cid, &entry);
    if (s != NULL && s->svga3d != NULL &&
        s->svga3d->active_dx_context_id == cid) {
        s->svga3d->active_dx_context_id = SVGA3D_INVALID_ID;
    }

    if (s != NULL && s->svga3d != NULL &&
        cid < SVGA3D_MAX_CONTEXT_IDS && s->svga3d->dx_contexts[cid] != NULL) {
        vmsvga3d_dxvk_d3d11_flush(s->dxvk);
    }

    if (!vmsvga3d_state_dx_context_destroy(s, cid)) {
        return false;
    }

    vmsvga3d_dxvk_d3d11_query_context_destroy(s->dxvk, cid);
    vmsvga3d_dxvk_d3d11_state_context_destroy(s->dxvk, cid);
    vmsvga3d_dxvk_d3d11_constant_buffer_context_destroy(s->dxvk, cid);
    vmsvga3d_dxvk_d3d11_view_context_destroy(s->dxvk, cid);
    vmsvga3d_dxvk_d3d11_shader_context_destroy(s->dxvk, cid);

    return true;
}

static bool vmsvga3d_dx_context_bind_backed(
    struct vmsvga_state_s *s, const SVGA3dCmdDXBindContext *command)
{
    SVGAOTableDXContextEntry entry;
    SVGADXContextMobFormat saved;
    const SVGADXContextMobFormat *valid_contents = NULL;
    VMSVGA3DMob *mob;

    if (command == NULL) {
        return false;
    }

    if (command->mobid != SVGA3D_INVALID_ID) {
        struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;

        if (state == NULL ||
            !vmsvga3d_otable_index_valid(&state->otables[SVGA_OTABLE_MOB],
                                         command->mobid,
                                         sizeof(SVGAOTableMobEntry))) {
            return false;
        }
    }

    if (!vmsvga3d_dx_context_entry_read(s, command->cid, &entry)) {
        return false;
    }

    if (command->mobid != entry.mobid && entry.mobid != SVGA3D_INVALID_ID) {
        if (vmsvga3d_state_dx_context_readback(s, command->cid, &saved)) {
            mob = vmsvga3d_mob_get(s, entry.mobid);
            if (mob != NULL) {
                (void)vmsvga3d_mob_write(s, mob, 0, &saved, sizeof(saved));
            }
        }
    }

    if (command->mobid != SVGA3D_INVALID_ID) {
        if (command->validContents) {
            mob = vmsvga3d_mob_get(s, command->mobid);
            if (mob != NULL &&
                vmsvga3d_mob_read(s, mob, 0, &saved, sizeof(saved))) {
                valid_contents = &saved;
            }
        }
        (void)vmsvga3d_state_dx_context_bind(s, command->cid, valid_contents);
        if (valid_contents != NULL &&
            s->svga3d->active_dx_context_id == command->cid) {
            s->svga3d->active_dx_context_id = SVGA3D_INVALID_ID;
        }
    }

    entry.mobid = command->mobid;

    return vmsvga3d_dx_context_entry_write(s, command->cid, &entry);
}

static bool vmsvga3d_dx_context_readback_backed(struct vmsvga_state_s *s,
                                                 uint32_t cid)
{
    SVGAOTableDXContextEntry entry;
    SVGADXContextMobFormat contents;
    VMSVGA3DMob *mob;

    if (!vmsvga3d_dx_context_entry_read(s, cid, &entry)) {
        return false;
    }

    if (entry.mobid == SVGA3D_INVALID_ID) {
        return true;
    }

    mob = vmsvga3d_mob_get(s, entry.mobid);
    if (mob == NULL) {
        return true;
    }

    return vmsvga3d_state_dx_context_readback(s, cid, &contents) &&
           vmsvga3d_mob_write(s, mob, 0, &contents, sizeof(contents));
}

#define VMSVGA3D_WIRE_SET_OTABLE_BASE64_SIZE 24u
#define VMSVGA3D_WIRE_GROW_OTABLE_SIZE 24u
#define VMSVGA3D_WIRE_DEFINE_GB_MOB64_SIZE 20u

static bool vmsvga3d_handle_set_otable_base(struct vmsvga_state_s *s,
                                             uint32_t cmd, int32_t *len,
                                             uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (cmd == SVGA_3D_CMD_SET_OTABLE_BASE &&
        size >= sizeof(SVGA3dCmdSetOTableBase)) {
        const SVGA3dCmdSetOTableBase *body = payload;
        bool result;

        result = vmsvga3d_otable_set_or_grow(
            s, body->type, body->baseAddress, body->sizeInBytes,
            body->validSizeInBytes, body->ptDepth, false);
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "OTABLE op=SET type=%u base=0x%016" PRIx64 " size=%u valid=%u "
            "ptDepth=%u result=%s",
            body->type, (uint64_t)body->baseAddress, body->sizeInBytes,
            body->validSizeInBytes, body->ptDepth, result ? "OK" : "REJECT");
    } else if (cmd == SVGA_3D_CMD_SET_OTABLE_BASE64 &&
               size >= VMSVGA3D_WIRE_SET_OTABLE_BASE64_SIZE) {
        const uint8_t *body = payload;
        SVGAOTableType type = (SVGAOTableType)ldl_le_p(body);
        PPN64 base = ldq_le_p(body + 4);
        uint32_t table_size = ldl_le_p(body + 12);
        uint32_t valid_size = ldl_le_p(body + 16);
        SVGAMobFormat format = (SVGAMobFormat)ldl_le_p(body + 20);
        bool result;

        result = vmsvga3d_otable_set_or_grow(
            s, type, base, table_size, valid_size, format, false);
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "OTABLE op=SET64 type=%u base=0x%016" PRIx64 " size=%u valid=%u "
            "ptDepth=%u result=%s",
            type, (uint64_t)base, table_size, valid_size, format,
            result ? "OK" : "REJECT");
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_grow_otable(struct vmsvga_state_s *s,
                                        uint32_t cmd, int32_t *len,
                                        uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= VMSVGA3D_WIRE_GROW_OTABLE_SIZE) {
        const uint8_t *body = payload;

        (void)vmsvga3d_otable_set_or_grow(
            s, (SVGAOTableType)ldl_le_p(body), ldq_le_p(body + 4),
            ldl_le_p(body + 12), ldl_le_p(body + 16),
            (SVGAMobFormat)ldl_le_p(body + 20), true);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_gb_surface_image_live(
    struct vmsvga_state_s *s, const SVGA3dSurfaceImageId *image_id,
    VMSVGA3DSurface **surface_out, VMSVGA3DSurfaceImage **image_out,
    uint32_t *subresource_out)
{
    struct vmsvga3d_state_s *state;
    VMSVGA3DSurface *surface;
    VMSVGA3DSurfaceImage *image;
    ptrdiff_t subresource;

    if (s == NULL || image_id == NULL || surface_out == NULL ||
        image_out == NULL || subresource_out == NULL ||
        image_id->sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    state = s->svga3d;
    if (state == NULL) {
        return false;
    }

    surface = state->surfaces[image_id->sid];
    if (!vmsvga3d_surface_image(surface, image_id, &image)) {
        return false;
    }

    subresource = image - surface->mips;
    if (subresource < 0 || (uint64_t)subresource > UINT32_MAX) {
        return false;
    }

    *surface_out = surface;
    *image_out = image;
    *subresource_out = (uint32_t)subresource;

    return true;
}

static bool vmsvga3d_gb_update_image_live(
    struct vmsvga_state_s *s, const SVGA3dSurfaceImageId *image_id,
    const SVGA3dBox *box)
{
    SVGA3dCmdDXUpdateSubResource command = {0};
    VMSVGA3DSurface *surface;
    VMSVGA3DSurfaceImage *image;
    uint32_t subresource;

    if (box == NULL ||
        !vmsvga3d_gb_surface_image_live(s, image_id, &surface, &image,
                                        &subresource)) {
        return false;
    }

    (void)surface;
    (void)image;
    command.sid = image_id->sid;
    command.subResource = subresource;
    command.box = *box;

    return vmsvga3d_d3d10_update_subresource_live(s, &command);
}

static bool vmsvga3d_gb_readback_image_live(
    struct vmsvga_state_s *s, const SVGA3dSurfaceImageId *image_id)
{
    SVGA3dCmdDXReadbackSubResource command = {0};
    VMSVGA3DSurface *surface;
    VMSVGA3DSurfaceImage *image;
    uint32_t subresource;

    if (!vmsvga3d_gb_surface_image_live(s, image_id, &surface, &image,
                                        &subresource)) {
        return false;
    }

    (void)surface;
    (void)image;
    command.sid = image_id->sid;
    command.subResource = subresource;

    return vmsvga3d_d3d10_readback_subresource_live(s, &command);
}

static bool vmsvga3d_gb_surface_transfer_live(struct vmsvga_state_s *s,
                                               uint32_t sid,
                                               bool readback)
{
    SVGAOTableSurfaceEntry entry;
    VMSVGA3DSurface *surface;
    VMSVGA3DMob *mob;
    uint32_t levels;
    uint32_t array_index;
    bool success = true;

    if (s == NULL || s->svga3d == NULL || sid >= SVGA3D_MAX_SURFACE_IDS ||
        !vmsvga3d_otable_read(s, SVGA_OTABLE_SURFACE, sid, sizeof(entry),
                              &entry, sizeof(entry))) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, le32_to_cpu(entry.mobid));
    if (mob == NULL) {
        return true;
    }

    surface = s->svga3d->surfaces[sid];
    levels = le32_to_cpu(entry.numMipLevels);
    if (surface == NULL || surface->mips == NULL ||
        surface->array_elements == 0 || levels == 0) {
        return false;
    }

    for (array_index = 0; array_index < surface->array_elements;
         array_index++) {
        uint32_t mipmap;

        for (mipmap = 0; mipmap < levels; mipmap++) {
            SVGA3dSurfaceImageId image_id = {0};
            VMSVGA3DSurface *mapped_surface;
            VMSVGA3DSurfaceImage *image;
            uint32_t subresource;
            bool rc;

            image_id.sid = sid;
            image_id.face = array_index;
            image_id.mipmap = mipmap;
            if (!vmsvga3d_gb_surface_image_live(s, &image_id, &mapped_surface,
                                                &image, &subresource)) {
                success = false;
                break;
            }
            (void)mapped_surface;
            (void)subresource;
            if (readback) {
                rc = vmsvga3d_gb_readback_image_live(s, &image_id);
            } else {
                SVGA3dBox box = {0};

                box.w = image->size.width;
                box.h = image->size.height;
                box.d = image->size.depth;
                rc = vmsvga3d_gb_update_image_live(s, &image_id, &box);
            }
            if (!rc) {
                /* VirtualBox's AssertRCBreak only stops the current mip loop. */
                success = false;
                break;
            }
        }
    }

    return success;
}

static bool vmsvga3d_gb_surface_invalidate_live(
    struct vmsvga_state_s *s, const SVGA3dSurfaceImageId *image_id,
    uint32_t sid, bool entire_surface)
{
    VMSVGA3DSurface *surface;

    if (s == NULL || s->svga3d == NULL || sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    surface = s->svga3d->surfaces[sid];
    if (surface == NULL || surface->dxvk_surface == NULL) {
        return false;
    }

    if (!entire_surface) {
        VMSVGA3DSurface *mapped_surface;
        VMSVGA3DSurfaceImage *image;
        uint32_t subresource;

        if (image_id == NULL ||
            !vmsvga3d_gb_surface_image_live(s, image_id, &mapped_surface,
                                            &image, &subresource)) {
            return false;
        }
        (void)mapped_surface;
        (void)image;
        (void)subresource;
    }

    /* VirtualBox leaves buffer invalidation as a no-op.  For textures it
     * destroys all cached views, irrespective of the requested image. */
    if (surface->format != SVGA3D_BUFFER) {
        vmsvga3d_dxvk_d3d11_surface_invalidate_views(surface->dxvk_surface);
    }

    return true;
}

static bool vmsvga3d_gb_zero_surface_live(struct vmsvga_state_s *s,
                                           uint32_t sid)
{
    VMSVGA3DSurface *surface;
    VMSVGA3DD3D9TransferSurface d3d9_info = {0};
    bool d3d9_resident;
    bool d3d11_resident;
    uint32_t subresource;
    bool success = true;

    if (s == NULL || s->svga3d == NULL || sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    surface = s->svga3d->surfaces[sid];
    if (surface == NULL || surface->mips == NULL ||
        surface->dxvk_surface == NULL) {
        return false;
    }

    d3d9_resident =
        vmsvga3d_d3d9_runtime_surface_info(s, surface, &d3d9_info) &&
        d3d9_info.resident;
    d3d11_resident =
        vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface);

    /* A conflicting dual residency is already invalid.  Zero is a full
     * replacement, so dropping both native copies leaves the zeroed CPU shadow
     * as an unambiguous authoritative representation. */
    if (d3d9_resident && d3d11_resident) {
        vmsvga3d_dxvk_surface_evict(surface->dxvk_surface);
        d3d9_resident = false;
        d3d11_resident = false;
    }

    for (subresource = 0; subresource < surface->mip_count; subresource++) {
        VMSVGA3DSurfaceImage *image = &surface->mips[subresource];
        SVGA3dBox box = {0};

        if (image->data == NULL || image->pitch == 0 ||
            image->plane_size == 0 || image->data_size == 0) {
            success = false;
            continue;
        }

        /* WRITE_ZERO_SURFACE and HINT_ZERO_SURFACE both establish zero as the
         * current surface contents.  Keep the bound MOB untouched; a later
         * explicit readback synchronizes guest backing in the normal GB path. */
        memset(image->data, 0, image->data_size);

        if (d3d9_resident) {
            VMSVGA3DD3D9AccelResult result =
                vmsvga3d_d3d9_runtime_upload_surface_image(
                    s, surface, image, subresource, 0, image->data_size);

            if (result != VMSVGA3D_D3D9_ACCEL_COMPLETE) {
                /* The CPU shadow is complete, so eviction is a correct
                 * fallback for D3D9 resource types that cannot be updated in
                 * place (for example a depth/stencil surface). */
                vmsvga3d_dxvk_surface_evict(surface->dxvk_surface);
                d3d9_resident = false;
                d3d11_resident = false;
                VMVGA_TRACE_LOCAL(
                    VMVGA_TRACE_3D,
                    "COHERENCE op=zero sid=%u sub=%u backend=d3d9 action=evict",
                    sid, subresource);
            }
        } else if (d3d11_resident && surface->multisample_count <= 1) {
            VMSVGA3DD3D10Box native_box;

            native_box.left = 0;
            native_box.top = 0;
            native_box.front = 0;
            native_box.right = image->size.width;
            native_box.bottom = image->size.height;
            native_box.back = image->size.depth;

            if (!vmsvga3d_dxvk_d3d11_update_subresource(
                    s->dxvk, surface->dxvk_surface, subresource, &native_box,
                    image->data, image->pitch, image->plane_size)) {
                vmsvga3d_dxvk_surface_evict(surface->dxvk_surface);
                d3d11_resident = false;
                VMVGA_TRACE_LOCAL(
                    VMVGA_TRACE_3D,
                    "COHERENCE op=zero sid=%u sub=%u backend=d3d11 action=evict",
                    sid, subresource);
            }
        } else if (d3d11_resident) {
            /* D3D11 UpdateSubresource cannot update multisampled resources. */
            vmsvga3d_dxvk_surface_evict(surface->dxvk_surface);
            d3d11_resident = false;
        }

        box.w = image->size.width;
        box.h = image->size.height;
        box.d = image->size.depth;
        (void)vmsvga3d_surface_changed_live(s, sid, subresource, &box);
    }

    return success;
}

static bool vmsvga3d_handle_zero_surface(struct vmsvga_state_s *s,
                                         uint32_t cmd, int32_t *len,
                                         uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(uint32_t)) {
        uint32_t sid = ldl_le_p(payload);
        bool success = vmsvga3d_gb_zero_surface_live(s, sid);

        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "ZERO-SURFACE cmd=%u kind=%s sid=%u result=%s",
            cmd,
            cmd == SVGA_3D_CMD_WRITE_ZERO_SURFACE ? "WRITE" : "HINT",
            sid, success ? "OK" : "PARTIAL");
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_gb_surface_sync(struct vmsvga_state_s *s,
                                            uint32_t cmd, int32_t *len,
                                            uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    switch (cmd) {
    case SVGA_3D_CMD_UPDATE_GB_IMAGE:
        if (size >= sizeof(SVGA3dCmdUpdateGBImage)) {
            const SVGA3dCmdUpdateGBImage *body = payload;
            (void)vmsvga3d_gb_update_image_live(s, &body->image, &body->box);
        }
        break;
    case SVGA_3D_CMD_UPDATE_GB_SURFACE:
        if (size >= sizeof(SVGA3dCmdUpdateGBSurface)) {
            const SVGA3dCmdUpdateGBSurface *body = payload;
            (void)vmsvga3d_gb_surface_transfer_live(s, body->sid, false);
        }
        break;
    case SVGA_3D_CMD_READBACK_GB_IMAGE:
        if (size >= sizeof(SVGA3dCmdReadbackGBImage)) {
            const SVGA3dCmdReadbackGBImage *body = payload;
            (void)vmsvga3d_gb_readback_image_live(s, &body->image);
        }
        break;
    case SVGA_3D_CMD_READBACK_GB_SURFACE:
        if (size >= sizeof(SVGA3dCmdReadbackGBSurface)) {
            const SVGA3dCmdReadbackGBSurface *body = payload;
            (void)vmsvga3d_gb_surface_transfer_live(s, body->sid, true);
        }
        break;
    case SVGA_3D_CMD_READBACK_GB_IMAGE_PARTIAL:
        if (size >= sizeof(SVGA3dCmdReadbackGBImagePartial)) {
            const SVGA3dCmdReadbackGBImagePartial *body = payload;
            bool success = vmsvga3d_gb_readback_image_partial_live(
                s, &body->image, &body->box, body->invertBox != 0);

            if (!success) {
                VMVGA_TRACE_LOCAL(
                    VMVGA_TRACE_3D,
                    "GB-READBACK-PARTIAL sid=%u face=%u mip=%u "
                    "box=%u,%u,%u/%ux%ux%u invert=%u result=REJECT",
                    body->image.sid, body->image.face, body->image.mipmap,
                    body->box.x, body->box.y, body->box.z, body->box.w,
                    body->box.h, body->box.d, body->invertBox);
            }
        }
        break;
    case SVGA_3D_CMD_INVALIDATE_GB_IMAGE:
        if (size >= sizeof(SVGA3dCmdInvalidateGBImage)) {
            const SVGA3dCmdInvalidateGBImage *body = payload;
            (void)vmsvga3d_gb_surface_invalidate_live(
                s, &body->image, body->image.sid, false);
        }
        break;
    case SVGA_3D_CMD_INVALIDATE_GB_SURFACE:
        if (size >= sizeof(SVGA3dCmdInvalidateGBSurface)) {
            const SVGA3dCmdInvalidateGBSurface *body = payload;
            (void)vmsvga3d_gb_surface_invalidate_live(s, NULL, body->sid, true);
        }
        break;
    default:
        break;
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_gb_screen_target_entry_read(
    struct vmsvga_state_s *s, uint32_t stid,
    SVGAOTableScreenTargetEntry *entry)
{
    return s != NULL && entry != NULL && stid == VMSVGA_SCREEN_V1_ID &&
           vmsvga3d_otable_read(s, SVGA_OTABLE_SCREENTARGET, stid,
                                sizeof(*entry), entry, sizeof(*entry));
}

static bool vmsvga3d_screen_target_present_live(
    struct vmsvga_state_s *s, uint32_t sid, uint32_t subresource,
    const SVGA3dRect *rect, bool readback, bool copy_to_screen)
{
    VMSVGA3DSurface *surface;
    VMSVGA3DSurfaceImage *image;
    const struct svga3d_surface_desc *desc;
    SVGA3dCopyRect copy;

    if (s == NULL || rect == NULL || s->svga3d == NULL) {
        return false;
    }

    if (sid == SVGA3D_INVALID_ID ||
        sid != s->svga3d->active_screen_target_sid) {
        return true;
    }

    if (sid >= SVGA3D_MAX_SURFACE_IDS || subresource != 0) {
        return false;
    }

    surface = s->svga3d->surfaces[sid];
    if (surface == NULL) {
        return false;
    }
    if (!surface->screen_target_content_valid) {
        return true;
    }
    if (surface->mips == NULL || surface->mip_count == 0 ||
        surface->format == SVGA3D_BUFFER ||
        (surface->surface_flags & SVGA3D_SURFACE_SCREENTARGET) == 0 ||
        (surface->surface_flags &
         (SVGA3D_SURFACE_1D | SVGA3D_SURFACE_VOLUME)) != 0 ||
        surface->mips[subresource].size.depth != 1 ||
        !vmsvga3d_present_format(surface, &desc)) {
        return false;
    }

    image = &surface->mips[subresource];

    if (rect->w == 0 || rect->h == 0 || rect->x >= image->size.width ||
        rect->y >= image->size.height) {
        return false;
    }

    copy.x = rect->x;
    copy.y = rect->y;
    copy.w = MIN(rect->w, image->size.width - rect->x);
    copy.h = MIN(rect->h, image->size.height - rect->y);
    copy.srcx = copy.x;
    copy.srcy = copy.y;

    if (copy.w == 0 || copy.h == 0) {
        return false;
    }

    /*
     * The screen-target path is restricted by vmsvga3d_present_format() to
     * single-sample, uncompressed 2D pixel formats. Read back only the dirty
     * rectangle into the existing CPU shadow. If the backend cannot service
     * a boxed readback, retain the previous full-subresource path as a
     * correctness fallback.
     */
    if (readback) {
        SVGA3dRect readback_rect = {
            .x = copy.srcx,
            .y = copy.srcy,
            .w = copy.w,
            .h = copy.h,
        };

        if (!vmsvga3d_d3d10_readback_image_rect_live(
                s, surface, subresource, &readback_rect,
                desc->bytes_per_block) &&
            !vmsvga3d_d3d10_readback_image_live(
                s, surface, subresource)) {
            return false;
        }
    }

    if (copy_to_screen) {
        if (!vmsvga3d_present_screen_rect(
                s, surface, image, desc, &copy, true)) {
            return false;
        }
    } else if (!vmsvga3d_present_screen_rect_damage_only(
                   s, surface, image, desc, &copy)) {
        return false;
    }

    /*
     * An unbacked Screen Object uses the handoff mirror as its lasting host
     * storage. Once a complete 3D frame has replaced the seeded transition
     * image, the handoff is complete; no scanout rebind is needed because the
     * storage pointer itself does not change.
     */
    if (s->screen_handoff_active && !s->screen_backing_valid &&
        copy.x == 0 && copy.y == 0 &&
        copy.w == s->screen_width && copy.h == s->screen_height) {
        s->screen_handoff_active = false;
        s->screen_handoff_same_backing = false;
        s->screen_handoff_skipped_same_backing_full = false;
    }

    return true;
}

static uint64_t vmsvga3d_screen_target_rect_area(const SVGA3dRect *rect)
{
    return (uint64_t)rect->w * rect->h;
}

static bool vmsvga3d_screen_target_rect_contains(
    const SVGA3dRect *outer, const SVGA3dRect *inner)
{
    uint64_t outer_right = (uint64_t)outer->x + outer->w;
    uint64_t outer_bottom = (uint64_t)outer->y + outer->h;
    uint64_t inner_right = (uint64_t)inner->x + inner->w;
    uint64_t inner_bottom = (uint64_t)inner->y + inner->h;

    return inner->x >= outer->x && inner->y >= outer->y &&
           inner_right <= outer_right && inner_bottom <= outer_bottom;
}

static bool vmsvga3d_screen_target_clip_rect(
    const VMSVGA3DSurface *surface, SVGA3dRect *rect)
{
    uint32_t width;
    uint32_t height;

    if (surface == NULL || rect == NULL || surface->mips == NULL ||
        surface->mip_count == 0 || rect->w == 0 || rect->h == 0) {
        return false;
    }

    width = surface->mips[0].size.width;
    height = surface->mips[0].size.height;
    if (rect->x >= width || rect->y >= height) {
        return false;
    }

    rect->w = MIN(rect->w, width - rect->x);
    rect->h = MIN(rect->h, height - rect->y);
    return rect->w != 0 && rect->h != 0;
}

static SVGA3dRect vmsvga3d_screen_target_rect_union(
    const SVGA3dRect *a, const SVGA3dRect *b)
{
    uint64_t left = MIN((uint64_t)a->x, (uint64_t)b->x);
    uint64_t top = MIN((uint64_t)a->y, (uint64_t)b->y);
    uint64_t right = MAX(
        MIN((uint64_t)UINT32_MAX + 1, (uint64_t)a->x + a->w),
        MIN((uint64_t)UINT32_MAX + 1, (uint64_t)b->x + b->w));
    uint64_t bottom = MAX(
        MIN((uint64_t)UINT32_MAX + 1, (uint64_t)a->y + a->h),
        MIN((uint64_t)UINT32_MAX + 1, (uint64_t)b->y + b->h));
    SVGA3dRect merged = {
        .x = (uint32_t)left,
        .y = (uint32_t)top,
        .w = (uint32_t)MIN(right - left, (uint64_t)UINT32_MAX),
        .h = (uint32_t)MIN(bottom - top, (uint64_t)UINT32_MAX),
    };

    return merged;
}

static uint64_t vmsvga3d_screen_target_area_add(uint64_t a, uint64_t b)
{
    return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}

static bool vmsvga3d_screen_target_merge_is_efficient(
    const SVGA3dRect *a, const SVGA3dRect *b, const SVGA3dRect *merged)
{
    uint64_t combined_area = vmsvga3d_screen_target_area_add(
        vmsvga3d_screen_target_rect_area(a),
        vmsvga3d_screen_target_rect_area(b));
    uint64_t merged_area = vmsvga3d_screen_target_rect_area(merged);
    uint64_t allowance = combined_area / 4;
    uint64_t limit = vmsvga3d_screen_target_area_add(combined_area, allowance);

    /*
     * A small amount of overdraw is cheaper than an extra GPU readback/map,
     * but keep genuinely sparse updates separate.  This allows nearby damage
     * to merge while preventing distant rectangles from becoming a near-full
     * screen bounding box.
     */
    return merged_area <= limit;
}

static void vmsvga3d_screen_target_merge_cheapest_pair(
    struct vmsvga3d_state_s *state)
{
    uint32_t best_i = 0;
    uint32_t best_j = 1;
    uint64_t best_cost = UINT64_MAX;
    uint64_t best_area = UINT64_MAX;
    uint32_t i;
    uint32_t j;

    if (state->screen_target_dirty_count < 2) {
        return;
    }

    for (i = 0; i < state->screen_target_dirty_count; i++) {
        for (j = i + 1; j < state->screen_target_dirty_count; j++) {
            const SVGA3dRect *a = &state->screen_target_dirty_rects[i];
            const SVGA3dRect *b = &state->screen_target_dirty_rects[j];
            SVGA3dRect merged = vmsvga3d_screen_target_rect_union(a, b);
            uint64_t merged_area =
                vmsvga3d_screen_target_rect_area(&merged);
            uint64_t combined_area = vmsvga3d_screen_target_area_add(
                vmsvga3d_screen_target_rect_area(a),
                vmsvga3d_screen_target_rect_area(b));
            uint64_t cost = merged_area > combined_area
                                ? merged_area - combined_area
                                : 0;

            if (cost < best_cost ||
                (cost == best_cost && merged_area < best_area)) {
                best_i = i;
                best_j = j;
                best_cost = cost;
                best_area = merged_area;
            }
        }
    }

    state->screen_target_dirty_rects[best_i] =
        vmsvga3d_screen_target_rect_union(
            &state->screen_target_dirty_rects[best_i],
            &state->screen_target_dirty_rects[best_j]);
    state->screen_target_dirty_count--;
    if (best_j != state->screen_target_dirty_count) {
        state->screen_target_dirty_rects[best_j] =
            state->screen_target_dirty_rects[state->screen_target_dirty_count];
    }
}

static bool vmsvga3d_screen_target_mark_dirty_live(
    struct vmsvga_state_s *s, uint32_t sid, uint32_t subresource,
    const SVGA3dRect *rect, bool presentation)
{
    struct vmsvga3d_state_s *state;
    VMSVGA3DSurface *surface;
    SVGA3dRect dirty;
    uint32_t i;

    if (s == NULL || rect == NULL || s->svga3d == NULL) {
        return false;
    }

    state = s->svga3d;
    if (sid != state->active_screen_target_sid || subresource != 0 ||
        rect->w == 0 || rect->h == 0) {
        return true;
    }

    dirty = *rect;
    surface = sid < SVGA3D_MAX_SURFACE_IDS ? state->surfaces[sid] : NULL;
    if (surface == NULL || !surface->screen_target_content_valid) {
        return true;
    }
    if (surface != NULL) {
        uint32_t width;
        uint32_t height;

        /* Only visible, non-empty damage can release a deferred frontend. */
        if (!vmsvga3d_screen_target_clip_rect(surface, &dirty)) {
            return true;
        }

        width = surface->mips[0].size.width;
        height = surface->mips[0].size.height;

        /*
         * Ordinary render-target writes only establish that the active target
         * has content.  They are not a presentation boundary: exposing one
         * here lets a display refresh race a clear/draw sequence before the
         * guest's actual UPDATE_GB_SCREENTARGET or PRESENTBLT.  Keep storage
         * mutation separate from presentation for the lifetime of the target.
         */
        if (!presentation) {
            return true;
        }

        if (state->screen_target_dirty_count == 0) {
            state->screen_target_dirty_sid = sid;
        } else if (state->screen_target_dirty_sid != sid) {
            if (vmsvga_trace_flight_enabled()) {
                fprintf(stderr,
                        "VMVGA-SCREEN-TARGET phase=dirty-owner-mismatch "
                        "active-sid=%u dirty-sid=%u new-sid=%u rects=%u\n",
                        state->active_screen_target_sid,
                        state->screen_target_dirty_sid, sid,
                        state->screen_target_dirty_count);
            }
            return false;
        }

        if (s->screen_frontend_deferred && presentation &&
            vmsvga_trace_flight_enabled()) {
            fprintf(stderr,
                    "VMVGA-SCREEN-HANDOFF phase=present-pending sid=%u "
                    "rect=%u,%u/%ux%u\n",
                    sid, dirty.x, dirty.y, dirty.w, dirty.h);
        }

        if (dirty.x == 0 && dirty.y == 0 &&
            dirty.w == width && dirty.h == height) {
            /* A queued full-screen update already covers an identical one. */
            if (state->screen_target_dirty_count == 1 &&
                vmsvga3d_screen_target_rect_contains(
                    &state->screen_target_dirty_rects[0], &dirty)) {
                return true;
            }

            state->screen_target_dirty_count = 1;
            state->screen_target_dirty_rects[0] = dirty;
            return true;
        }
    }

retry_merge:
    for (i = 0; i < state->screen_target_dirty_count; i++) {
        SVGA3dRect merged;

        /* Duplicate or fully covered damage needs no new readback. */
        if (vmsvga3d_screen_target_rect_contains(
                &state->screen_target_dirty_rects[i], &dirty)) {
            return true;
        }

        /* A larger new rectangle supersedes any queued rectangle it covers. */
        if (vmsvga3d_screen_target_rect_contains(
                &dirty, &state->screen_target_dirty_rects[i])) {
            state->screen_target_dirty_count--;
            if (i != state->screen_target_dirty_count) {
                state->screen_target_dirty_rects[i] =
                    state->screen_target_dirty_rects[
                        state->screen_target_dirty_count];
            }
            goto retry_merge;
        }

        merged = vmsvga3d_screen_target_rect_union(
            &dirty, &state->screen_target_dirty_rects[i]);
        if (!vmsvga3d_screen_target_merge_is_efficient(
                &dirty, &state->screen_target_dirty_rects[i], &merged)) {
            continue;
        }

        dirty = merged;
        state->screen_target_dirty_count--;
        if (i != state->screen_target_dirty_count) {
            state->screen_target_dirty_rects[i] =
                state->screen_target_dirty_rects[
                    state->screen_target_dirty_count];
        }
        goto retry_merge;
    }

    if (state->screen_target_dirty_count ==
        VMSVGA3D_SCREEN_TARGET_DAMAGE_RECTS) {
        vmsvga3d_screen_target_merge_cheapest_pair(state);
        goto retry_merge;
    }

    state->screen_target_dirty_rects[state->screen_target_dirty_count++] = dirty;
    return true;
}

static bool vmsvga3d_surface_changed_live(
    struct vmsvga_state_s *s, uint32_t sid, uint32_t subresource,
    const SVGA3dBox *box)
{
    VMSVGA3DSurface *surface;
    SVGA3dRect rect;

    if (s == NULL || box == NULL || s->svga3d == NULL) {
        return false;
    }

    if (box->z != 0 || box->d == 0 || box->w == 0 || box->h == 0) {
        return true;
    }

    rect.x = box->x;
    rect.y = box->y;
    rect.w = box->w;
    rect.h = box->h;

    surface = sid < SVGA3D_MAX_SURFACE_IDS ? s->svga3d->surfaces[sid] : NULL;
    if (surface != NULL && subresource == 0) {
        /* A writer establishes ScreenTarget readiness only if it actually
         * intersects visible subresource 0.  Out-of-bounds writes must not
         * make a later BIND/UPDATE release a deferred frontend. */
        if (!vmsvga3d_screen_target_clip_rect(surface, &rect)) {
            return true;
        }
        surface->screen_target_content_valid = true;
    }

    return vmsvga3d_screen_target_mark_dirty_live(
        s, sid, subresource, &rect, false);
}

static bool vmsvga3d_surface_presented_live(
    struct vmsvga_state_s *s, uint32_t sid, uint32_t subresource,
    const SVGA3dBox *box)
{
    VMSVGA3DSurface *surface;
    SVGA3dRect rect;

    if (s == NULL || box == NULL || s->svga3d == NULL) {
        return false;
    }

    if (box->z != 0 || box->d == 0 || box->w == 0 || box->h == 0) {
        return true;
    }

    rect.x = box->x;
    rect.y = box->y;
    rect.w = box->w;
    rect.h = box->h;

    surface = sid < SVGA3D_MAX_SURFACE_IDS ? s->svga3d->surfaces[sid] : NULL;
    if (surface != NULL && subresource == 0) {
        if (!vmsvga3d_screen_target_clip_rect(surface, &rect)) {
            return true;
        }
        surface->screen_target_content_valid = true;
    }

    return vmsvga3d_screen_target_mark_dirty_live(
        s, sid, subresource, &rect, true);
}

static bool vmsvga3d_screen_target_flush_live(struct vmsvga_state_s *s)
{
    struct vmsvga3d_state_s *state;
    SVGA3dRect rects[VMSVGA3D_SCREEN_TARGET_DAMAGE_RECTS];
    uint32_t rect_count;
    uint32_t sid;
    uint32_t i;
    bool batch_readback = false;
    bool direct_screen_readback = false;

    if (s == NULL || s->svga3d == NULL) {
        return true;
    }

    state = s->svga3d;
    rect_count = state->screen_target_dirty_count;
    if (rect_count == 0) {
        return true;
    }

    sid = state->screen_target_dirty_sid;
    if (sid == SVGA3D_INVALID_ID || sid != state->active_screen_target_sid) {
        if (vmsvga_trace_flight_enabled()) {
            fprintf(stderr,
                    "VMVGA-SCREEN-TARGET phase=flush-owner-mismatch "
                    "active-sid=%u dirty-sid=%u rects=%u\n",
                    state->active_screen_target_sid, sid, rect_count);
        }
        return false;
    }

    memcpy(rects, state->screen_target_dirty_rects,
           rect_count * sizeof(rects[0]));

    if (sid < SVGA3D_MAX_SURFACE_IDS) {
        VMSVGA3DSurface *surface = state->surfaces[sid];
        const struct svga3d_surface_desc *desc = NULL;

        if (surface != NULL && surface->screen_target_content_valid &&
            surface->mips != NULL && surface->mip_count != 0 &&
            surface->format != SVGA3D_BUFFER &&
            (surface->surface_flags & SVGA3D_SURFACE_SCREENTARGET) != 0 &&
            (surface->surface_flags &
             (SVGA3D_SURFACE_1D | SVGA3D_SURFACE_VOLUME)) == 0 &&
            surface->mips[0].size.depth == 1 &&
            vmsvga3d_present_format(surface, &desc)) {
            VMSVGA3DD3D9TransferSurface d3d9_info;
            uint8_t *screen_base = NULL;
            size_t screen_size = 0;
            uint32_t screen_stride = 0;
            bool d3d9_resident =
                vmsvga3d_d3d9_runtime_surface_info(s, surface, &d3d9_info) &&
                d3d9_info.resident;
            bool d3d11_resident =
                vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface);
            bool direct_candidate = false;

            /* ScreenTarget binding is backend-neutral.  Synchronize from the
             * renderer that actually owns the surface instead of assuming a
             * vGPU10 ScreenTarget must be D3D11-resident. */
            if (d3d9_resident && d3d11_resident) {
                return false;
            }

            if (desc->bytes_per_block == 4 &&
                (surface->format == SVGA3D_X8R8G8B8 ||
                 surface->format == SVGA3D_A8R8G8B8) &&
                vmsvga_screen_storage(s, &screen_base, &screen_size,
                                      &screen_stride) &&
                screen_size <= UINT32_MAX) {
                direct_candidate = true;
                for (i = 0; i < rect_count; i++) {
                    if (rects[i].x >= s->screen_width ||
                        rects[i].y >= s->screen_height ||
                        rects[i].w > s->screen_width - rects[i].x ||
                        rects[i].h > s->screen_height - rects[i].y) {
                        direct_candidate = false;
                        break;
                    }
                }
            }

            if (d3d9_resident) {
                batch_readback =
                    vmsvga3d_d3d9_runtime_readback_surface_rects(
                        s, surface, &surface->mips[0], 0, rects, rect_count,
                        desc->bytes_per_block,
                        direct_candidate ? screen_base : NULL,
                        direct_candidate ? screen_stride : 0,
                        direct_candidate ? (uint32_t)screen_size : 0) ==
                    VMSVGA3D_D3D9_ACCEL_COMPLETE;
                direct_screen_readback = batch_readback && direct_candidate;

                /* Keep the proven full-surface path as a correctness fallback
                 * if a host D3D9 implementation rejects the scratch-copy
                 * optimization for a particular render-target format. */
                if (!batch_readback) {
                    if (vmsvga3d_d3d9_runtime_readback_surface_image(
                            s, surface, &surface->mips[0], 0) !=
                        VMSVGA3D_D3D9_ACCEL_COMPLETE) {
                        return false;
                    }
                    batch_readback = true;
                    direct_screen_readback = false;
                }
            } else if (!d3d11_resident) {
                /* No accelerated backend owns newer contents; the canonical
                 * CPU shadow is already authoritative. */
                batch_readback = true;
            }

            if (d3d11_resident && (rect_count > 1 || direct_candidate)) {
                batch_readback = vmsvga3d_d3d10_readback_image_rects_live(
                    s, surface, 0, rects, rect_count, desc->bytes_per_block,
                    direct_candidate ? screen_base : NULL,
                    direct_candidate ? screen_stride : 0,
                    direct_candidate ? (uint32_t)screen_size : 0);
                direct_screen_readback = batch_readback && direct_candidate;
            }
        }
    }

    /*
     * Do not consume queued presentation damage until every readback succeeds.
     * A partial failure while the frontend is deferred must leave the old
     * frontend visible and retry the same presentation on the next refresh.
     */
    for (i = 0; i < rect_count; i++) {
        if (!vmsvga3d_screen_target_present_live(
                s, sid, 0, &rects[i], !batch_readback,
                !direct_screen_readback)) {
            if (s->screen_frontend_deferred &&
                vmsvga_trace_flight_enabled()) {
                fprintf(stderr,
                        "VMVGA-SCREEN-HANDOFF phase=frontend-wait sid=%u "
                        "reason=present-failed rect=%u,%u/%ux%u\n",
                        sid, rects[i].x, rects[i].y, rects[i].w,
                        rects[i].h);
            }
            return false;
        }
    }

    state->screen_target_dirty_sid = SVGA3D_INVALID_ID;
    state->screen_target_dirty_count = 0;
    memset(state->screen_target_dirty_rects, 0,
           sizeof(state->screen_target_dirty_rects));

    if (s->screen_frontend_deferred) {
        s->screen_frontend_deferred = false;
        if (vmsvga_trace_flight_enabled()) {
            fprintf(stderr,
                    "VMVGA-SCREEN-HANDOFF phase=frontend-ready sid=%u "
                    "size=%ux%u rects=%u\n",
                    sid, s->screen_width, s->screen_height, rect_count);
        }
    }

    return true;
}

static bool vmsvga3d_screen_target_quiesce_live(struct vmsvga_state_s *s)
{
    struct vmsvga3d_state_s *state;

    if (s == NULL || s->svga3d == NULL) {
        return true;
    }

    state = s->svga3d;
    if (state->screen_target_dirty_count == 0) {
        state->screen_target_dirty_sid = SVGA3D_INVALID_ID;
        return true;
    }

    if (state->screen_target_dirty_sid == SVGA3D_INVALID_ID ||
        state->screen_target_dirty_sid != state->active_screen_target_sid) {
        if (vmsvga_trace_flight_enabled()) {
            fprintf(stderr,
                    "VMVGA-SCREEN-TARGET phase=quiesce-owner-mismatch "
                    "active-sid=%u dirty-sid=%u rects=%u\n",
                    state->active_screen_target_sid,
                    state->screen_target_dirty_sid,
                    state->screen_target_dirty_count);
        }
        return false;
    }

    if (vmsvga_trace_flight_enabled()) {
        fprintf(stderr,
                "VMVGA-SCREEN-TARGET phase=quiesce sid=%u rects=%u\n",
                state->screen_target_dirty_sid,
                state->screen_target_dirty_count);
    }

    return vmsvga3d_screen_target_flush_live(s);
}

static bool vmsvga3d_gb_screen_target_update_live(
    struct vmsvga_state_s *s, uint32_t stid, const SVGA3dRect *rect)
{
    SVGAOTableScreenTargetEntry target;
    SVGAOTableSurfaceEntry surface_entry;
    uint32_t sid;

    if (s == NULL || rect == NULL || s->svga3d == NULL ||
        !vmsvga3d_gb_screen_target_entry_read(s, stid, &target) ||
        le32_to_cpu(target.image.face) != 0 ||
        le32_to_cpu(target.image.mipmap) != 0) {
        return false;
    }

    sid = le32_to_cpu(target.image.sid);
    if (sid == SVGA3D_INVALID_ID) {
        return true;
    }

    if (!vmsvga3d_otable_read(s, SVGA_OTABLE_SURFACE, sid,
                              sizeof(surface_entry), &surface_entry,
                              sizeof(surface_entry))) {
        return false;
    }

    /* VirtualBox gates an explicit UPDATE on the OTable surface MOB, then its
     * backend presents the surface that was actually bound successfully.  If a
     * preceding BIND changed the OTable but failed backend validation, these can
     * deliberately be different SIDs. */
    if (le32_to_cpu(surface_entry.mobid) == SVGA3D_INVALID_ID) {
        return true;
    }

    return vmsvga3d_screen_target_mark_dirty_live(
        s, s->svga3d->active_screen_target_sid, 0, rect, true);
}

static bool vmsvga3d_handle_gb_screen_target(struct vmsvga_state_s *s,
                                              uint32_t cmd, int32_t *len,
                                              uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    switch (cmd) {
    case SVGA_3D_CMD_DEFINE_GB_SCREENTARGET:
        if (size >= sizeof(SVGA3dCmdDefineGBScreenTarget)) {
            const SVGA3dCmdDefineGBScreenTarget *body = payload;
            SVGAOTableScreenTargetEntry entry;
            uint32_t flags;

            if (body->stid != VMSVGA_SCREEN_V1_ID || body->width == 0 ||
                body->height == 0 || body->width > VMSVGA_MAX_WIDTH ||
                body->height > VMSVGA_MAX_HEIGHT) {
                break;
            }
            if (!vmsvga3d_screen_target_quiesce_live(s)) {
                break;
            }
            memset(&entry, 0, sizeof(entry));
            entry.image.sid = cpu_to_le32(SVGA3D_INVALID_ID);
            entry.width = cpu_to_le32(body->width);
            entry.height = cpu_to_le32(body->height);
            entry.xRoot = cpu_to_le32(body->xRoot);
            entry.yRoot = cpu_to_le32(body->yRoot);
            entry.flags = cpu_to_le32(body->flags);
            entry.dpi = cpu_to_le32(body->dpi);
            if (!vmsvga3d_otable_write(s, SVGA_OTABLE_SCREENTARGET, body->stid,
                                       sizeof(entry), &entry, sizeof(entry))) {
                break;
            }
            s->svga3d->active_screen_target_sid = SVGA3D_INVALID_ID;
            s->svga3d->screen_target_dirty_sid = SVGA3D_INVALID_ID;
            /*
             * Do not destroy the currently visible Screen Object before a
             * redefine. vmsvga_screen_define() deliberately seeds its handoff
             * mirror from the existing frontend; destroying here would free
             * that storage while the frontend can still be displaying it and
             * would also throw away the previous good frame.
             */
            flags = SVGA_SCREEN_MUST_BE_SET;
            if (body->flags & SVGA_STFLAG_PRIMARY) {
                flags |= SVGA_SCREEN_IS_PRIMARY;
            }
            (void)vmsvga_screen_define(s, body->stid, flags, body->width,
                                       body->height, body->xRoot, body->yRoot,
                                       false, SVGA_GMR_NULL, 0, 0, 0,
                                       s->vgpu_generation == VMSVGA_VGPU_10 ||
                                       s->vgpu_generation == VMSVGA_VGPU_11);
        }
        break;

    case SVGA_3D_CMD_DESTROY_GB_SCREENTARGET:
        if (size >= sizeof(SVGA3dCmdDestroyGBScreenTarget)) {
            const SVGA3dCmdDestroyGBScreenTarget *body = payload;
            SVGAOTableScreenTargetEntry entry;

            if (body->stid != VMSVGA_SCREEN_V1_ID) {
                break;
            }
            if (!vmsvga3d_screen_target_quiesce_live(s)) {
                break;
            }
            memset(&entry, 0, sizeof(entry));
            if (vmsvga3d_otable_write(s, SVGA_OTABLE_SCREENTARGET, body->stid,
                                      sizeof(entry), &entry, sizeof(entry))) {
                if (s->svga3d != NULL) {
                    s->svga3d->active_screen_target_sid = SVGA3D_INVALID_ID;
                    s->svga3d->screen_target_dirty_sid = SVGA3D_INVALID_ID;
                }
                (void)vmsvga_screen_destroy(s, body->stid);
            }
        }
        break;

    case SVGA_3D_CMD_BIND_GB_SCREENTARGET:
        if (size >= sizeof(SVGA3dCmdBindGBScreenTarget)) {
            const SVGA3dCmdBindGBScreenTarget *body = payload;
            SVGAOTableScreenTargetEntry entry;
            uint32_t old_sid;
            bool update_screen = false;

            if (body->stid != VMSVGA_SCREEN_V1_ID || body->image.face != 0 ||
                body->image.mipmap != 0 || s == NULL || s->svga3d == NULL) {
                break;
            }
            if (body->image.sid != SVGA3D_INVALID_ID &&
                !vmsvga3d_otable_index_valid(&s->svga3d->otables[SVGA_OTABLE_SURFACE],
                                             body->image.sid,
                                             sizeof(SVGAOTableSurfaceEntry))) {
                break;
            }
            if (!vmsvga3d_gb_screen_target_entry_read(s, body->stid, &entry)) {
                break;
            }
            old_sid = le32_to_cpu(entry.image.sid);
            if (body->image.sid != old_sid) {
                update_screen = body->image.sid != SVGA3D_INVALID_ID;
                entry.image.sid = cpu_to_le32(body->image.sid);
                entry.image.face = cpu_to_le32(body->image.face);
                entry.image.mipmap = cpu_to_le32(body->image.mipmap);
                if (!vmsvga3d_otable_write(s, SVGA_OTABLE_SCREENTARGET, body->stid,
                                           sizeof(entry), &entry, sizeof(entry))) {
                    break;
                }
            }
            if (!vmsvga3d_d3d10_screen_target_bind_live(s, body->image.sid)) {
                break;
            }
            if (update_screen) {
                SVGA3dRect rect = {0};

                rect.w = le32_to_cpu(entry.width);
                rect.h = le32_to_cpu(entry.height);
                /* Binding a new surface is itself a flip.  Treat it as a
                 * presentation boundary outside deferred takeover; the bind
                 * path clears content-valid while deferred, so an empty new
                 * target still cannot become visible prematurely.  Unlike
                 * explicit UPDATE, VBox does not require the Surface OTable
                 * entry to have a valid MOB here. */
                (void)vmsvga3d_screen_target_mark_dirty_live(
                    s, s->svga3d->active_screen_target_sid, 0, &rect, true);
            }
        }
        break;

    case SVGA_3D_CMD_UPDATE_GB_SCREENTARGET:
        if (size >= sizeof(SVGA3dCmdUpdateGBScreenTarget)) {
            const SVGA3dCmdUpdateGBScreenTarget *body = payload;

            if (body->stid == VMSVGA_SCREEN_V1_ID) {
                (void)vmsvga3d_gb_screen_target_update_live(s, body->stid,
                                                            &body->rect);
            }
        }
        break;

    default:
        break;
    }

    g_free(payload);
    return true;
}


static bool vmsvga3d_handle_gb_context(struct vmsvga_state_s *s,
                                        uint32_t cmd, int32_t *len,
                                        uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (cmd == SVGA_3D_CMD_DEFINE_GB_CONTEXT &&
        size >= sizeof(SVGA3dCmdDefineGBContext)) {
        const SVGA3dCmdDefineGBContext *body = payload;
        SVGAOTableContextEntry entry = {
            .cid = body->cid,
            .mobid = SVGA3D_INVALID_ID,
        };

        if (vmsvga3d_gb_context_entry_write(s, body->cid, &entry) &&
            vmsvga3d_state_context_define(s, body->cid)) {
            vmsvga3d_gb_query_cancel_context(s, body->cid, "gb-context-define");
            vmsvga3d_dxvk_d3d9_query_context_destroy(s->dxvk, body->cid);
        }
    } else if (cmd == SVGA_3D_CMD_DESTROY_GB_CONTEXT &&
               size >= sizeof(SVGA3dCmdDestroyGBContext)) {
        const SVGA3dCmdDestroyGBContext *body = payload;
        SVGAOTableContextEntry entry = { 0 };

        (void)vmsvga3d_gb_context_entry_write(s, body->cid, &entry);
        if (vmsvga3d_state_context_destroy(s, body->cid)) {
            vmsvga3d_gb_query_cancel_context(s, body->cid, "gb-context-destroy");
            vmsvga3d_dxvk_d3d9_query_context_destroy(s->dxvk, body->cid);
        }
    } else if (cmd == SVGA_3D_CMD_BIND_GB_CONTEXT &&
               size >= sizeof(SVGA3dCmdBindGBContext)) {
        const SVGA3dCmdBindGBContext *body = payload;
        SVGAOTableContextEntry entry;
        VMSVGA3DMob *mob;
        SVGAGBContextData contents;

        if (vmsvga3d_gb_context_entry_read(s, body->cid, &entry) &&
            (body->mobid == SVGA3D_INVALID_ID ||
             vmsvga3d_mob_get(s, body->mobid) != NULL)) {
            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "GB-CONTEXT phase=bind cid=%u old-mob=%u new-mob=%u "
                "validContents=%u",
                body->cid, entry.mobid, body->mobid, body->validContents);
            if (entry.mobid != SVGA3D_INVALID_ID &&
                entry.mobid != body->mobid &&
                vmsvga3d_gb_context_snapshot(s, body->cid, &contents)) {
                mob = vmsvga3d_mob_get(s, entry.mobid);
                if (mob != NULL) {
                    if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
                        bool write_ok = vmsvga3d_mob_write(
                            s, mob, 0, &contents, sizeof(contents));

                        VMVGA_TRACE_LOCAL(
                            VMVGA_TRACE_3D,
                            "GB-CONTEXT phase=snapshot-write cid=%u mob=%u "
                            "result=%s",
                            body->cid, entry.mobid,
                            write_ok ? "OK" : "FAIL");
                    } else {
                        (void)vmsvga3d_mob_write(s, mob, 0, &contents,
                                                 sizeof(contents));
                    }
                }
            }

            if (body->mobid != SVGA3D_INVALID_ID && body->validContents) {
                mob = vmsvga3d_mob_get(s, body->mobid);
                if (mob != NULL &&
                    vmsvga3d_mob_read(s, mob, 0, &contents,
                                      sizeof(contents))) {
                    if (VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D)) {
                        bool restore_ok = vmsvga3d_gb_context_restore(
                            s, body->cid, &contents);

                        VMVGA_TRACE_LOCAL(
                            VMVGA_TRACE_3D,
                            "GB-CONTEXT phase=restore-read cid=%u mob=%u "
                            "result=%s",
                            body->cid, body->mobid,
                            restore_ok ? "OK" : "FAIL");
                    } else {
                        (void)vmsvga3d_gb_context_restore(
                            s, body->cid, &contents);
                    }
                }
            }
            entry.mobid = body->mobid;
            (void)vmsvga3d_gb_context_entry_write(s, body->cid, &entry);
        }
    } else if (cmd == SVGA_3D_CMD_READBACK_GB_CONTEXT &&
               size >= sizeof(SVGA3dCmdReadbackGBContext)) {
        const SVGA3dCmdReadbackGBContext *body = payload;
        SVGAOTableContextEntry entry;
        VMSVGA3DMob *mob;
        SVGAGBContextData contents;

        if (vmsvga3d_gb_context_entry_read(s, body->cid, &entry) &&
            entry.mobid != SVGA3D_INVALID_ID &&
            vmsvga3d_gb_context_snapshot(s, body->cid, &contents)) {
            mob = vmsvga3d_mob_get(s, entry.mobid);
            if (mob != NULL) {
                (void)vmsvga3d_mob_write(s, mob, 0, &contents,
                                         sizeof(contents));
            }
        }
    } else if (cmd == SVGA_3D_CMD_INVALIDATE_GB_CONTEXT &&
               size >= sizeof(SVGA3dCmdInvalidateGBContext)) {
        const SVGA3dCmdInvalidateGBContext *body = payload;

        /* Matching the DX/VirtualBox model: invalidation permits the device
         * copy to be discarded, but does not alter the guest backing MOB. */
        if (vmsvga3d_state_context_destroy(s, body->cid)) {
            (void)vmsvga3d_state_context_define(s, body->cid);
            vmsvga3d_gb_query_cancel_context(s, body->cid, "gb-context-invalidate");
            vmsvga3d_dxvk_d3d9_query_context_destroy(s->dxvk, body->cid);
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_gb_shader(struct vmsvga_state_s *s,
                                       uint32_t cmd, int32_t *len,
                                       uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (cmd == SVGA_3D_CMD_DEFINE_GB_SHADER &&
        size >= sizeof(SVGA3dCmdDefineGBShader)) {
        const SVGA3dCmdDefineGBShader *body = payload;
        SVGAOTableShaderEntry entry = {
            .type = body->type,
            .sizeInBytes = body->sizeInBytes,
            .offsetInBytes = 0,
            .mobid = SVGA3D_INVALID_ID,
        };

        if (body->shid < SVGA3D_MAX_SHADERIDS &&
            body->sizeInBytes != 0 &&
            body->sizeInBytes <= SVGA3D_MAX_SHADER_MEMORY_BYTES &&
            body->sizeInBytes % sizeof(uint32_t) == 0) {
            (void)vmsvga3d_gb_shader_entry_write(s, body->shid, &entry);
        }
    } else if (cmd == SVGA_3D_CMD_DESTROY_GB_SHADER &&
               size >= sizeof(SVGA3dCmdDestroyGBShader)) {
        const SVGA3dCmdDestroyGBShader *body = payload;
        SVGAOTableShaderEntry entry;
        uint32_t cid;

        if (vmsvga3d_gb_shader_entry_read(s, body->shid, &entry)) {
            SVGAOTableShaderEntry clear = { 0 };
            clear.mobid = SVGA3D_INVALID_ID;
            (void)vmsvga3d_gb_shader_entry_write(s, body->shid, &clear);
            for (cid = 0; cid < SVGA3D_MAX_CONTEXT_IDS; cid++) {
                (void)vmsvga3d_state_shader_destroy(s, cid, body->shid,
                                                    entry.type);
            }
        }
    } else if (cmd == SVGA_3D_CMD_BIND_GB_SHADER &&
               size >= sizeof(SVGA3dCmdBindGBShader)) {
        const SVGA3dCmdBindGBShader *body = payload;
        SVGAOTableShaderEntry entry;
        VMSVGA3DMob *mob;

        if (vmsvga3d_gb_shader_entry_read(s, body->shid, &entry) &&
            (body->mobid == SVGA3D_INVALID_ID ||
             (mob = vmsvga3d_mob_get(s, body->mobid)) != NULL)) {
            if (body->mobid == SVGA3D_INVALID_ID ||
                (body->offsetInBytes <= mob->gbo.size &&
                 entry.sizeInBytes <= mob->gbo.size - body->offsetInBytes)) {
                entry.mobid = body->mobid;
                entry.offsetInBytes = body->offsetInBytes;
                (void)vmsvga3d_gb_shader_entry_write(s, body->shid, &entry);
            }
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_define_gb_surface(struct vmsvga_state_s *s,
                                              uint32_t cmd, int32_t *len,
                                              uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (cmd == SVGA_3D_CMD_DEFINE_GB_SURFACE &&
        size >= sizeof(SVGA3dCmdDefineGBSurface)) {
        const SVGA3dCmdDefineGBSurface *body = payload;

        (void)vmsvga3d_gb_surface_define_live(
            s, body->sid, body->surfaceFlags, body->format, body->numMipLevels,
            body->multisampleCount,
            body->multisampleCount > 1 ? SVGA3D_MS_PATTERN_STANDARD
                                       : SVGA3D_MS_PATTERN_NONE,
            0, body->autogenFilter, &body->size, 0, 0);
    } else if (cmd == SVGA_3D_CMD_DEFINE_GB_SURFACE_V2 &&
               size >= sizeof(SVGA3dCmdDefineGBSurface_v2)) {
        const SVGA3dCmdDefineGBSurface_v2 *body = payload;

        (void)vmsvga3d_gb_surface_define_live(
            s, body->sid, body->surfaceFlags, body->format, body->numMipLevels,
            body->multisampleCount,
            body->multisampleCount > 1 ? SVGA3D_MS_PATTERN_STANDARD
                                       : SVGA3D_MS_PATTERN_NONE,
            0, body->autogenFilter, &body->size, body->arraySize, 0);
    } else if (cmd == SVGA_3D_CMD_DEFINE_GB_SURFACE_V3 &&
               size >= sizeof(SVGA3dCmdDefineGBSurface_v3)) {
        const SVGA3dCmdDefineGBSurface_v3 *body = payload;

        (void)vmsvga3d_gb_surface_define_live(
            s, body->sid, body->surfaceFlags, body->format, body->numMipLevels,
            body->multisampleCount, body->multisamplePattern,
            body->qualityLevel, body->autogenFilter, &body->size,
            body->arraySize, 0);
    } else if (cmd == SVGA_3D_CMD_DEFINE_GB_SURFACE_V4 &&
               size >= sizeof(SVGA3dCmdDefineGBSurface_v4) &&
               s->vgpu_generation == VMSVGA_VGPU_11) {
        const SVGA3dCmdDefineGBSurface_v4 *body = payload;

        (void)vmsvga3d_gb_surface_define_live(
            s, body->sid, body->surfaceFlags, body->format, body->numMipLevels,
            body->multisampleCount, body->multisamplePattern,
            body->qualityLevel, body->autogenFilter, &body->size,
            body->arraySize, body->bufferByteStride);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_destroy_gb_surface(struct vmsvga_state_s *s,
                                               uint32_t cmd, int32_t *len,
                                               uint32_t fifo_start)
{
    SVGA3dCmdDestroyGBSurface *body;
    SVGAOTableSurfaceEntry entry;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        if (s != NULL && s->svga3d != NULL &&
            body->sid == s->svga3d->active_screen_target_sid &&
            !vmsvga3d_screen_target_quiesce_live(s)) {
            g_free(payload);
            return true;
        }
        memset(&entry, 0, sizeof(entry));
        entry.mobid = cpu_to_le32(SVGA3D_INVALID_ID);
        (void)vmsvga3d_otable_write(s, SVGA_OTABLE_SURFACE, body->sid,
                                    sizeof(entry), &entry, sizeof(entry));
        vmsvga3d_surface_destroy_live(s, body->sid);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_define_gb_mob(struct vmsvga_state_s *s,
                                          uint32_t cmd, int32_t *len,
                                          uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

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
    } else if (cmd == SVGA_3D_CMD_REDEFINE_GB_MOB64 &&
               size >= VMSVGA3D_WIRE_DEFINE_GB_MOB64_SIZE) {
        const uint8_t *body = payload;

        (void)vmsvga3d_mob_redefine(
            s, ldl_le_p(body), (SVGAMobFormat)ldl_le_p(body + 4),
            ldq_le_p(body + 8), ldl_le_p(body + 16));
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_destroy_gb_mob(struct vmsvga_state_s *s,
                                           uint32_t cmd, int32_t *len,
                                           uint32_t fifo_start)
{
    SVGA3dCmdDestroyGBMob *body;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size >= sizeof(*body)) {
        body = payload;
        (void)vmsvga3d_mob_destroy(s, body->mobid);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_gb_mob_fence(struct vmsvga_state_s *s,
                                             uint32_t cmd, int32_t *len,
                                             uint32_t fifo_start)
{
    SVGA3dCmdGBMobFence *body;
    VMSVGA3DMob *mob;
    void *payload;
    uint32_t size;
    bool ok = false;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "GB-MOB-FENCE result=REJECT reason=PAYLOAD_READ "
                          "fifo=0x%08x",
                          fifo_start);
        return true;
    }

    if (size >= sizeof(*body)) {
        uint32_t value;

        body = payload;
        value = body->value;

        /* VMware normally places this fence immediately after ended GB
         * queries.  Give pending D3D9 queries a flushed, nonblocking poll
         * before publishing command-stream progress to the guest.  A query
         * which is still pending remains armed on the query-only timer. */
        if (s->svga3d != NULL && s->svga3d->gb_queries != NULL) {
            vmsvga3d_d3d9_process_pending_gb_queries(s, "GB_MOB_FENCE");
            vmsvga3d_d3d9_schedule_gb_query_poll(s);
        }

        mob = vmsvga3d_mob_get(s, body->mobId);
        ok = mob != NULL &&
             vmsvga3d_mob_write(s, mob, body->mobOffset,
                                &value, sizeof(value));

        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "GB-MOB-FENCE value=0x%08x mobid=%u offset=0x%08x result=%s",
            value, body->mobId, body->mobOffset, ok ? "OK" : "REJECT");
    } else {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "GB-MOB-FENCE result=REJECT reason=PACKET_SHORT "
                          "fifo=0x%08x bytes=%u expected=%zu",
                          fifo_start, size, sizeof(*body));
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_gart(struct vmsvga_state_s *s, uint32_t cmd,
                                  int32_t *len, uint32_t fifo_start)
{
    void *payload;
    uint32_t size;
    bool ok = false;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    switch (cmd) {
    case SVGA_3D_CMD_ENABLE_GART:
        if (size >= sizeof(SVGA3dCmdEnableGart)) {
            const SVGA3dCmdEnableGart *body = payload;

            ok = vmsvga3d_gart_enable_live(
                s, body->mobid, body->mustBeZero, body->initialized);
            if (!ok) {
                VMVGA_TRACE_LOCAL(
                    VMVGA_TRACE_3D,
                    "GART enable mobid=%u mustBeZero=%u initialized=%u result=REJECT",
                    body->mobid, body->mustBeZero, body->initialized);
            }
        }
        break;

    case SVGA_3D_CMD_DISABLE_GART:
        if (size == 0) {
            struct vmsvga3d_state_s *state = s != NULL ? s->svga3d : NULL;

            if (state != NULL) {
                vmsvga3d_gart_disable_live(s);
                ok = true;
                VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                                  "GART disable result=OK");
            }
        }
        break;

    case SVGA_3D_CMD_MAP_MOB_INTO_GART:
        if (size >= sizeof(SVGA3dCmdMapMobIntoGart)) {
            const SVGA3dCmdMapMobIntoGart *body = payload;

            ok = vmsvga3d_gart_map_mob_live(s, body->mobid,
                                             body->gartOffset);
            if (!ok) {
                VMVGA_TRACE_LOCAL(
                    VMVGA_TRACE_3D,
                    "GART map mobid=%u offset=0x%08x result=REJECT",
                    body->mobid, body->gartOffset);
            }
        }
        break;

    case SVGA_3D_CMD_UNMAP_GART_RANGE:
        if (size >= sizeof(SVGA3dCmdUnmapGartRange)) {
            const SVGA3dCmdUnmapGartRange *body = payload;

            ok = vmsvga3d_gart_unmap_live(s, body->gartOffset,
                                           body->numPages);
            if (!ok) {
                VMVGA_TRACE_LOCAL(
                    VMVGA_TRACE_3D,
                    "GART unmap offset=0x%08x pages=%u result=REJECT",
                    body->gartOffset, body->numPages);
            }
        }
        break;

    default:
        break;
    }

    if (!ok &&
        ((cmd == SVGA_3D_CMD_ENABLE_GART &&
          size < sizeof(SVGA3dCmdEnableGart)) ||
         (cmd == SVGA_3D_CMD_DISABLE_GART && size != 0) ||
         (cmd == SVGA_3D_CMD_MAP_MOB_INTO_GART &&
          size < sizeof(SVGA3dCmdMapMobIntoGart)) ||
         (cmd == SVGA_3D_CMD_UNMAP_GART_RANGE &&
          size < sizeof(SVGA3dCmdUnmapGartRange)))) {
        VMVGA_TRACE_LOCAL(VMVGA_TRACE_3D,
                          "GART cmd=%u size=%u result=REJECT-SIZE",
                          cmd, size);
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_bind_gb_surface(struct vmsvga_state_s *s,
                                             uint32_t cmd, int32_t *len,
                                             uint32_t fifo_start)
{
    const SVGA3dCmdBindGBSurface *body;
    const SVGA3dCmdBindGBSurfaceWithPitch *pitched_body;
    struct vmsvga3d_state_s *state;
    SVGAOTableSurfaceEntry entry;
    uint32_t sid;
    SVGAMobId mobid;
    uint32_t mob_pitch = 0;
    uint32_t surface_flags;
    bool pitched;
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    pitched = cmd == SVGA_3D_CMD_BIND_GB_SURFACE_WITH_PITCH;
    if (pitched) {
        if (size < sizeof(*pitched_body)) {
            g_free(payload);
            return true;
        }
        pitched_body = payload;
        sid = pitched_body->sid;
        mobid = pitched_body->mobid;
        mob_pitch = pitched_body->baseLevelPitch;
    } else {
        if (size < sizeof(*body)) {
            g_free(payload);
            return true;
        }
        body = payload;
        sid = body->sid;
        mobid = body->mobid;
    }

    state = s != NULL ? s->svga3d : NULL;
    if (state != NULL &&
        (mobid == SVGA3D_INVALID_ID ||
         vmsvga3d_otable_index_valid(&state->otables[SVGA_OTABLE_MOB],
                                      mobid,
                                      sizeof(SVGAOTableMobEntry))) &&
        vmsvga3d_otable_read(s, SVGA_OTABLE_SURFACE, sid,
                             sizeof(entry), &entry, sizeof(entry))) {
        surface_flags = le32_to_cpu(entry.surface1Flags);
        entry.mobid = cpu_to_le32(mobid);
        if (pitched && mobid != SVGA3D_INVALID_ID && mob_pitch != 0) {
            entry.mobPitch = cpu_to_le32(mob_pitch);
            surface_flags |= (uint32_t)SVGA3D_SURFACE_MOB_PITCH;
        } else {
            entry.mobPitch = cpu_to_le32(0);
            surface_flags &= ~(uint32_t)SVGA3D_SURFACE_MOB_PITCH;
        }
        entry.surface1Flags = cpu_to_le32(surface_flags);
        (void)vmsvga3d_otable_write(s, SVGA_OTABLE_SURFACE, sid,
                                    sizeof(entry), &entry, sizeof(entry));
        if (pitched) {
            VMVGA_TRACE_LOCAL(
                VMVGA_TRACE_3D,
                "GB-BIND-PITCH sid=%u mobid=%u pitch=%u flags=0x%08x",
                sid, mobid, mob_pitch, surface_flags);
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_cond_bind_gb_surface(
    struct vmsvga_state_s *s, uint32_t cmd, int32_t *len,
    uint32_t fifo_start)
{
    const SVGA3dCmdCondBindGBSurface *body;
    struct vmsvga3d_state_s *state;
    SVGAOTableSurfaceEntry entry;
    SVGAMobId old_mobid;
    bool host_mob_present;
    void *payload;
    uint32_t size;

    (void)cmd;
    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size < sizeof(*body)) {
        g_free(payload);
        return true;
    }

    body = payload;
    state = s != NULL ? s->svga3d : NULL;
    if (state != NULL &&
        (body->mobid == SVGA3D_INVALID_ID ||
         vmsvga3d_otable_index_valid(&state->otables[SVGA_OTABLE_MOB],
                                      body->mobid,
                                      sizeof(SVGAOTableMobEntry))) &&
        vmsvga3d_otable_read(s, SVGA_OTABLE_SURFACE, body->sid,
                             sizeof(entry), &entry, sizeof(entry))) {
        old_mobid = le32_to_cpu(entry.mobid);
        host_mob_present = body->mobid == SVGA3D_INVALID_ID ||
                           vmsvga3d_mob_get(s, body->mobid) != NULL;
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "COND-BIND sid=%u test=%u old=%u new=%u flags=0x%08x host-mob=%s",
            body->sid, body->testMobid, old_mobid, body->mobid, body->flags,
            host_mob_present ? "present" : "missing");
        if (old_mobid == body->testMobid && old_mobid != body->mobid) {
            if (body->flags & SVGA3D_COND_BIND_GB_SURFACE_FLAG_READBACK) {
                (void)vmsvga3d_gb_surface_transfer_live(s, body->sid, true);
            }
            if (body->flags & SVGA3D_COND_BIND_GB_SURFACE_FLAG_UPDATE) {
                (void)vmsvga3d_gb_surface_transfer_live(s, body->sid, false);
            }

            entry.mobid = cpu_to_le32(body->mobid);
            (void)vmsvga3d_otable_write(s, SVGA_OTABLE_SURFACE, body->sid,
                                        sizeof(entry), &entry, sizeof(entry));
        }
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_dx_context_lifecycle(
    struct vmsvga_state_s *s, uint32_t cmd, int32_t *len,
    uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    switch (cmd) {
    case SVGA_3D_CMD_DX_DEFINE_CONTEXT:
        if (size >= sizeof(SVGA3dCmdDXDefineContext)) {
            const SVGA3dCmdDXDefineContext *body = payload;
            (void)vmsvga3d_dx_context_define_backed(s, body->cid);
        }
        break;
    case SVGA_3D_CMD_DX_DESTROY_CONTEXT:
        if (size >= sizeof(SVGA3dCmdDXDestroyContext)) {
            const SVGA3dCmdDXDestroyContext *body = payload;
            (void)vmsvga3d_dx_context_destroy_backed(s, body->cid);
        }
        break;
    case SVGA_3D_CMD_DX_BIND_CONTEXT:
        if (size >= sizeof(SVGA3dCmdDXBindContext)) {
            (void)vmsvga3d_dx_context_bind_backed(s, payload);
        }
        break;
    case SVGA_3D_CMD_DX_READBACK_CONTEXT:
        if (size >= sizeof(SVGA3dCmdDXReadbackContext)) {
            const SVGA3dCmdDXReadbackContext *body = payload;
            (void)vmsvga3d_dx_context_readback_backed(s, body->cid);
        }
        break;
    default:
        break;
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_dx_cotable(struct vmsvga_state_s *s,
                                       uint32_t cmd, int32_t *len,
                                       uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    switch (cmd) {
    case SVGA_3D_CMD_DX_SET_COTABLE:
        if (size >= sizeof(SVGA3dCmdDXSetCOTable)) {
            const SVGA3dCmdDXSetCOTable *body = payload;
            (void)vmsvga3d_dx_cotable_set_or_grow(
                s, body->cid, body->mobid, body->type,
                body->validSizeInBytes, false);
        }
        break;
    case SVGA_3D_CMD_DX_GROW_COTABLE:
        if (size >= sizeof(SVGA3dCmdDXGrowCOTable)) {
            const SVGA3dCmdDXGrowCOTable *body = payload;
            (void)vmsvga3d_dx_cotable_set_or_grow(
                s, body->cid, body->mobid, body->type,
                body->validSizeInBytes, true);
        }
        break;
    case SVGA_3D_CMD_DX_READBACK_COTABLE:
        if (size >= sizeof(SVGA3dCmdDXReadbackCOTable)) {
            const SVGA3dCmdDXReadbackCOTable *body = payload;
            (void)vmsvga3d_dx_cotable_readback(s, body->cid, body->type);
        }
        break;
    default:
        break;
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_handle_surface_activation(
    struct vmsvga_state_s *s, uint32_t cmd, int32_t *len,
    uint32_t fifo_start)
{
    struct vmsvga3d_state_s *state = s->svga3d;
    VMSVGA3DSurface *surface = NULL;
    void *payload;
    uint32_t size;
    uint32_t sid;
    bool activate;
    bool trace_3d = VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D);

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (size < sizeof(SVGA3dCmdActivateSurface)) {
        if (trace_3d) {
            fprintf(stderr,
                    "VMVGA-SURFACE-ACTIVE cmd=%s result=short bytes=%u expected=%zu\n",
                    cmd == SVGA_3D_CMD_ACTIVATE_SURFACE ? "activate" : "deactivate",
                    size, sizeof(SVGA3dCmdActivateSurface));
        }
        g_free(payload);
        return true;
    }

    sid = ((const SVGA3dCmdActivateSurface *)payload)->sid;
    activate = cmd == SVGA_3D_CMD_ACTIVATE_SURFACE;
    if (state != NULL && sid < SVGA3D_MAX_SURFACE_IDS) {
        surface = state->surfaces[sid];
    }

    if (surface != NULL) {
        if (trace_3d) {
            VMSVGA3DD3D9TransferSurface info;
            bool d3d9_info =
                vmsvga3d_d3d9_runtime_surface_info(s, surface, &info);
            bool d3d11_resident =
                vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface);
            bool previous = surface->legacy_active;

            fprintf(stderr,
                    "VMVGA-SURFACE-ACTIVE cmd=%s sid=%u present=1 old=%u new=%u "
                    "flags=0x%016" PRIx64 " d3d9-info=%u d3d9-resident=%u "
                    "d3d9-bounce=%u d3d11-resident=%u\n",
                    activate ? "activate" : "deactivate", sid, previous,
                    activate, (uint64_t)surface->surface_flags, d3d9_info,
                    d3d9_info ? info.resident : 0,
                    d3d9_info ? info.has_bounce : 0, d3d11_resident);
        }
        surface->legacy_active = activate;
    } else if (trace_3d) {
        fprintf(stderr,
                "VMVGA-SURFACE-ACTIVE cmd=%s sid=%u present=0 old=0 new=%u\n",
                activate ? "activate" : "deactivate", sid, activate);
    }

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
    VMSVGA3D_HANDLER(SVGA_3D_CMD_PRESENT_READBACK, vmsvga3d_handle_present),
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
    VMSVGA3D_HANDLER(SVGA_3D_CMD_ACTIVATE_SURFACE,
                     vmsvga3d_handle_surface_activation),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DEACTIVATE_SURFACE,
                     vmsvga3d_handle_surface_activation),
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
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_SURFACE, vmsvga3d_handle_define_gb_surface),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DESTROY_GB_SURFACE, vmsvga3d_handle_destroy_gb_surface),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_BIND_GB_SURFACE, vmsvga3d_handle_bind_gb_surface),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_COND_BIND_GB_SURFACE,
                     vmsvga3d_handle_cond_bind_gb_surface),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_UPDATE_GB_IMAGE, vmsvga3d_handle_gb_surface_sync),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_UPDATE_GB_SURFACE, vmsvga3d_handle_gb_surface_sync),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_READBACK_GB_IMAGE, vmsvga3d_handle_gb_surface_sync),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_READBACK_GB_SURFACE, vmsvga3d_handle_gb_surface_sync),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_INVALIDATE_GB_IMAGE, vmsvga3d_handle_gb_surface_sync),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_INVALIDATE_GB_SURFACE, vmsvga3d_handle_gb_surface_sync),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_CONTEXT, vmsvga3d_handle_gb_context),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DESTROY_GB_CONTEXT, vmsvga3d_handle_gb_context),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_BIND_GB_CONTEXT, vmsvga3d_handle_gb_context),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_READBACK_GB_CONTEXT, vmsvga3d_handle_gb_context),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_INVALIDATE_GB_CONTEXT, vmsvga3d_handle_gb_context),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_SHADER, vmsvga3d_handle_gb_shader),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DESTROY_GB_SHADER, vmsvga3d_handle_gb_shader),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_BIND_GB_SHADER, vmsvga3d_handle_gb_shader),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_SET_OTABLE_BASE64, vmsvga3d_handle_set_otable_base),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_BEGIN_GB_QUERY, vmsvga3d_handle_gb_query),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_END_GB_QUERY, vmsvga3d_handle_gb_query),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_WAIT_FOR_GB_QUERY, vmsvga3d_handle_gb_query),
    VMSVGA3D_DISCARD(SVGA_3D_CMD_NOP),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_ENABLE_GART, vmsvga3d_handle_gart),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DISABLE_GART, vmsvga3d_handle_gart),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_MAP_MOB_INTO_GART, vmsvga3d_handle_gart),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_UNMAP_GART_RANGE, vmsvga3d_handle_gart),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_SCREENTARGET,
                     vmsvga3d_handle_gb_screen_target),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DESTROY_GB_SCREENTARGET,
                     vmsvga3d_handle_gb_screen_target),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_BIND_GB_SCREENTARGET,
                     vmsvga3d_handle_gb_screen_target),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_UPDATE_GB_SCREENTARGET,
                     vmsvga3d_handle_gb_screen_target),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_READBACK_GB_IMAGE_PARTIAL,
                     vmsvga3d_handle_gb_surface_sync),
    VMSVGA3D_STALL(SVGA_3D_CMD_INVALIDATE_GB_IMAGE_PARTIAL),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_SET_GB_SHADERCONSTS_INLINE,
                     vmsvga3d_handle_set_gb_shader_consts_inline),
    VMSVGA3D_DISCARD(SVGA_3D_CMD_GB_SCREEN_DMA),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_BIND_GB_SURFACE_WITH_PITCH,
                     vmsvga3d_handle_bind_gb_surface),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_GB_MOB_FENCE,
                     vmsvga3d_handle_gb_mob_fence),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_SURFACE_V2, vmsvga3d_handle_define_gb_surface),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_MOB64, vmsvga3d_handle_define_gb_mob),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_REDEFINE_GB_MOB64, vmsvga3d_handle_define_gb_mob),
    VMSVGA3D_DISCARD(SVGA_3D_CMD_NOP_ERROR),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_SET_VERTEX_STREAMS,
                     vmsvga3d_handle_set_vertex_streams),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_SET_VERTEX_DECLS,
                     vmsvga3d_handle_set_vertex_decls),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_SET_VERTEX_DIVISORS,
                     vmsvga3d_handle_set_vertex_divisors),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DRAW, vmsvga3d_handle_draw),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DRAW_INDEXED, vmsvga3d_handle_draw_indexed),
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
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_SURFACE_V3,
                     vmsvga3d_handle_define_gb_surface),
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
    VMSVGA3D_HANDLER(SVGA_3D_CMD_WRITE_ZERO_SURFACE,
                     vmsvga3d_handle_zero_surface),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_HINT_ZERO_SURFACE,
                     vmsvga3d_handle_zero_surface),
    VMSVGA3D_STALL(SVGA_3D_CMD_DX_TRANSFER_TO_BUFFER),
    VMSVGA3D_STALL(SVGA_3D_CMD_DX_SET_STRUCTURE_COUNT),
    VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_BITBLT),
    VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_TRANSBLT),
    VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_STRETCHBLT),
    VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_COLORFILL),
    VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_ALPHABLEND),
    VMSVGA3D_DISCARD(SVGA_3D_CMD_LOGICOPS_CLEARTYPEBLEND),
    VMSVGA3D_HANDLER(SVGA_3D_CMD_DEFINE_GB_SURFACE_V4,
                     vmsvga3d_handle_define_gb_surface),
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

static const VMSVGA3DCommandInfo *vmsvga3d_command_info(uint32_t cmd)
{
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
        }
    }

    return NULL;
}

static bool vmsvga3d_is_dx_command(uint32_t cmd)
{
    if (cmd >= SVGA_3D_CMD_DX_MIN && cmd <= SVGA_3D_CMD_DX_MAX) {
        return true;
    }

    switch (cmd) {
    case SVGA_3D_CMD_DX_GROW_COTABLE:
    case SVGA_3D_CMD_DX_RESOLVE_COPY:
    case SVGA_3D_CMD_DX_PRED_RESOLVE_COPY:
    case SVGA_3D_CMD_DX_PRED_CONVERT_REGION:
    case SVGA_3D_CMD_DX_PRED_CONVERT:
    case SVGA_3D_CMD_DX_DEFINE_UA_VIEW ... SVGA_3D_CMD_DX_DISPATCH_INDIRECT:
    case SVGA_3D_CMD_DX_TRANSFER_TO_BUFFER:
    case SVGA_3D_CMD_DX_SET_STRUCTURE_COUNT:
    case SVGA_3D_CMD_DX_COPY_COTABLE_INTO_MOB:
    case SVGA_3D_CMD_DX_SET_CS_UA_VIEWS:
    case SVGA_3D_CMD_DX_SET_MIN_LOD:
    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW_V2:
    case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT_WITH_MOB:
    case SVGA_3D_CMD_DX_SET_SHADER_IFACE:
    case SVGA_3D_CMD_DX_BIND_STREAMOUTPUT:
    case SVGA_3D_CMD_DX_BIND_SHADER_IFACE:
    case SVGA_3D_CMD_DX_PRED_STAGING_COPY ...
         SVGA_3D_CMD_DX_STAGING_BUFFER_COPY:
        return true;
    default:
        return false;
    }
}

static bool vmsvga3d_trace_fifo_command(uint32_t cmd)
{
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
    case SVGA_3D_CMD_DRAW_PRIMITIVES:
    case SVGA_3D_CMD_PRESENT:
    case SVGA_3D_CMD_PRESENT_READBACK:
    case SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN:
    case SVGA_3D_CMD_SHADER_DEFINE:
    case SVGA_3D_CMD_SHADER_DESTROY:
    case SVGA_3D_CMD_SET_SHADER:
    case SVGA_3D_CMD_SET_SHADER_CONST:
    case SVGA_3D_CMD_SETSCISSORRECT:
    case SVGA_3D_CMD_GENERATE_MIPMAPS:
        return true;
    default:
        return vmsvga3d_is_dx_command(cmd);
    }
}

static const char *vmsvga3d_trace_command_path(uint32_t cmd)
{
    if (vmsvga3d_is_dx_command(cmd)) {
        return "DX";
    }

    return "D3D9";
}

static const char *vmsvga3d_trace_command_action(
    uint32_t cmd, const VMSVGA3DCommandInfo *info)
{
    if (info->handler != NULL || vmsvga3d_is_dx_command(cmd)) {
        return "HANDLE";
    }

    if (info->action == VMSVGA3D_COMMAND_STALL) {
        return "STALL";
    }

    return "DISCARD";
}

static bool vmsvga3d_fifo_dx_command(struct vmsvga_state_s *s,
                                      uint32_t dx_context, uint32_t cmd,
                                      int32_t *len, uint32_t fifo_start)
{
    void *payload;
    uint32_t size;

    if (!vmsvga3d_fifo_read_payload(s, len, fifo_start, &payload, &size)) {
        return true;
    }

    if (!vmsvga3d_d3d10_command(s, dx_context, cmd, payload, size) &&
        (s->vgpu_generation != VMSVGA_VGPU_11 ||
         !vmsvga3d_d3d11_command(s, dx_context, cmd, payload, size))) {
        VMVGA_TRACE_LOCAL(
            VMVGA_TRACE_3D,
            "3D-DX-UNHANDLED name=%s id=%u cid=%u fifo=0x%08x",
            vmsvga3d_command_info(cmd) != NULL
                ? vmsvga3d_command_info(cmd)->name
                : "UNKNOWN",
            cmd, dx_context, fifo_start);
        g_free(payload);
        vmsvga3d_fifo_rewind(s, len, fifo_start);
        return true;
    }

    g_free(payload);
    return true;
}

static bool vmsvga3d_fifo_command(struct vmsvga_state_s *s,
                                   uint32_t dx_context, uint32_t cmd,
                                   int32_t *len, uint32_t fifo_start)
{
    const VMSVGA3DCommandInfo *info;
    uint32_t payload_size = UINT32_MAX;
    bool trace_3d;

    if (!s->svga3d_capable) {
        return false;
    }

    if (cmd < SVGA_3D_CMD_LEGACY_BASE || cmd > SVGA_3D_CMD_FUTURE_MAX) {
        return false;
    }

    info = vmsvga3d_command_info(cmd);
    trace_3d = VMVGA_TRACE_LOCAL_ENABLED(VMVGA_TRACE_3D);

    if (trace_3d) {
        if (*len >= 2) {
            uint32_t raw_size = 0;

            vmsvga_fifo_peek_raw_data(s, 0, &raw_size, sizeof(raw_size));
            payload_size = le32_to_cpu(raw_size);
        }
        fprintf(
            stderr,
            "VMVGA-3D-RAW name=%s cmd=%u size=%s%u action=%s fifo=0x%08x "
            "words=%d\n",
            info != NULL ? info->name : "UNKNOWN", cmd,
            payload_size == UINT32_MAX ? "INVALID/" : "",
            payload_size == UINT32_MAX ? 0 : payload_size,
            info != NULL ? vmsvga3d_trace_command_action(cmd, info) : "STALL",
            fifo_start, *len);
    }

    if (info == NULL) {
        return false;
    }

    if (trace_3d && vmsvga3d_trace_fifo_command(cmd)) {
        fprintf(stderr,
                "VMVGA-3D-CMD path=%s name=%s id=%u action=%s fifo=0x%08x\n",
                vmsvga3d_trace_command_path(cmd), info->name, cmd,
                vmsvga3d_trace_command_action(cmd, info), fifo_start);
    }

    if (info->handler != NULL) {
        return info->handler(s, cmd, len, fifo_start);
    }

    if (vmsvga3d_is_dx_command(cmd)) {
        return vmsvga3d_fifo_dx_command(s, dx_context, cmd, len, fifo_start);
    }

    if (info->action == VMSVGA3D_COMMAND_STALL) {
        if (trace_3d) {
            fprintf(stderr,
                    "VMVGA-3D-STALL path=%s name=%s id=%u fifo=0x%08x\n",
                    vmsvga3d_trace_command_path(cmd), info->name, cmd, fifo_start);
        }
        vmsvga3d_fifo_rewind(s, len, fifo_start);
        VPRINT("%s command %u in SVGA command FIFO\n", info->name, cmd);
        return true;
    }

    if (!vmsvga3d_fifo_discard_packet(s, len, fifo_start)) {
        VPRINT("rewind command %u in SVGA command FIFO\n", cmd);
        return true;
    }

    VPRINT("%s command %u in SVGA command FIFO\n", info->name, cmd);

    return true;
}

/*
 * Legacy devcaps (0..SVGA3D_DEVCAP_DEAD2) describe the intended D3D9 profile.
 * DX/vGPU10 devcaps are exposed as one bundle only when the optional D3D11
 * renderer tier passed its realize-time feature-level probe.
 */
static const char *vmsvga3d_devcap_name(uint32_t index)
{
    static const char *const names[SVGA3D_DEVCAP_MAX] = {
          [0] = "SVGA3D_DEVCAP_3D",
          [1] = "SVGA3D_DEVCAP_MAX_LIGHTS",
          [2] = "SVGA3D_DEVCAP_MAX_TEXTURES",
          [3] = "SVGA3D_DEVCAP_MAX_CLIP_PLANES",
          [4] = "SVGA3D_DEVCAP_VERTEX_SHADER_VERSION",
          [5] = "SVGA3D_DEVCAP_VERTEX_SHADER",
          [6] = "SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION",
          [7] = "SVGA3D_DEVCAP_FRAGMENT_SHADER",
          [8] = "SVGA3D_DEVCAP_MAX_RENDER_TARGETS",
          [9] = "SVGA3D_DEVCAP_S23E8_TEXTURES",
          [10] = "SVGA3D_DEVCAP_S10E5_TEXTURES",
          [11] = "SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND",
          [12] = "SVGA3D_DEVCAP_D16_BUFFER_FORMAT",
          [13] = "SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT",
          [14] = "SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT",
          [15] = "SVGA3D_DEVCAP_QUERY_TYPES",
          [16] = "SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING",
          [17] = "SVGA3D_DEVCAP_MAX_POINT_SIZE",
          [18] = "SVGA3D_DEVCAP_MAX_SHADER_TEXTURES",
          [19] = "SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH",
          [20] = "SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT",
          [21] = "SVGA3D_DEVCAP_MAX_VOLUME_EXTENT",
          [22] = "SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT",
          [23] = "SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO",
          [24] = "SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY",
          [25] = "SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT",
          [26] = "SVGA3D_DEVCAP_MAX_VERTEX_INDEX",
          [27] = "SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS",
          [28] = "SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS",
          [29] = "SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS",
          [30] = "SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS",
          [31] = "SVGA3D_DEVCAP_TEXTURE_OPS",
          [32] = "SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8",
          [33] = "SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8",
          [34] = "SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10",
          [35] = "SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5",
          [36] = "SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5",
          [37] = "SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4",
          [38] = "SVGA3D_DEVCAP_SURFACEFMT_R5G6B5",
          [39] = "SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16",
          [40] = "SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8",
          [41] = "SVGA3D_DEVCAP_SURFACEFMT_ALPHA8",
          [42] = "SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8",
          [43] = "SVGA3D_DEVCAP_SURFACEFMT_Z_D16",
          [44] = "SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8",
          [45] = "SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8",
          [46] = "SVGA3D_DEVCAP_SURFACEFMT_DXT1",
          [47] = "SVGA3D_DEVCAP_SURFACEFMT_DXT2",
          [48] = "SVGA3D_DEVCAP_SURFACEFMT_DXT3",
          [49] = "SVGA3D_DEVCAP_SURFACEFMT_DXT4",
          [50] = "SVGA3D_DEVCAP_SURFACEFMT_DXT5",
          [51] = "SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8",
          [52] = "SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10",
          [53] = "SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8",
          [54] = "SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8",
          [55] = "SVGA3D_DEVCAP_SURFACEFMT_CxV8U8",
          [56] = "SVGA3D_DEVCAP_SURFACEFMT_R_S10E5",
          [57] = "SVGA3D_DEVCAP_SURFACEFMT_R_S23E8",
          [58] = "SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5",
          [59] = "SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8",
          [60] = "SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5",
          [61] = "SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8",
          [62] = "SVGA3D_DEVCAP_MISSING62",
          [63] = "SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES",
          [64] = "SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS",
          [65] = "SVGA3D_DEVCAP_SURFACEFMT_V16U16",
          [66] = "SVGA3D_DEVCAP_SURFACEFMT_G16R16",
          [67] = "SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16",
          [68] = "SVGA3D_DEVCAP_SURFACEFMT_UYVY",
          [69] = "SVGA3D_DEVCAP_SURFACEFMT_YUY2",
          [70] = "SVGA3D_DEVCAP_DEAD4",
          [71] = "SVGA3D_DEVCAP_DEAD5",
          [72] = "SVGA3D_DEVCAP_DEAD7",
          [73] = "SVGA3D_DEVCAP_DEAD6",
          [74] = "SVGA3D_DEVCAP_AUTOGENMIPMAPS",
          [75] = "SVGA3D_DEVCAP_SURFACEFMT_NV12",
          [76] = "SVGA3D_DEVCAP_SURFACEFMT_AYUV",
          [77] = "SVGA3D_DEVCAP_MAX_CONTEXT_IDS",
          [78] = "SVGA3D_DEVCAP_MAX_SURFACE_IDS",
          [79] = "SVGA3D_DEVCAP_SURFACEFMT_Z_DF16",
          [80] = "SVGA3D_DEVCAP_SURFACEFMT_Z_DF24",
          [81] = "SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT",
          [82] = "SVGA3D_DEVCAP_SURFACEFMT_ATI1",
          [83] = "SVGA3D_DEVCAP_SURFACEFMT_ATI2",
          [84] = "SVGA3D_DEVCAP_DEAD1",
          [85] = "SVGA3D_DEVCAP_DEAD8",
          [86] = "SVGA3D_DEVCAP_DEAD9",
          [87] = "SVGA3D_DEVCAP_LINE_AA",
          [88] = "SVGA3D_DEVCAP_LINE_STIPPLE",
          [89] = "SVGA3D_DEVCAP_MAX_LINE_WIDTH",
          [90] = "SVGA3D_DEVCAP_MAX_AA_LINE_WIDTH",
          [91] = "SVGA3D_DEVCAP_SURFACEFMT_YV12",
          [92] = "SVGA3D_DEVCAP_DEAD3",
          [93] = "SVGA3D_DEVCAP_TS_COLOR_KEY",
          [94] = "SVGA3D_DEVCAP_DEAD2",
          [95] = "SVGA3D_DEVCAP_DXCONTEXT",
          [96] = "SVGA3D_DEVCAP_MAX_TEXTURE_ARRAY_SIZE",
          [97] = "SVGA3D_DEVCAP_DX_MAX_VERTEXBUFFERS",
          [98] = "SVGA3D_DEVCAP_DX_MAX_CONSTANT_BUFFERS",
          [99] = "SVGA3D_DEVCAP_DX_PROVOKING_VERTEX",
          [100] = "SVGA3D_DEVCAP_DXFMT_X8R8G8B8",
          [101] = "SVGA3D_DEVCAP_DXFMT_A8R8G8B8",
          [102] = "SVGA3D_DEVCAP_DXFMT_R5G6B5",
          [103] = "SVGA3D_DEVCAP_DXFMT_X1R5G5B5",
          [104] = "SVGA3D_DEVCAP_DXFMT_A1R5G5B5",
          [105] = "SVGA3D_DEVCAP_DXFMT_A4R4G4B4",
          [106] = "SVGA3D_DEVCAP_DXFMT_Z_D32",
          [107] = "SVGA3D_DEVCAP_DXFMT_Z_D16",
          [108] = "SVGA3D_DEVCAP_DXFMT_Z_D24S8",
          [109] = "SVGA3D_DEVCAP_DXFMT_Z_D15S1",
          [110] = "SVGA3D_DEVCAP_DXFMT_LUMINANCE8",
          [111] = "SVGA3D_DEVCAP_DXFMT_LUMINANCE4_ALPHA4",
          [112] = "SVGA3D_DEVCAP_DXFMT_LUMINANCE16",
          [113] = "SVGA3D_DEVCAP_DXFMT_LUMINANCE8_ALPHA8",
          [114] = "SVGA3D_DEVCAP_DXFMT_DXT1",
          [115] = "SVGA3D_DEVCAP_DXFMT_DXT2",
          [116] = "SVGA3D_DEVCAP_DXFMT_DXT3",
          [117] = "SVGA3D_DEVCAP_DXFMT_DXT4",
          [118] = "SVGA3D_DEVCAP_DXFMT_DXT5",
          [119] = "SVGA3D_DEVCAP_DXFMT_BUMPU8V8",
          [120] = "SVGA3D_DEVCAP_DXFMT_BUMPL6V5U5",
          [121] = "SVGA3D_DEVCAP_DXFMT_BUMPX8L8V8U8",
          [122] = "SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD1",
          [123] = "SVGA3D_DEVCAP_DXFMT_ARGB_S10E5",
          [124] = "SVGA3D_DEVCAP_DXFMT_ARGB_S23E8",
          [125] = "SVGA3D_DEVCAP_DXFMT_A2R10G10B10",
          [126] = "SVGA3D_DEVCAP_DXFMT_V8U8",
          [127] = "SVGA3D_DEVCAP_DXFMT_Q8W8V8U8",
          [128] = "SVGA3D_DEVCAP_DXFMT_CxV8U8",
          [129] = "SVGA3D_DEVCAP_DXFMT_X8L8V8U8",
          [130] = "SVGA3D_DEVCAP_DXFMT_A2W10V10U10",
          [131] = "SVGA3D_DEVCAP_DXFMT_ALPHA8",
          [132] = "SVGA3D_DEVCAP_DXFMT_R_S10E5",
          [133] = "SVGA3D_DEVCAP_DXFMT_R_S23E8",
          [134] = "SVGA3D_DEVCAP_DXFMT_RG_S10E5",
          [135] = "SVGA3D_DEVCAP_DXFMT_RG_S23E8",
          [136] = "SVGA3D_DEVCAP_DXFMT_BUFFER",
          [137] = "SVGA3D_DEVCAP_DXFMT_Z_D24X8",
          [138] = "SVGA3D_DEVCAP_DXFMT_V16U16",
          [139] = "SVGA3D_DEVCAP_DXFMT_G16R16",
          [140] = "SVGA3D_DEVCAP_DXFMT_A16B16G16R16",
          [141] = "SVGA3D_DEVCAP_DXFMT_UYVY",
          [142] = "SVGA3D_DEVCAP_DXFMT_YUY2",
          [143] = "SVGA3D_DEVCAP_DXFMT_NV12",
          [144] = "SVGA3D_DEVCAP_FORMAT_DEAD2",
          [145] = "SVGA3D_DEVCAP_DXFMT_R32G32B32A32_TYPELESS",
          [146] = "SVGA3D_DEVCAP_DXFMT_R32G32B32A32_UINT",
          [147] = "SVGA3D_DEVCAP_DXFMT_R32G32B32A32_SINT",
          [148] = "SVGA3D_DEVCAP_DXFMT_R32G32B32_TYPELESS",
          [149] = "SVGA3D_DEVCAP_DXFMT_R32G32B32_FLOAT",
          [150] = "SVGA3D_DEVCAP_DXFMT_R32G32B32_UINT",
          [151] = "SVGA3D_DEVCAP_DXFMT_R32G32B32_SINT",
          [152] = "SVGA3D_DEVCAP_DXFMT_R16G16B16A16_TYPELESS",
          [153] = "SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UINT",
          [154] = "SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SNORM",
          [155] = "SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SINT",
          [156] = "SVGA3D_DEVCAP_DXFMT_R32G32_TYPELESS",
          [157] = "SVGA3D_DEVCAP_DXFMT_R32G32_UINT",
          [158] = "SVGA3D_DEVCAP_DXFMT_R32G32_SINT",
          [159] = "SVGA3D_DEVCAP_DXFMT_R32G8X24_TYPELESS",
          [160] = "SVGA3D_DEVCAP_DXFMT_D32_FLOAT_S8X24_UINT",
          [161] = "SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24",
          [162] = "SVGA3D_DEVCAP_DXFMT_X32_G8X24_UINT",
          [163] = "SVGA3D_DEVCAP_DXFMT_R10G10B10A2_TYPELESS",
          [164] = "SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UINT",
          [165] = "SVGA3D_DEVCAP_DXFMT_R11G11B10_FLOAT",
          [166] = "SVGA3D_DEVCAP_DXFMT_R8G8B8A8_TYPELESS",
          [167] = "SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM",
          [168] = "SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM_SRGB",
          [169] = "SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UINT",
          [170] = "SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SINT",
          [171] = "SVGA3D_DEVCAP_DXFMT_R16G16_TYPELESS",
          [172] = "SVGA3D_DEVCAP_DXFMT_R16G16_UINT",
          [173] = "SVGA3D_DEVCAP_DXFMT_R16G16_SINT",
          [174] = "SVGA3D_DEVCAP_DXFMT_R32_TYPELESS",
          [175] = "SVGA3D_DEVCAP_DXFMT_D32_FLOAT",
          [176] = "SVGA3D_DEVCAP_DXFMT_R32_UINT",
          [177] = "SVGA3D_DEVCAP_DXFMT_R32_SINT",
          [178] = "SVGA3D_DEVCAP_DXFMT_R24G8_TYPELESS",
          [179] = "SVGA3D_DEVCAP_DXFMT_D24_UNORM_S8_UINT",
          [180] = "SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8",
          [181] = "SVGA3D_DEVCAP_DXFMT_X24_G8_UINT",
          [182] = "SVGA3D_DEVCAP_DXFMT_R8G8_TYPELESS",
          [183] = "SVGA3D_DEVCAP_DXFMT_R8G8_UNORM",
          [184] = "SVGA3D_DEVCAP_DXFMT_R8G8_UINT",
          [185] = "SVGA3D_DEVCAP_DXFMT_R8G8_SINT",
          [186] = "SVGA3D_DEVCAP_DXFMT_R16_TYPELESS",
          [187] = "SVGA3D_DEVCAP_DXFMT_R16_UNORM",
          [188] = "SVGA3D_DEVCAP_DXFMT_R16_UINT",
          [189] = "SVGA3D_DEVCAP_DXFMT_R16_SNORM",
          [190] = "SVGA3D_DEVCAP_DXFMT_R16_SINT",
          [191] = "SVGA3D_DEVCAP_DXFMT_R8_TYPELESS",
          [192] = "SVGA3D_DEVCAP_DXFMT_R8_UNORM",
          [193] = "SVGA3D_DEVCAP_DXFMT_R8_UINT",
          [194] = "SVGA3D_DEVCAP_DXFMT_R8_SNORM",
          [195] = "SVGA3D_DEVCAP_DXFMT_R8_SINT",
          [196] = "SVGA3D_DEVCAP_DXFMT_P8",
          [197] = "SVGA3D_DEVCAP_DXFMT_R9G9B9E5_SHAREDEXP",
          [198] = "SVGA3D_DEVCAP_DXFMT_R8G8_B8G8_UNORM",
          [199] = "SVGA3D_DEVCAP_DXFMT_G8R8_G8B8_UNORM",
          [200] = "SVGA3D_DEVCAP_DXFMT_BC1_TYPELESS",
          [201] = "SVGA3D_DEVCAP_DXFMT_BC1_UNORM_SRGB",
          [202] = "SVGA3D_DEVCAP_DXFMT_BC2_TYPELESS",
          [203] = "SVGA3D_DEVCAP_DXFMT_BC2_UNORM_SRGB",
          [204] = "SVGA3D_DEVCAP_DXFMT_BC3_TYPELESS",
          [205] = "SVGA3D_DEVCAP_DXFMT_BC3_UNORM_SRGB",
          [206] = "SVGA3D_DEVCAP_DXFMT_BC4_TYPELESS",
          [207] = "SVGA3D_DEVCAP_DXFMT_ATI1",
          [208] = "SVGA3D_DEVCAP_DXFMT_BC4_SNORM",
          [209] = "SVGA3D_DEVCAP_DXFMT_BC5_TYPELESS",
          [210] = "SVGA3D_DEVCAP_DXFMT_ATI2",
          [211] = "SVGA3D_DEVCAP_DXFMT_BC5_SNORM",
          [212] = "SVGA3D_DEVCAP_DXFMT_R10G10B10_XR_BIAS_A2_UNORM",
          [213] = "SVGA3D_DEVCAP_DXFMT_B8G8R8A8_TYPELESS",
          [214] = "SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM_SRGB",
          [215] = "SVGA3D_DEVCAP_DXFMT_B8G8R8X8_TYPELESS",
          [216] = "SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM_SRGB",
          [217] = "SVGA3D_DEVCAP_DXFMT_Z_DF16",
          [218] = "SVGA3D_DEVCAP_DXFMT_Z_DF24",
          [219] = "SVGA3D_DEVCAP_DXFMT_Z_D24S8_INT",
          [220] = "SVGA3D_DEVCAP_DXFMT_YV12",
          [221] = "SVGA3D_DEVCAP_DXFMT_R32G32B32A32_FLOAT",
          [222] = "SVGA3D_DEVCAP_DXFMT_R16G16B16A16_FLOAT",
          [223] = "SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UNORM",
          [224] = "SVGA3D_DEVCAP_DXFMT_R32G32_FLOAT",
          [225] = "SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UNORM",
          [226] = "SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SNORM",
          [227] = "SVGA3D_DEVCAP_DXFMT_R16G16_FLOAT",
          [228] = "SVGA3D_DEVCAP_DXFMT_R16G16_UNORM",
          [229] = "SVGA3D_DEVCAP_DXFMT_R16G16_SNORM",
          [230] = "SVGA3D_DEVCAP_DXFMT_R32_FLOAT",
          [231] = "SVGA3D_DEVCAP_DXFMT_R8G8_SNORM",
          [232] = "SVGA3D_DEVCAP_DXFMT_R16_FLOAT",
          [233] = "SVGA3D_DEVCAP_DXFMT_D16_UNORM",
          [234] = "SVGA3D_DEVCAP_DXFMT_A8_UNORM",
          [235] = "SVGA3D_DEVCAP_DXFMT_BC1_UNORM",
          [236] = "SVGA3D_DEVCAP_DXFMT_BC2_UNORM",
          [237] = "SVGA3D_DEVCAP_DXFMT_BC3_UNORM",
          [238] = "SVGA3D_DEVCAP_DXFMT_B5G6R5_UNORM",
          [239] = "SVGA3D_DEVCAP_DXFMT_B5G5R5A1_UNORM",
          [240] = "SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM",
          [241] = "SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM",
          [242] = "SVGA3D_DEVCAP_DXFMT_BC4_UNORM",
          [243] = "SVGA3D_DEVCAP_DXFMT_BC5_UNORM",
          [244] = "SVGA3D_DEVCAP_SM41",
          [245] = "SVGA3D_DEVCAP_MULTISAMPLE_2X",
          [246] = "SVGA3D_DEVCAP_MULTISAMPLE_4X",
          [247] = "SVGA3D_DEVCAP_MS_FULL_QUALITY",
          [248] = "SVGA3D_DEVCAP_LOGICOPS",
          [249] = "SVGA3D_DEVCAP_LOGIC_BLENDOPS",
          [250] = "SVGA3D_DEVCAP_DEAD12",
          [251] = "SVGA3D_DEVCAP_DXFMT_BC6H_TYPELESS",
          [252] = "SVGA3D_DEVCAP_DXFMT_BC6H_UF16",
          [253] = "SVGA3D_DEVCAP_DXFMT_BC6H_SF16",
          [254] = "SVGA3D_DEVCAP_DXFMT_BC7_TYPELESS",
          [255] = "SVGA3D_DEVCAP_DXFMT_BC7_UNORM",
          [256] = "SVGA3D_DEVCAP_DXFMT_BC7_UNORM_SRGB",
          [257] = "SVGA3D_DEVCAP_DEAD13",
          [258] = "SVGA3D_DEVCAP_SM5",
          [259] = "SVGA3D_DEVCAP_MULTISAMPLE_8X",
          [260] = "SVGA3D_DEVCAP_MAX_FORCED_SAMPLE_COUNT",
          [261] = "SVGA3D_DEVCAP_GL43",
    };

    if (index >= SVGA3D_DEVCAP_MAX || names[index] == NULL) {
        return "UNKNOWN";
    }

    return names[index];
}

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
      [SVGA3D_DEVCAP_MAX_POINT_SIZE] = 0x43800000, /* 256.0f */
      [SVGA3D_DEVCAP_MAX_SHADER_TEXTURES] = 0x00000014,
      /* [SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH] = 0x00008000, */
      [SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH] = 0x00004000,
      /* [SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT] = 0x00008000, */
      [SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT] = 0x00004000,
      [SVGA3D_DEVCAP_MAX_VOLUME_EXTENT] = 0x00000800,
      [SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT] = 0x00004000,
      [SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO] = 0x00004000,
      [SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY] = 0x00000010,
      [SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT] = 0x001fffff,
      [SVGA3D_DEVCAP_MAX_VERTEX_INDEX] = 0x000fffff,
      [SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS] = 0x0000ffff,
      [SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS] = 0x0000ffff,
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
      [SVGA3D_DEVCAP_SURFACEFMT_AYUV] = 0x01044000,
      [SVGA3D_DEVCAP_MAX_CONTEXT_IDS] = 0x00000100,
      [SVGA3D_DEVCAP_MAX_SURFACE_IDS] = 0x00008000,
      [SVGA3D_DEVCAP_SURFACEFMT_Z_DF16] = 0x000040c5,
      [SVGA3D_DEVCAP_SURFACEFMT_Z_DF24] = 0x000040c5,
      [SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT] = 0x000040c5,
      [SVGA3D_DEVCAP_SURFACEFMT_ATI1] = 0x00002005,
      [SVGA3D_DEVCAP_SURFACEFMT_ATI2] = 0x00002005,
      [SVGA3D_DEVCAP_DEAD1] = 0x00000000,
      [SVGA3D_DEVCAP_DEAD8] = 0x00000000,
      [SVGA3D_DEVCAP_DEAD9] = 0x00000000,
      [SVGA3D_DEVCAP_LINE_AA] = 0x00000001,
      [SVGA3D_DEVCAP_LINE_STIPPLE] = 0x00000000,
      [SVGA3D_DEVCAP_MAX_LINE_WIDTH] = 0x3f800000, /* 1.0f */
      [SVGA3D_DEVCAP_MAX_AA_LINE_WIDTH] = 0x3f800000, /* 1.0f */
      [SVGA3D_DEVCAP_SURFACEFMT_YV12] = 0x01246000,
      [SVGA3D_DEVCAP_DEAD3] = 0x00000000,
      [SVGA3D_DEVCAP_TS_COLOR_KEY] = 0x00000000,
      [SVGA3D_DEVCAP_DEAD2] = 0x00000000,
      [SVGA3D_DEVCAP_DXCONTEXT] = 0x00000001,
      [SVGA3D_DEVCAP_MAX_TEXTURE_ARRAY_SIZE] = 0x00000800,
      [SVGA3D_DEVCAP_DX_MAX_VERTEXBUFFERS] = 0x00000020,
      [SVGA3D_DEVCAP_DX_MAX_CONSTANT_BUFFERS] = 0x0000000e,
      [SVGA3D_DEVCAP_DX_PROVOKING_VERTEX] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_X8R8G8B8] = 0x000002f7,
      [SVGA3D_DEVCAP_DXFMT_A8R8G8B8] = 0x000003f7,
      [SVGA3D_DEVCAP_DXFMT_R5G6B5] = 0x000002f7,
      [SVGA3D_DEVCAP_DXFMT_X1R5G5B5] = 0x000000f7,
      [SVGA3D_DEVCAP_DXFMT_A1R5G5B5] = 0x000000f7,
      [SVGA3D_DEVCAP_DXFMT_A4R4G4B4] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_Z_D32] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_Z_D16] = 0x0000026b,
      [SVGA3D_DEVCAP_DXFMT_Z_D24S8] = 0x0000026b,
      [SVGA3D_DEVCAP_DXFMT_Z_D15S1] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_LUMINANCE8] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_LUMINANCE4_ALPHA4] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_LUMINANCE16] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_LUMINANCE8_ALPHA8] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_DXT1] = 0x00000063,
      [SVGA3D_DEVCAP_DXFMT_DXT2] = 0x00000063,
      [SVGA3D_DEVCAP_DXFMT_DXT3] = 0x00000063,
      [SVGA3D_DEVCAP_DXFMT_DXT4] = 0x00000063,
      [SVGA3D_DEVCAP_DXFMT_DXT5] = 0x00000063,
      [SVGA3D_DEVCAP_DXFMT_BUMPU8V8] = 0x000000e3,
      [SVGA3D_DEVCAP_DXFMT_BUMPL6V5U5] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_BUMPX8L8V8U8] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD1] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_ARGB_S10E5] = 0x000003f7,
      [SVGA3D_DEVCAP_DXFMT_ARGB_S23E8] = 0x000003f7,
      [SVGA3D_DEVCAP_DXFMT_A2R10G10B10] = 0x000003f7,
      [SVGA3D_DEVCAP_DXFMT_V8U8] = 0x000000e3,
      [SVGA3D_DEVCAP_DXFMT_Q8W8V8U8] = 0x00000063,
      [SVGA3D_DEVCAP_DXFMT_CxV8U8] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_X8L8V8U8] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_A2W10V10U10] = 0x00000000,
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
      [SVGA3D_DEVCAP_DXFMT_UYVY] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_YUY2] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_NV12] = 0x00000000,
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
      [SVGA3D_DEVCAP_DXFMT_P8] = 0x00000000,
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
      [SVGA3D_DEVCAP_DXFMT_YV12] = 0x00000000,
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
      [SVGA3D_DEVCAP_MS_FULL_QUALITY] = 0x00000000,
      [SVGA3D_DEVCAP_LOGICOPS] = 0x00000000,
      [SVGA3D_DEVCAP_LOGIC_BLENDOPS] = 0x00000000,
      [SVGA3D_DEVCAP_DEAD12] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_BC6H_TYPELESS] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_BC6H_UF16] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_BC6H_SF16] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_BC7_TYPELESS] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_BC7_UNORM] = 0x00000000,
      [SVGA3D_DEVCAP_DXFMT_BC7_UNORM_SRGB] = 0x00000000,
      [SVGA3D_DEVCAP_DEAD13] = 0x00000000,
      [SVGA3D_DEVCAP_SM5] = 0x00000000,
      [SVGA3D_DEVCAP_MULTISAMPLE_8X] = 0x00000000,
      [SVGA3D_DEVCAP_MAX_FORCED_SAMPLE_COUNT] = 0x00000000,
      [SVGA3D_DEVCAP_GL43] = 0x00000000};

static SVGA3dSurfaceFormat vmsvga3d_legacy_devcap_format(uint32_t index)
{
    static const SVGA3dSurfaceFormat formats[SVGA3D_DEVCAP_DXCONTEXT] = {
          [SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8] = SVGA3D_X8R8G8B8,
          [SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8] = SVGA3D_A8R8G8B8,
          [SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10] = SVGA3D_A2R10G10B10,
          [SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5] = SVGA3D_X1R5G5B5,
          [SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5] = SVGA3D_A1R5G5B5,
          [SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4] = SVGA3D_A4R4G4B4,
          [SVGA3D_DEVCAP_SURFACEFMT_R5G6B5] = SVGA3D_R5G6B5,
          [SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16] = SVGA3D_LUMINANCE16,
          [SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8] = SVGA3D_LUMINANCE8_ALPHA8,
          [SVGA3D_DEVCAP_SURFACEFMT_ALPHA8] = SVGA3D_ALPHA8,
          [SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8] = SVGA3D_LUMINANCE8,
          [SVGA3D_DEVCAP_SURFACEFMT_Z_D16] = SVGA3D_Z_D16,
          [SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8] = SVGA3D_Z_D24S8,
          [SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8] = SVGA3D_Z_D24X8,
          [SVGA3D_DEVCAP_SURFACEFMT_DXT1] = SVGA3D_DXT1,
          [SVGA3D_DEVCAP_SURFACEFMT_DXT2] = SVGA3D_DXT2,
          [SVGA3D_DEVCAP_SURFACEFMT_DXT3] = SVGA3D_DXT3,
          [SVGA3D_DEVCAP_SURFACEFMT_DXT4] = SVGA3D_DXT4,
          [SVGA3D_DEVCAP_SURFACEFMT_DXT5] = SVGA3D_DXT5,
          [SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8] = SVGA3D_BUMPX8L8V8U8,
          [SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10] = SVGA3D_A2W10V10U10,
          [SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8] = SVGA3D_BUMPU8V8,
          [SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8] = SVGA3D_Q8W8V8U8,
          [SVGA3D_DEVCAP_SURFACEFMT_CxV8U8] = SVGA3D_CxV8U8,
          [SVGA3D_DEVCAP_SURFACEFMT_R_S10E5] = SVGA3D_R_S10E5,
          [SVGA3D_DEVCAP_SURFACEFMT_R_S23E8] = SVGA3D_R_S23E8,
          [SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5] = SVGA3D_RG_S10E5,
          [SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8] = SVGA3D_RG_S23E8,
          [SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5] = SVGA3D_ARGB_S10E5,
          [SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8] = SVGA3D_ARGB_S23E8,
          [SVGA3D_DEVCAP_SURFACEFMT_V16U16] = SVGA3D_V16U16,
          [SVGA3D_DEVCAP_SURFACEFMT_G16R16] = SVGA3D_G16R16,
          [SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16] = SVGA3D_A16B16G16R16,
          [SVGA3D_DEVCAP_SURFACEFMT_UYVY] = SVGA3D_UYVY,
          [SVGA3D_DEVCAP_SURFACEFMT_YUY2] = SVGA3D_YUY2,
          [SVGA3D_DEVCAP_SURFACEFMT_NV12] = SVGA3D_NV12,
          [SVGA3D_DEVCAP_SURFACEFMT_AYUV] = SVGA3D_AYUV,
          [SVGA3D_DEVCAP_SURFACEFMT_Z_DF16] = SVGA3D_Z_DF16,
          [SVGA3D_DEVCAP_SURFACEFMT_Z_DF24] = SVGA3D_Z_DF24,
          [SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT] = SVGA3D_Z_D24S8_INT,
          [SVGA3D_DEVCAP_SURFACEFMT_ATI1] = SVGA3D_ATI1,
          [SVGA3D_DEVCAP_SURFACEFMT_ATI2] = SVGA3D_ATI2,
          [SVGA3D_DEVCAP_SURFACEFMT_YV12] = SVGA3D_YV12,
    };

    return index < ARRAY_SIZE(formats) ? formats[index] : SVGA3D_FORMAT_INVALID;
}

static SVGA3dSurfaceFormat vmsvga3d_dx_devcap_format(uint32_t index)
{
    static const SVGA3dSurfaceFormat formats[SVGA3D_DEVCAP_MAX] = {
          [SVGA3D_DEVCAP_DXFMT_X8R8G8B8] = SVGA3D_X8R8G8B8,
          [SVGA3D_DEVCAP_DXFMT_A8R8G8B8] = SVGA3D_A8R8G8B8,
          [SVGA3D_DEVCAP_DXFMT_R5G6B5] = SVGA3D_R5G6B5,
          [SVGA3D_DEVCAP_DXFMT_X1R5G5B5] = SVGA3D_X1R5G5B5,
          [SVGA3D_DEVCAP_DXFMT_A1R5G5B5] = SVGA3D_A1R5G5B5,
          [SVGA3D_DEVCAP_DXFMT_A4R4G4B4] = SVGA3D_A4R4G4B4,
          [SVGA3D_DEVCAP_DXFMT_Z_D32] = SVGA3D_Z_D32,
          [SVGA3D_DEVCAP_DXFMT_Z_D16] = SVGA3D_Z_D16,
          [SVGA3D_DEVCAP_DXFMT_Z_D24S8] = SVGA3D_Z_D24S8,
          [SVGA3D_DEVCAP_DXFMT_Z_D15S1] = SVGA3D_Z_D15S1,
          [SVGA3D_DEVCAP_DXFMT_LUMINANCE8] = SVGA3D_LUMINANCE8,
          [SVGA3D_DEVCAP_DXFMT_LUMINANCE4_ALPHA4] = SVGA3D_LUMINANCE4_ALPHA4,
          [SVGA3D_DEVCAP_DXFMT_LUMINANCE16] = SVGA3D_LUMINANCE16,
          [SVGA3D_DEVCAP_DXFMT_LUMINANCE8_ALPHA8] = SVGA3D_LUMINANCE8_ALPHA8,
          [SVGA3D_DEVCAP_DXFMT_DXT1] = SVGA3D_DXT1,
          [SVGA3D_DEVCAP_DXFMT_DXT2] = SVGA3D_DXT2,
          [SVGA3D_DEVCAP_DXFMT_DXT3] = SVGA3D_DXT3,
          [SVGA3D_DEVCAP_DXFMT_DXT4] = SVGA3D_DXT4,
          [SVGA3D_DEVCAP_DXFMT_DXT5] = SVGA3D_DXT5,
          [SVGA3D_DEVCAP_DXFMT_BUMPU8V8] = SVGA3D_BUMPU8V8,
          [SVGA3D_DEVCAP_DXFMT_BUMPL6V5U5] = SVGA3D_BUMPL6V5U5,
          [SVGA3D_DEVCAP_DXFMT_BUMPX8L8V8U8] = SVGA3D_BUMPX8L8V8U8,
          [SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD1] = SVGA3D_FORMAT_DEAD1,
          [SVGA3D_DEVCAP_DXFMT_ARGB_S10E5] = SVGA3D_ARGB_S10E5,
          [SVGA3D_DEVCAP_DXFMT_ARGB_S23E8] = SVGA3D_ARGB_S23E8,
          [SVGA3D_DEVCAP_DXFMT_A2R10G10B10] = SVGA3D_A2R10G10B10,
          [SVGA3D_DEVCAP_DXFMT_V8U8] = SVGA3D_V8U8,
          [SVGA3D_DEVCAP_DXFMT_Q8W8V8U8] = SVGA3D_Q8W8V8U8,
          [SVGA3D_DEVCAP_DXFMT_CxV8U8] = SVGA3D_CxV8U8,
          [SVGA3D_DEVCAP_DXFMT_X8L8V8U8] = SVGA3D_X8L8V8U8,
          [SVGA3D_DEVCAP_DXFMT_A2W10V10U10] = SVGA3D_A2W10V10U10,
          [SVGA3D_DEVCAP_DXFMT_ALPHA8] = SVGA3D_ALPHA8,
          [SVGA3D_DEVCAP_DXFMT_R_S10E5] = SVGA3D_R_S10E5,
          [SVGA3D_DEVCAP_DXFMT_R_S23E8] = SVGA3D_R_S23E8,
          [SVGA3D_DEVCAP_DXFMT_RG_S10E5] = SVGA3D_RG_S10E5,
          [SVGA3D_DEVCAP_DXFMT_RG_S23E8] = SVGA3D_RG_S23E8,
          [SVGA3D_DEVCAP_DXFMT_BUFFER] = SVGA3D_BUFFER,
          [SVGA3D_DEVCAP_DXFMT_Z_D24X8] = SVGA3D_Z_D24X8,
          [SVGA3D_DEVCAP_DXFMT_V16U16] = SVGA3D_V16U16,
          [SVGA3D_DEVCAP_DXFMT_G16R16] = SVGA3D_G16R16,
          [SVGA3D_DEVCAP_DXFMT_A16B16G16R16] = SVGA3D_A16B16G16R16,
          [SVGA3D_DEVCAP_DXFMT_UYVY] = SVGA3D_UYVY,
          [SVGA3D_DEVCAP_DXFMT_YUY2] = SVGA3D_YUY2,
          [SVGA3D_DEVCAP_DXFMT_NV12] = SVGA3D_NV12,
          [SVGA3D_DEVCAP_FORMAT_DEAD2] = SVGA3D_FORMAT_DEAD2,
          [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_TYPELESS] = SVGA3D_R32G32B32A32_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_UINT] = SVGA3D_R32G32B32A32_UINT,
          [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_SINT] = SVGA3D_R32G32B32A32_SINT,
          [SVGA3D_DEVCAP_DXFMT_R32G32B32_TYPELESS] = SVGA3D_R32G32B32_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R32G32B32_FLOAT] = SVGA3D_R32G32B32_FLOAT,
          [SVGA3D_DEVCAP_DXFMT_R32G32B32_UINT] = SVGA3D_R32G32B32_UINT,
          [SVGA3D_DEVCAP_DXFMT_R32G32B32_SINT] = SVGA3D_R32G32B32_SINT,
          [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_TYPELESS] = SVGA3D_R16G16B16A16_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UINT] = SVGA3D_R16G16B16A16_UINT,
          [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SNORM] = SVGA3D_R16G16B16A16_SNORM,
          [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SINT] = SVGA3D_R16G16B16A16_SINT,
          [SVGA3D_DEVCAP_DXFMT_R32G32_TYPELESS] = SVGA3D_R32G32_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R32G32_UINT] = SVGA3D_R32G32_UINT,
          [SVGA3D_DEVCAP_DXFMT_R32G32_SINT] = SVGA3D_R32G32_SINT,
          [SVGA3D_DEVCAP_DXFMT_R32G8X24_TYPELESS] = SVGA3D_R32G8X24_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_D32_FLOAT_S8X24_UINT] = SVGA3D_D32_FLOAT_S8X24_UINT,
          [SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24] = SVGA3D_R32_FLOAT_X8X24,
          [SVGA3D_DEVCAP_DXFMT_X32_G8X24_UINT] = SVGA3D_X32_G8X24_UINT,
          [SVGA3D_DEVCAP_DXFMT_R10G10B10A2_TYPELESS] = SVGA3D_R10G10B10A2_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UINT] = SVGA3D_R10G10B10A2_UINT,
          [SVGA3D_DEVCAP_DXFMT_R11G11B10_FLOAT] = SVGA3D_R11G11B10_FLOAT,
          [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_TYPELESS] = SVGA3D_R8G8B8A8_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM] = SVGA3D_R8G8B8A8_UNORM,
          [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM_SRGB] = SVGA3D_R8G8B8A8_UNORM_SRGB,
          [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UINT] = SVGA3D_R8G8B8A8_UINT,
          [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SINT] = SVGA3D_R8G8B8A8_SINT,
          [SVGA3D_DEVCAP_DXFMT_R16G16_TYPELESS] = SVGA3D_R16G16_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R16G16_UINT] = SVGA3D_R16G16_UINT,
          [SVGA3D_DEVCAP_DXFMT_R16G16_SINT] = SVGA3D_R16G16_SINT,
          [SVGA3D_DEVCAP_DXFMT_R32_TYPELESS] = SVGA3D_R32_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_D32_FLOAT] = SVGA3D_D32_FLOAT,
          [SVGA3D_DEVCAP_DXFMT_R32_UINT] = SVGA3D_R32_UINT,
          [SVGA3D_DEVCAP_DXFMT_R32_SINT] = SVGA3D_R32_SINT,
          [SVGA3D_DEVCAP_DXFMT_R24G8_TYPELESS] = SVGA3D_R24G8_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_D24_UNORM_S8_UINT] = SVGA3D_D24_UNORM_S8_UINT,
          [SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8] = SVGA3D_R24_UNORM_X8,
          [SVGA3D_DEVCAP_DXFMT_X24_G8_UINT] = SVGA3D_X24_G8_UINT,
          [SVGA3D_DEVCAP_DXFMT_R8G8_TYPELESS] = SVGA3D_R8G8_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R8G8_UNORM] = SVGA3D_R8G8_UNORM,
          [SVGA3D_DEVCAP_DXFMT_R8G8_UINT] = SVGA3D_R8G8_UINT,
          [SVGA3D_DEVCAP_DXFMT_R8G8_SINT] = SVGA3D_R8G8_SINT,
          [SVGA3D_DEVCAP_DXFMT_R16_TYPELESS] = SVGA3D_R16_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R16_UNORM] = SVGA3D_R16_UNORM,
          [SVGA3D_DEVCAP_DXFMT_R16_UINT] = SVGA3D_R16_UINT,
          [SVGA3D_DEVCAP_DXFMT_R16_SNORM] = SVGA3D_R16_SNORM,
          [SVGA3D_DEVCAP_DXFMT_R16_SINT] = SVGA3D_R16_SINT,
          [SVGA3D_DEVCAP_DXFMT_R8_TYPELESS] = SVGA3D_R8_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_R8_UNORM] = SVGA3D_R8_UNORM,
          [SVGA3D_DEVCAP_DXFMT_R8_UINT] = SVGA3D_R8_UINT,
          [SVGA3D_DEVCAP_DXFMT_R8_SNORM] = SVGA3D_R8_SNORM,
          [SVGA3D_DEVCAP_DXFMT_R8_SINT] = SVGA3D_R8_SINT,
          [SVGA3D_DEVCAP_DXFMT_P8] = SVGA3D_P8,
          [SVGA3D_DEVCAP_DXFMT_R9G9B9E5_SHAREDEXP] = SVGA3D_R9G9B9E5_SHAREDEXP,
          [SVGA3D_DEVCAP_DXFMT_R8G8_B8G8_UNORM] = SVGA3D_R8G8_B8G8_UNORM,
          [SVGA3D_DEVCAP_DXFMT_G8R8_G8B8_UNORM] = SVGA3D_G8R8_G8B8_UNORM,
          [SVGA3D_DEVCAP_DXFMT_BC1_TYPELESS] = SVGA3D_BC1_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_BC1_UNORM_SRGB] = SVGA3D_BC1_UNORM_SRGB,
          [SVGA3D_DEVCAP_DXFMT_BC2_TYPELESS] = SVGA3D_BC2_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_BC2_UNORM_SRGB] = SVGA3D_BC2_UNORM_SRGB,
          [SVGA3D_DEVCAP_DXFMT_BC3_TYPELESS] = SVGA3D_BC3_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_BC3_UNORM_SRGB] = SVGA3D_BC3_UNORM_SRGB,
          [SVGA3D_DEVCAP_DXFMT_BC4_TYPELESS] = SVGA3D_BC4_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_ATI1] = SVGA3D_ATI1,
          [SVGA3D_DEVCAP_DXFMT_BC4_SNORM] = SVGA3D_BC4_SNORM,
          [SVGA3D_DEVCAP_DXFMT_BC5_TYPELESS] = SVGA3D_BC5_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_ATI2] = SVGA3D_ATI2,
          [SVGA3D_DEVCAP_DXFMT_BC5_SNORM] = SVGA3D_BC5_SNORM,
          [SVGA3D_DEVCAP_DXFMT_R10G10B10_XR_BIAS_A2_UNORM] = SVGA3D_R10G10B10_XR_BIAS_A2_UNORM,
          [SVGA3D_DEVCAP_DXFMT_B8G8R8A8_TYPELESS] = SVGA3D_B8G8R8A8_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM_SRGB] = SVGA3D_B8G8R8A8_UNORM_SRGB,
          [SVGA3D_DEVCAP_DXFMT_B8G8R8X8_TYPELESS] = SVGA3D_B8G8R8X8_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM_SRGB] = SVGA3D_B8G8R8X8_UNORM_SRGB,
          [SVGA3D_DEVCAP_DXFMT_Z_DF16] = SVGA3D_Z_DF16,
          [SVGA3D_DEVCAP_DXFMT_Z_DF24] = SVGA3D_Z_DF24,
          [SVGA3D_DEVCAP_DXFMT_Z_D24S8_INT] = SVGA3D_Z_D24S8_INT,
          [SVGA3D_DEVCAP_DXFMT_YV12] = SVGA3D_YV12,
          [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_FLOAT] = SVGA3D_R32G32B32A32_FLOAT,
          [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_FLOAT] = SVGA3D_R16G16B16A16_FLOAT,
          [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UNORM] = SVGA3D_R16G16B16A16_UNORM,
          [SVGA3D_DEVCAP_DXFMT_R32G32_FLOAT] = SVGA3D_R32G32_FLOAT,
          [SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UNORM] = SVGA3D_R10G10B10A2_UNORM,
          [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SNORM] = SVGA3D_R8G8B8A8_SNORM,
          [SVGA3D_DEVCAP_DXFMT_R16G16_FLOAT] = SVGA3D_R16G16_FLOAT,
          [SVGA3D_DEVCAP_DXFMT_R16G16_UNORM] = SVGA3D_R16G16_UNORM,
          [SVGA3D_DEVCAP_DXFMT_R16G16_SNORM] = SVGA3D_R16G16_SNORM,
          [SVGA3D_DEVCAP_DXFMT_R32_FLOAT] = SVGA3D_R32_FLOAT,
          [SVGA3D_DEVCAP_DXFMT_R8G8_SNORM] = SVGA3D_R8G8_SNORM,
          [SVGA3D_DEVCAP_DXFMT_R16_FLOAT] = SVGA3D_R16_FLOAT,
          [SVGA3D_DEVCAP_DXFMT_D16_UNORM] = SVGA3D_D16_UNORM,
          [SVGA3D_DEVCAP_DXFMT_A8_UNORM] = SVGA3D_A8_UNORM,
          [SVGA3D_DEVCAP_DXFMT_BC1_UNORM] = SVGA3D_BC1_UNORM,
          [SVGA3D_DEVCAP_DXFMT_BC2_UNORM] = SVGA3D_BC2_UNORM,
          [SVGA3D_DEVCAP_DXFMT_BC3_UNORM] = SVGA3D_BC3_UNORM,
          [SVGA3D_DEVCAP_DXFMT_B5G6R5_UNORM] = SVGA3D_B5G6R5_UNORM,
          [SVGA3D_DEVCAP_DXFMT_B5G5R5A1_UNORM] = SVGA3D_B5G5R5A1_UNORM,
          [SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM] = SVGA3D_B8G8R8A8_UNORM,
          [SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM] = SVGA3D_B8G8R8X8_UNORM,
          [SVGA3D_DEVCAP_DXFMT_BC4_UNORM] = SVGA3D_BC4_UNORM,
          [SVGA3D_DEVCAP_DXFMT_BC5_UNORM] = SVGA3D_BC5_UNORM,
          [SVGA3D_DEVCAP_DXFMT_BC6H_TYPELESS] = SVGA3D_BC6H_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_BC6H_UF16] = SVGA3D_BC6H_UF16,
          [SVGA3D_DEVCAP_DXFMT_BC6H_SF16] = SVGA3D_BC6H_SF16,
          [SVGA3D_DEVCAP_DXFMT_BC7_TYPELESS] = SVGA3D_BC7_TYPELESS,
          [SVGA3D_DEVCAP_DXFMT_BC7_UNORM] = SVGA3D_BC7_UNORM,
          [SVGA3D_DEVCAP_DXFMT_BC7_UNORM_SRGB] = SVGA3D_BC7_UNORM_SRGB,
    };

    return index < ARRAY_SIZE(formats) ? formats[index] : SVGA3D_FORMAT_INVALID;
}

static bool vmsvga3d_vbox_dx11_devcap_format_unknown(
    SVGA3dSurfaceFormat format)
{
    /* Match VirtualBox's DX11 devcap format conversion exactly. */
    switch (format) {
    case SVGA3D_A4R4G4B4:
    case SVGA3D_Z_D32:
    case SVGA3D_Z_D15S1:
    case SVGA3D_LUMINANCE8:
    case SVGA3D_LUMINANCE4_ALPHA4:
    case SVGA3D_LUMINANCE16:
    case SVGA3D_LUMINANCE8_ALPHA8:
    case SVGA3D_DXT1:
    case SVGA3D_DXT2:
    case SVGA3D_DXT3:
    case SVGA3D_DXT4:
    case SVGA3D_DXT5:
    case SVGA3D_BUMPU8V8:
    case SVGA3D_BUMPL6V5U5:
    case SVGA3D_BUMPX8L8V8U8:
    case SVGA3D_FORMAT_DEAD1:
    case SVGA3D_ARGB_S10E5:
    case SVGA3D_ARGB_S23E8:
    case SVGA3D_A2R10G10B10:
    case SVGA3D_V8U8:
    case SVGA3D_Q8W8V8U8:
    case SVGA3D_CxV8U8:
    case SVGA3D_X8L8V8U8:
    case SVGA3D_A2W10V10U10:
    case SVGA3D_ALPHA8:
    case SVGA3D_R_S10E5:
    case SVGA3D_R_S23E8:
    case SVGA3D_RG_S10E5:
    case SVGA3D_RG_S23E8:
    case SVGA3D_BUFFER:
    case SVGA3D_Z_D24X8:
    case SVGA3D_V16U16:
    case SVGA3D_G16R16:
    case SVGA3D_A16B16G16R16:
    case SVGA3D_UYVY:
    case SVGA3D_YUY2:
    case SVGA3D_NV12:
    case SVGA3D_FORMAT_DEAD2:
    case SVGA3D_P8:
    case SVGA3D_ATI1:
    case SVGA3D_ATI2:
    case SVGA3D_Z_DF16:
    case SVGA3D_Z_DF24:
    case SVGA3D_YV12:
        return true;
    default:
        return false;
    }
}

static uint32_t vmsvga3d_get_devcap(struct vmsvga_state_s *s,
                                      uint32_t index)
{
    SVGA3dSurfaceFormat format;
    VMSVGA3DD3D10Format dx_format;
    uint32_t value;

    if (index >= SVGA3D_DEVCAP_MAX) {
        return 0;
    }

    if (index >= SVGA3D_DEVCAP_DXCONTEXT &&
        (s == NULL || !s->svga3d_dx_capable)) {
        return 0;
    }

    if (index == SVGA3D_DEVCAP_DXFMT_BUFFER) {
        return 1;
    }

    value = vmsvga3d_devcap[index];

    if (index == SVGA3D_DEVCAP_SM5) {
        return s != NULL && s->vgpu_generation == VMSVGA_VGPU_11 ? 1 : 0;
    }

    if (index == SVGA3D_DEVCAP_MULTISAMPLE_2X ||
        index == SVGA3D_DEVCAP_MULTISAMPLE_4X ||
        (index == SVGA3D_DEVCAP_MULTISAMPLE_8X &&
         s != NULL && s->vgpu_generation == VMSVGA_VGPU_11)) {
        uint32_t sample_count = index == SVGA3D_DEVCAP_MULTISAMPLE_2X ? 2u :
                                index == SVGA3D_DEVCAP_MULTISAMPLE_4X ? 4u : 8u;
        if (s == NULL || s->dxvk == NULL ||
            !vmsvga3d_dxvk_d3d11_supports_multisample(s->dxvk, sample_count)) {
            return 0;
        }
        if (index == SVGA3D_DEVCAP_MULTISAMPLE_8X) {
            value = 1;
        }
    }

    format = vmsvga3d_legacy_devcap_format(index);

    if (format != SVGA3D_FORMAT_INVALID && s != NULL && s->dxvk != NULL) {
        value = vmsvga3d_dxvk_d3d9_qualify_format_caps(
            s->dxvk, vmsvga3d_d3d9_surface_format(format), value);
    }

    format = vmsvga3d_dx_devcap_format(index);

    if (format != SVGA3D_FORMAT_INVALID && s != NULL && s->dxvk != NULL &&
        s->vgpu_generation == VMSVGA_VGPU_11) {
        if (vmsvga3d_vbox_dx11_devcap_format_unknown(format)) {
            value = 0;
        } else {
            dx_format = vmsvga3d_d3d10_surface_format(format);
            value = vmsvga3d_dxvk_d3d11_format_caps(
                s->dxvk, dx_format.dxgi_format);
        }
    } else if (format != SVGA3D_FORMAT_INVALID && s != NULL &&
               s->dxvk != NULL) {
        dx_format = vmsvga3d_d3d10_surface_format(format);
        value = vmsvga3d_dxvk_d3d11_qualify_format_caps(
            s->dxvk, dx_format.dxgi_format, false, value);
    } else if (index == SVGA3D_DEVCAP_DXFMT_BUFFER &&
               s != NULL && s->dxvk != NULL) {
        value = vmsvga3d_dxvk_d3d11_qualify_format_caps(
            s->dxvk, 0, true, value);
    }

    return value;
}

static void vmsvga3d_publish_fifo_caps(struct vmsvga_state_s *s)
{
    uint32_t *caps;
    uint32_t length;
    uint32_t i;

    if (!vmsvga_fifo_has_reg(s, SVGA_FIFO_3D_CAPS_LAST)) {
        return;
    }

    caps = &s->fifo[SVGA_FIFO_3D_CAPS];
    memset(caps, 0, SVGA_FIFO_3D_CAPS_SIZE * sizeof(*caps));
    if (!s->svga3d_capable) {
        return;
    }

    length = (sizeof(SVGA3dCapsRecordHeader) +
              SVGA3D_DEVCAP_DEAD1 * sizeof(SVGA3dCapPair)) /
             sizeof(uint32_t);
    caps[0] = cpu_to_le32(length);
    caps[1] = cpu_to_le32(SVGA3DCAPS_RECORD_DEVCAPS);

    for (i = 0; i < SVGA3D_DEVCAP_DEAD1; i++) {
        caps[2 + i * 2] = cpu_to_le32(i);
        caps[3 + i * 2] = cpu_to_le32(vmsvga3d_get_devcap(s, i));
    }
}
