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
