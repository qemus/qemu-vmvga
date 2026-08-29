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

/*
 * Legacy VGPU9 backend.
 *
 * FIFO handlers decode and structurally validate command packets. This layer
 * owns the semantic VGPU9 operations, mirroring the boundary used by other
 * VMSVGA implementations while remaining independent of their backend code.
 * A D3D9 provider can replace these operations without learning the FIFO
 * packet layout.
 */

static bool vmsvga3d_backend_context_define(struct vmsvga_state_s *s,
                                             uint32_t cid) {
  struct vmsvga3d_state_s *state;
  VMSVGA3DContext *context;
  uint32_t i;

  if (cid >= SVGA3D_MAX_CONTEXT_IDS) {
    return false;
  };
  context = g_try_new0(VMSVGA3DContext, 1);
  if (context == NULL) {
    return false;
  };
  state = vmsvga3d_state_ensure(s);
  if (state == NULL) {
    g_free(context);
    return false;
  };

  context->cid = cid;
  for (i = 0; i < SVGA3D_RT_MAX; i++) {
    context->render_targets[i].sid = SVGA3D_INVALID_ID;
  };
  for (i = 0; i < SVGA3D_NUM_SHADERTYPE_PREDX; i++) {
    context->bound_shader[i] = SVGA3D_INVALID_ID;
  };
  vmsvga3d_context_free(state, state->contexts[cid]);
  state->contexts[cid] = context;
  return true;
};

static bool vmsvga3d_backend_context_destroy(struct vmsvga_state_s *s,
                                              uint32_t cid) {
  struct vmsvga3d_state_s *state = s->svga3d;

  if (state == NULL || cid >= SVGA3D_MAX_CONTEXT_IDS) {
    return false;
  };
  vmsvga3d_context_free(state, state->contexts[cid]);
  state->contexts[cid] = NULL;
  return true;
};

static bool vmsvga3d_backend_set_transform(struct vmsvga_state_s *s,
                                            uint32_t cid,
                                            SVGA3dTransformType type,
                                            const float matrix[16]) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || matrix == NULL || type < SVGA3D_TRANSFORM_MIN ||
      type >= SVGA3D_TRANSFORM_MAX) {
    return false;
  };
  memcpy(context->transform[type].matrix, matrix,
         sizeof(context->transform[type].matrix));
  context->transform[type].valid = true;
  return true;
};

static bool vmsvga3d_backend_set_z_range(struct vmsvga_state_s *s,
                                          uint32_t cid,
                                          const SVGA3dZRange *z_range) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || z_range == NULL) {
    return false;
  };
  context->z_range = *z_range;
  context->z_range_valid = true;
  return true;
};

static bool vmsvga3d_backend_set_render_state(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t count,
    const SVGA3dRenderState *states) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  uint32_t i;

  if (context == NULL || (count != 0 && states == NULL)) {
    return false;
  };
  for (i = 0; i < count; i++) {
    if (states[i].state < SVGA3D_RS_MIN || states[i].state >= SVGA3D_RS_MAX) {
      return false;
    };
  };
  for (i = 0; i < count; i++) {
    context->render_state[states[i].state].value = states[i].uintValue;
    context->render_state[states[i].state].valid = true;
  };
  return true;
};

static bool vmsvga3d_backend_set_render_target(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dRenderTargetType type,
    const SVGA3dSurfaceImageId *target) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  VMSVGA3DSurface *surface;
  VMSVGA3DSurfaceImage *image;

  if (context == NULL || target == NULL || type < SVGA3D_RT_MIN ||
      type >= SVGA3D_RT_MAX) {
    return false;
  };
  if (target->sid == SVGA3D_INVALID_ID) {
    context->render_targets[type] = *target;
    return true;
  };
  if (s->svga3d == NULL || target->sid >= SVGA3D_MAX_SURFACE_IDS) {
    return false;
  };
  surface = s->svga3d->surfaces[target->sid];
  if (!vmsvga3d_surface_image(surface, target, &image)) {
    return false;
  };
  context->render_targets[type] = *target;
  return true;
};

static bool vmsvga3d_backend_set_texture_state(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t count,
    const SVGA3dTextureState *states) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  uint32_t i;

  if (context == NULL || (count != 0 && states == NULL)) {
    return false;
  };
  for (i = 0; i < count; i++) {
    if (states[i].stage >= VMSVGA3D_MAX_TEXTURE_STAGES ||
        states[i].name < SVGA3D_TS_MIN || states[i].name >= SVGA3D_TS_MAX) {
      return false;
    };
    if (states[i].name == SVGA3D_TS_BIND_TEXTURE &&
        states[i].value != SVGA3D_INVALID_ID &&
        (s->svga3d == NULL || states[i].value >= SVGA3D_MAX_SURFACE_IDS ||
         s->svga3d->surfaces[states[i].value] == NULL)) {
      return false;
    };
  };
  for (i = 0; i < count; i++) {
    context->texture_state[states[i].stage][states[i].name].value =
        states[i].value;
    context->texture_state[states[i].stage][states[i].name].valid = true;
  };
  return true;
};

