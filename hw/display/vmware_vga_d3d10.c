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
  case SVGA3D_YUY2:                     return fmt(DXGI_YUY2, VMSVGA3D_D3D10_LEVEL_11_1);
  case SVGA3D_NV12:                     return fmt(DXGI_NV12, VMSVGA3D_D3D10_LEVEL_11_1);
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
  case SVGA3D_AYUV:                     return fmt(DXGI_AYUV, VMSVGA3D_D3D10_LEVEL_11_1);
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
  uint32_t usage;
  uint32_t bind = 0;
  uint32_t cpu = 0;
  uint32_t misc = 0;

  if (!policy) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }

  if (flags & (SVGA3D_SURFACE_STAGING_UPLOAD | SVGA3D_SURFACE_STAGING_DOWNLOAD)) {
    usage = D3D10_USAGE_STAGING;
  } else if (flags & SVGA3D_SURFACE_HINT_DYNAMIC) {
    usage = D3D10_USAGE_DYNAMIC;
  } else if (flags & (SVGA3D_SURFACE_HINT_STATIC | SVGA3D_SURFACE_HINT_INDIRECT_UPDATE)) {
    usage = D3D10_USAGE_DEFAULT;
  } else if (flags & (SVGA3D_SURFACE_HINT_INDEXBUFFER |
                      SVGA3D_SURFACE_HINT_VERTEXBUFFER |
                      SVGA3D_SURFACE_BIND_VERTEX_BUFFER |
                      SVGA3D_SURFACE_BIND_INDEX_BUFFER |
                      SVGA3D_SURFACE_BIND_CONSTANT_BUFFER)) {
    usage = D3D10_USAGE_DYNAMIC;
  } else {
    usage = D3D10_USAGE_DEFAULT;
  }

  if (flags & (SVGA3D_SURFACE_BIND_VERTEX_BUFFER | SVGA3D_SURFACE_HINT_VERTEXBUFFER)) {
    bind |= D3D10_BIND_VERTEX_BUFFER;
  }
  if (flags & (SVGA3D_SURFACE_BIND_INDEX_BUFFER | SVGA3D_SURFACE_HINT_INDEXBUFFER)) {
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
  if (flags & SVGA3D_SURFACE_SCREENTARGET) {
    bind |= D3D10_BIND_SHADER_RESOURCE;
  }

  if (usage == D3D10_USAGE_STAGING) {
    cpu = D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE;
  } else if (usage == D3D10_USAGE_DYNAMIC) {
    cpu = D3D10_CPU_ACCESS_WRITE;
  }

  if (flags & SVGA3D_SURFACE_CUBEMAP) {
    misc |= D3D10_RESOURCE_MISC_TEXTURECUBE;
  }
  if (texture_resource &&
      (flags & SVGA3D_SURFACE_AUTOGENMIPMAPS) &&
      (bind & (D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET)) ==
          (D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET)) {
    misc |= D3D10_RESOURCE_MISC_GENERATE_MIPS;
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
    { D3D10_QUERY_PIPELINE_STATISTICS, sizeof(SVGADXPipelineStatisticsQueryResult), 64, false, false },
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
    if (flags & SVGA3D_DXQUERY_FLAG_PREDICATEHINT) {
      return VMSVGA3D_D3D10_LEVEL_INVALID;
    }
    *info = later[index];
    return VMSVGA3D_D3D10_LEVEL_11_0;
  }
  if ((flags & SVGA3D_DXQUERY_FLAG_PREDICATEHINT) &&
      type != SVGA3D_QUERYTYPE_OCCLUSIONPREDICATE) {
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }
  *info = base[type];
  if (flags & SVGA3D_DXQUERY_FLAG_PREDICATEHINT) {
    info->predicate_hint = true;
  }
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
      dst->view_dimension = multisample_count > 1 ? D3D10_SRV_TEXTURE2DMS : D3D10_SRV_TEXTURE2D;
    } else {
      dst->view_dimension = multisample_count > 1 ? D3D10_SRV_TEXTURE2DMSARRAY : D3D10_SRV_TEXTURE2DARRAY;
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
      dst->view_dimension = multisample_count > 1 ? D3D10_RTV_TEXTURE2DMS : D3D10_RTV_TEXTURE2D;
    } else {
      dst->view_dimension = multisample_count > 1 ? D3D10_RTV_TEXTURE2DMSARRAY : D3D10_RTV_TEXTURE2DARRAY;
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
      dst->view_dimension = multisample_count > 1 ? D3D10_DSV_TEXTURE2DMS : D3D10_DSV_TEXTURE2D;
    } else {
      dst->view_dimension = multisample_count > 1 ? D3D10_DSV_TEXTURE2DMSARRAY : D3D10_DSV_TEXTURE2DARRAY;
      dst->first_array_slice = src->firstArraySlice;
      dst->array_size = src->arraySize;
    }
    dst->mip_slice = src->mipSlice;
    break;
  case SVGA3D_RESOURCE_TEXTURECUBE:
    dst->view_dimension = D3D10_DSV_TEXTURE2DARRAY;
    dst->mip_slice = src->mipSlice;
    dst->first_array_slice = src->firstArraySlice;
    dst->array_size = src->arraySize;
    break;
  default:
    return VMSVGA3D_D3D10_LEVEL_INVALID;
  }
  return level;
}
