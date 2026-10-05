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

#ifndef HW_DISPLAY_VMWARE_VGA_VIDEO_H
#define HW_DISPLAY_VMWARE_VGA_VIDEO_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "svga_types.h"
#include "svga3d_types.h"

/* Guest video storage stays in its original format.  D3D9 uses native YUV
 * where possible, with a BGRA fallback; NV12 is Y/UV and YV12 is Y/V/U. */
typedef struct VMSVGA3DVideoLayout {
    uint32_t planes;
    uint32_t pitch[3];
    uint32_t rows[3];
    uint32_t offset[3];
    uint32_t size;
} VMSVGA3DVideoLayout;

static inline bool vmsvga3d_video_yuv(SVGA3dSurfaceFormat format)
{
    return format == SVGA3D_UYVY || format == SVGA3D_YUY2 ||
           format == SVGA3D_NV12 || format == SVGA3D_YV12;
}

static inline bool vmsvga3d_video_planar(SVGA3dSurfaceFormat format)
{
    return format == SVGA3D_NV12 || format == SVGA3D_YV12;
}

static inline bool vmsvga3d_video_layout(
    SVGA3dSurfaceFormat format, uint32_t width, uint32_t height,
    uint32_t pitch, VMSVGA3DVideoLayout *layout)
{
    uint64_t bytes = (uint64_t)pitch * height;
    uint64_t row_bytes;

    if (layout == NULL || width == 0 || height == 0 || pitch == 0) {
        return false;
    }
    memset(layout, 0, sizeof(*layout));
    if (vmsvga3d_video_yuv(format)) {
        if ((width & 1u) != 0 ||
            (vmsvga3d_video_planar(format) &&
             ((height & 1u) != 0 || (pitch & 1u) != 0))) {
            return false;
        }
        row_bytes = vmsvga3d_video_planar(format) ? width :
                    (uint64_t)width * 2u;
    } else if (format == SVGA3D_R8G8B8A8_UNORM ||
               format == SVGA3D_A8R8G8B8) {
        row_bytes = (uint64_t)width * 4u;
    } else {
        return false;
    }
    if (pitch < row_bytes || bytes > UINT32_MAX) {
        return false;
    }
    layout->planes = 1;
    layout->pitch[0] = pitch;
    layout->rows[0] = height;
    if (vmsvga3d_video_planar(format)) {
        layout->planes = format == SVGA3D_NV12 ? 2 : 3;
        layout->offset[1] = (uint32_t)bytes;
        layout->pitch[1] = format == SVGA3D_NV12 ? pitch : pitch / 2u;
        layout->rows[1] = height / 2u;
        if (format == SVGA3D_YV12) {
            layout->pitch[2] = pitch / 2u;
            layout->rows[2] = height / 2u;
            if (bytes + bytes / 4u > UINT32_MAX) {
                return false;
            }
            layout->offset[2] = (uint32_t)(bytes + bytes / 4u);
        }
        bytes += bytes / 2u;
    }
    if (bytes > UINT32_MAX) {
        return false;
    }
    layout->size = (uint32_t)bytes;
    return true;
}