static bool vmsvga3d_backend_set_material(struct vmsvga_state_s *s,
                                           uint32_t cid, SVGA3dFace face,
                                           const SVGA3dMaterial *material) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || material == NULL ||
      (face != SVGA3D_FACE_FRONT && face != SVGA3D_FACE_BACK &&
       face != SVGA3D_FACE_FRONT_BACK)) {
    return false;
  };
  if (face == SVGA3D_FACE_FRONT || face == SVGA3D_FACE_FRONT_BACK) {
    context->material[SVGA3D_FACE_FRONT].material = *material;
    context->material[SVGA3D_FACE_FRONT].valid = true;
  };
  if (face == SVGA3D_FACE_BACK || face == SVGA3D_FACE_FRONT_BACK) {
    context->material[SVGA3D_FACE_BACK].material = *material;
    context->material[SVGA3D_FACE_BACK].valid = true;
  };
  return true;
};

static bool vmsvga3d_backend_set_light_data(struct vmsvga_state_s *s,
                                             uint32_t cid, uint32_t index,
                                             const SVGA3dLightData *data) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || data == NULL || index >= SVGA3D_NUM_LIGHTS ||
      data->type < SVGA3D_LIGHTTYPE_MIN || data->type >= SVGA3D_LIGHTTYPE_MAX) {
    return false;
  };
  context->light[index].data = *data;
  context->light[index].data_valid = true;
  return true;
};

static bool vmsvga3d_backend_set_light_enabled(struct vmsvga_state_s *s,
                                                uint32_t cid,
                                                uint32_t index,
                                                uint32_t enabled) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || index >= SVGA3D_NUM_LIGHTS) {
    return false;
  };
  context->light[index].enabled = enabled;
  context->light[index].enabled_valid = true;
  return true;
};

static bool vmsvga3d_backend_set_viewport(struct vmsvga_state_s *s,
                                           uint32_t cid,
                                           const SVGA3dRect *rect) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || rect == NULL) {
    return false;
  };
  context->viewport = *rect;
  context->viewport_valid = true;
  return true;
};

static bool vmsvga3d_backend_set_clip_plane(struct vmsvga_state_s *s,
                                             uint32_t cid, uint32_t index,
                                             const float plane[4]) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || plane == NULL || index >= VMSVGA3D_MAX_CLIP_PLANES) {
    return false;
  };
  memcpy(context->clip_plane[index].plane, plane,
         sizeof(context->clip_plane[index].plane));
  context->clip_plane[index].valid = true;
  return true;
};

static bool vmsvga3d_backend_clear(struct vmsvga_state_s *s, uint32_t cid,
                                    SVGA3dClearFlag clear_flags,
                                    uint32_t color, float depth,
                                    uint32_t stencil, uint32_t rect_count,
                                    const SVGA3dRect *rects) {
  VMSVGA3DContext *context;
  uint32_t type;
  uint32_t flags = clear_flags;
  bool valid = true;

  if ((rect_count != 0 && rects == NULL) ||
      (flags & ~(SVGA3D_CLEAR_COLOR | SVGA3D_CLEAR_DEPTH |
                 SVGA3D_CLEAR_STENCIL))) {
    return false;
  };
  context = vmsvga3d_context(s, cid);
  if (context == NULL) {
    return false;
  };

  if (flags & SVGA3D_CLEAR_COLOR) {
    for (type = SVGA3D_RT_COLOR0; type <= SVGA3D_RT_COLOR7 && valid; type++) {
      valid = vmsvga3d_clear_target(s, context, type, color, depth, stencil,
                                    rects, rect_count, false);
    };
  };
  if (valid && (flags & SVGA3D_CLEAR_DEPTH)) {
    valid = vmsvga3d_clear_target(s, context, SVGA3D_RT_DEPTH, color, depth,
                                  stencil, rects, rect_count, false);
  };
  if (valid && (flags & SVGA3D_CLEAR_STENCIL)) {
    valid = vmsvga3d_clear_target(s, context, SVGA3D_RT_STENCIL, color, depth,
                                  stencil, rects, rect_count, false);
  };
  if (!valid) {
    return false;
  };

  if (flags & SVGA3D_CLEAR_COLOR) {
    for (type = SVGA3D_RT_COLOR0; type <= SVGA3D_RT_COLOR7 && valid; type++) {
      valid = vmsvga3d_clear_target(s, context, type, color, depth, stencil,
                                    rects, rect_count, true);
    };
  };
  if (valid && (flags & SVGA3D_CLEAR_DEPTH)) {
    valid = vmsvga3d_clear_target(s, context, SVGA3D_RT_DEPTH, color, depth,
                                  stencil, rects, rect_count, true);
  };
  if (valid && (flags & SVGA3D_CLEAR_STENCIL)) {
    valid = vmsvga3d_clear_target(s, context, SVGA3D_RT_STENCIL, color, depth,
                                  stencil, rects, rect_count, true);
  };
  return valid;
};

