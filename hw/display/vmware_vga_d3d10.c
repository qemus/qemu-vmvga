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
  case SVGA3D_X8R8G8B8:                 return fmt(DXGI_B8G8R8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_A8R8G8B8:                 return fmt(DXGI_B8G8R8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_R5G6B5:                   return fmt(DXGI_B5G6R5_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_X1R5G5B5:
  case SVGA3D_A1R5G5B5:                 return fmt(DXGI_B5G5R5A1_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_Z_D16:
  case SVGA3D_D16_UNORM:                return fmt(DXGI_D16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_Z_D24S8:
  case SVGA3D_Z_D24S8_INT:
  case SVGA3D_D24_UNORM_S8_UINT:        return fmt(DXGI_D24_UNORM_S8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32B32A32_TYPELESS:    return fmt(DXGI_R32G32B32A32_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32B32A32_FLOAT:       return fmt(DXGI_R32G32B32A32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32B32A32_UINT:        return fmt(DXGI_R32G32B32A32_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32B32A32_SINT:        return fmt(DXGI_R32G32B32A32_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32B32_TYPELESS:       return fmt(DXGI_R32G32B32_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32B32_FLOAT:          return fmt(DXGI_R32G32B32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32B32_UINT:           return fmt(DXGI_R32G32B32_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32B32_SINT:           return fmt(DXGI_R32G32B32_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16B16A16_TYPELESS:    return fmt(DXGI_R16G16B16A16_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16B16A16_FLOAT:       return fmt(DXGI_R16G16B16A16_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16B16A16_UNORM:       return fmt(DXGI_R16G16B16A16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16B16A16_UINT:        return fmt(DXGI_R16G16B16A16_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16B16A16_SNORM:       return fmt(DXGI_R16G16B16A16_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16B16A16_SINT:        return fmt(DXGI_R16G16B16A16_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32_TYPELESS:          return fmt(DXGI_R32G32_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32_FLOAT:             return fmt(DXGI_R32G32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32_UINT:              return fmt(DXGI_R32G32_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G32_SINT:              return fmt(DXGI_R32G32_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32G8X24_TYPELESS:        return fmt(DXGI_R32G8X24_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_D32_FLOAT_S8X24_UINT:     return fmt(DXGI_D32_FLOAT_S8X24_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32_FLOAT_X8X24:          return fmt(DXGI_R32_FLOAT_X8X24_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_X32_G8X24_UINT:           return fmt(DXGI_X32_TYPELESS_G8X24_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R10G10B10A2_TYPELESS:     return fmt(DXGI_R10G10B10A2_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R10G10B10A2_UNORM:        return fmt(DXGI_R10G10B10A2_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R10G10B10A2_UINT:         return fmt(DXGI_R10G10B10A2_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R11G11B10_FLOAT:          return fmt(DXGI_R11G11B10_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8B8A8_TYPELESS:        return fmt(DXGI_R8G8B8A8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8B8A8_UNORM:           return fmt(DXGI_R8G8B8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8B8A8_UNORM_SRGB:      return fmt(DXGI_R8G8B8A8_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8B8A8_UINT:            return fmt(DXGI_R8G8B8A8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8B8A8_SNORM:           return fmt(DXGI_R8G8B8A8_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8B8A8_SINT:            return fmt(DXGI_R8G8B8A8_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16_TYPELESS:          return fmt(DXGI_R16G16_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16_FLOAT:             return fmt(DXGI_R16G16_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16_UNORM:             return fmt(DXGI_R16G16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16_UINT:              return fmt(DXGI_R16G16_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16_SNORM:             return fmt(DXGI_R16G16_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16G16_SINT:              return fmt(DXGI_R16G16_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32_TYPELESS:             return fmt(DXGI_R32_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_D32_FLOAT:                return fmt(DXGI_D32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32_FLOAT:                return fmt(DXGI_R32_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32_UINT:                 return fmt(DXGI_R32_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R32_SINT:                 return fmt(DXGI_R32_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R24G8_TYPELESS:           return fmt(DXGI_R24G8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R24_UNORM_X8:             return fmt(DXGI_R24_UNORM_X8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_X24_G8_UINT:              return fmt(DXGI_X24_TYPELESS_G8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8_TYPELESS:            return fmt(DXGI_R8G8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8_UNORM:               return fmt(DXGI_R8G8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8_UINT:                return fmt(DXGI_R8G8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8_SNORM:               return fmt(DXGI_R8G8_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8_SINT:                return fmt(DXGI_R8G8_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16_TYPELESS:             return fmt(DXGI_R16_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16_FLOAT:                return fmt(DXGI_R16_FLOAT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16_UNORM:                return fmt(DXGI_R16_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16_UINT:                 return fmt(DXGI_R16_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16_SNORM:                return fmt(DXGI_R16_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R16_SINT:                 return fmt(DXGI_R16_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8_TYPELESS:              return fmt(DXGI_R8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8_UNORM:                 return fmt(DXGI_R8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8_UINT:                  return fmt(DXGI_R8_UINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8_SNORM:                 return fmt(DXGI_R8_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8_SINT:                  return fmt(DXGI_R8_SINT, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_A8_UNORM:                 return fmt(DXGI_A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R9G9B9E5_SHAREDEXP:       return fmt(DXGI_R9G9B9E5_SHAREDEXP, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R8G8_B8G8_UNORM:          return fmt(DXGI_R8G8_B8G8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_G8R8_G8B8_UNORM:          return fmt(DXGI_G8R8_G8B8_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC1_TYPELESS:             return fmt(DXGI_BC1_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC1_UNORM:                return fmt(DXGI_BC1_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC1_UNORM_SRGB:           return fmt(DXGI_BC1_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC2_TYPELESS:             return fmt(DXGI_BC2_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC2_UNORM:                return fmt(DXGI_BC2_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC2_UNORM_SRGB:           return fmt(DXGI_BC2_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC3_TYPELESS:             return fmt(DXGI_BC3_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC3_UNORM:                return fmt(DXGI_BC3_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC3_UNORM_SRGB:           return fmt(DXGI_BC3_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC4_TYPELESS:             return fmt(DXGI_BC4_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC4_UNORM:                return fmt(DXGI_BC4_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC4_SNORM:                return fmt(DXGI_BC4_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC5_TYPELESS:             return fmt(DXGI_BC5_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC5_UNORM:                return fmt(DXGI_BC5_UNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_BC5_SNORM:                return fmt(DXGI_BC5_SNORM, VMSVGA3D_D3D10_LEVEL_10_0);
  case SVGA3D_R10G10B10_XR_BIAS_A2_UNORM:
    return fmt(DXGI_R10G10B10_XR_BIAS_A2_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_B8G8R8A8_TYPELESS:        return fmt(DXGI_B8G8R8A8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_B5G6R5_UNORM:             return fmt(DXGI_B5G6R5_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_B5G5R5A1_UNORM:           return fmt(DXGI_B5G5R5A1_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_B8G8R8A8_UNORM:           return fmt(DXGI_B8G8R8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_B8G8R8A8_UNORM_SRGB:      return fmt(DXGI_B8G8R8A8_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_B8G8R8X8_TYPELESS:        return fmt(DXGI_B8G8R8A8_TYPELESS, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_B8G8R8X8_UNORM:           return fmt(DXGI_B8G8R8A8_UNORM, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_B8G8R8X8_UNORM_SRGB:      return fmt(DXGI_B8G8R8A8_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_10_1);
  case SVGA3D_BC6H_TYPELESS:            return fmt(DXGI_BC6H_TYPELESS, VMSVGA3D_D3D10_LEVEL_11_0);
  case SVGA3D_BC6H_UF16:                return fmt(DXGI_BC6H_UF16, VMSVGA3D_D3D10_LEVEL_11_0);
  case SVGA3D_BC6H_SF16:                return fmt(DXGI_BC6H_SF16, VMSVGA3D_D3D10_LEVEL_11_0);
  case SVGA3D_BC7_TYPELESS:             return fmt(DXGI_BC7_TYPELESS, VMSVGA3D_D3D10_LEVEL_11_0);
  case SVGA3D_BC7_UNORM:                return fmt(DXGI_BC7_UNORM, VMSVGA3D_D3D10_LEVEL_11_0);
  case SVGA3D_BC7_UNORM_SRGB:           return fmt(DXGI_BC7_UNORM_SRGB, VMSVGA3D_D3D10_LEVEL_11_0);
  default:                              return fmt(DXGI_UNKNOWN, VMSVGA3D_D3D10_LEVEL_INVALID);
  }
}

bool vmsvga3d_d3d10_is_srgb_format(uint32_t format)
{
  return format == 29u || format == 91u || format == 93u;
}

uint32_t vmsvga3d_d3d10_typeless_format(uint32_t format)
{
  switch (format) {
  case 2: case 3: case 4: return 1;
  case 6: case 7: case 8: return 5;
  case 10: case 11: case 12: case 13: case 14: return 9;
  case 16: case 17: case 18: return 15;
  case 20: case 21: case 22: return 19;
  case 24: case 25: return 23;
  case 28: case 29: case 30: case 31: case 32: return 27;
  case 34: case 35: case 36: case 37: case 38: return 33;
  case 40: case 41: case 42: case 43: return 39;
  case 45: case 46: case 47: return 44;
  case 49: case 50: case 51: case 52: return 48;
  case 54: case 55: case 56: case 57: case 58: case 59: return 53;
  case 61: case 62: case 63: case 64: return 60;
  case 71: case 72: return 70;
  case 74: case 75: return 73;
  case 77: case 78: return 76;
  case 80: case 81: return 79;
  case 83: case 84: return 82;
  case 87: case 91: return 90;
  case 88: case 93: return 92;
  case 95: case 96: return 94;
  case 98: case 99: return 97;
  default: return format;
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
  if (flags & SVGA3D_SURFACE_BIND_SHADER_RESOURCE) {
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
    desc->sample_count = 1;
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
      if (!(surface->surface_flags &
            (SVGA3D_SURFACE_STAGING_UPLOAD |
             SVGA3D_SURFACE_STAGING_DOWNLOAD |
             SVGA3D_SURFACE_HINT_DYNAMIC)) &&
          (surface->surface_flags & SVGA3D_SURFACE_HINT_STATIC) &&
          surface->has_initial_data) {
        usage = D3D10_USAGE_IMMUTABLE;
        cpu_access = 0;
      }
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

  d3d10_texture_desc(&plan->primary, surface, dimension,
                     plan->resource_format, &policy);
  d3d10_texture_companions(surface, plan);
  return level;
}

static uint32_t blend_color(uint8_t value)
{
  switch (value) {
  case SVGA3D_BLENDOP_ZERO:                return D3D10_BLEND_ZERO;
  case SVGA3D_BLENDOP_ONE:                 return D3D10_BLEND_ONE;
  case SVGA3D_BLENDOP_SRCCOLOR:            return D3D10_BLEND_SRC_COLOR;
  case SVGA3D_BLENDOP_INVSRCCOLOR:         return D3D10_BLEND_INV_SRC_COLOR;
  case SVGA3D_BLENDOP_SRCALPHA:            return D3D10_BLEND_SRC_ALPHA;
  case SVGA3D_BLENDOP_INVSRCALPHA:         return D3D10_BLEND_INV_SRC_ALPHA;
  case SVGA3D_BLENDOP_DESTALPHA:           return D3D10_BLEND_DEST_ALPHA;
  case SVGA3D_BLENDOP_INVDESTALPHA:        return D3D10_BLEND_INV_DEST_ALPHA;
  case SVGA3D_BLENDOP_DESTCOLOR:           return D3D10_BLEND_DEST_COLOR;
  case SVGA3D_BLENDOP_INVDESTCOLOR:        return D3D10_BLEND_INV_DEST_COLOR;
  case SVGA3D_BLENDOP_SRCALPHASAT:         return D3D10_BLEND_SRC_ALPHA_SAT;
  case SVGA3D_BLENDOP_BLENDFACTOR:         return D3D10_BLEND_BLEND_FACTOR;
  case SVGA3D_BLENDOP_INVBLENDFACTOR:      return D3D10_BLEND_INV_BLEND_FACTOR;
  case SVGA3D_BLENDOP_SRC1COLOR:           return D3D10_BLEND_SRC1_COLOR;
  case SVGA3D_BLENDOP_INVSRC1COLOR:        return D3D10_BLEND_INV_SRC1_COLOR;
  case SVGA3D_BLENDOP_SRC1ALPHA:           return D3D10_BLEND_SRC1_ALPHA;
  case SVGA3D_BLENDOP_INVSRC1ALPHA:        return D3D10_BLEND_INV_SRC1_ALPHA;
  case SVGA3D_BLENDOP_BLENDFACTORALPHA:    return D3D10_BLEND_BLEND_FACTOR;
  case SVGA3D_BLENDOP_INVBLENDFACTORALPHA: return D3D10_BLEND_INV_BLEND_FACTOR;
  default:                                 return D3D10_BLEND_ZERO;
  }
}

static uint32_t blend_alpha(uint8_t value)
{
  switch (value) {
  case SVGA3D_BLENDOP_ZERO:                return D3D10_BLEND_ZERO;
  case SVGA3D_BLENDOP_ONE:                 return D3D10_BLEND_ONE;
  case SVGA3D_BLENDOP_SRCCOLOR:
  case SVGA3D_BLENDOP_SRCALPHA:            return D3D10_BLEND_SRC_ALPHA;
  case SVGA3D_BLENDOP_INVSRCCOLOR:
  case SVGA3D_BLENDOP_INVSRCALPHA:         return D3D10_BLEND_INV_SRC_ALPHA;
  case SVGA3D_BLENDOP_DESTCOLOR:
  case SVGA3D_BLENDOP_DESTALPHA:           return D3D10_BLEND_DEST_ALPHA;
  case SVGA3D_BLENDOP_INVDESTCOLOR:
  case SVGA3D_BLENDOP_INVDESTALPHA:        return D3D10_BLEND_INV_DEST_ALPHA;
  case SVGA3D_BLENDOP_SRCALPHASAT:         return D3D10_BLEND_SRC_ALPHA_SAT;
  case SVGA3D_BLENDOP_BLENDFACTOR:         return D3D10_BLEND_BLEND_FACTOR;
  case SVGA3D_BLENDOP_INVBLENDFACTOR:      return D3D10_BLEND_INV_BLEND_FACTOR;
  case SVGA3D_BLENDOP_SRC1COLOR:
  case SVGA3D_BLENDOP_SRC1ALPHA:           return D3D10_BLEND_SRC1_ALPHA;
  case SVGA3D_BLENDOP_INVSRC1COLOR:
  case SVGA3D_BLENDOP_INVSRC1ALPHA:        return D3D10_BLEND_INV_SRC1_ALPHA;
  case SVGA3D_BLENDOP_BLENDFACTORALPHA:    return D3D10_BLEND_BLEND_FACTOR;
  case SVGA3D_BLENDOP_INVBLENDFACTORALPHA: return D3D10_BLEND_INV_BLEND_FACTOR;
  default:                                 return D3D10_BLEND_ZERO;
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

VMSVGA3DD3D10Level vmsvga3d_d3d10_object_lifecycle_plan(
    VMSVGA3DD3D10ObjectKind kind,
    VMSVGA3DD3D10ObjectLifecyclePlan *plan)
{
  if (!plan || kind >= VMSVGA3D_D3D10_OBJECT_COUNT) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(plan, 0, sizeof(*plan));
  plan->kind = kind;
  plan->define_calls_backend = true;
  plan->destroy_calls_backend = true;
  plan->destroy_releases_native = true;
  plan->destroy_entry_reset_after_backend = true;

  switch (kind) {
  case VMSVGA3D_D3D10_OBJECT_SRV:
    plan->define_creates_native_immediately = true;
    plan->define_backend_expects_empty_slot = true;
    plan->define_may_create_surface_resource = true;
    plan->define_surface_kind_depends_on_surface_format = true;
    plan->destroy_entry_reset_before_backend = true;
    plan->destroy_entry_reset_after_backend = false;
    plan->destroy_result_propagates = true;
    break;
  case VMSVGA3D_D3D10_OBJECT_RTV:
  case VMSVGA3D_D3D10_OBJECT_DSV:
    plan->define_creates_native_immediately = true;
    plan->define_backend_expects_empty_slot = true;
    plan->define_may_create_surface_resource = true;
    plan->define_surface_create_kind = VMSVGA3D_D3D10_CREATE_TEXTURE;
    plan->destroy_entry_reset_before_backend = true;
    plan->destroy_entry_reset_after_backend = false;
    plan->destroy_result_propagates = true;
    plan->destroy_clears_bound_references =
        kind == VMSVGA3D_D3D10_OBJECT_RTV;
    break;
  case VMSVGA3D_D3D10_OBJECT_ELEMENT_LAYOUT:
    plan->define_native_object_is_lazy = true;
    plan->backend_define_resets_existing_cached_object = true;
    break;
  case VMSVGA3D_D3D10_OBJECT_BLEND_STATE:
  case VMSVGA3D_D3D10_OBJECT_DEPTH_STENCIL_STATE:
  case VMSVGA3D_D3D10_OBJECT_RASTERIZER_STATE:
  case VMSVGA3D_D3D10_OBJECT_SAMPLER_STATE:
    plan->define_creates_native_immediately = true;
    plan->define_can_replace_existing_native_without_release = true;
    if (kind == VMSVGA3D_D3D10_OBJECT_RASTERIZER_STATE) {
      plan->destroy_result_propagates = true;
    }
    break;
  case VMSVGA3D_D3D10_OBJECT_SHADER:
    plan->define_native_object_is_lazy = true;
    plan->define_releases_existing_before_entry_update = true;
    break;
  case VMSVGA3D_D3D10_OBJECT_STREAM_OUTPUT:
    plan->define_native_object_is_lazy = true;
    plan->backend_define_resets_existing_cached_object = true;
    plan->destroy_result_propagates = true;
    break;
  case VMSVGA3D_D3D10_OBJECT_QUERY:
    plan->define_creates_native_immediately = true;
    plan->define_releases_existing_before_entry_update = true;
    break;
  case VMSVGA3D_D3D10_OBJECT_COUNT:
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }
  return VMSVGA3D_D3D10_LEVEL_10_0;
}

static VMSVGA3DD3D10Level cotable_info(
    SVGACOTableType type, uint32_t *entry_size,
    VMSVGA3DD3D10COTableReplayMode *replay_mode,
    bool *state_replay_overwrites_preserved_pointer)
{
  if (!entry_size || !replay_mode || !state_replay_overwrites_preserved_pointer) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  *state_replay_overwrites_preserved_pointer = false;
  switch (type) {
  case SVGA_COTABLE_RTVIEW:
    *entry_size = sizeof(SVGACOTableDXRTViewEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_VIEW;
    break;
  case SVGA_COTABLE_DSVIEW:
    *entry_size = sizeof(SVGACOTableDXDSViewEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_VIEW;
    break;
  case SVGA_COTABLE_SRVIEW:
    *entry_size = sizeof(SVGACOTableDXSRViewEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_VIEW;
    break;
  case SVGA_COTABLE_ELEMENTLAYOUT:
    *entry_size = sizeof(SVGACOTableDXElementLayoutEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_ELEMENT_LAYOUT;
    break;
  case SVGA_COTABLE_BLENDSTATE:
    *entry_size = sizeof(SVGACOTableDXBlendStateEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_STATE;
    *state_replay_overwrites_preserved_pointer = true;
    break;
  case SVGA_COTABLE_DEPTHSTENCIL:
    *entry_size = sizeof(SVGACOTableDXDepthStencilEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_STATE;
    *state_replay_overwrites_preserved_pointer = true;
    break;
  case SVGA_COTABLE_RASTERIZERSTATE:
    *entry_size = sizeof(SVGACOTableDXRasterizerStateEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_STATE;
    *state_replay_overwrites_preserved_pointer = true;
    break;
  case SVGA_COTABLE_SAMPLER:
    *entry_size = sizeof(SVGACOTableDXSamplerEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_STATE;
    *state_replay_overwrites_preserved_pointer = true;
    break;
  case SVGA_COTABLE_STREAMOUTPUT:
    *entry_size = sizeof(SVGACOTableDXStreamOutputEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_STREAM_OUTPUT;
    break;
  case SVGA_COTABLE_DXQUERY:
    *entry_size = VMSVGA3D_D3D10_QUERY_COTABLE_ENTRY_SIZE;
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_QUERY;
    break;
  case SVGA_COTABLE_DXSHADER:
    *entry_size = sizeof(SVGACOTableDXShaderEntry);
    *replay_mode = VMSVGA3D_D3D10_COTABLE_REPLAY_SHADER;
    break;
  default:
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }
  return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_cotable_plan(
    SVGACOTableType type, bool has_mob, uint32_t mob_size,
    uint32_t valid_size_in_bytes, bool grow, bool had_previous_mob,
    VMSVGA3DD3D10COTablePlan *plan)
{
  VMSVGA3DD3D10Level level;
  bool overwrite_preserved_pointer;

  if (!plan) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(plan, 0, sizeof(*plan));
  level = cotable_info(type, &plan->entry_size, &plan->replay_mode,
                       &overwrite_preserved_pointer);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }

  plan->type = type;
  if (has_mob) {
    if (valid_size_in_bytes > mob_size) {
      return VMSVGA3D_D3D10_LEVEL_INVALID;
    }
    plan->create_backing_store = true;
  } else {
    valid_size_in_bytes = 0;
    mob_size = 0;
    plan->unbind = true;
  }

  plan->capacity_entries = mob_size / plan->entry_size;
  plan->valid_entries = valid_size_in_bytes / plan->entry_size;
  plan->replace_frontend_table_before_backend = true;
  plan->grow_copies_valid_size_bytes = grow && had_previous_mob &&
                                       plan->valid_entries != 0;
  plan->grow_copy_bytes = plan->grow_copies_valid_size_bytes
                        ? valid_size_in_bytes : 0;
  plan->backend_reallocates_to_capacity = true;
  plan->backend_preserves_prefix_up_to_valid_count = true;
  plan->backend_zeroes_after_preserved_prefix = true;
  plan->backend_releases_truncated_entries = true;
  plan->skip_all_zero_entries_during_replay = true;
  plan->state_replay_can_replace_preserved_pointer_without_release =
      overwrite_preserved_pointer;
  return level;
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
  plan->bind_only_if_pipeline_differs = true;
  plan->bind_timing = VMSVGA3D_D3D10_BIND_DRAW_SETUP;

  if (sid == SVGA_ID_INVALID) {
    plan->unbind = true;
    return stage_level;
  }

  if (!surface_available || offset_in_bytes >= surface_bytes ||
      size_in_bytes > surface_bytes - offset_in_bytes) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  plan->create_buffer = true;
  plan->has_initial_data = has_surface_data;
  plan->initial_data_offset = offset_in_bytes;
  plan->replace_only_on_create_success = true;
  plan->preserve_old_buffer_on_create_failure = true;
  plan->create_failure_is_success = true;
  plan->bind_only_if_pipeline_differs = true;
  plan->create_desc.valid = true;
  plan->create_desc.resource_dimension = D3D10_RESOURCE_DIMENSION_BUFFER;
  plan->create_desc.byte_width = size_in_bytes;
  plan->create_desc.usage = D3D10_USAGE_DEFAULT;
  plan->create_desc.bind_flags = D3D10_BIND_CONSTANT_BUFFER;
  plan->create_desc.initial_subresource_count = has_surface_data ? 1u : 0u;
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
  plan->bind_full_table_at_draw = true;
  plan->bind_every_draw = true;
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
  plan->prepare_backend_on_set = true;
  plan->backend_updates_sequentially = true;
  plan->bind_from_slot_zero_at_draw = true;
  plan->bind_only_if_pipeline_differs = true;
  plan->bind_timing = VMSVGA3D_D3D10_BIND_DRAW_SETUP;

  for (i = 0; i < count; i++) {
    plan->bindings[i].sid = buffers[i].sid;
    plan->bindings[i].stride = buffers[i].stride;
    plan->bindings[i].offset = buffers[i].offset;
    plan->bindings[i].unbind = buffers[i].sid == SVGA_ID_INVALID;
    plan->bindings[i].ensure_buffer_on_set = !plan->bindings[i].unbind;
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
  VMSVGA3DD3D10Level level;

  if (!plan) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(plan, 0, sizeof(*plan));
  plan->sid = sid;
  plan->format = format;
  plan->offset = offset;
  plan->backend_offset = offset;
  plan->shadow_update = true;
  plan->bind_timing = VMSVGA3D_D3D10_BIND_DRAW_SETUP;

  if (sid == SVGA_ID_INVALID) {
    plan->unbind = true;
    plan->backend_offset = 0;
    return VMSVGA3D_D3D10_LEVEL_10_0;
  }

  plan->ensure_buffer_on_set = true;
  level = vmsvga3d_d3d10_index_format(format, &plan->dxgi_format,
                                      &plan->bytes_per_index);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    plan->backend_reject = true;
    return level;
  }
  return level;
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
      topology >= SVGA3D_PRIMITIVE_DX10_MAX) {
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

VMSVGA3DD3D10Level vmsvga3d_d3d10_pipeline_setup_plan(
    VMSVGA3DD3D10PipelineSetupPlan *plan)
{
  static const VMSVGA3DD3D10PipelineStep steps[] = {
    VMSVGA3D_D3D10_PIPELINE_UNBIND_OUTPUTS,
    VMSVGA3D_D3D10_PIPELINE_CONSTANT_BUFFERS,
    VMSVGA3D_D3D10_PIPELINE_VERTEX_BUFFERS,
    VMSVGA3D_D3D10_PIPELINE_INDEX_BUFFER,
    VMSVGA3D_D3D10_PIPELINE_SHADER_RESOURCES,
    VMSVGA3D_D3D10_PIPELINE_RENDER_TARGETS,
    VMSVGA3D_D3D10_PIPELINE_SHADERS,
    VMSVGA3D_D3D10_PIPELINE_INPUT_LAYOUT,
  };

  if (!plan) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(plan, 0, sizeof(*plan));
  memcpy(plan->steps, steps, sizeof(steps));
  plan->step_count = (uint32_t)(sizeof(steps) / sizeof(steps[0]));
  plan->ensure_all_shader_resource_views = true;
  plan->wait_for_shader_resource_surfaces = true;
  plan->bind_full_shader_resource_table_per_stage = true;
  plan->ensure_depth_stencil_view = true;
  plan->ensure_render_target_views = true;
  plan->create_shaders_lazily = true;
  plan->recreate_input_layout_from_vs_dxbc = true;
  /* dxSetupPipeline is void: asserts/early returns never abort the draw caller. */
  plan->failures_do_not_abort_draw = true;
  return VMSVGA3D_D3D10_LEVEL_10_0;
}

static VMSVGA3DD3D10Level draw_plan_init(
    SVGA3dPrimitiveType primitive, VMSVGA3DD3D10DrawKind kind,
    VMSVGA3DD3D10DrawPlan *plan)
{
  VMSVGA3DD3D10Level level;

  if (!plan) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(plan, 0, sizeof(*plan));
  level = vmsvga3d_d3d10_primitive_topology(primitive,
                                             &plan->native_topology);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }
  if (vmsvga3d_d3d10_pipeline_setup_plan(&plan->pipeline) ==
      VMSVGA3D_D3D10_LEVEL_INVALID) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  plan->requested_kind = kind;
  plan->native_kind = kind;
  plan->primitive = primitive;
  plan->track_render_targets_after_draw = true;
  return level;
}

static void triangle_fan_common(uint32_t count,
                                VMSVGA3DD3D10TriangleFanPlan *fan)
{
  fan->enabled = true;
  fan->reject_count_over_65535 = true;
  fan->temporary_index_buffer_is_immutable = true;
  fan->temporary_buffer_create_failure_assert_only = true;
  fan->generated_indices_are_u16 = true;
  fan->generated_index_count = 3u * (count - 2u);
  fan->generated_buffer_bytes = fan->generated_index_count * 2u;
  fan->generated_start_index = 0;
  fan->temporary_topology = D3D10_TOPOLOGY_TRIANGLELIST;
  fan->restored_topology = D3D10_TOPOLOGY_TRIANGLESTRIP;
  fan->backend_reject = count > 65535u;
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
  plan->remembered_count = previous_remembered_count > count
      ? previous_remembered_count : count;
  if (count != 0) {
    memcpy(plan->ids, ids, count * sizeof(*ids));
  }
  plan->shadow_update_count = count;
  plan->shadow_update_atomic = true;
  plan->preserve_unspecified_slots = true;
  plan->backend_set_is_noop = true;
  plan->ensure_dsv_at_draw = true;
  plan->ensure_all_rtv_slots_at_draw = true;
  plan->bind_at_draw_setup = true;
  return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_render_targets_pipeline_plan(
    SVGA3dDepthStencilViewId depth_stencil_view_id,
    const SVGA3dRenderTargetViewId ids[SVGA3D_MAX_RENDER_TARGETS],
    uint32_t remembered_count,
    VMSVGA3DD3D10RenderTargetsPipelinePlan *plan)
{
  uint32_t i;

  if (!plan || !ids || remembered_count > SVGA3D_MAX_RENDER_TARGETS) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(plan, 0, sizeof(*plan));
  plan->depth_stencil_view_id = depth_stencil_view_id;
  plan->remembered_count = remembered_count;
  for (i = 0; i < remembered_count; ++i) {
    plan->ids[i] = ids[i];
    if (ids[i] != SVGA3D_INVALID_ID) {
      ++plan->native_rtv_count;
    }
  }
  plan->native_count_is_number_of_valid_ids = true;
  plan->sparse_slot_bug_preserved = true;
  plan->use_render_targets_and_uavs_call = true;
  plan->vgpu10_uav_count_is_zero = true;
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
    SVGA3dSurfaceFormat source_format, bool source_resource_exists,
    bool destination_resource_exists, VMSVGA3DD3D10CopyResourcePlan *plan)
{
  VMSVGA3DD3D10ResourceCreateKind kind;

  if (!plan) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(plan, 0, sizeof(*plan));
  kind = copy_resource_create_kind(source_format);
  plan->ensure_source_resource = !source_resource_exists;
  plan->ensure_destination_resource = !destination_resource_exists;
  plan->source_create_kind = kind;
  plan->destination_create_kind = kind;
  plan->destination_create_kind_uses_source_format = true;
  plan->issue_copy_resource = true;
  plan->mark_destination_drawing_context = true;
  return VMSVGA3D_D3D10_LEVEL_10_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_copy_subresource_plan(
    SVGA3dSurfaceFormat source_format, bool source_resource_exists,
    bool destination_resource_exists, uint32_t destination_subresource,
    uint32_t source_subresource, const SVGA3dSize *source_size,
    const SVGA3dSize *destination_size, const SVGA3dCopyBox *box,
    VMSVGA3DD3D10CopySubresourcePlan *plan)
{
  VMSVGA3DD3D10ResourceCreateKind kind;
  VMSVGA3DD3D10Level level;

  if (!plan) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(plan, 0, sizeof(*plan));
  kind = copy_resource_create_kind(source_format);
  plan->ensure_source_resource = !source_resource_exists;
  plan->ensure_destination_resource = !destination_resource_exists;
  plan->source_create_kind = kind;
  plan->destination_create_kind = kind;
  plan->destination_create_kind_uses_source_format = true;
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

VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_plan(
    SVGA3dPrimitiveType primitive, uint32_t vertex_count,
    uint32_t start_vertex_location, VMSVGA3DD3D10DrawPlan *plan)
{
  VMSVGA3DD3D10Level level;

  level = draw_plan_init(primitive, VMSVGA3D_D3D10_DRAW, plan);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }

  plan->count0 = vertex_count;
  plan->start0 = start_vertex_location;
  if (primitive == SVGA3D_PRIMITIVE_TRIANGLEFAN) {
    triangle_fan_common(vertex_count, &plan->triangle_fan);
    plan->triangle_fan.save_restore_index_buffer = true;
    plan->triangle_fan.generated_base_vertex =
        (int32_t)start_vertex_location;
    plan->native_kind = VMSVGA3D_D3D10_DRAW_INDEXED;
    plan->backend_reject = plan->triangle_fan.backend_reject;
    plan->skip_render_target_tracking_on_backend_reject = true;
    plan->skip_render_target_tracking_on_emulation_failure = true;
  }
  return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_indexed_plan(
    SVGA3dPrimitiveType primitive, uint32_t index_count,
    uint32_t start_index_location, int32_t base_vertex_location,
    VMSVGA3DD3D10DrawPlan *plan)
{
  VMSVGA3DD3D10Level level;

  level = draw_plan_init(primitive, VMSVGA3D_D3D10_DRAW_INDEXED, plan);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }

  plan->count0 = index_count;
  plan->start0 = start_index_location;
  plan->base_vertex = base_vertex_location;
  if (primitive == SVGA3D_PRIMITIVE_TRIANGLEFAN) {
    triangle_fan_common(index_count, &plan->triangle_fan);
    plan->triangle_fan.indexed_source = true;
    plan->triangle_fan.helper_failure_ignored = true;
    plan->triangle_fan.save_restore_index_buffer = true;
    plan->triangle_fan.generated_base_vertex = base_vertex_location;
    plan->triangle_fan.source_read_offset = start_index_location;
    plan->triangle_fan.source_read_offset_is_raw_start_index = true;
    plan->triangle_fan.ignore_bound_index_buffer_offset = true;
    /* The caller ignores every error from dxDrawIndexedTriangleFan. */
    plan->backend_reject = false;
  }
  return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_instanced_plan(
    SVGA3dPrimitiveType primitive, uint32_t vertex_count_per_instance,
    uint32_t instance_count, uint32_t start_vertex_location,
    uint32_t start_instance_location, VMSVGA3DD3D10DrawPlan *plan)
{
  VMSVGA3DD3D10Level level;

  level = draw_plan_init(primitive, VMSVGA3D_D3D10_DRAW_INSTANCED, plan);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }

  plan->count0 = vertex_count_per_instance;
  plan->count1 = instance_count;
  plan->start0 = start_vertex_location;
  plan->start1 = start_instance_location;
  plan->triangle_fan_assert_only = primitive == SVGA3D_PRIMITIVE_TRIANGLEFAN;
  return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_indexed_instanced_plan(
    SVGA3dPrimitiveType primitive, uint32_t index_count_per_instance,
    uint32_t instance_count, uint32_t start_index_location,
    int32_t base_vertex_location, uint32_t start_instance_location,
    VMSVGA3DD3D10DrawPlan *plan)
{
  VMSVGA3DD3D10Level level;

  level = draw_plan_init(primitive, VMSVGA3D_D3D10_DRAW_INDEXED_INSTANCED,
                         plan);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }

  plan->count0 = index_count_per_instance;
  plan->count1 = instance_count;
  plan->start0 = start_index_location;
  plan->start1 = start_instance_location;
  plan->base_vertex = base_vertex_location;
  plan->triangle_fan_assert_only = primitive == SVGA3D_PRIMITIVE_TRIANGLEFAN;
  return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_draw_auto_plan(
    SVGA3dPrimitiveType primitive, VMSVGA3DD3D10DrawPlan *plan)
{
  VMSVGA3DD3D10Level level;

  level = draw_plan_init(primitive, VMSVGA3D_D3D10_DRAW_AUTO, plan);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }
  plan->triangle_fan_assert_only = primitive == SVGA3D_PRIMITIVE_TRIANGLEFAN;
  return level;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_indexed_triangle_fan_plan(
    uint32_t index_count, uint32_t start_index_location,
    int32_t base_vertex_location, uint32_t bound_dxgi_format,
    uint32_t bound_index_buffer_bytes, uint32_t bound_index_buffer_offset,
    VMSVGA3DD3D10TriangleFanPlan *plan)
{
  uint32_t bytes_per_index;

  if (!plan) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(plan, 0, sizeof(*plan));
  triangle_fan_common(index_count, plan);
  plan->indexed_source = true;
  plan->helper_failure_ignored = true;
  plan->save_restore_index_buffer = true;
  plan->generated_base_vertex = base_vertex_location;
  plan->source_read_offset = start_index_location;
  plan->source_read_offset_is_raw_start_index = true;
  plan->ignore_bound_index_buffer_offset = true;
  plan->bound_index_buffer_offset = bound_index_buffer_offset;
  plan->source_index_format = bound_dxgi_format;

  if (plan->backend_reject) {
    return VMSVGA3D_D3D10_LEVEL_10_0;
  }
  if (bound_dxgi_format == DXGI_R16_UINT) {
    bytes_per_index = 2;
  } else if (bound_dxgi_format == DXGI_R32_UINT) {
    bytes_per_index = 4;
    plan->truncate_u32_source_indices_to_u16 = true;
  } else {
    plan->backend_reject = true;
    return VMSVGA3D_D3D10_LEVEL_10_0;
  }

  plan->source_bytes_per_index = bytes_per_index;
  plan->source_read_bytes = bytes_per_index * index_count;
  if (start_index_location >= bound_index_buffer_bytes ||
      plan->source_read_bytes > bound_index_buffer_bytes - start_index_location ||
      plan->source_read_bytes < bytes_per_index) {
    plan->backend_reject = true;
  }
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

  if (!generated_count || count > 65535u) {
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
      src->type >= SVGA3D_SHADERTYPE_DX10_MAX) {
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
    binding->resource_dimension = VMSVGA3D_D3D10_SHADER_RESOURCE_BUFFER;
    return VMSVGA3D_D3D10_LEVEL_11_0;
  case SVGA3D_RESOURCE_BUFFER:
    binding->resource_dimension = VMSVGA3D_D3D10_SHADER_RESOURCE_BUFFER;
    break;
  case SVGA3D_RESOURCE_TEXTURE1D:
    binding->resource_dimension = array_elements <= 1 ?
        VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE1D :
        VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE1DARRAY;
    break;
  case SVGA3D_RESOURCE_TEXTURE2D:
    binding->resource_dimension = array_elements <= 1 ?
        VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2D :
        VMSVGA3D_D3D10_SHADER_RESOURCE_TEXTURE2DARRAY;
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

  if (info->program_type == VMSVGA3D_D3D10_SHADER_PROGRAM_PIXEL) {
    for (i = 0; i < binding_count; i++) {
      if (bindings[i].resource_dimension != VMSVGA3D_D3D10_SHADER_RESOURCE_UNKNOWN &&
          i < info->output_signature_count) {
        info->output_signature[i].componentType =
            vmsvga3d_d3d10_shader_component_type_from_format(bindings[i].format);
      }
    }
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

VMSVGA3DD3D10Level vmsvga3d_d3d10_shader_create_plan(
    SVGA3dShaderType type, uint32_t stream_output_id,
    VMSVGA3DD3D10ShaderCreatePlan *plan)
{
  VMSVGA3DD3D10Level level;

  if (plan == NULL) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }
  if (type < SVGA3D_SHADERTYPE_MIN ||
      type >= SVGA3D_SHADERTYPE_DX10_MAX) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }
  level = shader_type_level(type);

  memset(plan, 0, sizeof(*plan));
  plan->stage_index = (uint32_t)type - (uint32_t)SVGA3D_SHADERTYPE_MIN;
  plan->stream_output_id = SVGA3D_INVALID_ID;
  switch (type) {
  case SVGA3D_SHADERTYPE_VS:
    plan->create_kind = VMSVGA3D_D3D10_SHADER_CREATE_VERTEX;
    break;
  case SVGA3D_SHADERTYPE_PS:
    plan->create_kind = VMSVGA3D_D3D10_SHADER_CREATE_PIXEL;
    break;
  case SVGA3D_SHADERTYPE_GS:
    if (stream_output_id == SVGA3D_INVALID_ID) {
      plan->create_kind = VMSVGA3D_D3D10_SHADER_CREATE_GEOMETRY;
    } else {
      plan->create_kind = VMSVGA3D_D3D10_SHADER_CREATE_GEOMETRY_STREAM_OUTPUT;
      plan->stream_output_id = stream_output_id;
      plan->use_stream_output = true;
    }
    break;
  default:
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }
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
      src->numOutputStreamEntries >= SVGA3D_MAX_DX10_STREAMOUT_DECLS) {
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
    { D3D10_QUERY_TIMESTAMP_DISJOINT, sizeof(SVGADXTimestampDisjointQueryResult), 16, false, false },
    { D3D10_QUERY_PIPELINE_STATISTICS, sizeof(SVGADXPipelineStatisticsQueryResult), 88, false, false },
    { D3D10_QUERY_OCCLUSION_PREDICATE, sizeof(SVGADXOcclusionPredicateQueryResult), 4, true, false },
    { D3D10_QUERY_SO_STATISTICS, sizeof(SVGADXStreamOutStatisticsQueryResult), 16, false, false },
    { D3D10_QUERY_SO_OVERFLOW_PREDICATE, sizeof(SVGADXStreamOutPredicateQueryResult), 4, true, false },
    { D3D10_QUERY_OCCLUSION, sizeof(SVGADXOcclusion64QueryResult), 8, false, false },
  };

  if (!info || type >= SVGA3D_QUERYTYPE_MAX) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }
  if (type >= SVGA3D_QUERYTYPE_DX10_MAX) {
    static const VMSVGA3DD3D10QueryInfo later[] = {
      { D3D11_QUERY_SO_STATISTICS_STREAM0, sizeof(SVGADXStreamOutStatisticsQueryResult), 16, false, false },
      { D3D11_QUERY_SO_STATISTICS_STREAM1, sizeof(SVGADXStreamOutStatisticsQueryResult), 16, false, false },
      { D3D11_QUERY_SO_STATISTICS_STREAM2, sizeof(SVGADXStreamOutStatisticsQueryResult), 16, false, false },
      { D3D11_QUERY_SO_STATISTICS_STREAM3, sizeof(SVGADXStreamOutStatisticsQueryResult), 16, false, false },
      { D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0, sizeof(SVGADXStreamOutPredicateQueryResult), 4, true, false },
      { D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1, sizeof(SVGADXStreamOutPredicateQueryResult), 4, true, false },
      { D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2, sizeof(SVGADXStreamOutPredicateQueryResult), 4, true, false },
      { D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3, sizeof(SVGADXStreamOutPredicateQueryResult), 4, true, false },
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
  plan->get_data_after_end = true;
  plan->getdata_flags = 0;
  plan->wait_until_ready = true;
  /* Retry every non-S_OK return, not only S_FALSE. */
  plan->retry_non_s_ok = true;
  plan->yield_while_waiting = true;

  /* Queries are completed synchronously, so explicit readback is a NOP. */
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
  if (!enabled) {
    /* SVGA3D_INVALID_ID maps directly to SetPredication(NULL, FALSE). */
    return VMSVGA3D_D3D10_LEVEL_10_0;
  }

  level = vmsvga3d_d3d10_query_info(type, flags, &info);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }

  plan->enabled = true;
  plan->release_existing_query = true;
  plan->create_predicate = true;
  plan->d3d_query = info.d3d_query;
  plan->misc_flags = info.predicate_hint
                   ? VMSVGA3D_D3D10_QUERY_MISC_PREDICATEHINT : 0;
  plan->predicate_value = predicate_value != 0;

  /* Do not pre-check whether the query type is predicate-compatible. */
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
  (void)multisample_count;
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
    if (array_elements <= 1) {
      dst->view_dimension = D3D10_SRV_TEXTURE2D;
    } else {
      dst->view_dimension = D3D10_SRV_TEXTURE2DARRAY;
      dst->first_array_slice = src->desc.tex.firstArraySlice;
      dst->array_size = src->desc.tex.arraySize;
    }
    dst->most_detailed_mip = src->desc.tex.mostDetailedMip;
    dst->mip_levels = src->desc.tex.mipLevels;
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
  (void)multisample_count;
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
    if (array_elements <= 1) {
      dst->view_dimension = D3D10_RTV_TEXTURE2D;
    } else {
      dst->view_dimension = D3D10_RTV_TEXTURE2DARRAY;
      dst->first_array_slice = src->desc.tex.firstArraySlice;
      dst->array_size = src->desc.tex.arraySize;
    }
    dst->mip_slice = src->desc.tex.mipSlice;
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
  (void)multisample_count;
  memset(dst, 0, sizeof(*dst));
  format = vmsvga3d_d3d10_surface_format(src->format);
  if (format.min_level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }
  dst->format = format.dxgi_format;
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
    if (array_elements <= 1) {
      dst->view_dimension = D3D10_DSV_TEXTURE2D;
    } else {
      dst->view_dimension = D3D10_DSV_TEXTURE2DARRAY;
      dst->first_array_slice = src->firstArraySlice;
      dst->array_size = src->arraySize;
    }
    dst->mip_slice = src->mipSlice;
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
static bool vmsvga3d_d3d10_shader_bind_live(
    struct vmsvga_state_s *s, const SVGA3dCmdDXBindShader *command)
{
  VMSVGA3DMob *mob;
  SVGACOTableDXShaderEntry *entry;
  VMSVGA3DD3D10ShaderInfo info;
  VMSVGA3DD3D10Level level;
  void *blob;
  uint32_t mobid;

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

  /* A bad MOB id means unbind. A real MOB is validated after the COTable
   * binding fields are updated, matching the device's mutation ordering.
   */
  if (mob == NULL) {
    return true;
  }
  if (entry->sizeInBytes < 8 || command->offsetInBytes > mob->gbo.size ||
      entry->sizeInBytes > mob->gbo.size - command->offsetInBytes) {
    return false;
  }

  blob = malloc(entry->sizeInBytes);
  if (blob == NULL) {
    return false;
  }
  if (!vmsvga3d_mob_read(s, mob, command->offsetInBytes, blob,
                         entry->sizeInBytes)) {
    free(blob);
    return false;
  }

  memset(&info, 0, sizeof(info));
  level = vmsvga3d_d3d10_shader_parse(blob, entry->sizeInBytes, &info);
  vmsvga3d_d3d10_shader_release(&info);
  free(blob);
  return level != VMSVGA3D_D3D10_LEVEL_INVALID;
}

static bool vmsvga3d_d3d10_level_is_vgpu10(VMSVGA3DD3D10Level level)
{
  return level >= VMSVGA3D_D3D10_LEVEL_10_0 &&
         level <= VMSVGA3D_D3D10_LEVEL_10_1;
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
  array_elements = (surface->surface_flags & SVGA3D_SURFACE_CUBEMAP) != 0
                       ? 6u
                       : 1u;
  if (mip_levels == 0 || mip_levels > UINT32_MAX / array_elements) {
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
  if (s == NULL || s->svga3d == NULL || !vmsvga3d_dxvk_ready(s->dxvk)) {
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
  level = vmsvga3d_d3d10_srv_desc(
      entry, surface_info.array_elements, surface_info.multisample_count,
      &srv_desc);
  if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
      !vmsvga3d_d3d10_initial_subresources_live(
          surface, &resource_plan.primary, &initial_data,
          &initial_data_count)) {
    return false;
  }

  if (!vmsvga3d_dxvk_d3d11_surface_materialize(
          s->dxvk, surface->dxvk_surface, &resource_plan.primary,
          initial_data, initial_data_count)) {
    goto out;
  }
  success = vmsvga3d_dxvk_d3d11_shader_resource_view_ensure(
      s->dxvk, surface->dxvk_surface, &srv_desc);

out:
  g_free(initial_data);
  return success;
}

static bool vmsvga3d_d3d10_gen_mips_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXGenMips *command)
{
  VMSVGA3DDXContext *context;
  VMSVGA3DD3D10GenMipsPlan plan;
  SVGACOTableDXSRViewEntry *entry;
  VMSVGA3DSurface *surface;
  VMSVGA3DD3D10SurfaceInfo surface_info;
  VMSVGA3DD3D10SRVDesc srv_desc;
  VMSVGA3DD3D10Level level;

  if (s == NULL || command == NULL || s->svga3d == NULL ||
      !vmsvga3d_dxvk_ready(s->dxvk)) {
    return false;
  }
  context = vmsvga3d_dx_context(s, cid);
  if (context == NULL) {
    return false;
  }
  level = vmsvga3d_d3d10_gen_mips_plan(
      command->shaderResourceViewId,
      context->cotables[SVGA_COTABLE_SRVIEW].valid_entries, &plan);
  if (!vmsvga3d_d3d10_level_is_vgpu10(level)) {
    return false;
  }

  entry = vmsvga3d_dx_cotable_entry_ptr(
      s, cid, SVGA_COTABLE_SRVIEW, plan.view_id);
  if (entry == NULL || entry->sid == SVGA3D_INVALID_ID ||
      entry->sid >= SVGA3D_MAX_SURFACE_IDS) {
    return false;
  }
  surface = s->svga3d->surfaces[entry->sid];
  if (surface == NULL || surface->dxvk_surface == NULL ||
      !vmsvga3d_d3d10_surface_info_live(surface, &surface_info)) {
    return false;
  }
  level = vmsvga3d_d3d10_srv_desc(
      entry, surface_info.array_elements, surface_info.multisample_count,
      &srv_desc);
  if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
      !vmsvga3d_d3d10_srv_realize_live(s, cid, plan.view_id)) {
    return false;
  }
  return vmsvga3d_dxvk_d3d11_generate_mips(
      s->dxvk, surface->dxvk_surface, &srv_desc);
}

static bool vmsvga3d_d3d10_clear_rtv_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXClearRenderTargetView *command)
{
  VMSVGA3DDXContext *context;
  SVGACOTableDXRTViewEntry *entry;
  VMSVGA3DSurface *surface;
  VMSVGA3DD3D10ClearRTVPlan clear_plan;
  VMSVGA3DD3D10SurfaceInfo surface_info;
  VMSVGA3DD3D10ResourcePlan resource_plan;
  VMSVGA3DD3D10RTVDesc rtv_desc;
  VMSVGA3DDxvkSubresourceData *initial_data = NULL;
  VMSVGA3DD3D10ResourceUse resource_use;
  VMSVGA3DD3D10Level level;
  uint32_t initial_data_count = 0;
  bool success = false;

  if (s == NULL || command == NULL || s->svga3d == NULL ||
      !vmsvga3d_dxvk_ready(s->dxvk)) {
    return false;
  }
  context = vmsvga3d_dx_context(s, cid);
  if (context == NULL) {
    return false;
  }

  level = vmsvga3d_d3d10_clear_rtv_plan(
      command->renderTargetViewId, &command->rgba,
      context->cotables[SVGA_COTABLE_RTVIEW].valid_entries, &clear_plan);
  if (!vmsvga3d_d3d10_level_is_vgpu10(level)) {
    return false;
  }
  entry = vmsvga3d_dx_cotable_entry_ptr(
      s, cid, SVGA_COTABLE_RTVIEW, clear_plan.view_id);
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
  level = vmsvga3d_d3d10_rtv_desc(
      entry, surface_info.array_elements, surface_info.multisample_count,
      &rtv_desc);
  if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
      !vmsvga3d_d3d10_initial_subresources_live(
          surface, &resource_plan.primary, &initial_data,
          &initial_data_count)) {
    return false;
  }

  if (!vmsvga3d_dxvk_d3d11_surface_materialize(
          s->dxvk, surface->dxvk_surface, &resource_plan.primary,
          initial_data, initial_data_count)) {
    goto out;
  }
  success = vmsvga3d_dxvk_d3d11_clear_render_target_view(
      s->dxvk, surface->dxvk_surface, &rtv_desc, clear_plan.color);

out:
  g_free(initial_data);
  return success;
}

static bool vmsvga3d_d3d10_clear_dsv_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXClearDepthStencilView *command)
{
  VMSVGA3DDXContext *context;
  SVGACOTableDXDSViewEntry *entry;
  VMSVGA3DSurface *surface;
  VMSVGA3DD3D10ClearDSVPlan clear_plan;
  VMSVGA3DD3D10SurfaceInfo surface_info;
  VMSVGA3DD3D10ResourcePlan resource_plan;
  VMSVGA3DD3D10DSVDesc dsv_desc;
  VMSVGA3DDxvkSubresourceData *initial_data = NULL;
  VMSVGA3DD3D10Level level;
  uint32_t initial_data_count = 0;
  bool success = false;

  if (s == NULL || command == NULL || s->svga3d == NULL ||
      !vmsvga3d_dxvk_ready(s->dxvk)) {
    return false;
  }
  context = vmsvga3d_dx_context(s, cid);
  if (context == NULL) {
    return false;
  }

  level = vmsvga3d_d3d10_clear_dsv_plan(
      command->flags, command->depthStencilViewId, command->depth,
      command->stencil,
      context->cotables[SVGA_COTABLE_DSVIEW].valid_entries, &clear_plan);
  if (!vmsvga3d_d3d10_level_is_vgpu10(level)) {
    return false;
  }
  entry = vmsvga3d_dx_cotable_entry_ptr(
      s, cid, SVGA_COTABLE_DSVIEW, clear_plan.view_id);
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
  level = vmsvga3d_d3d10_dsv_desc(
      entry, surface_info.array_elements, surface_info.multisample_count,
      &dsv_desc);
  if (!vmsvga3d_d3d10_level_is_vgpu10(level) ||
      !vmsvga3d_d3d10_initial_subresources_live(
          surface, &resource_plan.primary, &initial_data,
          &initial_data_count)) {
    return false;
  }

  if (!vmsvga3d_dxvk_d3d11_surface_materialize(
          s->dxvk, surface->dxvk_surface, &resource_plan.primary,
          initial_data, initial_data_count)) {
    goto out;
  }
  success = vmsvga3d_dxvk_d3d11_clear_depth_stencil_view(
      s->dxvk, surface->dxvk_surface, &dsv_desc, clear_plan.d3d_clear_flags,
      clear_plan.depth, clear_plan.stencil);

out:
  g_free(initial_data);
  return success;
}

static bool vmsvga3d_d3d10_query_end_live(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGA3dCmdDXEndQuery *command)
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
  bool success = false;

  if (s == NULL || command == NULL || s->svga3d == NULL ||
      !vmsvga3d_dxvk_ready(s->dxvk)) {
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
          VMSVGA3D_D3D10_LEVEL_INVALID ||
      plan.d3d_result_size > sizeof(d3d_result)) {
    return false;
  }
  if (entry[3] == SVGADX_QDSTATE_FINISHED) {
    return true;
  }
  if (entry[3] != SVGADX_QDSTATE_ACTIVE &&
      entry[3] != SVGADX_QDSTATE_IDLE) {
    return false;
  }

  entry[3] = SVGADX_QDSTATE_PENDING;
  if (vmsvga3d_dxvk_d3d11_query_end(
          s->dxvk, cid, command->queryId, plan.issue_end, d3d_result,
          plan.d3d_result_size, plan.getdata_flags) &&
      vmsvga3d_d3d10_query_result(
          type, d3d_result, plan.d3d_result_size, &svga_result,
          &svga_result_size) != VMSVGA3D_D3D10_LEVEL_INVALID) {
    success = true;
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
  if (success) {
    entry[3] = SVGADX_QDSTATE_FINISHED;
  }
  return success;
}

static bool vmsvga3d_d3d10_command(struct vmsvga_state_s *s,
                                       uint32_t cid, uint32_t cmd,
                                       const void *payload, uint32_t size) {
  switch (cmd) {
  case SVGA_3D_CMD_DX_INVALIDATE_CONTEXT:
    if (size < sizeof(SVGA3dCmdDXInvalidateContext)) {
      return false;
    };
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

  case SVGA_3D_CMD_DX_INVALIDATE_SUBRESOURCE: {
    SVGA3dCmdDXInvalidateSubResource command;
    VMSVGA3DSurface *surface;

    if (size < sizeof(command)) {
      return false;
    }
    memcpy(&command, payload, sizeof(command));
    if (s == NULL || s->svga3d == NULL ||
        command.sid >= SVGA3D_MAX_SURFACE_IDS) {
      return false;
    }
    surface = s->svga3d->surfaces[command.sid];
    if (surface == NULL || surface->mips == NULL ||
        command.subResource >= surface->mip_count) {
      return false;
    }

    /* Invalidation is advisory: retaining the current contents is valid. */
    return true;
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
      /* The context shadow updates before backend surface/range checks. */
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
    if (vmsvga3d_d3d10_shader_resources_set_plan(
            command.startView, command.type, count,
            count != 0 ? ids : NULL,
            context->cotables[SVGA_COTABLE_SRVIEW].valid_entries,
            &plan) == VMSVGA3D_D3D10_LEVEL_INVALID ||
        !vmsvga3d_state_dx_apply_shader_resources(s, cid, &plan)) {
      return false;
    }
    for (uint32_t i = 0; i < count; i++) {
      if (!vmsvga3d_d3d10_srv_realize_live(s, cid, ids[i])) {
        return false;
      }
    }
    return true;
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
      /* Sampler validation and shadow writes are sequential. */
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
      /* The context shadow updates before backend format rejection. */
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

  case SVGA_3D_CMD_DX_DEFINE_SHADERRESOURCE_VIEW: {
    SVGA3dCmdDXDefineShaderResourceView command;
    SVGACOTableDXSRViewEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_SRVIEW, command.shaderResourceViewId);
    return entry != NULL &&
           vmsvga3d_d3d10_srv_define_entry(&command, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_SHADERRESOURCE_VIEW: {
    SVGA3dCmdDXDestroyShaderResourceView command;
    SVGACOTableDXSRViewEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_SRVIEW, command.shaderResourceViewId);
    return entry != NULL &&
           vmsvga3d_d3d10_srv_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DEFINE_RENDERTARGET_VIEW: {
    SVGA3dCmdDXDefineRenderTargetView command;
    SVGACOTableDXRTViewEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_RTVIEW, command.renderTargetViewId);
    return entry != NULL &&
           vmsvga3d_d3d10_rtv_define_entry(&command, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_RENDERTARGET_VIEW: {
    SVGA3dCmdDXDestroyRenderTargetView command;
    VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
    SVGACOTableDXRTViewEntry *entry;

    if (context == NULL || size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_RTVIEW, command.renderTargetViewId);
    return entry != NULL &&
           vmsvga3d_d3d10_rtv_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID &&
           vmsvga3d_d3d10_rtv_destroy_shadow_refs(
               command.renderTargetViewId,
               context->shadow.renderState.renderTargetViewIds) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW: {
    SVGA3dCmdDXDefineDepthStencilView command;
    SVGA3dCmdDXDefineDepthStencilView_v2 command_v2;
    SVGACOTableDXDSViewEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
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
    return entry != NULL &&
           vmsvga3d_d3d10_dsv_define_entry(&command_v2, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_VIEW: {
    SVGA3dCmdDXDestroyDepthStencilView command;
    SVGACOTableDXDSViewEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DSVIEW, command.depthStencilViewId);
    return entry != NULL &&
           vmsvga3d_d3d10_dsv_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DEFINE_ELEMENTLAYOUT: {
    SVGA3dCmdDXDefineElementLayout command;
    SVGACOTableDXElementLayoutEntry *entry;
    const uint32_t header_size = sizeof(command);
    uint32_t count;
    const SVGA3dInputElementDesc *descs;

    if (size < header_size) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    count = (size - header_size) / sizeof(SVGA3dInputElementDesc);
    descs = count != 0
                ? (const SVGA3dInputElementDesc *)((const uint8_t *)payload +
                                                   header_size)
                : NULL;
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_ELEMENTLAYOUT, command.elementLayoutId);
    return entry != NULL &&
           vmsvga3d_d3d10_element_layout_define_entry(
               command.elementLayoutId, count, descs, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_ELEMENTLAYOUT: {
    SVGA3dCmdDXDestroyElementLayout command;
    SVGACOTableDXElementLayoutEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_ELEMENTLAYOUT, command.elementLayoutId);
    return entry != NULL &&
           vmsvga3d_d3d10_element_layout_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
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

  case SVGA_3D_CMD_DX_DEFINE_QUERY: {
    SVGA3dCmdDXDefineQuery command;
    VMSVGA3DD3D10QueryExecutionPlan plan;
    VMSVGA3DD3D10Level level;
    void *entry;

    if (size < sizeof(command)) {
      return false;
    };
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
    };
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
    VMSVGA3DD3D10QueryExecutionPlan plan;
    uint8_t *entry;
    VMSVGA3DMob *mob;
    SVGA3dQueryType type;
    uint32_t flags;
    uint32_t mobid;
    uint32_t offset;
    uint32_t query_state = SVGA3D_QUERYSTATE_PENDING;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DXQUERY, command.queryId);
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
    if (entry[3] == SVGADX_QDSTATE_ACTIVE) {
      return true;
    }
    if (entry[3] != SVGADX_QDSTATE_IDLE &&
        entry[3] != SVGADX_QDSTATE_PENDING &&
        entry[3] != SVGADX_QDSTATE_FINISHED) {
      return false;
    }
    if (!vmsvga3d_dxvk_d3d11_query_begin(
            s->dxvk, cid, command.queryId, plan.issue_begin)) {
      return false;
    }
    entry[3] = SVGADX_QDSTATE_ACTIVE;
    mobid = query_read_u32(entry + 8);
    offset = query_read_u32(entry + 12);
    mob = vmsvga3d_mob_get(s, mobid);
    if (mob != NULL) {
      (void)vmsvga3d_mob_write(
          s, mob, offset, &query_state, sizeof(query_state));
    }
    return true;
  }

  case SVGA_3D_CMD_DX_END_QUERY: {
    SVGA3dCmdDXEndQuery command;

    if (size < sizeof(command)) {
      return false;
    };
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
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DXQUERY, command.queryId);
    if (entry == NULL) {
      return false;
    };
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
    };
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
    };
    memcpy(&command, payload, sizeof(command));
    /* Query completion is synchronous; explicit readback is a validated NOP. */
    return vmsvga3d_dx_cotable_entry_ptr(
               s, cid, SVGA_COTABLE_DXQUERY, command.queryId) != NULL;
  }

  case SVGA_3D_CMD_DX_DEFINE_SHADER: {
    SVGA3dCmdDXDefineShader command;
    SVGACOTableDXShaderEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DXSHADER, command.shaderId);
    return entry != NULL &&
           vmsvga3d_d3d10_shader_define_entry(&command, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_SHADER: {
    SVGA3dCmdDXDestroyShader command;
    SVGACOTableDXShaderEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_DXSHADER, command.shaderId);
    return entry != NULL &&
           vmsvga3d_d3d10_shader_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_BIND_SHADER: {
    SVGA3dCmdDXBindShader command;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    return vmsvga3d_d3d10_shader_bind_live(s, &command);
  }

  case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT: {
    SVGA3dCmdDXDefineStreamOutput command;
    SVGACOTableDXStreamOutputEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_STREAMOUTPUT, command.soid);
    return entry != NULL &&
           vmsvga3d_d3d10_stream_output_legacy_entry(&command, entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  case SVGA_3D_CMD_DX_DESTROY_STREAMOUTPUT: {
    SVGA3dCmdDXDestroyStreamOutput command;
    SVGACOTableDXStreamOutputEntry *entry;

    if (size < sizeof(command)) {
      return false;
    };
    memcpy(&command, payload, sizeof(command));
    entry = vmsvga3d_dx_cotable_entry_ptr(
        s, cid, SVGA_COTABLE_STREAMOUTPUT, command.soid);
    return entry != NULL &&
           vmsvga3d_d3d10_stream_output_destroy_entry(entry) !=
               VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  default:
    return false;
  }
}

#endif /* VMSVGA3D_D3D10_RUNTIME_INTEGRATION */