static inline bool vmsvga3d_video_copy_to_dxvk(
    SVGA3dSurfaceFormat format, uint32_t width, uint32_t height,
    const uint8_t *source, uint32_t source_pitch, uint32_t source_size,
    uint8_t *destination, uint32_t destination_pitch,
    uint32_t destination_size)
{
    VMSVGA3DVideoLayout layout;
    uint32_t row_bytes;
    uint32_t plane;
    uint32_t y;
    uint64_t rows = height;

    if (!vmsvga3d_video_yuv(format) || source == NULL || destination == NULL ||
        !vmsvga3d_video_layout(format, width, height, source_pitch, &layout) ||
        layout.size > source_size) {
        return false;
    }
    row_bytes = vmsvga3d_video_planar(format) ? width : width * 2u;
    if (vmsvga3d_video_planar(format)) {
        rows += height / 2u;
    }
    if (destination_pitch < row_bytes ||
        (uint64_t)destination_pitch * rows > destination_size) {
        return false;
    }
    for (y = 0; y < height; y++) {
        memcpy(destination + (size_t)y * destination_pitch,
               source + (size_t)y * source_pitch, row_bytes);
    }
    if (format == SVGA3D_NV12) {
        for (y = 0; y < height / 2u; y++) {
            memcpy(destination + (size_t)(height + y) * destination_pitch,
                   source + layout.offset[1] + (size_t)y * layout.pitch[1],
                   width);
        }
    } else if (format == SVGA3D_YV12) {
        /* DXVK strips padding from width-byte rows before GPU conversion.
         * Serialize V then U into that stream, rather than using half of the
         * locked pitch for chroma (which breaks widths not divisible by 4). */
        for (plane = 1; plane < 3; plane++) {
            for (y = 0; y < height / 2u; y++) {
                uint64_t offset = (uint64_t)(plane - 1u) * width * height / 4u +
                                  (uint64_t)y * (width / 2u);
                memcpy(destination + (size_t)(height + offset / width) *
                           destination_pitch + offset % width,
                       source + layout.offset[plane] +
                           (size_t)y * layout.pitch[plane], width / 2u);
            }
        }
    }
    return true;
}

static inline uint8_t vmsvga3d_video_clip(int value)
{
    return value < 0 ? 0 : value > 255 ? 255 : value;
}

static inline int vmsvga3d_video_div256(int value)
{
    /* Floor division without implementation-defined signed right shifts. */
    return value < 0 ? -((-value + 255) / 256) : value / 256;
}

static inline void vmsvga3d_video_decode(
    uint8_t y, uint8_t u, uint8_t v, bool bt709, uint8_t *bgra)
{
    int c = (int)y - 16;
    int d = (int)u - 128;
    int e = (int)v - 128;

    if (bt709) {
        int blue = 1164 * c + 2112 * d + 500;
        int green = 1164 * c - 213 * d - 533 * e + 500;
        int red = 1164 * c + 1793 * e + 500;

        /* Match DXVK's planar-video BT.709 matrix, including floor division
         * for negative results before clamping to the byte range. */
        bgra[0] = vmsvga3d_video_clip(
            blue < 0 ? -((-blue + 999) / 1000) : blue / 1000);
        bgra[1] = vmsvga3d_video_clip(
            green < 0 ? -((-green + 999) / 1000) : green / 1000);
        bgra[2] = vmsvga3d_video_clip(
            red < 0 ? -((-red + 999) / 1000) : red / 1000);
        bgra[3] = 255;
        return;
    }
    /* Packed D3D9 video uses the conventional BT.601 studio-range matrix. */
    bgra[0] = vmsvga3d_video_clip(
        vmsvga3d_video_div256(298 * c + 516 * d + 128));
    bgra[1] = vmsvga3d_video_clip(
        vmsvga3d_video_div256(298 * c - 100 * d - 208 * e + 128));
    bgra[2] = vmsvga3d_video_clip(
        vmsvga3d_video_div256(298 * c + 409 * e + 128));
    bgra[3] = 255;
}

