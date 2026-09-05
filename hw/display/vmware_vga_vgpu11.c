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

#include "include/vmware_vga_d3d11.h"
#include "include/vmware_vga_dxvk.h"

#include <string.h>

enum {
    D3D11_BIND_VERTEX_BUFFER = 0x01,
    D3D11_BIND_INDEX_BUFFER = 0x02,
    D3D11_BIND_CONSTANT_BUFFER = 0x04,
    D3D11_BIND_SHADER_RESOURCE = 0x08,
    D3D11_BIND_STREAM_OUTPUT = 0x10,
    D3D11_BIND_RENDER_TARGET = 0x20,
    D3D11_BIND_DEPTH_STENCIL = 0x40,
    D3D11_BIND_UNORDERED_ACCESS = 0x80,
    D3D11_BIND_DECODER = 0x200,

    D3D11_RESOURCE_MISC_GENERATE_MIPS = 0x01,
    D3D11_RESOURCE_MISC_TEXTURECUBE = 0x04,
    D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS = 0x10,
    D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS = 0x20,
    D3D11_RESOURCE_MISC_BUFFER_STRUCTURED = 0x40,
    D3D11_RESOURCE_MISC_RESOURCE_CLAMP = 0x80,

    D3D11_SRV_BUFFEREX = 11,

    D3D11_UAV_UNKNOWN = 0,
    D3D11_UAV_BUFFER = 1,
    D3D11_UAV_TEXTURE1D = 2,
    D3D11_UAV_TEXTURE1DARRAY = 3,
    D3D11_UAV_TEXTURE2D = 4,
    D3D11_UAV_TEXTURE2DARRAY = 5,
    D3D11_UAV_TEXTURE3D = 8,
};

