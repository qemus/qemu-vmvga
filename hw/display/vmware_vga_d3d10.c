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

  if (src == NULL || dst == NULL || src->sizeInBytes < 8) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  level = shader_type_level(src->type);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }

  /* VBox updates these fields without clearing the COTable padding. */
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

  /* VBox deliberately leaves the padding words alone. */
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
      /* VBox's void helper asserts and returns, without failing shader bind. */
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
    /* VBox's format helper deliberately defaults unknown formats to float. */
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
        /* VBox asserts this condition but still applies the semantic index. */
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
  level = shader_type_level(type);
  if (level == VMSVGA3D_D3D10_LEVEL_INVALID) {
    return level;
  }

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
  case SVGA3D_SHADERTYPE_HS:
    plan->create_kind = VMSVGA3D_D3D10_SHADER_CREATE_HULL;
    break;
  case SVGA3D_SHADERTYPE_DS:
    plan->create_kind = VMSVGA3D_D3D10_SHADER_CREATE_DOMAIN;
    break;
  case SVGA3D_SHADERTYPE_CS:
    plan->create_kind = VMSVGA3D_D3D10_SHADER_CREATE_COMPUTE;
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

VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_mob_entry(
    const SVGA3dCmdDXDefineStreamOutputWithMob *src,
    SVGACOTableDXStreamOutputEntry *dst)
{
  if (src == NULL || dst == NULL ||
      src->numOutputStreamEntries >= SVGA3D_MAX_STREAMOUT_DECLS) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  memset(dst, 0, sizeof(*dst));
  dst->numOutputStreamEntries = src->numOutputStreamEntries;
  memcpy(dst->streamOutputStrideInBytes, src->streamOutputStrideInBytes,
         sizeof(dst->streamOutputStrideInBytes));
  dst->rasterizedStream = src->rasterizedStream;
  dst->numOutputStreamStrides = src->numOutputStreamStrides;
  dst->mobid = SVGA3D_INVALID_ID;
  dst->usesMob = 1;
  return VMSVGA3D_D3D10_LEVEL_11_0;
}

VMSVGA3DD3D10Level vmsvga3d_d3d10_stream_output_bind(
    SVGACOTableDXStreamOutputEntry *entry, uint32_t mobid,
    uint32_t offset_in_bytes, uint32_t size_in_bytes)
{
  uint64_t required_size;

  if (entry == NULL) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  required_size = (uint64_t)entry->numOutputStreamEntries *
                  sizeof(SVGA3dStreamOutputDeclarationEntry);
  if (size_in_bytes < required_size) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  /* VBox only asserts the previous usesMob value and still performs the bind. */
  entry->mobid = mobid;
  entry->offsetInBytes = offset_in_bytes;
  entry->usesMob = 1;
  return VMSVGA3D_D3D10_LEVEL_11_0;
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
      /* VBox asserts here but keeps the unresolved declaration for host create. */
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
   * VirtualBox stores the guest records unchanged and its D3D11 backend casts
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