static inline bool vmsvga3d_video_to_bgra(
    SVGA3dSurfaceFormat format, uint32_t width, uint32_t height,
    const uint8_t *source, uint32_t source_pitch, uint32_t source_size,
    uint8_t *destination, uint32_t destination_pitch, uint32_t destination_size)
{
    VMSVGA3DVideoLayout layout;
    VMSVGA3DVideoLayout rgb;
    uint32_t x;
    uint32_t y;

    if (source == NULL || destination == NULL ||
        !vmsvga3d_video_layout(format, width, height, source_pitch, &layout) ||
        !vmsvga3d_video_layout(SVGA3D_A8R8G8B8, width, height,
                                 destination_pitch, &rgb) ||
        layout.size > source_size || rgb.size > destination_size) {
        return false;
    }
    for (y = 0; y < height; y++) {
        const uint8_t *row = source + (size_t)y * source_pitch;
        uint8_t *out = destination + (size_t)y * destination_pitch;
        for (x = 0; x < width; x++, out += 4) {
            uint8_t luma;
            uint8_t u;
            uint8_t v;
            if (format == SVGA3D_R8G8B8A8_UNORM) {
                out[0] = row[4u * x + 2];
                out[1] = row[4u * x + 1];
                out[2] = row[4u * x];
                out[3] = row[4u * x + 3];
                continue;
            }
            if (format == SVGA3D_A8R8G8B8) {
                memcpy(out, row + 4u * x, 4);
                continue;
            }
            if (format == SVGA3D_YUY2 || format == SVGA3D_UYVY) {
                const uint8_t *pair = row + (size_t)(x / 2u) * 4u;
                uint32_t swap = format == SVGA3D_UYVY ? 1u : 0u;
                luma = pair[2u * (x & 1u) + swap];
                u = pair[1u - swap];
                v = pair[3u - swap];
            } else if (format == SVGA3D_NV12) {
                const uint8_t *uv = source + layout.offset[1] +
                    (size_t)(y / 2u) * layout.pitch[1] + (x & ~1u);
                luma = row[x];
                u = uv[0];
                v = uv[1];
            } else {
                size_t uv = (size_t)(y / 2u) * layout.pitch[1] + x / 2u;
                luma = row[x];
                v = source[layout.offset[1] + uv];
                u = source[layout.offset[2] + uv];
            }
            vmsvga3d_video_decode(
                luma, u, v, vmsvga3d_video_planar(format), out);
        }
    }
    return true;
}

static inline bool vmsvga3d_video_from_bgra(
    SVGA3dSurfaceFormat format, uint32_t width, uint32_t height,
    const uint8_t *source, uint32_t source_pitch, uint32_t source_size,
    uint8_t *destination, uint32_t destination_pitch, uint32_t destination_size)
{
    VMSVGA3DVideoLayout layout;
    VMSVGA3DVideoLayout rgb;
    uint32_t x;
    uint32_t y;
    uint32_t block_height = vmsvga3d_video_planar(format) ? 2u : 1u;

    if (source == NULL || destination == NULL ||
        !vmsvga3d_video_layout(format, width, height, destination_pitch, &layout) ||
        !vmsvga3d_video_layout(SVGA3D_A8R8G8B8, width, height,
                                 source_pitch, &rgb) ||
        layout.size > destination_size || rgb.size > source_size) {
        return false;
    }
    for (y = 0; y < height; y++) {
        const uint8_t *row = source + (size_t)y * source_pitch;
        uint8_t *out = destination + (size_t)y * destination_pitch;
        for (x = 0; x < width; x++) {
            const uint8_t *pixel = row + (size_t)x * 4u;
            if (format == SVGA3D_R8G8B8A8_UNORM) {
                out[4u * x] = pixel[2];
                out[4u * x + 1] = pixel[1];
                out[4u * x + 2] = pixel[0];
                out[4u * x + 3] = pixel[3];
            } else if (format == SVGA3D_A8R8G8B8) {
                memcpy(out + (size_t)x * 4u, pixel, 4);
            } else {
                bool bt709 = vmsvga3d_video_planar(format);
                uint8_t luma = vmsvga3d_video_clip(vmsvga3d_video_div256(
                    (bt709 ? 47 : 66) * pixel[2] +
                    (bt709 ? 157 : 129) * pixel[1] +
                    (bt709 ? 16 : 25) * pixel[0] + 128) + 16);
                if (vmsvga3d_video_planar(format)) {
                    out[x] = luma;
                } else {
                    out[2u * x + (format == SVGA3D_UYVY ? 1u : 0u)] = luma;
                }
            }
        }
    }
    if (!vmsvga3d_video_yuv(format)) {
        return true;
    }
    for (y = 0; y < height; y += block_height) {
        for (x = 0; x < width; x += 2) {
            uint32_t r = 0;
            uint32_t g = 0;
            uint32_t b = 0;
            uint32_t dy;
            uint32_t dx;
            uint32_t count = 2u * block_height;
            uint8_t u;
            uint8_t v;
            for (dy = 0; dy < block_height; dy++) {
                for (dx = 0; dx < 2; dx++) {
                    const uint8_t *pixel = source +
                        (size_t)(y + dy) * source_pitch + (size_t)(x + dx) * 4u;
                    b += pixel[0];
                    g += pixel[1];
                    r += pixel[2];
                }
            }
            /* Average the chroma over the complete 2x1 or 2x2 footprint. */
            u = vmsvga3d_video_clip(vmsvga3d_video_div256(
                ((block_height == 2 ? -26 : -38) * (int)r -
                 (block_height == 2 ? 87 : 74) * (int)g + 112 * (int)b +
                 (int)count * 128) / (int)count) + 128);
            v = vmsvga3d_video_clip(vmsvga3d_video_div256(
                (112 * (int)r - (block_height == 2 ? 102 : 94) * (int)g -
                 (block_height == 2 ? 10 : 18) * (int)b +
                 (int)count * 128) / (int)count) + 128);
            if (format == SVGA3D_NV12) {
                size_t uv = layout.offset[1] +
                    (size_t)(y / 2u) * layout.pitch[1] + x;
                destination[uv] = u;
                destination[uv + 1] = v;
            } else if (format == SVGA3D_YV12) {
                size_t uv = (size_t)(y / 2u) * layout.pitch[1] + x / 2u;
                destination[layout.offset[1] + uv] = v;
                destination[layout.offset[2] + uv] = u;
            } else {
                size_t pair = (size_t)y * destination_pitch + (size_t)x * 2u;
                uint32_t swap = format == SVGA3D_UYVY ? 1u : 0u;
                destination[pair + 1u - swap] = u;
                destination[pair + 3u - swap] = v;
            }
        }
    }
    return true;
}