static bool vmsvga3d_backend_draw_primitives(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t vertex_decl_count,
    const SVGA3dVertexDecl *vertex_decls, uint32_t range_count,
    const SVGA3dPrimitiveRange *ranges, uint32_t divisor_count,
    const SVGA3dVertexDivisor *divisors) {
  struct vmsvga3d_state_s *state = s->svga3d;
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  uint32_t i;

  if (state == NULL || context == NULL || vertex_decl_count == 0 ||
      vertex_decl_count > SVGA3D_MAX_VERTEX_ARRAYS || range_count == 0 ||
      range_count > SVGA3D_MAX_DRAW_PRIMITIVE_RANGES || vertex_decls == NULL ||
      ranges == NULL || (divisor_count != 0 && divisors == NULL) ||
      (divisor_count != 0 && divisor_count != vertex_decl_count) ||
      !vmsvga3d_draw_decls_valid(vertex_decls, vertex_decl_count) ||
      !vmsvga3d_draw_vertex_surfaces_valid(state, vertex_decls,
                                           vertex_decl_count) ||
      !vmsvga3d_draw_ranges_valid(state, ranges, range_count)) {
    return false;
  };
  for (i = 0; i < divisor_count; i++) {
    if (divisors[i].indexedData && divisors[i].instanceData) {
      return false;
    };
  };

  /* Rendering stops at the VGPU9 backend boundary until D3D9 is connected. */
  return true;
};

static bool vmsvga3d_backend_set_scissor(struct vmsvga_state_s *s,
                                          uint32_t cid,
                                          const SVGA3dRect *rect) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || rect == NULL) {
    return false;
  };
  context->scissor = *rect;
  context->scissor_valid = true;
  return true;
};

static bool vmsvga3d_backend_generate_mipmaps(struct vmsvga_state_s *s,
                                               uint32_t sid,
                                               SVGA3dTextureFilter filter) {
  VMSVGA3DSurface *surface;

  if (s->svga3d == NULL || sid >= SVGA3D_MAX_SURFACE_IDS ||
      filter < SVGA3D_TEX_FILTER_MIN || filter >= SVGA3D_TEX_FILTER_MAX) {
    return false;
  };
  surface = s->svga3d->surfaces[sid];
  if (surface == NULL) {
    return false;
  };

  /*
   * Preserve the requested autogen state now. Actual mip generation belongs
   * to the renderer backend; the CPU surface store remains authoritative until
   * that renderer exists.
   */
  surface->autogen_filter = filter;
  return true;
};

static bool vmsvga3d_backend_shader_define(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t shid,
    SVGA3dShaderType type, uint32_t bytecode_size, const uint32_t *bytecode) {
  struct vmsvga3d_state_s *state;
  VMSVGA3DContext *context;
  VMSVGA3DShader *shader = NULL;
  VMSVGA3DShader *old_shader;
  uint32_t type_index;
  size_t new_shader_bytes;

  context = vmsvga3d_context(s, cid);
  if (context == NULL || !vmsvga3d_shader_type_index(type, &type_index) ||
      shid >= SVGA3D_MAX_SHADERIDS || bytecode == NULL || bytecode_size == 0 ||
      bytecode_size > SVGA3D_MAX_SHADER_MEMORY_BYTES ||
      bytecode_size % sizeof(uint32_t) != 0) {
    return false;
  };

  state = s->svga3d;
  old_shader = context->shader[type_index][shid];
  new_shader_bytes = state->shader_bytes;
  if (old_shader != NULL) {
    if (new_shader_bytes < old_shader->bytecode_size) {
      return false;
    };
    new_shader_bytes -= old_shader->bytecode_size;
  };
  if (new_shader_bytes > SVGA3D_MAX_SHADER_MEMORY_BYTES ||
      bytecode_size > SVGA3D_MAX_SHADER_MEMORY_BYTES - new_shader_bytes) {
    return false;
  };

  shader = g_try_new0(VMSVGA3DShader, 1);
  if (shader != NULL) {
    shader->bytecode = g_try_malloc(bytecode_size);
  };
  if (shader == NULL || shader->bytecode == NULL) {
    vmsvga3d_shader_free(shader);
    return false;
  };
  shader->shid = shid;
  shader->type = type;
  shader->bytecode_size = bytecode_size;
  memcpy(shader->bytecode, bytecode, bytecode_size);

  if (old_shader != NULL) {
    vmsvga3d_shader_free(old_shader);
  };
  context->shader[type_index][shid] = shader;
  state->shader_bytes = new_shader_bytes + bytecode_size;
  return true;
};

