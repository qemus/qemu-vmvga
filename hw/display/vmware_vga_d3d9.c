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

#include "include/vmware_vga_d3d9.h"

#include <string.h>

/* D3D9 ABI values used by the protocol translator. */
enum {
  D3D9_FMT_UNKNOWN = 0,
  D3D9_FMT_A8R8G8B8 = 21,
  D3D9_FMT_X8R8G8B8 = 22,
  D3D9_FMT_R5G6B5 = 23,
  D3D9_FMT_X1R5G5B5 = 24,
  D3D9_FMT_A1R5G5B5 = 25,
  D3D9_FMT_A4R4G4B4 = 26,
  D3D9_FMT_A8 = 28,
  D3D9_FMT_A8B8G8R8 = 32,
  D3D9_FMT_G16R16 = 34,
  D3D9_FMT_A2R10G10B10 = 35,
  D3D9_FMT_A16B16G16R16 = 36,
  D3D9_FMT_L8 = 50,
  D3D9_FMT_A8L8 = 51,
  D3D9_FMT_A4L4 = 52,
  D3D9_FMT_V8U8 = 60,
  D3D9_FMT_L6V5U5 = 61,
  D3D9_FMT_X8L8V8U8 = 62,
  D3D9_FMT_Q8W8V8U8 = 63,
  D3D9_FMT_V16U16 = 64,
  D3D9_FMT_A2W10V10U10 = 67,
  D3D9_FMT_D32 = 71,
  D3D9_FMT_D15S1 = 73,
  D3D9_FMT_D24S8 = 75,
  D3D9_FMT_D24X8 = 77,
  D3D9_FMT_D16 = 80,
  D3D9_FMT_L16 = 81,
  D3D9_FMT_D24FS8 = 83,
  D3D9_FMT_R16F = 111,
  D3D9_FMT_G16R16F = 112,
  D3D9_FMT_A16B16G16R16F = 113,
  D3D9_FMT_R32F = 114,
  D3D9_FMT_G32R32F = 115,
  D3D9_FMT_A32B32G32R32F = 116,
  D3D9_FMT_CXV8U8 = 117,
};

enum {
  D3D9_RTYPE_SURFACE = 1,
  D3D9_RTYPE_TEXTURE = 3,
  D3D9_RTYPE_VOLUME_TEXTURE = 4,
  D3D9_RTYPE_CUBE_TEXTURE = 5,
  D3D9_RTYPE_VERTEX_BUFFER = 6,
  D3D9_RTYPE_INDEX_BUFFER = 7,
};

enum {
  D3D9_USAGE_RENDERTARGET = 0x00000001u,
  D3D9_USAGE_DEPTHSTENCIL = 0x00000002u,
  D3D9_USAGE_WRITEONLY = 0x00000008u,
  D3D9_USAGE_DYNAMIC = 0x00000200u,
  D3D9_USAGE_AUTOGENMIPMAP = 0x00000400u,
};

enum {
  D3D9_POOL_DEFAULT = 0,
  D3D9_POOL_SYSTEMMEM = 2,
  D3D9_FMT_INDEX16 = 101,
  D3D9_FMT_INDEX32 = 102,
};

enum {
  D3D9_RS_ZENABLE = 7,
  D3D9_RS_FILLMODE = 8,
  D3D9_RS_SHADEMODE = 9,
  D3D9_RS_ZWRITEENABLE = 14,
  D3D9_RS_ALPHATESTENABLE = 15,
  D3D9_RS_LASTPIXEL = 16,
  D3D9_RS_SRCBLEND = 19,
  D3D9_RS_DESTBLEND = 20,
  D3D9_RS_CULLMODE = 22,
  D3D9_RS_ZFUNC = 23,
  D3D9_RS_ALPHAREF = 24,
  D3D9_RS_ALPHAFUNC = 25,
  D3D9_RS_DITHERENABLE = 26,
  D3D9_RS_ALPHABLENDENABLE = 27,
  D3D9_RS_FOGENABLE = 28,
  D3D9_RS_SPECULARENABLE = 29,
  D3D9_RS_FOGCOLOR = 34,
  D3D9_RS_FOGTABLEMODE = 35,
  D3D9_RS_FOGSTART = 36,
  D3D9_RS_FOGEND = 37,
  D3D9_RS_FOGDENSITY = 38,
  D3D9_RS_RANGEFOGENABLE = 48,
  D3D9_RS_STENCILENABLE = 52,
  D3D9_RS_STENCILFAIL = 53,
  D3D9_RS_STENCILZFAIL = 54,
  D3D9_RS_STENCILPASS = 55,
  D3D9_RS_STENCILFUNC = 56,
  D3D9_RS_STENCILREF = 57,
  D3D9_RS_STENCILMASK = 58,
  D3D9_RS_STENCILWRITEMASK = 59,
  D3D9_RS_TEXTUREFACTOR = 60,
  D3D9_RS_WRAP0 = 128,
  D3D9_RS_CLIPPING = 136,
  D3D9_RS_LIGHTING = 137,
  D3D9_RS_AMBIENT = 139,
  D3D9_RS_FOGVERTEXMODE = 140,
  D3D9_RS_LOCALVIEWER = 142,
  D3D9_RS_NORMALIZENORMALS = 143,
  D3D9_RS_DIFFUSEMATERIALSOURCE = 145,
  D3D9_RS_SPECULARMATERIALSOURCE = 146,
  D3D9_RS_AMBIENTMATERIALSOURCE = 147,
  D3D9_RS_EMISSIVEMATERIALSOURCE = 148,
  D3D9_RS_VERTEXBLEND = 151,
  D3D9_RS_CLIPPLANEENABLE = 152,
  D3D9_RS_POINTSIZE = 154,
  D3D9_RS_POINTSIZE_MIN = 155,
  D3D9_RS_POINTSPRITEENABLE = 156,
  D3D9_RS_POINTSCALEENABLE = 157,
  D3D9_RS_POINTSCALE_A = 158,
  D3D9_RS_POINTSCALE_B = 159,
  D3D9_RS_POINTSCALE_C = 160,
  D3D9_RS_MULTISAMPLEANTIALIAS = 161,
  D3D9_RS_MULTISAMPLEMASK = 162,
  D3D9_RS_POINTSIZE_MAX = 166,
  D3D9_RS_INDEXEDVERTEXBLENDENABLE = 167,
  D3D9_RS_COLORWRITEENABLE = 168,
  D3D9_RS_TWEENFACTOR = 170,
  D3D9_RS_BLENDOP = 171,
  D3D9_RS_SCISSORTESTENABLE = 174,
  D3D9_RS_SLOPESCALEDEPTHBIAS = 175,
  D3D9_RS_ANTIALIASEDLINEENABLE = 176,
  D3D9_RS_TWOSIDEDSTENCILMODE = 185,
  D3D9_RS_CCW_STENCILFAIL = 186,
  D3D9_RS_CCW_STENCILZFAIL = 187,
  D3D9_RS_CCW_STENCILPASS = 188,
  D3D9_RS_CCW_STENCILFUNC = 189,
  D3D9_RS_COLORWRITEENABLE1 = 190,
  D3D9_RS_COLORWRITEENABLE2 = 191,
  D3D9_RS_COLORWRITEENABLE3 = 192,
  D3D9_RS_BLENDFACTOR = 193,
  D3D9_RS_DEPTHBIAS = 195,
  D3D9_RS_WRAP8 = 198,
  D3D9_RS_SEPARATEALPHABLENDENABLE = 206,
  D3D9_RS_SRCBLENDALPHA = 207,
  D3D9_RS_DESTBLENDALPHA = 208,
  D3D9_RS_BLENDOPALPHA = 209,
};

enum {
  D3D9_TSS_COLOROP = 1,
  D3D9_TSS_COLORARG1 = 2,
  D3D9_TSS_COLORARG2 = 3,
  D3D9_TSS_ALPHAOP = 4,
  D3D9_TSS_ALPHAARG1 = 5,
  D3D9_TSS_ALPHAARG2 = 6,
  D3D9_TSS_BUMPENVMAT00 = 7,
  D3D9_TSS_BUMPENVMAT01 = 8,
  D3D9_TSS_BUMPENVMAT10 = 9,
  D3D9_TSS_BUMPENVMAT11 = 10,
  D3D9_TSS_TEXCOORDINDEX = 11,
  D3D9_TSS_BUMPENVLSCALE = 22,
  D3D9_TSS_BUMPENVLOFFSET = 23,
  D3D9_TSS_TEXTURETRANSFORMFLAGS = 24,
  D3D9_TSS_COLORARG0 = 26,
  D3D9_TSS_ALPHAARG0 = 27,
};

enum {
  D3D9_SAMP_ADDRESSU = 1,
  D3D9_SAMP_ADDRESSV = 2,
  D3D9_SAMP_ADDRESSW = 3,
  D3D9_SAMP_BORDERCOLOR = 4,
  D3D9_SAMP_MAGFILTER = 5,
  D3D9_SAMP_MINFILTER = 6,
  D3D9_SAMP_MIPFILTER = 7,
  D3D9_SAMP_MIPMAPLODBIAS = 8,
  D3D9_SAMP_MAXMIPLEVEL = 9,
  D3D9_SAMP_MAXANISOTROPY = 10,
  D3D9_SAMP_SRGBTEXTURE = 11,
};

enum {
  D3D9_TOP_DISABLE = 1,
  D3D9_TOP_SELECTARG1 = 2,
  D3D9_TOP_SELECTARG2 = 3,
  D3D9_TOP_MODULATE = 4,
  D3D9_TOP_MODULATE2X = 5,
  D3D9_TOP_MODULATE4X = 6,
  D3D9_TOP_ADD = 7,
  D3D9_TOP_ADDSIGNED = 8,
  D3D9_TOP_ADDSIGNED2X = 9,
  D3D9_TOP_SUBTRACT = 10,
  D3D9_TOP_ADDSMOOTH = 11,
  D3D9_TOP_BLENDDIFFUSEALPHA = 12,
  D3D9_TOP_BLENDTEXTUREALPHA = 13,
  D3D9_TOP_BLENDFACTORALPHA = 14,
  D3D9_TOP_BLENDTEXTUREALPHAPM = 15,
  D3D9_TOP_BLENDCURRENTALPHA = 16,
  D3D9_TOP_PREMODULATE = 17,
  D3D9_TOP_MODULATEALPHA_ADDCOLOR = 18,
  D3D9_TOP_MODULATECOLOR_ADDALPHA = 19,
  D3D9_TOP_MODULATEINVALPHA_ADDCOLOR = 20,
  D3D9_TOP_MODULATEINVCOLOR_ADDALPHA = 21,
  D3D9_TOP_BUMPENVMAPLUMINANCE = 23,
  D3D9_TOP_DOTPRODUCT3 = 24,
  D3D9_TOP_MULTIPLYADD = 25,
  D3D9_TOP_LERP = 26,
};

enum {
  D3D9_TA_DIFFUSE = 0,
  D3D9_TA_CURRENT = 1,
  D3D9_TA_TEXTURE = 2,
  D3D9_TA_SPECULAR = 4,
  D3D9_TA_CONSTANT = 6,
};