/* Describe one plane's rows for an aligned planar-video rectangle.  Starts
 * must respect shared chroma samples; odd rectangle extents include the last
 * shared sample.  All arithmetic is validated before offsets are narrowed. */
static inline bool vmsvga3d_video_plane_box(
    SVGA3dSurfaceFormat format, const VMSVGA3DVideoLayout *layout,
    uint32_t plane, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
    uint32_t *offset, uint32_t *row_bytes, uint32_t *rows)
{
    uint64_t first;
    uint32_t bytes = width;
    uint32_t count = height;

    if (layout == NULL || offset == NULL || row_bytes == NULL || rows == NULL ||
        !vmsvga3d_video_planar(format) || plane >= layout->planes ||
        width == 0 || height == 0 || (x & 1u) != 0 || (y & 1u) != 0) {
        return false;
    }
    if (plane != 0) {
        y /= 2u;
        count = height / 2u + (height & 1u);
        if (format == SVGA3D_YV12) {
            x /= 2u;
            bytes = width / 2u + (width & 1u);
        } else {
            if (width == UINT32_MAX) {
                return false;
            }
            bytes = (width + 1u) & ~1u;
        }
    }
    if (x > layout->pitch[plane] || bytes > layout->pitch[plane] - x ||
        y > layout->rows[plane] || count > layout->rows[plane] - y) {
        return false;
    }
    first = (uint64_t)layout->offset[plane] +
            (uint64_t)y * layout->pitch[plane] + x;
    if (first + (uint64_t)(count - 1u) * layout->pitch[plane] + bytes >
        layout->size) {
        return false;
    }
    *offset = (uint32_t)first;
    *row_bytes = bytes;
    *rows = count;
    return true;
}

#endif /* HW_DISPLAY_VMWARE_VGA_VIDEO_H */