static VMSVGA3DD3D11Level d3d11_level(VMSVGA3DD3D10Level level)
{
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    if (level == VMSVGA3D_D3D10_LEVEL_11_1) {
        return VMSVGA3D_D3D11_LEVEL_11_1;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_shader_define_entry(
    const SVGA3dCmdDXDefineShader *src, SVGACOTableDXShaderEntry *dst)
{
    if (!src || !dst || src->sizeInBytes < 8 ||
        src->type < SVGA3D_SHADERTYPE_MIN || src->type >= SVGA3D_SHADERTYPE_MAX) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    dst->type = src->type;
    dst->sizeInBytes = src->sizeInBytes;
    dst->offsetInBytes = 0;
    dst->mobid = SVGA3D_INVALID_ID;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_stream_output_mob_entry(
    const SVGA3dCmdDXDefineStreamOutputWithMob *src,
    SVGACOTableDXStreamOutputEntry *dst)
{
    if (!src || !dst ||
        src->numOutputStreamEntries > SVGA3D_MAX_STREAMOUT_DECLS ||
        src->numOutputStreamStrides > SVGA3D_DX_MAX_SOTARGETS ||
        (src->rasterizedStream >= SVGA3D_DX_MAX_SOTARGETS &&
         src->rasterizedStream != SVGA3D_DX_SO_NO_RASTERIZED_STREAM)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));
    dst->numOutputStreamEntries = src->numOutputStreamEntries;
    memcpy(dst->streamOutputStrideInBytes, src->streamOutputStrideInBytes,
           sizeof(dst->streamOutputStrideInBytes));

    dst->rasterizedStream = src->rasterizedStream;
    dst->numOutputStreamStrides = src->numOutputStreamStrides;
    dst->mobid = SVGA3D_INVALID_ID;
    dst->usesMob = 1;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_stream_output_bind(
    SVGACOTableDXStreamOutputEntry *entry, uint32_t mobid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes)
{
    uint64_t required_size;

    if (!entry) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    required_size = (uint64_t)entry->numOutputStreamEntries *
                    sizeof(SVGA3dStreamOutputDeclarationEntry);
    if (size_in_bytes < required_size) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    entry->mobid = mobid;
    entry->offsetInBytes = offset_in_bytes;
    entry->usesMob = 1;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_rasterizer_define_entry(
    const SVGA3dCmdDXDefineRasterizerState_v2 *src,
    SVGACOTableDXRasterizerStateEntry *entry)
{
    if (!src || !entry) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    entry->fillMode = src->fillMode;
    entry->cullMode = src->cullMode;
    entry->frontCounterClockwise = src->frontCounterClockwise;
    entry->provokingVertexLast = src->provokingVertexLast;
    entry->depthBias = src->depthBias;
    entry->depthBiasClamp = src->depthBiasClamp;
    entry->slopeScaledDepthBias = src->slopeScaledDepthBias;
    entry->depthClipEnable = src->depthClipEnable;
    entry->scissorEnable = src->scissorEnable;
    entry->multisampleEnable = src->multisampleEnable;
    entry->antialiasedLineEnable = src->antialiasedLineEnable;
    entry->lineWidth = src->lineWidth;
    entry->lineStippleEnable = src->lineStippleEnable;
    entry->lineStippleFactor = src->lineStippleFactor;
    entry->lineStipplePattern = src->lineStipplePattern;
    entry->forcedSampleCount = (uint8_t)src->forcedSampleCount;

    memset(entry->mustBeZero, 0, sizeof(entry->mustBeZero));

    return VMSVGA3D_D3D11_LEVEL_11_1;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_define_entry(
    const SVGA3dCmdDXDefineUAView *src, SVGACOTableDXUAViewEntry *dst)
{
    if (!src || !dst) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    dst->sid = src->sid;
    dst->format = src->format;
    dst->resourceDimension = src->resourceDimension;
    dst->desc = src->desc;
    dst->structureCount = 0;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_destroy_entry(
    SVGACOTableDXUAViewEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_set_structure_count(
    SVGACOTableDXUAViewEntry *entry, uint32_t structure_count)
{
    if (!entry) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    entry->structureCount = structure_count;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_define_plan(
    const SVGA3dCmdDXDefineUAView *src, uint32_t cotable_count,
    VMSVGA3DD3D11UAVDefinePlan *plan)
{
    VMSVGA3DD3D11Level level;

    if (!src || !plan || src->uaViewId >= cotable_count) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));
    plan->view_id = src->uaViewId;
    level = vmsvga3d_d3d11_uav_define_entry(src, &plan->entry);
    if (level == VMSVGA3D_D3D11_LEVEL_INVALID) {
        return level;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_destroy_plan(
    const SVGA3dCmdDXDestroyUAView *src, uint32_t cotable_count,
    VMSVGA3DD3D11UAVDestroyPlan *plan)
{
    if (!src || !plan || src->uaViewId >= cotable_count) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    plan->view_id = src->uaViewId;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_set_plan(
    const SVGA3dCmdDXSetUAViews *src, uint32_t count,
    const SVGA3dUAViewId *ids, uint32_t cotable_count,
    VMSVGA3DD3D11UAVSetPlan *plan)
{
    uint32_t i;

    if (!src || !plan ||
        src->uavSpliceIndex > SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS ||
        count > SVGA3D_DX11_1_MAX_UAVIEWS ||
        (count != 0 && !ids)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    for (i = 0; i < count; i++) {
        if (ids[i] >= cotable_count && ids[i] != SVGA3D_INVALID_ID) {
            return VMSVGA3D_D3D11_LEVEL_INVALID;
        }
    }

    memset(plan, 0, sizeof(*plan));
    plan->uav_splice_index = src->uavSpliceIndex;
    plan->count = count;
    if (count != 0) {
        memcpy(plan->ids, ids, count * sizeof(plan->ids[0]));
    }
    plan->shadow_update_atomic = true;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_cs_uav_set_plan(
    const SVGA3dCmdDXSetCSUAViews *src, uint32_t count,
    const SVGA3dUAViewId *ids, uint32_t cotable_count,
    VMSVGA3DD3D11CSUAVSetPlan *plan)
{
    uint32_t i;

    if (!src || !plan ||
        src->startIndex >= SVGA3D_DX11_1_MAX_UAVIEWS ||
        count > SVGA3D_DX11_1_MAX_UAVIEWS - src->startIndex ||
        (count != 0 && !ids)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    for (i = 0; i < count; i++) {
        if (ids[i] >= cotable_count && ids[i] != SVGA3D_INVALID_ID) {
            return VMSVGA3D_D3D11_LEVEL_INVALID;
        }
    }

    memset(plan, 0, sizeof(*plan));
    plan->start_index = src->startIndex;
    plan->count = count;
    if (count != 0) {
        memcpy(plan->ids, ids, count * sizeof(plan->ids[0]));
    }
    plan->shadow_update_atomic = true;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_clear_uint_plan(
    const SVGA3dCmdDXClearUAViewUint *src, uint32_t cotable_count,
    VMSVGA3DD3D11UAVClearUintPlan *plan)
{
    if (!src || !plan || src->uaViewId >= cotable_count) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    plan->view_id = src->uaViewId;
    memcpy(plan->values, src->value.value, sizeof(plan->values));

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_clear_float_plan(
    const SVGA3dCmdDXClearUAViewFloat *src, uint32_t cotable_count,
    VMSVGA3DD3D11UAVClearFloatPlan *plan)
{
    if (!src || !plan || src->uaViewId >= cotable_count) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    plan->view_id = src->uaViewId;
    memcpy(plan->values, src->value.value, sizeof(plan->values));

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_copy_structure_count_plan(
    const SVGA3dCmdDXCopyStructureCount *src, uint32_t cotable_count,
    VMSVGA3DD3D11CopyStructureCountPlan *plan)
{
    if (!src || !plan || src->srcUAViewId >= cotable_count) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    plan->source_view_id = src->srcUAViewId;
    plan->destination_sid = src->destSid;
    plan->destination_byte_offset = src->destByteOffset;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_set_structure_count_plan(
    const SVGA3dCmdDXSetStructureCount *src, uint32_t cotable_count,
    VMSVGA3DD3D11SetStructureCountPlan *plan)
{
    if (!src || !plan || src->uaViewId >= cotable_count) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    plan->view_id = src->uaViewId;
    plan->structure_count = src->structureCount;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_draw_indexed_instanced_indirect_plan(
    const SVGA3dCmdDXDrawIndexedInstancedIndirect *src,
    VMSVGA3DD3D11DrawIndexedInstancedIndirectPlan *plan)
{
    if (!src || !plan) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    plan->args_buffer_sid = src->argsBufferSid;
    plan->aligned_byte_offset = src->byteOffsetForArgs;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_draw_instanced_indirect_plan(
    const SVGA3dCmdDXDrawInstancedIndirect *src,
    VMSVGA3DD3D11DrawInstancedIndirectPlan *plan)
{
    if (!src || !plan) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    plan->args_buffer_sid = src->argsBufferSid;
    plan->aligned_byte_offset = src->byteOffsetForArgs;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_dispatch_plan(
    const SVGA3dCmdDXDispatch *src, VMSVGA3DD3D11DispatchPlan *plan)
{
    if (!src || !plan) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    plan->thread_group_count_x = src->threadGroupCountX;
    plan->thread_group_count_y = src->threadGroupCountY;
    plan->thread_group_count_z = src->threadGroupCountZ;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_plan(
    uint32_t slot, SVGA3dShaderType type, SVGA3dSurfaceId sid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes, bool surface_available,
    uint32_t surface_bytes, bool has_surface_data,
    VMSVGA3DD3D11ConstantBufferPlan *plan)
{
    VMSVGA3DD3D11Level level;
    uint32_t stage_index;
    uint32_t aligned_size;

    if (plan == NULL || slot >= SVGA3D_DX_MAX_CONSTBUFFERS) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d11_shader_stage(type, &stage_index);
    if (level == VMSVGA3D_D3D11_LEVEL_INVALID) {
        return level;
    }

    memset(plan, 0, sizeof(*plan));
    plan->shader_type = type;
    plan->stage_index = stage_index;
    plan->slot = slot;
    plan->sid = sid;
    plan->offset_in_bytes = offset_in_bytes;
    plan->size_in_bytes = size_in_bytes;
    plan->shadow_update = true;

    if (sid == SVGA3D_INVALID_ID) {
        plan->unbind = true;
        return level;
    }

    /* Match VirtualBox's SetSingleConstantBuffer snapshot range: align the
     * backend allocation to 16 constants (256 bytes), clamp to 4096
     * constants, then clip the guest copy to that allocation.  Keep the same
     * uint32_t alignment semantics as the existing D3D10 path.
     */
    aligned_size = (size_in_bytes + 255u) & ~255u;
    if (aligned_size > 4096u * 16u) {
        aligned_size = 4096u * 16u;
    }

    plan->backend_buffer_size = aligned_size;
    plan->backend_copy_size = size_in_bytes < aligned_size ?
                                  size_in_bytes : aligned_size;

    if (!surface_available || !has_surface_data ||
        offset_in_bytes >= surface_bytes ||
        plan->backend_copy_size > surface_bytes - offset_in_bytes) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    plan->create_buffer = aligned_size != 0;
    plan->has_initial_data = true;
    plan->initial_data_offset = offset_in_bytes;

    return level;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_offset_plan(
    const SVGA3dCmdDXSetConstantBufferOffset *src, SVGA3dShaderType type,
    VMSVGA3DD3D11ConstantBufferOffsetPlan *plan)
{
    VMSVGA3DD3D11Level level;
    uint32_t stage_index;

    if (!src || !plan || src->slot >= SVGA3D_DX_MAX_CONSTBUFFERS) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d11_shader_stage(type, &stage_index);
    if (level == VMSVGA3D_D3D11_LEVEL_INVALID) {
        return level;
    }

    plan->shader_type = type;
    plan->stage_index = stage_index;
    plan->slot = src->slot;
    plan->offset_in_bytes = src->offsetInBytes;

    return level;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_offset_snapshot_plan(
    const VMSVGA3DD3D11ConstantBufferOffsetPlan *offset_plan,
    const SVGA3dConstantBufferBinding *binding, bool surface_available,
    uint32_t surface_bytes, bool has_surface_data,
    VMSVGA3DD3D11ConstantBufferPlan *plan)
{
    VMSVGA3DD3D11Level level;
    uint32_t stage_index;

    if (offset_plan == NULL || binding == NULL || plan == NULL) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d11_shader_stage(
        offset_plan->shader_type, &stage_index);
    if (level == VMSVGA3D_D3D11_LEVEL_INVALID ||
        stage_index != offset_plan->stage_index) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return vmsvga3d_d3d11_constant_buffer_plan(
        offset_plan->slot, offset_plan->shader_type, binding->sid,
        offset_plan->offset_in_bytes, binding->sizeInBytes,
        surface_available, surface_bytes, has_surface_data, plan);
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid,
    const VMSVGA3DD3D11ConstantBufferPlan *plan,
    const uint8_t *surface_data, uint32_t surface_bytes)
{
    VMSVGA3DD3D11Level level;
    uint32_t stage_index;

    if (dxvk == NULL || plan == NULL || !plan->shadow_update ||
        plan->slot >= SVGA3D_DX_MAX_CONSTBUFFERS) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d11_shader_stage(plan->shader_type, &stage_index);
    if (level == VMSVGA3D_D3D11_LEVEL_INVALID ||
        stage_index != plan->stage_index) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    if (plan->unbind || plan->backend_buffer_size == 0) {
        if (!vmsvga3d_dxvk_d3d11_constant_buffer_destroy(
                dxvk, cid, stage_index, plan->slot)) {
            return VMSVGA3D_D3D11_LEVEL_INVALID;
        }
        return level;
    }

    if (!plan->create_buffer || !plan->has_initial_data ||
        surface_data == NULL || plan->initial_data_offset >= surface_bytes ||
        plan->backend_copy_size >
            surface_bytes - plan->initial_data_offset ||
        plan->backend_copy_size > plan->backend_buffer_size ||
        !vmsvga3d_dxvk_d3d11_constant_buffer_snapshot(
            dxvk, cid, stage_index, plan->slot,
            surface_data + plan->initial_data_offset,
            plan->backend_copy_size, plan->backend_buffer_size)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return level;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffer_offset_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid,
    const VMSVGA3DD3D11ConstantBufferOffsetPlan *offset_plan,
    const SVGA3dConstantBufferBinding *binding,
    const uint8_t *surface_data, uint32_t surface_bytes,
    bool surface_available, bool has_surface_data)
{
    VMSVGA3DD3D11ConstantBufferPlan plan;
    VMSVGA3DD3D11Level level;

    if (dxvk == NULL || offset_plan == NULL || binding == NULL) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d11_constant_buffer_offset_snapshot_plan(
        offset_plan, binding, surface_available, surface_bytes,
        has_surface_data, &plan);
    if (level == VMSVGA3D_D3D11_LEVEL_INVALID) {
        return level;
    }

    /* The caller updates offsetInBytes in the context shadow first, matching
     * VirtualBox, then this helper re-snapshots the same SID/size at the new
     * offset into VMVGA's dedicated native constant-buffer object.
     */
    return vmsvga3d_d3d11_constant_buffer_live(
        dxvk, cid, &plan, surface_data, surface_bytes);
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_constant_buffers_bind_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dShaderType type,
    uint32_t start_slot, uint32_t buffer_count,
    const uint32_t *first_constants, const uint32_t *constant_counts)
{
    VMSVGA3DD3D11Level level;
    uint32_t stage_index;

    if (dxvk == NULL || start_slot > SVGA3D_DX_MAX_CONSTBUFFERS ||
        buffer_count > SVGA3D_DX_MAX_CONSTBUFFERS - start_slot ||
        (buffer_count != 0 &&
         (first_constants == NULL || constant_counts == NULL))) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d11_shader_stage(type, &stage_index);
    if (level == VMSVGA3D_D3D11_LEVEL_INVALID) {
        return level;
    }

    /* The SVGA context shadows 16 slots, but D3D11 exposes only 14 native
     * constant-buffer slots.  Match VirtualBox and preserve slots 14-15 only
     * in the context shadow.
     */
    if (start_slot >= 14u) {
        buffer_count = 0;
    } else if (buffer_count > 14u - start_slot) {
        buffer_count = 14u - start_slot;
    }

    if (buffer_count != 0 &&
        !vmsvga3d_dxvk_d3d11_set_constant_buffers1(
            dxvk, cid, stage_index, start_slot, buffer_count,
            first_constants, constant_counts)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return level;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_define_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id,
    const SVGACOTableDXUAViewEntry *entry)
{
    /* VirtualBox creates the native UAV lazily in pipeline setup or clear. */
    (void)dxvk;
    (void)cid;
    (void)view_id;
    (void)entry;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_destroy_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id)
{
    if (!dxvk || view_id == SVGA3D_INVALID_ID ||
        !vmsvga3d_dxvk_d3d11_unordered_access_view_destroy(
            dxvk, cid, view_id)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_set_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid,
    const VMSVGA3DD3D11UAVSetPlan *plan)
{
    /* VirtualBox defers graphics UAV binding to pipeline setup. */
    (void)dxvk;
    (void)cid;
    (void)plan;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_cs_uav_set_live(
    VMSVGA3DDxvk *dxvk, const VMSVGA3DD3D11CSUAVSetPlan *plan,
    const uint64_t *modified)
{
    if (!dxvk || !plan || !plan->shadow_update_atomic ||
        !vmsvga3d_dxvk_d3d11_unbind_cs_unordered_access_views(
            dxvk, plan->start_index, plan->count, modified)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_ensure_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id,
    VMSVGA3DDxvkSurface *surface, const SVGACOTableDXUAViewEntry *entry,
    uint32_t array_elements)
{
    VMSVGA3DD3D11UAVDesc desc;

    if (!dxvk || !surface || !entry ||
        vmsvga3d_d3d11_uav_desc(entry, array_elements, &desc) ==
            VMSVGA3D_D3D11_LEVEL_INVALID ||
        !vmsvga3d_dxvk_d3d11_unordered_access_view_ensure(
            dxvk, cid, view_id, surface, &desc)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_graphics_uav_bind_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t render_target_count,
    const uint32_t *render_target_ids, uint32_t depth_stencil_view_id,
    uint32_t uav_start_slot, uint32_t uav_count,
    const SVGA3dUAViewId *uav_ids,
    const SVGACOTableDXUAViewEntry *uav_entries, uint32_t cotable_count)
{
    uint32_t initial_counts[SVGA3D_MAX_UAVIEWS];
    uint32_t i;

    if (!dxvk || uav_count > SVGA3D_MAX_UAVIEWS ||
        (uav_count != 0 && (!uav_ids || !uav_entries))) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    for (i = 0; i < uav_count; i++) {
        if (uav_ids[i] == SVGA3D_INVALID_ID) {
            initial_counts[i] = UINT32_MAX;
        } else {
            if (uav_ids[i] >= cotable_count) {
                return VMSVGA3D_D3D11_LEVEL_INVALID;
            }
            initial_counts[i] = uav_entries[uav_ids[i]].structureCount;
        }
    }

    if (!vmsvga3d_dxvk_d3d11_set_render_targets_and_uavs(
            dxvk, cid, render_target_count, render_target_ids,
            depth_stencil_view_id, uav_start_slot, uav_count, uav_ids,
            initial_counts)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_cs_uav_bind_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, uint32_t uav_count,
    const SVGA3dUAViewId *uav_ids,
    const SVGACOTableDXUAViewEntry *uav_entries, uint32_t cotable_count)
{
    uint32_t initial_counts[SVGA3D_MAX_UAVIEWS];
    uint32_t i;

    if (!dxvk || uav_count > SVGA3D_MAX_UAVIEWS ||
        (uav_count != 0 && (!uav_ids || !uav_entries))) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    for (i = 0; i < uav_count; i++) {
        if (uav_ids[i] == SVGA3D_INVALID_ID) {
            initial_counts[i] = UINT32_MAX;
        } else {
            if (uav_ids[i] >= cotable_count) {
                return VMSVGA3D_D3D11_LEVEL_INVALID;
            }
            initial_counts[i] = uav_entries[uav_ids[i]].structureCount;
        }
    }

    if (!vmsvga3d_dxvk_d3d11_set_cs_unordered_access_views(
            dxvk, cid, 0, uav_count, uav_ids, initial_counts)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_clear_uint_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id,
    VMSVGA3DDxvkSurface *surface, const SVGACOTableDXUAViewEntry *entry,
    uint32_t array_elements, const uint32_t values[4])
{
    if (!values ||
        vmsvga3d_d3d11_uav_ensure_live(
            dxvk, cid, view_id, surface, entry, array_elements) ==
            VMSVGA3D_D3D11_LEVEL_INVALID ||
        !vmsvga3d_dxvk_d3d11_clear_unordered_access_view_uint(
            dxvk, cid, view_id, values)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_clear_float_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId view_id,
    VMSVGA3DDxvkSurface *surface, const SVGACOTableDXUAViewEntry *entry,
    uint32_t array_elements, const float values[4])
{
    if (!values ||
        vmsvga3d_d3d11_uav_ensure_live(
            dxvk, cid, view_id, surface, entry, array_elements) ==
            VMSVGA3D_D3D11_LEVEL_INVALID ||
        !vmsvga3d_dxvk_d3d11_clear_unordered_access_view_float(
            dxvk, cid, view_id, values)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_copy_structure_count_live(
    VMSVGA3DDxvk *dxvk, uint32_t cid, SVGA3dUAViewId source_view_id,
    VMSVGA3DDxvkSurface *destination, uint32_t destination_byte_offset)
{
    if (!dxvk || !vmsvga3d_dxvk_d3d11_copy_structure_count(
                     dxvk, destination, destination_byte_offset, cid,
                     source_view_id)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_dispatch_live(
    VMSVGA3DDxvk *dxvk, uint32_t thread_group_count_x,
    uint32_t thread_group_count_y, uint32_t thread_group_count_z)
{
    if (!dxvk || !vmsvga3d_dxvk_d3d11_dispatch(
                     dxvk, thread_group_count_x, thread_group_count_y,
                     thread_group_count_z)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_draw_indexed_instanced_indirect_live(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *args_buffer,
    uint32_t aligned_byte_offset)
{
    if (!dxvk || !vmsvga3d_dxvk_d3d11_draw_indexed_instanced_indirect(
                     dxvk, args_buffer, aligned_byte_offset)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_draw_instanced_indirect_live(
    VMSVGA3DDxvk *dxvk, VMSVGA3DDxvkSurface *args_buffer,
    uint32_t aligned_byte_offset)
{
    if (!dxvk || !vmsvga3d_dxvk_d3d11_draw_instanced_indirect(
                     dxvk, args_buffer, aligned_byte_offset)) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_query_define_entry(
    const SVGA3dCmdDXDefineQuery *src, SVGACOTableDXQueryEntry *dst)
{
    VMSVGA3DD3D11QueryInfo info;

    if (!src || !dst || src->type < SVGA3D_QUERYTYPE_MIN ||
        src->type >= SVGA3D_QUERYTYPE_MAX ||
        vmsvga3d_d3d11_query_info(src->type, src->flags, &info) ==
            VMSVGA3D_D3D11_LEVEL_INVALID) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    dst->type = src->type;
    dst->state = SVGADX_QDSTATE_IDLE;
    dst->flags = src->flags;
    dst->mobid = SVGA3D_INVALID_ID;
    dst->offset = 0;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Format vmsvga3d_d3d11_surface_format(SVGA3dSurfaceFormat format)
{
    VMSVGA3DD3D10Format base = vmsvga3d_d3d10_surface_format(format);
    VMSVGA3DD3D11Format result = { base.dxgi_format, d3d11_level(base.min_level) };

    return result;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_resource_policy(
    SVGA3dSurfaceAllFlags flags, bool texture_resource,
    uint32_t buffer_byte_stride, VMSVGA3DD3D11ResourcePolicy *policy)
{
    VMSVGA3DD3D10ResourcePolicy base;
    VMSVGA3DD3D10Level base_level;
    uint32_t bind;
    uint32_t misc;

    if (!policy) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    base_level = vmsvga3d_d3d10_resource_policy(flags, texture_resource, &base);
    if (base_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    bind = 0;
    if (flags & (SVGA3D_SURFACE_BIND_VERTEX_BUFFER | SVGA3D_SURFACE_HINT_VERTEXBUFFER)) {
        bind |= D3D11_BIND_VERTEX_BUFFER;
    }

    if (flags & (SVGA3D_SURFACE_BIND_INDEX_BUFFER | SVGA3D_SURFACE_HINT_INDEXBUFFER)) {
        bind |= D3D11_BIND_INDEX_BUFFER;
    }

    if (flags & SVGA3D_SURFACE_BIND_CONSTANT_BUFFER) {
        bind |= D3D11_BIND_CONSTANT_BUFFER;
    }

    if (flags & SVGA3D_SURFACE_BIND_SHADER_RESOURCE) {
        bind |= D3D11_BIND_SHADER_RESOURCE;
    }

    if (flags & SVGA3D_SURFACE_BIND_RENDER_TARGET) {
        bind |= D3D11_BIND_RENDER_TARGET;
    }

    if (flags & SVGA3D_SURFACE_BIND_DEPTH_STENCIL) {
        bind |= D3D11_BIND_DEPTH_STENCIL;
    }

    if (flags & SVGA3D_SURFACE_BIND_STREAM_OUTPUT) {
        bind |= D3D11_BIND_STREAM_OUTPUT;
    }

    if (flags & SVGA3D_SURFACE_BIND_UAVIEW) {
        bind |= D3D11_BIND_UNORDERED_ACCESS;
    }

    if (flags & SVGA3D_SURFACE_RESERVED1) {
        bind |= D3D11_BIND_DECODER;
    }

    if (flags & SVGA3D_SURFACE_SCREENTARGET) {
        bind |= D3D11_BIND_SHADER_RESOURCE;
    }

    misc = 0;
    if (flags & SVGA3D_SURFACE_CUBEMAP) {
        misc |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    }

    if (flags & SVGA3D_SURFACE_DRAWINDIRECT_ARGS) {
        misc |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }

    if (flags & SVGA3D_SURFACE_BIND_RAW_VIEWS) {
        misc |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    }

    if (flags & SVGA3D_SURFACE_BUFFER_STRUCTURED) {
        misc |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    }

    if (flags & SVGA3D_SURFACE_RESOURCE_CLAMP) {
        misc |= D3D11_RESOURCE_MISC_RESOURCE_CLAMP;
    }

    if (texture_resource && (flags & SVGA3D_SURFACE_AUTOGENMIPMAPS) &&
        (bind & (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)) ==
            (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)) {
        misc |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    policy->usage = base.usage;
    policy->bind_flags = bind;
    policy->cpu_access_flags = base.cpu_access_flags;
    policy->misc_flags = misc;
    policy->structure_byte_stride =
        !texture_resource && (misc & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
        ? buffer_byte_stride : 0;

    return flags & (SVGA3D_SURFACE_BIND_LOGICOPS | SVGA3D_SURFACE_RESERVED1)
         ? VMSVGA3D_D3D11_LEVEL_11_1
         : VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_input_element(
    const SVGA3dInputElementDesc *src, VMSVGA3DD3D11InputElement *dst)
{
    return d3d11_level(vmsvga3d_d3d10_input_element(src, dst));
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_blend_state(
    const SVGACOTableDXBlendStateEntry *src, VMSVGA3DD3D11BlendDesc *dst)
{
    VMSVGA3DD3D10BlendDesc base;
    VMSVGA3DD3D10Level level;
    uint32_t i;

    if (!src || !dst) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_blend_state(src, &base);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    dst->alpha_to_coverage_enable = base.alpha_to_coverage_enable;
    dst->independent_blend_enable = base.independent_blend_enable;

    for (i = 0; i < SVGA3D_DX_MAX_RENDER_TARGETS; i++) {
        dst->render_target[i].blend_enable = base.render_target[i].blend_enable;
        dst->render_target[i].logic_op_enable = !!src->perRT[i].logicOpEnable;
        dst->render_target[i].src_blend = base.render_target[i].src_blend;
        dst->render_target[i].dest_blend = base.render_target[i].dest_blend;
        dst->render_target[i].blend_op = base.render_target[i].blend_op;
        dst->render_target[i].src_blend_alpha = base.render_target[i].src_blend_alpha;
        dst->render_target[i].dest_blend_alpha = base.render_target[i].dest_blend_alpha;
        dst->render_target[i].blend_op_alpha = base.render_target[i].blend_op_alpha;
        dst->render_target[i].logic_op = src->perRT[i].logicOp;
        dst->render_target[i].write_mask = base.render_target[i].write_mask;
    }

    return VMSVGA3D_D3D11_LEVEL_11_1;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_depth_stencil_state(
    const SVGACOTableDXDepthStencilEntry *src,
    VMSVGA3DD3D11DepthStencilDesc *dst)
{
    return d3d11_level(vmsvga3d_d3d10_depth_stencil_state(src, dst));
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_rasterizer_state(
    const SVGACOTableDXRasterizerStateEntry *src,
    VMSVGA3DD3D11RasterizerDesc *dst)
{
    VMSVGA3DD3D10RasterizerDesc base;
    VMSVGA3DD3D10Level level;

    if (!src || !dst) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_rasterizer_state(src, &base);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    dst->fill_mode = base.fill_mode;
    dst->cull_mode = base.cull_mode;
    dst->front_counter_clockwise = base.front_counter_clockwise;
    dst->depth_bias = base.depth_bias;
    dst->depth_bias_clamp = base.depth_bias_clamp;
    dst->slope_scaled_depth_bias = base.slope_scaled_depth_bias;
    dst->depth_clip_enable = base.depth_clip_enable;
    dst->scissor_enable = base.scissor_enable;
    dst->multisample_enable = base.multisample_enable;
    dst->antialiased_line_enable = base.antialiased_line_enable;
    dst->forced_sample_count = src->forcedSampleCount;

    return VMSVGA3D_D3D11_LEVEL_11_1;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_sampler_state(
    const SVGACOTableDXSamplerEntry *src, VMSVGA3DD3D11SamplerDesc *dst)
{
    return d3d11_level(vmsvga3d_d3d10_sampler_state(src, dst));
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_primitive_topology(
    SVGA3dPrimitiveType primitive, uint32_t *d3d_topology)
{
    return d3d11_level(vmsvga3d_d3d10_primitive_topology(primitive, d3d_topology));
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_shader_stage(
    SVGA3dShaderType shader_type, uint32_t *stage_index)
{
    return d3d11_level(vmsvga3d_d3d10_shader_stage(shader_type, stage_index));
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_query_info(
    SVGA3dQueryType type, uint32_t flags, VMSVGA3DD3D11QueryInfo *info)
{
    return d3d11_level(vmsvga3d_d3d10_query_info(type, flags, info));
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_srv_desc(
    const SVGACOTableDXSRViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D11SRVDesc *dst)
{
    VMSVGA3DD3D10SRVDesc base;
    VMSVGA3DD3D10Level level;

    if (!src || !dst) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    if (src->resourceDimension == SVGA3D_RESOURCE_BUFFEREX) {
        VMSVGA3DD3D11Format format = vmsvga3d_d3d11_surface_format(src->format);
        if (format.min_level == VMSVGA3D_D3D11_LEVEL_INVALID &&
            src->format != SVGA3D_BUFFER) {
            return VMSVGA3D_D3D11_LEVEL_INVALID;
        }
        dst->format = format.dxgi_format;
        dst->view_dimension = D3D11_SRV_BUFFEREX;
        dst->first_element = src->desc.bufferex.firstElement;
        dst->num_elements = src->desc.bufferex.numElements;
        dst->flags = src->desc.bufferex.flags;
        return VMSVGA3D_D3D11_LEVEL_11_0;
    }

    level = vmsvga3d_d3d10_srv_desc(src, array_elements,
                                    multisample_count, &base);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    dst->format = base.format;
    dst->view_dimension = base.view_dimension;
    dst->most_detailed_mip = base.most_detailed_mip;
    dst->mip_levels = base.mip_levels;
    dst->first_array_slice = base.first_array_slice;
    dst->array_size = base.array_size;
    dst->first_element = base.first_element;
    dst->num_elements = base.num_elements;

    return d3d11_level(level);
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_rtv_desc(
    const SVGACOTableDXRTViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D11RTVDesc *dst)
{
    VMSVGA3DD3D10Level level;

    if (!src || !dst || src->resourceDimension == SVGA3D_RESOURCE_BUFFEREX) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_rtv_desc(src, array_elements,
                                    multisample_count, dst);

    return d3d11_level(level);
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_dsv_desc(
    const SVGACOTableDXDSViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D11DSVDesc *dst)
{
    VMSVGA3DD3D10DSVDesc base;
    VMSVGA3DD3D10Level level;

    if (!src || !dst) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_dsv_desc(src, array_elements,
                                    multisample_count, &base);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    dst->format = base.format;
    dst->view_dimension = base.view_dimension;
    dst->flags = src->flags;
    dst->mip_slice = base.mip_slice;
    dst->first_array_slice = base.first_array_slice;
    dst->array_size = base.array_size;

    return VMSVGA3D_D3D11_LEVEL_11_0;
}

VMSVGA3DD3D11Level vmsvga3d_d3d11_uav_desc(
    const SVGACOTableDXUAViewEntry *src, uint32_t array_elements,
    VMSVGA3DD3D11UAVDesc *dst)
{
    VMSVGA3DD3D11Format format;

    if (!src || !dst) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    format = vmsvga3d_d3d11_surface_format(src->format);
    if (format.min_level == VMSVGA3D_D3D11_LEVEL_INVALID &&
        src->format != SVGA3D_BUFFER) {
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }

    dst->format = format.dxgi_format;

    switch (src->resourceDimension) {
    case SVGA3D_RESOURCE_BUFFER:
        dst->view_dimension = D3D11_UAV_BUFFER;
        dst->first_element = src->desc.buffer.firstElement;
        dst->num_elements = src->desc.buffer.numElements;
        dst->flags = src->desc.buffer.flags;
        break;
    case SVGA3D_RESOURCE_TEXTURE1D:
        dst->view_dimension = array_elements <= 1
                            ? D3D11_UAV_TEXTURE1D
                            : D3D11_UAV_TEXTURE1DARRAY;
        dst->mip_slice = src->desc.tex.mipSlice;
        if (array_elements > 1) {
            dst->first_array_slice = src->desc.tex.firstArraySlice;
            dst->array_size = src->desc.tex.arraySize;
        }
        break;
    case SVGA3D_RESOURCE_TEXTURE2D:
        dst->view_dimension = array_elements <= 1
                            ? D3D11_UAV_TEXTURE2D
                            : D3D11_UAV_TEXTURE2DARRAY;
        dst->mip_slice = src->desc.tex.mipSlice;
        if (array_elements > 1) {
            dst->first_array_slice = src->desc.tex.firstArraySlice;
            dst->array_size = src->desc.tex.arraySize;
        }
        break;
    case SVGA3D_RESOURCE_TEXTURE3D:
        /* Preserve the zero-initialized dimension for this 3D-texture path. */
        dst->view_dimension = D3D11_UAV_UNKNOWN;
        dst->mip_slice = src->desc.tex3D.mipSlice;
        dst->first_w_slice = src->desc.tex3D.firstW;
        dst->w_size = src->desc.tex3D.wSize;
        break;
    default:
        return VMSVGA3D_D3D11_LEVEL_INVALID;
    }
    return format.min_level == VMSVGA3D_D3D11_LEVEL_11_1
         ? VMSVGA3D_D3D11_LEVEL_11_1
         : VMSVGA3D_D3D11_LEVEL_11_0;
}