enum {
  D3D9_TTFF_DISABLE = 0,
  D3D9_TTFF_COUNT1 = 1,
  D3D9_TTFF_COUNT2 = 2,
  D3D9_TTFF_COUNT3 = 3,
  D3D9_TTFF_COUNT4 = 4,
  D3D9_TTFF_PROJECTED = 256,
};

enum {
  D3D9_BLEND_ZERO = 1,
  D3D9_BLEND_ONE = 2,
  D3D9_BLEND_BLENDFACTOR = 14,
  D3D9_BLEND_INVBLENDFACTOR = 15,
  D3D9_CULL_NONE = 1,
  D3D9_CULL_CW = 2,
  D3D9_CULL_CCW = 3,
  D3D9_FOG_NONE = 0,
  D3D9_FOG_EXP = 1,
  D3D9_FOG_EXP2 = 2,
  D3D9_FOG_LINEAR = 3,
  D3D9_FILL_POINT = 1,
  D3D9_FILL_WIREFRAME = 2,
  D3D9_FILL_SOLID = 3,
  D3D9_LIGHT_POINT = 1,
  D3D9_LIGHT_SPOT = 2,
  D3D9_LIGHT_DIRECTIONAL = 3,
  D3D9_PT_POINTLIST = 1,
  D3D9_PT_LINELIST = 2,
  D3D9_PT_LINESTRIP = 3,
  D3D9_PT_TRIANGLELIST = 4,
  D3D9_PT_TRIANGLESTRIP = 5,
  D3D9_PT_TRIANGLEFAN = 6,
  D3D9_CLEAR_TARGET = 1,
  D3D9_CLEAR_ZBUFFER = 2,
  D3D9_CLEAR_STENCIL = 4,
};

#define D3D9_TS_VIEW 2u
#define D3D9_TS_PROJECTION 3u
#define D3D9_TS_TEXTURE0 16u
#define D3D9_TS_WORLD0 256u

static const uint32_t vmsvga3d_d3d9_format_map[SVGA3D_FORMAT_MAX] = {
    [SVGA3D_X8R8G8B8] = D3D9_FMT_X8R8G8B8,
    [SVGA3D_A8R8G8B8] = D3D9_FMT_A8R8G8B8,
    [SVGA3D_R5G6B5] = D3D9_FMT_R5G6B5,
    [SVGA3D_X1R5G5B5] = D3D9_FMT_X1R5G5B5,
    [SVGA3D_A1R5G5B5] = D3D9_FMT_A1R5G5B5,
    [SVGA3D_A4R4G4B4] = D3D9_FMT_A4R4G4B4,
    [SVGA3D_Z_D32] = D3D9_FMT_D32,
    [SVGA3D_Z_D16] = D3D9_FMT_D16,
    [SVGA3D_Z_D24S8] = D3D9_FMT_D24S8,
    [SVGA3D_Z_D15S1] = D3D9_FMT_D15S1,
    [SVGA3D_LUMINANCE8] = D3D9_FMT_L8,
    [SVGA3D_LUMINANCE4_ALPHA4] = D3D9_FMT_A4L4,
    [SVGA3D_LUMINANCE16] = D3D9_FMT_L16,
    [SVGA3D_LUMINANCE8_ALPHA8] = D3D9_FMT_A8L8,
    [SVGA3D_DXT1] = VMSVGA3D_D3D9_MAKE_FOURCC('D', 'X', 'T', '1'),
    [SVGA3D_DXT2] = VMSVGA3D_D3D9_MAKE_FOURCC('D', 'X', 'T', '2'),
    [SVGA3D_DXT3] = VMSVGA3D_D3D9_MAKE_FOURCC('D', 'X', 'T', '3'),
    [SVGA3D_DXT4] = VMSVGA3D_D3D9_MAKE_FOURCC('D', 'X', 'T', '4'),
    [SVGA3D_DXT5] = VMSVGA3D_D3D9_MAKE_FOURCC('D', 'X', 'T', '5'),
    [SVGA3D_BUMPU8V8] = D3D9_FMT_V8U8,
    [SVGA3D_BUMPL6V5U5] = D3D9_FMT_L6V5U5,
    [SVGA3D_BUMPX8L8V8U8] = D3D9_FMT_X8L8V8U8,
    [SVGA3D_ARGB_S10E5] = D3D9_FMT_A16B16G16R16F,
    [SVGA3D_ARGB_S23E8] = D3D9_FMT_A32B32G32R32F,
    [SVGA3D_A2R10G10B10] = D3D9_FMT_A2R10G10B10,
    [SVGA3D_V8U8] = D3D9_FMT_V8U8,
    [SVGA3D_Q8W8V8U8] = D3D9_FMT_Q8W8V8U8,
    [SVGA3D_CxV8U8] = D3D9_FMT_CXV8U8,
    [SVGA3D_X8L8V8U8] = D3D9_FMT_X8L8V8U8,
    [SVGA3D_A2W10V10U10] = D3D9_FMT_A2W10V10U10,
    [SVGA3D_ALPHA8] = D3D9_FMT_A8,
    [SVGA3D_R_S10E5] = D3D9_FMT_R16F,
    [SVGA3D_R_S23E8] = D3D9_FMT_R32F,
    [SVGA3D_RG_S10E5] = D3D9_FMT_G16R16F,
    [SVGA3D_RG_S23E8] = D3D9_FMT_G32R32F,
    [SVGA3D_Z_D24X8] = D3D9_FMT_D24X8,
    [SVGA3D_V16U16] = D3D9_FMT_V16U16,
    [SVGA3D_G16R16] = D3D9_FMT_G16R16,
    [SVGA3D_A16B16G16R16] = D3D9_FMT_A16B16G16R16,
    [SVGA3D_UYVY] = VMSVGA3D_D3D9_MAKE_FOURCC('U', 'Y', 'V', 'Y'),
    [SVGA3D_YUY2] = VMSVGA3D_D3D9_MAKE_FOURCC('Y', 'U', 'Y', '2'),
    [SVGA3D_NV12] = VMSVGA3D_D3D9_MAKE_FOURCC('N', 'V', '1', '2'),
    [SVGA3D_FORMAT_DEAD2] = VMSVGA3D_D3D9_MAKE_FOURCC('A', 'Y', 'U', 'V'),
    [SVGA3D_R8G8B8A8_UNORM] = D3D9_FMT_A8B8G8R8,
    [SVGA3D_Z_DF24] = D3D9_FMT_D24FS8,
    [SVGA3D_Z_D24S8_INT] = D3D9_FMT_D24S8,
    [SVGA3D_R8G8B8A8_SNORM] = D3D9_FMT_Q8W8V8U8,
    [SVGA3D_R16G16_UNORM] = D3D9_FMT_G16R16,
};

typedef struct vmsvga3d_d3d9_direct_render_state_s {
  uint16_t state;
  uint16_t d3d_state;
} VMSVGA3DD3D9DirectRenderState;

static const VMSVGA3DD3D9DirectRenderState vmsvga3d_d3d9_render_map[] = {
    {SVGA3D_RS_ZENABLE, D3D9_RS_ZENABLE},
    {SVGA3D_RS_ZWRITEENABLE, D3D9_RS_ZWRITEENABLE},
    {SVGA3D_RS_ALPHATESTENABLE, D3D9_RS_ALPHATESTENABLE},
    {SVGA3D_RS_DITHERENABLE, D3D9_RS_DITHERENABLE},
    {SVGA3D_RS_BLENDENABLE, D3D9_RS_ALPHABLENDENABLE},
    {SVGA3D_RS_FOGENABLE, D3D9_RS_FOGENABLE},
    {SVGA3D_RS_SPECULARENABLE, D3D9_RS_SPECULARENABLE},
    {SVGA3D_RS_STENCILENABLE, D3D9_RS_STENCILENABLE},
    {SVGA3D_RS_LIGHTINGENABLE, D3D9_RS_LIGHTING},
    {SVGA3D_RS_NORMALIZENORMALS, D3D9_RS_NORMALIZENORMALS},
    {SVGA3D_RS_POINTSPRITEENABLE, D3D9_RS_POINTSPRITEENABLE},
    {SVGA3D_RS_POINTSCALEENABLE, D3D9_RS_POINTSCALEENABLE},
    {SVGA3D_RS_STENCILREF, D3D9_RS_STENCILREF},
    {SVGA3D_RS_STENCILMASK, D3D9_RS_STENCILMASK},
    {SVGA3D_RS_STENCILWRITEMASK, D3D9_RS_STENCILWRITEMASK},
    {SVGA3D_RS_FOGSTART, D3D9_RS_FOGSTART},
    {SVGA3D_RS_FOGEND, D3D9_RS_FOGEND},
    {SVGA3D_RS_FOGDENSITY, D3D9_RS_FOGDENSITY},
    {SVGA3D_RS_POINTSIZE, D3D9_RS_POINTSIZE},
    {SVGA3D_RS_POINTSIZEMIN, D3D9_RS_POINTSIZE_MIN},
    {SVGA3D_RS_POINTSIZEMAX, D3D9_RS_POINTSIZE_MAX},
    {SVGA3D_RS_POINTSCALE_A, D3D9_RS_POINTSCALE_A},
    {SVGA3D_RS_POINTSCALE_B, D3D9_RS_POINTSCALE_B},
    {SVGA3D_RS_POINTSCALE_C, D3D9_RS_POINTSCALE_C},
    {SVGA3D_RS_FOGCOLOR, D3D9_RS_FOGCOLOR},
    {SVGA3D_RS_AMBIENT, D3D9_RS_AMBIENT},
    {SVGA3D_RS_CLIPPLANEENABLE, D3D9_RS_CLIPPLANEENABLE},
    {SVGA3D_RS_SHADEMODE, D3D9_RS_SHADEMODE},
    {SVGA3D_RS_BLENDEQUATION, D3D9_RS_BLENDOP},
    {SVGA3D_RS_ZFUNC, D3D9_RS_ZFUNC},
    {SVGA3D_RS_ALPHAFUNC, D3D9_RS_ALPHAFUNC},
    {SVGA3D_RS_STENCILFUNC, D3D9_RS_STENCILFUNC},
    {SVGA3D_RS_STENCILFAIL, D3D9_RS_STENCILFAIL},
    {SVGA3D_RS_STENCILZFAIL, D3D9_RS_STENCILZFAIL},
    {SVGA3D_RS_STENCILPASS, D3D9_RS_STENCILPASS},
    {SVGA3D_RS_RANGEFOGENABLE, D3D9_RS_RANGEFOGENABLE},
    {SVGA3D_RS_COLORWRITEENABLE, D3D9_RS_COLORWRITEENABLE},
    {SVGA3D_RS_VERTEXMATERIALENABLE, D3D9_RS_INDEXEDVERTEXBLENDENABLE},
    {SVGA3D_RS_DIFFUSEMATERIALSOURCE, D3D9_RS_DIFFUSEMATERIALSOURCE},
    {SVGA3D_RS_SPECULARMATERIALSOURCE, D3D9_RS_SPECULARMATERIALSOURCE},
    {SVGA3D_RS_AMBIENTMATERIALSOURCE, D3D9_RS_AMBIENTMATERIALSOURCE},
    {SVGA3D_RS_EMISSIVEMATERIALSOURCE, D3D9_RS_EMISSIVEMATERIALSOURCE},
    {SVGA3D_RS_TEXTUREFACTOR, D3D9_RS_TEXTUREFACTOR},
    {SVGA3D_RS_LOCALVIEWER, D3D9_RS_LOCALVIEWER},
    {SVGA3D_RS_SCISSORTESTENABLE, D3D9_RS_SCISSORTESTENABLE},
    {SVGA3D_RS_BLENDCOLOR, D3D9_RS_BLENDFACTOR},
    {SVGA3D_RS_STENCILENABLE2SIDED, D3D9_RS_TWOSIDEDSTENCILMODE},
    {SVGA3D_RS_CCWSTENCILFUNC, D3D9_RS_CCW_STENCILFUNC},
    {SVGA3D_RS_CCWSTENCILFAIL, D3D9_RS_CCW_STENCILFAIL},
    {SVGA3D_RS_CCWSTENCILZFAIL, D3D9_RS_CCW_STENCILZFAIL},
    {SVGA3D_RS_CCWSTENCILPASS, D3D9_RS_CCW_STENCILPASS},
    {SVGA3D_RS_VERTEXBLEND, D3D9_RS_VERTEXBLEND},
    {SVGA3D_RS_SLOPESCALEDEPTHBIAS, D3D9_RS_SLOPESCALEDEPTHBIAS},
    {SVGA3D_RS_DEPTHBIAS, D3D9_RS_DEPTHBIAS},
    {SVGA3D_RS_LASTPIXEL, D3D9_RS_LASTPIXEL},
    {SVGA3D_RS_CLIPPING, D3D9_RS_CLIPPING},
    {SVGA3D_RS_MULTISAMPLEANTIALIAS, D3D9_RS_MULTISAMPLEANTIALIAS},
    {SVGA3D_RS_MULTISAMPLEMASK, D3D9_RS_MULTISAMPLEMASK},
    {SVGA3D_RS_INDEXEDVERTEXBLENDENABLE, D3D9_RS_INDEXEDVERTEXBLENDENABLE},
    {SVGA3D_RS_TWEENFACTOR, D3D9_RS_TWEENFACTOR},
    {SVGA3D_RS_ANTIALIASEDLINEENABLE, D3D9_RS_ANTIALIASEDLINEENABLE},
    {SVGA3D_RS_COLORWRITEENABLE1, D3D9_RS_COLORWRITEENABLE1},
    {SVGA3D_RS_COLORWRITEENABLE2, D3D9_RS_COLORWRITEENABLE2},
    {SVGA3D_RS_COLORWRITEENABLE3, D3D9_RS_COLORWRITEENABLE3},
    {SVGA3D_RS_SEPARATEALPHABLENDENABLE, D3D9_RS_SEPARATEALPHABLENDENABLE},
    {SVGA3D_RS_BLENDEQUATIONALPHA, D3D9_RS_BLENDOPALPHA},
};

