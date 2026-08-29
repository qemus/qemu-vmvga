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

#ifndef HW_DISPLAY_VMWARE_VGA_3D_BACKEND_H
#define HW_DISPLAY_VMWARE_VGA_3D_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#include "svga_types.h"
#include "svga_reg.h"
#include "svga3d_cmd.h"
#include "svga3d_types.h"

struct vmsvga_state_s;

typedef struct vmsvga3d_backend_ops_vgpu9_s {
  bool (*context_define)(struct vmsvga_state_s *s, uint32_t cid);
  bool (*context_destroy)(struct vmsvga_state_s *s, uint32_t cid);
  bool (*set_transform)(struct vmsvga_state_s *s, uint32_t cid,
                        SVGA3dTransformType type, const float matrix[16]);
  bool (*set_z_range)(struct vmsvga_state_s *s, uint32_t cid,
                      const SVGA3dZRange *z_range);
  bool (*set_render_state)(struct vmsvga_state_s *s, uint32_t cid,
                           uint32_t count, const SVGA3dRenderState *states);
  bool (*set_render_target)(struct vmsvga_state_s *s, uint32_t cid,
                            SVGA3dRenderTargetType type,
                            const SVGA3dSurfaceImageId *target);
  bool (*set_texture_state)(struct vmsvga_state_s *s, uint32_t cid,
                            uint32_t count,
                            const SVGA3dTextureState *states);
  bool (*set_material)(struct vmsvga_state_s *s, uint32_t cid,
                       SVGA3dFace face, const SVGA3dMaterial *material);
  bool (*set_light_data)(struct vmsvga_state_s *s, uint32_t cid,
                         uint32_t index, const SVGA3dLightData *data);
  bool (*set_light_enabled)(struct vmsvga_state_s *s, uint32_t cid,
                            uint32_t index, uint32_t enabled);
  bool (*set_viewport)(struct vmsvga_state_s *s, uint32_t cid,
                       const SVGA3dRect *rect);
  bool (*set_clip_plane)(struct vmsvga_state_s *s, uint32_t cid,
                         uint32_t index, const float plane[4]);
  bool (*clear)(struct vmsvga_state_s *s, uint32_t cid,
                SVGA3dClearFlag clear_flags, uint32_t color, float depth,
                uint32_t stencil, uint32_t rect_count,
                const SVGA3dRect *rects);
  bool (*draw_primitives)(struct vmsvga_state_s *s, uint32_t cid,
                          uint32_t vertex_decl_count,
                          const SVGA3dVertexDecl *vertex_decls,
                          uint32_t range_count,
                          const SVGA3dPrimitiveRange *ranges,
                          uint32_t divisor_count,
                          const SVGA3dVertexDivisor *divisors);
  bool (*set_scissor)(struct vmsvga_state_s *s, uint32_t cid,
                      const SVGA3dRect *rect);
  bool (*generate_mipmaps)(struct vmsvga_state_s *s, uint32_t sid,
                           SVGA3dTextureFilter filter);
  bool (*shader_define)(struct vmsvga_state_s *s, uint32_t cid, uint32_t shid,
                        SVGA3dShaderType type, uint32_t bytecode_size,
                        const uint32_t *bytecode);
  bool (*shader_destroy)(struct vmsvga_state_s *s, uint32_t cid,
                         uint32_t shid, SVGA3dShaderType type);
  bool (*set_shader)(struct vmsvga_state_s *s, uint32_t cid,
                     SVGA3dShaderType type, uint32_t shid);
  bool (*set_shader_const)(struct vmsvga_state_s *s, uint32_t cid,
                           uint32_t reg, SVGA3dShaderType type,
                           SVGA3dShaderConstType ctype, uint32_t count,
                           const uint32_t (*values)[4]);
} VMSVGA3DBackendOpsVGPU9;

#ifdef __cplusplus
extern "C" {
#endif

bool vmsvga3d_backend_vgpu9_set(struct vmsvga_state_s *s,
                                 const VMSVGA3DBackendOpsVGPU9 *ops,
                                 void *opaque);
void *vmsvga3d_backend_vgpu9_opaque(struct vmsvga_state_s *s);

#ifdef __cplusplus
}
#endif

#endif
