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

#ifndef HW_DISPLAY_VMWARE_VGA_3D_STATE_H
#define HW_DISPLAY_VMWARE_VGA_3D_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "svga_types.h"
#include "svga_reg.h"
#include "svga3d_cmd.h"
#include "svga3d_types.h"

struct vmsvga_state_s;

static bool vmsvga3d_state_context_define(struct vmsvga_state_s *s, uint32_t cid);
static bool vmsvga3d_state_context_destroy(struct vmsvga_state_s *s, uint32_t cid);
static bool vmsvga3d_state_set_transform(struct vmsvga_state_s *s, uint32_t cid,
                                  SVGA3dTransformType type,
                                  const float matrix[16]);
static bool vmsvga3d_state_set_z_range(struct vmsvga_state_s *s, uint32_t cid,
                                const SVGA3dZRange *z_range);
static bool vmsvga3d_state_set_render_state(struct vmsvga_state_s *s, uint32_t cid,
                                     uint32_t count,
                                     const SVGA3dRenderState *states);
static bool vmsvga3d_state_set_render_target(struct vmsvga_state_s *s, uint32_t cid,
                                      SVGA3dRenderTargetType type,
                                      const SVGA3dSurfaceImageId *target);
static bool vmsvga3d_state_set_texture_state(struct vmsvga_state_s *s, uint32_t cid,
                                      uint32_t count,
                                      const SVGA3dTextureState *states);
static bool vmsvga3d_state_set_material(struct vmsvga_state_s *s, uint32_t cid,
                                 SVGA3dFace face,
                                 const SVGA3dMaterial *material);
static bool vmsvga3d_state_set_light_data(struct vmsvga_state_s *s, uint32_t cid,
                                   uint32_t index,
                                   const SVGA3dLightData *data);
static bool vmsvga3d_state_set_light_enabled(struct vmsvga_state_s *s, uint32_t cid,
                                      uint32_t index, uint32_t enabled);
static bool vmsvga3d_state_set_viewport(struct vmsvga_state_s *s, uint32_t cid,
                                 const SVGA3dRect *rect);
static bool vmsvga3d_state_set_clip_plane(struct vmsvga_state_s *s, uint32_t cid,
                                   uint32_t index, const float plane[4]);
static bool vmsvga3d_state_clear(struct vmsvga_state_s *s, uint32_t cid,
                          SVGA3dClearFlag clear_flags, uint32_t color,
                          float depth, uint32_t stencil, uint32_t rect_count,
                          const SVGA3dRect *rects);
static bool vmsvga3d_state_draw_primitives(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t vertex_decl_count,
    const SVGA3dVertexDecl *vertex_decls, uint32_t range_count,
    const SVGA3dPrimitiveRange *ranges, uint32_t divisor_count,
    const SVGA3dVertexDivisor *divisors);
static bool vmsvga3d_state_set_scissor(struct vmsvga_state_s *s, uint32_t cid,
                                const SVGA3dRect *rect);
static bool vmsvga3d_state_generate_mipmaps(struct vmsvga_state_s *s, uint32_t sid,
                                     SVGA3dTextureFilter filter);
typedef enum vmsvga3d_query_wait_status_e {
  VMSVGA3D_QUERY_WAIT_FAILED = 0,
  VMSVGA3D_QUERY_WAIT_READY,
  VMSVGA3D_QUERY_WAIT_NEEDS_RENDERER,
} VMSVGA3DQueryWaitStatus;

static bool vmsvga3d_state_query_begin(struct vmsvga_state_s *s, uint32_t cid,
                                       SVGA3dQueryType type);
static bool vmsvga3d_state_query_end(struct vmsvga_state_s *s, uint32_t cid,
                                     SVGA3dQueryType type);
bool vmsvga3d_state_query_complete(struct vmsvga_state_s *s, uint32_t cid,
                                    SVGA3dQueryType type, uint32_t result);
static VMSVGA3DQueryWaitStatus vmsvga3d_state_query_wait(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dQueryType type,
    uint32_t *result);
static bool vmsvga3d_state_shader_define(struct vmsvga_state_s *s, uint32_t cid,
                                  uint32_t shid, SVGA3dShaderType type,
                                  uint32_t bytecode_size,
                                  const uint32_t *bytecode);
static bool vmsvga3d_state_shader_destroy(struct vmsvga_state_s *s, uint32_t cid,
                                   uint32_t shid, SVGA3dShaderType type);
static bool vmsvga3d_state_set_shader(struct vmsvga_state_s *s, uint32_t cid,
                               SVGA3dShaderType type, uint32_t shid);
static bool vmsvga3d_state_set_shader_const(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t reg,
    SVGA3dShaderType type, SVGA3dShaderConstType ctype, uint32_t count,
    const uint32_t (*values)[4]);

#endif