static bool vmsvga3d_d3d9_direct_render_state(uint32_t state,
                                               uint32_t *d3d_state) {
  uint32_t i;

  for (i = 0; i < sizeof(vmsvga3d_d3d9_render_map) /
                          sizeof(vmsvga3d_d3d9_render_map[0]);
       i++) {
    if (vmsvga3d_d3d9_render_map[i].state == state) {
      *d3d_state = vmsvga3d_d3d9_render_map[i].d3d_state;
      return true;
    }
  }
  if (state >= SVGA3D_RS_WRAP0 && state <= SVGA3D_RS_WRAP7) {
    *d3d_state = D3D9_RS_WRAP0 + state - SVGA3D_RS_WRAP0;
    return true;
  }
  if (state >= SVGA3D_RS_WRAP8 && state <= SVGA3D_RS_WRAP15) {
    *d3d_state = D3D9_RS_WRAP8 + state - SVGA3D_RS_WRAP8;
    return true;
  }
  return false;
}

static uint32_t vmsvga3d_d3d9_blend(uint32_t value, uint32_t fallback) {
  if (value >= SVGA3D_BLENDOP_ZERO && value <= SVGA3D_BLENDOP_SRCALPHASAT) {
    return value;
  }
  if (value == SVGA3D_BLENDOP_BLENDFACTOR) {
    return D3D9_BLEND_BLENDFACTOR;
  }
  if (value == SVGA3D_BLENDOP_INVBLENDFACTOR) {
    return D3D9_BLEND_INVBLENDFACTOR;
  }
  return fallback;
}

static bool vmsvga3d_d3d9_render_state_ignored(uint32_t state) {
  switch (state) {
  case SVGA3D_RS_LINEPATTERN:
  case SVGA3D_RS_FRONTWINDING:
  case SVGA3D_RS_COORDINATETYPE:
  case SVGA3D_RS_ZBIAS:
  case SVGA3D_RS_OUTPUTGAMMA:
  case SVGA3D_RS_ZVISIBLE:
  case SVGA3D_RS_TRANSPARENCYANTIALIAS:
  case SVGA3D_RS_LINEWIDTH:
    return true;
  default:
    return false;
  }
}

static uint32_t vmsvga3d_d3d9_texture_combiner(uint32_t value) {
  static const uint8_t map[SVGA3D_TC_MAX] = {
      [SVGA3D_TC_DISABLE] = D3D9_TOP_DISABLE,
      [SVGA3D_TC_SELECTARG1] = D3D9_TOP_SELECTARG1,
      [SVGA3D_TC_SELECTARG2] = D3D9_TOP_SELECTARG2,
      [SVGA3D_TC_MODULATE] = D3D9_TOP_MODULATE,
      [SVGA3D_TC_ADD] = D3D9_TOP_ADD,
      [SVGA3D_TC_ADDSIGNED] = D3D9_TOP_ADDSIGNED,
      [SVGA3D_TC_SUBTRACT] = D3D9_TOP_SUBTRACT,
      [SVGA3D_TC_BLENDTEXTUREALPHA] = D3D9_TOP_BLENDTEXTUREALPHA,
      [SVGA3D_TC_BLENDDIFFUSEALPHA] = D3D9_TOP_BLENDDIFFUSEALPHA,
      [SVGA3D_TC_BLENDCURRENTALPHA] = D3D9_TOP_BLENDCURRENTALPHA,
      [SVGA3D_TC_BLENDFACTORALPHA] = D3D9_TOP_BLENDFACTORALPHA,
      [SVGA3D_TC_MODULATE2X] = D3D9_TOP_MODULATE2X,
      [SVGA3D_TC_MODULATE4X] = D3D9_TOP_MODULATE4X,
      [SVGA3D_TC_DSDT] = D3D9_TOP_DISABLE,
      [SVGA3D_TC_DOTPRODUCT3] = D3D9_TOP_DOTPRODUCT3,
      [SVGA3D_TC_BLENDTEXTUREALPHAPM] = D3D9_TOP_BLENDTEXTUREALPHAPM,
      [SVGA3D_TC_ADDSIGNED2X] = D3D9_TOP_ADDSIGNED2X,
      [SVGA3D_TC_ADDSMOOTH] = D3D9_TOP_ADDSMOOTH,
      [SVGA3D_TC_PREMODULATE] = D3D9_TOP_PREMODULATE,
      [SVGA3D_TC_MODULATEALPHA_ADDCOLOR] = D3D9_TOP_MODULATEALPHA_ADDCOLOR,
      [SVGA3D_TC_MODULATECOLOR_ADDALPHA] = D3D9_TOP_MODULATECOLOR_ADDALPHA,
      [SVGA3D_TC_MODULATEINVALPHA_ADDCOLOR] =
          D3D9_TOP_MODULATEINVALPHA_ADDCOLOR,
      [SVGA3D_TC_MODULATEINVCOLOR_ADDALPHA] =
          D3D9_TOP_MODULATEINVCOLOR_ADDALPHA,
      [SVGA3D_TC_BUMPENVMAPLUMINANCE] = D3D9_TOP_BUMPENVMAPLUMINANCE,
      [SVGA3D_TC_MULTIPLYADD] = D3D9_TOP_MULTIPLYADD,
      [SVGA3D_TC_LERP] = D3D9_TOP_LERP,
  };

  if (value >= SVGA3D_TC_MAX || map[value] == 0) {
    return D3D9_TOP_DISABLE;
  }
  return map[value];
}

static uint32_t vmsvga3d_d3d9_texture_arg(uint32_t value) {
  switch (value) {
  case SVGA3D_TA_CONSTANT:
    return D3D9_TA_CONSTANT;
  case SVGA3D_TA_PREVIOUS:
    return D3D9_TA_CURRENT;
  case SVGA3D_TA_DIFFUSE:
    return D3D9_TA_DIFFUSE;
  case SVGA3D_TA_TEXTURE:
    return D3D9_TA_TEXTURE;
  case SVGA3D_TA_SPECULAR:
    return D3D9_TA_SPECULAR;
  default:
    return D3D9_TA_DIFFUSE;
  }
}

static uint32_t vmsvga3d_d3d9_texture_transform(uint32_t value) {
  switch (value) {
  case SVGA3D_TEX_TRANSFORM_OFF:
    return D3D9_TTFF_DISABLE;
  case SVGA3D_TEX_TRANSFORM_S:
    return D3D9_TTFF_COUNT1;
  case SVGA3D_TEX_TRANSFORM_T:
    return D3D9_TTFF_COUNT2;
  case SVGA3D_TEX_TRANSFORM_R:
    return D3D9_TTFF_COUNT3;
  case SVGA3D_TEX_TRANSFORM_Q:
    return D3D9_TTFF_COUNT4;
  case SVGA3D_TEX_PROJECTED:
    return D3D9_TTFF_PROJECTED;
  default:
    return D3D9_TTFF_DISABLE;
  }
}

uint32_t vmsvga3d_d3d9_surface_format(SVGA3dSurfaceFormat format) {
  if ((uint32_t)format >= SVGA3D_FORMAT_MAX) {
    return D3D9_FMT_UNKNOWN;
  }
  return vmsvga3d_d3d9_format_map[format];
}

static bool vmsvga3d_d3d9_depth_format(uint32_t format) {
  return format == D3D9_FMT_D24S8 || format == D3D9_FMT_D24X8 ||
         format == D3D9_FMT_D32 || format == D3D9_FMT_D16;
}

