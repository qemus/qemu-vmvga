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

#include "include/vmware_vga_d3d10.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Public DXGI ABI values. */
enum {
    DXGI_UNKNOWN = 0,
    DXGI_R32G32B32A32_TYPELESS = 1,
    DXGI_R32G32B32A32_FLOAT = 2,
    DXGI_R32G32B32A32_UINT = 3,
    DXGI_R32G32B32A32_SINT = 4,
    DXGI_R32G32B32_TYPELESS = 5,
    DXGI_R32G32B32_FLOAT = 6,
    DXGI_R32G32B32_UINT = 7,
    DXGI_R32G32B32_SINT = 8,
    DXGI_R16G16B16A16_TYPELESS = 9,
    DXGI_R16G16B16A16_FLOAT = 10,
    DXGI_R16G16B16A16_UNORM = 11,
    DXGI_R16G16B16A16_UINT = 12,
    DXGI_R16G16B16A16_SNORM = 13,
    DXGI_R16G16B16A16_SINT = 14,
    DXGI_R32G32_TYPELESS = 15,
    DXGI_R32G32_FLOAT = 16,
    DXGI_R32G32_UINT = 17,
    DXGI_R32G32_SINT = 18,
    DXGI_R32G8X24_TYPELESS = 19,
    DXGI_D32_FLOAT_S8X24_UINT = 20,
    DXGI_R32_FLOAT_X8X24_TYPELESS = 21,
    DXGI_X32_TYPELESS_G8X24_UINT = 22,
    DXGI_R10G10B10A2_TYPELESS = 23,
    DXGI_R10G10B10A2_UNORM = 24,
    DXGI_R10G10B10A2_UINT = 25,
    DXGI_R11G11B10_FLOAT = 26,
    DXGI_R8G8B8A8_TYPELESS = 27,
    DXGI_R8G8B8A8_UNORM = 28,
    DXGI_R8G8B8A8_UNORM_SRGB = 29,
    DXGI_R8G8B8A8_UINT = 30,
    DXGI_R8G8B8A8_SNORM = 31,
    DXGI_R8G8B8A8_SINT = 32,
    DXGI_R16G16_TYPELESS = 33,
    DXGI_R16G16_FLOAT = 34,
    DXGI_R16G16_UNORM = 35,
    DXGI_R16G16_UINT = 36,
    DXGI_R16G16_SNORM = 37,
    DXGI_R16G16_SINT = 38,
    DXGI_R32_TYPELESS = 39,
    DXGI_D32_FLOAT = 40,
    DXGI_R32_FLOAT = 41,
    DXGI_R32_UINT = 42,
    DXGI_R32_SINT = 43,
    DXGI_R24G8_TYPELESS = 44,
    DXGI_D24_UNORM_S8_UINT = 45,
    DXGI_R24_UNORM_X8_TYPELESS = 46,
    DXGI_X24_TYPELESS_G8_UINT = 47,
    DXGI_R8G8_TYPELESS = 48,
    DXGI_R8G8_UNORM = 49,
    DXGI_R8G8_UINT = 50,
    DXGI_R8G8_SNORM = 51,
    DXGI_R8G8_SINT = 52,
    DXGI_R16_TYPELESS = 53,
    DXGI_R16_FLOAT = 54,
    DXGI_D16_UNORM = 55,
    DXGI_R16_UNORM = 56,
    DXGI_R16_UINT = 57,
    DXGI_R16_SNORM = 58,
    DXGI_R16_SINT = 59,
    DXGI_R8_TYPELESS = 60,
    DXGI_R8_UNORM = 61,
    DXGI_R8_UINT = 62,
    DXGI_R8_SNORM = 63,
    DXGI_R8_SINT = 64,
    DXGI_A8_UNORM = 65,
    DXGI_R9G9B9E5_SHAREDEXP = 67,
    DXGI_R8G8_B8G8_UNORM = 68,
    DXGI_G8R8_G8B8_UNORM = 69,
    DXGI_BC1_TYPELESS = 70,
    DXGI_BC1_UNORM = 71,
    DXGI_BC1_UNORM_SRGB = 72,
    DXGI_BC2_TYPELESS = 73,
    DXGI_BC2_UNORM = 74,
    DXGI_BC2_UNORM_SRGB = 75,
    DXGI_BC3_TYPELESS = 76,
    DXGI_BC3_UNORM = 77,
    DXGI_BC3_UNORM_SRGB = 78,
    DXGI_BC4_TYPELESS = 79,
    DXGI_BC4_UNORM = 80,
    DXGI_BC4_SNORM = 81,
    DXGI_BC5_TYPELESS = 82,
    DXGI_BC5_UNORM = 83,
    DXGI_BC5_SNORM = 84,
    DXGI_B5G6R5_UNORM = 85,
    DXGI_B5G5R5A1_UNORM = 86,
    DXGI_B8G8R8A8_UNORM = 87,
    DXGI_B8G8R8X8_UNORM = 88,
    DXGI_R10G10B10_XR_BIAS_A2_UNORM = 89,
    DXGI_B8G8R8A8_TYPELESS = 90,
    DXGI_B8G8R8A8_UNORM_SRGB = 91,
    DXGI_B8G8R8X8_TYPELESS = 92,
    DXGI_B8G8R8X8_UNORM_SRGB = 93,
    DXGI_BC6H_TYPELESS = 94,
    DXGI_BC6H_UF16 = 95,
    DXGI_BC6H_SF16 = 96,
    DXGI_BC7_TYPELESS = 97,
    DXGI_BC7_UNORM = 98,
    DXGI_BC7_UNORM_SRGB = 99,
    DXGI_AYUV = 100,
    DXGI_NV12 = 103,
    DXGI_YUY2 = 107,
    DXGI_B4G4R4A4_UNORM = 115,
};

/* D3D10 / D3D10.1 ABI values shared by the corresponding D3D11 enums. */
enum {
    D3D10_BLEND_ZERO = 1,
    D3D10_BLEND_ONE = 2,
    D3D10_BLEND_SRC_COLOR = 3,
    D3D10_BLEND_INV_SRC_COLOR = 4,
    D3D10_BLEND_SRC_ALPHA = 5,
    D3D10_BLEND_INV_SRC_ALPHA = 6,
    D3D10_BLEND_DEST_ALPHA = 7,
    D3D10_BLEND_INV_DEST_ALPHA = 8,
    D3D10_BLEND_DEST_COLOR = 9,
    D3D10_BLEND_INV_DEST_COLOR = 10,
    D3D10_BLEND_SRC_ALPHA_SAT = 11,
    D3D10_BLEND_BLEND_FACTOR = 14,
    D3D10_BLEND_INV_BLEND_FACTOR = 15,
    D3D10_BLEND_SRC1_COLOR = 16,
    D3D10_BLEND_INV_SRC1_COLOR = 17,
    D3D10_BLEND_SRC1_ALPHA = 18,
    D3D10_BLEND_INV_SRC1_ALPHA = 19,

    D3D10_FILL_WIREFRAME = 2,
    D3D10_FILL_SOLID = 3,

    D3D10_TOPOLOGY_UNDEFINED = 0,
    D3D10_TOPOLOGY_POINTLIST = 1,
    D3D10_TOPOLOGY_LINELIST = 2,
    D3D10_TOPOLOGY_LINESTRIP = 3,
    D3D10_TOPOLOGY_TRIANGLELIST = 4,
    D3D10_TOPOLOGY_TRIANGLESTRIP = 5,
    D3D10_TOPOLOGY_LINELIST_ADJ = 10,
    D3D10_TOPOLOGY_LINESTRIP_ADJ = 11,
    D3D10_TOPOLOGY_TRIANGLELIST_ADJ = 12,
    D3D10_TOPOLOGY_TRIANGLESTRIP_ADJ = 13,

    D3D10_SRV_BUFFER = 1,
    D3D10_SRV_TEXTURE1D = 2,
    D3D10_SRV_TEXTURE1DARRAY = 3,
    D3D10_SRV_TEXTURE2D = 4,
    D3D10_SRV_TEXTURE2DARRAY = 5,
    D3D10_SRV_TEXTURE2DMS = 6,
    D3D10_SRV_TEXTURE2DMSARRAY = 7,
    D3D10_SRV_TEXTURE3D = 8,
    D3D10_SRV_TEXTURECUBE = 9,
    D3D10_1_SRV_TEXTURECUBEARRAY = 10,

    D3D10_RTV_BUFFER = 1,
    D3D10_RTV_TEXTURE1D = 2,
    D3D10_RTV_TEXTURE1DARRAY = 3,
    D3D10_RTV_TEXTURE2D = 4,
    D3D10_RTV_TEXTURE2DARRAY = 5,
    D3D10_RTV_TEXTURE2DMS = 6,
    D3D10_RTV_TEXTURE2DMSARRAY = 7,
    D3D10_RTV_TEXTURE3D = 8,

    D3D10_DSV_TEXTURE1D = 1,
    D3D10_DSV_TEXTURE1DARRAY = 2,
    D3D10_DSV_TEXTURE2D = 3,
    D3D10_DSV_TEXTURE2DARRAY = 4,
    D3D10_DSV_TEXTURE2DMS = 5,
    D3D10_DSV_TEXTURE2DMSARRAY = 6,

    D3D10_QUERY_OCCLUSION = 1,
    D3D10_QUERY_TIMESTAMP = 2,
    D3D10_QUERY_TIMESTAMP_DISJOINT = 3,
    D3D10_QUERY_PIPELINE_STATISTICS = 4,
    D3D10_QUERY_OCCLUSION_PREDICATE = 5,
    D3D10_QUERY_SO_STATISTICS = 6,
    D3D10_QUERY_SO_OVERFLOW_PREDICATE = 7,
    D3D11_QUERY_SO_STATISTICS_STREAM0 = 8,
    D3D11_QUERY_SO_STATISTICS_STREAM1 = 9,
    D3D11_QUERY_SO_STATISTICS_STREAM2 = 10,
    D3D11_QUERY_SO_STATISTICS_STREAM3 = 11,
    D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0 = 12,
    D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1 = 13,
    D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2 = 14,
    D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3 = 15,

    D3D10_USAGE_DEFAULT = 0,
    D3D10_USAGE_IMMUTABLE = 1,
    D3D10_USAGE_DYNAMIC = 2,
    D3D10_USAGE_STAGING = 3,

    D3D10_BIND_VERTEX_BUFFER = 0x01,
    D3D10_BIND_INDEX_BUFFER = 0x02,
    D3D10_BIND_CONSTANT_BUFFER = 0x04,
    D3D10_BIND_SHADER_RESOURCE = 0x08,
    D3D10_BIND_STREAM_OUTPUT = 0x10,
    D3D10_BIND_RENDER_TARGET = 0x20,
    D3D10_BIND_DEPTH_STENCIL = 0x40,

    D3D10_CPU_ACCESS_WRITE = 0x10000,
    D3D10_CPU_ACCESS_READ = 0x20000,

    D3D10_RESOURCE_DIMENSION_BUFFER = 1,
    D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
    D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
    D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4,

    D3D10_RESOURCE_MISC_GENERATE_MIPS = 0x01,
    D3D10_RESOURCE_MISC_TEXTURECUBE = 0x04,
};

static VMSVGA3DD3D10Level max_level(VMSVGA3DD3D10Level a,
                                     VMSVGA3DD3D10Level b)
{
    return a > b ? a : b;
}

static VMSVGA3DD3D10Format fmt(uint32_t value, VMSVGA3DD3D10Level level)
{
    VMSVGA3DD3D10Format out = { value, level };

    return out;
}

VMSVGA3DD3D10Format vmsvga3d_d3d10_surface_format(SVGA3dSurfaceFormat format)
{
    switch (format) {
      /* Use alpha-capable B8 formats for X8 surfaces so all host operations remain valid. */
    case SVGA3D_X8R8G8B8:
        return fmt(DXGI_B8G8R8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_A8R8G8B8:
        return fmt(DXGI_B8G8R8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_R5G6B5:
        return fmt(DXGI_B5G6R5_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_X1R5G5B5:
    case SVGA3D_A1R5G5B5:
        return fmt(DXGI_B5G5R5A1_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_A4R4G4B4:
        return fmt(DXGI_B4G4R4A4_UNORM, VMSVGA3D_D3D10_LEVEL_11_1);
    case SVGA3D_Z_D16:
    case SVGA3D_Z_DF16:
    case SVGA3D_D16_UNORM:
        return fmt(DXGI_D16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_Z_D24S8:
    case SVGA3D_Z_DF24:
    case SVGA3D_Z_D24S8_INT:
    case SVGA3D_D24_UNORM_S8_UINT:
        return fmt(DXGI_D24_UNORM_S8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_DXT1:
        return fmt(DXGI_BC1_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_DXT2:
    case SVGA3D_DXT3:
        return fmt(DXGI_BC2_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_DXT4:
    case SVGA3D_DXT5:
        return fmt(DXGI_BC3_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BUMPU8V8:
    case SVGA3D_V8U8:
        return fmt(DXGI_R8G8_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_Q8W8V8U8:
        return fmt(DXGI_R8G8B8A8_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_ARGB_S10E5:
        return fmt(DXGI_R16G16B16A16_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_ARGB_S23E8:
        return fmt(DXGI_R32G32B32A32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_A2R10G10B10:
        return fmt(DXGI_R10G10B10A2_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_ALPHA8:
        return fmt(DXGI_A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R_S10E5:
        return fmt(DXGI_R16_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R_S23E8:
        return fmt(DXGI_R32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_RG_S10E5:
        return fmt(DXGI_R16G16_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_RG_S23E8:
        return fmt(DXGI_R32G32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_Z_D24X8:
        return fmt(DXGI_D24_UNORM_S8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_V16U16:
        return fmt(DXGI_R16G16_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_G16R16:
        return fmt(DXGI_R16G16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_A16B16G16R16:
        return fmt(DXGI_R16G16B16A16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_YUY2:
        return fmt(DXGI_YUY2, VMSVGA3D_D3D10_LEVEL_11_0);
    case SVGA3D_NV12:
        return fmt(DXGI_NV12, VMSVGA3D_D3D10_LEVEL_11_0);
    case SVGA3D_R32G32B32A32_TYPELESS:
        return fmt(DXGI_R32G32B32A32_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32B32A32_FLOAT:
        return fmt(DXGI_R32G32B32A32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32B32A32_UINT:
        return fmt(DXGI_R32G32B32A32_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32B32A32_SINT:
        return fmt(DXGI_R32G32B32A32_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32B32_TYPELESS:
        return fmt(DXGI_R32G32B32_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32B32_FLOAT:
        return fmt(DXGI_R32G32B32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32B32_UINT:
        return fmt(DXGI_R32G32B32_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32B32_SINT:
        return fmt(DXGI_R32G32B32_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16B16A16_TYPELESS:
        return fmt(DXGI_R16G16B16A16_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16B16A16_FLOAT:
        return fmt(DXGI_R16G16B16A16_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16B16A16_UNORM:
        return fmt(DXGI_R16G16B16A16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16B16A16_UINT:
        return fmt(DXGI_R16G16B16A16_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16B16A16_SNORM:
        return fmt(DXGI_R16G16B16A16_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16B16A16_SINT:
        return fmt(DXGI_R16G16B16A16_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32_TYPELESS:
        return fmt(DXGI_R32G32_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32_FLOAT:
        return fmt(DXGI_R32G32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32_UINT:
        return fmt(DXGI_R32G32_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G32_SINT:
        return fmt(DXGI_R32G32_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32G8X24_TYPELESS:
        return fmt(DXGI_R32G8X24_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_D32_FLOAT_S8X24_UINT:
        return fmt(DXGI_D32_FLOAT_S8X24_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32_FLOAT_X8X24:
        return fmt(DXGI_R32_FLOAT_X8X24_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_X32_G8X24_UINT:
        return fmt(DXGI_X32_TYPELESS_G8X24_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R10G10B10A2_TYPELESS:
        return fmt(DXGI_R10G10B10A2_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R10G10B10A2_UNORM:
        return fmt(DXGI_R10G10B10A2_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R10G10B10A2_UINT:
        return fmt(DXGI_R10G10B10A2_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R11G11B10_FLOAT:
        return fmt(DXGI_R11G11B10_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8B8A8_TYPELESS:
        return fmt(DXGI_R8G8B8A8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8B8A8_UNORM:
        return fmt(DXGI_R8G8B8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8B8A8_UNORM_SRGB:
        return fmt(DXGI_R8G8B8A8_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8B8A8_UINT:
        return fmt(DXGI_R8G8B8A8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8B8A8_SNORM:
        return fmt(DXGI_R8G8B8A8_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8B8A8_SINT:
        return fmt(DXGI_R8G8B8A8_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16_TYPELESS:
        return fmt(DXGI_R16G16_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16_FLOAT:
        return fmt(DXGI_R16G16_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16_UNORM:
        return fmt(DXGI_R16G16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16_UINT:
        return fmt(DXGI_R16G16_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16_SNORM:
        return fmt(DXGI_R16G16_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16G16_SINT:
        return fmt(DXGI_R16G16_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32_TYPELESS:
        return fmt(DXGI_R32_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_D32_FLOAT:
        return fmt(DXGI_D32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32_FLOAT:
        return fmt(DXGI_R32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32_UINT:
        return fmt(DXGI_R32_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R32_SINT:
        return fmt(DXGI_R32_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R24G8_TYPELESS:
        return fmt(DXGI_R24G8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R24_UNORM_X8:
        return fmt(DXGI_R24_UNORM_X8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_X24_G8_UINT:
        return fmt(DXGI_X24_TYPELESS_G8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8_TYPELESS:
        return fmt(DXGI_R8G8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8_UNORM:
        return fmt(DXGI_R8G8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8_UINT:
        return fmt(DXGI_R8G8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8_SNORM:
        return fmt(DXGI_R8G8_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8_SINT:
        return fmt(DXGI_R8G8_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16_TYPELESS:
        return fmt(DXGI_R16_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16_FLOAT:
        return fmt(DXGI_R16_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16_UNORM:
        return fmt(DXGI_R16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16_UINT:
        return fmt(DXGI_R16_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16_SNORM:
        return fmt(DXGI_R16_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R16_SINT:
        return fmt(DXGI_R16_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8_TYPELESS:
        return fmt(DXGI_R8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8_UNORM:
        return fmt(DXGI_R8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8_UINT:
        return fmt(DXGI_R8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8_SNORM:
        return fmt(DXGI_R8_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8_SINT:
        return fmt(DXGI_R8_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_A8_UNORM:
        return fmt(DXGI_A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R9G9B9E5_SHAREDEXP:
        return fmt(DXGI_R9G9B9E5_SHAREDEXP, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R8G8_B8G8_UNORM:
        return fmt(DXGI_R8G8_B8G8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_G8R8_G8B8_UNORM:
        return fmt(DXGI_G8R8_G8B8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC1_TYPELESS:
        return fmt(DXGI_BC1_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC1_UNORM:
        return fmt(DXGI_BC1_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC1_UNORM_SRGB:
        return fmt(DXGI_BC1_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC2_TYPELESS:
        return fmt(DXGI_BC2_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC2_UNORM:
        return fmt(DXGI_BC2_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC2_UNORM_SRGB:
        return fmt(DXGI_BC2_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC3_TYPELESS:
        return fmt(DXGI_BC3_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC3_UNORM:
        return fmt(DXGI_BC3_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC3_UNORM_SRGB:
        return fmt(DXGI_BC3_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC4_TYPELESS:
        return fmt(DXGI_BC4_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_ATI1:
    case SVGA3D_BC4_UNORM:
        return fmt(DXGI_BC4_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC4_SNORM:
        return fmt(DXGI_BC4_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC5_TYPELESS:
        return fmt(DXGI_BC5_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_ATI2:
    case SVGA3D_BC5_UNORM:
        return fmt(DXGI_BC5_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_BC5_SNORM:
        return fmt(DXGI_BC5_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
    case SVGA3D_R10G10B10_XR_BIAS_A2_UNORM:
        return fmt(DXGI_R10G10B10_XR_BIAS_A2_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_B8G8R8A8_TYPELESS:
        return fmt(DXGI_B8G8R8A8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_B5G6R5_UNORM:
        return fmt(DXGI_B5G6R5_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_B5G5R5A1_UNORM:
        return fmt(DXGI_B5G5R5A1_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_B8G8R8A8_UNORM:
        return fmt(DXGI_B8G8R8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_B8G8R8A8_UNORM_SRGB:
        return fmt(DXGI_B8G8R8A8_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_B8G8R8X8_TYPELESS:
        return fmt(DXGI_B8G8R8A8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_B8G8R8X8_UNORM:
        return fmt(DXGI_B8G8R8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_B8G8R8X8_UNORM_SRGB:
        return fmt(DXGI_B8G8R8A8_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_1);
    case SVGA3D_BC6H_TYPELESS:
        return fmt(DXGI_BC6H_TYPELESS, VMSVGA3D_D3D10_LEVEL_11_0);
    case SVGA3D_BC6H_UF16:
        return fmt(DXGI_BC6H_UF16, VMSVGA3D_D3D10_LEVEL_11_0);
    case SVGA3D_BC6H_SF16:
        return fmt(DXGI_BC6H_SF16, VMSVGA3D_D3D10_LEVEL_11_0);
    case SVGA3D_BC7_TYPELESS:
        return fmt(DXGI_BC7_TYPELESS, VMSVGA3D_D3D10_LEVEL_11_0);
    case SVGA3D_BC7_UNORM:
        return fmt(DXGI_BC7_UNORM, VMSVGA3D_D3D10_LEVEL_11_0);
    case SVGA3D_BC7_UNORM_SRGB:
        return fmt(DXGI_BC7_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_11_0);
    default:
        return fmt(DXGI_UNKNOWN, VMSVGA3D_D3D10_LEVEL_INVALID);
    }
}

bool vmsvga3d_d3d10_is_srgb_format(uint32_t format)
{
    return format == 29u || format == 91u || format == 93u;
}

uint32_t vmsvga3d_d3d10_typeless_format(uint32_t format)
{
    switch (format) {
    case 2: case 3: case 4:
        return 1;
    case 6: case 7: case 8:
        return 5;
    case 10: case 11: case 12: case 13: case 14:
        return 9;
    case 16: case 17: case 18:
        return 15;
    case 20: case 21: case 22:
        return 19;
    case 24: case 25:
        return 23;
    case 28: case 29: case 30: case 31: case 32:
        return 27;
    case 34: case 35: case 36: case 37: case 38:
        return 33;
    case 40: case 41: case 42: case 43:
        return 39;
    case 45: case 46: case 47:
        return 44;
    case 49: case 50: case 51: case 52:
        return 48;
    case 54: case 55: case 56: case 57: case 58: case 59:
        return 53;
    case 61: case 62: case 63: case 64:
        return 60;
    case 71: case 72:
        return 70;
    case 74: case 75:
        return 73;
    case 77: case 78:
        return 76;
    case 80: case 81:
        return 79;
    case 83: case 84:
        return 82;
    case 87: case 91:
        return 90;
    case 88: case 93:
        return 92;
    case 95: case 96:
        return 94;
    case 98: case 99:
        return 97;
    default:
        return format;
    }
}

bool vmsvga3d_d3d10_is_depth_stencil_format(uint32_t format)
{
    return format == 20u || format == 40u || format == 45u || format == 55u;
}

uint32_t vmsvga3d_d3d10_resource_format(SVGA3dSurfaceFormat format,
                                        SVGA3dSurfaceAllFlags flags)
{
    VMSVGA3DD3D10Format translated = vmsvga3d_d3d10_surface_format(format);
    uint32_t result = translated.dxgi_format;

    (void)flags;
    if (translated.min_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return 0;
    }

    if (!vmsvga3d_d3d10_is_depth_stencil_format(result)) {
        result = vmsvga3d_d3d10_typeless_format(result);
    }

    return result;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_resource_policy(
    SVGA3dSurfaceAllFlags flags, bool texture_resource,
    VMSVGA3DD3D10ResourcePolicy *policy)
{
    VMSVGA3DD3D10Level level = VMSVGA3D_D3D10_LEVEL_10_0;
    uint32_t usage = D3D10_USAGE_DEFAULT;
    uint32_t bind = 0;
    uint32_t cpu = 0;
    uint32_t misc = 0;

    if (!policy) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    /*
     * Texture resources stay DEFAULT regardless of the guest usage hints.
     * Only the generic buffer path applies staging/dynamic usage to the primary.
     */
    if (!texture_resource) {
        if (flags & (SVGA3D_SURFACE_STAGING_UPLOAD |
                     SVGA3D_SURFACE_STAGING_DOWNLOAD)) {
            usage = D3D10_USAGE_STAGING;
        } else if (flags & SVGA3D_SURFACE_HINT_DYNAMIC) {
            usage = D3D10_USAGE_DYNAMIC;
        }
    }

    if (flags & (SVGA3D_SURFACE_BIND_VERTEX_BUFFER |
                 SVGA3D_SURFACE_HINT_VERTEXBUFFER)) {
        bind |= D3D10_BIND_VERTEX_BUFFER;
    }

    if (flags & (SVGA3D_SURFACE_BIND_INDEX_BUFFER |
                 SVGA3D_SURFACE_HINT_INDEXBUFFER)) {
        bind |= D3D10_BIND_INDEX_BUFFER;
    }

    if (flags & SVGA3D_SURFACE_BIND_CONSTANT_BUFFER) {
        bind |= D3D10_BIND_CONSTANT_BUFFER;
    }

    if (flags & (SVGA3D_SURFACE_BIND_SHADER_RESOURCE |
                 SVGA3D_SURFACE_SCREENTARGET)) {
        bind |= D3D10_BIND_SHADER_RESOURCE;
    }

    if (flags & SVGA3D_SURFACE_BIND_RENDER_TARGET) {
        bind |= D3D10_BIND_RENDER_TARGET;
    }

    if (flags & SVGA3D_SURFACE_BIND_DEPTH_STENCIL) {
        bind |= D3D10_BIND_DEPTH_STENCIL;
    }

    if (flags & SVGA3D_SURFACE_BIND_STREAM_OUTPUT) {
        bind |= D3D10_BIND_STREAM_OUTPUT;
    }

    if (usage == D3D10_USAGE_STAGING) {
        cpu = D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE;
    } else if (usage == D3D10_USAGE_DYNAMIC) {
        cpu = D3D10_CPU_ACCESS_WRITE;
    }

    if (texture_resource && (flags & SVGA3D_SURFACE_CUBEMAP)) {
        misc |= D3D10_RESOURCE_MISC_TEXTURECUBE;
    }

    /* These protocol extensions require D3D11 or later resource flags. */
    if (flags & (SVGA3D_SURFACE_BIND_UAVIEW |
                 SVGA3D_SURFACE_BIND_RAW_VIEWS |
                 SVGA3D_SURFACE_BUFFER_STRUCTURED |
                 SVGA3D_SURFACE_DRAWINDIRECT_ARGS |
                 SVGA3D_SURFACE_RESOURCE_CLAMP)) {
        level = max_level(level, VMSVGA3D_D3D10_LEVEL_11_0);
    }

    if (flags & (SVGA3D_SURFACE_BIND_LOGICOPS | SVGA3D_SURFACE_RESERVED1)) {
        level = max_level(level, VMSVGA3D_D3D10_LEVEL_11_1);
    }

    policy->usage = usage;
    policy->bind_flags = bind;
    policy->cpu_access_flags = cpu;
    policy->misc_flags = misc;

    return level;
}

static uint32_t d3d10_dynamic_resource_format(uint32_t typeless_format)
{
    if (typeless_format == DXGI_R24G8_TYPELESS) {
        return DXGI_R32_UINT;
    }

    if (typeless_format == DXGI_R32G8X24_TYPELESS) {
        return DXGI_R32G32_UINT;
    }

    return typeless_format;
}

static void d3d10_buffer_desc(VMSVGA3DD3D10CreateDesc *desc,
                              uint32_t byte_width, uint32_t usage,
                              uint32_t bind_flags, uint32_t cpu_access_flags,
                              uint32_t initial_subresource_count)
{
    memset(desc, 0, sizeof(*desc));

    desc->valid = true;
    desc->resource_dimension = D3D10_RESOURCE_DIMENSION_BUFFER;
    desc->byte_width = byte_width;
    desc->format = DXGI_UNKNOWN;
    desc->usage = usage;
    desc->bind_flags = bind_flags;
    desc->cpu_access_flags = cpu_access_flags;
    desc->initial_subresource_count = initial_subresource_count;
}

static void d3d10_texture_desc(VMSVGA3DD3D10CreateDesc *desc,
                               const VMSVGA3DD3D10SurfaceInfo *surface,
                               uint32_t resource_dimension, uint32_t format,
                               const VMSVGA3DD3D10ResourcePolicy *policy)
{
    memset(desc, 0, sizeof(*desc));

    desc->valid = true;
    desc->resource_dimension = resource_dimension;
    desc->width = surface->size.width;
    desc->height = surface->size.height;
    desc->depth = surface->size.depth;
    desc->mip_levels = surface->mip_levels;
    desc->array_size = surface->array_elements;
    desc->format = format;
    desc->usage = D3D10_USAGE_DEFAULT;
    desc->bind_flags = policy->bind_flags;
    desc->misc_flags = policy->misc_flags;
    desc->initial_subresource_count = surface->has_initial_data
                                          ? surface->mip_levels *
                                                surface->array_elements
                                          : 0;

    if (resource_dimension == D3D10_RESOURCE_DIMENSION_TEXTURE1D) {
        desc->height = 0;
        desc->depth = 0;
    } else if (resource_dimension == D3D10_RESOURCE_DIMENSION_TEXTURE2D) {
        desc->depth = 0;
        desc->sample_count = MAX(surface->multisample_count, 1u);
        desc->sample_quality = 0;
        if (desc->sample_count > 1) {
            /* D3D10/11 multisampled textures have one mip level and cannot be
             * initialized from guest backing at creation time.  VBox likewise
             * skips guest-backed transfers for multisampled surfaces. */
            desc->initial_subresource_count = 0;
        }
    } else {
        desc->array_size = 0;
    }
}

static void d3d10_texture_companions(
    const VMSVGA3DD3D10SurfaceInfo *surface, VMSVGA3DD3D10ResourcePlan *plan)
{
    plan->dynamic = plan->primary;
    plan->dynamic.format = plan->dynamic_format;
    plan->dynamic.mip_levels = 1;
    if (plan->dynamic.resource_dimension != D3D10_RESOURCE_DIMENSION_TEXTURE3D) {
        plan->dynamic.array_size = 1;
    }
    plan->dynamic.usage = D3D10_USAGE_DYNAMIC;
    plan->dynamic.bind_flags = D3D10_BIND_SHADER_RESOURCE;
    plan->dynamic.cpu_access_flags = D3D10_CPU_ACCESS_WRITE;
    plan->dynamic.misc_flags = 0;
    plan->dynamic.initial_subresource_count = surface->has_initial_data ? 1 : 0;
    plan->has_dynamic = true;

    plan->staging = plan->dynamic;
    plan->staging.format = plan->staging_format;
    plan->staging.usage = D3D10_USAGE_STAGING;
    plan->staging.bind_flags = 0;
    plan->staging.cpu_access_flags =
        D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE;
    plan->has_staging = true;
}

static bool d3d10_buffer_use_valid(SVGA3dSurfaceAllFlags flags,
                                   VMSVGA3DD3D10ResourceUse use)
{
    if (use == VMSVGA3D_D3D10_RESOURCE_USE_BUFFER) {
        return !!(flags & (SVGA3D_SURFACE_HINT_INDEXBUFFER |
                           SVGA3D_SURFACE_HINT_VERTEXBUFFER |
                           SVGA3D_SURFACE_BIND_VERTEX_BUFFER |
                           SVGA3D_SURFACE_BIND_INDEX_BUFFER));
    }

    if (use == VMSVGA3D_D3D10_RESOURCE_USE_STREAM_OUTPUT_BUFFER) {
        return !!(flags & SVGA3D_SURFACE_BIND_STREAM_OUTPUT);
    }

    return true;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_resource_plan(
    const VMSVGA3DD3D10SurfaceInfo *surface, VMSVGA3DD3D10ResourceUse use,
    VMSVGA3DD3D10ResourcePlan *plan)
{
    VMSVGA3DD3D10ResourcePolicy policy;
    VMSVGA3DD3D10Format translated;
    VMSVGA3DD3D10Level level;
    uint32_t dimension;

    if (!surface || !plan || surface->mip_levels == 0 ||
        surface->array_elements == 0) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));
    plan->use = use;

    if (use != VMSVGA3D_D3D10_RESOURCE_USE_TEXTURE) {
        uint32_t usage;
        uint32_t cpu_access;
        uint32_t initial_count;

        if (use != VMSVGA3D_D3D10_RESOURCE_USE_BUFFER &&
            use != VMSVGA3D_D3D10_RESOURCE_USE_STREAM_OUTPUT_BUFFER &&
            use != VMSVGA3D_D3D10_RESOURCE_USE_GENERIC_BUFFER) {
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }

        if (!d3d10_buffer_use_valid(surface->surface_flags, use)) {
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }

        if (use == VMSVGA3D_D3D10_RESOURCE_USE_GENERIC_BUFFER &&
            surface->format != SVGA3D_BUFFER) {
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }

        level = vmsvga3d_d3d10_resource_policy(surface->surface_flags, false,
                                                &policy);
        if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
            return level;
        }

        usage = D3D10_USAGE_DEFAULT;
        cpu_access = 0;
        initial_count = surface->has_initial_data ? 1 : 0;

        if (use == VMSVGA3D_D3D10_RESOURCE_USE_GENERIC_BUFFER) {
            usage = policy.usage;
            cpu_access = policy.cpu_access_flags;
        } else if (use == VMSVGA3D_D3D10_RESOURCE_USE_STREAM_OUTPUT_BUFFER) {
            initial_count = 0;
        }

        d3d10_buffer_desc(&plan->primary, surface->surface_bytes, usage,
                          policy.bind_flags, cpu_access, initial_count);

        /* Buffers use the common staging-buffer path, not per-buffer companions. */
        plan->uses_common_staging_buffer = true;
        return level;
    }

    translated = vmsvga3d_d3d10_surface_format(surface->format);
    if (translated.min_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_resource_policy(surface->surface_flags, true,
                                            &policy);
    level = max_level(level, translated.min_level);

    if (surface->mip_levels > 1 &&
        (policy.bind_flags &
         (D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET)) ==
            (D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET)) {
        policy.misc_flags |= D3D10_RESOURCE_MISC_GENERATE_MIPS;
    }

    plan->requested_format = translated.dxgi_format;
    plan->resource_format = vmsvga3d_d3d10_resource_format(
        surface->format, surface->surface_flags);
    plan->staging_format = vmsvga3d_d3d10_typeless_format(
        plan->requested_format);
    plan->dynamic_format = d3d10_dynamic_resource_format(plan->staging_format);

    if (surface->surface_flags & SVGA3D_SURFACE_CUBEMAP) {
        dimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
        if (surface->array_elements > 6) {
            level = max_level(level, VMSVGA3D_D3D10_LEVEL_10_1);
        }
    } else if (surface->surface_flags & SVGA3D_SURFACE_1D) {
        dimension = D3D10_RESOURCE_DIMENSION_TEXTURE1D;
    } else if (surface->surface_flags & SVGA3D_SURFACE_VOLUME) {
        dimension = D3D10_RESOURCE_DIMENSION_TEXTURE3D;
    } else {
        dimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    }

    if (surface->multisample_count > 1 &&
        (dimension != D3D10_RESOURCE_DIMENSION_TEXTURE2D ||
         surface->mip_levels != 1 ||
         (surface->surface_flags & (SVGA3D_SURFACE_CUBEMAP |
                                    SVGA3D_SURFACE_1D |
                                    SVGA3D_SURFACE_VOLUME)) != 0)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    d3d10_texture_desc(&plan->primary, surface, dimension,
                       plan->resource_format, &policy);

    if (surface->multisample_count <= 1) {
        d3d10_texture_companions(surface, plan);
    }

    return level;
}

static uint32_t blend_color(uint8_t value)
{
    switch (value) {
    case SVGA3D_BLENDOP_ZERO:
        return D3D10_BLEND_ZERO;
    case SVGA3D_BLENDOP_ONE:
        return D3D10_BLEND_ONE;
    case SVGA3D_BLENDOP_SRCCOLOR:
        return D3D10_BLEND_SRC_COLOR;
    case SVGA3D_BLENDOP_INVSRCCOLOR:
        return D3D10_BLEND_INV_SRC_COLOR;
    case SVGA3D_BLENDOP_SRCALPHA:
        return D3D10_BLEND_SRC_ALPHA;
    case SVGA3D_BLENDOP_INVSRCALPHA:
        return D3D10_BLEND_INV_SRC_ALPHA;
    case SVGA3D_BLENDOP_DESTALPHA:
        return D3D10_BLEND_DEST_ALPHA;
    case SVGA3D_BLENDOP_INVDESTALPHA:
        return D3D10_BLEND_INV_DEST_ALPHA;
    case SVGA3D_BLENDOP_DESTCOLOR:
        return D3D10_BLEND_DEST_COLOR;
    case SVGA3D_BLENDOP_INVDESTCOLOR:
        return D3D10_BLEND_INV_DEST_COLOR;
    case SVGA3D_BLENDOP_SRCALPHASAT:
        return D3D10_BLEND_SRC_ALPHA_SAT;
    case SVGA3D_BLENDOP_BLENDFACTOR:
        return D3D10_BLEND_BLEND_FACTOR;
    case SVGA3D_BLENDOP_INVBLENDFACTOR:
        return D3D10_BLEND_INV_BLEND_FACTOR;
    case SVGA3D_BLENDOP_SRC1COLOR:
        return D3D10_BLEND_SRC1_COLOR;
    case SVGA3D_BLENDOP_INVSRC1COLOR:
        return D3D10_BLEND_INV_SRC1_COLOR;
    case SVGA3D_BLENDOP_SRC1ALPHA:
        return D3D10_BLEND_SRC1_ALPHA;
    case SVGA3D_BLENDOP_INVSRC1ALPHA:
        return D3D10_BLEND_INV_SRC1_ALPHA;
    case SVGA3D_BLENDOP_BLENDFACTORALPHA:
        return D3D10_BLEND_BLEND_FACTOR;
    case SVGA3D_BLENDOP_INVBLENDFACTORALPHA:
        return D3D10_BLEND_INV_BLEND_FACTOR;
    default:
        return D3D10_BLEND_ZERO;
    }
}

static uint32_t blend_alpha(uint8_t value)
{
    switch (value) {
    case SVGA3D_BLENDOP_ZERO:
        return D3D10_BLEND_ZERO;
    case SVGA3D_BLENDOP_ONE:
        return D3D10_BLEND_ONE;
    case SVGA3D_BLENDOP_SRCCOLOR:
    case SVGA3D_BLENDOP_SRCALPHA:
        return D3D10_BLEND_SRC_ALPHA;
    case SVGA3D_BLENDOP_INVSRCCOLOR:
    case SVGA3D_BLENDOP_INVSRCALPHA:
        return D3D10_BLEND_INV_SRC_ALPHA;
    case SVGA3D_BLENDOP_DESTCOLOR:
    case SVGA3D_BLENDOP_DESTALPHA:
        return D3D10_BLEND_DEST_ALPHA;
    case SVGA3D_BLENDOP_INVDESTCOLOR:
    case SVGA3D_BLENDOP_INVDESTALPHA:
        return D3D10_BLEND_INV_DEST_ALPHA;
    case SVGA3D_BLENDOP_SRCALPHASAT:
        return D3D10_BLEND_SRC_ALPHA_SAT;
    case SVGA3D_BLENDOP_BLENDFACTOR:
        return D3D10_BLEND_BLEND_FACTOR;
    case SVGA3D_BLENDOP_INVBLENDFACTOR:
        return D3D10_BLEND_INV_BLEND_FACTOR;
    case SVGA3D_BLENDOP_SRC1COLOR:
    case SVGA3D_BLENDOP_SRC1ALPHA:
        return D3D10_BLEND_SRC1_ALPHA;
    case SVGA3D_BLENDOP_INVSRC1COLOR:
    case SVGA3D_BLENDOP_INVSRC1ALPHA:
        return D3D10_BLEND_INV_SRC1_ALPHA;
    case SVGA3D_BLENDOP_BLENDFACTORALPHA:
        return D3D10_BLEND_BLEND_FACTOR;
    case SVGA3D_BLENDOP_INVBLENDFACTORALPHA:
        return D3D10_BLEND_INV_BLEND_FACTOR;
    default:
        return D3D10_BLEND_ZERO;
    }
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_index_format(
    SVGA3dSurfaceFormat format, uint32_t *dxgi_format,
    uint32_t *bytes_per_index)
{
    VMSVGA3DD3D10Format translated;

    if (!dxgi_format || !bytes_per_index) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    translated = vmsvga3d_d3d10_surface_format(format);
    if (translated.min_level == VMSVGA3D_D3D10_LEVEL_INVALID ||
        (translated.dxgi_format != DXGI_R16_UINT &&
         translated.dxgi_format != DXGI_R32_UINT)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    *dxgi_format = translated.dxgi_format;
    *bytes_per_index = translated.dxgi_format == DXGI_R16_UINT ? 2u : 4u;

    return translated.min_level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_box(
    const SVGA3dBox *src, VMSVGA3DD3D10Box *dst)
{
    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    dst->left = src->x;
    dst->top = src->y;
    dst->front = src->z;
    dst->right = src->x + src->w;
    dst->bottom = src->y + src->h;
    dst->back = src->z + src->d;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

static void copy_box_clip(const SVGA3dSize *src_size,
                          const SVGA3dSize *dst_size,
                          SVGA3dCopyBox *box)
{
    /* Keep the established clipping order and strict > comparisons. */
    if (box->srcx > src_size->width) {
        box->srcx = src_size->width;
    }

    if (box->w > src_size->width - box->srcx) {
        box->w = src_size->width - box->srcx;
    }

    if (box->srcy > src_size->height) {
        box->srcy = src_size->height;
    }

    if (box->h > src_size->height - box->srcy) {
        box->h = src_size->height - box->srcy;
    }

    if (box->srcz > src_size->depth) {
        box->srcz = src_size->depth;
    }

    if (box->d > src_size->depth - box->srcz) {
        box->d = src_size->depth - box->srcz;
    }

    if (box->x > dst_size->width) {
        box->x = dst_size->width;
    }

    if (box->w > dst_size->width - box->x) {
        box->w = dst_size->width - box->x;
    }

    if (box->y > dst_size->height) {
        box->y = dst_size->height;
    }

    if (box->h > dst_size->height - box->y) {
        box->h = dst_size->height - box->y;
    }

    if (box->z > dst_size->depth) {
        box->z = dst_size->depth;
    }

    if (box->d > dst_size->depth - box->z) {
        box->d = dst_size->depth - box->z;
    }
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_copy_region_plan(
    const SVGA3dSize *src_size, const SVGA3dSize *dst_size,
    const SVGA3dCopyBox *box, VMSVGA3DD3D10CopyRegionPlan *plan)
{
    if (!src_size || !dst_size || !box || !plan) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    plan->clipped_box = *box;
    copy_box_clip(src_size, dst_size, &plan->clipped_box);

    plan->destination_x = plan->clipped_box.x;
    plan->destination_y = plan->clipped_box.y;
    plan->destination_z = plan->clipped_box.z;
    plan->source_box.left = plan->clipped_box.srcx;
    plan->source_box.top = plan->clipped_box.srcy;
    plan->source_box.front = plan->clipped_box.srcz;
    plan->source_box.right = plan->clipped_box.srcx + plan->clipped_box.w;
    plan->source_box.bottom = plan->clipped_box.srcy + plan->clipped_box.h;
    plan->source_box.back = plan->clipped_box.srcz + plan->clipped_box.d;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_so_targets_plan(
    uint32_t count, const SVGA3dSoTarget *targets,
    VMSVGA3DD3D10SOTargetsPlan *plan)
{
    uint32_t i;

    if (!plan || count > SVGA3D_DX_MAX_SOTARGETS || (count && !targets)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->guest_count = count;
    plan->native_target_count = SVGA3D_DX_MAX_SOTARGETS;
    plan->backend_remembered_count = count;
    plan->shadow_update = true;
    plan->shadow_update_before_backend = true;
    plan->shadow_stores_only_surface_ids = true;
    plan->shadow_clears_unspecified_slots = true;
    plan->prepare_targets_sequentially = true;
    plan->size_in_bytes_ignored = true;
    plan->bind_full_native_table = true;
    plan->immediate_bind = true;

    for (i = 0; i < SVGA3D_DX_MAX_SOTARGETS; i++) {
        VMSVGA3DD3D10SOTargetBinding *binding = &plan->bindings[i];

        if (i < count) {
            plan->shadow_targets[i] = targets[i].sid;
            binding->sid = targets[i].sid;
            binding->size_in_bytes = targets[i].sizeInBytes;
            if (targets[i].sid != SVGA_ID_INVALID) {
                binding->active = true;
                binding->ensure_stream_output_buffer = true;
                binding->offset = targets[i].offset;
            }
        } else {
            plan->shadow_targets[i] = SVGA3D_INVALID_ID;
            binding->sid = SVGA3D_INVALID_ID;
        }
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_so_targets_restore_plan(
    const SVGA3dSurfaceId targets[SVGA3D_DX_MAX_SOTARGETS],
    VMSVGA3DD3D10SOTargetsPlan *plan)
{
    SVGA3dSoTarget restore_targets[SVGA3D_DX_MAX_SOTARGETS];
    uint32_t i;

    if (!targets || !plan) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(restore_targets, 0, sizeof(restore_targets));

    for (i = 0; i < SVGA3D_DX_MAX_SOTARGETS; i++) {
        restore_targets[i].sid = targets[i];
    }

    if (vmsvga3d_d3d10_so_targets_plan(
            SVGA3D_DX_MAX_SOTARGETS, restore_targets, plan) ==
        VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    plan->shadow_update = false;
    plan->shadow_update_before_backend = false;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_set_plan(
    SVGA3dStreamOutputId stream_output_id, uint32_t table_count,
    VMSVGA3DD3D10StreamOutputSetPlan *plan)
{
    if (!plan || (stream_output_id != SVGA_ID_INVALID &&
                  stream_output_id >= table_count)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->stream_output_id = stream_output_id;
    plan->shadow_update = true;
    plan->backend_set_is_noop = true;
    plan->affects_geometry_shader_creation = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_destroy_entry(
    SVGACOTableDXStreamOutputEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));

    entry->mobid = SVGA3D_INVALID_ID;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_rtv_destroy_shadow_refs(
    SVGA3dRenderTargetViewId destroyed_id,
    SVGA3dRenderTargetViewId ids[SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS])
{
    uint32_t i;

    if (!ids) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    for (i = 0; i < SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS; i++) {
        if (ids[i] == destroyed_id) {
            ids[i] = SVGA3D_INVALID_ID;
        }
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_srv_define_entry(
    const SVGA3dCmdDXDefineShaderResourceView *src,
    SVGACOTableDXSRViewEntry *dst)
{
    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    dst->sid = src->sid;
    dst->format = src->format;
    dst->resourceDimension = src->resourceDimension;
    dst->desc = src->desc;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_srv_destroy_entry(
    SVGACOTableDXSRViewEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_rtv_define_entry(
    const SVGA3dCmdDXDefineRenderTargetView *src,
    SVGACOTableDXRTViewEntry *dst)
{
    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    dst->sid = src->sid;
    dst->format = src->format;
    dst->resourceDimension = src->resourceDimension;
    dst->desc = src->desc;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_rtv_destroy_entry(
    SVGACOTableDXRTViewEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_dsv_define_entry(
    const SVGA3dCmdDXDefineDepthStencilView_v2 *src,
    SVGACOTableDXDSViewEntry *dst)
{
    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    dst->sid = src->sid;
    dst->format = src->format;
    dst->resourceDimension = src->resourceDimension;
    dst->mipSlice = src->mipSlice;
    dst->firstArraySlice = src->firstArraySlice;
    dst->arraySize = src->arraySize;
    dst->flags = src->flags;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_dsv_destroy_entry(
    SVGACOTableDXDSViewEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_element_layout_define_entry(
    SVGA3dElementLayoutId layout_id, uint32_t count,
    const SVGA3dInputElementDesc *descs,
    SVGACOTableDXElementLayoutEntry *entry)
{
    uint32_t copy_count;

    if (!entry || (count && !descs)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    copy_count = count;
    if (copy_count > sizeof(entry->descs) / sizeof(entry->descs[0])) {
        copy_count = sizeof(entry->descs) / sizeof(entry->descs[0]);
    }

    entry->elid = layout_id;
    entry->numDescs = copy_count;

    if (copy_count) {
        memcpy(entry->descs, descs, copy_count * sizeof(entry->descs[0]));
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_element_layout_destroy_entry(
    SVGACOTableDXElementLayoutEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));
    entry->elid = SVGA3D_INVALID_ID;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_blend_define_entry(
    const SVGA3dCmdDXDefineBlendState *src,
    SVGACOTableDXBlendStateEntry *entry)
{
    if (!src || !entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    entry->alphaToCoverageEnable = src->alphaToCoverageEnable;
    entry->independentBlendEnable = src->independentBlendEnable;

    memcpy(entry->perRT, src->perRT, sizeof(entry->perRT));

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_blend_destroy_entry(
    SVGACOTableDXBlendStateEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_depth_stencil_define_entry(
    const SVGA3dCmdDXDefineDepthStencilState *src,
    SVGACOTableDXDepthStencilEntry *entry)
{
    if (!src || !entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    entry->depthEnable = src->depthEnable;
    entry->depthWriteMask = src->depthWriteMask;
    entry->depthFunc = src->depthFunc;
    entry->stencilEnable = src->stencilEnable;
    entry->frontEnable = src->frontEnable;
    entry->backEnable = src->backEnable;
    entry->stencilReadMask = src->stencilReadMask;
    entry->stencilWriteMask = src->stencilWriteMask;
    entry->frontStencilFailOp = src->frontStencilFailOp;
    entry->frontStencilDepthFailOp = src->frontStencilDepthFailOp;
    entry->frontStencilPassOp = src->frontStencilPassOp;
    entry->frontStencilFunc = src->frontStencilFunc;
    entry->backStencilFailOp = src->backStencilFailOp;
    entry->backStencilDepthFailOp = src->backStencilDepthFailOp;
    entry->backStencilPassOp = src->backStencilPassOp;
    entry->backStencilFunc = src->backStencilFunc;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_depth_stencil_destroy_entry(
    SVGACOTableDXDepthStencilEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_rasterizer_define_entry(
    const SVGA3dCmdDXDefineRasterizerState *src,
    SVGACOTableDXRasterizerStateEntry *entry)
{
    if (!src || !entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
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
    entry->forcedSampleCount = 0;

    memset(entry->mustBeZero, 0, sizeof(entry->mustBeZero));

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_rasterizer_destroy_entry(
    SVGACOTableDXRasterizerStateEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_sampler_define_entry(
    const SVGA3dCmdDXDefineSamplerState *src,
    SVGACOTableDXSamplerEntry *entry)
{
    if (!src || !entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    entry->filter = src->filter;
    entry->addressU = src->addressU;
    entry->addressV = src->addressV;
    entry->addressW = src->addressW;
    entry->mipLODBias = src->mipLODBias;
    entry->maxAnisotropy = src->maxAnisotropy;
    entry->comparisonFunc = src->comparisonFunc;
    entry->borderColor = src->borderColor;
    entry->minLOD = src->minLOD;
    entry->maxLOD = src->maxLOD;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_sampler_destroy_entry(
    SVGACOTableDXSamplerEntry *entry)
{
    if (!entry) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(entry, 0, sizeof(*entry));

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

static VMSVGA3DD3D10Level vgpu10_shader_stage(
    SVGA3dShaderType type, uint32_t *stage_index)
{
    if (type < SVGA3D_SHADERTYPE_MIN || type >= SVGA3D_SHADERTYPE_DX10_MAX) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    return vmsvga3d_d3d10_shader_stage(type, stage_index);
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_constant_buffer_plan(
    uint32_t slot, SVGA3dShaderType type, SVGA3dSurfaceId sid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes, bool surface_available,
    uint32_t surface_bytes, bool has_surface_data,
    VMSVGA3DD3D10ConstantBufferPlan *plan)
{
    VMSVGA3DD3D10Level stage_level;
    uint32_t stage_index;
    uint32_t aligned_size;

    if (!plan || slot >= SVGA3D_DX_MAX_CONSTBUFFERS) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    stage_level = vgpu10_shader_stage(type, &stage_index);
    if (stage_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->stage_index = stage_index;
    plan->slot = slot;
    plan->sid = sid;
    plan->offset_in_bytes = offset_in_bytes;
    plan->size_in_bytes = size_in_bytes;
    plan->shadow_update = true;
    plan->bind_only_if_pipeline_differs = false;
    plan->bind_timing = VMSVGA3D_D3D10_BIND_DRAW_SETUP;

    if (sid == SVGA_ID_INVALID) {
        plan->unbind = true;
        return stage_level;
    }

    /* VirtualBox snapshots constant-buffer contents during SET.  It rounds the
     * backend range to 16 constants (256 bytes), clamps it to 4096 constants,
     * and then clips the guest copy to that backend range.  Preserve uint32_t
     * wrap semantics of RT_ALIGN for pathological guest sizes.
     */
    aligned_size = (size_in_bytes + 255u) & ~255u;
    if (aligned_size > 4096u * 16u) {
        aligned_size = 4096u * 16u;
    }

    plan->backend_buffer_size = aligned_size;
    plan->backend_copy_size = MIN(size_in_bytes, aligned_size);

    if (!surface_available || !has_surface_data ||
        offset_in_bytes >= surface_bytes ||
        plan->backend_copy_size > surface_bytes - offset_in_bytes) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    plan->create_buffer = aligned_size != 0;
    plan->has_initial_data = true;
    plan->initial_data_offset = offset_in_bytes;
    plan->replace_only_on_create_success = true;
    plan->preserve_old_buffer_on_create_failure = true;
    plan->create_failure_is_success = false;
    plan->create_desc.valid = aligned_size != 0;
    plan->create_desc.resource_dimension = D3D10_RESOURCE_DIMENSION_BUFFER;
    plan->create_desc.byte_width = aligned_size;
    plan->create_desc.usage = D3D10_USAGE_DEFAULT;
    plan->create_desc.bind_flags = D3D10_BIND_CONSTANT_BUFFER;
    plan->create_desc.initial_subresource_count = aligned_size != 0 ? 1u : 0u;

    return stage_level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_resources_set_plan(
    uint32_t start_view, SVGA3dShaderType type, uint32_t count,
    const SVGA3dShaderResourceViewId *ids, uint32_t view_table_count,
    VMSVGA3DD3D10ShaderResourceSetPlan *plan)
{
    VMSVGA3DD3D10Level stage_level;
    uint32_t stage_index;
    uint32_t i;

    if (!plan || start_view >= SVGA3D_DX_MAX_SRVIEWS ||
        count > SVGA3D_DX_MAX_SRVIEWS - start_view || (count && !ids)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    stage_level = vgpu10_shader_stage(type, &stage_index);
    if (stage_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->stage_index = stage_index;
    plan->start_view = start_view;
    plan->count = count;
    plan->shadow_update_atomic = true;
    plan->ensure_views_at_draw = true;
    plan->bind_full_table_at_draw = false;
    plan->bind_every_draw = false;
    plan->bind_timing = VMSVGA3D_D3D10_BIND_DRAW_SETUP;

    for (i = 0; i < count; i++) {
        if (ids[i] >= view_table_count && ids[i] != SVGA3D_INVALID_ID) {
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }
    }

    for (i = 0; i < count; i++) {
        plan->ids[i] = ids[i];
    }

    plan->shadow_update_count = count;

    return stage_level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_set_plan(
    SVGA3dShaderId shader_id, SVGA3dShaderType type,
    uint32_t shader_table_count, VMSVGA3DD3D10ShaderSetPlan *plan)
{
    VMSVGA3DD3D10Level stage_level;
    uint32_t stage_index;

    if (!plan ||
        (shader_id >= shader_table_count && shader_id != SVGA_ID_INVALID)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    stage_level = vgpu10_shader_stage(type, &stage_index);
    if (stage_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->stage_index = stage_index;
    plan->shader_id = shader_id;
    plan->shadow_update = true;
    plan->unbind = shader_id == SVGA_ID_INVALID;
    plan->create_if_missing_at_draw = !plan->unbind;
    plan->patch_pixel_resources_on_create =
        !plan->unbind && type == SVGA3D_SHADERTYPE_PS;
    plan->update_vs_input_signature_on_create =
        !plan->unbind && type == SVGA3D_SHADERTYPE_VS;
    plan->match_signatures_on_create = !plan->unbind;
    plan->stream_output_affects_geometry_creation =
        !plan->unbind && type == SVGA3D_SHADERTYPE_GS;
    plan->bind_every_draw = true;
    plan->bind_timing = VMSVGA3D_D3D10_BIND_DRAW_SETUP;

    return stage_level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_samplers_set_plan(
    uint32_t start_sampler, SVGA3dShaderType type, uint32_t count,
    const SVGA3dSamplerId *ids, uint32_t sampler_table_count,
    VMSVGA3DD3D10SamplerSetPlan *plan)
{
    VMSVGA3DD3D10Level stage_level;
    uint32_t stage_index;
    uint32_t i;

    if (!plan || start_sampler >= SVGA3D_DX_MAX_SAMPLERS ||
        count > SVGA3D_DX_MAX_SAMPLERS - start_sampler || (count && !ids)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    stage_level = vgpu10_shader_stage(type, &stage_index);
    if (stage_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->stage_index = stage_index;
    plan->start_sampler = start_sampler;
    plan->count = count;
    plan->partial_shadow_update_on_failure = true;
    plan->bind_timing = VMSVGA3D_D3D10_BIND_IMMEDIATE;

    for (i = 0; i < count; i++) {
        if (ids[i] >= sampler_table_count && ids[i] != SVGA3D_INVALID_ID) {
            plan->shadow_update_count = i;
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }
        plan->ids[i] = ids[i];
        plan->shadow_update_count = i + 1u;
    }

    return stage_level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_input_layout_set_plan(
    SVGA3dElementLayoutId layout_id, uint32_t layout_table_count,
    VMSVGA3DD3D10InputLayoutSetPlan *plan)
{
    if (!plan || (layout_id >= layout_table_count &&
                  layout_id != SVGA3D_INVALID_ID)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->layout_id = layout_id;
    plan->shadow_update = true;
    plan->unbind = layout_id == SVGA3D_INVALID_ID;
    plan->create_lazily_at_draw = !plan->unbind;
    plan->requires_vertex_shader_dxbc = !plan->unbind;
    plan->bind_every_draw = true;
    plan->bind_timing = VMSVGA3D_D3D10_BIND_DRAW_SETUP;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_vertex_buffers_set_plan(
    uint32_t start_buffer, uint32_t count, const SVGA3dVertexBuffer *buffers,
    VMSVGA3DD3D10VertexBufferSetPlan *plan)
{
    uint32_t i;

    if (!plan || start_buffer >= SVGA3D_DX_MAX_VERTEXBUFFERS ||
        count > SVGA3D_DX_MAX_VERTEXBUFFERS - start_buffer ||
        (count && !buffers)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->start_buffer = start_buffer;
    plan->count = count;
    plan->shadow_update_count = count;
    plan->prepare_backend_on_set = false;
    plan->backend_updates_sequentially = false;
    plan->bind_from_slot_zero_at_draw = false;
    plan->bind_only_if_pipeline_differs = false;
    plan->bind_timing = VMSVGA3D_D3D10_BIND_DRAW_SETUP;

    for (i = 0; i < count; i++) {
        plan->bindings[i].sid = buffers[i].sid;
        plan->bindings[i].stride = buffers[i].stride;
        plan->bindings[i].offset = buffers[i].offset;
        plan->bindings[i].unbind = buffers[i].sid == SVGA_ID_INVALID;
        plan->bindings[i].ensure_buffer_on_set = false;
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_vertex_buffer_pipeline_binding(
    bool has_buffer, uint32_t surface_bytes, uint32_t stride, uint32_t offset,
    VMSVGA3DD3D10VertexBufferPipelineBinding *binding)
{
    uint32_t pipeline_stride;

    if (!binding) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(binding, 0, sizeof(*binding));

    if (!has_buffer) {
        return VMSVGA3D_D3D10_LEVEL_10_0;
    }

    /* Ignore the large strides emitted by some guests. */
    pipeline_stride = stride <= 2048u ? stride : 0u;
    if (pipeline_stride <= surface_bytes &&
        offset <= surface_bytes - pipeline_stride) {
        binding->stride = pipeline_stride;
        binding->offset = offset;
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_index_buffer_set_plan(
    SVGA3dSurfaceId sid, SVGA3dSurfaceFormat format, uint32_t offset,
    VMSVGA3DD3D10IndexBufferSetPlan *plan)
{
    if (!plan) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->sid = sid;
    plan->format = format;
    plan->offset = offset;
    plan->backend_offset = sid == SVGA_ID_INVALID ? 0u : offset;
    plan->shadow_update = true;
    plan->unbind = sid == SVGA_ID_INVALID;
    plan->bind_timing = VMSVGA3D_D3D10_BIND_DRAW_SETUP;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_viewports_set_plan(
    uint32_t count, const SVGA3dViewport *viewports,
    VMSVGA3DD3D10ViewportsSetPlan *plan)
{
    VMSVGA3DD3D10Level level;

    if (!plan) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    level = vmsvga3d_d3d10_viewports(viewports, count, plan->viewports);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    plan->count = count;
    plan->shadow_update = true;
    plan->preserve_unspecified_slots = true;
    plan->immediate_bind = true;

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_topology_set_plan(
    SVGA3dPrimitiveType topology, VMSVGA3DD3D10TopologySetPlan *plan)
{
    VMSVGA3DD3D10Level level;

    if (!plan || topology < SVGA3D_PRIMITIVE_MIN ||
        topology >= SVGA3D_PRIMITIVE_MAX) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    level = vmsvga3d_d3d10_primitive_topology(topology,
                                                 &plan->native_topology);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    plan->topology = topology;
    plan->shadow_update = true;
    plan->immediate_bind = true;

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_blend_state_set_plan(
    const SVGA3dCmdDXSetBlendState *command, uint32_t table_count,
    VMSVGA3DD3D10BlendStateSetPlan *plan)
{
    if (!command || !plan ||
        (command->blendId >= table_count &&
         command->blendId != SVGA3D_INVALID_ID)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->blend_id = command->blendId;

    memcpy(plan->blend_factor, command->blendFactor,
           sizeof(plan->blend_factor));

    plan->sample_mask = command->sampleMask;
    plan->shadow_update = true;
    plan->immediate_bind = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_depth_stencil_state_set_plan(
    const SVGA3dCmdDXSetDepthStencilState *command, uint32_t table_count,
    VMSVGA3DD3D10DepthStencilStateSetPlan *plan)
{
    if (!command || !plan ||
        (command->depthStencilId >= table_count &&
         command->depthStencilId != SVGA3D_INVALID_ID)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->depth_stencil_id = command->depthStencilId;
    plan->stencil_ref = command->stencilRef;
    plan->shadow_update = true;
    plan->immediate_bind = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_rasterizer_state_set_plan(
    SVGA3dRasterizerStateId rasterizer_id, uint32_t table_count,
    VMSVGA3DD3D10RasterizerStateSetPlan *plan)
{
    if (!plan ||
        (rasterizer_id >= table_count && rasterizer_id != SVGA3D_INVALID_ID)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->rasterizer_id = rasterizer_id;
    plan->shadow_update = true;
    plan->immediate_bind = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_scissor_plan(
    uint32_t count, const SVGASignedRect *rects,
    VMSVGA3DD3D10ScissorPlan *plan)
{
    if (!plan || count > SVGA3D_DX_MAX_SCISSORRECTS ||
        (count != 0 && !rects)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->count = count;
    if (count != 0) {
        memcpy(plan->rects, rects, count * sizeof(*rects));
    }

    plan->shadow_update = true;
    plan->native_layout_identical = true;
    plan->immediate_bind = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_render_targets_set_plan(
    SVGA3dDepthStencilViewId depth_stencil_view_id, uint32_t count,
    const SVGA3dRenderTargetViewId *ids, uint32_t dsv_table_count,
    uint32_t rtv_table_count, uint32_t previous_remembered_count,
    VMSVGA3DD3D10RenderTargetsSetPlan *plan)
{
    uint32_t i;

    if (!plan || count > SVGA3D_MAX_RENDER_TARGETS ||
        previous_remembered_count > SVGA3D_MAX_RENDER_TARGETS ||
        (count != 0 && !ids) ||
        (depth_stencil_view_id >= dsv_table_count &&
         depth_stencil_view_id != SVGA3D_INVALID_ID)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    for (i = 0; i < count; ++i) {
        if (ids[i] >= rtv_table_count && ids[i] != SVGA3D_INVALID_ID) {
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }
    }

    memset(plan, 0, sizeof(*plan));

    plan->depth_stencil_view_id = depth_stencil_view_id;
    plan->supplied_count = count;
    plan->previous_remembered_count = previous_remembered_count;

    if (count != 0) {
        memcpy(plan->ids, ids, count * sizeof(*ids));
    }

    for (i = 0; i < count; i++) {
        if (ids[i] != SVGA3D_INVALID_ID) {
            plan->remembered_count = i + 1u;
        }
    }

    plan->shadow_update_count = count;
    plan->shadow_update_atomic = true;
    plan->preserve_unspecified_slots = false;
    plan->backend_set_is_noop = true;
    plan->ensure_dsv_at_draw = true;
    plan->ensure_all_rtv_slots_at_draw = true;
    plan->bind_at_draw_setup = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_gen_mips_plan(
    SVGA3dShaderResourceViewId view_id, uint32_t srv_table_count,
    VMSVGA3DD3D10GenMipsPlan *plan)
{
    if (!plan || view_id >= srv_table_count) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->view_id = view_id;
    plan->require_existing_backend_view = false;
    plan->do_not_create_view_on_call = false;
    plan->require_view_entry = true;
    plan->require_surface = true;
    plan->require_backend_surface = false;
    plan->immediate_generate_mips = true;
    plan->mark_surface_drawing_context = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_clear_rtv_plan(
    SVGA3dRenderTargetViewId view_id, const SVGA3dRGBAFloat *rgba,
    uint32_t rtv_table_count, VMSVGA3DD3D10ClearRTVPlan *plan)
{
    if (!rgba || !plan || view_id >= rtv_table_count) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->view_id = view_id;

    memcpy(plan->color, rgba->value, sizeof(plan->color));

    plan->ensure_view_on_clear = true;
    plan->immediate_clear = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_clear_dsv_plan(
    uint32_t flags, SVGA3dDepthStencilViewId view_id, float depth,
    uint32_t stencil, uint32_t dsv_table_count,
    VMSVGA3DD3D10ClearDSVPlan *plan)
{
    if (!plan || view_id >= dsv_table_count) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->view_id = view_id;
    plan->guest_flags = flags;
    plan->depth = depth;
    plan->stencil = (uint8_t)stencil;
    plan->ensure_view_on_clear = true;
    plan->immediate_clear = true;

    if (flags & SVGA3D_CLEAR_DEPTH) {
        plan->d3d_clear_flags |= 0x1u;
    }

    if (flags & SVGA3D_CLEAR_STENCIL) {
        plan->d3d_clear_flags |= 0x2u;
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

static VMSVGA3DD3D10ResourceCreateKind copy_resource_create_kind(
    SVGA3dSurfaceFormat source_format)
{
    return source_format == SVGA3D_BUFFER ? VMSVGA3D_D3D10_CREATE_BUFFER
                                          : VMSVGA3D_D3D10_CREATE_TEXTURE;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_copy_resource_plan(
    SVGA3dSurfaceFormat source_format, SVGA3dSurfaceFormat destination_format,
    bool source_resource_exists, bool destination_resource_exists,
    VMSVGA3DD3D10CopyResourcePlan *plan)
{

    if (!plan) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->ensure_source_resource = !source_resource_exists;
    plan->ensure_destination_resource = !destination_resource_exists;
    plan->source_create_kind = copy_resource_create_kind(source_format);
    plan->destination_create_kind = copy_resource_create_kind(destination_format);
    plan->issue_copy_resource = true;
    plan->mark_destination_drawing_context = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_copy_subresource_plan(
    SVGA3dSurfaceFormat source_format, SVGA3dSurfaceFormat destination_format,
    bool source_resource_exists, bool destination_resource_exists,
    uint32_t destination_subresource,
    uint32_t source_subresource, const SVGA3dSize *source_size,
    const SVGA3dSize *destination_size, const SVGA3dCopyBox *box,
    VMSVGA3DD3D10CopySubresourcePlan *plan)
{
    VMSVGA3DD3D10Level level;

    if (!plan) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->ensure_source_resource = !source_resource_exists;
    plan->ensure_destination_resource = !destination_resource_exists;
    plan->source_create_kind = copy_resource_create_kind(source_format);
    plan->destination_create_kind = copy_resource_create_kind(destination_format);
    plan->destination_subresource = destination_subresource;
    plan->source_subresource = source_subresource;

    level = vmsvga3d_d3d10_copy_region_plan(source_size, destination_size,
                                            box, &plan->region);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    plan->issue_copy_subresource_region = true;
    plan->mark_destination_drawing_context = true;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_triangle_fan_generate_u16(
    bool indexed_source, uint32_t count, uint32_t source_dxgi_format,
    const void *source_indices, uint32_t source_bytes,
    uint16_t *generated_indices, uint32_t generated_capacity,
    uint32_t *generated_count)
{
    uint32_t count_out;
    uint32_t source_bytes_per_index = 0;
    uint32_t i_vertex = 1;
    uint32_t i;

    if (!generated_count || count > 65535u || count < 3u) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    count_out = 3u * (count - 2u);
    *generated_count = count_out;
    if (count_out > generated_capacity || (count_out && !generated_indices)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    if (indexed_source) {
        if (source_dxgi_format == DXGI_R16_UINT) {
            source_bytes_per_index = 2;
        } else if (source_dxgi_format == DXGI_R32_UINT) {
            source_bytes_per_index = 4;
        } else {
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }
        if (!source_indices || source_bytes_per_index * count > source_bytes) {
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }
    }

    for (i = 0; i < count_out; i += 3) {
        uint16_t center;
        uint16_t second;
        uint16_t third;

        if (!indexed_source) {
            center = 0;
            second = (uint16_t)i_vertex;
            third = (uint16_t)(i_vertex + 1u);
        } else if (source_dxgi_format == DXGI_R16_UINT) {
            const uint16_t *src = source_indices;
            center = src[0];
            second = src[i_vertex];
            third = src[i_vertex + 1u];
        } else {
            const uint32_t *src = source_indices;
            center = (uint16_t)src[0];
            second = (uint16_t)src[i_vertex];
            third = (uint16_t)src[i_vertex + 1u];
        }

        generated_indices[i] = center;
        generated_indices[i + 1u] = second;
        generated_indices[i + 2u] = third;
        i_vertex++;
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_input_element(
    const SVGA3dInputElementDesc *src, VMSVGA3DD3D10InputElement *dst)
{
    VMSVGA3DD3D10Format format;

    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    format = vmsvga3d_d3d10_surface_format(src->format);
    if (format.min_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return format.min_level;
    }

    dst->semantic_index = src->inputRegister;
    dst->format = format.dxgi_format;
    dst->input_slot = src->inputSlot;
    dst->aligned_byte_offset = src->alignedByteOffset;
    dst->input_slot_class = src->inputSlotClass;
    dst->instance_data_step_rate = src->instanceDataStepRate;

    return format.min_level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_blend_state(
    const SVGACOTableDXBlendStateEntry *src, VMSVGA3DD3D10BlendDesc *dst)
{
    VMSVGA3DD3D10Level level = VMSVGA3D_D3D10_LEVEL_10_0;
    uint32_t i;

    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    dst->alpha_to_coverage_enable = !!src->alphaToCoverageEnable;
    dst->independent_blend_enable = !!src->independentBlendEnable;

    if (dst->independent_blend_enable) {
        level = VMSVGA3D_D3D10_LEVEL_10_1;
    }

    for (i = 0; i < SVGA3D_DX_MAX_RENDER_TARGETS; ++i) {
        const SVGA3dDXBlendStatePerRT *s = &src->perRT[i];
        VMSVGA3DD3D10RTBlend *d = &dst->render_target[i];

        d->blend_enable = !!s->blendEnable;
        d->src_blend = blend_color(s->srcBlend);
        d->dest_blend = blend_color(s->destBlend);
        d->blend_op = s->blendOp;
        d->src_blend_alpha = blend_alpha(s->srcBlendAlpha);
        d->dest_blend_alpha = blend_alpha(s->destBlendAlpha);
        d->blend_op_alpha = s->blendOpAlpha;
        d->write_mask = s->renderTargetWriteMask;

        if (s->logicOpEnable) {
            level = max_level(level, VMSVGA3D_D3D10_LEVEL_11_1);
        }
    }

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_depth_stencil_state(
    const SVGACOTableDXDepthStencilEntry *src,
    VMSVGA3DD3D10DepthStencilDesc *dst)
{
    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    dst->depth_enable = src->depthEnable;
    dst->depth_write_mask = src->depthWriteMask;
    dst->depth_func = src->depthFunc;
    dst->stencil_enable = src->stencilEnable;
    dst->stencil_read_mask = src->stencilReadMask;
    dst->stencil_write_mask = src->stencilWriteMask;
    dst->front_face.fail_op = src->frontStencilFailOp;
    dst->front_face.depth_fail_op = src->frontStencilDepthFailOp;
    dst->front_face.pass_op = src->frontStencilPassOp;
    dst->front_face.func = src->frontStencilFunc;
    dst->back_face.fail_op = src->backStencilFailOp;
    dst->back_face.depth_fail_op = src->backStencilDepthFailOp;
    dst->back_face.pass_op = src->backStencilPassOp;
    dst->back_face.func = src->backStencilFunc;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_rasterizer_state(
    const SVGACOTableDXRasterizerStateEntry *src,
    VMSVGA3DD3D10RasterizerDesc *dst)
{
    VMSVGA3DD3D10Level level = VMSVGA3D_D3D10_LEVEL_10_0;

    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    dst->fill_mode = src->fillMode == SVGA3D_FILLMODE_POINT
                   ? D3D10_FILL_WIREFRAME : src->fillMode;
    dst->cull_mode = src->cullMode;
    dst->front_counter_clockwise = src->frontCounterClockwise;
    dst->depth_bias = src->depthBias;
    dst->depth_bias_clamp = src->depthBiasClamp;
    dst->slope_scaled_depth_bias = src->slopeScaledDepthBias;
    dst->depth_clip_enable = src->depthClipEnable;
    dst->scissor_enable = src->scissorEnable;
    dst->multisample_enable = src->multisampleEnable;
    dst->antialiased_line_enable = src->antialiasedLineEnable;

    /* provokingVertexLast, line width and stipple are intentionally ignored. */
    if (src->forcedSampleCount) {
        level = VMSVGA3D_D3D10_LEVEL_11_1;
    }

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_sampler_state(
    const SVGACOTableDXSamplerEntry *src, VMSVGA3DD3D10SamplerDesc *dst)
{
    uint32_t max_anisotropy;

    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    if (src->filter & SVGA3D_FILTER_ANISOTROPIC) {
        dst->filter = (src->filter & SVGA3D_FILTER_COMPARE) ? 0xd5u : 0x55u;
    } else {
        dst->filter = src->filter;
    }

    dst->address_u = src->addressU;
    dst->address_v = src->addressV;
    dst->address_w = src->addressW;
    dst->mip_lod_bias = src->mipLODBias;
    max_anisotropy = src->maxAnisotropy;

    if (max_anisotropy < 1) {
        max_anisotropy = 1;
    } else if (max_anisotropy > 16) {
        max_anisotropy = 16;
    }

    dst->max_anisotropy = max_anisotropy;
    dst->comparison_func = src->comparisonFunc;

    memcpy(dst->border_color, src->borderColor.value, sizeof(dst->border_color));

    dst->min_lod = src->minLOD;
    dst->max_lod = src->maxLOD;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

typedef struct vmsvga3d_d3d10_semantic_info_s {
    const char *name;
    uint32_t component_type;
} VMSVGA3DD3D10SemanticInfo;

static const VMSVGA3DD3D10SemanticInfo shader_semantic_info[] = {
    { "ATTRIB", VMSVGA3D_D3D10_SHADER_COMPONENT_UNKNOWN },
    { "SV_Position", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_ClipDistance", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_CullDistance", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_RenderTargetArrayIndex", VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32 },
    { "SV_ViewportArrayIndex", VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32 },
    { "SV_VertexID", VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32 },
    { "SV_PrimitiveID", VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32 },
    { "SV_InstanceID", VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32 },
    { "SV_IsFrontFace", VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32 },
    { "SV_SampleIndex", VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32 },
    { "SV_TessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_TessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_TessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_TessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_InsideTessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_InsideTessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_TessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_TessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_TessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_InsideTessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_TessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
    { "SV_TessFactor", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32 },
};

static const VMSVGA3DD3D10SemanticInfo shader_ps_output_semantic = {
    "SV_TARGET", VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32
};

static VMSVGA3DD3D10Level shader_type_level(SVGA3dShaderType type)
{
    if (type <= SVGA3D_SHADERTYPE_INVALID || type >= SVGA3D_SHADERTYPE_MAX) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    if (type < SVGA3D_SHADERTYPE_DX10_MAX) {
        return VMSVGA3D_D3D10_LEVEL_10_0;
    }

    return VMSVGA3D_D3D10_LEVEL_11_0;
}

static VMSVGA3DD3D10Level shader_program_level(uint32_t program_type)
{
    if (program_type <= VMSVGA3D_D3D10_SHADER_PROGRAM_GEOMETRY) {
        return VMSVGA3D_D3D10_LEVEL_10_0;
    }

    if (program_type <= VMSVGA3D_D3D10_SHADER_PROGRAM_COMPUTE) {
        return VMSVGA3D_D3D10_LEVEL_11_0;
    }

    return VMSVGA3D_D3D10_LEVEL_INVALID;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_define_entry(
    const SVGA3dCmdDXDefineShader *src, SVGACOTableDXShaderEntry *dst)
{
    VMSVGA3DD3D10Level level;

    if (src == NULL || dst == NULL || src->sizeInBytes < 8 ||
        src->type < SVGA3D_SHADERTYPE_MIN ||
        src->type >= SVGA3D_SHADERTYPE_MAX) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = shader_type_level(src->type);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    /* Update these fields without clearing the COTable padding. */
    dst->type = src->type;
    dst->sizeInBytes = src->sizeInBytes;
    dst->offsetInBytes = 0;
    dst->mobid = SVGA3D_INVALID_ID;

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_destroy_entry(
    SVGACOTableDXShaderEntry *entry)
{
    if (entry == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    /* Leave the padding words unchanged. */
    entry->type = SVGA3D_SHADERTYPE_INVALID;
    entry->sizeInBytes = 0;
    entry->offsetInBytes = 0;
    entry->mobid = SVGA3D_INVALID_ID;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_bind_entry(
    SVGACOTableDXShaderEntry *entry, uint32_t mobid, uint32_t offset_in_bytes)
{
    if (entry == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    /* DefineShader already supplied type and size. BindShader only changes MOB state. */
    entry->offsetInBytes = offset_in_bytes;
    entry->mobid = mobid;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_blob_info(
    const void *blob, uint32_t size_in_bytes, VMSVGA3DD3D10ShaderInfo *info)
{
    const uint8_t *bytes = blob;
    uint32_t program_token;
    uint32_t token_count;
    uint32_t program_type;
    uint32_t major_version;

    if (info == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(info, 0, sizeof(*info));

    info->semantics_complete = true;
    info->match_masks_covered = true;

    if (bytes == NULL || size_in_bytes > SVGA3D_MAX_SHADER_MEMORY_BYTES ||
        (size_in_bytes & 3u) != 0 || size_in_bytes < 8) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memcpy(&program_token, bytes, sizeof(program_token));
    memcpy(&token_count, bytes + sizeof(program_token), sizeof(token_count));

    program_type = program_token >> 16;
    major_version = (program_token >> 4) & 0x0fu;

    if (major_version < 4 ||
        program_type > VMSVGA3D_D3D10_SHADER_PROGRAM_COMPUTE ||
        token_count > size_in_bytes / 4) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    info->program_type = program_type;
    info->major_version = major_version;
    info->minor_version = program_token & 0x0fu;
    info->token_count = token_count;
    info->bytecode_size = token_count * 4;

    return shader_program_level(program_type);
}

static int shader_signature_compare(const SVGA3dDXShaderSignatureEntry *a,
                                    const SVGA3dDXShaderSignatureEntry *b)
{
    if (a->registerIndex < b->registerIndex) {
        return -1;
    }

    if (a->registerIndex > b->registerIndex) {
        return 1;
    }

    if ((a->mask & 0x0f) < (b->mask & 0x0f)) {
        return -1;
    }

    if ((a->mask & 0x0f) > (b->mask & 0x0f)) {
        return 1;
    }

    return 0;
}

static void shader_sort_signature_array(SVGA3dDXShaderSignatureEntry *array,
                                        uint32_t count)
{
    uint32_t gap;

    if (count < 2) {
        return;
    }

    gap = (count + 1) / 2;
    while (gap > 0) {
        uint32_t i;

        for (i = gap; i < count; i++) {
            SVGA3dDXShaderSignatureEntry tmp = array[i];
            uint32_t j = i;

            while (j >= gap && shader_signature_compare(&array[j - gap], &tmp) > 0) {
                array[j] = array[j - gap];
                j -= gap;
            }
            array[j] = tmp;
        }
        gap /= 2;
    }
}

static void shader_generate_semantics(VMSVGA3DD3D10ShaderInfo *info,
                                      SVGA3dDXShaderSignatureEntry *signature,
                                      VMSVGA3DD3D10ShaderSemantic *semantic,
                                      uint32_t count, bool pixel_output)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        const VMSVGA3DD3D10SemanticInfo *semantic_info;
        uint32_t j;

        if (signature[i].semanticName >= SVGADX_SIGNATURE_SEMANTIC_NAME_MAX) {
            /* The void helper may return early without failing shader bind. */
            info->semantics_complete = false;
            return;
        }

        if (pixel_output &&
            signature[i].semanticName == SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED) {
            semantic_info = &shader_ps_output_semantic;
        } else {
            semantic_info = &shader_semantic_info[signature[i].semanticName];
        }

        semantic[i].semantic_name = semantic_info->name;
        semantic[i].semantic_index = 0;
        if (signature[i].componentType ==
            VMSVGA3D_D3D10_SHADER_COMPONENT_UNKNOWN) {
            signature[i].componentType = semantic_info->component_type;
        }

        for (j = 0; j < i; j++) {
            if (strcmp(semantic[j].semantic_name, semantic[i].semantic_name) == 0) {
                semantic[i].semantic_index++;
            }
        }
    }
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_finalize_signatures(
    VMSVGA3DD3D10ShaderInfo *info)
{
    VMSVGA3DD3D10Level level;

    if (info == NULL ||
        info->input_signature_count > VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES ||
        info->output_signature_count > VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES ||
        info->patch_signature_count > VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = shader_program_level(info->program_type);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    info->semantics_complete = true;
    shader_sort_signature_array(info->input_signature, info->input_signature_count);
    shader_sort_signature_array(info->output_signature, info->output_signature_count);
    shader_sort_signature_array(info->patch_signature, info->patch_signature_count);

    shader_generate_semantics(info, info->input_signature, info->input_semantic,
                              info->input_signature_count, false);
    shader_generate_semantics(info, info->output_signature, info->output_semantic,
                              info->output_signature_count,
                              info->program_type ==
                                  VMSVGA3D_D3D10_SHADER_PROGRAM_PIXEL);
    shader_generate_semantics(info, info->patch_signature, info->patch_semantic,
                              info->patch_signature_count, false);

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_guest_signatures(
    const void *blob, uint32_t size_in_bytes, VMSVGA3DD3D10ShaderInfo *info)
{
    const uint8_t *bytes = blob;
    SVGA3dDXShaderSignatureHeader header;
    uint32_t available;
    uint32_t signature_count;
    uint32_t signature_bytes;
    uint32_t offset;
    VMSVGA3DD3D10Level level;

    if (bytes == NULL || info == NULL || info->bytecode_size > size_in_bytes) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = shader_program_level(info->program_type);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    info->guest_signatures = false;
    available = size_in_bytes - info->bytecode_size;
    if (available < sizeof(header)) {
        return level;
    }

    memcpy(&header, bytes + info->bytecode_size, sizeof(header));

    if (header.headerVersion != SVGADX_SIGNATURE_HEADER_VERSION_0) {
        return level;
    }

    if (header.numInputSignatures > VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES ||
        header.numOutputSignatures > VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES ||
        header.numPatchConstantSignatures >
            VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    available -= sizeof(header);
    signature_count = header.numInputSignatures + header.numOutputSignatures +
                      header.numPatchConstantSignatures;
    signature_bytes = signature_count * sizeof(SVGA3dDXShaderSignatureEntry);
    if (available < signature_bytes) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    info->guest_signatures = true;
    info->input_signature_count = header.numInputSignatures;
    info->output_signature_count = header.numOutputSignatures;
    info->patch_signature_count = header.numPatchConstantSignatures;
    offset = info->bytecode_size + sizeof(header);

    memcpy(info->input_signature, bytes + offset,
           info->input_signature_count * sizeof(info->input_signature[0]));
    offset += info->input_signature_count * sizeof(info->input_signature[0]);
    memcpy(info->output_signature, bytes + offset,
           info->output_signature_count * sizeof(info->output_signature[0]));
    offset += info->output_signature_count * sizeof(info->output_signature[0]);
    memcpy(info->patch_signature, bytes + offset,
           info->patch_signature_count * sizeof(info->patch_signature[0]));

    return vmsvga3d_d3d10_shader_finalize_signatures(info);
}

/* VGPU10 token values used by the shader parser. */
enum {
    SHADER_OPCODE_CUSTOMDATA = 53,
    SHADER_OPCODE_MOV = 54,
    SHADER_OPCODE_RET = 62,
    SHADER_OPCODE_UDIV = 78,
    SHADER_OPCODE_DCL_RESOURCE = 88,
    SHADER_OPCODE_DCL_INDEX_RANGE = 91,
    SHADER_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT = 94,
    SHADER_OPCODE_DCL_INPUT = 95,
    SHADER_OPCODE_DCL_INPUT_SGV = 96,
    SHADER_OPCODE_DCL_INPUT_SIV = 97,
    SHADER_OPCODE_DCL_INPUT_PS = 98,
    SHADER_OPCODE_DCL_INPUT_PS_SGV = 99,
    SHADER_OPCODE_DCL_INPUT_PS_SIV = 100,
    SHADER_OPCODE_DCL_OUTPUT = 101,
    SHADER_OPCODE_DCL_OUTPUT_SGV = 102,
    SHADER_OPCODE_DCL_OUTPUT_SIV = 103,
    SHADER_OPCODE_DCL_TEMPS = 104,
    SHADER_OPCODE_DCL_INDEXABLE_TEMP = 105,
    SHADER_OPCODE_VMWARE = 107,
    SHADER_OPCODE_INTERFACE_CALL = 120,
    SHADER_OPCODE_DCL_FUNCTION_BODY = 144,
    SHADER_OPCODE_DCL_FUNCTION_TABLE = 145,
    SHADER_OPCODE_DCL_INTERFACE = 146,
    SHADER_OPCODE_DCL_HS_MAX_TESSFACTOR = 152,
    SHADER_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT = 153,
    SHADER_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT = 154,
    SHADER_OPCODE_DCL_THREAD_GROUP = 155,
    SHADER_OPCODE_DCL_UAV_TYPED = 156,
    SHADER_OPCODE_DCL_UAV_STRUCTURED = 158,
    SHADER_OPCODE_DCL_TGSM_RAW = 159,
    SHADER_OPCODE_DCL_TGSM_STRUCTURED = 160,
    SHADER_OPCODE_DCL_RESOURCE_STRUCTURED = 162,
    SHADER_OPCODE_DCL_GS_INSTANCE_COUNT = 206,
    SHADER_OPCODE_COUNT = 218,
};

enum {
    SHADER_VMWARE_IDIV = 0,
    SHADER_VMWARE_DFRC = 1,
    SHADER_VMWARE_DRSQ = 2,
};

enum {
    SHADER_OPERAND_1_COMPONENT = 1,
    SHADER_OPERAND_4_COMPONENT = 2,
    SHADER_OPERAND_MASK_MODE = 0,
    SHADER_OPERAND_SELECT_1_MODE = 2,
    SHADER_OPERAND_TYPE_IMMEDIATE32 = 4,
    SHADER_OPERAND_TYPE_IMMEDIATE64 = 5,
    SHADER_OPERAND_TYPE_LABEL = 10,
    SHADER_OPERAND_TYPE_INPUT_PRIMITIVEID = 11,
    SHADER_OPERAND_TYPE_OUTPUT_DEPTH = 12,
    SHADER_OPERAND_TYPE_SM50_MAX = 40,
    SHADER_OPERAND_TYPE_COUNT = 41,
    SHADER_OPERAND_INDEX_0D = 0,
    SHADER_OPERAND_INDEX_1D = 1,
    SHADER_OPERAND_INDEX_2D = 2,
    SHADER_OPERAND_INDEX_3D = 3,
    SHADER_OPERAND_INDEX_IMMEDIATE32 = 0,
    SHADER_OPERAND_INDEX_IMMEDIATE64 = 1,
    SHADER_OPERAND_INDEX_RELATIVE = 2,
    SHADER_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE = 3,
    SHADER_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE = 4,
};

#define SHADER_OPCODE_SPECIAL 255u
#define SHADER_WRITER_MAX_BYTES (2u * SVGA3D_MAX_SHADER_MEMORY_BYTES)
#define SHADER_DXBC_MAGIC UINT32_C(0x43425844)
#define SHADER_DXBC_ISGN UINT32_C(0x4e475349)
#define SHADER_DXBC_OSGN UINT32_C(0x4e47534f)
#define SHADER_DXBC_PCSG UINT32_C(0x47534350)
#define SHADER_DXBC_SHDR UINT32_C(0x52444853)

static const uint8_t shader_opcode_operand_count[SHADER_OPCODE_COUNT] = {
    3u, 3u, 0u, 1u, 1u, 2u, 1u, 0u, 1u, 0u, 0u, 2u, 2u, 1u, 3u, 3u,
    3u, 3u, 0u, 0u, 0u, 0u, 0u, 0u, 3u, 2u, 2u, 2u, 2u, 3u, 3u, 1u,
    3u, 3u, 3u, 4u, 3u, 3u, 4u, 3u, 2u, 3u, 3u, 2u, 1u, 3u, 4u, 2u,
    0u, 3u, 4u, 3u, 3u, 255u, 2u, 4u, 3u, 3u, 0u, 2u, 3u, 3u, 0u, 1u,
    2u, 2u, 2u, 2u, 2u, 4u, 5u, 5u, 5u, 6u, 5u, 2u, 1u, 3u, 4u, 3u,
    3u, 4u, 4u, 3u, 3u, 3u, 2u, 3u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u,
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 255u, 4u, 4u, 3u, 2u,
    255u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 5u, 5u,
    6u, 2u, 2u, 2u, 4u, 4u, 2u, 2u, 2u, 2u, 4u, 4u, 5u, 2u, 5u, 1u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u,
    1u, 1u, 1u, 3u, 3u, 3u, 3u, 4u, 4u, 3u, 3u, 3u, 4u, 3u, 3u, 3u,
    3u, 3u, 2u, 2u, 4u, 4u, 4u, 4u, 4u, 5u, 4u, 4u, 4u, 4u, 0u, 3u,
    3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 4u, 2u, 2u, 3u, 3u, 2u, 0u, 0u,
    0u, 0u, 3u, 4u, 2u, 4u, 2u, 2u, 2u, 2u,
};

typedef struct shader_token_reader_s {
    const uint32_t *token;
    uint32_t total;
    uint32_t remaining;
} ShaderTokenReader;

typedef struct shader_operand_index_s {
    uint32_t representation;
    uint64_t immediate;
} ShaderOperandIndex;

typedef struct shader_operand_s {
    uint32_t num_components;
    uint32_t selection_mode;
    uint32_t mask;
    uint32_t operand_type;
    uint32_t index_dimension;
    ShaderOperandIndex index[SHADER_OPERAND_INDEX_3D];
    const uint32_t *tokens;
    uint32_t token_count;
} ShaderOperand;

typedef struct shader_opcode_s {
    const uint32_t *tokens;
    uint32_t token_count;
    uint32_t opcode_type;
    uint32_t opcode_subtype;
    uint32_t semantic_name;
    uint32_t operand_count;
    uint32_t operand_index[8];
    ShaderOperand operands[16];
} ShaderOpcode;

typedef struct shader_byte_writer_s {
    uint8_t *data;
    uint32_t capacity;
    uint32_t offset;
    uint32_t written;
    bool failed;
} ShaderByteWriter;

typedef struct shader_writer_state_s {
    uint32_t offset;
} ShaderWriterState;

typedef struct shader_output_context_s {
    uint32_t program_token;
    uint32_t original_token_count;
    uint32_t subroutine_offset;
} ShaderOutputContext;

typedef struct shader_dxbc_header_s {
    uint32_t magic;
    uint8_t hash[16];
    uint32_t version;
    uint32_t total_size;
    uint32_t blob_count;
    uint32_t blob_offsets[1];
} ShaderDXBCHeader;

typedef struct shader_dxbc_blob_header_s {
    uint32_t type;
    uint32_t size;
} ShaderDXBCBlobHeader;

typedef struct shader_dxbc_signature_element_s {
    uint32_t name_offset;
    uint32_t semantic_index;
    uint32_t system_value;
    uint32_t component_type;
    uint32_t register_index;
    uint32_t mask;
} ShaderDXBCSignatureElement;

static uint32_t shader_align_up(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static bool shader_reader_can_read(const ShaderTokenReader *reader,
                                   uint32_t count)
{
    return count <= reader->remaining;
}

static const uint32_t *shader_reader_ptr(const ShaderTokenReader *reader)
{
    return reader->token;
}

static uint32_t shader_reader_read32(ShaderTokenReader *reader)
{
    uint32_t value;

    if (reader->remaining == 0) {
        return 0;
    }

    value = *reader->token++;
    reader->remaining--;

    return value;
}

static uint64_t shader_reader_read64(ShaderTokenReader *reader)
{
    uint64_t low = shader_reader_read32(reader);
    uint64_t high = shader_reader_read32(reader);

    return low | (high << 32);
}

static void shader_reader_skip(ShaderTokenReader *reader, uint32_t count)
{
    reader->token += count;
    reader->remaining -= count;
}

static bool shader_writer_realloc(ShaderByteWriter *writer, uint32_t capacity)
{
    uint8_t *data;

    data = calloc(1, capacity);

    if (data == NULL) {
        writer->failed = true;
        return false;
    }

    if (writer->data != NULL) {
        if (writer->offset != 0) {
            memcpy(data, writer->data, writer->offset);
        }
        free(writer->data);
    }

    writer->data = data;
    writer->capacity = capacity;

    return true;
}

static bool shader_writer_can_write(ShaderByteWriter *writer, uint32_t more)
{
    uint32_t remaining;
    uint32_t growth;

    if (writer->failed) {
        return false;
    }

    remaining = writer->capacity - writer->offset;
    if (more <= remaining) {
        return true;
    }

    if (more >= SHADER_WRITER_MAX_BYTES) {
        writer->failed = true;
        return false;
    }

    growth = shader_align_up(more, 4096u);
    if (growth > SHADER_WRITER_MAX_BYTES - writer->capacity) {
        writer->failed = true;
        return false;
    }

    return shader_writer_realloc(writer, writer->capacity + growth);
}

static bool shader_writer_init(ShaderByteWriter *writer, uint32_t initial)
{
    memset(writer, 0, sizeof(*writer));
    return shader_writer_can_write(writer, initial);
}

static void shader_writer_reset(ShaderByteWriter *writer)
{
    free(writer->data);
    memset(writer, 0, sizeof(*writer));
}

static uint32_t shader_writer_size(const ShaderByteWriter *writer)
{
    return writer->offset;
}

static void *shader_writer_ptr(const ShaderByteWriter *writer)
{
    return writer->data + writer->offset;
}

static void shader_writer_commit(ShaderByteWriter *writer, uint32_t amount)
{
    uint32_t remaining;

    if (writer->failed) {
        return;
    }

    remaining = writer->capacity - writer->offset;
    if (amount > remaining) {
        amount = remaining;
    }

    writer->offset += amount;
    if (writer->offset > writer->written) {
        writer->written = writer->offset;
    }
}

static bool shader_writer_add_tokens(ShaderByteWriter *writer,
                                     const uint32_t *tokens, uint32_t count)
{
    uint32_t bytes;

    if (count > UINT32_MAX / sizeof(*tokens)) {
        writer->failed = true;
        return false;
    }

    bytes = count * sizeof(*tokens);
    if (!shader_writer_can_write(writer, bytes)) {
        return false;
    }

    memcpy(shader_writer_ptr(writer), tokens, bytes);
    shader_writer_commit(writer, bytes);

    return true;
}

static bool shader_writer_set_offset(ShaderByteWriter *writer, uint32_t offset,
                                     ShaderWriterState *state)
{
    uint32_t capacity;

    if (writer->failed || offset >= SHADER_WRITER_MAX_BYTES) {
        writer->failed = true;
        return false;
    }

    capacity = shader_align_up(offset, 1024u);
    if (capacity >= SHADER_WRITER_MAX_BYTES) {
        writer->failed = true;
        return false;
    }

    if (capacity > writer->capacity && !shader_writer_realloc(writer, capacity)) {
        return false;
    }

    state->offset = writer->offset;
    writer->offset = offset;

    return true;
}

static void shader_writer_restore(ShaderByteWriter *writer,
                                  const ShaderWriterState *state)
{
    writer->offset = state->offset;
}

static bool shader_parse_operand(ShaderTokenReader *reader,
                                 ShaderOperand *operands, uint32_t *used,
                                 uint32_t *operand_index)
{
    ShaderOperand *operand;
    uint32_t token0;
    uint32_t num_components;
    uint32_t operand_type;
    uint32_t index_dimension;
    uint32_t i;

    if (*used >= 16 || !shader_reader_can_read(reader, 1)) {
        return false;
    }

    *operand_index = (*used)++;
    operand = &operands[*operand_index];
    memset(operand, 0, sizeof(*operand));
    operand->tokens = shader_reader_ptr(reader);
    token0 = shader_reader_read32(reader);

    num_components = token0 & 0x3u;
    operand->num_components = num_components;
    operand->selection_mode = (token0 >> 2) & 0x3u;
    operand->mask = (token0 >> 4) & 0x0fu;
    operand_type = (token0 >> 12) & 0xffu;
    operand->operand_type = operand_type;
    index_dimension = (token0 >> 20) & 0x3u;
    operand->index_dimension = index_dimension;

    if (num_components > SHADER_OPERAND_4_COMPONENT ||
        operand_type >= SHADER_OPERAND_TYPE_COUNT) {
        return false;
    }

    if (operand_type != SHADER_OPERAND_TYPE_IMMEDIATE32 &&
        operand_type != SHADER_OPERAND_TYPE_IMMEDIATE64 &&
        num_components == SHADER_OPERAND_4_COMPONENT &&
        operand->selection_mode > SHADER_OPERAND_SELECT_1_MODE) {
        return false;
    }

    if ((token0 >> 31) != 0) {
        if (!shader_reader_can_read(reader, 1)) {
            return false;
        }
        shader_reader_read32(reader);
    }

    if (operand_type == SHADER_OPERAND_TYPE_IMMEDIATE32 ||
        operand_type == SHADER_OPERAND_TYPE_IMMEDIATE64) {
        uint32_t component_count = 0;

        if (num_components == SHADER_OPERAND_4_COMPONENT) {
            component_count = 4;
        } else if (num_components == SHADER_OPERAND_1_COMPONENT) {
            component_count = 1;
        }
        if (!shader_reader_can_read(reader, component_count)) {
            return false;
        }
        shader_reader_skip(reader, component_count);
    }

    for (i = 0; i < index_dimension; i++) {
        uint32_t representation;
        uint32_t relative_index;

        if (i == 0) {
            representation = (token0 >> 22) & 0x7u;
        } else if (i == 1) {
            representation = (token0 >> 25) & 0x7u;
        } else {
            /* Deliberately do not parse the third index dimension. */
            continue;
        }

        operand->index[i].representation = representation;

        switch (representation) {
        case SHADER_OPERAND_INDEX_IMMEDIATE32:
            if (!shader_reader_can_read(reader, 1)) {
                return false;
            }
            operand->index[i].immediate = shader_reader_read32(reader);
            break;
        case SHADER_OPERAND_INDEX_IMMEDIATE64:
            if (!shader_reader_can_read(reader, 2)) {
                return false;
            }
            operand->index[i].immediate = shader_reader_read64(reader);
            break;
        case SHADER_OPERAND_INDEX_RELATIVE:
            if (!shader_reader_can_read(reader, 1) ||
                !shader_parse_operand(reader, operands, used, &relative_index)) {
                return false;
            }
            break;
        case SHADER_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE:
            if (!shader_reader_can_read(reader, 2)) {
                return false;
            }
            operand->index[i].immediate = shader_reader_read32(reader);
            if (!shader_parse_operand(reader, operands, used, &relative_index)) {
                return false;
            }
            break;
        case SHADER_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE:
            if (!shader_reader_can_read(reader, 3)) {
                return false;
            }
            operand->index[i].immediate = shader_reader_read64(reader);
            if (!shader_parse_operand(reader, operands, used, &relative_index)) {
                return false;
            }
            break;
        default:
            return false;
        }
    }

    operand->token_count = (uint32_t)(shader_reader_ptr(reader) - operand->tokens);
    return true;
}

static bool shader_opcode_has_long_length(uint32_t opcode)
{
    return opcode == SHADER_OPCODE_DCL_FUNCTION_BODY ||
           opcode == SHADER_OPCODE_DCL_FUNCTION_TABLE ||
           opcode == SHADER_OPCODE_DCL_INTERFACE ||
           opcode == SHADER_OPCODE_INTERFACE_CALL ||
           opcode == SHADER_OPCODE_DCL_THREAD_GROUP;
}

static bool shader_parse_opcode(ShaderTokenReader *reader, ShaderOpcode *opcode)
{
    uint32_t token0;
    uint32_t operand_count;
    uint32_t opcode_tokens;
    uint32_t consumed;
    uint32_t used = 0;
    uint32_t i;

    memset(opcode, 0, sizeof(*opcode));

    if (!shader_reader_can_read(reader, 1)) {
        return false;
    }

    opcode->tokens = shader_reader_ptr(reader);
    token0 = shader_reader_read32(reader);
    opcode->opcode_type = token0 & 0x7ffu;

    if (opcode->opcode_type >= SHADER_OPCODE_COUNT) {
        return false;
    }

    operand_count = shader_opcode_operand_count[opcode->opcode_type];
    if (operand_count != SHADER_OPCODE_SPECIAL) {
        opcode_tokens = (token0 >> 24) & 0x7fu;
        consumed = 1;
        if ((token0 >> 31) != 0) {
            if (shader_opcode_has_long_length(opcode->opcode_type)) {
                if (!shader_reader_can_read(reader, 1)) {
                    return false;
                }
                opcode_tokens = shader_reader_read32(reader);
                consumed++;
            } else {
                uint32_t extended;

                do {
                    if (!shader_reader_can_read(reader, 1)) {
                        return false;
                    }
                    extended = shader_reader_read32(reader);
                    consumed++;
                } while ((extended >> 31) != 0);
            }
        }
        if (opcode_tokens < 1 || opcode_tokens >= 256 || opcode_tokens < consumed ||
            !shader_reader_can_read(reader, opcode_tokens - consumed) ||
            operand_count >= 8) {
            return false;
        }
        opcode->token_count = opcode_tokens;

        if (opcode->opcode_type == SHADER_OPCODE_INTERFACE_CALL) {
            if (!shader_reader_can_read(reader, 1)) {
                return false;
            }
            shader_reader_skip(reader, 1);
        }

        for (i = 0; i < operand_count; i++) {
            if (!shader_parse_operand(reader, opcode->operands, &used,
                                      &opcode->operand_index[i])) {
                return false;
            }
        }
        opcode->operand_count = operand_count;

        switch (opcode->opcode_type) {
        case SHADER_OPCODE_DCL_INPUT_SIV:
        case SHADER_OPCODE_DCL_INPUT_SGV:
        case SHADER_OPCODE_DCL_INPUT_PS_SIV:
        case SHADER_OPCODE_DCL_INPUT_PS_SGV:
        case SHADER_OPCODE_DCL_OUTPUT_SIV:
        case SHADER_OPCODE_DCL_OUTPUT_SGV:
            if (!shader_reader_can_read(reader, 1)) {
                return false;
            }
            opcode->semantic_name = shader_reader_read32(reader) & 0xffffu;
            break;
        case SHADER_OPCODE_DCL_RESOURCE:
        case SHADER_OPCODE_DCL_TEMPS:
        case SHADER_OPCODE_DCL_INDEX_RANGE:
        case SHADER_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
        case SHADER_OPCODE_DCL_GS_INSTANCE_COUNT:
        case SHADER_OPCODE_DCL_HS_MAX_TESSFACTOR:
        case SHADER_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
        case SHADER_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:
        case SHADER_OPCODE_DCL_UAV_TYPED:
        case SHADER_OPCODE_DCL_UAV_STRUCTURED:
        case SHADER_OPCODE_DCL_TGSM_RAW:
        case SHADER_OPCODE_DCL_RESOURCE_STRUCTURED:
            if (!shader_reader_can_read(reader, 1)) {
                return false;
            }
            shader_reader_skip(reader, 1);
            break;
        case SHADER_OPCODE_DCL_INDEXABLE_TEMP:
        case SHADER_OPCODE_DCL_THREAD_GROUP:
            if (!shader_reader_can_read(reader, 3)) {
                return false;
            }
            shader_reader_skip(reader, 3);
            break;
        case SHADER_OPCODE_DCL_TGSM_STRUCTURED:
            if (!shader_reader_can_read(reader, 2)) {
                return false;
            }
            shader_reader_skip(reader, 2);
            break;
        default:
            break;
        }
        return true;
    }

    if (opcode->opcode_type == SHADER_OPCODE_CUSTOMDATA) {
        if (!shader_reader_can_read(reader, 1)) {
            return false;
        }
        opcode->token_count = shader_reader_read32(reader);
        if (opcode->token_count < 2) {
            opcode->token_count = 2;
        }
        if (!shader_reader_can_read(reader, opcode->token_count - 2)) {
            return false;
        }
        shader_reader_skip(reader, opcode->token_count - 2);
        return true;
    }

    if (opcode->opcode_type != SHADER_OPCODE_VMWARE) {
        return false;
    }

    opcode->token_count = (token0 >> 24) & 0x7fu;
    if (opcode->token_count < 3 ||
        !shader_reader_can_read(reader, opcode->token_count - 1)) {
        return false;
    }

    opcode->opcode_subtype = (token0 >> 11) & 0x0fu;
    switch (opcode->opcode_subtype) {
    case SHADER_VMWARE_IDIV:
        operand_count = 4;
        break;
    case SHADER_VMWARE_DFRC:
    case SHADER_VMWARE_DRSQ:
        operand_count = 2;
        break;
    default:
        return false;
    }

    for (i = 0; i < operand_count; i++) {
        if (!shader_parse_operand(reader, opcode->operands, &used,
                                  &opcode->operand_index[i])) {
            return false;
        }
    }
    opcode->operand_count = operand_count;

    return true;
}

static uint32_t shader_make_opcode(uint32_t opcode, uint32_t length)
{
    return (opcode & 0x7ffu) | ((length & 0x7fu) << 24);
}

static uint32_t shader_make_label_operand(void)
{
    return SHADER_OPERAND_1_COMPONENT |
           (SHADER_OPERAND_TYPE_LABEL << 12) |
           (SHADER_OPERAND_INDEX_1D << 20) |
           (SHADER_OPERAND_INDEX_IMMEDIATE32 << 22);
}

static bool shader_emit_call(ShaderByteWriter *writer,
                             const ShaderOpcode *opcode, uint32_t label)
{
    uint32_t tokens[3];
    uint32_t nop;
    uint32_t i;

    tokens[0] = shader_make_opcode(4u, 3u);
    tokens[1] = shader_make_label_operand();
    tokens[2] = label;

    if (!shader_writer_add_tokens(writer, tokens, 3)) {
        return false;
    }

    nop = shader_make_opcode(58u, 1u);

    for (i = 3; i < opcode->token_count; i++) {
        if (!shader_writer_add_tokens(writer, &nop, 1)) {
            return false;
        }
    }

    return true;
}

static bool shader_emit_label(ShaderByteWriter *writer, uint32_t label)
{
    uint32_t tokens[3];

    tokens[0] = shader_make_opcode(44u, 3u);
    tokens[1] = shader_make_label_operand();
    tokens[2] = label;

    return shader_writer_add_tokens(writer, tokens, 3);
}

static bool shader_emit_ret(ShaderByteWriter *writer)
{
    uint32_t token = shader_make_opcode(SHADER_OPCODE_RET, 1u);

    return shader_writer_add_tokens(writer, &token, 1);
}

static bool shader_emit_vmware_subroutine(ShaderOutputContext *context,
                                          ShaderByteWriter *writer,
                                          const ShaderOpcode *opcode,
                                          uint32_t replacement_opcode)
{
    ShaderWriterState state;
    uint32_t label;
    uint32_t token;

    label = (context->subroutine_offset - shader_writer_size(writer)) / 4u;
    if (!shader_emit_call(writer, opcode, label) ||
        !shader_writer_set_offset(writer, context->subroutine_offset, &state) ||
        !shader_emit_label(writer, label)) {
        return false;
    }

    token = shader_make_opcode(replacement_opcode, opcode->token_count);
    if (!shader_writer_add_tokens(writer, &token, 1) ||
        !shader_writer_add_tokens(writer, &opcode->tokens[1],
                                  opcode->token_count - 1) ||
        !shader_emit_ret(writer)) {
        return false;
    }

    context->subroutine_offset = shader_writer_size(writer);
    shader_writer_restore(writer, &state);

    return true;
}

static bool shader_output_opcode(ShaderOutputContext *context,
                                 ShaderByteWriter *writer,
                                 const ShaderOpcode *opcode)
{
    if ((context->program_token >> 16) == VMSVGA3D_D3D10_SHADER_PROGRAM_PIXEL &&
        opcode->opcode_type == SHADER_OPCODE_DCL_RESOURCE) {
        uint32_t token0;
        uint32_t return_type;

        if (opcode->token_count != 4) {
            return false;
        }

        token0 = opcode->tokens[0];
        if (((token0 >> 11) & 0x1fu) == VMSVGA3D_D3D10_SHADER_RESOURCE_BUFFER) {
            token0 &= ~(UINT32_C(0x1f) << 11);
            token0 |= VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2D << 11;
            return_type = UINT32_C(0x5555);
            return shader_writer_add_tokens(writer, &token0, 1) &&
                   shader_writer_add_tokens(writer, &opcode->tokens[1], 2) &&
                   shader_writer_add_tokens(writer, &return_type, 1);
        }
    } else if (opcode->opcode_type == SHADER_OPCODE_VMWARE) {
        switch (opcode->opcode_subtype) {
        case SHADER_VMWARE_IDIV:
            return shader_emit_vmware_subroutine(context, writer, opcode,
                                                 SHADER_OPCODE_UDIV);
        case SHADER_VMWARE_DFRC:
        case SHADER_VMWARE_DRSQ:
            return shader_emit_vmware_subroutine(context, writer, opcode,
                                                 SHADER_OPCODE_MOV);
        default:
            return false;
        }
    }

    return shader_writer_add_tokens(writer, opcode->tokens, opcode->token_count);
}

static bool shader_opcode_signature_target(const ShaderOpcode *opcode,
                                           bool *input)
{
    switch (opcode->opcode_type) {
    case SHADER_OPCODE_DCL_INPUT:
    case SHADER_OPCODE_DCL_INPUT_SIV:
    case SHADER_OPCODE_DCL_INPUT_PS:
        *input = true;
        return true;
    case SHADER_OPCODE_DCL_OUTPUT:
    case SHADER_OPCODE_DCL_OUTPUT_SIV:
    case SHADER_OPCODE_DCL_OUTPUT_SGV:
        *input = false;
        return true;
    default:
        return false;
    }
}

static bool shader_extract_signature(VMSVGA3DD3D10ShaderInfo *info,
                                     const ShaderOpcode *opcode)
{
    const ShaderOperand *operand;
    SVGA3dDXShaderSignatureEntry *entry;
    bool input;
    uint32_t index_dimension;

    if (!shader_opcode_signature_target(opcode, &input)) {
        return true;
    }

    if (opcode->operand_count == 0) {
        return false;
    }

    operand = &opcode->operands[opcode->operand_index[0]];

    if (input) {
        if (info->input_signature_count >= VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES) {
            return false;
        }
        entry = &info->input_signature[info->input_signature_count++];
    } else {
        if (info->output_signature_count >= VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES) {
            return false;
        }
        entry = &info->output_signature[info->output_signature_count++];
    }

    memset(entry, 0, sizeof(*entry));

    if (operand->index[0].representation != SHADER_OPERAND_INDEX_IMMEDIATE32 &&
        operand->index[0].representation != SHADER_OPERAND_INDEX_IMMEDIATE64) {
        return false;
    }

    index_dimension = operand->index_dimension;
    if (index_dimension == SHADER_OPERAND_INDEX_0D) {
        if (operand->operand_type == SHADER_OPERAND_TYPE_INPUT_PRIMITIVEID) {
            entry->registerIndex = 0;
            entry->semanticName = SVGADX_SIGNATURE_SEMANTIC_NAME_PRIMITIVE_ID;
        } else if (operand->operand_type == SHADER_OPERAND_TYPE_OUTPUT_DEPTH) {
            entry->registerIndex = UINT32_MAX;
            entry->semanticName = SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED;
        } else if (operand->operand_type <= SHADER_OPERAND_TYPE_SM50_MAX) {
            entry->registerIndex = 0;
            entry->semanticName = opcode->semantic_name;
        } else {
            return false;
        }
    } else if (index_dimension <= SHADER_OPERAND_INDEX_3D) {
        entry->registerIndex =
            (uint32_t)operand->index[index_dimension - SHADER_OPERAND_INDEX_1D]
                .immediate;
        entry->semanticName = opcode->semantic_name;
    } else {
        return false;
    }

    entry->mask = operand->mask;
    entry->componentType = VMSVGA3D_D3D10_SHADER_COMPONENT_UNKNOWN;
    entry->minPrecision = SVGADX_SIGNATURE_MIN_PRECISION_DEFAULT;

    return true;
}

static void shader_record_resource(VMSVGA3DD3D10ShaderInfo *info,
                                   const ShaderOpcode *opcode,
                                   uint32_t output_offset)
{
    const ShaderOperand *operand;
    uint32_t resource_index;

    if (info->program_type != VMSVGA3D_D3D10_SHADER_PROGRAM_PIXEL ||
        opcode->opcode_type != SHADER_OPCODE_DCL_RESOURCE ||
        opcode->operand_count != 1) {
        return;
    }

    operand = &opcode->operands[opcode->operand_index[0]];
    if (operand->index_dimension != SHADER_OPERAND_INDEX_1D ||
        operand->index[0].representation != SHADER_OPERAND_INDEX_IMMEDIATE32) {
        return;
    }

    resource_index = (uint32_t)operand->index[0].immediate;
    if (resource_index >= SVGA3D_DX_MAX_SRVIEWS) {
        return;
    }

    info->resource_declaration_offsets[resource_index] = output_offset;
    if (info->resource_declaration_count < resource_index + 1) {
        info->resource_declaration_count = resource_index + 1;
    }
}

static bool shader_parse_and_rewrite(const uint32_t *tokens,
                                     uint32_t shader_object_size,
                                     VMSVGA3DD3D10ShaderInfo *info)
{
    ShaderTokenReader reader;
    ShaderByteWriter writer;
    ShaderOutputContext context;
    uint32_t initial_size;

    if (info->token_count < 2 ||
        shader_object_size > UINT32_MAX - 4096u) {
        return false;
    }

    /* Size the writer from the complete shader object, signatures included. */
    initial_size = 4096u + shader_object_size;
    if (!shader_writer_init(&writer, initial_size) ||
        !shader_writer_add_tokens(&writer, tokens, 2)) {
        shader_writer_reset(&writer);
        return false;
    }

    reader.token = &tokens[2];
    reader.total = info->token_count - 2;
    reader.remaining = reader.total;
    context.program_token = tokens[0];
    context.original_token_count = info->token_count;
    context.subroutine_offset = info->token_count * 4u;

    while (shader_reader_can_read(&reader, 1)) {
        ShaderOpcode opcode;
        uint32_t output_offset = shader_writer_size(&writer);

        if (!shader_parse_opcode(&reader, &opcode) ||
            !shader_output_opcode(&context, &writer, &opcode)) {
            shader_writer_reset(&writer);
            return false;
        }
        shader_record_resource(info, &opcode, output_offset);
        if (!shader_extract_signature(info, &opcode)) {
            shader_writer_reset(&writer);
            return false;
        }
    }

    info->rewritten_bytecode = writer.data;
    info->rewritten_bytecode_size = writer.written;
    writer.data = NULL;
    shader_writer_reset(&writer);

    if (info->rewritten_bytecode_size < 8 ||
        (info->rewritten_bytecode_size & 3u) != 0) {
        return false;
    }
    ((uint32_t *)info->rewritten_bytecode)[1] =
        info->rewritten_bytecode_size / 4u;

    return true;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_parse(
    const void *blob, uint32_t size_in_bytes, VMSVGA3DD3D10ShaderInfo *info)
{
    VMSVGA3DD3D10Level level;

    if (info == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_shader_blob_info(blob, size_in_bytes, info);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    if (!shader_parse_and_rewrite(blob, size_in_bytes, info)) {
        vmsvga3d_d3d10_shader_release(info);
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_shader_finalize_signatures(info);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        vmsvga3d_d3d10_shader_release(info);
        return level;
    }

    level = vmsvga3d_d3d10_shader_guest_signatures(blob, size_in_bytes, info);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        vmsvga3d_d3d10_shader_release(info);
    }

    return level;
}

void vmsvga3d_d3d10_shader_release(VMSVGA3DD3D10ShaderInfo *info)
{
    if (info == NULL) {
        return;
    }

    free(info->rewritten_bytecode);
    memset(info, 0, sizeof(*info));
}

uint32_t vmsvga3d_d3d10_shader_resource_return_type(
    SVGA3dSurfaceFormat format)
{
    switch (format) {
    case SVGA3D_R32G32B32A32_UINT:
    case SVGA3D_R32G32B32_UINT:
    case SVGA3D_R16G16B16A16_UINT:
    case SVGA3D_R32G32_UINT:
    case SVGA3D_D32_FLOAT_S8X24_UINT:
    case SVGA3D_X32_G8X24_UINT:
    case SVGA3D_R10G10B10A2_UINT:
    case SVGA3D_R8G8B8A8_UINT:
    case SVGA3D_R16G16_UINT:
    case SVGA3D_R32_UINT:
    case SVGA3D_X24_G8_UINT:
    case SVGA3D_R8G8_UINT:
    case SVGA3D_R16_UINT:
    case SVGA3D_R8_UINT:
        return VMSVGA3D_D3D10_SHADER_RETURN_UINT;
    case SVGA3D_R32G32B32A32_SINT:
    case SVGA3D_R32G32B32_SINT:
    case SVGA3D_R16G16B16A16_SINT:
    case SVGA3D_R32G32_SINT:
    case SVGA3D_R8G8B8A8_SINT:
    case SVGA3D_R16G16_SINT:
    case SVGA3D_R32_SINT:
    case SVGA3D_R8G8_SINT:
    case SVGA3D_R16_SINT:
    case SVGA3D_R8_SINT:
        return VMSVGA3D_D3D10_SHADER_RETURN_SINT;
    case SVGA3D_R16G16B16A16_SNORM:
    case SVGA3D_R16_SNORM:
    case SVGA3D_R8_SNORM:
    case SVGA3D_BC4_SNORM:
    case SVGA3D_BC5_SNORM:
    case SVGA3D_R8G8B8A8_SNORM:
    case SVGA3D_R16G16_SNORM:
    case SVGA3D_R8G8_SNORM:
        return VMSVGA3D_D3D10_SHADER_RETURN_SNORM;
    case SVGA3D_R8G8B8A8_UNORM:
    case SVGA3D_R8G8B8A8_UNORM_SRGB:
    case SVGA3D_D24_UNORM_S8_UINT:
    case SVGA3D_R24_UNORM_X8:
    case SVGA3D_R8G8_UNORM:
    case SVGA3D_R16_UNORM:
    case SVGA3D_R8_UNORM:
    case SVGA3D_R8G8_B8G8_UNORM:
    case SVGA3D_G8R8_G8B8_UNORM:
    case SVGA3D_BC1_UNORM_SRGB:
    case SVGA3D_BC2_UNORM_SRGB:
    case SVGA3D_BC3_UNORM_SRGB:
    case SVGA3D_R10G10B10_XR_BIAS_A2_UNORM:
    case SVGA3D_B8G8R8A8_UNORM_SRGB:
    case SVGA3D_B8G8R8X8_UNORM_SRGB:
    case SVGA3D_R16G16B16A16_UNORM:
    case SVGA3D_R10G10B10A2_UNORM:
    case SVGA3D_R16G16_UNORM:
    case SVGA3D_D16_UNORM:
    case SVGA3D_A8_UNORM:
    case SVGA3D_BC1_UNORM:
    case SVGA3D_BC2_UNORM:
    case SVGA3D_BC3_UNORM:
    case SVGA3D_B5G6R5_UNORM:
    case SVGA3D_B5G5R5A1_UNORM:
    case SVGA3D_B8G8R8A8_UNORM:
    case SVGA3D_B8G8R8X8_UNORM:
    case SVGA3D_BC4_UNORM:
    case SVGA3D_BC5_UNORM:
    case SVGA3D_B4G4R4A4_UNORM:
    case SVGA3D_BC7_UNORM:
    case SVGA3D_BC7_UNORM_SRGB:
        return VMSVGA3D_D3D10_SHADER_RETURN_UNORM;
    case SVGA3D_R32G32B32_FLOAT:
    case SVGA3D_R32_FLOAT_X8X24:
    case SVGA3D_R11G11B10_FLOAT:
    case SVGA3D_D32_FLOAT:
    case SVGA3D_R32G32B32A32_FLOAT:
    case SVGA3D_R16G16B16A16_FLOAT:
    case SVGA3D_R32G32_FLOAT:
    case SVGA3D_R16G16_FLOAT:
    case SVGA3D_R32_FLOAT:
    case SVGA3D_R16_FLOAT:
    case SVGA3D_R9G9B9E5_SHAREDEXP:
        return VMSVGA3D_D3D10_SHADER_RETURN_FLOAT;
    default:
        /* Deliberately default unrecognized formats to UNORM. */
        return VMSVGA3D_D3D10_SHADER_RETURN_UNORM;
    }
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_resource_binding(
    const SVGACOTableDXSRViewEntry *view, uint32_t array_elements,
    uint32_t multisample_count,
    VMSVGA3DD3D10ShaderResourceBinding *binding)
{
    if (view == NULL || binding == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(binding, 0, sizeof(*binding));

    binding->format = view->format;
    binding->return_type = vmsvga3d_d3d10_shader_resource_return_type(view->format);

    switch (view->resourceDimension) {
    case SVGA3D_RESOURCE_BUFFEREX:
    case SVGA3D_RESOURCE_BUFFER:
        binding->resource_dimension = VMSVGA3D_D3D10_SHADER_RESOURCE_BUFFER;
        break;
    case SVGA3D_RESOURCE_TEXTURE1D:
        binding->resource_dimension = array_elements <= 1 ?
            VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE1D :
            VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE1DARRAY;
        break;
    case SVGA3D_RESOURCE_TEXTURE2D:
        if (array_elements <= 1) {
            binding->resource_dimension = multisample_count > 1 ?
                VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2DMS :
                VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2D;
        } else {
            binding->resource_dimension = multisample_count > 1 ?
                VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2DMSARRAY :
                VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2DARRAY;
        }
        break;
    case SVGA3D_RESOURCE_TEXTURE3D:
        binding->resource_dimension = VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE3D;
        break;
    case SVGA3D_RESOURCE_TEXTURECUBE:
        binding->resource_dimension = array_elements <= 6 ?
            VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURECUBE :
            VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURECUBEARRAY;
        break;
    default:
        /* Continue with TEXTURE2D for an unexpected dimension. */
        binding->resource_dimension = VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2D;
        break;
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_update_resources(
    VMSVGA3DD3D10ShaderInfo *info,
    const VMSVGA3DD3D10ShaderResourceBinding *bindings,
    uint32_t binding_count)
{
    uint32_t i;

    if (info == NULL || info->rewritten_bytecode == NULL ||
        binding_count > SVGA3D_DX_MAX_SRVIEWS ||
        (binding_count != 0 && bindings == NULL)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    for (i = 0; i < info->resource_declaration_count; i++) {
        uint32_t dimension = VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2D;
        uint32_t return_type = VMSVGA3D_D3D10_SHADER_RETURN_FLOAT;
        uint32_t offset;
        uint32_t *tokens;

        if (i < binding_count) {
            dimension = bindings[i].resource_dimension;
            return_type = bindings[i].return_type;
        }

        if (dimension > VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURECUBEARRAY ||
            return_type > VMSVGA3D_D3D10_SHADER_RETURN_MIXED) {
            /* Skip only this declaration when its patch values are invalid. */
            continue;
        }

        offset = info->resource_declaration_offsets[i];
        if (offset == 0 || offset >= info->rewritten_bytecode_size ||
            info->rewritten_bytecode_size - offset < 16) {
            continue;
        }

        tokens = (uint32_t *)((uint8_t *)info->rewritten_bytecode + offset);
        if (dimension != VMSVGA3D_D3D10_SHADER_RESOURCE_UNKNOWN) {
            tokens[0] &= ~(UINT32_C(0x1f) << 11);
            tokens[0] |= (dimension & 0x1fu) << 11;
        }

        if ((uint8_t)return_type != 0) {
            tokens[3] &= UINT32_C(0xffff0000);
            tokens[3] |= (return_type & 0x0fu) |
                         ((return_type & 0x0fu) << 4) |
                         ((return_type & 0x0fu) << 8) |
                         ((return_type & 0x0fu) << 12);
        }
    }

    return shader_program_level(info->program_type);
}

static uint32_t shader_load_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void shader_store_le32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

#define SHADER_MD5_F1(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define SHADER_MD5_F2(x, y, z) SHADER_MD5_F1((z), (x), (y))
#define SHADER_MD5_F3(x, y, z) ((x) ^ (y) ^ (z))
#define SHADER_MD5_F4(x, y, z) ((y) ^ ((x) | ~(z)))
#define SHADER_MD5_STEP(f, w, x, y, z, data, shift) do { \
    (w) += f((x), (y), (z)) + (data); \
    (w) = ((w) << (shift)) | ((w) >> (32u - (shift))); \
    (w) += (x); \
} while (0)

static void shader_md5_transform(uint32_t state[4], const uint8_t *block)
{
    uint32_t in[16];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t i;

    for (i = 0; i < 16; i++) {
        in[i] = shader_load_le32(block + i * 4u);
    }

    SHADER_MD5_STEP(SHADER_MD5_F1, a, b, c, d, in[0] + UINT32_C(0xd76aa478), 7);
    SHADER_MD5_STEP(SHADER_MD5_F1, d, a, b, c, in[1] + UINT32_C(0xe8c7b756), 12);
    SHADER_MD5_STEP(SHADER_MD5_F1, c, d, a, b, in[2] + UINT32_C(0x242070db), 17);
    SHADER_MD5_STEP(SHADER_MD5_F1, b, c, d, a, in[3] + UINT32_C(0xc1bdceee), 22);
    SHADER_MD5_STEP(SHADER_MD5_F1, a, b, c, d, in[4] + UINT32_C(0xf57c0faf), 7);
    SHADER_MD5_STEP(SHADER_MD5_F1, d, a, b, c, in[5] + UINT32_C(0x4787c62a), 12);
    SHADER_MD5_STEP(SHADER_MD5_F1, c, d, a, b, in[6] + UINT32_C(0xa8304613), 17);
    SHADER_MD5_STEP(SHADER_MD5_F1, b, c, d, a, in[7] + UINT32_C(0xfd469501), 22);
    SHADER_MD5_STEP(SHADER_MD5_F1, a, b, c, d, in[8] + UINT32_C(0x698098d8), 7);
    SHADER_MD5_STEP(SHADER_MD5_F1, d, a, b, c, in[9] + UINT32_C(0x8b44f7af), 12);
    SHADER_MD5_STEP(SHADER_MD5_F1, c, d, a, b, in[10] + UINT32_C(0xffff5bb1), 17);
    SHADER_MD5_STEP(SHADER_MD5_F1, b, c, d, a, in[11] + UINT32_C(0x895cd7be), 22);
    SHADER_MD5_STEP(SHADER_MD5_F1, a, b, c, d, in[12] + UINT32_C(0x6b901122), 7);
    SHADER_MD5_STEP(SHADER_MD5_F1, d, a, b, c, in[13] + UINT32_C(0xfd987193), 12);
    SHADER_MD5_STEP(SHADER_MD5_F1, c, d, a, b, in[14] + UINT32_C(0xa679438e), 17);
    SHADER_MD5_STEP(SHADER_MD5_F1, b, c, d, a, in[15] + UINT32_C(0x49b40821), 22);

    SHADER_MD5_STEP(SHADER_MD5_F2, a, b, c, d, in[1] + UINT32_C(0xf61e2562), 5);
    SHADER_MD5_STEP(SHADER_MD5_F2, d, a, b, c, in[6] + UINT32_C(0xc040b340), 9);
    SHADER_MD5_STEP(SHADER_MD5_F2, c, d, a, b, in[11] + UINT32_C(0x265e5a51), 14);
    SHADER_MD5_STEP(SHADER_MD5_F2, b, c, d, a, in[0] + UINT32_C(0xe9b6c7aa), 20);
    SHADER_MD5_STEP(SHADER_MD5_F2, a, b, c, d, in[5] + UINT32_C(0xd62f105d), 5);
    SHADER_MD5_STEP(SHADER_MD5_F2, d, a, b, c, in[10] + UINT32_C(0x02441453), 9);
    SHADER_MD5_STEP(SHADER_MD5_F2, c, d, a, b, in[15] + UINT32_C(0xd8a1e681), 14);
    SHADER_MD5_STEP(SHADER_MD5_F2, b, c, d, a, in[4] + UINT32_C(0xe7d3fbc8), 20);
    SHADER_MD5_STEP(SHADER_MD5_F2, a, b, c, d, in[9] + UINT32_C(0x21e1cde6), 5);
    SHADER_MD5_STEP(SHADER_MD5_F2, d, a, b, c, in[14] + UINT32_C(0xc33707d6), 9);
    SHADER_MD5_STEP(SHADER_MD5_F2, c, d, a, b, in[3] + UINT32_C(0xf4d50d87), 14);
    SHADER_MD5_STEP(SHADER_MD5_F2, b, c, d, a, in[8] + UINT32_C(0x455a14ed), 20);
    SHADER_MD5_STEP(SHADER_MD5_F2, a, b, c, d, in[13] + UINT32_C(0xa9e3e905), 5);
    SHADER_MD5_STEP(SHADER_MD5_F2, d, a, b, c, in[2] + UINT32_C(0xfcefa3f8), 9);
    SHADER_MD5_STEP(SHADER_MD5_F2, c, d, a, b, in[7] + UINT32_C(0x676f02d9), 14);
    SHADER_MD5_STEP(SHADER_MD5_F2, b, c, d, a, in[12] + UINT32_C(0x8d2a4c8a), 20);

    SHADER_MD5_STEP(SHADER_MD5_F3, a, b, c, d, in[5] + UINT32_C(0xfffa3942), 4);
    SHADER_MD5_STEP(SHADER_MD5_F3, d, a, b, c, in[8] + UINT32_C(0x8771f681), 11);
    SHADER_MD5_STEP(SHADER_MD5_F3, c, d, a, b, in[11] + UINT32_C(0x6d9d6122), 16);
    SHADER_MD5_STEP(SHADER_MD5_F3, b, c, d, a, in[14] + UINT32_C(0xfde5380c), 23);
    SHADER_MD5_STEP(SHADER_MD5_F3, a, b, c, d, in[1] + UINT32_C(0xa4beea44), 4);
    SHADER_MD5_STEP(SHADER_MD5_F3, d, a, b, c, in[4] + UINT32_C(0x4bdecfa9), 11);
    SHADER_MD5_STEP(SHADER_MD5_F3, c, d, a, b, in[7] + UINT32_C(0xf6bb4b60), 16);
    SHADER_MD5_STEP(SHADER_MD5_F3, b, c, d, a, in[10] + UINT32_C(0xbebfbc70), 23);
    SHADER_MD5_STEP(SHADER_MD5_F3, a, b, c, d, in[13] + UINT32_C(0x289b7ec6), 4);
    SHADER_MD5_STEP(SHADER_MD5_F3, d, a, b, c, in[0] + UINT32_C(0xeaa127fa), 11);
    SHADER_MD5_STEP(SHADER_MD5_F3, c, d, a, b, in[3] + UINT32_C(0xd4ef3085), 16);
    SHADER_MD5_STEP(SHADER_MD5_F3, b, c, d, a, in[6] + UINT32_C(0x04881d05), 23);
    SHADER_MD5_STEP(SHADER_MD5_F3, a, b, c, d, in[9] + UINT32_C(0xd9d4d039), 4);
    SHADER_MD5_STEP(SHADER_MD5_F3, d, a, b, c, in[12] + UINT32_C(0xe6db99e5), 11);
    SHADER_MD5_STEP(SHADER_MD5_F3, c, d, a, b, in[15] + UINT32_C(0x1fa27cf8), 16);
    SHADER_MD5_STEP(SHADER_MD5_F3, b, c, d, a, in[2] + UINT32_C(0xc4ac5665), 23);

    SHADER_MD5_STEP(SHADER_MD5_F4, a, b, c, d, in[0] + UINT32_C(0xf4292244), 6);
    SHADER_MD5_STEP(SHADER_MD5_F4, d, a, b, c, in[7] + UINT32_C(0x432aff97), 10);
    SHADER_MD5_STEP(SHADER_MD5_F4, c, d, a, b, in[14] + UINT32_C(0xab9423a7), 15);
    SHADER_MD5_STEP(SHADER_MD5_F4, b, c, d, a, in[5] + UINT32_C(0xfc93a039), 21);
    SHADER_MD5_STEP(SHADER_MD5_F4, a, b, c, d, in[12] + UINT32_C(0x655b59c3), 6);
    SHADER_MD5_STEP(SHADER_MD5_F4, d, a, b, c, in[3] + UINT32_C(0x8f0ccc92), 10);
    SHADER_MD5_STEP(SHADER_MD5_F4, c, d, a, b, in[10] + UINT32_C(0xffeff47d), 15);
    SHADER_MD5_STEP(SHADER_MD5_F4, b, c, d, a, in[1] + UINT32_C(0x85845dd1), 21);
    SHADER_MD5_STEP(SHADER_MD5_F4, a, b, c, d, in[8] + UINT32_C(0x6fa87e4f), 6);
    SHADER_MD5_STEP(SHADER_MD5_F4, d, a, b, c, in[15] + UINT32_C(0xfe2ce6e0), 10);
    SHADER_MD5_STEP(SHADER_MD5_F4, c, d, a, b, in[6] + UINT32_C(0xa3014314), 15);
    SHADER_MD5_STEP(SHADER_MD5_F4, b, c, d, a, in[13] + UINT32_C(0x4e0811a1), 21);
    SHADER_MD5_STEP(SHADER_MD5_F4, a, b, c, d, in[4] + UINT32_C(0xf7537e82), 6);
    SHADER_MD5_STEP(SHADER_MD5_F4, d, a, b, c, in[11] + UINT32_C(0xbd3af235), 10);
    SHADER_MD5_STEP(SHADER_MD5_F4, c, d, a, b, in[2] + UINT32_C(0x2ad7d2bb), 15);
    SHADER_MD5_STEP(SHADER_MD5_F4, b, c, d, a, in[9] + UINT32_C(0xeb86d391), 21);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void shader_dxbc_hash(const void *data, uint32_t size, uint8_t digest[16])
{
    const uint8_t *bytes = data;
    uint32_t state[4] = {
        UINT32_C(0x67452301), UINT32_C(0xefcdab89),
        UINT32_C(0x98badcfe), UINT32_C(0x10325476)
    };
    uint8_t block[64];
    uint32_t complete = size & ~UINT32_C(63);
    uint32_t remaining = size - complete;
    uint32_t offset;
    uint32_t i;

    for (offset = 0; offset < complete; offset += 64) {
        shader_md5_transform(state, bytes + offset);
    }
    bytes += complete;

    memset(block, 0, sizeof(block));

    if (remaining >= 56) {
        memcpy(block, bytes, remaining);
        block[remaining] = 0x80;
        shader_md5_transform(state, block);
        memset(block, 0, sizeof(block));
    } else {
        memcpy(block + 4, bytes, remaining);
        block[4 + remaining] = 0x80;
    }

    shader_store_le32(block, size << 3);
    shader_store_le32(block + 60, (size << 1) | 1u);
    shader_md5_transform(state, block);

    for (i = 0; i < 4; i++) {
        shader_store_le32(digest + i * 4u, state[i]);
    }
}

static uint32_t shader_system_value(uint32_t semantic)
{
    switch (semantic) {
    case SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED:
        return 0;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_POSITION:
        return 1;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_CLIP_DISTANCE:
        return 2;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_CULL_DISTANCE:
        return 3;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_RENDER_TARGET_ARRAY_INDEX:
        return 4;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_VIEWPORT_ARRAY_INDEX:
        return 5;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_VERTEX_ID:
        return 6;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_PRIMITIVE_ID:
        return 7;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_INSTANCE_ID:
        return 8;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_IS_FRONT_FACE:
        return 9;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_SAMPLE_INDEX:
        return 10;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR:
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR:
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR:
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR:
        return 11;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR:
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR:
        return 12;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR:
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR:
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR:
        return 13;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_INSIDE_TESSFACTOR:
        return 14;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_LINE_DETAIL_TESSFACTOR:
        return 15;
    case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_LINE_DENSITY_TESSFACTOR:
        return 16;
    default:
        return 0;
    }
}

static bool shader_create_signature_blob(const VMSVGA3DD3D10ShaderInfo *info,
                                         ShaderDXBCHeader *header,
                                         uint32_t blob_type, uint32_t count,
                                         const SVGA3dDXShaderSignatureEntry *signature,
                                         const VMSVGA3DD3D10ShaderSemantic *semantic,
                                         ShaderByteWriter *writer)
{
    ShaderDXBCBlobHeader *blob;
    uint8_t *blob_data;
    ShaderDXBCSignatureElement *elements;
    uint32_t blob_size;
    uint32_t i;

    (void)info;
    if (count > SVGA3D_DX_SM41_MAX_VERTEXINPUTREGISTERS) {
        return false;
    }

    blob_size = 8u + count * sizeof(*elements);
    if (!shader_writer_can_write(writer, sizeof(*blob) + blob_size)) {
        return false;
    }

    blob = shader_writer_ptr(writer);
    blob->type = blob_type;
    blob->size = 0;
    blob_data = (uint8_t *)(blob + 1);
    ((uint32_t *)blob_data)[0] = count;
    ((uint32_t *)blob_data)[1] = 8u;
    elements = (ShaderDXBCSignatureElement *)(blob_data + 8u);

    for (i = 0; i < count; i++) {
        uint32_t j;
        uint32_t name_offset = 0;
        uint32_t name_size;

        for (j = 0; j < i; j++) {
            const char *prior_name = (const char *)blob_data + elements[j].name_offset;

            if (strcmp(prior_name, semantic[i].semantic_name) == 0) {
                name_offset = elements[j].name_offset;
                break;
            }
        }
        elements[i].name_offset = name_offset;
        elements[i].semantic_index = semantic[i].semantic_index;
        elements[i].system_value = shader_system_value(signature[i].semanticName);
        elements[i].component_type = signature[i].componentType;
        elements[i].register_index = signature[i].registerIndex;
        elements[i].mask = signature[i].mask;

        if (name_offset == 0) {
            name_size = (uint32_t)strlen(semantic[i].semantic_name) + 1u;
            elements[i].name_offset = blob_size;
            if (!shader_writer_can_write(writer,
                                         sizeof(*blob) + blob_size + name_size)) {
                return false;
            }
            memcpy(blob_data + blob_size, semantic[i].semantic_name, name_size);
            blob_size += name_size;
        }
    }

    blob_size = shader_align_up(blob_size, 4u);
    blob->size = blob_size;
    header->total_size += sizeof(*blob) + blob_size;

    shader_writer_commit(writer, sizeof(*blob) + blob_size);

    return true;
}

static bool shader_create_code_blob(ShaderDXBCHeader *header,
                                    const void *bytecode, uint32_t bytecode_size,
                                    ShaderByteWriter *writer)
{
    ShaderDXBCBlobHeader *blob;
    uint32_t blob_size = shader_align_up(bytecode_size, 4u);

    if (!shader_writer_can_write(writer, sizeof(*blob) + blob_size)) {
        return false;
    }

    blob = shader_writer_ptr(writer);
    blob->type = SHADER_DXBC_SHDR;
    blob->size = blob_size;

    memcpy(blob + 1, bytecode, bytecode_size);

    header->total_size += sizeof(*blob) + blob_size;
    shader_writer_commit(writer, sizeof(*blob) + blob_size);

    return true;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_create_dxbc(
    const VMSVGA3DD3D10ShaderInfo *info, VMSVGA3DD3D10ShaderDXBC *dxbc)
{
    ShaderByteWriter writer;
    ShaderDXBCHeader *header;
    uint32_t blob_count = 3;
    uint32_t header_size;
    uint32_t blob_index = 0;
    VMSVGA3DD3D10Level level;

    if (info == NULL || dxbc == NULL || info->rewritten_bytecode == NULL ||
        info->rewritten_bytecode_size == 0) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = shader_program_level(info->program_type);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    memset(dxbc, 0, sizeof(*dxbc));

    if (info->program_type == VMSVGA3D_D3D10_SHADER_PROGRAM_HULL ||
        info->program_type == VMSVGA3D_D3D10_SHADER_PROGRAM_DOMAIN) {
        blob_count++;
    }

    header_size = 32u + blob_count * sizeof(uint32_t);
    if (!shader_writer_init(&writer, 4096u + info->rewritten_bytecode_size) ||
        !shader_writer_can_write(&writer, header_size)) {
        shader_writer_reset(&writer);
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    header = shader_writer_ptr(&writer);
    header->magic = SHADER_DXBC_MAGIC;
    header->version = 1;
    header->total_size = header_size;
    header->blob_count = blob_count;
    shader_writer_commit(&writer, header_size);

    header->blob_offsets[blob_index++] = shader_writer_size(&writer);
    if (!shader_create_signature_blob(info, header, SHADER_DXBC_ISGN,
                                      info->input_signature_count,
                                      info->input_signature, info->input_semantic,
                                      &writer)) {
        shader_writer_reset(&writer);
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    header->blob_offsets[blob_index++] = shader_writer_size(&writer);
    if (!shader_create_signature_blob(info, header, SHADER_DXBC_OSGN,
                                      info->output_signature_count,
                                      info->output_signature,
                                      info->output_semantic, &writer)) {
        shader_writer_reset(&writer);
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    if (info->program_type == VMSVGA3D_D3D10_SHADER_PROGRAM_HULL ||
        info->program_type == VMSVGA3D_D3D10_SHADER_PROGRAM_DOMAIN) {
        header->blob_offsets[blob_index++] = shader_writer_size(&writer);
        if (!shader_create_signature_blob(info, header, SHADER_DXBC_PCSG,
                                          info->patch_signature_count,
                                          info->patch_signature,
                                          info->patch_semantic, &writer)) {
            shader_writer_reset(&writer);
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }
    }

    header->blob_offsets[blob_index++] = shader_writer_size(&writer);
    if (!shader_create_code_blob(header, info->rewritten_bytecode,
                                 info->rewritten_bytecode_size, &writer) ||
        blob_index != blob_count) {
        shader_writer_reset(&writer);
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    shader_dxbc_hash(&header->version,
                     header->total_size - offsetof(ShaderDXBCHeader, version),
                     header->hash);

    dxbc->data = writer.data;
    dxbc->size = writer.written;
    writer.data = NULL;

    shader_writer_reset(&writer);

    return level;
}

void vmsvga3d_d3d10_shader_dxbc_release(VMSVGA3DD3D10ShaderDXBC *dxbc)
{
    if (dxbc == NULL) {
        return;
    }

    free(dxbc->data);
    memset(dxbc, 0, sizeof(*dxbc));
}

uint32_t vmsvga3d_d3d10_shader_component_type_from_format(
    SVGA3dSurfaceFormat format)
{
    switch (format) {
    case SVGA3D_R32G32B32A32_UINT:
    case SVGA3D_R32G32B32_UINT:
    case SVGA3D_R16G16B16A16_UINT:
    case SVGA3D_R32G32_UINT:
    case SVGA3D_D32_FLOAT_S8X24_UINT:
    case SVGA3D_X32_G8X24_UINT:
    case SVGA3D_R10G10B10A2_UINT:
    case SVGA3D_R8G8B8A8_UINT:
    case SVGA3D_R16G16_UINT:
    case SVGA3D_R32_UINT:
    case SVGA3D_X24_G8_UINT:
    case SVGA3D_R8G8_UINT:
    case SVGA3D_R16_UINT:
    case SVGA3D_R8_UINT:
        return VMSVGA3D_D3D10_SHADER_COMPONENT_UINT32;
    case SVGA3D_R32G32B32A32_SINT:
    case SVGA3D_R32G32B32_SINT:
    case SVGA3D_R16G16B16A16_SINT:
    case SVGA3D_R32G32_SINT:
    case SVGA3D_R8G8B8A8_SINT:
    case SVGA3D_R16G16_SINT:
    case SVGA3D_R32_SINT:
    case SVGA3D_R8G8_SINT:
    case SVGA3D_R16_SINT:
    case SVGA3D_R8_SINT:
        return VMSVGA3D_D3D10_SHADER_COMPONENT_SINT32;
    default:
        /* Deliberately default unknown formats to float. */
        return VMSVGA3D_D3D10_SHADER_COMPONENT_FLOAT32;
    }
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_update_vs_input_signature(
    VMSVGA3DD3D10ShaderInfo *info, const SVGA3dInputElementDesc *elements,
    uint32_t element_count)
{
    uint32_t count;
    uint32_t i;

    if (info == NULL || (element_count != 0 && elements == NULL)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    count = element_count < info->input_signature_count ?
                element_count : info->input_signature_count;

    for (i = 0; i < count; i++) {
        info->input_signature[i].componentType =
            vmsvga3d_d3d10_shader_component_type_from_format(elements[i].format);
    }

    return shader_program_level(info->program_type);
}

static bool shader_match_input(VMSVGA3DD3D10ShaderInfo *shader,
                               const VMSVGA3DD3D10ShaderInfo *prior)
{
    bool masks_covered = true;
    uint32_t i;

    for (i = 0; i < shader->input_signature_count; i++) {
        const SVGA3dDXShaderSignatureEntry *input = &shader->input_signature[i];
        VMSVGA3DD3D10ShaderSemantic *semantic = &shader->input_semantic[i];
        int32_t match = -1;
        uint32_t j;

        if (input->semanticName != SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED) {
            continue;
        }

        for (j = 0; j < prior->output_signature_count; j++) {
            const SVGA3dDXShaderSignatureEntry *output = &prior->output_signature[j];

            if (output->semanticName != SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED) {
                continue;
            }

            if (output->registerIndex == input->registerIndex) {
                match = (int32_t)j;
                if (output->mask == input->mask) {
                    break;
                }
            }
        }

        if (match >= 0) {
            const SVGA3dDXShaderSignatureEntry *output =
                &prior->output_signature[(uint32_t)match];
            const VMSVGA3DD3D10ShaderSemantic *output_semantic =
                &prior->output_semantic[(uint32_t)match];

            if ((output->mask & input->mask) != input->mask) {
                /* Apply the semantic index even when the masks are not fully covered. */
                masks_covered = false;
            }

            semantic->semantic_index = output_semantic->semantic_index;
        }
    }
    return masks_covered;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_match_signatures(
    SVGA3dShaderType type, VMSVGA3DD3D10ShaderInfo *shader,
    const VMSVGA3DD3D10ShaderInfo *vs, const VMSVGA3DD3D10ShaderInfo *hs,
    const VMSVGA3DD3D10ShaderInfo *ds, const VMSVGA3DD3D10ShaderInfo *gs,
    const VMSVGA3DD3D10ShaderInfo *ps)
{
    const VMSVGA3DD3D10ShaderInfo *prior = NULL;
    VMSVGA3DD3D10Level level;
    uint32_t i;

    if (shader == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = shader_type_level(type);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    shader->match_masks_covered = true;
    switch (type) {
    case SVGA3D_SHADERTYPE_VS:
        for (i = 0; i < shader->input_signature_count; i++) {
            if (shader->input_signature[i].semanticName ==
                SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED) {
                shader->input_semantic[i].semantic_index =
                    shader->input_signature[i].registerIndex;
            }
        }
        break;
    case SVGA3D_SHADERTYPE_HS:
        if (vs != NULL) {
            shader->match_masks_covered = shader_match_input(shader, vs);
        }
        break;
    case SVGA3D_SHADERTYPE_DS:
        if (hs != NULL) {
            shader->match_masks_covered = shader_match_input(shader, hs);
        }
        break;
    case SVGA3D_SHADERTYPE_GS:
        prior = ds != NULL ? ds : vs;
        if (prior != NULL) {
            if (shader->input_signature_count == 0) {
                shader->input_signature_count = prior->output_signature_count;
                memcpy(shader->input_signature, prior->output_signature,
                       prior->output_signature_count * sizeof(shader->input_signature[0]));
                memcpy(shader->input_semantic, prior->output_semantic,
                       prior->output_signature_count * sizeof(shader->input_semantic[0]));
            } else {
                shader->match_masks_covered = shader_match_input(shader, prior);
            }
        }
        if (ps != NULL && shader->output_signature_count == 0) {
            shader->output_signature_count = ps->input_signature_count;
            memcpy(shader->output_signature, ps->input_signature,
                   ps->input_signature_count * sizeof(shader->output_signature[0]));
            memcpy(shader->output_semantic, ps->input_semantic,
                   ps->input_signature_count * sizeof(shader->output_semantic[0]));
        }
        break;
    case SVGA3D_SHADERTYPE_PS:
        prior = gs != NULL ? gs : (ds != NULL ? ds : vs);
        if (prior != NULL) {
            shader->match_masks_covered = shader_match_input(shader, prior);
        }
        break;
    case SVGA3D_SHADERTYPE_CS:
        break;
    default:
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_output_semantics(
    const VMSVGA3DD3D10ShaderInfo *info,
    VMSVGA3DD3D10ShaderOutputSemantic *outputs, uint32_t output_capacity,
    uint32_t *output_count)
{
    uint32_t i;
    VMSVGA3DD3D10Level level;

    if (info == NULL || output_count == NULL ||
        info->output_signature_count > output_capacity ||
        (info->output_signature_count != 0 && outputs == NULL)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = shader_program_level(info->program_type);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    for (i = 0; i < info->output_signature_count; i++) {
        outputs[i].register_index = info->output_signature[i].registerIndex;
        outputs[i].mask = info->output_signature[i].mask;
        outputs[i].semantic_name = info->output_semantic[i].semantic_name;
        outputs[i].semantic_index = info->output_semantic[i].semantic_index;
    }

    *output_count = info->output_signature_count;

    return level;
}

static uint32_t bit_first_set_1based(uint32_t value)
{
    uint32_t bit;

    for (bit = 0; bit < 32; bit++) {
        if (value & (UINT32_C(1) << bit)) {
            return bit + 1;
        }
    }

    return 0;
}

static uint32_t bit_last_set_1based(uint32_t value)
{
    uint32_t bit;

    for (bit = 32; bit > 0; bit--) {
        if (value & (UINT32_C(1) << (bit - 1))) {
            return bit;
        }
    }

    return 0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_legacy_entry(
    const SVGA3dCmdDXDefineStreamOutput *src,
    SVGACOTableDXStreamOutputEntry *dst)
{
    if (src == NULL || dst == NULL ||
        src->numOutputStreamEntries > SVGA3D_MAX_DX10_STREAMOUT_DECLS) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));
    dst->numOutputStreamEntries = src->numOutputStreamEntries;
    memcpy(dst->decl, src->decl, sizeof(dst->decl));
    memcpy(dst->streamOutputStrideInBytes, src->streamOutputStrideInBytes,
           sizeof(dst->streamOutputStrideInBytes));

    dst->rasterizedStream = 0;
    dst->numOutputStreamStrides = 0;
    dst->mobid = SVGA3D_INVALID_ID;
    dst->usesMob = 0;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}


VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_mob_entry(
    const SVGA3dCmdDXDefineStreamOutputWithMob *src,
    SVGACOTableDXStreamOutputEntry *dst)
{
    if (src == NULL || dst == NULL ||
        src->numOutputStreamEntries > SVGA3D_MAX_STREAMOUT_DECLS ||
        src->numOutputStreamStrides > SVGA3D_DX_MAX_SOTARGETS ||
        (src->rasterizedStream >= SVGA3D_DX_MAX_SOTARGETS &&
         src->rasterizedStream != SVGA3D_DX_SO_NO_RASTERIZED_STREAM)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));
    dst->numOutputStreamEntries = src->numOutputStreamEntries;
    memcpy(dst->streamOutputStrideInBytes, src->streamOutputStrideInBytes,
           sizeof(dst->streamOutputStrideInBytes));

    dst->rasterizedStream = src->rasterizedStream;
    dst->numOutputStreamStrides = src->numOutputStreamStrides;
    dst->mobid = SVGA3D_INVALID_ID;
    dst->offsetInBytes = 0;
    dst->usesMob = 1;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_bind_entry(
    SVGACOTableDXStreamOutputEntry *entry, uint32_t mobid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes)
{
    uint64_t required_bytes;

    if (entry == NULL || !entry->usesMob) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    required_bytes = (uint64_t)entry->numOutputStreamEntries *
                     sizeof(SVGA3dStreamOutputDeclarationEntry);

    if (size_in_bytes < required_bytes) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    entry->mobid = mobid;
    entry->offsetInBytes = offset_in_bytes;
    entry->usesMob = 1;

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_plan(
    const SVGACOTableDXStreamOutputEntry *entry,
    const SVGA3dStreamOutputDeclarationEntry *declarations,
    const VMSVGA3DD3D10ShaderOutputSemantic *shader_outputs,
    uint32_t shader_output_count, VMSVGA3DD3D10StreamOutputPlan *plan)
{
    uint32_t max_semantic_index = 0;
    uint32_t i;

    if (entry == NULL || plan == NULL ||
        entry->numOutputStreamEntries > SVGA3D_MAX_STREAMOUT_DECLS ||
        entry->numOutputStreamStrides > SVGA3D_DX_MAX_SOTARGETS ||
        (entry->rasterizedStream >= SVGA3D_DX_MAX_SOTARGETS &&
         entry->rasterizedStream != SVGA3D_DX_SO_NO_RASTERIZED_STREAM) ||
        (!entry->usesMob &&
         entry->numOutputStreamEntries > SVGA3D_MAX_DX10_STREAMOUT_DECLS) ||
        (entry->usesMob && entry->mobid == SVGA3D_INVALID_ID) ||
        ((entry->usesMob || entry->numOutputStreamEntries != 0) &&
         declarations == NULL) ||
        (shader_output_count != 0 && shader_outputs == NULL)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));

    plan->declaration_count = entry->numOutputStreamEntries;
    memcpy(plan->strides, entry->streamOutputStrideInBytes,
           sizeof(plan->strides));

    plan->stride_count = entry->numOutputStreamStrides;
    plan->rasterized_stream = entry->rasterizedStream;
    plan->use_explicit_strides = entry->numOutputStreamStrides != 0;
    plan->uses_mob = entry->usesMob != 0;
    plan->all_semantics_resolved = true;

    for (i = 0; i < plan->declaration_count; i++) {
        const SVGA3dStreamOutputDeclarationEntry *src = &declarations[i];
        VMSVGA3DD3D10StreamOutputDecl *dst = &plan->declarations[i];
        uint32_t register_mask = src->registerMask & 0x0f;
        uint32_t first_bit = bit_first_set_1based(register_mask);
        uint32_t last_bit = bit_last_set_1based(register_mask);
        uint32_t output_index;

        dst->stream = src->stream;
        dst->semantic_name = NULL;
        dst->semantic_index = 0;
        dst->start_component = first_bit > 0 ? (uint8_t)(first_bit - 1) : 0;
        dst->component_count = first_bit > 0 ?
                                   (uint8_t)(last_bit - (first_bit - 1)) :
                                   0;
        dst->output_slot = (uint8_t)src->outputSlot;

        for (output_index = 0; output_index < shader_output_count; output_index++) {
            const VMSVGA3DD3D10ShaderOutputSemantic *output =
                &shader_outputs[output_index];

            if (output->register_index == src->registerIndex &&
                (src->registerMask & ~output->mask) == 0) {
                dst->semantic_name = output->semantic_name;
                dst->semantic_index = output->semantic_index;
                if (dst->semantic_index > max_semantic_index) {
                    max_semantic_index = dst->semantic_index;
                }
                break;
            }
        }

        if (dst->semantic_name == NULL) {
            /* Keep unresolved declarations for host creation. */
            plan->all_semantics_resolved = false;
        }
    }

    for (i = 0; i <= max_semantic_index; i++) {
        uint32_t min_start_component = UINT32_MAX;
        uint32_t j;

        for (j = 0; j < plan->declaration_count; j++) {
            const VMSVGA3DD3D10StreamOutputDecl *decl = &plan->declarations[j];

            if (decl->semantic_index == i &&
                decl->start_component < min_start_component) {
                min_start_component = decl->start_component;
            }
        }

        if (min_start_component == UINT32_MAX) {
            continue;
        }

        for (j = 0; j < plan->declaration_count; j++) {
            VMSVGA3DD3D10StreamOutputDecl *decl = &plan->declarations[j];

            if (decl->semantic_index == i) {
                decl->start_component =
                    (uint8_t)(decl->start_component - min_start_component);
            }
        }
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_viewports(
    const SVGA3dViewport *src, uint32_t count, SVGA3dViewport *dst)
{
    if (!dst || count > SVGA3D_DX_MAX_VIEWPORTS || (count && !src)) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    /*
     * The guest records are stored unchanged and the host backend casts
     * the same bytes directly to D3D11_VIEWPORT.  Keep all float bit patterns
     * intact and leave unused destination entries untouched.
     */
    if (count) {
        memcpy(dst, src, count * sizeof(*dst));
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_primitive_topology(
    SVGA3dPrimitiveType primitive, uint32_t *d3d_topology)
{
    static const uint32_t map[SVGA3D_PRIMITIVE_DX10_MAX] = {
        D3D10_TOPOLOGY_UNDEFINED,
        D3D10_TOPOLOGY_TRIANGLELIST,
        D3D10_TOPOLOGY_POINTLIST,
        D3D10_TOPOLOGY_LINELIST,
        D3D10_TOPOLOGY_LINESTRIP,
        D3D10_TOPOLOGY_TRIANGLESTRIP,
        D3D10_TOPOLOGY_TRIANGLESTRIP,
        D3D10_TOPOLOGY_LINELIST_ADJ,
        D3D10_TOPOLOGY_LINESTRIP_ADJ,
        D3D10_TOPOLOGY_TRIANGLELIST_ADJ,
        D3D10_TOPOLOGY_TRIANGLESTRIP_ADJ,
    };

    if (!d3d_topology || primitive >= SVGA3D_PRIMITIVE_MAX) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    if (primitive >= SVGA3D_PRIMITIVE_DX10_MAX) {
        /* D3D11 patch-list topology values run contiguously from 33 to 64. */
        *d3d_topology = 33u + (uint32_t)(primitive - SVGA3D_PRIMITIVE_DX10_MAX);
        return VMSVGA3D_D3D10_LEVEL_11_0;
    }

    *d3d_topology = map[primitive];
    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_stage(
    SVGA3dShaderType shader_type, uint32_t *stage_index)
{
    if (!stage_index || shader_type <= SVGA3D_SHADERTYPE_INVALID ||
        shader_type >= SVGA3D_SHADERTYPE_MAX) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    *stage_index = (uint32_t)shader_type - (uint32_t)SVGA3D_SHADERTYPE_MIN;
    if (shader_type < SVGA3D_SHADERTYPE_DX10_MAX) {
        return VMSVGA3D_D3D10_LEVEL_10_0;
    }

    return VMSVGA3D_D3D10_LEVEL_11_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_query_info(
    SVGA3dQueryType type, uint32_t flags, VMSVGA3DD3D10QueryInfo *info)
{
    static const VMSVGA3DD3D10QueryInfo base[SVGA3D_QUERYTYPE_DX10_MAX] = {
        { D3D10_QUERY_OCCLUSION, sizeof(SVGADXOcclusionQueryResult), 8, false, false },
        { D3D10_QUERY_TIMESTAMP, sizeof(SVGADXTimestampQueryResult), 8, false, false },
        {
            D3D10_QUERY_TIMESTAMP_DISJOINT, sizeof(SVGADXTimestampDisjointQueryResult),
            16, false, false
        },
        {
            D3D10_QUERY_PIPELINE_STATISTICS, sizeof(SVGADXPipelineStatisticsQueryResult),
            88, false, false
        },
        {
            D3D10_QUERY_OCCLUSION_PREDICATE, sizeof(SVGADXOcclusionPredicateQueryResult),
            4, true, false
        },
        {
            D3D10_QUERY_SO_STATISTICS, sizeof(SVGADXStreamOutStatisticsQueryResult),
            16, false, false
        },
        {
            D3D10_QUERY_SO_OVERFLOW_PREDICATE, sizeof(SVGADXStreamOutPredicateQueryResult),
            4, true, false
        },
        { D3D10_QUERY_OCCLUSION, sizeof(SVGADXOcclusion64QueryResult), 8, false, false },
    };

    if (!info || type >= SVGA3D_QUERYTYPE_MAX) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    if (type >= SVGA3D_QUERYTYPE_DX10_MAX) {
        static const VMSVGA3DD3D10QueryInfo later[] = {
            {
                D3D11_QUERY_SO_STATISTICS_STREAM0, sizeof(SVGADXStreamOutStatisticsQueryResult),
                16, false, false
            },
            {
                D3D11_QUERY_SO_STATISTICS_STREAM1, sizeof(SVGADXStreamOutStatisticsQueryResult),
                16, false, false
            },
            {
                D3D11_QUERY_SO_STATISTICS_STREAM2, sizeof(SVGADXStreamOutStatisticsQueryResult),
                16, false, false
            },
            {
                D3D11_QUERY_SO_STATISTICS_STREAM3, sizeof(SVGADXStreamOutStatisticsQueryResult),
                16, false, false
            },
            {
                D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0,
                    sizeof(SVGADXStreamOutPredicateQueryResult),
                4, true, false
            },
            {
                D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1,
                    sizeof(SVGADXStreamOutPredicateQueryResult),
                4, true, false
            },
            {
                D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2,
                    sizeof(SVGADXStreamOutPredicateQueryResult),
                4, true, false
            },
            {
                D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3,
                    sizeof(SVGADXStreamOutPredicateQueryResult),
                4, true, false
            },
        };
        uint32_t index = (uint32_t)type - (uint32_t)SVGA3D_QUERYTYPE_DX10_MAX;
        if (index >= sizeof(later) / sizeof(later[0])) {
            return VMSVGA3D_D3D10_LEVEL_INVALID;
        }
        *info = later[index];
        info->predicate_hint = !!(flags & SVGA3D_DXQUERY_FLAG_PREDICATEHINT);
        return VMSVGA3D_D3D10_LEVEL_11_0;
    }

    *info = base[type];
    info->predicate_hint = !!(flags & SVGA3D_DXQUERY_FLAG_PREDICATEHINT);
    /* Current VirtualBox creates an ID3D11Predicate at DEFINE time only for
     * OCCLUSIONPREDICATE + PREDICATEHINT. Other hinted query types are rejected
     * before the backend object is created.
     */
    if (info->predicate_hint && type != SVGA3D_QUERYTYPE_OCCLUSIONPREDICATE) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

static void query_entry_set_u32(uint8_t *entry, uint32_t offset,
                                uint32_t value)
{
    entry[offset] = (uint8_t)value;
    entry[offset + 1] = (uint8_t)(value >> 8);
    entry[offset + 2] = (uint8_t)(value >> 16);
    entry[offset + 3] = (uint8_t)(value >> 24);
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_query_define_entry(
    const SVGA3dCmdDXDefineQuery *src, void *dst)
{
    VMSVGA3DD3D10QueryInfo info;
    VMSVGA3DD3D10Level level;
    uint8_t *entry = dst;

    if (src == NULL || entry == NULL || src->type < SVGA3D_QUERYTYPE_MIN ||
        src->type >= SVGA3D_QUERYTYPE_DX10_MAX) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_query_info(src->type, src->flags, &info);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    /* Packed wire layout: type[0], pad0[1..2], state[3], then uint32 fields. */
    entry[0] = (uint8_t)src->type;
    entry[3] = SVGADX_QDSTATE_IDLE;
    query_entry_set_u32(entry, 4, src->flags);
    query_entry_set_u32(entry, 8, SVGA3D_INVALID_ID);
    query_entry_set_u32(entry, 12, 0);

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_query_destroy_entry(void *entry_ptr)
{
    uint8_t *entry = entry_ptr;

    if (entry == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    /* Leave the two packed pad0 bytes untouched. */
    entry[0] = (uint8_t)SVGA3D_QUERYTYPE_INVALID;
    entry[3] = SVGADX_QDSTATE_INVALID;
    query_entry_set_u32(entry, 4, 0);
    query_entry_set_u32(entry, 8, SVGA3D_INVALID_ID);
    query_entry_set_u32(entry, 12, 0);

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_query_bind_entry(
    void *entry_ptr, uint32_t mobid)
{
    uint8_t *entry = entry_ptr;

    if (entry == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    query_entry_set_u32(entry, 8, mobid);
    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_query_set_offset(
    void *entry_ptr, uint32_t offset)
{
    uint8_t *entry = entry_ptr;

    if (entry == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    query_entry_set_u32(entry, 12, offset);

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_query_bind_all(
    void *entries_ptr, uint32_t count, uint32_t mobid)
{
    uint8_t *entries = entries_ptr;
    uint32_t i;

    if (entries == NULL && count != 0) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    for (i = 0; i < count; ++i) {
        uint8_t *entry = entries +
            (size_t)i * VMSVGA3D_D3D10_QUERY_COTABLE_ENTRY_SIZE;

        if (entry[0] != (uint8_t)SVGA3D_QUERYTYPE_INVALID) {
            query_entry_set_u32(entry, 8, mobid);
        }
    }

    return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_query_execution_plan(
    SVGA3dQueryType type, uint32_t flags,
    VMSVGA3DD3D10QueryExecutionPlan *plan)
{
    VMSVGA3DD3D10QueryInfo info;
    VMSVGA3DD3D10Level level;

    if (plan == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_query_info(type, flags, &info);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    memset(plan, 0, sizeof(*plan));

    plan->d3d_query = info.d3d_query;
    plan->misc_flags = info.predicate_hint
                     ? VMSVGA3D_D3D10_QUERY_MISC_PREDICATEHINT : 0;
    plan->svga_result_size = info.svga_result_size;
    plan->d3d_result_size = info.d3d_result_size;

    /* Skip Begin only for timestamp queries. */
    plan->issue_begin = type != SVGA3D_QUERYTYPE_TIMESTAMP;
    plan->issue_end = true;
    /* Current VirtualBox ends predicate-hint queries but deliberately does not
     * call GetData for them: the completed ID3D11Predicate remains pending in
     * the guest query state and is consumed directly by SetPredication.
     */
    plan->get_data_after_end = !info.predicate_hint;
    plan->getdata_flags = 0;
    /* Current VirtualBox never waits in DX_END_QUERY.  It probes GetData once,
     * queues S_FALSE queries for ProcessPendingTasks, and treats real failures
     * as terminal. */
    plan->wait_until_ready = false;
    plan->retry_non_s_ok = false;
    plan->yield_while_waiting = false;

    /* The device does not cache query results, so explicit readback is a NOP. */
    plan->readback_is_noop = true;

    return level;
}

static uint32_t query_read_u32(const uint8_t *src)
{
    uint32_t value;
    memcpy(&value, src, sizeof(value));
    return value;
}

static uint64_t query_read_u64(const uint8_t *src)
{
    uint64_t value;
    memcpy(&value, src, sizeof(value));
    return value;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_query_result(
    SVGA3dQueryType type, const void *d3d_result, uint32_t d3d_result_size,
    SVGADXQueryResultUnion *svga_result, uint32_t *svga_result_size)
{
    const uint8_t *src = d3d_result;
    VMSVGA3DD3D10QueryInfo info;
    VMSVGA3DD3D10Level level;

    if (src == NULL || svga_result == NULL || svga_result_size == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    level = vmsvga3d_d3d10_query_info(type, 0, &info);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID ||
        d3d_result_size < info.d3d_result_size) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    switch (type) {
    case SVGA3D_QUERYTYPE_OCCLUSION:
        svga_result->occ.samplesRendered = (uint32_t)query_read_u64(src);
        break;
    case SVGA3D_QUERYTYPE_TIMESTAMP:
        svga_result->ts.timestamp = query_read_u64(src);
        break;
    case SVGA3D_QUERYTYPE_TIMESTAMPDISJOINT:
        svga_result->tsDisjoint.realFrequency = query_read_u64(src);
        svga_result->tsDisjoint.disjoint = query_read_u32(src + 8);
        break;
    case SVGA3D_QUERYTYPE_PIPELINESTATS:
        svga_result->pipelineStats.inputAssemblyVertices = query_read_u64(src);
        svga_result->pipelineStats.inputAssemblyPrimitives = query_read_u64(src + 8);
        svga_result->pipelineStats.vertexShaderInvocations = query_read_u64(src + 16);
        svga_result->pipelineStats.geometryShaderInvocations = query_read_u64(src + 24);
        svga_result->pipelineStats.geometryShaderPrimitives = query_read_u64(src + 32);
        svga_result->pipelineStats.clipperInvocations = query_read_u64(src + 40);
        svga_result->pipelineStats.clipperPrimitives = query_read_u64(src + 48);
        svga_result->pipelineStats.pixelShaderInvocations = query_read_u64(src + 56);
        svga_result->pipelineStats.hullShaderInvocations = query_read_u64(src + 64);
        svga_result->pipelineStats.domainShaderInvocations = query_read_u64(src + 72);
        svga_result->pipelineStats.computeShaderInvocations = query_read_u64(src + 80);
        break;
    case SVGA3D_QUERYTYPE_OCCLUSIONPREDICATE:
        svga_result->occPred.anySamplesRendered = query_read_u32(src);
        break;
    case SVGA3D_QUERYTYPE_STREAMOUTPUTSTATS:
    case SVGA3D_QUERYTYPE_SOSTATS_STREAM0:
    case SVGA3D_QUERYTYPE_SOSTATS_STREAM1:
    case SVGA3D_QUERYTYPE_SOSTATS_STREAM2:
    case SVGA3D_QUERYTYPE_SOSTATS_STREAM3:
        svga_result->soStats.numPrimitivesWritten = query_read_u64(src);
        svga_result->soStats.numPrimitivesRequired = query_read_u64(src + 8);
        break;
    case SVGA3D_QUERYTYPE_STREAMOVERFLOWPREDICATE:
    case SVGA3D_QUERYTYPE_SOP_STREAM0:
    case SVGA3D_QUERYTYPE_SOP_STREAM1:
    case SVGA3D_QUERYTYPE_SOP_STREAM2:
    case SVGA3D_QUERYTYPE_SOP_STREAM3:
        svga_result->soPred.overflowed = query_read_u32(src);
        break;
    case SVGA3D_QUERYTYPE_OCCLUSION64:
        svga_result->occ64.samplesRendered = query_read_u64(src);
        break;
    default:
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    *svga_result_size = info.svga_result_size;

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_predication_plan(
    bool enabled, SVGA3dQueryType type, uint32_t flags,
    uint32_t predicate_value, VMSVGA3DD3D10PredicationPlan *plan)
{
    VMSVGA3DD3D10QueryInfo info;
    VMSVGA3DD3D10Level level;

    if (plan == NULL) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(plan, 0, sizeof(*plan));
    plan->predicate_value = predicate_value != 0;
    if (!enabled) {
        /* Current VirtualBox forwards predicateValue even with a NULL predicate. */
        return VMSVGA3D_D3D10_LEVEL_10_0;
    }

    level = vmsvga3d_d3d10_query_info(type, flags, &info);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return level;
    }

    plan->enabled = true;
    /* Current VirtualBox binds the query's existing backend object. Predicate
     * creation moved to DEFINE_QUERY; SET_PREDICATION must not replace it.
     */
    plan->release_existing_query = false;
    plan->create_predicate = false;
    plan->d3d_query = info.d3d_query;
    plan->misc_flags = info.predicate_hint
                     ? VMSVGA3D_D3D10_QUERY_MISC_PREDICATEHINT : 0;

    /* The backend only asserts predicate compatibility; do not add a guest
     * validation here that VirtualBox release builds do not perform.
     */
    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_srv_desc(
    const SVGACOTableDXSRViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D10SRVDesc *dst)
{
    VMSVGA3DD3D10Format format;
    VMSVGA3DD3D10Level level;

    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    format = vmsvga3d_d3d10_surface_format(src->format);
    if (format.min_level == VMSVGA3D_D3D10_LEVEL_INVALID && src->format != SVGA3D_BUFFER) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    dst->format = format.dxgi_format;
    level = format.min_level == VMSVGA3D_D3D10_LEVEL_INVALID
          ? VMSVGA3D_D3D10_LEVEL_10_0 : format.min_level;

    switch (src->resourceDimension) {
    case SVGA3D_RESOURCE_BUFFER:
        dst->view_dimension = D3D10_SRV_BUFFER;
        dst->first_element = src->desc.buffer.firstElement;
        dst->num_elements = src->desc.buffer.numElements;
        break;
    case SVGA3D_RESOURCE_TEXTURE1D:
        dst->view_dimension = array_elements <= 1 ? D3D10_SRV_TEXTURE1D : D3D10_SRV_TEXTURE1DARRAY;
        dst->most_detailed_mip = src->desc.tex.mostDetailedMip;
        dst->mip_levels = src->desc.tex.mipLevels;
        if (array_elements > 1) {
            dst->first_array_slice = src->desc.tex.firstArraySlice;
            dst->array_size = src->desc.tex.arraySize;
        }
        break;
    case SVGA3D_RESOURCE_TEXTURE2D:
        if (multisample_count > 1) {
            if (array_elements <= 1) {
                dst->view_dimension = D3D10_SRV_TEXTURE2DMS;
            } else {
                dst->view_dimension = D3D10_SRV_TEXTURE2DMSARRAY;
                dst->first_array_slice = src->desc.tex.firstArraySlice;
                dst->array_size = src->desc.tex.arraySize;
            }
        } else {
            if (array_elements <= 1) {
                dst->view_dimension = D3D10_SRV_TEXTURE2D;
            } else {
                dst->view_dimension = D3D10_SRV_TEXTURE2DARRAY;
                dst->first_array_slice = src->desc.tex.firstArraySlice;
                dst->array_size = src->desc.tex.arraySize;
            }
            dst->most_detailed_mip = src->desc.tex.mostDetailedMip;
            dst->mip_levels = src->desc.tex.mipLevels;
        }
        break;
    case SVGA3D_RESOURCE_TEXTURE3D:
        dst->view_dimension = D3D10_SRV_TEXTURE3D;
        dst->most_detailed_mip = src->desc.tex.mostDetailedMip;
        dst->mip_levels = src->desc.tex.mipLevels;
        break;
    case SVGA3D_RESOURCE_TEXTURECUBE:
        dst->most_detailed_mip = src->desc.tex.mostDetailedMip;
        dst->mip_levels = src->desc.tex.mipLevels;
        if (array_elements <= 6) {
            dst->view_dimension = D3D10_SRV_TEXTURECUBE;
        } else {
            dst->view_dimension = D3D10_1_SRV_TEXTURECUBEARRAY;
            dst->first_array_slice = src->desc.tex.firstArraySlice;
            dst->array_size = src->desc.tex.arraySize / 6;
            level = max_level(level, VMSVGA3D_D3D10_LEVEL_10_1);
        }
        break;
    case SVGA3D_RESOURCE_BUFFEREX:
        return VMSVGA3D_D3D10_LEVEL_11_0;
    default:
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_rtv_desc(
    const SVGACOTableDXRTViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D10RTVDesc *dst)
{
    VMSVGA3DD3D10Format format;
    VMSVGA3DD3D10Level level;

    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    format = vmsvga3d_d3d10_surface_format(src->format);
    if (format.min_level == VMSVGA3D_D3D10_LEVEL_INVALID && src->format != SVGA3D_BUFFER) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    dst->format = format.dxgi_format;
    level = format.min_level == VMSVGA3D_D3D10_LEVEL_INVALID
          ? VMSVGA3D_D3D10_LEVEL_10_0 : format.min_level;

    switch (src->resourceDimension) {
    case SVGA3D_RESOURCE_BUFFER:
        dst->view_dimension = D3D10_RTV_BUFFER;
        dst->first_element = src->desc.buffer.firstElement;
        dst->num_elements = src->desc.buffer.numElements;
        break;
    case SVGA3D_RESOURCE_TEXTURE1D:
        dst->view_dimension = array_elements <= 1 ? D3D10_RTV_TEXTURE1D : D3D10_RTV_TEXTURE1DARRAY;
        dst->mip_slice = src->desc.tex.mipSlice;
        if (array_elements > 1) {
            dst->first_array_slice = src->desc.tex.firstArraySlice;
            dst->array_size = src->desc.tex.arraySize;
        }
        break;
    case SVGA3D_RESOURCE_TEXTURE2D:
        if (multisample_count > 1) {
            if (array_elements <= 1) {
                dst->view_dimension = D3D10_RTV_TEXTURE2DMS;
            } else {
                dst->view_dimension = D3D10_RTV_TEXTURE2DMSARRAY;
                dst->first_array_slice = src->desc.tex.firstArraySlice;
                dst->array_size = src->desc.tex.arraySize;
            }
        } else {
            if (array_elements <= 1) {
                dst->view_dimension = D3D10_RTV_TEXTURE2D;
            } else {
                dst->view_dimension = D3D10_RTV_TEXTURE2DARRAY;
                dst->first_array_slice = src->desc.tex.firstArraySlice;
                dst->array_size = src->desc.tex.arraySize;
            }
            dst->mip_slice = src->desc.tex.mipSlice;
        }
        break;
    case SVGA3D_RESOURCE_TEXTURE3D:
        dst->view_dimension = D3D10_RTV_TEXTURE3D;
        dst->mip_slice = src->desc.tex3D.mipSlice;
        dst->first_w_slice = src->desc.tex3D.firstW;
        dst->w_size = src->desc.tex3D.wSize;
        break;
    case SVGA3D_RESOURCE_TEXTURECUBE:
        dst->view_dimension = D3D10_RTV_TEXTURE2DARRAY;
        dst->mip_slice = src->desc.tex.mipSlice;
        dst->first_array_slice = 0;
        dst->array_size = 6;
        break;
    case SVGA3D_RESOURCE_BUFFEREX:
        return VMSVGA3D_D3D10_LEVEL_11_0;
    default:
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_dsv_desc(
    const SVGACOTableDXDSViewEntry *src, uint32_t array_elements,
    uint32_t multisample_count, VMSVGA3DD3D10DSVDesc *dst)
{
    VMSVGA3DD3D10Format format;
    VMSVGA3DD3D10Level level;

    if (!src || !dst) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    memset(dst, 0, sizeof(*dst));

    format = vmsvga3d_d3d10_surface_format(src->format);
    if (format.min_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    dst->format = format.dxgi_format;
    dst->flags = src->flags;
    level = format.min_level;
    if (src->flags) {
        level = max_level(level, VMSVGA3D_D3D10_LEVEL_11_0);
    }

    switch (src->resourceDimension) {
    case SVGA3D_RESOURCE_TEXTURE1D:
        dst->view_dimension = array_elements <= 1 ? D3D10_DSV_TEXTURE1D : D3D10_DSV_TEXTURE1DARRAY;
        dst->mip_slice = src->mipSlice;
        if (array_elements > 1) {
            dst->first_array_slice = src->firstArraySlice;
            dst->array_size = src->arraySize;
        }
        break;
    case SVGA3D_RESOURCE_TEXTURE2D:
        if (multisample_count > 1) {
            if (array_elements <= 1) {
                dst->view_dimension = D3D10_DSV_TEXTURE2DMS;
            } else {
                dst->view_dimension = D3D10_DSV_TEXTURE2DMSARRAY;
                dst->first_array_slice = src->firstArraySlice;
                dst->array_size = src->arraySize;
            }
        } else {
            if (array_elements <= 1) {
                dst->view_dimension = D3D10_DSV_TEXTURE2D;
            } else {
                dst->view_dimension = D3D10_DSV_TEXTURE2DARRAY;
                dst->first_array_slice = src->firstArraySlice;
                dst->array_size = src->arraySize;
            }
            dst->mip_slice = src->mipSlice;
        }
        break;
    default:
        return VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    return level;
}

#ifdef VMSVGA3D_D3D10_RUNTIME_INTEGRATION

/*
 * Live vGPU10 command-buffer boundary. Generic framing, MOB/COTable backing,
 * and completion remain in vmware_vga_3d.c; D3D10 packet decoding and
 * protocol semantics belong here.
 */
#if defined(__GNUC__)
#define VMSVGA3D_D3D10_LIVE_UNUSED __attribute__((unused))
#else
#define VMSVGA3D_D3D10_LIVE_UNUSED
#endif

static bool vmsvga3d_d3d10_level_is_vgpu10(VMSVGA3DD3D10Level level)
{
    return level >= VMSVGA3D_D3D10_LEVEL_10_0 &&
           level <= VMSVGA3D_D3D10_LEVEL_10_1;
}

static bool vmsvga3d_d3d10_input_layout_realize_live(
    struct vmsvga_state_s *s, uint32_t cid,
    SVGA3dElementLayoutId layout_id, uint32_t shader_id)
{
    SVGACOTableDXElementLayoutEntry *entry;
    VMSVGA3DD3D10InputElement elements[32];
    VMSVGA3DD3D10Level level;
    uint32_t i;

    if (s == NULL || shader_id == SVGA3D_INVALID_ID) {
        return false;
    }

    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_ELEMENTLAYOUT, layout_id);
    if (entry == NULL ||
        entry->numDescs > sizeof(elements) / sizeof(elements[0])) {
        return false;
    }

    for (i = 0; i < entry->numDescs; i++) {
        level = vmsvga3d_d3d10_input_element(&entry->descs[i], &elements[i]);
        if (!vmsvga3d_d3d10_level_is_vgpu10(level)) {
            return false;
        }
    }

    return vmsvga3d_dxvk_d3d11_input_layout_ensure(
        s->dxvk, cid, layout_id, shader_id,
        entry->numDescs != 0 ? elements : NULL, entry->numDescs);
}

static bool vmsvga3d_d3d10_shader_bind_live(
    struct vmsvga_state_s *s, const SVGA3dCmdDXBindShader *command)
{
    VMSVGA3DMob *mob;
    SVGACOTableDXShaderEntry *entry;
    VMSVGA3DD3D10ShaderInfo info;
    VMSVGA3DD3D10Level level;
    void *blob = NULL;
    uint32_t mobid;
    bool success = false;

    if (s == NULL || command == NULL) {
        return false;
    }

    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, command->cid, SVGA_COTABLE_DXSHADER, command->shid);
    if (entry == NULL) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, command->mobid);
    mobid = mob != NULL ? command->mobid : SVGA3D_INVALID_ID;

    if (vmsvga3d_d3d10_shader_bind_entry(entry, mobid,
                                         command->offsetInBytes) ==
        VMSVGA3D_D3D10_LEVEL_INVALID) {
        return false;
    }

    /* VirtualBox updates the COTable even for an invalid MOB and does not call
     * the backend in that case, leaving the previous backend shader untouched.
     */
    if (mob == NULL) {
        return true;
    }

    if (entry->offsetInBytes >= mob->gbo.size ||
        entry->sizeInBytes > mob->gbo.size - entry->offsetInBytes) {
        return false;
    }

    blob = malloc(entry->sizeInBytes);
    if (blob == NULL ||
        !vmsvga3d_mob_read(s, mob, entry->offsetInBytes, blob,
                           entry->sizeInBytes)) {
        free(blob);
        return false;
    }

    memset(&info, 0, sizeof(info));

    level = vmsvga3d_d3d10_shader_parse(blob, entry->sizeInBytes, &info);
    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        goto out;
    }

    success = vmsvga3d_dxvk_d3d11_shader_bind_info(
        s->dxvk, command->cid, command->shid, &info);

out:
    vmsvga3d_d3d10_shader_release(&info);

    free(blob);
    return success;
}

static bool vmsvga3d_d3d10_blend_state_realize_live(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t state_id,
    const SVGACOTableDXBlendStateEntry *entry)
{
    VMSVGA3DD3D10BlendDesc desc;
    VMSVGA3DD3D10Level level;

    if (s == NULL || entry == NULL) {
        return false;
    }

    level = vmsvga3d_d3d10_blend_state(entry, &desc);

    return vmsvga3d_d3d10_level_is_vgpu10(level) &&
           vmsvga3d_dxvk_d3d11_blend_state_define(
               s->dxvk, cid, state_id, &desc);
}

static bool vmsvga3d_d3d10_depth_stencil_state_realize_live(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t state_id,
    const SVGACOTableDXDepthStencilEntry *entry)
{
    VMSVGA3DD3D10DepthStencilDesc desc;
    VMSVGA3DD3D10Level level;

    if (s == NULL || entry == NULL) {
        return false;
    }

    level = vmsvga3d_d3d10_depth_stencil_state(entry, &desc);

    return vmsvga3d_d3d10_level_is_vgpu10(level) &&
           vmsvga3d_dxvk_d3d11_depth_stencil_state_define(
               s->dxvk, cid, state_id, &desc);
}

static bool vmsvga3d_d3d10_rasterizer_state_realize_live(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t state_id,
    const SVGACOTableDXRasterizerStateEntry *entry)
{
    VMSVGA3DD3D10RasterizerDesc desc;
    VMSVGA3DD3D10Level level;

    if (s == NULL || entry == NULL) {
        return false;
    }

    level = vmsvga3d_d3d10_rasterizer_state(entry, &desc);

    return vmsvga3d_d3d10_level_is_vgpu10(level) &&
           vmsvga3d_dxvk_d3d11_rasterizer_state_define(
               s->dxvk, cid, state_id, &desc);
}

static bool vmsvga3d_d3d10_sampler_state_realize_live(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t state_id,
    const SVGACOTableDXSamplerEntry *entry)
{
    VMSVGA3DD3D10SamplerDesc desc;
    VMSVGA3DD3D10Level level;

    if (s == NULL || entry == NULL) {
        return false;
    }

    level = vmsvga3d_d3d10_sampler_state(entry, &desc);

    return vmsvga3d_d3d10_level_is_vgpu10(level) &&
           vmsvga3d_dxvk_d3d11_sampler_state_define(
               s->dxvk, cid, state_id, &desc);
}

static bool vmsvga3d_d3d10_buffer_materialize_live(
    struct vmsvga_state_s *s, SVGA3dSurfaceId sid);
static bool vmsvga3d_d3d10_rtv_realize_live(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dRenderTargetViewId view_id);
static bool vmsvga3d_d3d10_dsv_realize_live(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dDepthStencilViewId view_id);
static bool vmsvga3d_d3d10_srv_realize_live(
    struct vmsvga_state_s *s, uint32_t cid,
    SVGA3dShaderResourceViewId view_id);
static bool vmsvga3d_d3d10_stream_output_invalidate_bound_gs_live(
    struct vmsvga_state_s *s, VMSVGA3DDXContext *context, uint32_t cid);

/*
 * First part of VirtualBox dxSetupPipeline: make every currently referenced
 * resource/view resident before state or shader setup.  Failures are kept
 * local to the ensure operation because the Oracle's setup function is void.
 */
static void vmsvga3d_d3d10_pipeline_resources_views_ensure_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    uint32_t stage;
    uint32_t slot;

    if (context == NULL) {
        return;
    }

    if (context->shadow.renderState.depthStencilViewId != SVGA3D_INVALID_ID) {
        (void)vmsvga3d_d3d10_dsv_realize_live(
            s, cid, context->shadow.renderState.depthStencilViewId);
    }

    for (slot = 0; slot < SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS; slot++) {
        uint32_t id = context->shadow.renderState.renderTargetViewIds[slot];

        if (id != SVGA3D_INVALID_ID) {
            (void)vmsvga3d_d3d10_rtv_realize_live(s, cid, id);
        }
    }

    for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE; stage++) {
        for (slot = 0; slot < context->shader_resource_max_bound[stage]; slot++) {
            uint32_t id = context->shadow.shaderState[stage].shaderResources[slot];

            if (id != SVGA3D_INVALID_ID) {
                (void)vmsvga3d_d3d10_srv_realize_live(s, cid, id);
            }
        }
    }

    for (slot = 0; slot < context->vertex_buffer_max_bound; slot++) {
        uint32_t sid = context->shadow.inputAssembly.vertexBuffers[slot].bufferId;

        if (sid != SVGA3D_INVALID_ID) {
            (void)vmsvga3d_d3d10_buffer_materialize_live(s, sid);
        }
    }

    if (context->shadow.inputAssembly.indexBufferSid != SVGA3D_INVALID_ID) {
        (void)vmsvga3d_d3d10_buffer_materialize_live(
            s, context->shadow.inputAssembly.indexBufferSid);
    }
}

static void vmsvga3d_d3d10_pipeline_output_targets_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

    if (context == NULL ||
        (context->renderer_dirty & VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET) == 0) {
        return;
    }

    /* VirtualBox clears the dirty bit before calling dxBindRenderTargetViews
     * and does not restore it if the backend operation fails.
     */
    context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET;
    (void)vmsvga3d_dxvk_d3d11_set_render_targets(
        s->dxvk, cid, context->render_target_count,
        context->shadow.renderState.renderTargetViewIds,
        context->shadow.renderState.depthStencilViewId,
        context->shadow.uavSpliceIndex);
}


static void vmsvga3d_d3d10_pipeline_constant_buffers_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    uint32_t stage;

    if (context == NULL) {
        return;
    }

    for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE_DX10; stage++) {
        uint32_t start_slot = context->constant_buffer_start_slot[stage];
        uint32_t count = context->constant_buffer_num_buffers[stage];

        if (count == 0) {
            continue;
        }

        /* The SVGA protocol shadows 16 constant-buffer slots, while the native
         * D3D10/D3D11 API exposes only 14.  VirtualBox preserves slots 14-15 in
         * the context MOB but clips the backend SetConstantBuffers call here.
         */
        if (start_slot >= 14u) {
            count = 0;
        } else if (count > 14u - start_slot) {
            count = 14u - start_slot;
        }
        if (count != 0) {
            (void)vmsvga3d_dxvk_d3d11_set_constant_buffers(
                s->dxvk, cid, stage, start_slot, count);
        }

        /* VirtualBox clears the pending range after the void D3D call even if
         * the backend/runtime reports a problem through debug diagnostics.
         */
        context->constant_buffer_start_slot[stage] = 0;
        context->constant_buffer_num_buffers[stage] = 0;
    }
}

static void vmsvga3d_d3d10_pipeline_vertex_buffers_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    VMSVGA3DDxvkSurface *surfaces[SVGA3D_DX_MAX_VERTEXBUFFERS] = { NULL };
    uint32_t strides[SVGA3D_DX_MAX_VERTEXBUFFERS] = { 0 };
    uint32_t offsets[SVGA3D_DX_MAX_VERTEXBUFFERS] = { 0 };
    uint32_t start_slot = 0;
    uint32_t count = 0;
    uint32_t slot;

    if (context == NULL ||
        (context->renderer_dirty & VMSVGA3D_DX_CTX_F_STATE_VERTEXBUFFER) == 0) {
        return;
    }

    /* The Oracle clears the context dirty flag before dxSetVertexBuffers; the
     * per-slot modified mask survives until dxPostDraw.
     */
    context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_VERTEXBUFFER;
    for (slot = 0; slot < context->vertex_buffer_max_bound; slot++) {
        if ((context->vertex_buffer_modified & (UINT64_C(1) << slot)) != 0) {
            const SVGA3dBufferBinding *binding =
                &context->shadow.inputAssembly.vertexBuffers[slot];

            if (binding->bufferId != SVGA3D_INVALID_ID && s->svga3d != NULL &&
                binding->bufferId < SVGA3D_MAX_SURFACE_IDS) {
                VMSVGA3DSurface *surface = s->svga3d->surfaces[binding->bufferId];

                if (surface != NULL && surface->dxvk_surface != NULL &&
                    surface->mips != NULL && surface->mip_count != 0 &&
                    vmsvga3d_d3d10_buffer_materialize_live(s, binding->bufferId)) {
                    VMSVGA3DD3D10VertexBufferPipelineBinding pipeline_binding;

                    surfaces[slot] = surface->dxvk_surface;
                    (void)vmsvga3d_d3d10_vertex_buffer_pipeline_binding(
                        true, surface->mips[0].data_size, binding->stride,
                        binding->offset, &pipeline_binding);
                    strides[slot] = pipeline_binding.stride;
                    offsets[slot] = pipeline_binding.offset;
                }
            }

            if (count == 0) {
                start_slot = slot;
            }
            count++;
        } else if (count != 0) {
            (void)vmsvga3d_dxvk_d3d11_set_vertex_buffers(
                s->dxvk, start_slot, count, &surfaces[start_slot],
                &strides[start_slot], &offsets[start_slot]);
            count = 0;
        }
    }
    if (count != 0) {
        (void)vmsvga3d_dxvk_d3d11_set_vertex_buffers(
            s->dxvk, start_slot, count, &surfaces[start_slot],
            &strides[start_slot], &offsets[start_slot]);
    }
}

static void vmsvga3d_d3d10_pipeline_index_buffer_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    VMSVGA3DDxvkSurface *surface_binding = NULL;
    uint32_t dxgi_format = 0;
    uint32_t bytes_per_index = 0;
    uint32_t offset = 0;

    if (context == NULL ||
        (context->renderer_dirty & VMSVGA3D_DX_CTX_F_STATE_INDEXBUFFER) == 0) {
        return;
    }

    context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_INDEXBUFFER;
    if (context->shadow.inputAssembly.indexBufferSid != SVGA3D_INVALID_ID &&
        vmsvga3d_d3d10_index_format(
            (SVGA3dSurfaceFormat)context->shadow.inputAssembly.indexBufferFormat,
            &dxgi_format, &bytes_per_index) != VMSVGA3D_D3D10_LEVEL_INVALID &&
        s->svga3d != NULL &&
        context->shadow.inputAssembly.indexBufferSid < SVGA3D_MAX_SURFACE_IDS) {
        VMSVGA3DSurface *surface =
            s->svga3d->surfaces[context->shadow.inputAssembly.indexBufferSid];

        if (surface != NULL && surface->dxvk_surface != NULL &&
            vmsvga3d_d3d10_buffer_materialize_live(
                s, context->shadow.inputAssembly.indexBufferSid)) {
            surface_binding = surface->dxvk_surface;
            offset = context->shadow.inputAssembly.indexBufferOffset;
        } else {
            dxgi_format = 0;
        }
    }

    /* Invalid index formats are converted to a NULL/UNKNOWN binding at draw
     * setup instead of being rejected by DX_SET_INDEX_BUFFER.
     */
    (void)bytes_per_index;
    (void)vmsvga3d_dxvk_d3d11_set_index_buffer(
        s->dxvk, surface_binding, dxgi_format, offset);
}

/*
 * State-object part of the deferred dxSetupPipeline path.  Draw execution is
 * still capability-gated, but keep the already implemented realization code
 * connected to its final pipeline location so intermediate batches build as a
 * coherent implementation rather than accumulating dead helpers.
 */
static void vmsvga3d_d3d10_pipeline_state_realize_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    uint32_t stage;
    uint32_t slot;

    if (context == NULL) {
        return;
    }

    /* Current VirtualBox DX_STATE_TRACKER order: topology, blend, depth,
     * viewports, scissors, rasterizer, then one full sampler table per dirty
     * shader stage.  Dirty bits are cleared before the backend operation.
     */
    if ((context->renderer_dirty & VMSVGA3D_DX_CTX_F_STATE_TOPOLOGY) != 0) {
        uint32_t topology = 0;

        context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_TOPOLOGY;
        if (vmsvga3d_d3d10_primitive_topology(
                context->shadow.inputAssembly.topology, &topology) !=
            VMSVGA3D_D3D10_LEVEL_INVALID) {
            (void)vmsvga3d_dxvk_d3d11_set_primitive_topology(s->dxvk, topology);
        }
    }

    if ((context->renderer_dirty & VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE) != 0) {
        uint32_t id = context->shadow.renderState.blendStateId;
        bool realized = id == SVGA3D_INVALID_ID;
        float blend_factor[4];

        memcpy(blend_factor, context->shadow.renderState.blendFactor,
               sizeof(blend_factor));
        context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE;
        if (id != SVGA3D_INVALID_ID) {
            SVGACOTableDXBlendStateEntry *entry = vmsvga3d_dx_cotable_entry_ptr(
                s, cid, SVGA_COTABLE_BLENDSTATE, id);

            if (entry != NULL) {
                realized = vmsvga3d_d3d10_blend_state_realize_live(
                    s, cid, id, entry);
            }
        }
        (void)vmsvga3d_dxvk_d3d11_set_blend_state(
            s->dxvk, cid, realized ? id : SVGA3D_INVALID_ID,
            realized && id != SVGA3D_INVALID_ID ? blend_factor : NULL,
            realized && id != SVGA3D_INVALID_ID
                ? context->shadow.renderState.sampleMask
                : 0);
    }

    if ((context->renderer_dirty &
         VMSVGA3D_DX_CTX_F_STATE_DEPTHSTENCILSTATE) != 0) {
        uint32_t id = context->shadow.renderState.depthStencilStateId;
        bool realized = id == SVGA3D_INVALID_ID;

        context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_DEPTHSTENCILSTATE;
        if (id != SVGA3D_INVALID_ID) {
            SVGACOTableDXDepthStencilEntry *entry = vmsvga3d_dx_cotable_entry_ptr(
                s, cid, SVGA_COTABLE_DEPTHSTENCIL, id);

            if (entry != NULL) {
                realized = vmsvga3d_d3d10_depth_stencil_state_realize_live(
                    s, cid, id, entry);
            }
        }
        (void)vmsvga3d_dxvk_d3d11_set_depth_stencil_state(
            s->dxvk, cid, realized ? id : SVGA3D_INVALID_ID,
            realized && id != SVGA3D_INVALID_ID
                ? context->shadow.renderState.stencilRef
                : 0);
    }

    if ((context->renderer_dirty & VMSVGA3D_DX_CTX_F_STATE_VIEWPORT) != 0) {
        uint32_t count = MIN((uint32_t)context->shadow.numViewports,
                             (uint32_t)SVGA3D_DX_MAX_VIEWPORTS);

        context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_VIEWPORT;
        (void)vmsvga3d_dxvk_d3d11_set_viewports(
            s->dxvk, count, count != 0 ? context->shadow.viewports : NULL);
    }

    if ((context->renderer_dirty & VMSVGA3D_DX_CTX_F_STATE_SCISSORRECT) != 0) {
        uint32_t count = MIN((uint32_t)context->shadow.numScissorRects,
                             (uint32_t)SVGA3D_DX_MAX_SCISSORRECTS);

        context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_SCISSORRECT;
        (void)vmsvga3d_dxvk_d3d11_set_scissor_rects(
            s->dxvk, count, count != 0 ? context->shadow.scissorRects : NULL);
    }

    if ((context->renderer_dirty &
         VMSVGA3D_DX_CTX_F_STATE_RASTERIZERSTATE) != 0) {
        uint32_t id = context->shadow.renderState.rasterizerStateId;
        bool realized = id == SVGA3D_INVALID_ID;

        context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_RASTERIZERSTATE;
        if (id != SVGA3D_INVALID_ID) {
            SVGACOTableDXRasterizerStateEntry *entry = vmsvga3d_dx_cotable_entry_ptr(
                s, cid, SVGA_COTABLE_RASTERIZERSTATE, id);

            if (entry != NULL) {
                realized = vmsvga3d_d3d10_rasterizer_state_realize_live(
                    s, cid, id, entry);
            }
        }
        (void)vmsvga3d_dxvk_d3d11_set_rasterizer_state(
            s->dxvk, cid, realized ? id : SVGA3D_INVALID_ID);
    }

    for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE; stage++) {
        uint64_t flag = VMSVGA3D_DX_CTX_F_STATE_SAMPLER_VS << stage;

        if ((context->renderer_dirty & flag) == 0) {
            continue;
        }
        context->renderer_dirty &= ~flag;
        for (slot = 0; slot < SVGA3D_DX_MAX_SAMPLERS; slot++) {
            uint32_t id = context->shadow.shaderState[stage].samplers[slot];
            SVGACOTableDXSamplerEntry *entry;

            if (id == SVGA3D_INVALID_ID) {
                continue;
            }
            entry = vmsvga3d_dx_cotable_entry_ptr(
                s, cid, SVGA_COTABLE_SAMPLER, id);
            if (entry != NULL) {
                (void)vmsvga3d_d3d10_sampler_state_realize_live(
                    s, cid, id, entry);
            }
        }
        if (stage < SVGA3D_NUM_SHADERTYPE_DX10) {
            (void)vmsvga3d_dxvk_d3d11_set_samplers(
                s->dxvk, cid, stage, 0, SVGA3D_DX_MAX_SAMPLERS,
                context->shadow.shaderState[stage].samplers);
        }
    }
}

static bool vmsvga3d_d3d10_surface_info_live(
    const VMSVGA3DSurface *surface, VMSVGA3DD3D10SurfaceInfo *info);

static bool vmsvga3d_d3d10_shader_prepare_ps_live(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t stage,
    VMSVGA3DD3D10ShaderInfo *info)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    VMSVGA3DD3D10ShaderResourceBinding
        bindings[SVGA3D_DX_MAX_SRVIEWS];
    uint32_t binding_count = 0;
    uint32_t slot;

    if (context == NULL || info == NULL || s->svga3d == NULL) {
        return false;
    }

    /* Match dxSetupPipeline's PS create-time resource patching.  The Oracle
     * zero-initializes the full arrays, records the highest valid SRV slot, and
     * leaves holes as UNKNOWN/NONE so DXShaderUpdateResources preserves those
     * declarations.
     */
    memset(bindings, 0, sizeof(bindings));

    for (slot = 0; slot < SVGA3D_DX_MAX_SRVIEWS; slot++) {
        uint32_t view_id = context->shadow.shaderState[stage].shaderResources[slot];
        SVGACOTableDXSRViewEntry *entry;
        VMSVGA3DSurface *surface;
        VMSVGA3DD3D10SurfaceInfo surface_info;

        if (view_id == SVGA3D_INVALID_ID) {
            continue;
        }
        entry = vmsvga3d_dx_cotable_entry_ptr(
            s, cid, SVGA_COTABLE_SRVIEW, view_id);
        if (entry == NULL) {
            continue;
        }
        if (entry->sid == SVGA3D_INVALID_ID ||
            entry->sid >= SVGA3D_MAX_SURFACE_IDS) {
            return false;
        }
        surface = s->svga3d->surfaces[entry->sid];
        if (surface == NULL ||
            !vmsvga3d_d3d10_surface_info_live(surface, &surface_info)) {
            return false;
        }

        (void)vmsvga3d_d3d10_shader_resource_binding(
            entry, surface_info.array_elements, surface_info.multisample_count,
            &bindings[slot]);
        binding_count = slot + 1;
    }

    /* VirtualBox asserts this helper's rc but deliberately continues shader
     * creation because an unpatched declaration will often still work.
     */
    (void)vmsvga3d_d3d10_shader_update_resources(
        info, bindings, binding_count);

    /* PS output component types come from bound render-target formats, not from
     * the SRV formats above.  Invalid/unbound RTV slots leave the signature as
     * parsed, exactly like the Oracle.
     */
    for (slot = 0;
         slot < SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS &&
         slot < info->output_signature_count;
         slot++) {
        uint32_t view_id = context->shadow.renderState.renderTargetViewIds[slot];
        SVGACOTableDXRTViewEntry *entry;

        if (view_id == SVGA3D_INVALID_ID) {
            continue;
        }
        entry = vmsvga3d_dx_cotable_entry_ptr(
            s, cid, SVGA_COTABLE_RTVIEW, view_id);
        if (entry != NULL) {
            info->output_signature[slot].componentType =
                vmsvga3d_d3d10_shader_component_type_from_format(entry->format);
        }
    }

    return true;
}

static const VMSVGA3DD3D10ShaderInfo *
vmsvga3d_d3d10_bound_shader_info_live(
    struct vmsvga_state_s *s, VMSVGA3DDXContext *context, uint32_t cid,
    SVGA3dShaderType type)
{
    uint32_t stage = (uint32_t)type - (uint32_t)SVGA3D_SHADERTYPE_MIN;
    uint32_t shader_id;
    const VMSVGA3DD3D10ShaderInfo *info = NULL;

    if (stage >= SVGA3D_NUM_SHADERTYPE) {
        return NULL;
    }

    shader_id = context->shadow.shaderState[stage].shaderId;

    if (shader_id == SVGA3D_INVALID_ID ||
        !vmsvga3d_dxvk_d3d11_shader_info(
            s->dxvk, cid, shader_id, type, &info)) {
        return NULL;
    }

    return info;
}

static bool vmsvga3d_d3d10_stream_output_prepare_live(
    struct vmsvga_state_s *s, VMSVGA3DDXContext *context, uint32_t cid,
    VMSVGA3DD3D10ShaderInfo *gs_info, uint32_t *stream_output_id,
    VMSVGA3DD3D10StreamOutputPlan *stream_output)
{
    SVGACOTableDXStreamOutputEntry *entry;
    SVGA3dStreamOutputDeclarationEntry mob_decls[SVGA3D_MAX_STREAMOUT_DECLS];
    const SVGA3dStreamOutputDeclarationEntry *declarations;
    VMSVGA3DD3D10ShaderOutputSemantic
        outputs[VMSVGA3D_D3D10_MAX_SHADER_SIGNATURES];
    uint32_t output_count = 0;
    uint32_t soid = context->shadow.streamOut.soid;

    *stream_output_id = SVGA3D_INVALID_ID;

    memset(stream_output, 0, sizeof(*stream_output));

    if (soid == SVGA3D_INVALID_ID) {
        return true;
    }

    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_STREAMOUTPUT, soid);
    if (entry == NULL ||
        entry->numOutputStreamEntries > SVGA3D_MAX_STREAMOUT_DECLS ||
        entry->numOutputStreamStrides > SVGA3D_DX_MAX_SOTARGETS ||
        (entry->rasterizedStream >= SVGA3D_DX_MAX_SOTARGETS &&
         entry->rasterizedStream != SVGA3D_DX_SO_NO_RASTERIZED_STREAM)) {
        return false;
    }

    if (vmsvga3d_dxvk_d3d11_stream_output_cached(
            s->dxvk, cid, soid, stream_output)) {
        *stream_output_id = soid;
        return true;
    }

    if (entry->usesMob) {
        VMSVGA3DMob *mob = vmsvga3d_mob_get(s, entry->mobid);
        uint32_t bytes;

        if (mob == NULL ||
            entry->numOutputStreamEntries > ARRAY_SIZE(mob_decls)) {
            return false;
        }

        bytes = entry->numOutputStreamEntries * sizeof(mob_decls[0]);

        if (entry->offsetInBytes >= mob->gbo.size ||
            (uint64_t)bytes > mob->gbo.size - entry->offsetInBytes ||
            !vmsvga3d_mob_read(
                s, mob, entry->offsetInBytes, mob_decls, bytes)) {
            return false;
        }

        declarations = mob_decls;
    } else {
        declarations = entry->decl;
    }

    if (vmsvga3d_d3d10_shader_output_semantics(
            gs_info, outputs, ARRAY_SIZE(outputs), &output_count) ==
            VMSVGA3D_D3D10_LEVEL_INVALID ||
        vmsvga3d_d3d10_stream_output_plan(
            entry, declarations, outputs, output_count, stream_output) ==
            VMSVGA3D_D3D10_LEVEL_INVALID ||
        !vmsvga3d_dxvk_d3d11_stream_output_cache(
            s->dxvk, cid, soid, stream_output)) {
        return false;
    }

    *stream_output_id = soid;

    return true;
}

static void vmsvga3d_d3d10_pipeline_shaders_setup_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    uint32_t stage;

    if (context == NULL) {
        return;
    }

    /* VirtualBox visits every shader stage on every dxSetupPipeline call and
     * calls *SetShader for every supported stage, including NULL unbinds.  The
     * vGPU10 backend currently implements the VS/PS/GS subset; later D3D11
     * batches extend this same path to HS/DS/CS.
     */
    for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE; stage++) {
        uint32_t shader_type = stage + SVGA3D_SHADERTYPE_MIN;
        uint32_t shader_id = context->shadow.shaderState[stage].shaderId;
        uint32_t defined_type;
        uint32_t stream_output_id = SVGA3D_INVALID_ID;
        VMSVGA3DD3D10StreamOutputPlan stream_output;
        const VMSVGA3DD3D10StreamOutputPlan *stream_output_ptr = NULL;
        bool prepared = true;
        bool realized = false;

        memset(&stream_output, 0, sizeof(stream_output));

        if (shader_type != SVGA3D_SHADERTYPE_VS &&
            shader_type != SVGA3D_SHADERTYPE_PS &&
            shader_type != SVGA3D_SHADERTYPE_GS) {
            continue;
        }

        if (shader_id == SVGA3D_INVALID_ID) {
            /* dxShaderSet explicitly unbinds inactive stages on every setup. */
            (void)vmsvga3d_dxvk_d3d11_shader_set(
                s->dxvk, cid, shader_id, shader_type);
            continue;
        }

        if (!vmsvga3d_dxvk_d3d11_shader_object_exists(
                s->dxvk, cid, shader_id, &defined_type) ||
            defined_type != shader_type) {
            continue;
        }

        if (shader_type == SVGA3D_SHADERTYPE_PS) {
            VMSVGA3DD3D10ShaderInfo *info = NULL;

            prepared = vmsvga3d_dxvk_d3d11_shader_info_for_realize(
                s->dxvk, cid, shader_id, shader_type, &info);
            if (prepared && info != NULL) {
                const VMSVGA3DD3D10ShaderInfo *vs =
                    vmsvga3d_d3d10_bound_shader_info_live(
                        s, context, cid, SVGA3D_SHADERTYPE_VS);
                const VMSVGA3DD3D10ShaderInfo *gs =
                    vmsvga3d_d3d10_bound_shader_info_live(
                        s, context, cid, SVGA3D_SHADERTYPE_GS);

                /* The Oracle patches PS resource/output types first, then matches
                 * generic PS inputs to the prior GS (or VS for vGPU10) before DXBC.
                 * Stage visitation remains VS -> PS -> GS, so a newly created PS can
                 * intentionally observe GS metadata before that GS is realized later
                 * in the same setup call.
                 */
                prepared = vmsvga3d_d3d10_shader_prepare_ps_live(
                    s, cid, stage, info);
                if (prepared) {
                    prepared = vmsvga3d_d3d10_shader_match_signatures(
                        SVGA3D_SHADERTYPE_PS, info, vs, NULL, NULL, gs, NULL) !=
                        VMSVGA3D_D3D10_LEVEL_INVALID;
                }
            }
        }

        if (shader_type == SVGA3D_SHADERTYPE_GS) {
            VMSVGA3DD3D10ShaderInfo *info = NULL;

            prepared = vmsvga3d_dxvk_d3d11_shader_info_for_realize(
                s->dxvk, cid, shader_id, shader_type, &info);
            if (prepared && info != NULL) {
                const VMSVGA3DD3D10ShaderInfo *vs =
                    vmsvga3d_d3d10_bound_shader_info_live(
                        s, context, cid, SVGA3D_SHADERTYPE_VS);
                const VMSVGA3DD3D10ShaderInfo *ps =
                    vmsvga3d_d3d10_bound_shader_info_live(
                        s, context, cid, SVGA3D_SHADERTYPE_PS);

                /* vGPU10 has no HS/DS stages.  Match GS input against VS and fill a
                 * missing GS output signature from PS before resolving SO semantics.
                 */
                prepared =
                    vmsvga3d_d3d10_shader_match_signatures(
                        SVGA3D_SHADERTYPE_GS, info, vs, NULL, NULL, NULL, ps) !=
                        VMSVGA3D_D3D10_LEVEL_INVALID;
                if (prepared) {
                    prepared = vmsvga3d_d3d10_stream_output_prepare_live(
                        s, context, cid, info, &stream_output_id, &stream_output);
                    if (prepared && stream_output_id != SVGA3D_INVALID_ID) {
                        stream_output_ptr = &stream_output;
                    }
                }
            }
        }

        if (shader_type == SVGA3D_SHADERTYPE_VS) {
            VMSVGA3DD3D10ShaderInfo *info = NULL;

            prepared = vmsvga3d_dxvk_d3d11_shader_info_for_realize(
                s->dxvk, cid, shader_id, shader_type, &info);
            if (prepared && info != NULL) {
                const SVGA3dInputElementDesc *descs = NULL;
                uint32_t desc_count = 0;
                uint32_t layout_id = context->shadow.inputAssembly.layoutId;

                if (layout_id != SVGA3D_INVALID_ID) {
                    SVGACOTableDXElementLayoutEntry *entry =
                        vmsvga3d_dx_cotable_entry_ptr(
                            s, cid, SVGA_COTABLE_ELEMENTLAYOUT, layout_id);

                    if (entry != NULL && entry->numDescs <= 32u) {
                        descs = entry->descs;
                        desc_count = entry->numDescs;
                    }
                }

                /* Match dxSetupPipeline's VS create-time preparation: the current
                 * input declaration supplies component types, then generic VS input
                 * semantics use the source register as their semantic index.
                 */
                prepared =
                    vmsvga3d_d3d10_shader_update_vs_input_signature(
                        info, descs, desc_count) != VMSVGA3D_D3D10_LEVEL_INVALID &&
                    vmsvga3d_d3d10_shader_match_signatures(
                        SVGA3D_SHADERTYPE_VS, info, NULL, NULL, NULL, NULL, NULL) !=
                        VMSVGA3D_D3D10_LEVEL_INVALID;
            }
        }

        /* dxSetupPipeline is void in the Oracle: creation failures assert/log but
         * do not become a frontend command failure.  The stage is only bound when
         * its native object exists, exactly like the RT_SUCCESS(rc) dxShaderSet
         * guard in VirtualBox.
         */
        if (prepared) {
            realized = vmsvga3d_dxvk_d3d11_shader_realize(
                s->dxvk, cid, shader_id, stream_output_id, stream_output_ptr);
        }
        if (realized) {
            (void)vmsvga3d_dxvk_d3d11_shader_set(
                s->dxvk, cid, shader_id, shader_type);
        }
    }
}

static void vmsvga3d_d3d10_pipeline_shader_resources_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    uint32_t stage;

    if (context == NULL) {
        return;
    }

    for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE_DX10; stage++) {
        uint64_t dirty = VMSVGA3D_DX_CTX_F_STATE_SRV_VS << stage;
        uint32_t count;

        if ((context->renderer_dirty & dirty) == 0) {
            continue;
        }
        /* VirtualBox clears the dirty bit before dxUpdateShaderResources and
         * rebinds the entire remembered 0..cMaxBound span, including NULL holes.
         */
        context->renderer_dirty &= ~dirty;
        count = MIN(context->shader_resource_max_bound[stage],
                    (uint32_t)SVGA3D_DX_MAX_SRVIEWS);
        (void)vmsvga3d_dxvk_d3d11_set_shader_resources(
            s->dxvk, cid, stage, 0, count,
            context->shadow.shaderState[stage].shaderResources);
    }
}

static void vmsvga3d_d3d10_pipeline_input_layout_realize_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    uint32_t stage = SVGA3D_SHADERTYPE_VS - SVGA3D_SHADERTYPE_MIN;
    uint32_t layout_id;
    uint32_t shader_id;

    if (context == NULL ||
        (context->renderer_dirty & VMSVGA3D_DX_CTX_F_STATE_INPUTLAYOUT) == 0) {
        return;
    }

    /* dxUpdateInputLayout clears the dirty bit first, lazily creates the native
     * layout from the current VS DXBC if possible, and always issues
     * IASetInputLayout afterwards.  Failed/missing creation therefore binds
     * NULL rather than retaining a stale native layout.
     */
    context->renderer_dirty &= ~VMSVGA3D_DX_CTX_F_STATE_INPUTLAYOUT;
    layout_id = context->shadow.inputAssembly.layoutId;
    if (layout_id != SVGA3D_INVALID_ID) {
        shader_id = context->shadow.shaderState[stage].shaderId;
        if (shader_id != SVGA3D_INVALID_ID) {
            (void)vmsvga3d_d3d10_input_layout_realize_live(
                s, cid, layout_id, shader_id);
        }
    }

    (void)vmsvga3d_dxvk_d3d11_set_input_layout(s->dxvk, cid, layout_id);
}

/*
 * VirtualBox-style live dxSetupPipeline executor for the vGPU10 stages that
 * are implemented here.
 */
static void VMSVGA3D_D3D10_LIVE_UNUSED
vmsvga3d_d3d10_pipeline_setup_live(struct vmsvga_state_s *s, uint32_t cid)
{
    vmsvga3d_d3d10_pipeline_resources_views_ensure_live(s, cid);
    vmsvga3d_d3d10_pipeline_state_realize_live(s, cid);
    vmsvga3d_d3d10_pipeline_output_targets_live(s, cid);
    vmsvga3d_d3d10_pipeline_constant_buffers_live(s, cid);
    vmsvga3d_d3d10_pipeline_vertex_buffers_live(s, cid);
    vmsvga3d_d3d10_pipeline_index_buffer_live(s, cid);
    vmsvga3d_d3d10_pipeline_shader_resources_live(s, cid);
    vmsvga3d_d3d10_pipeline_shaders_setup_live(s, cid);
    vmsvga3d_d3d10_pipeline_input_layout_realize_live(s, cid);
}

static void vmsvga3d_d3d10_post_draw_live(VMSVGA3DDXContext *context)
{
    if (context == NULL) {
        return;
    }

    /* Frontend dxPostDraw in VirtualBox clears the per-slot modification
     * history only after a draw has been submitted.
     */
    context->vertex_buffer_modified = 0;
    memset(context->shader_resource_modified, 0,
           sizeof(context->shader_resource_modified));
}

static bool vmsvga3d_d3d10_draw_live(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t vertex_count,
    uint32_t start_vertex_location)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    bool success;

    if (s == NULL || s->dxvk == NULL || context == NULL) {
        return false;
    }

    vmsvga3d_d3d10_pipeline_setup_live(s, cid);
    if (context->shadow.inputAssembly.topology ==
        SVGA3D_PRIMITIVE_TRIANGLEFAN) {
        VMSVGA3DDxvkD3D11IndexBinding saved_binding;
        uint16_t *indices;
        void *index_buffer = NULL;
        uint32_t index_count;
        uint32_t buffer_bytes;
        uint32_t generated_count;
        bool created;

        /* Current VirtualBox also rejects counts below one complete triangle. */
        if (vertex_count > 65535u || vertex_count < 3u) {
            return false;
        }

        index_count = 3u * (vertex_count - 2u);
        buffer_bytes = index_count * sizeof(*indices);
        indices = malloc(buffer_bytes);
        if (indices == NULL) {
            return false;
        }
        if (vmsvga3d_d3d10_triangle_fan_generate_u16(
                false, vertex_count, DXGI_UNKNOWN, NULL, 0, indices,
                index_count, &generated_count) == VMSVGA3D_D3D10_LEVEL_INVALID ||
            generated_count != index_count) {
            free(indices);
            return false;
        }

        /* VirtualBox asserts CreateBuffer success but does not return an error. */
        created = vmsvga3d_dxvk_d3d11_create_immutable_index_buffer(
            s->dxvk, indices, buffer_bytes, &index_buffer);
        (void)created;

        if (!vmsvga3d_dxvk_d3d11_get_index_buffer(s->dxvk, &saved_binding)) {
            vmsvga3d_dxvk_d3d11_release_index_buffer(index_buffer);
            free(indices);
            return false;
        }

        success = vmsvga3d_dxvk_d3d11_set_native_index_buffer(
            s->dxvk, index_buffer, DXGI_R16_UINT, 0);
        if (!vmsvga3d_dxvk_d3d11_set_primitive_topology(
                s->dxvk, D3D10_TOPOLOGY_TRIANGLELIST)) {
            success = false;
        }
        if (!vmsvga3d_dxvk_d3d11_draw_indexed(
                s->dxvk, index_count, 0, (int32_t)start_vertex_location)) {
            success = false;
        }

        /* VirtualBox restores TRIANGLESTRIP explicitly for emulated fans. */
        if (!vmsvga3d_dxvk_d3d11_set_primitive_topology(
                s->dxvk, D3D10_TOPOLOGY_TRIANGLESTRIP)) {
            success = false;
        }
        if (!vmsvga3d_dxvk_d3d11_set_native_index_buffer(
                s->dxvk, saved_binding.buffer, saved_binding.format,
                saved_binding.offset)) {
            success = false;
        }
        vmsvga3d_dxvk_d3d11_release_index_buffer(saved_binding.buffer);
        vmsvga3d_dxvk_d3d11_release_index_buffer(index_buffer);
        free(indices);
    } else {
        success = vmsvga3d_dxvk_d3d11_draw(
            s->dxvk, vertex_count, start_vertex_location);
    }

    vmsvga3d_d3d10_post_draw_live(context);
    return success;
}

static bool vmsvga3d_d3d10_draw_indexed_triangle_fan_live(
    struct vmsvga_state_s *s, uint32_t index_count,
    uint32_t start_index_location, int32_t base_vertex_location)
{
    VMSVGA3DDxvkD3D11IndexBinding saved_binding;
    void *source_indices = NULL;
    uint32_t source_bytes = 0;
    uint32_t bytes_per_index;
    uint16_t *generated_indices = NULL;
    uint32_t generated_count;
    uint32_t generated_capacity;
    uint32_t generated_bytes;
    void *index_buffer = NULL;
    bool created;
    bool success = false;

    /* Current VirtualBox rejects counts below one complete triangle too. */
    if (index_count > 65535u || index_count < 3u) {
        return false;
    }

    memset(&saved_binding, 0, sizeof(saved_binding));

    if (!vmsvga3d_dxvk_d3d11_get_index_buffer(
            s->dxvk, &saved_binding)) {
        return false;
    }

    if (saved_binding.format == DXGI_R16_UINT) {
        bytes_per_index = 2;
    } else if (saved_binding.format == DXGI_R32_UINT) {
        bytes_per_index = 4;
    } else {
        goto out;
    }

    /* VirtualBox deliberately treats startIndexLocation as a raw byte offset
     * here and ignores the IA index-buffer binding offset.
     */
    if (!vmsvga3d_dxvk_d3d11_read_index_buffer(
            s->dxvk, saved_binding.buffer, start_index_location,
            bytes_per_index * index_count, &source_indices, &source_bytes)) {
        goto out;
    }

    generated_capacity = 3u * (index_count - 2u);
    generated_bytes = generated_capacity * sizeof(*generated_indices);
    generated_indices = malloc(generated_bytes);

    if (generated_indices == NULL) {
        goto out;
    }

    if (vmsvga3d_d3d10_triangle_fan_generate_u16(
            true, index_count, saved_binding.format, source_indices,
            source_bytes, generated_indices, generated_capacity,
            &generated_count) == VMSVGA3D_D3D10_LEVEL_INVALID ||
        generated_count != generated_capacity) {
        goto out;
    }

    /* VirtualBox asserts CreateBuffer success but continues regardless. */
    created = vmsvga3d_dxvk_d3d11_create_immutable_index_buffer(
        s->dxvk, generated_indices, generated_bytes, &index_buffer);
    (void)created;

    (void)vmsvga3d_dxvk_d3d11_set_native_index_buffer(
        s->dxvk, index_buffer, DXGI_R16_UINT, 0);
    (void)vmsvga3d_dxvk_d3d11_set_primitive_topology(
        s->dxvk, D3D10_TOPOLOGY_TRIANGLELIST);
    (void)vmsvga3d_dxvk_d3d11_draw_indexed(
        s->dxvk, generated_count, 0, base_vertex_location);

    /* VirtualBox restores TRIANGLESTRIP explicitly, then the exact saved
     * native index-buffer binding.
     */
    (void)vmsvga3d_dxvk_d3d11_set_primitive_topology(
        s->dxvk, D3D10_TOPOLOGY_TRIANGLESTRIP);
    (void)vmsvga3d_dxvk_d3d11_set_native_index_buffer(
        s->dxvk, saved_binding.buffer, saved_binding.format,
        saved_binding.offset);
    success = true;

out:
    vmsvga3d_dxvk_d3d11_release_index_buffer(index_buffer);

    free(generated_indices);
    free(source_indices);

    vmsvga3d_dxvk_d3d11_release_index_buffer(saved_binding.buffer);

    return success;
}

static bool vmsvga3d_d3d10_draw_indexed_live(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t index_count,
    uint32_t start_index_location, int32_t base_vertex_location)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    bool success;

    if (s == NULL || s->dxvk == NULL || context == NULL) {
        return false;
    }

    vmsvga3d_d3d10_pipeline_setup_live(s, cid);
    if (context->shadow.inputAssembly.topology ==
        SVGA3D_PRIMITIVE_TRIANGLEFAN) {
        /* VirtualBox ignores every error from dxDrawIndexedTriangleFan. */
        (void)vmsvga3d_d3d10_draw_indexed_triangle_fan_live(
            s, index_count, start_index_location, base_vertex_location);
        success = true;
    } else {
        success = vmsvga3d_dxvk_d3d11_draw_indexed(
            s->dxvk, index_count, start_index_location, base_vertex_location);
    }

    vmsvga3d_d3d10_post_draw_live(context);
    return success;
}

static bool vmsvga3d_d3d10_draw_instanced_live(
    struct vmsvga_state_s *s, uint32_t cid,
    uint32_t vertex_count_per_instance, uint32_t instance_count,
    uint32_t start_vertex_location, uint32_t start_instance_location)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    bool success;

    if (s == NULL || s->dxvk == NULL || context == NULL) {
        return false;
    }

    vmsvga3d_d3d10_pipeline_setup_live(s, cid);

    /* VirtualBox only asserts that triangle fans are not used for instanced
     * draws and still submits the native call.  Keep that assert-only behavior
     * rather than adding a different runtime rejection here.
     */
    success = vmsvga3d_dxvk_d3d11_draw_instanced(
        s->dxvk, vertex_count_per_instance, instance_count,
        start_vertex_location, start_instance_location);

    vmsvga3d_d3d10_post_draw_live(context);
    return success;
}

static bool vmsvga3d_d3d10_draw_indexed_instanced_live(
    struct vmsvga_state_s *s, uint32_t cid,
    uint32_t index_count_per_instance, uint32_t instance_count,
    uint32_t start_index_location, int32_t base_vertex_location,
    uint32_t start_instance_location)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    bool success;

    if (s == NULL || s->dxvk == NULL || context == NULL) {
        return false;
    }

    vmsvga3d_d3d10_pipeline_setup_live(s, cid);
    /* As in VirtualBox, triangle-fan topology is assert-only for this command;
     * the native instanced draw is still submitted.
     */
    success = vmsvga3d_dxvk_d3d11_draw_indexed_instanced(
        s->dxvk, index_count_per_instance, instance_count,
        start_index_location, base_vertex_location, start_instance_location);

    vmsvga3d_d3d10_post_draw_live(context);
    return success;
}

static bool vmsvga3d_d3d10_draw_auto_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    bool success;

    if (s == NULL || s->dxvk == NULL || context == NULL) {
        return false;
    }

    vmsvga3d_d3d10_pipeline_setup_live(s, cid);
    /* VirtualBox only asserts that triangle fans are not used for DrawAuto and
     * still submits the native call.  Preserve that assert-only behavior.
     */
    success = vmsvga3d_dxvk_d3d11_draw_auto(s->dxvk);

    vmsvga3d_d3d10_post_draw_live(context);
    return success;
}

static bool vmsvga3d_d3d10_shader_resources_unbind_modified_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10ShaderResourceSetPlan *plan)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    uint32_t null_view = SVGA3D_INVALID_ID;
    uint32_t i;

    if (context == NULL || plan == NULL ||
        plan->stage_index >= SVGA3D_NUM_SHADERTYPE_DX10) {
        return false;
    }
    /* VirtualBox keeps au64Modified set across SET calls until dxPostDraw.
     * Any later SET covering such a slot NULL-unbinds it immediately so it
     * cannot conflict with an RTV/DSV bound by the next pipeline setup.
     */
    for (i = 0; i < plan->shadow_update_count; i++) {
        uint32_t slot = plan->start_view + i;

        if ((context->shader_resource_modified[plan->stage_index][slot / 64u] &
             (UINT64_C(1) << (slot % 64u))) != 0 &&
            !vmsvga3d_dxvk_d3d11_set_shader_resources(
                s->dxvk, cid, plan->stage_index, slot, 1, &null_view)) {
            return false;
        }
    }

    return true;
}

static bool vmsvga3d_d3d10_entry_is_zero(const void *entry, size_t size)
{
    const uint8_t *bytes = entry;
    size_t i;

    if (entry == NULL) {
        return false;
    }

    for (i = 0; i < size; i++) {
        if (bytes[i] != 0) {
            return false;
        }
    }

    return true;
}

static void vmsvga3d_d3d10_cotable_sanitize_live(
    SVGACOTableType type, void *entries, uint32_t valid_entries,
    uint32_t capacity_entries)
{
    uint32_t i;

    if (entries == NULL || valid_entries == 0) {
        return;
    }

    switch (type) {
    case SVGA_COTABLE_ELEMENTLAYOUT: {
          SVGACOTableDXElementLayoutEntry *table = entries;

          for (i = 0; i < valid_entries; i++) {
              SVGACOTableDXElementLayoutEntry *entry = &table[i];

              if (vmsvga3d_d3d10_entry_is_zero(entry, sizeof(*entry))) {
                  continue;
              }
              if (entry->elid >= capacity_entries) {
                  memset(entry, 0, sizeof(*entry));
                  continue;
              }
              entry->numDescs = MIN(entry->numDescs, ARRAY_SIZE(entry->descs));
          }
          break;
      }
    case SVGA_COTABLE_STREAMOUTPUT:
        /* VirtualBox preserves raw COTable stream-output entries here.  Its
         * backend only resets the cached SO declaration during SET/GROW; field
         * validation is deferred until a GS lazily realizes the selected SO.
         */
        break;
    case SVGA_COTABLE_DXQUERY: {
          SVGACOTableDXQueryEntry *table = entries;

          for (i = 0; i < valid_entries; i++) {
              SVGACOTableDXQueryEntry *entry = &table[i];

              if (!vmsvga3d_d3d10_entry_is_zero(entry, sizeof(*entry)) &&
                  entry->type >= SVGA3D_QUERYTYPE_MAX) {
                  memset(entry, 0, sizeof(*entry));
              }
          }
          break;
      }
    case SVGA_COTABLE_DXSHADER: {
          SVGACOTableDXShaderEntry *table = entries;

          for (i = 0; i < valid_entries; i++) {
              SVGACOTableDXShaderEntry *entry = &table[i];
              bool valid = true;

              if (vmsvga3d_d3d10_entry_is_zero(entry, sizeof(*entry))) {
                  continue;
              }
              if (entry->type == SVGA3D_SHADERTYPE_INVALID) {
                  valid = entry->sizeInBytes == 0 && entry->offsetInBytes == 0 &&
                          entry->mobid == SVGA3D_INVALID_ID;
              } else {
                  valid = entry->type >= SVGA3D_SHADERTYPE_MIN &&
                          entry->type < SVGA3D_SHADERTYPE_MAX &&
                          entry->sizeInBytes >= 8;
              }
              if (!valid) {
                  memset(entry, 0, sizeof(*entry));
              }
          }
          break;
      }
    default:
        break;
    }
}

static bool vmsvga3d_d3d10_state_cotable_replay_live(
    struct vmsvga_state_s *s, uint32_t cid, SVGACOTableType type,
    uint32_t old_capacity_entries, uint32_t new_valid_entries, bool grow)
{
    VMSVGA3DDXContext *context;
    uint32_t first_destroy;
    uint32_t i;

    if (s == NULL) {
        return false;
    }

    context = vmsvga3d_dx_context(s, cid);
    if (context == NULL) {
        return false;
    }

    /*
     * VirtualBox's DX_STATE_TRACKER keeps the valid backend prefix on GROW,
     * but drops it on SET for state-tracked objects. Shader objects are an
     * intentional exception and preserve the valid prefix for both commands.
     */
    first_destroy = grow ? MIN(new_valid_entries, old_capacity_entries) : 0;

    switch (type) {
    case SVGA_COTABLE_ELEMENTLAYOUT:
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_input_layout_destroy(s->dxvk, cid, i)) {
                return false;
            }
        }
        if (!grow && new_valid_entries != 0) {
            context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_INPUTLAYOUT;
        }
        break;
    case SVGA_COTABLE_BLENDSTATE:
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_blend_state_destroy(s->dxvk, cid, i)) {
                return false;
            }
        }
        if (!grow && new_valid_entries != 0) {
            context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE;
        }
        break;
    case SVGA_COTABLE_DEPTHSTENCIL:
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_depth_stencil_state_destroy(
                    s->dxvk, cid, i)) {
                return false;
            }
        }
        if (!grow && new_valid_entries != 0) {
            context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_DEPTHSTENCILSTATE;
        }
        break;
    case SVGA_COTABLE_RASTERIZERSTATE:
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_rasterizer_state_destroy(
                    s->dxvk, cid, i)) {
                return false;
            }
        }
        if (!grow && new_valid_entries != 0) {
            context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RASTERIZERSTATE;
        }
        break;
    case SVGA_COTABLE_SAMPLER:
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_sampler_state_destroy(s->dxvk, cid, i)) {
                return false;
            }
        }
        if (!grow && new_valid_entries != 0) {
            context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_SAMPLERS;
        }
        break;
    case SVGA_COTABLE_STREAMOUTPUT:
        /* VirtualBox preserves the backend prefix allocation on GROW, then resets
         * every initialized preserved SO entry so its semantic mapping is rebuilt
         * when a GS next uses it. SET drops the whole old backend range.
         *
         * A native D3D11 geometry shader embeds its stream-output declaration at
         * creation time. Replacing the SO COTable can therefore invalidate the
         * declaration of the currently selected SO even though the GS id itself
         * did not change. Drop that native GS realization so the next draw
         * rebuilds it from the replacement table.
         */
        if (!grow &&
            !vmsvga3d_d3d10_stream_output_invalidate_bound_gs_live(
                s, context, cid)) {
            return false;
        }
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_stream_output_destroy(
                    s->dxvk, cid, i)) {
                return false;
            }
        }
        for (i = 0; i < new_valid_entries; i++) {
            SVGACOTableDXStreamOutputEntry *entry =
                vmsvga3d_dx_cotable_entry_ptr(
                    s, cid, SVGA_COTABLE_STREAMOUTPUT, i);
            if (entry != NULL &&
                !vmsvga3d_d3d10_entry_is_zero(entry, sizeof(*entry)) &&
                !vmsvga3d_dxvk_d3d11_stream_output_destroy(
                    s->dxvk, cid, i)) {
                return false;
            }
        }
        break;
    case SVGA_COTABLE_DXQUERY:
        first_destroy = MIN(new_valid_entries, old_capacity_entries);
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_query_destroy(s->dxvk, cid, i)) {
                return false;
            }
        }
        for (i = 0; i < new_valid_entries; i++) {
            uint8_t *entry = vmsvga3d_dx_cotable_entry_ptr(
                s, cid, SVGA_COTABLE_DXQUERY, i);
            SVGA3dQueryType query_type;
            VMSVGA3DD3D10QueryExecutionPlan plan;
            uint32_t flags;

            if (entry == NULL ||
                vmsvga3d_d3d10_entry_is_zero(
                    entry, VMSVGA3D_D3D10_QUERY_COTABLE_ENTRY_SIZE)) {
                continue;
            }
            query_type = (SVGA3dQueryType)entry[0];
            if (query_type == SVGA3D_QUERYTYPE_INVALID ||
                vmsvga3d_dxvk_d3d11_query_exists(s->dxvk, cid, i)) {
                continue;
            }
            flags = query_read_u32(entry + 4);
            if (vmsvga3d_d3d10_query_execution_plan(
                    query_type, flags, &plan) == VMSVGA3D_D3D10_LEVEL_INVALID ||
                !vmsvga3d_dxvk_d3d11_query_define(
                    s->dxvk, cid, i, plan.d3d_query, plan.misc_flags)) {
                return false;
            }
        }
        break;
    case SVGA_COTABLE_DXSHADER:
        first_destroy = MIN(new_valid_entries, old_capacity_entries);
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_shader_destroy(s->dxvk, cid, i)) {
                return false;
            }
        }
        for (i = 0; i < new_valid_entries; i++) {
            SVGACOTableDXShaderEntry *entry = vmsvga3d_dx_cotable_entry_ptr(
                s, cid, SVGA_COTABLE_DXSHADER, i);
            uint32_t backend_type;

            if (entry == NULL ||
                vmsvga3d_d3d10_entry_is_zero(entry, sizeof(*entry))) {
                continue;
            }
            if (vmsvga3d_dxvk_d3d11_shader_object_exists(
                    s->dxvk, cid, i, &backend_type)) {
                /* VirtualBox preserves the backend prefix and only asserts that a
                 * restored COTable entry agrees with the backend shader type.
                 */
                assert(entry->type == backend_type);
                continue;
            }
            if (entry->type != SVGA3D_SHADERTYPE_INVALID &&
                !vmsvga3d_dxvk_d3d11_shader_object_define(
                    s->dxvk, cid, i, entry->type)) {
                return false;
            }
        }
        break;
    case SVGA_COTABLE_RTVIEW:
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_render_target_view_destroy(
                    s->dxvk, cid, i)) {
                return false;
            }
        }
        if (!grow && new_valid_entries != 0) {
            context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET;
        }
        break;
    case SVGA_COTABLE_DSVIEW:
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_depth_stencil_view_destroy(
                    s->dxvk, cid, i)) {
                return false;
            }
        }
        if (!grow && new_valid_entries != 0) {
            context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET;
        }
        break;
    case SVGA_COTABLE_SRVIEW:
        for (i = first_destroy; i < old_capacity_entries; i++) {
            if (!vmsvga3d_dxvk_d3d11_shader_resource_view_destroy(
                    s->dxvk, cid, i)) {
                return false;
            }
        }
        if (!grow && new_valid_entries != 0) {
            context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_SRVS;
        }
        break;
    case SVGA_COTABLE_UAVIEW:
        if (!grow && new_valid_entries != 0) {
            context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET |
                                       VMSVGA3D_DX_CTX_F_STATE_CSTARGET;
        }
        break;
    default:
        break;
    }

    return true;
}

static bool vmsvga3d_d3d10_surface_info_live(
    const VMSVGA3DSurface *surface, VMSVGA3DD3D10SurfaceInfo *info)
{
    uint32_t mip_levels;
    uint32_t array_elements;
    uint32_t expected_mips;
    uint32_t i;

    if (surface == NULL || info == NULL || surface->mips == NULL ||
        surface->mip_count == 0) {
        return false;
    }

    mip_levels = surface->face[0].numMipLevels;
    array_elements = surface->array_elements;
    if (mip_levels == 0 || array_elements == 0 ||
        mip_levels > UINT32_MAX / array_elements) {
        return false;
    }

    expected_mips = mip_levels * array_elements;
    if (surface->mip_count != expected_mips) {
        return false;
    }

    memset(info, 0, sizeof(*info));

    info->surface_flags = surface->surface_flags;
    info->format = surface->format;
    info->size = surface->mips[0].size;
    info->mip_levels = mip_levels;
    info->array_elements = array_elements;
    info->multisample_count = surface->multisample_count;
    info->autogen_filter = surface->autogen_filter;
    info->surface_bytes = surface->mips[0].data_size;
    info->has_initial_data = true;

    for (i = 0; i < expected_mips; i++) {
        if (surface->mips[i].data == NULL) {
            info->has_initial_data = false;
            break;
        }
    }

    return true;
}

static bool vmsvga3d_d3d10_initial_subresources_live(
    const VMSVGA3DSurface *surface, const VMSVGA3DD3D10CreateDesc *desc,
    VMSVGA3DDxvkSubresourceData **initial_data, uint32_t *initial_data_count)
{
    VMSVGA3DDxvkSubresourceData *data;
    uint32_t count;
    uint32_t i;

    if (surface == NULL || desc == NULL || initial_data == NULL ||
        initial_data_count == NULL) {
        return false;
    }

    *initial_data = NULL;
    *initial_data_count = 0;

    count = desc->initial_subresource_count;
    if (count == 0) {
        return true;
    }

    if (surface->mips == NULL || count > surface->mip_count) {
        return false;
    }

    data = g_try_new0(VMSVGA3DDxvkSubresourceData, count);
    if (data == NULL) {
        return false;
    }

    for (i = 0; i < count; i++) {
        if (surface->mips[i].data == NULL) {
            g_free(data);
            return false;
        }
        data[i].data = surface->mips[i].data;
        data[i].row_pitch = surface->mips[i].pitch;
        data[i].slice_pitch = surface->mips[i].plane_size;
    }

    *initial_data = data;
    *initial_data_count = count;

    return true;
}

static bool vmsvga3d_d3d10_buffer_materialize_live(
    struct vmsvga_state_s *s, SVGA3dSurfaceId sid)
{
    VMSVGA3DSurface *surface;
    VMSVGA3DD3D10SurfaceInfo surface_info;
    VMSVGA3DD3D10ResourcePlan resource_plan;
    VMSVGA3DDxvkSubresourceData *initial_data = NULL;
    uint32_t initial_data_count = 0;
    bool success;

    if (s == NULL || s->svga3d == NULL || !vmsvga3d_dxvk_d3d11_ready(s->dxvk) ||
        sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    surface = s->svga3d->surfaces[sid];

    if (surface == NULL || surface->dxvk_surface == NULL ||
        !vmsvga3d_d3d10_surface_info_live(surface, &surface_info) ||
        !vmsvga3d_d3d10_level_is_vgpu10(
            vmsvga3d_d3d10_resource_plan(
                &surface_info, VMSVGA3D_D3D10_RESOURCE_USE_BUFFER,
                &resource_plan)) ||
        !resource_plan.primary.valid ||
        !vmsvga3d_d3d10_initial_subresources_live(
            surface, &resource_plan.primary, &initial_data,
            &initial_data_count)) {
        return false;
    }

    success = vmsvga3d_dxvk_d3d11_surface_materialize(
        s->dxvk, surface->dxvk_surface, &resource_plan.primary,
        initial_data, initial_data_count);

    g_free(initial_data);
    return success;
}

static bool vmsvga3d_d3d10_so_targets_bind_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10SOTargetsPlan *plan)
{
    VMSVGA3DDXContext *context;
    VMSVGA3DDxvkSurface *surfaces[SVGA3D_DX_MAX_SOTARGETS] = { NULL };
    uint32_t offsets[SVGA3D_DX_MAX_SOTARGETS] = { 0 };
    uint32_t i;

    if (s == NULL || s->svga3d == NULL || plan == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk) ||
        plan->native_target_count != SVGA3D_DX_MAX_SOTARGETS ||
        plan->backend_remembered_count > SVGA3D_DX_MAX_SOTARGETS) {
        return false;
    }

    context = vmsvga3d_dx_context(s, cid);
    if (context == NULL) {
        return false;
    }

    /* VirtualBox prepares SO targets sequentially and aborts on first failure. */
    for (i = 0; i < SVGA3D_DX_MAX_SOTARGETS; i++) {
        const VMSVGA3DD3D10SOTargetBinding *binding = &plan->bindings[i];
        VMSVGA3DSurface *surface;
        VMSVGA3DD3D10SurfaceInfo surface_info;
        VMSVGA3DD3D10ResourcePlan resource_plan;

        if (!binding->active) {
            continue;
        }

        if (!binding->ensure_stream_output_buffer ||
            binding->sid >= SVGA3D_MAX_SURFACE_IDS) {
            return false;
        }

        surface = s->svga3d->surfaces[binding->sid];
        if (surface == NULL || surface->dxvk_surface == NULL ||
            !vmsvga3d_d3d10_surface_info_live(surface, &surface_info) ||
            !vmsvga3d_d3d10_level_is_vgpu10(
                vmsvga3d_d3d10_resource_plan(
                    &surface_info,
                    VMSVGA3D_D3D10_RESOURCE_USE_STREAM_OUTPUT_BUFFER,
                    &resource_plan)) ||
            !resource_plan.primary.valid ||
            !vmsvga3d_dxvk_d3d11_surface_materialize(
                s->dxvk, surface->dxvk_surface, &resource_plan.primary, NULL, 0)) {
            return false;
        }
        surfaces[i] = surface->dxvk_surface;
        offsets[i] = binding->offset;
    }

    if (!vmsvga3d_dxvk_d3d11_set_stream_output_targets(
            s->dxvk, surfaces, offsets)) {
        return false;
    }

    context->stream_output_target_count = plan->backend_remembered_count;
    return true;
}


static bool vmsvga3d_d3d10_context_switch_live(
    struct vmsvga_state_s *s, uint32_t cid)
{
    VMSVGA3DDXContext *context;
    VMSVGA3DDXContext *old_context = NULL;
    VMSVGA3DD3D10SOTargetsPlan plan;
    uint32_t null_views[SVGA3D_DX_MAX_SRVIEWS];
    uint32_t old_cid;
    uint32_t stage;
    uint32_t slot;
    uint32_t old_vb_count;

    if (s == NULL || s->svga3d == NULL || cid >= SVGA3D_MAX_CONTEXT_IDS) {
        return false;
    }

    context = vmsvga3d_dx_context(s, cid);
    if (context == NULL) {
        return false;
    }

    old_cid = s->svga3d->active_dx_context_id;
    if (old_cid == cid) {
        return true;
    }

    if (old_cid != SVGA3D_INVALID_ID && old_cid < SVGA3D_MAX_CONTEXT_IDS) {
        old_context = vmsvga3d_dx_context(s, old_cid);
    }

    /* Current VirtualBox DX_STATE_TRACKER marks every frontend pipeline state
     * dirty before switching the D3D11 backend context.  The next Draw will
     * therefore replay the complete new-context state from its shadow MOB.
     */
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_ALL;

    /* The backend makes every constant-buffer slot pending again on a context
     * switch.  vGPU10 exposes only VS/PS/GS, so mirror the 14 API slots for
     * those three stages here.
     */
    for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE_DX10; stage++) {
        context->constant_buffer_start_slot[stage] = 0;
        context->constant_buffer_num_buffers[stage] = SVGA3D_DX_MAX_CONSTBUFFERS;
    }

    /* Old SRVs are explicitly NULL-unbound before setupPipeline binds the new
     * context.  This is required because RTV/DSV targets are installed before
     * SRVs and stale views from the prior context could otherwise conflict.
     */
    for (slot = 0; slot < SVGA3D_DX_MAX_SRVIEWS; slot++) {
        null_views[slot] = SVGA3D_INVALID_ID;
    }

    for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE_DX10; stage++) {
        uint32_t count = old_context != NULL
                             ? MIN(old_context->shader_resource_max_bound[stage],
                                   (uint32_t)SVGA3D_DX_MAX_SRVIEWS)
                             : SVGA3D_DX_MAX_SRVIEWS;

        if (count != 0 &&
            !vmsvga3d_dxvk_d3d11_set_shader_resources(
                s->dxvk, cid, stage, 0, count, null_views)) {
            return false;
        }
    }

    /* Preserve VirtualBox's exact modified-mask quirk: the new context's VB
     * mask is reset to the span that was bound by the old context (all 32 slots
     * on the first switch).  Do not silently widen this to the new context's
     * cMaxBound even though that would look more intuitive.
     */
    old_vb_count = old_context != NULL
                       ? MIN(old_context->vertex_buffer_max_bound,
                             (uint32_t)SVGA3D_DX_MAX_VERTEXBUFFERS)
                       : SVGA3D_DX_MAX_VERTEXBUFFERS;

    if (old_vb_count == 0) {
        context->vertex_buffer_modified = 0;
    } else if (old_vb_count >= 64) {
        context->vertex_buffer_modified = UINT64_MAX;
    } else {
        context->vertex_buffer_modified =
            (UINT64_C(1) << old_vb_count) - UINT64_C(1);
    }

    /* VirtualBox restores SO targets on every DX context switch.  The context
     * MOB stores only target SIDs, so the original offsets and sizes are lost:
     * all four slots are rebound with offset 0 and sizeInBytes 0.
     */
    if (vmsvga3d_d3d10_so_targets_restore_plan(
            context->shadow.streamOut.targets, &plan) ==
            VMSVGA3D_D3D10_LEVEL_INVALID ||
        !vmsvga3d_d3d10_so_targets_bind_live(s, cid, &plan)) {
        return false;
    }

    s->svga3d->active_dx_context_id = cid;
    return true;
}

static bool vmsvga3d_d3d10_rtv_realize_live(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dRenderTargetViewId view_id)
{
    SVGACOTableDXRTViewEntry *entry;
    VMSVGA3DSurface *surface;
    VMSVGA3DD3D10SurfaceInfo surface_info;
    VMSVGA3DD3D10ResourcePlan resource_plan;
    VMSVGA3DD3D10RTVDesc rtv_desc;
    VMSVGA3DDxvkSubresourceData *initial_data = NULL;
    VMSVGA3DD3D10ResourceUse resource_use;
    VMSVGA3DD3D10Level level;
    uint32_t initial_data_count = 0;
    bool success = false;

    if (view_id == SVGA3D_INVALID_ID) {
        return true;
    }

    if (s == NULL || s->svga3d == NULL || !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return false;
    }

    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_RTVIEW, view_id);
    if (entry == NULL || entry->sid == SVGA3D_INVALID_ID ||
        entry->sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    surface = s->svga3d->surfaces[entry->sid];
    if (surface == NULL || surface->dxvk_surface == NULL ||
        !vmsvga3d_d3d10_surface_info_live(surface, &surface_info)) {
        return false;
    }

    resource_use = entry->resourceDimension == SVGA3D_RESOURCE_BUFFER
                       ? VMSVGA3D_D3D10_RESOURCE_USE_GENERIC_BUFFER
                       : VMSVGA3D_D3D10_RESOURCE_USE_TEXTURE;

    level = vmsvga3d_d3d10_resource_plan(
        &surface_info, resource_use, &resource_plan);
    if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
        !resource_plan.primary.valid) {
        return false;
    }

    if (!vmsvga3d_d3d10_initial_subresources_live(
            surface, &resource_plan.primary, &initial_data,
            &initial_data_count)) {
        return false;
    }

    if (!vmsvga3d_dxvk_d3d11_surface_materialize(
            s->dxvk, surface->dxvk_surface, &resource_plan.primary,
            initial_data, initial_data_count)) {
        goto out;
    }

    if (vmsvga3d_dxvk_d3d11_render_target_view_ensure(
            s->dxvk, cid, view_id, surface->dxvk_surface, NULL)) {
        success = true;
        goto out;
    }

    level = vmsvga3d_d3d10_rtv_desc(
        entry, surface_info.array_elements, surface_info.multisample_count,
        &rtv_desc);

    if (!vmsvga3d_d3d10_level_is_vgpu10(level)) {
        goto out;
    }

    success = vmsvga3d_dxvk_d3d11_render_target_view_ensure(
        s->dxvk, cid, view_id, surface->dxvk_surface, &rtv_desc);

out:
    g_free(initial_data);
    return success;
}

static bool vmsvga3d_d3d10_dsv_realize_live(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dDepthStencilViewId view_id)
{
    SVGACOTableDXDSViewEntry *entry;
    VMSVGA3DSurface *surface;
    VMSVGA3DD3D10SurfaceInfo surface_info;
    VMSVGA3DD3D10ResourcePlan resource_plan;
    VMSVGA3DD3D10DSVDesc dsv_desc;
    VMSVGA3DDxvkSubresourceData *initial_data = NULL;
    VMSVGA3DD3D10Level level;
    uint32_t initial_data_count = 0;
    bool success = false;

    if (view_id == SVGA3D_INVALID_ID) {
        return true;
    }

    if (s == NULL || s->svga3d == NULL || !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return false;
    }

    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DSVIEW, view_id);
    if (entry == NULL || entry->sid == SVGA3D_INVALID_ID ||
        entry->sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    surface = s->svga3d->surfaces[entry->sid];
    if (surface == NULL || surface->dxvk_surface == NULL ||
        !vmsvga3d_d3d10_surface_info_live(surface, &surface_info)) {
        return false;
    }

    level = vmsvga3d_d3d10_resource_plan(
        &surface_info, VMSVGA3D_D3D10_RESOURCE_USE_TEXTURE, &resource_plan);
    if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
        !resource_plan.primary.valid) {
        return false;
    }

    if (!vmsvga3d_d3d10_initial_subresources_live(
            surface, &resource_plan.primary, &initial_data,
            &initial_data_count)) {
        return false;
    }

    if (!vmsvga3d_dxvk_d3d11_surface_materialize(
            s->dxvk, surface->dxvk_surface, &resource_plan.primary,
            initial_data, initial_data_count)) {
        goto out;
    }

    if (vmsvga3d_dxvk_d3d11_depth_stencil_view_ensure(
            s->dxvk, cid, view_id, surface->dxvk_surface, NULL)) {
        success = true;
        goto out;
    }

    level = vmsvga3d_d3d10_dsv_desc(
        entry, surface_info.array_elements, surface_info.multisample_count,
        &dsv_desc);
    if (level < VMSVGA3D_D3D10_LEVEL_10_0 ||
        level > VMSVGA3D_D3D10_LEVEL_11_0) {
        goto out;
    }

    success = vmsvga3d_dxvk_d3d11_depth_stencil_view_ensure(
        s->dxvk, cid, view_id, surface->dxvk_surface, &dsv_desc);

out:
    g_free(initial_data);
    return success;
}

static bool vmsvga3d_d3d10_srv_realize_live(
    struct vmsvga_state_s *s, uint32_t cid,
    SVGA3dShaderResourceViewId view_id)
{
    SVGACOTableDXSRViewEntry *entry;
    VMSVGA3DSurface *surface;
    VMSVGA3DD3D10SurfaceInfo surface_info;
    VMSVGA3DD3D10ResourcePlan resource_plan;
    VMSVGA3DD3D10SRVDesc srv_desc;
    VMSVGA3DDxvkSubresourceData *initial_data = NULL;
    VMSVGA3DD3D10ResourceUse resource_use;
    VMSVGA3DD3D10Level level;
    uint32_t initial_data_count = 0;
    bool success = false;

    if (view_id == SVGA3D_INVALID_ID) {
        return true;
    }

    if (s == NULL || s->svga3d == NULL || !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return false;
    }

    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_SRVIEW, view_id);
    if (entry == NULL || entry->sid == SVGA3D_INVALID_ID ||
        entry->sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    surface = s->svga3d->surfaces[entry->sid];
    if (surface == NULL || surface->dxvk_surface == NULL ||
        !vmsvga3d_d3d10_surface_info_live(surface, &surface_info)) {
        return false;
    }

    resource_use = entry->resourceDimension == SVGA3D_RESOURCE_BUFFER
                       ? VMSVGA3D_D3D10_RESOURCE_USE_GENERIC_BUFFER
                       : VMSVGA3D_D3D10_RESOURCE_USE_TEXTURE;
    level = vmsvga3d_d3d10_resource_plan(
        &surface_info, resource_use, &resource_plan);

    if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
        !resource_plan.primary.valid) {
        return false;
    }

    if (!vmsvga3d_d3d10_initial_subresources_live(
            surface, &resource_plan.primary, &initial_data,
            &initial_data_count)) {
        return false;
    }

    if (!vmsvga3d_dxvk_d3d11_surface_materialize(
            s->dxvk, surface->dxvk_surface, &resource_plan.primary,
            initial_data, initial_data_count)) {
        goto out;
    }

    if (vmsvga3d_dxvk_d3d11_shader_resource_view_ensure(
            s->dxvk, cid, view_id, surface->dxvk_surface, NULL)) {
        success = true;
        goto out;
    }

    level = vmsvga3d_d3d10_srv_desc(
        entry, surface_info.array_elements, surface_info.multisample_count,
        &srv_desc);
    if (!vmsvga3d_d3d10_level_is_vgpu10(level)) {
        goto out;
    }

    success = vmsvga3d_dxvk_d3d11_shader_resource_view_ensure(
        s->dxvk, cid, view_id, surface->dxvk_surface, &srv_desc);

out:
    g_free(initial_data);
    return success;
}

static bool vmsvga3d_d3d10_copy_surface_materialize_live(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    VMSVGA3DD3D10ResourceCreateKind create_kind)
{
    VMSVGA3DD3D10SurfaceInfo surface_info;
    VMSVGA3DD3D10ResourcePlan resource_plan;
    VMSVGA3DDxvkSubresourceData *initial_data = NULL;
    VMSVGA3DD3D10ResourceUse resource_use;
    VMSVGA3DD3D10Level level;
    uint32_t initial_data_count = 0;
    bool success;

    if (s == NULL || surface == NULL || surface->dxvk_surface == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return false;
    }

    /* VBox dxEnsureResource reuses an existing backend resource verbatim.
     * Do not reject a resident resource merely because a later caller would
     * have chosen a different creation policy for a fresh resource. */
    if (vmsvga3d_dxvk_d3d11_surface_resident(surface->dxvk_surface)) {
        return true;
    }

    if (!vmsvga3d_d3d10_surface_info_live(surface, &surface_info)) {
        return false;
    }

    resource_use = create_kind == VMSVGA3D_D3D10_CREATE_BUFFER
                       ? VMSVGA3D_D3D10_RESOURCE_USE_GENERIC_BUFFER
                       : VMSVGA3D_D3D10_RESOURCE_USE_TEXTURE;

    level = vmsvga3d_d3d10_resource_plan(
        &surface_info, resource_use, &resource_plan);

    if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
        !resource_plan.primary.valid ||
        !vmsvga3d_d3d10_initial_subresources_live(
            surface, &resource_plan.primary, &initial_data,
            &initial_data_count)) {
        return false;
    }

    success = vmsvga3d_dxvk_d3d11_surface_materialize(
        s->dxvk, surface->dxvk_surface, &resource_plan.primary,
        initial_data, initial_data_count);

    g_free(initial_data);
    return success;
}

typedef struct vmsvga3d_d3d10_update_layout_s {
    SVGA3dBox box;
    uint32_t box_offset;
    uint32_t row_bytes;
    uint32_t row_count;
    uint32_t depth_count;
} VMSVGA3DD3D10UpdateLayout;

static bool vmsvga3d_d3d10_screen_target_bind_live(
    struct vmsvga_state_s *s, uint32_t sid)
{
    VMSVGA3DSurface *surface;

    if (s == NULL || s->svga3d == NULL) {
        return false;
    }

    if (sid == SVGA3D_INVALID_ID) {
        s->svga3d->active_screen_target_sid = sid;
        return true;
    }

    if (sid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    surface = s->svga3d->surfaces[sid];
    if (surface == NULL || surface->mips == NULL || surface->mip_count == 0 ||
        !vmsvga3d_d3d10_copy_surface_materialize_live(
            s, surface, VMSVGA3D_D3D10_CREATE_TEXTURE)) {
        return false;
    }

    /* VBox materializes/resolves the surface before this same-SID fast path,
     * but deliberately skips the Texture2D/SCREENTARGET validation afterwards. */
    if (s->svga3d->active_screen_target_sid == sid) {
        return true;
    }

    if (surface->format == SVGA3D_BUFFER ||
        (surface->surface_flags & SVGA3D_SURFACE_SCREENTARGET) == 0 ||
        (surface->surface_flags & (SVGA3D_SURFACE_1D | SVGA3D_SURFACE_VOLUME)) != 0 ||
        surface->mips[0].size.depth != 1) {
        return false;
    }

    s->svga3d->active_screen_target_sid = sid;
    return true;
}

static bool vmsvga3d_d3d10_update_box_live(
    VMSVGA3DSurface *surface, VMSVGA3DSurfaceImage *image,
    const SVGA3dBox *source, VMSVGA3DD3D10UpdateLayout *layout)
{
    const struct svga3d_surface_desc *desc;
    uint64_t right;
    uint64_t bottom;
    uint64_t back;
    uint64_t offset;
    uint64_t bytes;
    uint32_t block_width;
    uint32_t block_height;
    uint32_t block_depth;
    uint32_t blocks_x;

    if (surface == NULL || image == NULL || source == NULL || layout == NULL ||
        source->w == 0 || source->h == 0 || source->d == 0 ||
        source->x >= image->size.width || source->y >= image->size.height ||
        source->z >= image->size.depth) {
        return false;
    }

    desc = svga3dsurface_get_desc(surface->format);
    if (desc->format != surface->format || desc->pitch_bytes_per_block == 0 ||
        desc->block_size.width == 0 || desc->block_size.height == 0 ||
        desc->block_size.depth == 0) {
        return false;
    }

    block_width = desc->block_size.width;
    block_height = desc->block_size.height;
    block_depth = desc->block_size.depth;
    right = MIN((uint64_t)source->x + source->w, (uint64_t)image->size.width);
    bottom = MIN((uint64_t)source->y + source->h,
                 (uint64_t)image->size.height);
    back = MIN((uint64_t)source->z + source->d, (uint64_t)image->size.depth);

    if (right <= source->x || bottom <= source->y || back <= source->z) {
        return false;
    }

    memset(layout, 0, sizeof(*layout));

    layout->box = *source;
    layout->box.w = (uint32_t)(right - source->x);
    layout->box.h = (uint32_t)(bottom - source->y);
    layout->box.d = (uint32_t)(back - source->z);

    if (source->x % block_width != 0 || source->y % block_height != 0 ||
        source->z % block_depth != 0 ||
        ((uint32_t)right % block_width != 0 && right != image->size.width) ||
        ((uint32_t)bottom % block_height != 0 && bottom != image->size.height) ||
        ((uint32_t)back % block_depth != 0 && back != image->size.depth)) {
        return false;
    }

    blocks_x = (layout->box.w + block_width - 1) / block_width;
    bytes = (uint64_t)blocks_x * desc->pitch_bytes_per_block;
    offset = (uint64_t)(layout->box.x / block_width) *
                 desc->pitch_bytes_per_block +
             (uint64_t)(layout->box.y / block_height) * image->pitch +
             (uint64_t)(layout->box.z / block_depth) * image->plane_size;

    if (bytes == 0 || bytes > UINT32_MAX || offset > UINT32_MAX ||
        offset > image->data_size || bytes > image->data_size - offset) {
        return false;
    }

    layout->box_offset = (uint32_t)offset;
    layout->row_bytes = (uint32_t)bytes;
    layout->row_count = (layout->box.h + block_height - 1) / block_height;
    layout->depth_count = (layout->box.d + block_depth - 1) / block_depth;

    return layout->row_count != 0 && layout->depth_count != 0;
}

static bool vmsvga3d_d3d10_invalidate_subresource_live(
    struct vmsvga_state_s *s,
    const SVGA3dCmdDXInvalidateSubResource *command)
{
    SVGAOTableSurfaceEntry entry;
    VMSVGA3DSurface *surface;
    uint32_t otable_mip_levels;
    uint32_t local_mip_levels;
    uint32_t face = 0;
    uint32_t mipmap = 0;
    uint64_t local_subresource;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        command->sid >= SVGA3D_MAX_SURFACE_IDS ||
        !vmsvga3d_otable_read(s, SVGA_OTABLE_SURFACE, command->sid,
                              sizeof(entry), &entry, sizeof(entry))) {
        return false;
    }

    /* VBox derives face/mipmap from the OTable copy, not from the live
     * surface.  Its helper falls back to face=0,mipmap=0 when numMipLevels is
     * zero after an assertion, so preserve that release-build behavior.
     */
    otable_mip_levels = le32_to_cpu(entry.numMipLevels);
    if (otable_mip_levels != 0) {
        face = command->subResource / otable_mip_levels;
        mipmap = command->subResource % otable_mip_levels;
    }

    /* vmsvga3dCmdDXInvalidateSubResource returns the OTable-read status and
     * deliberately ignores vmsvga3dSurfaceInvalidate's status.  A stale OTable
     * entry or an invalid live image is therefore still command success.
     */
    surface = s->svga3d->surfaces[command->sid];
    if (surface == NULL || surface->mips == NULL) {
        return true;
    }

    local_mip_levels = surface->face[0].numMipLevels;
    if (local_mip_levels == 0 || surface->array_elements == 0 ||
        face >= surface->array_elements || mipmap >= local_mip_levels) {
        return true;
    }

    local_subresource = (uint64_t)face * local_mip_levels + mipmap;
    if (local_subresource >= surface->mip_count) {
        return true;
    }

    /* VBox D3D11 leaves buffer invalidation as a no-op.  For textures it
     * destroys all cached views on the surface, even though only one image was
     * requested; they are lazily recreated on their next use.
     */
    if (surface->format != SVGA3D_BUFFER && surface->dxvk_surface != NULL) {
        vmsvga3d_dxvk_d3d11_surface_invalidate_views(surface->dxvk_surface);
    }

    return true;
}

static bool vmsvga3d_d3d10_update_subresource_live(
    struct vmsvga_state_s *s, const SVGA3dCmdDXUpdateSubResource *command)
{
    SVGAOTableSurfaceEntry entry;
    VMSVGA3DSurface *surface;
    VMSVGA3DSurfaceImage *image;
    VMSVGA3DMob *mob;
    VMSVGA3DD3D10UpdateLayout layout;
    VMSVGA3DD3D10Box native_box;
    uint64_t subresource_offset = 0;
    uint32_t z;
    uint32_t y;
    uint32_t i;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        command->sid >= SVGA3D_MAX_SURFACE_IDS ||
        !vmsvga3d_otable_read(s, SVGA_OTABLE_SURFACE, command->sid,
                              sizeof(entry), &entry, sizeof(entry))) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, le32_to_cpu(entry.mobid));
    if (mob == NULL) {
        return true;
    }

    surface = s->svga3d->surfaces[command->sid];
    if (surface == NULL || surface->mips == NULL ||
        command->subResource >= surface->mip_count) {
        return false;
    }

    /* VBox skips guest-backed transfers for multisampled surfaces. */
    if (surface->multisample_count > 1) {
        return true;
    }

    image = &surface->mips[command->subResource];
    if (!vmsvga3d_d3d10_update_box_live(
            surface, image, &command->box, &layout)) {
        return false;
    }

    for (i = 0; i < command->subResource; i++) {
        subresource_offset += surface->mips[i].data_size;
    }

    if (subresource_offset > UINT32_MAX ||
        layout.box_offset > UINT32_MAX - (uint32_t)subresource_offset) {
        return false;
    }

    subresource_offset += layout.box_offset;

    for (z = 0; z < layout.depth_count; z++) {
        for (y = 0; y < layout.row_count; y++) {
            uint64_t offset = subresource_offset +
                              (uint64_t)z * image->plane_size +
                              (uint64_t)y * image->pitch;
            uint64_t host_offset = (uint64_t)layout.box_offset +
                                   (uint64_t)z * image->plane_size +
                                   (uint64_t)y * image->pitch;

            if (offset > UINT32_MAX || host_offset > image->data_size ||
                layout.row_bytes > image->data_size - host_offset ||
                !vmsvga3d_mob_read(s, mob, (uint32_t)offset,
                                   image->data + host_offset, layout.row_bytes)) {
                return false;
            }
        }
    }

    native_box.left = layout.box.x;
    native_box.top = layout.box.y;
    native_box.front = layout.box.z;
    native_box.right = layout.box.x + layout.box.w;
    native_box.bottom = layout.box.y + layout.box.h;
    native_box.back = layout.box.z + layout.box.d;

    return vmsvga3d_dxvk_d3d11_update_subresource(
        s->dxvk, surface->dxvk_surface, command->subResource, &native_box,
        image->data + layout.box_offset, image->pitch, image->plane_size);
}

static bool vmsvga3d_d3d10_readback_image_live(
    struct vmsvga_state_s *s, VMSVGA3DSurface *surface,
    uint32_t subresource)
{
    VMSVGA3DSurfaceImage *image;
    uint32_t row_count;
    uint32_t depth_count;

    if (s == NULL || surface == NULL || surface->mips == NULL ||
        subresource >= surface->mip_count) {
        return false;
    }

    /* VirtualBox skips guest-backed transfers for multisampled surfaces. */
    if (surface->multisample_count > 1) {
        return true;
    }

    image = &surface->mips[subresource];
    if (image->data == NULL || image->pitch == 0 || image->plane_size == 0 ||
        image->data_size == 0 || image->plane_size % image->pitch != 0 ||
        image->data_size % image->plane_size != 0) {
        return false;
    }

    row_count = image->plane_size / image->pitch;
    depth_count = image->data_size / image->plane_size;

    return vmsvga3d_dxvk_d3d11_readback_subresource(
        s->dxvk, surface->dxvk_surface, subresource, image->data,
        image->pitch, image->pitch, row_count, image->plane_size,
        depth_count);
}

static bool vmsvga3d_d3d10_subresource_offset_live(
    const VMSVGA3DSurface *surface, uint32_t subresource,
    uint32_t *offset_out)
{
    uint64_t offset = 0;
    uint32_t i;

    if (surface == NULL || surface->mips == NULL || offset_out == NULL ||
        subresource >= surface->mip_count) {
        return false;
    }

    for (i = 0; i < subresource; i++) {
        offset += surface->mips[i].data_size;
    }

    if (offset > UINT32_MAX) {
        return false;
    }

    *offset_out = (uint32_t)offset;
    return true;
}

static bool vmsvga3d_d3d10_readback_subresource_live(
    struct vmsvga_state_s *s,
    const SVGA3dCmdDXReadbackSubResource *command)
{
    SVGAOTableSurfaceEntry entry;
    VMSVGA3DSurface *surface;
    VMSVGA3DSurfaceImage *image;
    VMSVGA3DMob *mob;
    uint32_t subresource_offset;
    uint32_t row_count;
    uint32_t depth_count;
    uint32_t z;
    uint32_t y;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        command->sid >= SVGA3D_MAX_SURFACE_IDS ||
        !vmsvga3d_otable_read(s, SVGA_OTABLE_SURFACE, command->sid,
                              sizeof(entry), &entry, sizeof(entry))) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, le32_to_cpu(entry.mobid));
    /* VBox leaves the successful OTable-read status unchanged if the surface
     * currently has no MOB backing. */
    if (mob == NULL) {
        return true;
    }

    surface = s->svga3d->surfaces[command->sid];
    if (surface == NULL || surface->mips == NULL ||
        command->subResource >= surface->mip_count) {
        return false;
    }

    if (surface->multisample_count > 1) {
        return true;
    }

    image = &surface->mips[command->subResource];
    if (image->data == NULL || image->pitch == 0 || image->plane_size == 0 ||
        image->data_size == 0 || image->plane_size % image->pitch != 0 ||
        image->data_size % image->plane_size != 0 ||
        !vmsvga3d_d3d10_subresource_offset_live(
            surface, command->subResource, &subresource_offset) ||
        !vmsvga3d_d3d10_readback_image_live(
            s, surface, command->subResource)) {
        return false;
    }

    row_count = image->plane_size / image->pitch;
    depth_count = image->data_size / image->plane_size;

    for (z = 0; z < depth_count; z++) {
        for (y = 0; y < row_count; y++) {
            uint64_t offset = (uint64_t)subresource_offset +
                              (uint64_t)z * image->plane_size +
                              (uint64_t)y * image->pitch;
            const uint8_t *source = image->data +
                                    (size_t)z * image->plane_size +
                                    (size_t)y * image->pitch;

            if (offset > UINT32_MAX ||
                !vmsvga3d_mob_write(s, mob, (uint32_t)offset,
                                    source, image->pitch)) {
                return false;
            }
        }
    }

    return true;
}

static bool vmsvga3d_d3d10_buffer_copy_live(
    struct vmsvga_state_s *s,
    const SVGA3dCmdDXBufferCopy *command)
{
    VMSVGA3DSurface *source;
    VMSVGA3DSurface *destination;
    VMSVGA3DSurfaceImage *source_image;
    VMSVGA3DSurfaceImage *destination_image;
    VMSVGA3DD3D10Box native_box;
    bool copy_ok = false;
    bool update_ok;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        command->src >= SVGA3D_MAX_SURFACE_IDS ||
        command->dest >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    source = s->svga3d->surfaces[command->src];
    destination = s->svga3d->surfaces[command->dest];

    if (source == NULL || destination == NULL || source->mips == NULL ||
        destination->mips == NULL || source->mip_count == 0 ||
        destination->mip_count == 0 || source->dxvk_surface == NULL ||
        destination->dxvk_surface == NULL) {
        return false;
    }

    source_image = &source->mips[0];
    destination_image = &destination->mips[0];

    if (source_image->data == NULL || destination_image->data == NULL ||
        source_image->pitch == 0 || destination_image->pitch == 0 ||
        destination_image->plane_size == 0 ||
        !vmsvga3d_d3d10_readback_image_live(s, source, 0)) {
        return false;
    }

    /* Current VirtualBox maps both surfaces before checking that they really
     * are one-dimensional buffers.  It accepts a zero-byte copy as long as
     * both offsets themselves are in range. */
    if (source->format == SVGA3D_BUFFER &&
        source_image->size.height == 1 && source_image->size.depth == 1 &&
        destination->format == SVGA3D_BUFFER &&
        destination_image->size.height == 1 &&
        destination_image->size.depth == 1 &&
        command->srcX < source_image->pitch &&
        command->width <= source_image->pitch - command->srcX &&
        command->destX < destination_image->pitch &&
        command->width <= destination_image->pitch - command->destX) {
        memcpy(destination_image->data + command->destX,
               source_image->data + command->srcX, command->width);
        copy_ok = true;
    }

    native_box.left = 0;
    native_box.top = 0;
    native_box.front = 0;
    native_box.right = destination_image->size.width;
    native_box.bottom = destination_image->size.height;
    native_box.back = destination_image->size.depth;

    /* VBox unmaps the destination as written even after a type or range error,
     * so preserve that observable writeback ordering for resident resources. */
    update_ok = vmsvga3d_dxvk_d3d11_update_subresource(
        s->dxvk, destination->dxvk_surface, 0, &native_box,
        destination_image->data, destination_image->pitch,
        destination_image->plane_size);

    return copy_ok && update_ok;
}

static bool vmsvga3d_d3d10_transfer_from_buffer_live(
    struct vmsvga_state_s *s,
    const SVGA3dCmdDXTransferFromBuffer *command)
{
    VMSVGA3DSurface *source;
    VMSVGA3DSurface *destination;
    VMSVGA3DSurfaceImage *source_image;
    VMSVGA3DSurfaceImage *destination_image;
    VMSVGA3DD3D10UpdateLayout layout;
    VMSVGA3DD3D10Box native_box;
    uint32_t source_pitch;
    uint32_t row_copy;
    uint32_t z;
    uint32_t y;
    bool copy_ok = true;
    bool update_ok;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        command->srcSid >= SVGA3D_MAX_SURFACE_IDS ||
        command->destSid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    source = s->svga3d->surfaces[command->srcSid];
    destination = s->svga3d->surfaces[command->destSid];

    if (source == NULL || destination == NULL || source->mips == NULL ||
        destination->mips == NULL || source->mip_count == 0 ||
        command->destSubResource >= destination->mip_count) {
        return false;
    }

    source_image = &source->mips[0];
    destination_image = &destination->mips[command->destSubResource];

    /* Current VirtualBox explicitly rejects a non-buffer source here. */
    if (source->format != SVGA3D_BUFFER || source_image->data == NULL ||
        source_image->size.height != 1 || source_image->size.depth != 1 ||
        source_image->data_size == 0 || destination_image->data == NULL ||
        !vmsvga3d_d3d10_update_box_live(
            destination, destination_image, &command->destBox, &layout) ||
        !vmsvga3d_d3d10_readback_image_live(s, source, 0)) {
        return false;
    }

    if (command->srcOffset > source_image->data_size) {
        return false;
    }

    /* VBox treats a zero srcPitch as an exact-row mapping for planar formats. */
    source_pitch = command->srcPitch != 0 ? command->srcPitch : layout.row_bytes;
    row_copy = MIN(source_pitch, layout.row_bytes);
    if (row_copy == 0) {
        return false;
    }

    for (z = 0; z < layout.depth_count && copy_ok; z++) {
        uint64_t source_slice = (uint64_t)command->srcOffset +
                                (uint64_t)z * command->srcSlicePitch;

        for (y = 0; y < layout.row_count; y++) {
            uint64_t source_offset = source_slice + (uint64_t)y * source_pitch;
            uint64_t destination_offset = (uint64_t)layout.box_offset +
                                          (uint64_t)z * destination_image->plane_size +
                                          (uint64_t)y * destination_image->pitch;

            if (source_offset >= source_image->data_size ||
                row_copy > source_image->data_size - source_offset ||
                destination_offset > destination_image->data_size ||
                row_copy > destination_image->data_size - destination_offset) {
                copy_ok = false;
                break;
            }
            memcpy(destination_image->data + destination_offset,
                   source_image->data + source_offset, row_copy);
        }
    }

    native_box.left = layout.box.x;
    native_box.top = layout.box.y;
    native_box.front = layout.box.z;
    native_box.right = layout.box.x + layout.box.w;
    native_box.bottom = layout.box.y + layout.box.h;
    native_box.back = layout.box.z + layout.box.d;

    /* VirtualBox unmaps the writable destination even after a row-copy error,
     * so any rows copied before the failure are still made visible. */
    update_ok = vmsvga3d_dxvk_d3d11_update_subresource(
        s->dxvk, destination->dxvk_surface, command->destSubResource,
        &native_box, destination_image->data + layout.box_offset,
        destination_image->pitch, destination_image->plane_size);

    return copy_ok && update_ok;
}


static bool vmsvga3d_d3d10_pred_copy_region_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXPredCopyRegion *command)
{
    VMSVGA3DSurface *source;
    VMSVGA3DSurface *destination;
    VMSVGA3DD3D10CopySubresourcePlan plan;
    VMSVGA3DD3D10Level level;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk) ||
        vmsvga3d_dx_context(s, cid) == NULL ||
        command->srcSid >= SVGA3D_MAX_SURFACE_IDS ||
        command->dstSid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    source = s->svga3d->surfaces[command->srcSid];
    destination = s->svga3d->surfaces[command->dstSid];

    if (source == NULL || destination == NULL ||
        command->srcSubResource >= source->mip_count ||
        command->dstSubResource >= destination->mip_count) {
        return false;
    }

    level = vmsvga3d_d3d10_copy_subresource_plan(
        source->format, destination->format, false, false,
        command->dstSubResource,
        command->srcSubResource, &source->mips[command->srcSubResource].size,
        &destination->mips[command->dstSubResource].size, &command->box, &plan);

    if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
        plan.region.clipped_box.w == 0 ||
        plan.region.clipped_box.h == 0 ||
        plan.region.clipped_box.d == 0 ||
        !vmsvga3d_d3d10_copy_surface_materialize_live(
            s, source, plan.source_create_kind) ||
        !vmsvga3d_d3d10_copy_surface_materialize_live(
            s, destination, plan.destination_create_kind)) {
        return false;
    }

    return vmsvga3d_dxvk_d3d11_copy_subresource_region(
        s->dxvk, destination->dxvk_surface, plan.destination_subresource,
        plan.region.destination_x, plan.region.destination_y,
        plan.region.destination_z, source->dxvk_surface,
        plan.source_subresource, &plan.region.source_box);
}

static bool vmsvga3d_d3d10_pred_copy_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXPredCopy *command)
{
    VMSVGA3DSurface *source;
    VMSVGA3DSurface *destination;
    VMSVGA3DD3D10CopyResourcePlan plan;
    VMSVGA3DD3D10Level level;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk) ||
        vmsvga3d_dx_context(s, cid) == NULL ||
        command->srcSid >= SVGA3D_MAX_SURFACE_IDS ||
        command->dstSid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    source = s->svga3d->surfaces[command->srcSid];
    destination = s->svga3d->surfaces[command->dstSid];

    if (source == NULL || destination == NULL) {
        return false;
    }

    level = vmsvga3d_d3d10_copy_resource_plan(
        source->format, destination->format, false, false, &plan);

    if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
        !vmsvga3d_d3d10_copy_surface_materialize_live(
            s, source, plan.source_create_kind) ||
        !vmsvga3d_d3d10_copy_surface_materialize_live(
            s, destination, plan.destination_create_kind)) {
        return false;
    }

    return vmsvga3d_dxvk_d3d11_copy_resource(
        s->dxvk, destination->dxvk_surface, source->dxvk_surface);
}

static bool vmsvga3d_d3d10_resolve_copy_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXResolveCopy *command)
{
    VMSVGA3DSurface *source;
    VMSVGA3DSurface *destination;
    VMSVGA3DD3D10Format resolve_format;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk) ||
        vmsvga3d_dx_context(s, cid) == NULL ||
        command->srcSid >= SVGA3D_MAX_SURFACE_IDS ||
        command->dstSid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    source = s->svga3d->surfaces[command->srcSid];
    destination = s->svga3d->surfaces[command->dstSid];

    if (source == NULL || destination == NULL || source->dxvk_surface == NULL ||
        destination->dxvk_surface == NULL ||
        !vmsvga3d_d3d10_copy_surface_materialize_live(
            s, source, copy_resource_create_kind(source->format)) ||
        !vmsvga3d_d3d10_copy_surface_materialize_live(
            s, destination, copy_resource_create_kind(destination->format))) {
        return false;
    }

    /* Current VirtualBox translates copyFormat but does not validate the
     * result before issuing the void ResolveSubresource call.  Preserve that
     * behavior, including DXGI_FORMAT_UNKNOWN for an unmapped format. */
    resolve_format = vmsvga3d_d3d10_surface_format(command->copyFormat);

    return vmsvga3d_dxvk_d3d11_resolve_subresource(
        s->dxvk, destination->dxvk_surface, command->dstSubResource,
        source->dxvk_surface, command->srcSubResource,
        resolve_format.dxgi_format);
}

static bool vmsvga3d_d3d10_gen_mips_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXGenMips *command)
{
    VMSVGA3DDXContext *context;
    VMSVGA3DD3D10GenMipsPlan plan;
    VMSVGA3DD3D10Level level;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return false;
    }

    context = vmsvga3d_dx_context(s, cid);
    if (context == NULL) {
        return false;
    }

    level = vmsvga3d_d3d10_gen_mips_plan(
        command->shaderResourceViewId,
        context->cotables[SVGA_COTABLE_SRVIEW].capacity_entries, &plan);
    if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
        !vmsvga3d_d3d10_srv_realize_live(s, cid, plan.view_id)) {
        return false;
    }

    return vmsvga3d_dxvk_d3d11_generate_mips(
        s->dxvk, cid, plan.view_id);
}

static bool vmsvga3d_d3d10_clear_rtv_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXClearRenderTargetView *command)
{
    VMSVGA3DDXContext *context;
    VMSVGA3DD3D10ClearRTVPlan clear_plan;
    VMSVGA3DD3D10Level level;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return false;
    }

    context = vmsvga3d_dx_context(s, cid);
    if (context == NULL) {
        return false;
    }

    level = vmsvga3d_d3d10_clear_rtv_plan(
        command->renderTargetViewId, &command->rgba,
        context->cotables[SVGA_COTABLE_RTVIEW].capacity_entries, &clear_plan);
    if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
        !vmsvga3d_d3d10_rtv_realize_live(s, cid, clear_plan.view_id)) {
        return false;
    }

    return vmsvga3d_dxvk_d3d11_clear_render_target_view(
        s->dxvk, cid, clear_plan.view_id, clear_plan.color);
}

static bool vmsvga3d_d3d10_clear_dsv_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXClearDepthStencilView *command)
{
    VMSVGA3DDXContext *context;
    VMSVGA3DD3D10ClearDSVPlan clear_plan;
    VMSVGA3DD3D10Level level;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return false;
    }

    context = vmsvga3d_dx_context(s, cid);
    if (context == NULL) {
        return false;
    }

    level = vmsvga3d_d3d10_clear_dsv_plan(
        command->flags, command->depthStencilViewId, command->depth,
        command->stencil,
        context->cotables[SVGA_COTABLE_DSVIEW].capacity_entries, &clear_plan);
    if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
        !vmsvga3d_d3d10_dsv_realize_live(s, cid, clear_plan.view_id)) {
        return false;
    }

    return vmsvga3d_dxvk_d3d11_clear_depth_stencil_view(
        s->dxvk, cid, clear_plan.view_id, clear_plan.d3d_clear_flags,
        clear_plan.depth, clear_plan.stencil);
}

static void vmsvga3d_d3d10_present_blt_clip_box(
    const SVGA3dSize *size, SVGA3dBox *box)
{
    if (box->x > size->width) {
        box->x = size->width;
    }

    if (box->w > size->width - box->x) {
        box->w = size->width - box->x;
    }

    if (box->y > size->height) {
        box->y = size->height;
    }

    if (box->h > size->height - box->y) {
        box->h = size->height - box->y;
    }

    if (box->z > size->depth) {
        box->z = size->depth;
    }

    if (box->d > size->depth - box->z) {
        box->d = size->depth - box->z;
    }
}

static bool vmsvga3d_d3d10_present_blt_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXPresentBlt *command)
{
    VMSVGA3DSurface *source;
    VMSVGA3DSurface *destination;
    VMSVGA3DSurfaceImage *source_image;
    VMSVGA3DSurfaceImage *destination_image;
    VMSVGA3DD3D10Format source_format;
    VMSVGA3DD3D10Format destination_format;
    SVGA3dBox source_box;
    SVGA3dBox destination_box;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk) ||
        vmsvga3d_dx_context(s, cid) == NULL ||
        command->boxDest.z != 0 || command->boxDest.d != 1 ||
        command->boxSrc.z != 0 || command->boxSrc.d != 1 ||
        command->srcSid >= SVGA3D_MAX_SURFACE_IDS ||
        command->dstSid >= SVGA3D_MAX_SURFACE_IDS) {
        return false;
    }

    source = s->svga3d->surfaces[command->srcSid];
    destination = s->svga3d->surfaces[command->dstSid];

    if (source == NULL || destination == NULL || source->dxvk_surface == NULL ||
        destination->dxvk_surface == NULL || source->mips == NULL ||
        destination->mips == NULL || command->srcSubResource >= source->mip_count ||
        command->destSubResource >= destination->mip_count ||
        !vmsvga3d_d3d10_copy_surface_materialize_live(
            s, source, VMSVGA3D_D3D10_CREATE_TEXTURE) ||
        !vmsvga3d_d3d10_copy_surface_materialize_live(
            s, destination, VMSVGA3D_D3D10_CREATE_TEXTURE)) {
        return false;
    }

    source_image = &source->mips[command->srcSubResource];
    destination_image = &destination->mips[command->destSubResource];
    source_box = command->boxSrc;
    destination_box = command->boxDest;

    vmsvga3d_d3d10_present_blt_clip_box(&source_image->size, &source_box);
    vmsvga3d_d3d10_present_blt_clip_box(&destination_image->size,
                                         &destination_box);
    if (destination_box.w == 0 || destination_box.h == 0 ||
        destination_box.d == 0) {
        return false;
    }

    source_format = vmsvga3d_d3d10_surface_format(source->format);
    destination_format = vmsvga3d_d3d10_surface_format(destination->format);
    if (!vmsvga3d_d3d10_level_is_vgpu10(source_format.min_level) ||
        !vmsvga3d_d3d10_level_is_vgpu10(destination_format.min_level)) {
        return false;
    }

    /* mode is intentionally ignored: VirtualBox always uses its fixed
     * anisotropic blitter and derives sRGB handling from the source format. */
    return vmsvga3d_dxvk_d3d11_present_blt(
        s->dxvk, source->dxvk_surface, command->srcSubResource,
        source_format.dxgi_format, &source_box, &source_image->size,
        vmsvga3d_d3d10_is_srgb_format(source_format.dxgi_format),
        destination->dxvk_surface, command->destSubResource,
        destination_format.dxgi_format, &destination_box,
        &destination_image->size);
}

typedef enum vmsvga3d_d3d10_query_poll_result_e {
    VMSVGA3D_D3D10_QUERY_POLL_FAILED = 0,
    VMSVGA3D_D3D10_QUERY_POLL_PENDING,
    VMSVGA3D_D3D10_QUERY_POLL_FINISHED,
} VMSVGA3DD3D10QueryPollResult;

static VMSVGA3DD3D10QueryPollResult
vmsvga3d_d3d10_query_poll_live(struct vmsvga_state_s *s, uint32_t cid,
                                uint32_t query_id)
{
    VMSVGA3DD3D10QueryExecutionPlan plan;
    SVGADXQueryResultUnion svga_result = { 0 };
    uint8_t d3d_result[sizeof(SVGADXQueryResultUnion)] = { 0 };
    uint8_t *entry;
    VMSVGA3DMob *mob;
    SVGA3dQueryType type;
    uint32_t flags;
    uint32_t mobid;
    uint32_t offset;
    uint32_t svga_result_size = 0;
    uint32_t query_state;
    bool ready = false;
    bool success = false;

    if (s == NULL || s->svga3d == NULL || !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return VMSVGA3D_D3D10_QUERY_POLL_FAILED;
    }

    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DXQUERY, query_id);

    if (entry == NULL ||
        !vmsvga3d_dxvk_d3d11_query_pending(s->dxvk, cid, query_id)) {
        return VMSVGA3D_D3D10_QUERY_POLL_FAILED;
    }

    type = (SVGA3dQueryType)entry[0];
    flags = query_read_u32(entry + 4);

    if (type >= SVGA3D_QUERYTYPE_DX10_MAX ||
        vmsvga3d_d3d10_query_execution_plan(type, flags, &plan) ==
            VMSVGA3D_D3D10_LEVEL_INVALID ||
        !plan.get_data_after_end ||
        plan.d3d_result_size > sizeof(d3d_result)) {
        return VMSVGA3D_D3D10_QUERY_POLL_FAILED;
    }

    if (vmsvga3d_dxvk_d3d11_query_get_data(
            s->dxvk, cid, query_id, d3d_result, plan.d3d_result_size,
            plan.getdata_flags, &ready)) {

        if (!ready) {
            return VMSVGA3D_D3D10_QUERY_POLL_PENDING;
        }

        success = vmsvga3d_d3d10_query_result(
                      type, d3d_result, plan.d3d_result_size, &svga_result,
                      &svga_result_size) != VMSVGA3D_D3D10_LEVEL_INVALID;
    }

    mobid = query_read_u32(entry + 8);
    offset = query_read_u32(entry + 12);
    mob = vmsvga3d_mob_get(s, mobid);

    if (mob != NULL) {
        if (success) {
            (void)vmsvga3d_mob_write(
                s, mob, offset + sizeof(uint32_t), &svga_result, svga_result_size);
        }
        query_state = success ? SVGA3D_QUERYSTATE_SUCCEEDED
                              : SVGA3D_QUERYSTATE_FAILED;
        (void)vmsvga3d_mob_write(
            s, mob, offset, &query_state, sizeof(query_state));
    }

    /* vmsvga3dDXCbFinishQuery marks both successful and failed completions
     * FINISHED.  Only S_FALSE stays on the backend pending list.  In the odd
     * repeated-BEGIN case VBox may overwrite an ACTIVE guest state here too. */
    entry[3] = SVGADX_QDSTATE_FINISHED;

    return success ? VMSVGA3D_D3D10_QUERY_POLL_FINISHED
                   : VMSVGA3D_D3D10_QUERY_POLL_FAILED;
}

static bool vmsvga3d_d3d10_query_end_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXEndQuery *command)
{
    VMSVGA3DD3D10QueryExecutionPlan plan;
    VMSVGA3DD3D10QueryPollResult poll;
    uint8_t *entry;
    SVGA3dQueryType type;
    uint32_t flags;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return false;
    }

    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DXQUERY, command->queryId);
    if (entry == NULL) {
        return false;
    }

    type = (SVGA3dQueryType)entry[0];
    flags = query_read_u32(entry + 4);

    if (type >= SVGA3D_QUERYTYPE_DX10_MAX ||
        vmsvga3d_d3d10_query_execution_plan(type, flags, &plan) ==
            VMSVGA3D_D3D10_LEVEL_INVALID) {
        return false;
    }

    if (entry[3] == SVGADX_QDSTATE_FINISHED) {
        return true;
    }

    if (entry[3] != SVGADX_QDSTATE_ACTIVE &&
        entry[3] != SVGADX_QDSTATE_IDLE) {
        return false;
    }

    /* Timestamp queries have no BEGIN, so END is the first operation that
     * exposes the pending state to the guest result MOB.  VirtualBox writes
     * that state before issuing the native End call. */
    if (type == SVGA3D_QUERYTYPE_TIMESTAMP) {
        VMSVGA3DMob *mob = vmsvga3d_mob_get(s, query_read_u32(entry + 8));

        if (mob != NULL) {
            uint32_t query_state = SVGA3D_QUERYSTATE_PENDING;

            (void)vmsvga3d_mob_write(
                s, mob, query_read_u32(entry + 12), &query_state,
                sizeof(query_state));
        }
    }

    entry[3] = SVGADX_QDSTATE_PENDING;

    if (!vmsvga3d_dxvk_d3d11_query_end(
            s->dxvk, cid, command->queryId, plan.issue_end)) {
        return false;
    }

    if (!plan.get_data_after_end) {
        /* Predicate-hint queries intentionally remain pending forever from the
         * guest-status perspective; SetPredication consumes the native predicate. */
        return true;
    }

    /* VBox probes once here.  S_OK may finish immediately, S_FALSE stays
     * pending for ProcessPendingTasks, and any real HRESULT failure finishes
     * the guest query with FAILED. */
    poll = vmsvga3d_d3d10_query_poll_live(s, cid, command->queryId);

    return poll != VMSVGA3D_D3D10_QUERY_POLL_FAILED;
}

static void vmsvga3d_d3d10_process_pending_queries(
    struct vmsvga_state_s *s)
{
    uint32_t cid;

    if (s == NULL || s->svga3d == NULL || !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return;
    }

    for (cid = 0; cid < SVGA3D_MAX_CONTEXT_IDS; cid++) {
        VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
        VMSVGA3DDXCOTable *binding;
        SVGACOTableDXQueryEntry *entries;
        uint32_t query_id;

        if (context == NULL) {
            continue;
        }

        binding = &context->cotables[SVGA_COTABLE_DXQUERY];
        if (binding->capacity_entries == 0 || binding->host == NULL ||
            binding->entry_size != sizeof(SVGACOTableDXQueryEntry) ||
            binding->host_size / sizeof(SVGACOTableDXQueryEntry) <
                binding->capacity_entries) {
            continue;
        }

        entries = (SVGACOTableDXQueryEntry *)binding->host;
        for (query_id = 0; query_id < binding->capacity_entries; query_id++) {
            VMSVGA3DD3D10QueryExecutionPlan plan;
            SVGA3dQueryType type;

            if (!vmsvga3d_dxvk_d3d11_query_pending(
                    s->dxvk, cid, query_id)) {
                continue;
            }
            {
                const uint8_t *entry = (const uint8_t *)&entries[query_id];

                type = (SVGA3dQueryType)entry[0];
                if (type >= SVGA3D_QUERYTYPE_DX10_MAX ||
                    vmsvga3d_d3d10_query_execution_plan(
                        type, query_read_u32(entry + 4), &plan) ==
                      VMSVGA3D_D3D10_LEVEL_INVALID ||
                    !plan.get_data_after_end) {
                    continue;
                }
            }
            (void)vmsvga3d_d3d10_query_poll_live(s, cid, query_id);
        }
    }
}

static bool vmsvga3d_d3d10_query_begin_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXBeginQuery *command)
{
    VMSVGA3DD3D10QueryExecutionPlan plan;
    uint8_t *entry;
    VMSVGA3DMob *mob;
    SVGA3dQueryType type;
    uint32_t flags;
    uint32_t mobid;
    uint32_t offset;
    uint32_t query_state;

    if (s == NULL || command == NULL || s->svga3d == NULL ||
        !vmsvga3d_dxvk_d3d11_ready(s->dxvk)) {
        return false;
    }

    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DXQUERY, command->queryId);
    if (entry == NULL) {
        return false;
    }

    type = (SVGA3dQueryType)entry[0];
    flags = query_read_u32(entry + 4);

    if (type >= SVGA3D_QUERYTYPE_DX10_MAX ||
        vmsvga3d_d3d10_query_execution_plan(type, flags, &plan) ==
            VMSVGA3D_D3D10_LEVEL_INVALID) {
        return false;
    }

    /* Current VirtualBox treats BEGIN(timestamp) as a complete no-op: it does
     * not issue ID3D11DeviceContext::Begin and does not change the query state.
     */
    if (!plan.issue_begin) {
        return true;
    }

    /* A second BEGIN on an active query first ends the previous interval and
     * then begins a new one.  This matters for predicate-hint queries too: END
     * leaves them pending, after which the same predicate is begun again.
     */
    if (entry[3] == SVGADX_QDSTATE_ACTIVE) {
        SVGA3dCmdDXEndQuery end_command = { .queryId = command->queryId };

        if (!vmsvga3d_d3d10_query_end_live(s, cid, &end_command)) {
            return false;
        }
    }
    if (entry[3] != SVGADX_QDSTATE_IDLE &&
        entry[3] != SVGADX_QDSTATE_PENDING &&
        entry[3] != SVGADX_QDSTATE_FINISHED) {
        return false;
    }

    mobid = query_read_u32(entry + 8);
    offset = query_read_u32(entry + 12);
    mob = vmsvga3d_mob_get(s, mobid);

    if (!vmsvga3d_dxvk_d3d11_query_begin(
            s->dxvk, cid, command->queryId, true)) {
        if (mob != NULL) {
            query_state = SVGA3D_QUERYSTATE_FAILED;
            (void)vmsvga3d_mob_write(
                s, mob, offset, &query_state, sizeof(query_state));
        }
        return false;
    }

    entry[3] = SVGADX_QDSTATE_ACTIVE;

    if (mob != NULL) {
        query_state = SVGA3D_QUERYSTATE_PENDING;
        (void)vmsvga3d_mob_write(
            s, mob, offset, &query_state, sizeof(query_state));
    }

    return true;
}

static bool vmsvga3d_d3d10_bind_all_query_live(
    struct vmsvga_state_s *s, const SVGA3dCmdDXBindAllQuery *command)
{
    VMSVGA3DDXContext *context;
    VMSVGA3DDXCOTable *binding;
    SVGACOTableDXQueryEntry *entries;
    uint32_t i;

    if (s == NULL || command == NULL) {
        return false;
    }

    context = vmsvga3d_dx_context(s, command->cid);
    if (context == NULL) {
        return false;
    }

    binding = &context->cotables[SVGA_COTABLE_DXQUERY];
    if (binding->capacity_entries == 0) {
        return true;
    }

    if (binding->host == NULL ||
        binding->entry_size != sizeof(SVGACOTableDXQueryEntry) ||
        binding->host_size / sizeof(SVGACOTableDXQueryEntry) <
            binding->capacity_entries) {
        return false;
    }

    entries = (SVGACOTableDXQueryEntry *)binding->host;

    for (i = 0; i < binding->capacity_entries; i++) {
        if (entries[i].type != SVGA3D_QUERYTYPE_INVALID) {
            entries[i].mobid = command->mobid;
        }
    }

    return true;
}

static bool vmsvga3d_d3d10_readback_all_query_live(
    struct vmsvga_state_s *s, const SVGA3dCmdDXReadbackAllQuery *command)
{
    return s != NULL && command != NULL &&
           vmsvga3d_dx_context(s, command->cid) != NULL;
}

static bool vmsvga3d_d3d10_mob_fence64_live(
    struct vmsvga_state_s *s, const SVGA3dCmdDXMobFence64 *command)
{
    VMSVGA3DMob *mob;

    if (s == NULL || command == NULL) {
        return false;
    }

    mob = vmsvga3d_mob_get(s, command->mobId);

    return mob != NULL &&
           vmsvga3d_mob_write(s, mob, command->mobOffset,
                              &command->value, sizeof(command->value));
}

static bool vmsvga3d_d3d10_set_predication_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXSetPredication *command)
{
    VMSVGA3DD3D10PredicationPlan plan;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    SVGA3dQueryType type = SVGA3D_QUERYTYPE_INVALID;
    uint32_t flags = 0;
    uint8_t *entry = NULL;
    bool enabled;

    if (command == NULL || context == NULL) {
        return false;
    }

    enabled = command->queryId != SVGA3D_INVALID_ID;

    if (enabled) {
        entry = vmsvga3d_dx_cotable_entry_ptr(
            s, cid, SVGA_COTABLE_DXQUERY, command->queryId);
        if (entry == NULL) {
            return false;
        }
        type = (SVGA3dQueryType)entry[0];
        flags = query_read_u32(entry + 4);
    }

    if (!vmsvga3d_d3d10_level_is_vgpu10(
            vmsvga3d_d3d10_predication_plan(
                enabled, type, flags, command->predicateValue, &plan))) {
        return false;
    }

    context->shadow.predication.queryID = command->queryId;
    context->shadow.predication.value = command->predicateValue;

    return vmsvga3d_dxvk_d3d11_set_predication(
        s->dxvk, cid, command->queryId, plan.enabled, plan.predicate_value);
}

static bool vmsvga3d_d3d10_constant_buffer_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10ConstantBufferPlan *plan,
    const VMSVGA3DSurface *surface)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    uint8_t *upload = NULL;
    bool success;
    uint32_t new_start;
    uint32_t new_end;

    if (s == NULL || context == NULL || plan == NULL ||
        plan->stage_index >= SVGA3D_NUM_SHADERTYPE_DX10 ||
        plan->slot >= SVGA3D_DX_MAX_CONSTBUFFERS) {
        return false;
    }

    if (plan->unbind || plan->backend_buffer_size == 0) {
        success = vmsvga3d_dxvk_d3d11_constant_buffer_destroy(
            s->dxvk, cid, plan->stage_index, plan->slot);
    } else {
        if (!plan->create_buffer || !plan->replace_only_on_create_success ||
            !plan->preserve_old_buffer_on_create_failure ||
            plan->create_failure_is_success || surface == NULL ||
            surface->mips == NULL || surface->mip_count == 0 ||
            surface->mips[0].data == NULL ||
            plan->initial_data_offset >= surface->mips[0].data_size ||
            plan->backend_copy_size >
                surface->mips[0].data_size - plan->initial_data_offset ||
            plan->backend_copy_size > plan->backend_buffer_size) {
            return false;
        }

        upload = g_try_malloc0(plan->backend_buffer_size);
        if (upload == NULL) {
            return false;
        }

        if (plan->backend_copy_size != 0) {
            memcpy(upload,
                   surface->mips[0].data + plan->initial_data_offset,
                   plan->backend_copy_size);
        }

        success = vmsvga3d_dxvk_d3d11_constant_buffer_define(
            s->dxvk, cid, plan->stage_index, plan->slot, upload,
            plan->backend_buffer_size);
        g_free(upload);
    }

    if (!success) {
        return false;
    }

    /* Match DXCONSTANTBUFFERSTATE::StartSlot/NumBuffers: SET tracks one
     * contiguous pending range, including unchanged slots between two writes.
     */
    if (context->constant_buffer_num_buffers[plan->stage_index] == 0) {
        context->constant_buffer_start_slot[plan->stage_index] = plan->slot;
        context->constant_buffer_num_buffers[plan->stage_index] = 1;
    } else {
        new_start = MIN(context->constant_buffer_start_slot[plan->stage_index],
                        plan->slot);
        new_end = context->constant_buffer_start_slot[plan->stage_index] +
                  context->constant_buffer_num_buffers[plan->stage_index];
        new_end = MAX(new_end, plan->slot + 1u);
        context->constant_buffer_start_slot[plan->stage_index] = new_start;
        context->constant_buffer_num_buffers[plan->stage_index] =
            new_end - new_start;
    }
    return true;
}

static bool vmsvga3d_d3d10_constant_buffer_offset_live(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dShaderType type,
    const SVGA3dCmdDXSetConstantBufferOffset *command)
{
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    VMSVGA3DD3D10ConstantBufferPlan plan;
    SVGA3dConstantBufferBinding *binding;
    VMSVGA3DSurface *surface = NULL;
    uint32_t stage_index;
    uint32_t surface_bytes = 0;
    bool surface_available = false;
    bool has_surface_data = false;
    VMSVGA3DD3D10Level level;

    if (context == NULL || command == NULL ||
        command->slot >= SVGA3D_DX_MAX_CONSTBUFFERS ||
        vgpu10_shader_stage(type, &stage_index) == VMSVGA3D_D3D10_LEVEL_INVALID) {
        return false;
    }

    binding = &context->shadow.shaderState[stage_index]
                   .constantBuffers[command->slot];

    if (binding->sid != SVGA_ID_INVALID && s->svga3d != NULL &&
        binding->sid < SVGA3D_MAX_SURFACE_IDS) {
        surface = s->svga3d->surfaces[binding->sid];
        if (surface != NULL && surface->mip_count != 0 && surface->mips != NULL) {
            surface_available = true;
            surface_bytes = surface->mips[0].data_size;
            has_surface_data = surface->mips[0].data != NULL;
        }
    }

    memset(&plan, 0, sizeof(plan));

    level = vmsvga3d_d3d10_constant_buffer_plan(
        command->slot, type, binding->sid, command->offsetInBytes,
        binding->sizeInBytes, surface_available, surface_bytes,
        has_surface_data, &plan);

    if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
        if (plan.shadow_update) {
            (void)vmsvga3d_state_dx_apply_constant_buffer(s, cid, &plan);
        }
        return false;
    }

    return vmsvga3d_state_dx_apply_constant_buffer(s, cid, &plan) &&
           vmsvga3d_d3d10_constant_buffer_live(s, cid, &plan, surface);
}

static bool vmsvga3d_d3d10_samplers_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10SamplerSetPlan *plan, uint32_t bind_count)
{
    if (s == NULL || plan == NULL ||
        plan->bind_timing != VMSVGA3D_D3D10_BIND_IMMEDIATE ||
        bind_count > plan->shadow_update_count) {
        return false;
    }

    return vmsvga3d_dxvk_d3d11_set_samplers(
        s->dxvk, cid, plan->stage_index, plan->start_sampler, bind_count,
        bind_count != 0 ? plan->ids : NULL);
}

static bool vmsvga3d_d3d10_topology_live(
    struct vmsvga_state_s *s, const VMSVGA3DD3D10TopologySetPlan *plan)
{
    return s != NULL && plan != NULL && plan->immediate_bind &&
           vmsvga3d_dxvk_d3d11_set_primitive_topology(
               s->dxvk, plan->native_topology);
}

static bool vmsvga3d_d3d10_viewports_live(
    struct vmsvga_state_s *s, const VMSVGA3DD3D10ViewportsSetPlan *plan)
{
    return s != NULL && plan != NULL && plan->immediate_bind &&
           vmsvga3d_dxvk_d3d11_set_viewports(
               s->dxvk, plan->count,
               plan->count != 0 ? plan->viewports : NULL);
}

static bool vmsvga3d_d3d10_scissors_live(
    struct vmsvga_state_s *s, const VMSVGA3DD3D10ScissorPlan *plan)
{
    return s != NULL && plan != NULL && plan->immediate_bind &&
           plan->native_layout_identical &&
           vmsvga3d_dxvk_d3d11_set_scissor_rects(
               s->dxvk, plan->count,
               plan->count != 0 ? plan->rects : NULL);
}

static bool vmsvga3d_d3d10_blend_state_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10BlendStateSetPlan *plan)
{
    return s != NULL && plan != NULL && plan->immediate_bind &&
           vmsvga3d_dxvk_d3d11_set_blend_state(
               s->dxvk, cid, plan->blend_id, plan->blend_factor,
               plan->sample_mask);
}

static bool vmsvga3d_d3d10_depth_stencil_state_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10DepthStencilStateSetPlan *plan)
{
    return s != NULL && plan != NULL && plan->immediate_bind &&
           vmsvga3d_dxvk_d3d11_set_depth_stencil_state(
               s->dxvk, cid, plan->depth_stencil_id, plan->stencil_ref);
}

static bool vmsvga3d_d3d10_rasterizer_state_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10RasterizerStateSetPlan *plan)
{
    return s != NULL && plan != NULL && plan->immediate_bind &&
           vmsvga3d_dxvk_d3d11_set_rasterizer_state(
               s->dxvk, cid, plan->rasterizer_id);
}

static bool vmsvga3d_d3d10_stream_output_invalidate_bound_gs_live(
    struct vmsvga_state_s *s, VMSVGA3DDXContext *context, uint32_t cid)
{
    uint32_t stage = SVGA3D_SHADERTYPE_GS - SVGA3D_SHADERTYPE_MIN;
    uint32_t shader_id;

    if (context == NULL || stage >= SVGA3D_NUM_SHADERTYPE) {
        return false;
    }

    shader_id = context->shadow.shaderState[stage].shaderId;

    if (shader_id == SVGA3D_INVALID_ID) {
        return true;
    }

    return vmsvga3d_dxvk_d3d11_shader_invalidate(s->dxvk, cid, shader_id);
}

static bool vmsvga3d_d3d10_command(struct vmsvga_state_s *s,
                                       uint32_t cid, uint32_t cmd,
                                       const void *payload, uint32_t size)
{
    switch (cmd) {
    case SVGA_3D_CMD_DX_INVALIDATE_CONTEXT:

        if (size < sizeof(SVGA3dCmdDXInvalidateContext)) {
            return false;
        }

        /* The packet cid is ignored; command-buffer metadata selects the context. */
        return vmsvga3d_state_dx_context_invalidate(s, cid);

    case SVGA_3D_CMD_DX_CLEAR_RENDERTARGET_VIEW: {
          SVGA3dCmdDXClearRenderTargetView command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_clear_rtv_live(s, cid, &command);
      }

    case SVGA_3D_CMD_DX_CLEAR_DEPTHSTENCIL_VIEW: {
          SVGA3dCmdDXClearDepthStencilView command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_clear_dsv_live(s, cid, &command);
      }

    case SVGA_3D_CMD_DX_UPDATE_SUBRESOURCE: {
          SVGA3dCmdDXUpdateSubResource command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_update_subresource_live(s, &command);
      }

    case SVGA_3D_CMD_DX_PRESENTBLT: {
          SVGA3dCmdDXPresentBlt command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_present_blt_live(s, cid, &command);
      }

    case SVGA_3D_CMD_DX_READBACK_SUBRESOURCE: {
          SVGA3dCmdDXReadbackSubResource command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_readback_subresource_live(s, &command);
      }

    case SVGA_3D_CMD_DX_INVALIDATE_SUBRESOURCE: {
          SVGA3dCmdDXInvalidateSubResource command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_invalidate_subresource_live(s, &command);
      }

    case SVGA_3D_CMD_DX_PRED_COPY_REGION: {
          SVGA3dCmdDXPredCopyRegion command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_pred_copy_region_live(s, cid, &command);
      }

    case SVGA_3D_CMD_DX_PRED_COPY: {
          SVGA3dCmdDXPredCopy command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_pred_copy_live(s, cid, &command);
      }

    case SVGA_3D_CMD_DX_RESOLVE_COPY: {
          SVGA3dCmdDXResolveCopy command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_resolve_copy_live(s, cid, &command);
      }

    case SVGA_3D_CMD_DX_GENMIPS: {
          SVGA3dCmdDXGenMips command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          return vmsvga3d_d3d10_gen_mips_live(s, cid, &command);
      }

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
          }

          memcpy(&command, payload, sizeof(command));

          if (command.sid != SVGA_ID_INVALID && s->svga3d != NULL &&
              command.sid < SVGA3D_MAX_SURFACE_IDS) {
              surface = s->svga3d->surfaces[command.sid];
              if (surface != NULL && surface->mip_count != 0 &&
                  surface->mips != NULL) {
                  surface_available = true;
                  surface_bytes = surface->mips[0].data_size;
                  has_surface_data = surface->mips[0].data != NULL;
              }
          }

          memset(&plan, 0, sizeof(plan));

          level = vmsvga3d_d3d10_constant_buffer_plan(
              command.slot, command.type, command.sid, command.offsetInBytes,
              command.sizeInBytes, surface_available, surface_bytes,
              has_surface_data, &plan);

          if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
              /* The context shadow updates before backend surface/range checks. */
              if (plan.shadow_update) {
                  (void)vmsvga3d_state_dx_apply_constant_buffer(s, cid, &plan);
              }
              return false;
          }

          return vmsvga3d_state_dx_apply_constant_buffer(s, cid, &plan) &&
                 vmsvga3d_d3d10_constant_buffer_live(
                     s, cid, &plan, surface);
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
          }

          memcpy(&command, payload, sizeof(command));

          count = (size - header_size) / sizeof(ids[0]);
          if (count > SVGA3D_DX_MAX_SRVIEWS) {
              return false;
          }

          if (count != 0) {
              memcpy(ids, (const uint8_t *)payload + header_size,
                     count * sizeof(ids[0]));
          }

          if (vmsvga3d_d3d10_shader_resources_set_plan(
                  command.startView, command.type, count,
                  count != 0 ? ids : NULL,
                  context->cotables[SVGA_COTABLE_SRVIEW].capacity_entries,
                  &plan) == VMSVGA3D_D3D10_LEVEL_INVALID ||
              !vmsvga3d_state_dx_apply_shader_resources(s, cid, &plan)) {
              return false;
          }

          return vmsvga3d_d3d10_shader_resources_unbind_modified_live(
              s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_SET_SHADER: {
          SVGA3dCmdDXSetShader command;
          VMSVGA3DD3D10ShaderSetPlan plan;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_shader_set_plan(
                     command.shaderId, command.type,
                     context->cotables[SVGA_COTABLE_DXSHADER].capacity_entries,
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
          }

          memcpy(&command, payload, sizeof(command));
          count = (size - header_size) / sizeof(ids[0]);

          if (count > SVGA3D_DX_MAX_SAMPLERS) {
              return false;
          }

          if (count != 0) {
              memcpy(ids, (const uint8_t *)payload + header_size,
                     count * sizeof(ids[0]));
          }

          memset(&plan, 0, sizeof(plan));

          level = vmsvga3d_d3d10_samplers_set_plan(
              command.startSampler, command.type, count,
              count != 0 ? ids : NULL,
              context->cotables[SVGA_COTABLE_SAMPLER].capacity_entries, &plan);

          if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
              if (plan.partial_shadow_update_on_failure &&
                  plan.shadow_update_count != 0) {
                  (void)vmsvga3d_state_dx_apply_samplers(s, cid, &plan);
              }
              return false;
          }

          return vmsvga3d_state_dx_apply_samplers(s, cid, &plan) &&
                 vmsvga3d_d3d10_samplers_live(s, cid, &plan, count);
      }

    case SVGA_3D_CMD_DX_DRAW: {
          SVGA3dCmdDXDraw command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_draw_live(
              s, cid, command.vertexCount, command.startVertexLocation);
      }

    case SVGA_3D_CMD_DX_DRAW_INDEXED: {
          SVGA3dCmdDXDrawIndexed command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_draw_indexed_live(
              s, cid, command.indexCount, command.startIndexLocation,
              command.baseVertexLocation);
      }

    case SVGA_3D_CMD_DX_DRAW_INSTANCED: {
          SVGA3dCmdDXDrawInstanced command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_draw_instanced_live(
              s, cid, command.vertexCountPerInstance, command.instanceCount,
              command.startVertexLocation, command.startInstanceLocation);
      }

    case SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED: {
          SVGA3dCmdDXDrawIndexedInstanced command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_draw_indexed_instanced_live(
              s, cid, command.indexCountPerInstance, command.instanceCount,
              command.startIndexLocation, command.baseVertexLocation,
              command.startInstanceLocation);
      }

    case SVGA_3D_CMD_DX_DRAW_AUTO:
        /* VirtualBox enforces the minimum 4-byte command size but ignores pad0. */
        if (size < sizeof(SVGA3dCmdDXDrawAuto)) {
            return false;
        }

        return vmsvga3d_d3d10_draw_auto_live(s, cid);

    case SVGA_3D_CMD_DX_SET_INPUT_LAYOUT: {
          SVGA3dCmdDXSetInputLayout command;
          VMSVGA3DD3D10InputLayoutSetPlan plan;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_input_layout_set_plan(
                     command.elementLayoutId,
                     context->cotables[SVGA_COTABLE_ELEMENTLAYOUT].capacity_entries,
                     &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_input_layout(s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_SET_VERTEX_BUFFERS: {
          SVGA3dCmdDXSetVertexBuffers command;
          SVGA3dVertexBuffer buffers[SVGA3D_DX_MAX_VERTEXBUFFERS];
          VMSVGA3DD3D10VertexBufferSetPlan plan;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
          const uint32_t header_size = sizeof(command);
          uint32_t count;

          if (context == NULL || size < header_size) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          count = (size - header_size) / sizeof(buffers[0]);
          if (count > SVGA3D_DX_MAX_VERTEXBUFFERS) {
              return false;
          }

          if (count != 0) {
              memcpy(buffers, (const uint8_t *)payload + header_size,
                     count * sizeof(buffers[0]));
          }

          return vmsvga3d_d3d10_vertex_buffers_set_plan(
                     command.startBuffer, count, count != 0 ? buffers : NULL,
                     &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_vertex_buffers(s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_SET_INDEX_BUFFER: {
          SVGA3dCmdDXSetIndexBuffer command;
          VMSVGA3DD3D10IndexBufferSetPlan plan;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_index_buffer_set_plan(
                     command.sid, command.format, command.offset, &plan) !=
                     VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_index_buffer(s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_SET_TOPOLOGY: {
          SVGA3dCmdDXSetTopology command;
          VMSVGA3DD3D10TopologySetPlan plan;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_topology_set_plan(command.topology, &plan) !=
                     VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_topology(s, cid, &plan) &&
                 vmsvga3d_d3d10_topology_live(s, &plan);
      }

    case SVGA_3D_CMD_DX_SET_VIEWPORTS: {
          SVGA3dViewport viewports[SVGA3D_DX_MAX_VIEWPORTS];
          VMSVGA3DD3D10ViewportsSetPlan plan;
          const uint32_t header_size = sizeof(SVGA3dCmdDXSetViewports);
          uint32_t count;

          if (size < header_size) {
              return false;
          }

          count = (size - header_size) / sizeof(viewports[0]);
          if (count > SVGA3D_DX_MAX_VIEWPORTS) {
              return false;
          }

          if (count != 0) {
              memcpy(viewports, (const uint8_t *)payload + header_size,
                     count * sizeof(viewports[0]));
          }

          return vmsvga3d_d3d10_viewports_set_plan(
                     count, count != 0 ? viewports : NULL, &plan) !=
                     VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_viewports(s, cid, &plan) &&
                 vmsvga3d_d3d10_viewports_live(s, &plan);
      }

    case SVGA_3D_CMD_DX_SET_SCISSORRECTS: {
          SVGASignedRect rects[SVGA3D_DX_MAX_SCISSORRECTS];
          VMSVGA3DD3D10ScissorPlan plan;
          const uint32_t header_size = sizeof(SVGA3dCmdDXSetScissorRects);
          uint32_t count;

          if (size < header_size) {
              return false;
          }

          count = (size - header_size) / sizeof(rects[0]);
          if (count > SVGA3D_DX_MAX_SCISSORRECTS) {
              return false;
          }

          if (count != 0) {
              memcpy(rects, (const uint8_t *)payload + header_size,
                     count * sizeof(rects[0]));
          }

          return vmsvga3d_d3d10_scissor_plan(
                     count, count != 0 ? rects : NULL, &plan) !=
                     VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_scissors(s, cid, &plan) &&
                 vmsvga3d_d3d10_scissors_live(s, &plan);
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
          }

          memcpy(&command, payload, sizeof(command));

          count = (size - header_size) / sizeof(ids[0]);
          if (count > SVGA3D_MAX_RENDER_TARGETS) {
              return false;
          }

          if (count != 0) {
              memcpy(ids, (const uint8_t *)payload + header_size,
                     count * sizeof(ids[0]));
          }

          return vmsvga3d_d3d10_render_targets_set_plan(
                     command.depthStencilViewId, count,
                     count != 0 ? ids : NULL,
                     context->cotables[SVGA_COTABLE_DSVIEW].capacity_entries,
                     context->cotables[SVGA_COTABLE_RTVIEW].capacity_entries,
                     context->render_target_count, &plan) !=
                     VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_render_targets(s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_SET_PREDICATION: {
          SVGA3dCmdDXSetPredication command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_set_predication_live(s, cid, &command);
      }

    case SVGA_3D_CMD_DX_SET_SOTARGETS: {
          SVGA3dSoTarget targets[SVGA3D_DX_MAX_SOTARGETS];
          VMSVGA3DD3D10SOTargetsPlan plan;
          const uint32_t header_size = sizeof(SVGA3dCmdDXSetSOTargets);
          uint32_t count;

          if (size < header_size) {
              return false;
          }

          count = (size - header_size) / sizeof(targets[0]);
          if (count > SVGA3D_DX_MAX_SOTARGETS) {
              return false;
          }

          if (count != 0) {
              memcpy(targets, (const uint8_t *)payload + header_size,
                     count * sizeof(targets[0]));
          }

          if (vmsvga3d_d3d10_so_targets_plan(
                  count, count != 0 ? targets : NULL, &plan) ==
              VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          /* VirtualBox stores the guest shadow before attempting backend binding. */
          if (!vmsvga3d_state_dx_apply_so_targets(s, cid, &plan)) {
              return false;
          }

          return vmsvga3d_d3d10_so_targets_bind_live(s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_SET_STREAMOUTPUT: {
          SVGA3dCmdDXSetStreamOutput command;
          VMSVGA3DD3D10StreamOutputSetPlan plan;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          if (vmsvga3d_d3d10_stream_output_set_plan(
                  command.soid,
                  context->cotables[SVGA_COTABLE_STREAMOUTPUT].capacity_entries,
                  &plan) == VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          if (context->shadow.streamOut.soid != command.soid &&
              !vmsvga3d_d3d10_stream_output_invalidate_bound_gs_live(
                  s, context, cid)) {
              return false;
          }

          return vmsvga3d_state_dx_apply_stream_output(s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_DEFINE_SHADERRESOURCE_VIEW: {
          SVGA3dCmdDXDefineShaderResourceView command;
          SVGACOTableDXSRViewEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
          bool found = false;
          uint32_t stage;
          uint32_t slot;

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_SRVIEW, command.shaderResourceViewId);
          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_shader_resource_view_destroy(
                  s->dxvk, cid, command.shaderResourceViewId)) {
              return false;
          }

          for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE && !found; stage++) {
              for (slot = 0; slot < context->shader_resource_max_bound[stage]; slot++) {
                  if (context->shadow.shaderState[stage].shaderResources[slot] ==
                      command.shaderResourceViewId) {
                      context->renderer_dirty |=
                          VMSVGA3D_DX_CTX_F_STATE_SRV_VS << stage;
                      found = true;
                      break;
                  }
              }
          }

          return vmsvga3d_d3d10_srv_define_entry(&command, entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_DESTROY_SHADERRESOURCE_VIEW: {
          SVGA3dCmdDXDestroyShaderResourceView command;
          SVGACOTableDXSRViewEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
          uint32_t stage;
          uint32_t slot;

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_SRVIEW, command.shaderResourceViewId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_shader_resource_view_destroy(
                  s->dxvk, cid, command.shaderResourceViewId) ||
              vmsvga3d_d3d10_srv_destroy_entry(entry) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE; stage++) {
              for (slot = 0; slot < context->shader_resource_max_bound[stage]; slot++) {
                  if (context->shadow.shaderState[stage].shaderResources[slot] ==
                      command.shaderResourceViewId) {
                      context->shadow.shaderState[stage].shaderResources[slot] =
                          SVGA3D_INVALID_ID;
                      context->renderer_dirty |=
                          VMSVGA3D_DX_CTX_F_STATE_SRV_VS << stage;
                  }
              }
          }

          return true;
      }

    case SVGA_3D_CMD_DX_DEFINE_RENDERTARGET_VIEW: {
          SVGA3dCmdDXDefineRenderTargetView command;
          SVGACOTableDXRTViewEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
          uint32_t slot;

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_RTVIEW, command.renderTargetViewId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_render_target_view_destroy(
                  s->dxvk, cid, command.renderTargetViewId)) {
              return false;
          }

          for (slot = 0; slot < SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS; slot++) {
              if (context->shadow.renderState.renderTargetViewIds[slot] ==
                  command.renderTargetViewId) {
                  context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET;
                  break;
              }
          }

          return vmsvga3d_d3d10_rtv_define_entry(&command, entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_DESTROY_RENDERTARGET_VIEW: {
          SVGA3dCmdDXDestroyRenderTargetView command;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
          SVGACOTableDXRTViewEntry *entry;

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_RTVIEW, command.renderTargetViewId);

          return entry != NULL &&
                 vmsvga3d_d3d10_rtv_destroy_entry(entry) !=
                     VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_d3d10_rtv_destroy_shadow_refs(
                     command.renderTargetViewId,
                     context->shadow.renderState.renderTargetViewIds) !=
                     VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_dxvk_d3d11_render_target_view_destroy(
                     s->dxvk, cid, command.renderTargetViewId);
      }

    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW: {
          SVGA3dCmdDXDefineDepthStencilView command;
          SVGA3dCmdDXDefineDepthStencilView_v2 command_v2;
          SVGACOTableDXDSViewEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          memset(&command_v2, 0, sizeof(command_v2));

          command_v2.depthStencilViewId = command.depthStencilViewId;
          command_v2.sid = command.sid;
          command_v2.format = command.format;
          command_v2.resourceDimension = command.resourceDimension;
          command_v2.mipSlice = command.mipSlice;
          command_v2.firstArraySlice = command.firstArraySlice;
          command_v2.arraySize = command.arraySize;
          command_v2.flags = 0;

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DSVIEW, command.depthStencilViewId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_depth_stencil_view_destroy(
                  s->dxvk, cid, command.depthStencilViewId)) {
              return false;
          }

          if (context->shadow.renderState.depthStencilViewId ==
              command.depthStencilViewId) {
              context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET;
          }

          return vmsvga3d_d3d10_dsv_define_entry(&command_v2, entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW_V2: {
          SVGA3dCmdDXDefineDepthStencilView_v2 command;
          SVGACOTableDXDSViewEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DSVIEW, command.depthStencilViewId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_depth_stencil_view_destroy(
                  s->dxvk, cid, command.depthStencilViewId)) {
              return false;
          }

          if (context->shadow.renderState.depthStencilViewId ==
              command.depthStencilViewId) {
              context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET;
          }

          return vmsvga3d_d3d10_dsv_define_entry(&command, entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_VIEW: {
          SVGA3dCmdDXDestroyDepthStencilView command;
          SVGACOTableDXDSViewEntry *entry;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DSVIEW, command.depthStencilViewId);

          return entry != NULL &&
                 vmsvga3d_d3d10_dsv_destroy_entry(entry) !=
                     VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_dxvk_d3d11_depth_stencil_view_destroy(
                     s->dxvk, cid, command.depthStencilViewId);
      }

    case SVGA_3D_CMD_DX_DEFINE_ELEMENTLAYOUT: {
          SVGA3dCmdDXDefineElementLayout command;
          SVGACOTableDXElementLayoutEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
          const uint32_t header_size = sizeof(command);
          uint32_t count;
          const SVGA3dInputElementDesc *descs;

          if (context == NULL || size < header_size) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          count = (size - header_size) / sizeof(SVGA3dInputElementDesc);
          descs = count != 0
                      ? (const SVGA3dInputElementDesc *)((const uint8_t *)payload +
                                                         header_size)
                      : NULL;

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_ELEMENTLAYOUT, command.elementLayoutId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_input_layout_destroy(
                  s->dxvk, cid, command.elementLayoutId)) {
              return false;
          }

          if (context->shadow.inputAssembly.layoutId == command.elementLayoutId) {
              context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_INPUTLAYOUT;
          }

          return vmsvga3d_d3d10_element_layout_define_entry(
                     command.elementLayoutId, count, descs, entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_DESTROY_ELEMENTLAYOUT: {
          SVGA3dCmdDXDestroyElementLayout command;
          SVGACOTableDXElementLayoutEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_ELEMENTLAYOUT, command.elementLayoutId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_input_layout_destroy(
                  s->dxvk, cid, command.elementLayoutId) ||
              vmsvga3d_d3d10_element_layout_destroy_entry(entry) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          if (context->shadow.inputAssembly.layoutId == command.elementLayoutId) {
              context->shadow.inputAssembly.layoutId = SVGA3D_INVALID_ID;
              context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_INPUTLAYOUT;
          }

          return true;
      }

    case SVGA_3D_CMD_DX_DEFINE_BLEND_STATE: {
          SVGA3dCmdDXDefineBlendState command;
          SVGACOTableDXBlendStateEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_BLENDSTATE, command.blendId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_blend_state_destroy(s->dxvk, cid,
                                                        command.blendId)) {
              return false;
          }

          if (context->shadow.renderState.blendStateId == command.blendId) {
              context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE;
          }

          return vmsvga3d_d3d10_blend_define_entry(&command, entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_DESTROY_BLEND_STATE: {
          SVGA3dCmdDXDestroyBlendState command;
          SVGACOTableDXBlendStateEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_BLENDSTATE, command.blendId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_blend_state_destroy(s->dxvk, cid,
                                                        command.blendId) ||
              vmsvga3d_d3d10_blend_destroy_entry(entry) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          if (context->shadow.renderState.blendStateId == command.blendId) {
              context->shadow.renderState.blendStateId = SVGA3D_INVALID_ID;
              context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE;
          }

          return true;
      }

    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_STATE: {
          SVGA3dCmdDXDefineDepthStencilState command;
          SVGACOTableDXDepthStencilEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DEPTHSTENCIL, command.depthStencilId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_depth_stencil_state_destroy(
                  s->dxvk, cid, command.depthStencilId)) {
              return false;
          }

          if (context->shadow.renderState.depthStencilStateId ==
              command.depthStencilId) {
              context->renderer_dirty |=
                  VMSVGA3D_DX_CTX_F_STATE_DEPTHSTENCILSTATE;
          }

          return vmsvga3d_d3d10_depth_stencil_define_entry(&command, entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_STATE: {
          SVGA3dCmdDXDestroyDepthStencilState command;
          SVGACOTableDXDepthStencilEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DEPTHSTENCIL, command.depthStencilId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_depth_stencil_state_destroy(
                  s->dxvk, cid, command.depthStencilId) ||
              vmsvga3d_d3d10_depth_stencil_destroy_entry(entry) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          if (context->shadow.renderState.depthStencilStateId ==
              command.depthStencilId) {
              context->shadow.renderState.depthStencilStateId = SVGA3D_INVALID_ID;
              context->renderer_dirty |=
                  VMSVGA3D_DX_CTX_F_STATE_DEPTHSTENCILSTATE;
          }

          return true;
      }

    case SVGA_3D_CMD_DX_DEFINE_RASTERIZER_STATE: {
          SVGA3dCmdDXDefineRasterizerState command;
          SVGACOTableDXRasterizerStateEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_RASTERIZERSTATE, command.rasterizerId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_rasterizer_state_destroy(
                  s->dxvk, cid, command.rasterizerId)) {
              return false;
          }

          if (context->shadow.renderState.rasterizerStateId ==
              command.rasterizerId) {
              context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RASTERIZERSTATE;
          }

          return vmsvga3d_d3d10_rasterizer_define_entry(&command, entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_DESTROY_RASTERIZER_STATE: {
          SVGA3dCmdDXDestroyRasterizerState command;
          SVGACOTableDXRasterizerStateEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_RASTERIZERSTATE, command.rasterizerId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_rasterizer_state_destroy(
                  s->dxvk, cid, command.rasterizerId) ||
              vmsvga3d_d3d10_rasterizer_destroy_entry(entry) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          if (context->shadow.renderState.rasterizerStateId ==
              command.rasterizerId) {
              context->shadow.renderState.rasterizerStateId = SVGA3D_INVALID_ID;
              context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RASTERIZERSTATE;
          }

          return true;
      }

    case SVGA_3D_CMD_DX_DEFINE_SAMPLER_STATE: {
          SVGA3dCmdDXDefineSamplerState command;
          SVGACOTableDXSamplerEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
          uint32_t stage;
          uint32_t slot;

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));
          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_SAMPLER, command.samplerId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_sampler_state_destroy(
                  s->dxvk, cid, command.samplerId)) {
              return false;
          }

          for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE; stage++) {
              for (slot = 0; slot < SVGA3D_DX_MAX_SAMPLERS; slot++) {
                  if (context->shadow.shaderState[stage].samplers[slot] ==
                      command.samplerId) {
                      context->renderer_dirty |=
                          VMSVGA3D_DX_CTX_F_STATE_SAMPLER_VS << stage;
                      break;
                  }
              }
          }

          return vmsvga3d_d3d10_sampler_define_entry(&command, entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_DESTROY_SAMPLER_STATE: {
          SVGA3dCmdDXDestroySamplerState command;
          SVGACOTableDXSamplerEntry *entry;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
          uint32_t stage;
          uint32_t slot;

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_SAMPLER, command.samplerId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_sampler_state_destroy(
                  s->dxvk, cid, command.samplerId) ||
              vmsvga3d_d3d10_sampler_destroy_entry(entry) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          for (stage = 0; stage < SVGA3D_NUM_SHADERTYPE; stage++) {
              for (slot = 0; slot < SVGA3D_DX_MAX_SAMPLERS; slot++) {
                  if (context->shadow.shaderState[stage].samplers[slot] ==
                      command.samplerId) {
                      context->shadow.shaderState[stage].samplers[slot] =
                          SVGA3D_INVALID_ID;
                      context->renderer_dirty |=
                          VMSVGA3D_DX_CTX_F_STATE_SAMPLER_VS << stage;
                  }
              }
          }

          return true;
      }

    case SVGA_3D_CMD_DX_SET_BLEND_STATE: {
          SVGA3dCmdDXSetBlendState command;
          VMSVGA3DD3D10BlendStateSetPlan plan;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_blend_state_set_plan(
                     &command,
                     context->cotables[SVGA_COTABLE_BLENDSTATE].capacity_entries,
                     &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_blend_state(s, cid, &plan) &&
                 vmsvga3d_d3d10_blend_state_live(s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_SET_DEPTHSTENCIL_STATE: {
          SVGA3dCmdDXSetDepthStencilState command;
          VMSVGA3DD3D10DepthStencilStateSetPlan plan;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_depth_stencil_state_set_plan(
                     &command,
                     context->cotables[SVGA_COTABLE_DEPTHSTENCIL].capacity_entries,
                     &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_depth_stencil_state(s, cid, &plan) &&
                 vmsvga3d_d3d10_depth_stencil_state_live(s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_SET_RASTERIZER_STATE: {
          SVGA3dCmdDXSetRasterizerState command;
          VMSVGA3DD3D10RasterizerStateSetPlan plan;
          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          if (context == NULL || size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_rasterizer_state_set_plan(
                     command.rasterizerId,
                     context->cotables[SVGA_COTABLE_RASTERIZERSTATE].capacity_entries,
                     &plan) != VMSVGA3D_D3D10_LEVEL_INVALID &&
                 vmsvga3d_state_dx_apply_rasterizer_state(s, cid, &plan) &&
                 vmsvga3d_d3d10_rasterizer_state_live(s, cid, &plan);
      }

    case SVGA_3D_CMD_DX_DEFINE_QUERY: {
          SVGA3dCmdDXDefineQuery command;
          VMSVGA3DD3D10QueryExecutionPlan plan;
          VMSVGA3DD3D10Level level;
          void *entry;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DXQUERY, command.queryId);
          level = vmsvga3d_d3d10_query_execution_plan(
              command.type, command.flags, &plan);

          if (entry == NULL || !vmsvga3d_d3d10_level_is_vgpu10(level)) {
              return false;
          }

          (void)vmsvga3d_dxvk_d3d11_query_destroy(s->dxvk, cid, command.queryId);
          if (vmsvga3d_d3d10_query_define_entry(&command, entry) ==
              VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          return vmsvga3d_dxvk_d3d11_query_define(
              s->dxvk, cid, command.queryId, plan.d3d_query, plan.misc_flags);
      }

    case SVGA_3D_CMD_DX_DESTROY_QUERY: {
          SVGA3dCmdDXDestroyQuery command;
          void *entry;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DXQUERY, command.queryId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_query_destroy(s->dxvk, cid, command.queryId)) {
              return false;
          }

          return vmsvga3d_d3d10_query_destroy_entry(entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_BEGIN_QUERY: {
          SVGA3dCmdDXBeginQuery command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_query_begin_live(s, cid, &command);
      }

    case SVGA_3D_CMD_DX_END_QUERY: {
          SVGA3dCmdDXEndQuery command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_query_end_live(s, cid, &command);
      }

    case SVGA_3D_CMD_DX_BIND_QUERY: {
          SVGA3dCmdDXBindQuery command;
          VMSVGA3DMob *mob;
          void *entry;
          uint32_t mobid;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DXQUERY, command.queryId);

          if (entry == NULL) {
              return false;
          }

          /* A nonexistent backing MOB is treated as an unbind. */
          mob = vmsvga3d_mob_get(s, command.mobid);
          mobid = mob != NULL ? command.mobid : SVGA3D_INVALID_ID;

          return vmsvga3d_d3d10_query_bind_entry(entry, mobid) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_SET_QUERY_OFFSET: {
          SVGA3dCmdDXSetQueryOffset command;
          void *entry;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DXQUERY, command.queryId);

          return entry != NULL &&
                 vmsvga3d_d3d10_query_set_offset(entry, command.mobOffset) !=
                     VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_READBACK_QUERY: {
          SVGA3dCmdDXReadbackQuery command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          /* The device does not cache queries; pending completion is handled by
           * the renderer task pump, so explicit readback remains a validated NOP. */
          return vmsvga3d_dx_cotable_entry_ptr(
                     s, cid, SVGA_COTABLE_DXQUERY, command.queryId) != NULL;
      }

    case SVGA_3D_CMD_DX_BIND_ALL_QUERY: {
          SVGA3dCmdDXBindAllQuery command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_bind_all_query_live(s, &command);
      }

    case SVGA_3D_CMD_DX_BUFFER_COPY: {
          SVGA3dCmdDXBufferCopy command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_buffer_copy_live(s, &command);
      }

    case SVGA_3D_CMD_DX_TRANSFER_FROM_BUFFER: {
          SVGA3dCmdDXTransferFromBuffer command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_transfer_from_buffer_live(s, &command);
      }

    case SVGA_3D_CMD_DX_PRED_TRANSFER_FROM_BUFFER: {
          SVGA3dCmdDXPredTransferFromBuffer predicated;
          SVGA3dCmdDXTransferFromBuffer command;

          if (size < sizeof(predicated)) {
              return false;
          }

          memcpy(&predicated, payload, sizeof(predicated));

          /* The context is implied by the command-buffer header, but VirtualBox's
           * implementation deliberately ignores it and reuses the context-less
           * transfer handler byte-for-byte. */
          command.srcSid = predicated.srcSid;
          command.srcOffset = predicated.srcOffset;
          command.srcPitch = predicated.srcPitch;
          command.srcSlicePitch = predicated.srcSlicePitch;
          command.destSid = predicated.destSid;
          command.destSubResource = predicated.destSubResource;
          command.destBox = predicated.destBox;

          return vmsvga3d_d3d10_transfer_from_buffer_live(s, &command);
      }

    case SVGA_3D_CMD_DX_READBACK_ALL_QUERY: {
          SVGA3dCmdDXReadbackAllQuery command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_readback_all_query_live(s, &command);
      }

    case SVGA_3D_CMD_DX_DEFINE_SHADER: {
          SVGA3dCmdDXDefineShader command;
          SVGACOTableDXShaderEntry *entry;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DXSHADER, command.shaderId);

          if (entry == NULL || command.sizeInBytes < 8 ||
              command.type < SVGA3D_SHADERTYPE_MIN ||
              command.type >= SVGA3D_SHADERTYPE_MAX) {
              return false;
          }

          if (!vmsvga3d_dxvk_d3d11_shader_destroy(
                  s->dxvk, cid, command.shaderId) ||
              vmsvga3d_d3d10_shader_define_entry(&command, entry) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          return vmsvga3d_dxvk_d3d11_shader_object_define(
              s->dxvk, cid, command.shaderId, command.type);
      }

    case SVGA_3D_CMD_DX_DESTROY_SHADER: {
          SVGA3dCmdDXDestroyShader command;
          SVGACOTableDXShaderEntry *entry;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_DXSHADER, command.shaderId);

          if (entry == NULL ||
              !vmsvga3d_dxvk_d3d11_shader_destroy(
                  s->dxvk, cid, command.shaderId)) {
              return false;
          }

          return vmsvga3d_d3d10_shader_destroy_entry(entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_BIND_SHADER: {
          SVGA3dCmdDXBindShader command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_shader_bind_live(s, &command);
      }

    case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT: {
          SVGA3dCmdDXDefineStreamOutput command;
          SVGACOTableDXStreamOutputEntry *entry;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_STREAMOUTPUT, command.soid);

          if (entry == NULL ||
              vmsvga3d_d3d10_stream_output_legacy_entry(&command, entry) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID ||
              !vmsvga3d_dxvk_d3d11_stream_output_destroy(
                  s->dxvk, cid, command.soid)) {
              return false;
          }

          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          return context != NULL &&
                 (context->shadow.streamOut.soid != command.soid ||
                  vmsvga3d_d3d10_stream_output_invalidate_bound_gs_live(
                      s, context, cid));
      }

    case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT_WITH_MOB: {
          SVGA3dCmdDXDefineStreamOutputWithMob command;
          SVGACOTableDXStreamOutputEntry *entry;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_STREAMOUTPUT, command.soid);

          if (entry == NULL ||
              vmsvga3d_d3d10_stream_output_mob_entry(&command, entry) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID ||
              !vmsvga3d_dxvk_d3d11_stream_output_destroy(
                  s->dxvk, cid, command.soid)) {
              return false;
          }

          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          return context != NULL &&
                 (context->shadow.streamOut.soid != command.soid ||
                  vmsvga3d_d3d10_stream_output_invalidate_bound_gs_live(
                      s, context, cid));
      }

    case SVGA_3D_CMD_DX_BIND_STREAMOUTPUT: {
          SVGA3dCmdDXBindStreamOutput command;
          SVGACOTableDXStreamOutputEntry *entry;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_STREAMOUTPUT, command.soid);

          if (entry == NULL ||
              vmsvga3d_d3d10_stream_output_bind_entry(
                  entry, command.mobid, command.offsetInBytes, command.sizeInBytes) ==
                  VMSVGA3D_D3D10_LEVEL_INVALID) {
              return false;
          }

          if (!vmsvga3d_dxvk_d3d11_stream_output_destroy(
                  s->dxvk, cid, command.soid)) {
              return false;
          }

          VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

          return context != NULL &&
                 (context->shadow.streamOut.soid != command.soid ||
                  vmsvga3d_d3d10_stream_output_invalidate_bound_gs_live(
                      s, context, cid));
      }

    case SVGA_3D_CMD_DX_DESTROY_STREAMOUTPUT: {
          SVGA3dCmdDXDestroyStreamOutput command;
          SVGACOTableDXStreamOutputEntry *entry;
          VMSVGA3DDXContext *context;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          entry = vmsvga3d_dx_cotable_entry_ptr(
              s, cid, SVGA_COTABLE_STREAMOUTPUT, command.soid);

          context = vmsvga3d_dx_context(s, cid);

          if (entry == NULL || context == NULL ||
              !vmsvga3d_dxvk_d3d11_stream_output_destroy(
                  s->dxvk, cid, command.soid) ||
              (context->shadow.streamOut.soid == command.soid &&
               !vmsvga3d_d3d10_stream_output_invalidate_bound_gs_live(
                   s, context, cid))) {
              return false;
          }

          return vmsvga3d_d3d10_stream_output_destroy_entry(entry) !=
                 VMSVGA3D_D3D10_LEVEL_INVALID;
      }

    case SVGA_3D_CMD_DX_MOB_FENCE_64: {
          SVGA3dCmdDXMobFence64 command;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          return vmsvga3d_d3d10_mob_fence64_live(s, &command);
      }

    case SVGA_3D_CMD_DX_HINT:
        /* Residency hints are advisory.  The renderer may ignore both the hint
         * and its variable-sized payload without changing guest-visible state.
         */
        return size >= sizeof(SVGA3dCmdDXHint) &&
               vmsvga3d_dx_context(s, cid) != NULL;

    case SVGA_3D_CMD_DX_SET_VS_CONSTANT_BUFFER_OFFSET:
    case SVGA_3D_CMD_DX_SET_PS_CONSTANT_BUFFER_OFFSET:
    case SVGA_3D_CMD_DX_SET_GS_CONSTANT_BUFFER_OFFSET: {
          SVGA3dCmdDXSetConstantBufferOffset command;
          SVGA3dShaderType type;

          if (size < sizeof(command)) {
              return false;
          }

          memcpy(&command, payload, sizeof(command));

          if (cmd == SVGA_3D_CMD_DX_SET_VS_CONSTANT_BUFFER_OFFSET) {
              type = SVGA3D_SHADERTYPE_VS;
          } else if (cmd == SVGA_3D_CMD_DX_SET_PS_CONSTANT_BUFFER_OFFSET) {
              type = SVGA3D_SHADERTYPE_PS;
          } else {
              type = SVGA3D_SHADERTYPE_GS;
          }

          return vmsvga3d_d3d10_constant_buffer_offset_live(
              s, cid, type, &command);
      }

    default:
        return false;
    }
}

#endif /* VMSVGA3D_D3D10_RUNTIME_INTEGRATION */