static bool vmsvga3d_backend_shader_destroy(struct vmsvga_state_s *s,
                                             uint32_t cid, uint32_t shid,
                                             SVGA3dShaderType type) {
  struct vmsvga3d_state_s *state = s->svga3d;
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  VMSVGA3DShader *shader;
  uint32_t type_index;

  if (state == NULL || context == NULL ||
      !vmsvga3d_shader_type_index(type, &type_index) ||
      shid >= SVGA3D_MAX_SHADERIDS) {
    return false;
  };
  shader = context->shader[type_index][shid];
  if (shader == NULL) {
    return true;
  };
  if (state->shader_bytes >= shader->bytecode_size) {
    state->shader_bytes -= shader->bytecode_size;
  } else {
    state->shader_bytes = 0;
  };
  vmsvga3d_shader_free(shader);
  context->shader[type_index][shid] = NULL;
  return true;
};

static bool vmsvga3d_backend_set_shader(struct vmsvga_state_s *s,
                                         uint32_t cid,
                                         SVGA3dShaderType type,
                                         uint32_t shid) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  uint32_t type_index;

  if (context == NULL || !vmsvga3d_shader_type_index(type, &type_index)) {
    return false;
  };
  if (shid == SVGA3D_INVALID_ID) {
    context->bound_shader[type_index] = SVGA3D_INVALID_ID;
    return true;
  };
  if (shid >= SVGA3D_MAX_SHADERIDS || context->shader[type_index][shid] == NULL) {
    return false;
  };
  context->bound_shader[type_index] = shid;
  return true;
};

static bool vmsvga3d_backend_set_shader_const(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t reg,
    SVGA3dShaderType type, SVGA3dShaderConstType ctype, uint32_t count,
    const uint32_t (*values)[4]) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  VMSVGA3DShaderConstant *constants;
  uint32_t limit;
  uint32_t type_index;
  uint32_t i;

  limit = vmsvga3d_shader_const_limit(ctype);
  if (context == NULL || limit == 0 || count == 0 || values == NULL ||
      !vmsvga3d_shader_type_index(type, &type_index) || reg >= limit ||
      count > limit - reg) {
    return false;
  };
  constants = vmsvga3d_shader_const_array(context, type_index, ctype);
  if (constants == NULL) {
    return false;
  };
  for (i = 0; i < count; i++) {
    memcpy(constants[reg + i].values, values[i],
           sizeof(constants[reg + i].values));
    constants[reg + i].valid = true;
  };
  return true;
};

static const VMSVGA3DBackendOpsVGPU9 vmsvga3d_backend_vgpu9_default = {
    .context_define = vmsvga3d_backend_context_define,
    .context_destroy = vmsvga3d_backend_context_destroy,
    .set_transform = vmsvga3d_backend_set_transform,
    .set_z_range = vmsvga3d_backend_set_z_range,
    .set_render_state = vmsvga3d_backend_set_render_state,
    .set_render_target = vmsvga3d_backend_set_render_target,
    .set_texture_state = vmsvga3d_backend_set_texture_state,
    .set_material = vmsvga3d_backend_set_material,
    .set_light_data = vmsvga3d_backend_set_light_data,
    .set_light_enabled = vmsvga3d_backend_set_light_enabled,
    .set_viewport = vmsvga3d_backend_set_viewport,
    .set_clip_plane = vmsvga3d_backend_set_clip_plane,
    .clear = vmsvga3d_backend_clear,
    .draw_primitives = vmsvga3d_backend_draw_primitives,
    .set_scissor = vmsvga3d_backend_set_scissor,
    .generate_mipmaps = vmsvga3d_backend_generate_mipmaps,
    .shader_define = vmsvga3d_backend_shader_define,
    .shader_destroy = vmsvga3d_backend_shader_destroy,
    .set_shader = vmsvga3d_backend_set_shader,
    .set_shader_const = vmsvga3d_backend_set_shader_const,
};