SVGA3dSurface1Flags vmsvga3d_d3d9_normalize_surface_flags(
    SVGA3dSurface1Flags flags, SVGA3dSurfaceFormat format) {
  switch (format) {
  case SVGA3D_Z_D32:
  case SVGA3D_Z_D16:
  case SVGA3D_Z_D24S8:
  case SVGA3D_Z_D15S1:
  case SVGA3D_Z_D24X8:
  case SVGA3D_Z_DF16:
  case SVGA3D_Z_DF24:
  case SVGA3D_Z_D24S8_INT:
    flags |= (SVGA3dSurface1Flags)SVGA3D_SURFACE_HINT_DEPTHSTENCIL;
    break;
  case SVGA3D_DXT1:
  case SVGA3D_DXT2:
  case SVGA3D_DXT3:
  case SVGA3D_DXT4:
  case SVGA3D_DXT5:
  case SVGA3D_BUMPU8V8:
  case SVGA3D_BUMPL6V5U5:
  case SVGA3D_BUMPX8L8V8U8:
  case SVGA3D_V8U8:
  case SVGA3D_Q8W8V8U8:
  case SVGA3D_CxV8U8:
  case SVGA3D_X8L8V8U8:
  case SVGA3D_A2W10V10U10:
  case SVGA3D_V16U16:
  case SVGA3D_X8R8G8B8:
  case SVGA3D_A8R8G8B8:
  case SVGA3D_R5G6B5:
  case SVGA3D_X1R5G5B5:
  case SVGA3D_A1R5G5B5:
  case SVGA3D_A4R4G4B4:
    flags |= (SVGA3dSurface1Flags)SVGA3D_SURFACE_HINT_TEXTURE;
    break;
  default:
    break;
  }
  return flags;
}

uint32_t vmsvga3d_d3d9_surface_usage(SVGA3dSurface1Flags flags) {
  uint32_t usage = 0;

  if (flags & SVGA3D_SURFACE_HINT_DYNAMIC) {
    usage |= D3D9_USAGE_DYNAMIC;
  }
  if (flags & SVGA3D_SURFACE_HINT_RENDERTARGET) {
    usage |= D3D9_USAGE_RENDERTARGET;
  }
  if (flags & SVGA3D_SURFACE_HINT_DEPTHSTENCIL) {
    usage |= D3D9_USAGE_DEPTHSTENCIL;
  }
  if (flags & SVGA3D_SURFACE_HINT_WRITEONLY) {
    usage |= D3D9_USAGE_WRITEONLY;
  }
  if (flags & SVGA3D_SURFACE_AUTOGENMIPMAPS) {
    usage |= D3D9_USAGE_AUTOGENMIPMAP;
  }
  return usage;
}

uint32_t vmsvga3d_d3d9_actual_format(
    uint32_t requested_format, const VMSVGA3DD3D9ResourceCaps *caps) {
  if (caps == NULL) {
    return requested_format;
  }
  if (requested_format == VMSVGA3D_D3D9_MAKE_FOURCC('U', 'Y', 'V', 'Y') &&
      !caps->supports_uyvy) {
    return D3D9_FMT_A8R8G8B8;
  }
  if (requested_format == VMSVGA3D_D3D9_MAKE_FOURCC('Y', 'U', 'Y', '2') &&
      !caps->supports_yuy2) {
    return D3D9_FMT_A8R8G8B8;
  }
  if (requested_format == D3D9_FMT_A8B8G8R8 &&
      !caps->supports_a8b8g8r8) {
    return D3D9_FMT_A8R8G8B8;
  }
  return requested_format;
}

static void vmsvga3d_d3d9_create_desc(
    VMSVGA3DD3D9CreateDesc *desc, uint32_t resource_type,
    const VMSVGA3DD3D9SurfaceInfo *surface, uint32_t levels,
    uint32_t usage, uint32_t format, uint32_t pool) {
  memset(desc, 0, sizeof(*desc));
  desc->valid = true;
  desc->resource_type = resource_type;
  desc->width = surface->size.width;
  desc->height = surface->size.height;
  desc->depth = surface->size.depth;
  desc->levels = levels;
  desc->usage = usage;
  desc->format = format;
  desc->pool = pool;
}

static void vmsvga3d_d3d9_surface_desc(
    VMSVGA3DD3D9CreateDesc *desc, const VMSVGA3DD3D9SurfaceInfo *surface,
    uint32_t usage, uint32_t format, bool lockable) {
  vmsvga3d_d3d9_create_desc(desc, D3D9_RTYPE_SURFACE, surface, 1, usage,
                            format, D3D9_POOL_DEFAULT);
  desc->depth = 1;
  desc->multisample_type =
      vmsvga3d_d3d9_multisample_type(surface->multisample_count);
  desc->multisample_quality =
      desc->multisample_type != 0 && surface->multisample_quality_levels > 0
          ? surface->multisample_quality_levels - 1
          : 0;
  desc->lockable = lockable;
}

static void vmsvga3d_d3d9_texture_plan(
    const VMSVGA3DD3D9SurfaceInfo *surface,
    VMSVGA3DD3D9ResourcePlan *plan) {
  uint32_t fallback_usage =
      (plan->base_usage & ~D3D9_USAGE_RENDERTARGET) | D3D9_USAGE_DYNAMIC;

  if (plan->normalized_surface_flags & SVGA3D_SURFACE_CUBEMAP) {
    vmsvga3d_d3d9_create_desc(&plan->primary, D3D9_RTYPE_CUBE_TEXTURE,
                              surface, surface->mip_levels, plan->base_usage,
                              plan->actual_format, D3D9_POOL_DEFAULT);
    plan->primary.height = plan->primary.width;
    plan->primary.depth = 1;
    plan->primary.shared_handle = true;
    plan->bounce = plan->primary;
    plan->bounce.usage = fallback_usage;
    plan->bounce.pool = D3D9_POOL_SYSTEMMEM;
    plan->bounce.shared_handle = false;
    plan->has_bounce = true;
    plan->fallback = plan->primary;
    plan->fallback.usage = fallback_usage;
    plan->has_fallback = true;
  } else if (vmsvga3d_d3d9_depth_format(plan->actual_format)) {
    vmsvga3d_d3d9_create_desc(&plan->primary, D3D9_RTYPE_TEXTURE, surface,
                              1, D3D9_USAGE_DEPTHSTENCIL,
                              VMSVGA3D_D3D9_MAKE_FOURCC('I', 'N', 'T', 'Z'),
                              D3D9_POOL_DEFAULT);
    plan->primary.depth = 1;
    plan->primary.shared_handle = true;
    plan->stencil_as_texture = true;
    if (plan->actual_format == D3D9_FMT_D24S8 ||
        plan->actual_format == D3D9_FMT_D24X8) {
      plan->bounce = plan->primary;
      plan->bounce.usage = D3D9_USAGE_DYNAMIC;
      plan->bounce.pool = D3D9_POOL_SYSTEMMEM;
      plan->bounce.shared_handle = false;
      plan->has_bounce = true;
    }
  } else if (surface->size.depth > 1) {
    vmsvga3d_d3d9_create_desc(&plan->primary, D3D9_RTYPE_VOLUME_TEXTURE,
                              surface, surface->mip_levels, plan->base_usage,
                              plan->actual_format, D3D9_POOL_DEFAULT);
    plan->primary.shared_handle = true;
    plan->bounce = plan->primary;
    plan->bounce.usage = fallback_usage;
    plan->bounce.pool = D3D9_POOL_SYSTEMMEM;
    plan->bounce.shared_handle = false;
    plan->has_bounce = true;
    plan->fallback = plan->primary;
    plan->fallback.usage = fallback_usage;
    plan->has_fallback = true;
  } else {
    vmsvga3d_d3d9_create_desc(&plan->primary, D3D9_RTYPE_TEXTURE, surface,
                              surface->mip_levels,
                              plan->base_usage | D3D9_USAGE_RENDERTARGET,
                              plan->actual_format, D3D9_POOL_DEFAULT);
    plan->primary.depth = 1;
    plan->primary.shared_handle = true;
    plan->bounce = plan->primary;
    plan->bounce.usage = fallback_usage;
    plan->bounce.pool = D3D9_POOL_SYSTEMMEM;
    plan->bounce.shared_handle = false;
    plan->has_bounce = true;
    plan->fallback = plan->primary;
    plan->fallback.usage = fallback_usage;
    plan->has_fallback = true;
    if (plan->actual_format != plan->requested_format) {
      plan->emulated = plan->primary;
      plan->emulated.shared_handle = false;
      plan->has_emulated = true;
      plan->needs_format_conversion = true;
    }
  }
  if (surface->autogen_filter != SVGA3D_TEX_FILTER_NONE) {
    plan->set_autogen_filter = true;
    plan->autogen_filter = (uint32_t)surface->autogen_filter;
  }
}

bool vmsvga3d_d3d9_resource_plan(
    const VMSVGA3DD3D9SurfaceInfo *surface, VMSVGA3DD3D9ResourceUse use,
    const VMSVGA3DD3D9ResourceCaps *caps, VMSVGA3DD3D9ResourcePlan *plan) {
  if (surface == NULL || caps == NULL || plan == NULL) {
    return false;
  }

  memset(plan, 0, sizeof(*plan));
  plan->use = use;
  plan->normalized_surface_flags = vmsvga3d_d3d9_normalize_surface_flags(
      surface->surface_flags, surface->format);
  plan->requested_format = vmsvga3d_d3d9_surface_format(surface->format);
  plan->actual_format =
      vmsvga3d_d3d9_actual_format(plan->requested_format, caps);
  plan->base_usage =
      vmsvga3d_d3d9_surface_usage(plan->normalized_surface_flags);
  plan->post_surface_flags = plan->normalized_surface_flags;
  plan->post_usage = plan->base_usage;

  if (use == VMSVGA3D_D3D9_RESOURCE_USE_VERTEX_BUFFER ||
      use == VMSVGA3D_D3D9_RESOURCE_USE_INDEX_BUFFER) {
    uint32_t type = use == VMSVGA3D_D3D9_RESOURCE_USE_VERTEX_BUFFER
                        ? D3D9_RTYPE_VERTEX_BUFFER
                        : D3D9_RTYPE_INDEX_BUFFER;
    vmsvga3d_d3d9_create_desc(&plan->primary, type, surface, 1,
                              D3D9_USAGE_DYNAMIC | D3D9_USAGE_WRITEONLY,
                              D3D9_FMT_UNKNOWN, D3D9_POOL_DEFAULT);
    plan->primary.length = surface->surface_bytes;
    plan->primary.width = 0;
    plan->primary.height = 0;
    plan->primary.depth = 0;
    if (use == VMSVGA3D_D3D9_RESOURCE_USE_INDEX_BUFFER) {
      plan->primary.format = surface->index_width == sizeof(uint16_t)
                                 ? D3D9_FMT_INDEX16
                                 : D3D9_FMT_INDEX32;
      plan->post_surface_flags |=
          (SVGA3dSurface1Flags)SVGA3D_SURFACE_HINT_INDEXBUFFER;
    } else {
      plan->post_surface_flags |=
          (SVGA3dSurface1Flags)SVGA3D_SURFACE_HINT_VERTEXBUFFER;
    }
    return true;
  }

  if (plan->requested_format == D3D9_FMT_UNKNOWN) {
    return false;
  }

  if (use == VMSVGA3D_D3D9_RESOURCE_USE_TEXTURE) {
    vmsvga3d_d3d9_texture_plan(surface, plan);
    return true;
  }

  if (use == VMSVGA3D_D3D9_RESOURCE_USE_COLOR_TARGET) {
    plan->post_surface_flags |=
        (SVGA3dSurface1Flags)SVGA3D_SURFACE_HINT_RENDERTARGET;
    plan->post_usage |= D3D9_USAGE_RENDERTARGET;
    if (plan->normalized_surface_flags & SVGA3D_SURFACE_HINT_TEXTURE) {
      vmsvga3d_d3d9_texture_plan(surface, plan);
      plan->use = use;
    } else {
      vmsvga3d_d3d9_surface_desc(&plan->primary, surface,
                                 D3D9_USAGE_RENDERTARGET,
                                 plan->actual_format, true);
    }
    return true;
  }

  if (use == VMSVGA3D_D3D9_RESOURCE_USE_DEPTH_TARGET) {
    VMSVGA3DD3D9CreateDesc depth_surface;
    uint32_t ms = vmsvga3d_d3d9_multisample_type(surface->multisample_count);

    plan->post_surface_flags |=
        (SVGA3dSurface1Flags)SVGA3D_SURFACE_HINT_DEPTHSTENCIL;
    plan->post_usage |= D3D9_USAGE_DEPTHSTENCIL;
    vmsvga3d_d3d9_surface_desc(&depth_surface, surface,
                               D3D9_USAGE_DEPTHSTENCIL,
                               plan->actual_format, false);
    if (caps->supports_intz && ms == 0 &&
        vmsvga3d_d3d9_depth_format(plan->actual_format)) {
      vmsvga3d_d3d9_texture_plan(surface, plan);
      plan->use = use;
      plan->surface_fallback = depth_surface;
      plan->has_surface_fallback = true;
    } else {
      plan->primary = depth_surface;
    }
    return true;
  }

  return false;
}

uint32_t vmsvga3d_d3d9_multisample_type(uint32_t sample_count) {
  return sample_count <= 16 ? sample_count : 0;
}

bool vmsvga3d_d3d9_transform_type(SVGA3dTransformType type,
                                   uint32_t *d3d_transform) {
  if (d3d_transform == NULL) {
    return false;
  }
  switch (type) {
  case SVGA3D_TRANSFORM_VIEW:
    *d3d_transform = D3D9_TS_VIEW;
    return true;
  case SVGA3D_TRANSFORM_PROJECTION:
    *d3d_transform = D3D9_TS_PROJECTION;
    return true;
  case SVGA3D_TRANSFORM_WORLD:
  case SVGA3D_TRANSFORM_WORLD1:
  case SVGA3D_TRANSFORM_WORLD2:
  case SVGA3D_TRANSFORM_WORLD3:
    *d3d_transform = D3D9_TS_WORLD0 +
                     (type == SVGA3D_TRANSFORM_WORLD
                          ? 0
                          : type - SVGA3D_TRANSFORM_WORLD1 + 1);
    return true;
  case SVGA3D_TRANSFORM_TEXTURE0:
  case SVGA3D_TRANSFORM_TEXTURE1:
  case SVGA3D_TRANSFORM_TEXTURE2:
  case SVGA3D_TRANSFORM_TEXTURE3:
  case SVGA3D_TRANSFORM_TEXTURE4:
  case SVGA3D_TRANSFORM_TEXTURE5:
  case SVGA3D_TRANSFORM_TEXTURE6:
  case SVGA3D_TRANSFORM_TEXTURE7:
    *d3d_transform = D3D9_TS_TEXTURE0 + type - SVGA3D_TRANSFORM_TEXTURE0;
    return true;
  default:
    return false;
  }
}

void vmsvga3d_d3d9_apply_z_range(VMSVGA3DD3D9Viewport *viewport,
                                  const SVGA3dZRange *z_range) {
  float min_z;
  float max_z;

  if (viewport == NULL || z_range == NULL) {
    return;
  }
  min_z = z_range->min;
  max_z = z_range->max;
  if (min_z < 0.0f) {
    min_z = 0.0f;
  }
  if (max_z > 1.0f) {
    max_z = 1.0f;
  }
  viewport->min_z = min_z;
  viewport->max_z = max_z;
}

VMSVGA3DD3D9TranslateResult
vmsvga3d_d3d9_render_state(const SVGA3dRenderState *state,
                            VMSVGA3DD3D9RenderStatePlan *plan) {
  uint32_t d3d_state;

  if (state == NULL || plan == NULL || state->state < SVGA3D_RS_MIN ||
      state->state >= SVGA3D_RS_MAX) {
    return VMSVGA3D_D3D9_TRANSLATE_INVALID;
  }
  memset(plan, 0, sizeof(*plan));

  if (vmsvga3d_d3d9_render_state_ignored(state->state)) {
    return VMSVGA3D_D3D9_TRANSLATE_IGNORE;
  }

  if (vmsvga3d_d3d9_direct_render_state(state->state, &d3d_state)) {
    plan->count = 1;
    plan->ops[0].state = d3d_state;
    plan->ops[0].value = state->uintValue;
    return VMSVGA3D_D3D9_TRANSLATE_EMIT;
  }

  plan->count = 1;
  switch (state->state) {
  case SVGA3D_RS_SRCBLEND:
    plan->ops[0].state = D3D9_RS_SRCBLEND;
    plan->ops[0].value = vmsvga3d_d3d9_blend(state->uintValue, D3D9_BLEND_ONE);
    break;
  case SVGA3D_RS_DSTBLEND:
    plan->ops[0].state = D3D9_RS_DESTBLEND;
    plan->ops[0].value = vmsvga3d_d3d9_blend(state->uintValue, D3D9_BLEND_ZERO);
    break;
  case SVGA3D_RS_SRCBLENDALPHA:
    plan->ops[0].state = D3D9_RS_SRCBLENDALPHA;
    plan->ops[0].value = vmsvga3d_d3d9_blend(state->uintValue, D3D9_BLEND_ONE);
    break;
  case SVGA3D_RS_DSTBLENDALPHA:
    plan->ops[0].state = D3D9_RS_DESTBLENDALPHA;
    plan->ops[0].value = vmsvga3d_d3d9_blend(state->uintValue, D3D9_BLEND_ZERO);
    break;
  case SVGA3D_RS_ALPHAREF:
    plan->ops[0].state = D3D9_RS_ALPHAREF;
    plan->ops[0].value = (uint8_t)(state->floatValue * 255.0f);
    break;
  case SVGA3D_RS_FILLMODE: {
    SVGA3dFillMode fill;
    fill.uintValue = state->uintValue;
    plan->ops[0].state = D3D9_RS_FILLMODE;
    switch (fill.mode) {
    case SVGA3D_FILLMODE_POINT:
      plan->ops[0].value = D3D9_FILL_POINT;
      break;
    case SVGA3D_FILLMODE_LINE:
      plan->ops[0].value = D3D9_FILL_WIREFRAME;
      break;
    case SVGA3D_FILLMODE_FILL:
      plan->ops[0].value = D3D9_FILL_SOLID;
      break;
    default:
      return VMSVGA3D_D3D9_TRANSLATE_INVALID;
    }
    break;
  }
  case SVGA3D_RS_CULLMODE:
    plan->ops[0].state = D3D9_RS_CULLMODE;
    switch (state->uintValue) {
    case SVGA3D_FACE_NONE:
      plan->ops[0].value = D3D9_CULL_NONE;
      break;
    case SVGA3D_FACE_FRONT:
    case SVGA3D_FACE_FRONT_BACK:
      plan->ops[0].value = D3D9_CULL_CW;
      break;
    case SVGA3D_FACE_BACK:
      plan->ops[0].value = D3D9_CULL_CCW;
      break;
    default:
      return VMSVGA3D_D3D9_TRANSLATE_INVALID;
    }
    break;
  case SVGA3D_RS_FOGMODE: {
    SVGA3dFogMode fog;
    uint32_t fog_value;
    uint32_t fog_state;

    fog.uintValue = state->uintValue;
    switch (fog.function) {
    case SVGA3D_FOGFUNC_INVALID:
      fog_value = D3D9_FOG_NONE;
      break;
    case SVGA3D_FOGFUNC_EXP:
      fog_value = D3D9_FOG_EXP;
      break;
    case SVGA3D_FOGFUNC_EXP2:
      fog_value = D3D9_FOG_EXP2;
      break;
    case SVGA3D_FOGFUNC_LINEAR:
      fog_value = D3D9_FOG_LINEAR;
      break;
    default:
      return VMSVGA3D_D3D9_TRANSLATE_INVALID;
    }
    if (fog.type == SVGA3D_FOGTYPE_VERTEX) {
      fog_state = D3D9_RS_FOGVERTEXMODE;
    } else if (fog.type == SVGA3D_FOGTYPE_PIXEL) {
      fog_state = D3D9_RS_FOGTABLEMODE;
    } else {
      return VMSVGA3D_D3D9_TRANSLATE_INVALID;
    }

    plan->count = 0;
    if (fog.base == SVGA3D_FOGBASE_DEPTHBASED ||
        fog.base == SVGA3D_FOGBASE_RANGEBASED) {
      plan->ops[plan->count].state = D3D9_RS_RANGEFOGENABLE;
      plan->ops[plan->count].value =
          fog.base == SVGA3D_FOGBASE_RANGEBASED ? 1u : 0u;
      plan->count++;
    }
    plan->ops[plan->count].state = fog_state;
    plan->ops[plan->count].value = fog_value;
    plan->count++;
    break;
  }
  default:
    return VMSVGA3D_D3D9_TRANSLATE_INVALID;
  }
  return VMSVGA3D_D3D9_TRANSLATE_EMIT;
}

VMSVGA3DD3D9TranslateResult
vmsvga3d_d3d9_render_target(SVGA3dRenderTargetType type,
                             const SVGA3dSurfaceImageId *target,
                             VMSVGA3DD3D9RenderTargetPlan *plan) {
  if (target == NULL || plan == NULL || (uint32_t)type >= SVGA3D_RT_MAX) {
    return VMSVGA3D_D3D9_TRANSLATE_INVALID;
  }
  memset(plan, 0, sizeof(*plan));
  plan->unbind = target->sid == SVGA3D_INVALID_ID;

  if (type == SVGA3D_RT_DEPTH) {
    plan->action = VMSVGA3D_D3D9_RT_ACTION_DEPTH_STENCIL;
    return VMSVGA3D_D3D9_TRANSLATE_EMIT;
  }
  if (type == SVGA3D_RT_STENCIL) {
    if (plan->unbind) {
      return VMSVGA3D_D3D9_TRANSLATE_IGNORE;
    }
    plan->action = VMSVGA3D_D3D9_RT_ACTION_DEPTH_STENCIL;
    return VMSVGA3D_D3D9_TRANSLATE_EMIT;
  }
  if (type >= SVGA3D_RT_COLOR0 && type <= SVGA3D_RT_COLOR7) {
    plan->action = VMSVGA3D_D3D9_RT_ACTION_COLOR;
    plan->color_index = type - SVGA3D_RT_COLOR0;
    plan->restore_viewport_zrange_scissor = !plan->unbind;
    return VMSVGA3D_D3D9_TRANSLATE_EMIT;
  }
  return VMSVGA3D_D3D9_TRANSLATE_INVALID;
}

uint32_t vmsvga3d_d3d9_sampler_index(uint32_t stage) {
  if (stage < VMSVGA3D_D3D9_MAX_PIXEL_SAMPLERS) {
    return stage;
  }
  return VMSVGA3D_D3D9_DMAP_SAMPLER +
         stage - VMSVGA3D_D3D9_MAX_PIXEL_SAMPLERS;
}

VMSVGA3DD3D9TranslateResult
vmsvga3d_d3d9_texture_state(const SVGA3dTextureState *state,
                             VMSVGA3DD3D9TextureStatePlan *plan) {
  uint32_t value;

  if (state == NULL || plan == NULL || state->name < SVGA3D_TS_MIN ||
      state->name >= SVGA3D_TS_MAX) {
    return VMSVGA3D_D3D9_TRANSLATE_INVALID;
  }
  memset(plan, 0, sizeof(*plan));

  if (state->name == SVGA3D_TS_BIND_TEXTURE) {
    if (state->stage >= VMSVGA3D_D3D9_MAX_SAMPLERS) {
      return VMSVGA3D_D3D9_TRANSLATE_IGNORE;
    }
    plan->action = VMSVGA3D_D3D9_TEXTURE_ACTION_BIND;
    plan->stage = vmsvga3d_d3d9_sampler_index(state->stage);
    plan->value = state->value;
    return VMSVGA3D_D3D9_TRANSLATE_EMIT;
  }

  switch (state->name) {
  case SVGA3D_TS_COLOROP:
    plan->state = D3D9_TSS_COLOROP;
    value = vmsvga3d_d3d9_texture_combiner(state->value);
    break;
  case SVGA3D_TS_COLORARG0:
    plan->state = D3D9_TSS_COLORARG0;
    value = vmsvga3d_d3d9_texture_arg(state->value);
    break;
  case SVGA3D_TS_COLORARG1:
    plan->state = D3D9_TSS_COLORARG1;
    value = vmsvga3d_d3d9_texture_arg(state->value);
    break;
  case SVGA3D_TS_COLORARG2:
    plan->state = D3D9_TSS_COLORARG2;
    value = vmsvga3d_d3d9_texture_arg(state->value);
    break;
  case SVGA3D_TS_ALPHAOP:
    plan->state = D3D9_TSS_ALPHAOP;
    value = vmsvga3d_d3d9_texture_combiner(state->value);
    break;
  case SVGA3D_TS_ALPHAARG0:
    plan->state = D3D9_TSS_ALPHAARG0;
    value = vmsvga3d_d3d9_texture_arg(state->value);
    break;
  case SVGA3D_TS_ALPHAARG1:
    plan->state = D3D9_TSS_ALPHAARG1;
    value = vmsvga3d_d3d9_texture_arg(state->value);
    break;
  case SVGA3D_TS_ALPHAARG2:
    plan->state = D3D9_TSS_ALPHAARG2;
    value = vmsvga3d_d3d9_texture_arg(state->value);
    break;
  case SVGA3D_TS_BUMPENVMAT00:
    plan->state = D3D9_TSS_BUMPENVMAT00;
    value = state->value;
    break;
  case SVGA3D_TS_BUMPENVMAT01:
    plan->state = D3D9_TSS_BUMPENVMAT01;
    value = state->value;
    break;
  case SVGA3D_TS_BUMPENVMAT10:
    plan->state = D3D9_TSS_BUMPENVMAT10;
    value = state->value;
    break;
  case SVGA3D_TS_BUMPENVMAT11:
    plan->state = D3D9_TSS_BUMPENVMAT11;
    value = state->value;
    break;
  case SVGA3D_TS_TEXCOORDINDEX:
    plan->state = D3D9_TSS_TEXCOORDINDEX;
    value = state->value;
    break;
  case SVGA3D_TS_BUMPENVLSCALE:
    plan->state = D3D9_TSS_BUMPENVLSCALE;
    value = state->value;
    break;
  case SVGA3D_TS_BUMPENVLOFFSET:
    plan->state = D3D9_TSS_BUMPENVLOFFSET;
    value = state->value;
    break;
  case SVGA3D_TS_TEXTURETRANSFORMFLAGS:
    plan->state = D3D9_TSS_TEXTURETRANSFORMFLAGS;
    value = vmsvga3d_d3d9_texture_transform(state->value);
    break;
  case SVGA3D_TS_TEXCOORDGEN:
  case SVGA3D_TS_CONSTANT:
  case SVGA3D_TS_COLOR_KEY_ENABLE:
  case SVGA3D_TS_COLOR_KEY:
    return VMSVGA3D_D3D9_TRANSLATE_IGNORE;
  default:
    goto sampler_state;
  }

  if (state->stage >= VMSVGA3D_D3D9_MAX_TEXTURE_STAGES) {
    return VMSVGA3D_D3D9_TRANSLATE_IGNORE;
  }
  plan->action = VMSVGA3D_D3D9_TEXTURE_ACTION_STAGE_STATE;
  plan->stage = state->stage;
  plan->value = value;
  return VMSVGA3D_D3D9_TRANSLATE_EMIT;

sampler_state:
  if (state->stage >= VMSVGA3D_D3D9_MAX_SAMPLERS) {
    return VMSVGA3D_D3D9_TRANSLATE_IGNORE;
  }
  plan->action = VMSVGA3D_D3D9_TEXTURE_ACTION_SAMPLER_STATE;
  plan->stage = vmsvga3d_d3d9_sampler_index(state->stage);
  plan->value = state->value;
  switch (state->name) {
  case SVGA3D_TS_ADDRESSU:
    plan->state = D3D9_SAMP_ADDRESSU;
    break;
  case SVGA3D_TS_ADDRESSV:
    plan->state = D3D9_SAMP_ADDRESSV;
    break;
  case SVGA3D_TS_ADDRESSW:
    plan->state = D3D9_SAMP_ADDRESSW;
    break;
  case SVGA3D_TS_MIPFILTER:
    plan->state = D3D9_SAMP_MIPFILTER;
    break;
  case SVGA3D_TS_MAGFILTER:
    plan->state = D3D9_SAMP_MAGFILTER;
    break;
  case SVGA3D_TS_MINFILTER:
    plan->state = D3D9_SAMP_MINFILTER;
    break;
  case SVGA3D_TS_BORDERCOLOR:
    plan->state = D3D9_SAMP_BORDERCOLOR;
    break;
  case SVGA3D_TS_TEXTURE_LOD_BIAS:
    plan->state = D3D9_SAMP_MIPMAPLODBIAS;
    break;
  case SVGA3D_TS_TEXTURE_MIPMAP_LEVEL:
    plan->state = D3D9_SAMP_MAXMIPLEVEL;
    break;
  case SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL:
    plan->state = D3D9_SAMP_MAXANISOTROPY;
    break;
  case SVGA3D_TS_GAMMA:
    plan->state = D3D9_SAMP_SRGBTEXTURE;
    plan->value = state->floatValue == 1.0f ? 0u : 1u;
    break;
  default:
    memset(plan, 0, sizeof(*plan));
    return VMSVGA3D_D3D9_TRANSLATE_IGNORE;
  }
  return VMSVGA3D_D3D9_TRANSLATE_EMIT;
}

static void vmsvga3d_d3d9_color(const float input[4],
                                 VMSVGA3DD3D9Color *output) {
  output->r = input[0];
  output->g = input[1];
  output->b = input[2];
  output->a = input[3];
}

bool vmsvga3d_d3d9_material(SVGA3dFace face,
                             const SVGA3dMaterial *material,
                             VMSVGA3DD3D9Material *d3d_material) {
  if ((uint32_t)face >= SVGA3D_FACE_MAX || material == NULL ||
      d3d_material == NULL) {
    return false;
  }
  vmsvga3d_d3d9_color(material->diffuse, &d3d_material->diffuse);
  vmsvga3d_d3d9_color(material->ambient, &d3d_material->ambient);
  vmsvga3d_d3d9_color(material->specular, &d3d_material->specular);
  vmsvga3d_d3d9_color(material->emissive, &d3d_material->emissive);
  d3d_material->power = material->shininess;
  return true;
}

bool vmsvga3d_d3d9_light(const SVGA3dLightData *light,
                          VMSVGA3DD3D9Light *d3d_light) {
  if (light == NULL || d3d_light == NULL) {
    return false;
  }
  switch (light->type) {
  case SVGA3D_LIGHTTYPE_POINT:
    d3d_light->type = D3D9_LIGHT_POINT;
    break;
  case SVGA3D_LIGHTTYPE_SPOT1:
    d3d_light->type = D3D9_LIGHT_SPOT;
    break;
  case SVGA3D_LIGHTTYPE_DIRECTIONAL:
    d3d_light->type = D3D9_LIGHT_DIRECTIONAL;
    break;
  default:
    return false;
  }
  vmsvga3d_d3d9_color(light->diffuse, &d3d_light->diffuse);
  vmsvga3d_d3d9_color(light->specular, &d3d_light->specular);
  vmsvga3d_d3d9_color(light->ambient, &d3d_light->ambient);
  d3d_light->position.x = light->position[0];
  d3d_light->position.y = light->position[1];
  d3d_light->position.z = light->position[2];
  d3d_light->direction.x = light->direction[0];
  d3d_light->direction.y = light->direction[1];
  d3d_light->direction.z = light->direction[2];
  d3d_light->range = light->range;
  d3d_light->falloff = light->falloff;
  d3d_light->attenuation0 = light->attenuation0;
  d3d_light->attenuation1 = light->attenuation1;
  d3d_light->attenuation2 = light->attenuation2;
  d3d_light->theta = light->theta;
  d3d_light->phi = light->phi;
  return true;
}

void vmsvga3d_d3d9_apply_viewport(VMSVGA3DD3D9Viewport *viewport,
                                   const SVGA3dRect *rect) {
  if (viewport == NULL || rect == NULL) {
    return;
  }
  viewport->x = rect->x;
  viewport->y = rect->y;
  viewport->width = rect->w;
  viewport->height = rect->h;
}

void vmsvga3d_d3d9_rect(const SVGA3dRect *rect,
                         VMSVGA3DD3D9Rect *d3d_rect) {
  uint32_t right;
  uint32_t bottom;

  if (rect == NULL || d3d_rect == NULL) {
    return;
  }
  right = rect->x + rect->w;
  bottom = rect->y + rect->h;
  d3d_rect->left = (int32_t)rect->x;
  d3d_rect->top = (int32_t)rect->y;
  d3d_rect->right = (int32_t)right;
  d3d_rect->bottom = (int32_t)bottom;
}

uint32_t vmsvga3d_d3d9_clear_flags(SVGA3dClearFlag flags) {
  uint32_t d3d_flags = 0;

  if (flags & SVGA3D_CLEAR_COLOR) {
    d3d_flags |= D3D9_CLEAR_TARGET;
  }
  if (flags & SVGA3D_CLEAR_DEPTH) {
    d3d_flags |= D3D9_CLEAR_ZBUFFER;
  }
  if (flags & SVGA3D_CLEAR_STENCIL) {
    d3d_flags |= D3D9_CLEAR_STENCIL;
  }
  return d3d_flags;
}

bool vmsvga3d_d3d9_clear_plan(
    SVGA3dClearFlag flags, uint32_t color, float depth, uint32_t stencil,
    uint32_t rect_count, uint32_t target_width, uint32_t target_height,
    uint32_t active_render_target_mask, VMSVGA3DD3D9ClearPlan *plan) {
  if (plan == NULL || target_width == 0 || target_height == 0 ||
      target_width > INT32_MAX || target_height > INT32_MAX) {
    return false;
  }

  memset(plan, 0, sizeof(*plan));
  plan->execution = VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED;
  plan->cpu_fallback_allowed = true;
  plan->require_color0 = true;
  plan->flags = vmsvga3d_d3d9_clear_flags(flags);
  plan->color = color;
  plan->depth = depth;
  plan->stencil = stencil;
  plan->rect_count = rect_count;
  plan->convert_rectangles = rect_count != 0;
  plan->save_scissor = true;
  plan->override_scissor = true;
  plan->clear_scissor.left = 0;
  plan->clear_scissor.top = 0;
  plan->clear_scissor.right = (int32_t)target_width;
  plan->clear_scissor.bottom = (int32_t)target_height;
  plan->restore_scissor = true;
  plan->clear = true;
  plan->track_active_render_targets = true;
  plan->active_render_target_mask = active_render_target_mask;
  return true;
}

bool vmsvga3d_d3d9_vertex_element(const SVGA3dVertexArrayIdentity *identity,
                                   uint32_t stream, uint32_t offset,
                                   VMSVGA3DD3D9VertexElement *element) {
  if (identity == NULL || element == NULL) {
    return false;
  }
  /* D3DVERTEXELEMENT9 stores these fields as WORD/BYTE.  Preserve the ABI's
   * direct narrowing behavior when converting the wider SVGA fields. */
  element->stream = (uint16_t)stream;
  element->offset = (uint16_t)offset;
  element->type = (uint8_t)identity->type;
  element->method = (uint8_t)identity->method;
  element->usage = (uint8_t)identity->usage;
  element->usage_index = (uint8_t)identity->usageIndex;
  return true;
}

bool vmsvga3d_d3d9_vertex_layout(
    const SVGA3dVertexDecl *decls, uint32_t decl_count,
    const SVGA3dVertexDivisor *divisors, uint32_t divisor_count,
    VMSVGA3DD3D9VertexElement *elements, uint32_t element_capacity,
    VMSVGA3DD3D9VertexStream *streams, uint32_t stream_capacity,
    uint32_t *stream_count) {
  uint32_t current = 0;
  uint32_t stream_id = 0;

  if (decls == NULL || elements == NULL || streams == NULL ||
      stream_count == NULL || decl_count == 0 || element_capacity < decl_count + 1 ||
      (divisor_count != 0 && (divisors == NULL || divisor_count != decl_count))) {
    return false;
  }

  while (current < decl_count) {
    uint32_t sid = decls[current].array.surfaceId;
    uint32_t stride = decls[current].array.stride;
    uint32_t min_offset = UINT32_MAX;
    uint32_t max_offset = 0;
    uint32_t end;

    if (stream_id >= stream_capacity) {
      return false;
    }
    for (end = current; end < decl_count; end++) {
      uint32_t offset;
      uint32_t new_min;
      uint32_t new_max;

      if (decls[end].array.surfaceId != sid) {
        break;
      }
      offset = decls[end].array.offset;
      new_min = min_offset < offset ? min_offset : offset;
      new_max = max_offset > offset ? max_offset : offset;
      if (stride != 0 && new_max - new_min >= stride) {
        break;
      }
      min_offset = new_min;
      max_offset = new_max;
    }
    if (end == current) {
      return false;
    }

    streams[stream_id].surface_id = sid;
    streams[stream_id].source_offset = min_offset;
    streams[stream_id].stride = stride;
    streams[stream_id].frequency = divisor_count ? divisors[current].value : 1u;
    streams[stream_id].first_decl = current;
    streams[stream_id].decl_count = end - current;

    for (uint32_t i = current; i < end; i++) {
      if (!vmsvga3d_d3d9_vertex_element(&decls[i].identity, stream_id,
                                         decls[i].array.offset - min_offset,
                                         &elements[i])) {
        return false;
      }
    }
    current = end;
    stream_id++;
  }

  /* D3DDECL_END(): { 0xff, 0, D3DDECLTYPE_UNUSED(17), 0, 0, 0 }. */
  elements[decl_count].stream = VMSVGA3D_D3D9_DECL_END_STREAM;
  elements[decl_count].offset = 0;
  elements[decl_count].type = 17;
  elements[decl_count].method = 0;
  elements[decl_count].usage = 0;
  elements[decl_count].usage_index = 0;
  *stream_count = stream_id;
  return true;
}

uint32_t vmsvga3d_d3d9_index_format(uint32_t index_width) {
  return index_width == sizeof(uint16_t) ? 101u : 102u;
}

bool vmsvga3d_d3d9_indexed_draw(const SVGA3dVertexDecl *first_decl,
                                 const SVGA3dPrimitiveRange *range,
                                 uint32_t vertex_buffer_bytes,
                                 VMSVGA3DD3D9IndexedDraw *draw) {
  uint32_t stride;

  if (first_decl == NULL || range == NULL || draw == NULL ||
      range->indexWidth == 0) {
    return false;
  }
  stride = first_decl->array.stride;
  if (first_decl->rangeHint.last != 0) {
    draw->num_vertices = first_decl->rangeHint.last - first_decl->rangeHint.first;
  } else {
    if (stride == 0) {
      return false;
    }
    draw->num_vertices = vertex_buffer_bytes / stride -
                         first_decl->array.offset / stride -
                         first_decl->rangeHint.first - range->indexBias;
  }
  draw->base_vertex_index = (int32_t)range->indexBias;
  draw->min_vertex_index = 0;
  draw->start_index = range->indexArray.offset / range->indexWidth;
  draw->primitive_count = range->primitiveCount;
  return true;
}

uint32_t vmsvga3d_d3d9_texture_filter(SVGA3dTextureFilter filter) {
  return (uint32_t)filter;
}

bool vmsvga3d_d3d9_mipmap_plan(SVGA3dTextureFilter filter,
                                 VMSVGA3DD3D9MipmapPlan *plan) {
  if (plan == NULL || filter < SVGA3D_TEX_FILTER_MIN ||
      filter >= SVGA3D_TEX_FILTER_MAX) {
    return false;
  }

  memset(plan, 0, sizeof(*plan));
  plan->filter = vmsvga3d_d3d9_texture_filter(filter);
  plan->store_filter = true;
  plan->require_associated_context = true;
  plan->create_texture_if_missing = true;
  plan->allow_texture = true;
  plan->allow_cube_texture = true;
  plan->allow_volume_texture = true;
  plan->set_autogen_filter = true;
  plan->filter_debug_assert =
      filter == SVGA3D_TEX_FILTER_FLATCUBIC ||
      filter == SVGA3D_TEX_FILTER_GAUSSIANCUBIC;
  plan->filter_failure_is_fatal = false;
  plan->generate_after_filter_failure = true;
  plan->generate_sublevels = true;
  return true;
}

static bool vmsvga3d_d3d9_transfer_texture(
    const VMSVGA3DD3D9TransferSurface *surface) {
  return surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_TEXTURE ||
         surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_CUBE_TEXTURE;
}

static bool vmsvga3d_d3d9_transfer_volume(
    const VMSVGA3DD3D9TransferSurface *surface) {
  return surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_VOLUME_TEXTURE ||
         (surface->surface_flags & SVGA3D_SURFACE_VOLUME) != 0;
}

static bool vmsvga3d_d3d9_transfer_buffer(
    const VMSVGA3DD3D9TransferSurface *surface) {
  return surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_VERTEX_BUFFER ||
         surface->resource_type == VMSVGA3D_D3D9_HOST_RESOURCE_INDEX_BUFFER;
}

bool vmsvga3d_d3d9_surface_copy_plan(
    const VMSVGA3DD3D9TransferSurface *source,
    const VMSVGA3DD3D9TransferSurface *destination,
    VMSVGA3DD3D9SurfaceCopyPlan *plan) {
  bool source_gpu;
  bool destination_gpu;

  if (source == NULL || destination == NULL || plan == NULL) {
    return false;
  }
  memset(plan, 0, sizeof(*plan));
  plan->cpu_fallback_allowed = true;
  plan->reject_volume_texture = true;
  plan->require_identical_layout = true;
  plan->skip_identical_self_copy = true;
  plan->require_zero_z = true;

  if (vmsvga3d_d3d9_transfer_volume(source) ||
      vmsvga3d_d3d9_transfer_volume(destination)) {
    plan->execution = VMSVGA3D_D3D9_EXECUTION_CPU_ONLY;
    return true;
  }
  if (source->block_width != destination->block_width ||
      source->block_height != destination->block_height ||
      source->block_depth != destination->block_depth ||
      source->bytes_per_block != destination->bytes_per_block) {
    return false;
  }

  source_gpu = source->resident;
  destination_gpu = destination->resident;
  if (source_gpu && !destination_gpu &&
      (destination->surface_flags & SVGA3D_SURFACE_HINT_TEXTURE)) {
    plan->create_destination_texture = true;
    destination_gpu = true;
  }

  if (source_gpu && destination_gpu) {
    plan->execution = VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED;
    plan->flush_source = true;
    plan->flush_destination = true;
    plan->stretch_rect = true;
    plan->stretch_filter = 0; /* D3DTEXF_NONE */
    plan->track_source = true;
    plan->track_destination = true;
    if (source->has_bounce &&
        (source->usage & D3D9_USAGE_RENDERTARGET)) {
      plan->gpu_failure_fallback =
          VMSVGA3D_D3D9_COPY_FALLBACK_READBACK_UPDATE;
    } else if ((source->usage & D3D9_USAGE_RENDERTARGET) == 0 &&
               (destination->usage & D3D9_USAGE_RENDERTARGET) == 0) {
      plan->gpu_failure_fallback = VMSVGA3D_D3D9_COPY_FALLBACK_LOCK_BOTH;
    }
    return true;
  }

  plan->execution = VMSVGA3D_D3D9_EXECUTION_CPU_ONLY;
  if (source_gpu || destination_gpu) {
    plan->lock_single_gpu_surface = true;
    plan->lock_source_readonly = source_gpu;
    plan->flush_source = source_gpu;
    plan->flush_destination = destination_gpu;
  }
  plan->mark_cpu_destination_dirty = !destination_gpu;
  if (destination_gpu && vmsvga3d_d3d9_transfer_texture(destination) &&
      destination->has_bounce) {
    plan->update_destination_texture = true;
    plan->track_destination = true;
  }
  return true;
}

bool vmsvga3d_d3d9_stretch_blt_plan(
    const VMSVGA3DD3D9TransferSurface *source,
    const VMSVGA3DD3D9TransferSurface *destination,
    SVGA3dStretchBltMode mode, VMSVGA3DD3D9StretchBltPlan *plan) {
  if (source == NULL || destination == NULL || plan == NULL) {
    return false;
  }
  memset(plan, 0, sizeof(*plan));
  plan->execution = source->resident || destination->resident
                        ? VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED
                        : VMSVGA3D_D3D9_EXECUTION_CPU_ONLY;
  plan->cpu_fallback_allowed = true;
  plan->require_existing_context = true;
  plan->create_source_texture = !source->resident && destination->resident;
  plan->create_destination_texture = !destination->resident && source->resident;
  plan->reject_volume_texture = true;
  plan->flush_source = true;
  plan->flush_destination = true;
  plan->stretch_rect = true;
  plan->require_zero_z = true;
  plan->track_source = true;
  plan->track_destination = true;

  switch (mode) {
  case SVGA3D_STRETCH_BLT_POINT:
    plan->filter = 1; /* D3DTEXF_POINT */
    break;
  case SVGA3D_STRETCH_BLT_LINEAR:
    plan->filter = 2; /* D3DTEXF_LINEAR */
    break;
  default:
    plan->filter = 0; /* D3DTEXF_NONE */
    plan->filter_debug_assert = true;
    break;
  }

  if (vmsvga3d_d3d9_transfer_volume(source) ||
      vmsvga3d_d3d9_transfer_volume(destination)) {
    plan->execution = VMSVGA3D_D3D9_EXECUTION_CPU_ONLY;
  }
  return true;
}

bool vmsvga3d_d3d9_dma_plan(
    const VMSVGA3DD3D9TransferSurface *surface,
    SVGA3dTransferType transfer, bool first_box,
    VMSVGA3DD3D9DmaPlan *plan) {
  bool texture;

  if (surface == NULL || plan == NULL ||
      (transfer != SVGA3D_WRITE_HOST_VRAM &&
       transfer != SVGA3D_READ_HOST_VRAM)) {
    return false;
  }
  memset(plan, 0, sizeof(*plan));
  plan->cpu_fallback_allowed = true;
  plan->gmr_transfer = true;
  plan->reject_volume_texture = true;

  if (!surface->resident) {
    plan->execution = VMSVGA3D_D3D9_EXECUTION_CPU_ONLY;
    plan->path = VMSVGA3D_D3D9_DMA_PATH_CPU_SHADOW;
    if (transfer == SVGA3D_WRITE_HOST_VRAM) {
      plan->mark_mipmap_dirty = true;
      plan->mark_surface_dirty = true;
    }
    return true;
  }

  if (vmsvga3d_d3d9_transfer_buffer(surface)) {
    plan->execution = VMSVGA3D_D3D9_EXECUTION_CPU_ONLY;
    plan->path = VMSVGA3D_D3D9_DMA_PATH_BUFFER_SHADOW;
    if (transfer == SVGA3D_WRITE_HOST_VRAM) {
      plan->mark_mipmap_dirty = true;
      plan->mark_surface_dirty = true;
    }
    return true;
  }

  if (vmsvga3d_d3d9_transfer_volume(surface)) {
    plan->execution = VMSVGA3D_D3D9_EXECUTION_CPU_ONLY;
    plan->path = VMSVGA3D_D3D9_DMA_PATH_CPU_SHADOW;
    return true;
  }

  if (surface->resource_type != VMSVGA3D_D3D9_HOST_RESOURCE_SURFACE &&
      !vmsvga3d_d3d9_transfer_texture(surface)) {
    return false;
  }

  texture = vmsvga3d_d3d9_transfer_texture(surface);
  plan->execution = VMSVGA3D_D3D9_EXECUTION_GPU_PREFERRED;
  plan->path = VMSVGA3D_D3D9_DMA_PATH_GPU_SURFACE;
  plan->flush_surface = true;
  plan->use_bounce_surface = texture && surface->has_bounce;
  plan->lock_readonly = transfer == SVGA3D_READ_HOST_VRAM;
  plan->readback_render_target_first_box =
      transfer == SVGA3D_READ_HOST_VRAM && texture && surface->has_bounce &&
      first_box && (surface->surface_flags & SVGA3D_SURFACE_HINT_RENDERTARGET);
  plan->update_texture_after_write =
      transfer == SVGA3D_WRITE_HOST_VRAM && texture && surface->has_bounce;
  plan->track_after_update = plan->update_texture_after_write;
  return true;
}

bool vmsvga3d_d3d9_present_plan(
    const VMSVGA3DD3D9TransferSurface *surface, uint32_t rect_count,
    uint32_t screen_width, uint32_t screen_height,
    VMSVGA3DD3D9PresentPlan *plan) {
  if (surface == NULL || plan == NULL || screen_width == 0 ||
      screen_height == 0) {
    return false;
  }

  memset(plan, 0, sizeof(*plan));
  plan->cpu_fallback_allowed = true;
  plan->destination_screen = 0;
  plan->source_face = 0;
  plan->source_mipmap = 0;
  plan->input_rect_count = rect_count;
  plan->effective_rect_count = rect_count != 0 ? rect_count : 1;
  plan->synthesize_full_screen_rect = rect_count == 0;
  if (plan->synthesize_full_screen_rect) {
    plan->full_screen_rect.x = 0;
    plan->full_screen_rect.y = 0;
    plan->full_screen_rect.w = screen_width;
    plan->full_screen_rect.h = screen_height;
    plan->full_screen_rect.srcx = 0;
    plan->full_screen_rect.srcy = 0;
  }

  /* VBox's legacy D3D9 SurfaceBlitToScreen callback returns
   * VERR_NOT_IMPLEMENTED.  The generic path therefore presents through a
   * READ_HOST_VRAM SurfaceDMA to screen 0's framebuffer. */
  plan->legacy_d3d9_screen_blit_unimplemented = true;
  plan->use_surface_dma_readback = true;
  plan->transfer = SVGA3D_READ_HOST_VRAM;
  plan->one_dma_per_rect = true;
  plan->dma_first_box_each_rect = true;
  plan->update_screen_after_each_rect = true;

  if (!vmsvga3d_d3d9_dma_plan(surface, SVGA3D_READ_HOST_VRAM, true,
                               &plan->dma)) {
    return false;
  }
  plan->execution = plan->dma.execution;
  return true;
}

bool vmsvga3d_d3d9_present_dma_box(const SVGA3dCopyRect *rect,
                                     SVGA3dCopyBox *box) {
  if (rect == NULL || box == NULL) {
    return false;
  }
  memset(box, 0, sizeof(*box));
  /* SurfaceDMA names its copy-box source after the guest image even for a
   * READ_HOST_VRAM transfer.  PRESENT therefore places the host source in
   * x/y and the screen destination in srcx/srcy, exactly as VBox does. */
  box->x = rect->srcx;
  box->y = rect->srcy;
  box->z = 0;
  box->w = rect->w;
  box->h = rect->h;
  box->d = 1;
  box->srcx = rect->x;
  box->srcy = rect->y;
  box->srcz = 0;
  return true;
}

bool vmsvga3d_d3d9_query_plan(SVGA3dQueryType type,
                               VMSVGA3DD3D9QueryPlan *plan) {
  if (plan == NULL || type != SVGA3D_QUERYTYPE_OCCLUSION) {
    return false;
  }
  plan->query_type = 9;    /* D3DQUERYTYPE_OCCLUSION */
  plan->issue_begin = 2;   /* D3DISSUE_BEGIN */
  plan->issue_end = 1;     /* D3DISSUE_END */
  plan->getdata_flags = 1; /* D3DGETDATA_FLUSH */
  plan->result_size = sizeof(uint32_t);
  plan->wait_until_ready = true; /* Loop while GetData returns S_FALSE. */
  return true;
}

bool vmsvga3d_d3d9_primitive_type(SVGA3dPrimitiveType type,
                                   uint32_t primitive_count,
                                   uint32_t *d3d_primitive) {
  if (d3d_primitive == NULL) {
    return false;
  }
  switch (type) {
  case SVGA3D_PRIMITIVE_TRIANGLELIST:
    *d3d_primitive = D3D9_PT_TRIANGLELIST;
    break;
  case SVGA3D_PRIMITIVE_POINTLIST:
    *d3d_primitive = D3D9_PT_POINTLIST;
    break;
  case SVGA3D_PRIMITIVE_LINELIST:
    *d3d_primitive = D3D9_PT_LINELIST;
    break;
  case SVGA3D_PRIMITIVE_LINESTRIP:
    *d3d_primitive = D3D9_PT_LINESTRIP;
    break;
  case SVGA3D_PRIMITIVE_TRIANGLESTRIP:
    *d3d_primitive = D3D9_PT_TRIANGLESTRIP;
    break;
  case SVGA3D_PRIMITIVE_TRIANGLEFAN:
    *d3d_primitive = D3D9_PT_TRIANGLEFAN;
    break;
  default:
    return false;
  }
  if (primitive_count == 1 &&
      (*d3d_primitive == D3D9_PT_TRIANGLESTRIP ||
       *d3d_primitive == D3D9_PT_TRIANGLEFAN)) {
    *d3d_primitive = D3D9_PT_TRIANGLELIST;
  }
  return true;
}

VMSVGA3DD3D9ShaderStage vmsvga3d_d3d9_shader_stage(SVGA3dShaderType type) {
  switch (type) {
  case SVGA3D_SHADERTYPE_VS:
    return VMSVGA3D_D3D9_SHADER_STAGE_VERTEX;
  case SVGA3D_SHADERTYPE_PS:
    return VMSVGA3D_D3D9_SHADER_STAGE_PIXEL;
  default:
    return VMSVGA3D_D3D9_SHADER_STAGE_INVALID;
  }
}

VMSVGA3DD3D9ShaderConstTarget
vmsvga3d_d3d9_shader_const_target(SVGA3dShaderType type,
                                   SVGA3dShaderConstType ctype) {
  if (type == SVGA3D_SHADERTYPE_VS) {
    switch (ctype) {
    case SVGA3D_CONST_TYPE_FLOAT:
      return VMSVGA3D_D3D9_CONST_TARGET_VS_FLOAT;
    case SVGA3D_CONST_TYPE_INT:
      return VMSVGA3D_D3D9_CONST_TARGET_VS_INT;
    case SVGA3D_CONST_TYPE_BOOL:
      return VMSVGA3D_D3D9_CONST_TARGET_VS_BOOL;
    default:
      return VMSVGA3D_D3D9_CONST_TARGET_INVALID;
    }
  }
  if (type == SVGA3D_SHADERTYPE_PS) {
    switch (ctype) {
    case SVGA3D_CONST_TYPE_FLOAT:
      return VMSVGA3D_D3D9_CONST_TARGET_PS_FLOAT;
    case SVGA3D_CONST_TYPE_INT:
      return VMSVGA3D_D3D9_CONST_TARGET_PS_INT;
    case SVGA3D_CONST_TYPE_BOOL:
      return VMSVGA3D_D3D9_CONST_TARGET_PS_BOOL;
    default:
      return VMSVGA3D_D3D9_CONST_TARGET_INVALID;
    }
  }
  return VMSVGA3D_D3D9_CONST_TARGET_INVALID;
}
