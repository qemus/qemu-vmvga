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
 * Guest-visible 3D protocol shadow state.
 *
 * FIFO handlers own packet framing and call these operations directly.  This
 * file keeps legacy VGPU9 state and VGPU10 DX-context shadow state separate
 * from FIFO decoding and renderer dispatch.  DX helpers here only mutate or
 * copy protocol state; renderer-side bind/restore work is intentionally left
 * to the shared execution layer.
 */

static bool vmsvga3d_state_context_define(struct vmsvga_state_s *s,
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

static bool vmsvga3d_state_context_destroy(struct vmsvga_state_s *s,
                                              uint32_t cid) {
  struct vmsvga3d_state_s *state = s->svga3d;

  if (state == NULL || cid >= SVGA3D_MAX_CONTEXT_IDS) {
    return false;
  };
  vmsvga3d_context_free(state, state->contexts[cid]);
  state->contexts[cid] = NULL;
  return true;
};

#if defined(__GNUC__)
#define VMSVGA3D_DX_STATE_UNUSED __attribute__((unused))
#else
#define VMSVGA3D_DX_STATE_UNUSED
#endif

static void vmsvga3d_state_dx_context_init_shadow(
    SVGADXContextMobFormat *shadow) {
  uint32_t i;
  uint32_t j;

  memset(shadow, 0xff, sizeof(*shadow));

  shadow->inputAssembly.layoutId = SVGA3D_INVALID_ID;
  for (i = 0; i < SVGA3D_DX_MAX_VERTEXBUFFERS; i++) {
    shadow->inputAssembly.vertexBuffers[i].bufferId = SVGA3D_INVALID_ID;
    shadow->inputAssembly.vertexBuffers[i].stride = 0;
    shadow->inputAssembly.vertexBuffers[i].offset = 0;
  }
  shadow->inputAssembly.indexBufferSid = SVGA3D_INVALID_ID;
  shadow->inputAssembly.indexBufferOffset = 0;
  shadow->inputAssembly.indexBufferFormat = SVGA3D_FORMAT_INVALID;
  shadow->inputAssembly.topology = SVGA3D_PRIMITIVE_INVALID;

  shadow->renderState.blendStateId = SVGA3D_INVALID_ID;
  memset(shadow->renderState.blendFactor, 0,
         sizeof(shadow->renderState.blendFactor));
  shadow->renderState.sampleMask = 0;
  shadow->renderState.depthStencilStateId = SVGA3D_INVALID_ID;
  shadow->renderState.stencilRef = 0;
  shadow->renderState.rasterizerStateId = SVGA3D_INVALID_ID;
  shadow->renderState.depthStencilViewId = SVGA3D_INVALID_ID;
  for (i = 0; i < SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS; i++) {
    shadow->renderState.renderTargetViewIds[i] = SVGA3D_INVALID_ID;
  }

  for (i = 0; i < SVGA3D_DX_MAX_SOTARGETS; i++) {
    shadow->streamOut.targets[i] = SVGA3D_INVALID_ID;
  }
  shadow->streamOut.soid = SVGA3D_INVALID_ID;

  shadow->uavSpliceIndex = 0;
  shadow->numViewports = 0;
  shadow->numScissorRects = 0;
  memset(shadow->viewports, 0, sizeof(shadow->viewports));
  memset(shadow->scissorRects, 0, sizeof(shadow->scissorRects));

  shadow->predication.queryID = SVGA3D_INVALID_ID;
  shadow->predication.value = 0;
  shadow->shaderIfaceMobid = SVGA3D_INVALID_ID;
  shadow->shaderIfaceOffset = 0;

  for (i = 0; i < SVGA3D_NUM_SHADERTYPE; i++) {
    shadow->shaderState[i].shaderId = SVGA3D_INVALID_ID;
    for (j = 0; j < SVGA3D_DX_MAX_CONSTBUFFERS; j++) {
      shadow->shaderState[i].constantBuffers[j].sid = SVGA3D_INVALID_ID;
      shadow->shaderState[i].constantBuffers[j].offsetInBytes = 0;
      shadow->shaderState[i].constantBuffers[j].sizeInBytes = 0;
    }
    for (j = 0; j < SVGA3D_DX_MAX_SRVIEWS; j++) {
      shadow->shaderState[i].shaderResources[j] = SVGA3D_INVALID_ID;
    }
    for (j = 0; j < SVGA3D_DX_MAX_SAMPLERS; j++) {
      shadow->shaderState[i].samplers[j] = SVGA3D_INVALID_ID;
    }
  }

  for (i = 0; i < SVGA3D_MAX_QUERY; i++) {
    shadow->queryID[i] = SVGA3D_INVALID_ID;
  }
  for (i = 0; i < SVGA_COTABLE_MAX; i++) {
    shadow->cotables[i].mobid = SVGA3D_INVALID_ID;
  }
  for (i = 0; i < SVGA3D_DX11_1_MAX_UAVIEWS; i++) {
    shadow->uaViewIds[i] = SVGA3D_INVALID_ID;
    shadow->csuaViewIds[i] = SVGA3D_INVALID_ID;
  }
}

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_context_define(struct vmsvga_state_s *s,
                                             uint32_t cid) {
  struct vmsvga3d_state_s *state;
  VMSVGA3DDXContext *context;

  if (cid >= SVGA3D_MAX_CONTEXT_IDS) {
    return false;
  };
  context = g_try_new0(VMSVGA3DDXContext, 1);
  if (context == NULL) {
    return false;
  };
  state = vmsvga3d_state_ensure(s);
  if (state == NULL) {
    g_free(context);
    return false;
  };

  context->cid = cid;
  vmsvga3d_state_dx_context_init_shadow(&context->shadow);
  context->renderer_dirty = VMSVGA3D_DX_CTX_F_STATE_ALL;
  for (uint32_t type = 0; type < SVGA_COTABLE_MAX; type++) {
    context->cotables[type].mobid = SVGA3D_INVALID_ID;
  };

  vmsvga3d_dx_context_free(state->dx_contexts[cid]);
  state->dx_contexts[cid] = context;
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_context_destroy(struct vmsvga_state_s *s,
                                              uint32_t cid) {
  struct vmsvga3d_state_s *state = s->svga3d;

  if (state == NULL || cid >= SVGA3D_MAX_CONTEXT_IDS ||
      state->dx_contexts[cid] == NULL) {
    return false;
  };
  vmsvga3d_dx_context_free(state->dx_contexts[cid]);
  state->dx_contexts[cid] = NULL;
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_context_bind(
    struct vmsvga_state_s *s, uint32_t cid,
    const SVGADXContextMobFormat *valid_contents) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL) {
    return false;
  };
  if (valid_contents != NULL) {
    memcpy(&context->shadow, valid_contents, sizeof(context->shadow));
  };
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_context_readback(
    struct vmsvga_state_s *s, uint32_t cid,
    SVGADXContextMobFormat *contents) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || contents == NULL) {
    return false;
  };
  memcpy(contents, &context->shadow, sizeof(*contents));
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_context_invalidate(struct vmsvga_state_s *s,
                                                 uint32_t cid) {
  /* VirtualBox deliberately leaves backend context invalidation unimplemented. */
  (void)s;
  (void)cid;
  return false;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_render_target_count(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t *count) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || count == NULL) {
    return false;
  };
  *count = context->render_target_count;
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_constant_buffer(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10ConstantBufferPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  SVGA3dConstantBufferBinding *binding;

  if (context == NULL || plan == NULL || !plan->shadow_update ||
      plan->stage_index >= SVGA3D_NUM_SHADERTYPE ||
      plan->slot >= SVGA3D_DX_MAX_CONSTBUFFERS) {
    return false;
  };
  binding = &context->shadow.shaderState[plan->stage_index]
                 .constantBuffers[plan->slot];
  binding->sid = plan->sid;
  binding->offsetInBytes = plan->offset_in_bytes;
  binding->sizeInBytes = plan->size_in_bytes;
  if (plan->sid != SVGA3D_INVALID_ID &&
      context->constant_buffer_max_bound[plan->stage_index] < plan->slot + 1u) {
    context->constant_buffer_max_bound[plan->stage_index] = plan->slot + 1u;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_shader_resources(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10ShaderResourceSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  uint32_t c_bound;
  int32_t last_not_null = -1;
  bool modified = false;
  uint32_t i;

  if (context == NULL || plan == NULL || !plan->shadow_update_atomic ||
      plan->stage_index >= SVGA3D_NUM_SHADERTYPE ||
      plan->start_view > SVGA3D_DX_MAX_SRVIEWS ||
      plan->shadow_update_count >
          SVGA3D_DX_MAX_SRVIEWS - plan->start_view) {
    return false;
  };
  for (i = 0; i < plan->shadow_update_count; i++) {
    uint32_t slot = plan->start_view + i;
    uint32_t *shadow_id =
        &context->shadow.shaderState[plan->stage_index].shaderResources[slot];

    if (*shadow_id != plan->ids[i]) {
      *shadow_id = plan->ids[i];
      context->shader_resource_modified[plan->stage_index][slot / 64u] |=
          UINT64_C(1) << (slot % 64u);
      modified = true;
    }
    if (plan->ids[i] != SVGA3D_INVALID_ID) {
      last_not_null = (int32_t)i;
    }
  };
  c_bound = plan->start_view +
            (last_not_null >= 0 ? (uint32_t)last_not_null + 1u : 0u);
  if (context->shader_resource_max_bound[plan->stage_index] < c_bound) {
    context->shader_resource_max_bound[plan->stage_index] = c_bound;
  }
  if (modified) {
    context->renderer_dirty |=
        VMSVGA3D_DX_CTX_F_STATE_SRV_VS << plan->stage_index;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_shader(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10ShaderSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || plan == NULL || !plan->shadow_update ||
      plan->stage_index >= SVGA3D_NUM_SHADERTYPE) {
    return false;
  };
  context->shadow.shaderState[plan->stage_index].shaderId = plan->shader_id;
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_samplers(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10SamplerSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  bool modified = false;
  uint32_t i;

  if (context == NULL || plan == NULL ||
      plan->stage_index >= SVGA3D_NUM_SHADERTYPE ||
      plan->start_sampler > SVGA3D_DX_MAX_SAMPLERS ||
      plan->shadow_update_count >
          SVGA3D_DX_MAX_SAMPLERS - plan->start_sampler) {
    return false;
  };
  for (i = 0; i < plan->shadow_update_count; i++) {
    uint32_t *shadow_id =
        &context->shadow.shaderState[plan->stage_index]
             .samplers[plan->start_sampler + i];

    if (*shadow_id != plan->ids[i]) {
      *shadow_id = plan->ids[i];
      modified = true;
    }
  };
  if (modified) {
    context->renderer_dirty |=
        VMSVGA3D_DX_CTX_F_STATE_SAMPLER_VS << plan->stage_index;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_input_layout(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10InputLayoutSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || plan == NULL || !plan->shadow_update) {
    return false;
  };
  if (context->shadow.inputAssembly.layoutId != plan->layout_id) {
    context->shadow.inputAssembly.layoutId = plan->layout_id;
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_INPUTLAYOUT;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_vertex_buffers(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10VertexBufferSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  uint32_t max_bound = 0;
  bool modified = false;
  uint32_t i;

  if (context == NULL || plan == NULL ||
      plan->start_buffer > SVGA3D_DX_MAX_VERTEXBUFFERS ||
      plan->shadow_update_count >
          SVGA3D_DX_MAX_VERTEXBUFFERS - plan->start_buffer) {
    return false;
  };
  for (i = 0; i < plan->shadow_update_count; i++) {
    uint32_t slot = plan->start_buffer + i;
    SVGA3dBufferBinding *binding =
        &context->shadow.inputAssembly.vertexBuffers[slot];

    if (binding->bufferId != plan->bindings[i].sid ||
        binding->stride != plan->bindings[i].stride ||
        binding->offset != plan->bindings[i].offset) {
      binding->bufferId = plan->bindings[i].sid;
      binding->stride = plan->bindings[i].stride;
      binding->offset = plan->bindings[i].offset;
      context->vertex_buffer_modified |= UINT64_C(1) << slot;
      modified = true;
    }
    if (plan->bindings[i].sid != SVGA3D_INVALID_ID) {
      max_bound = slot + 1u;
    }
  };
  if (modified) {
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_VERTEXBUFFER;
  }
  if (context->vertex_buffer_max_bound < max_bound) {
    context->vertex_buffer_max_bound = max_bound;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_index_buffer(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10IndexBufferSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || plan == NULL || !plan->shadow_update) {
    return false;
  };
  if (context->shadow.inputAssembly.indexBufferSid != plan->sid ||
      context->shadow.inputAssembly.indexBufferOffset != plan->offset ||
      context->shadow.inputAssembly.indexBufferFormat != (uint32_t)plan->format) {
    context->shadow.inputAssembly.indexBufferSid = plan->sid;
    context->shadow.inputAssembly.indexBufferOffset = plan->offset;
    context->shadow.inputAssembly.indexBufferFormat = plan->format;
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_INDEXBUFFER;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_viewports(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10ViewportsSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  bool modified;
  uint32_t i;

  if (context == NULL || plan == NULL || !plan->shadow_update ||
      plan->count > SVGA3D_DX_MAX_VIEWPORTS) {
    return false;
  };
  modified = context->shadow.numViewports != (uint8_t)plan->count;
  context->shadow.numViewports = (uint8_t)plan->count;
  for (i = 0; i < plan->count; i++) {
    const SVGA3dViewport *src = &plan->viewports[i];
    SVGA3dViewport *dst = &context->shadow.viewports[i];

    if (src->x != dst->x || src->y != dst->y ||
        src->width != dst->width || src->height != dst->height ||
        src->minDepth != dst->minDepth || src->maxDepth != dst->maxDepth) {
      *dst = *src;
      modified = true;
    }
  };
  if (modified) {
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_VIEWPORT;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_topology(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10TopologySetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || plan == NULL || !plan->shadow_update) {
    return false;
  };
  if (context->shadow.inputAssembly.topology != (uint32_t)plan->topology) {
    context->shadow.inputAssembly.topology = plan->topology;
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_TOPOLOGY;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_blend_state(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10BlendStateSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || plan == NULL || !plan->shadow_update) {
    return false;
  };
  if (context->shadow.renderState.blendStateId != plan->blend_id) {
    context->shadow.renderState.blendStateId = plan->blend_id;
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE;
  }
  if (memcmp(context->shadow.renderState.blendFactor, plan->blend_factor,
             sizeof(context->shadow.renderState.blendFactor)) != 0) {
    memcpy(context->shadow.renderState.blendFactor, plan->blend_factor,
           sizeof(context->shadow.renderState.blendFactor));
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE;
  }
  if (context->shadow.renderState.sampleMask != plan->sample_mask) {
    context->shadow.renderState.sampleMask = plan->sample_mask;
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_BLENDSTATE;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_depth_stencil_state(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10DepthStencilStateSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || plan == NULL || !plan->shadow_update) {
    return false;
  };
  if (context->shadow.renderState.depthStencilStateId !=
      plan->depth_stencil_id) {
    context->shadow.renderState.depthStencilStateId = plan->depth_stencil_id;
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_DEPTHSTENCILSTATE;
  }
  if (context->shadow.renderState.stencilRef != plan->stencil_ref) {
    context->shadow.renderState.stencilRef = plan->stencil_ref;
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_DEPTHSTENCILSTATE;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_rasterizer_state(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10RasterizerStateSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || plan == NULL || !plan->shadow_update) {
    return false;
  };
  if (context->shadow.renderState.rasterizerStateId != plan->rasterizer_id) {
    context->shadow.renderState.rasterizerStateId = plan->rasterizer_id;
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RASTERIZERSTATE;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_scissors(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10ScissorPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  bool modified;
  uint32_t i;

  if (context == NULL || plan == NULL || !plan->shadow_update ||
      plan->count > SVGA3D_DX_MAX_SCISSORRECTS) {
    return false;
  };
  modified = context->shadow.numScissorRects != (uint8_t)plan->count;
  context->shadow.numScissorRects = (uint8_t)plan->count;
  for (i = 0; i < plan->count; i++) {
    const SVGASignedRect *src = &plan->rects[i];
    SVGASignedRect *dst = &context->shadow.scissorRects[i];

    if (src->left != dst->left || src->top != dst->top ||
        src->right != dst->right || src->bottom != dst->bottom) {
      *dst = *src;
      modified = true;
    }
  };
  if (modified) {
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_SCISSORRECT;
  }
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_render_targets(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10RenderTargetsSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);
  uint32_t render_target_count = 0;
  bool modified = false;
  uint32_t i;

  if (context == NULL || plan == NULL || !plan->shadow_update_atomic ||
      plan->shadow_update_count > SVGA3D_MAX_RENDER_TARGETS) {
    return false;
  };
  if (context->shadow.renderState.depthStencilViewId !=
      plan->depth_stencil_view_id) {
    context->shadow.renderState.depthStencilViewId =
        plan->depth_stencil_view_id;
    modified = true;
  }
  for (i = 0; i < SVGA3D_MAX_RENDER_TARGETS; i++) {
    uint32_t id = i < plan->shadow_update_count
                      ? plan->ids[i]
                      : SVGA3D_INVALID_ID;

    if (context->shadow.renderState.renderTargetViewIds[i] != id) {
      context->shadow.renderState.renderTargetViewIds[i] = id;
      modified = true;
    }
    if (id != SVGA3D_INVALID_ID) {
      render_target_count = i + 1u;
    }
  };
  if (modified) {
    context->renderer_dirty |= VMSVGA3D_DX_CTX_F_STATE_RENDERTARGET;
  }
  context->render_target_count = render_target_count;
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_so_targets(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10SOTargetsPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || plan == NULL || !plan->shadow_update) {
    return false;
  };
  memcpy(context->shadow.streamOut.targets, plan->shadow_targets,
         sizeof(context->shadow.streamOut.targets));
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_apply_stream_output(
    struct vmsvga_state_s *s, uint32_t cid,
    const VMSVGA3DD3D10StreamOutputSetPlan *plan) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL || plan == NULL || !plan->shadow_update) {
    return false;
  };
  context->shadow.streamOut.soid = plan->stream_output_id;
  return true;
};

static bool VMSVGA3D_DX_STATE_UNUSED vmsvga3d_state_dx_clear_rtv_references(
    struct vmsvga_state_s *s, uint32_t cid,
    SVGA3dRenderTargetViewId destroyed_id) {
  VMSVGA3DDXContext *context = vmsvga3d_dx_context(s, cid);

  if (context == NULL) {
    return false;
  };
  return vmsvga3d_d3d10_rtv_destroy_shadow_refs(
             destroyed_id, context->shadow.renderState.renderTargetViewIds) !=
         VMSVGA3D_D3D10_LEVEL_INVALID;
};

#undef VMSVGA3D_DX_STATE_UNUSED

static bool vmsvga3d_state_set_transform(struct vmsvga_state_s *s,
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

static bool vmsvga3d_state_set_z_range(struct vmsvga_state_s *s,
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

static bool vmsvga3d_state_set_render_state(
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

static bool vmsvga3d_state_set_render_target(
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

static bool vmsvga3d_state_set_texture_state(
    struct vmsvga_state_s *s, uint32_t cid, uint32_t count,
    const SVGA3dTextureState *states) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  uint32_t i;

  if (context == NULL || (count != 0 && states == NULL)) {
    return false;
  };
  for (i = 0; i < count; i++) {
    if (states[i].stage >= VMSVGA3D_MAX_SAMPLERS ||
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

static bool vmsvga3d_state_set_material(struct vmsvga_state_s *s,
                                           uint32_t cid, SVGA3dFace face,
                                           const SVGA3dMaterial *material) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || material == NULL || (uint32_t)face >= SVGA3D_FACE_MAX) {
    return false;
  };
  context->material[face].material = *material;
  context->material[face].valid = true;
  return true;
};

static bool vmsvga3d_state_set_light_data(struct vmsvga_state_s *s,
                                             uint32_t cid, uint32_t index,
                                             const SVGA3dLightData *data) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);

  if (context == NULL || data == NULL || index >= SVGA3D_NUM_LIGHTS ||
      (data->type != SVGA3D_LIGHTTYPE_POINT &&
       data->type != SVGA3D_LIGHTTYPE_SPOT1 &&
       data->type != SVGA3D_LIGHTTYPE_DIRECTIONAL)) {
    return false;
  };
  context->light[index].data = *data;
  context->light[index].data_valid = true;
  return true;
};

static bool vmsvga3d_state_set_light_enabled(struct vmsvga_state_s *s,
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

static bool vmsvga3d_state_set_viewport(struct vmsvga_state_s *s,
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

static bool vmsvga3d_state_set_clip_plane(struct vmsvga_state_s *s,
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

static bool vmsvga3d_state_clear(struct vmsvga_state_s *s, uint32_t cid,
                                    SVGA3dClearFlag clear_flags,
                                    uint32_t color, float depth,
                                    uint32_t stencil, uint32_t rect_count,
                                    const SVGA3dRect *rects) {
  VMSVGA3DContext *context;
  uint32_t type;
  uint32_t flags = clear_flags;
  bool valid = true;

  if (rect_count != 0 && rects == NULL) {
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

static bool vmsvga3d_state_draw_primitives(
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

  /* Protocol validation completes here; the FIFO handler executes D3D9. */
  return true;
};

static bool vmsvga3d_state_set_scissor(struct vmsvga_state_s *s,
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

static bool vmsvga3d_state_generate_mipmaps(struct vmsvga_state_s *s,
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
   * to D3D execution; the CPU surface store remains authoritative until that
   * path is connected.
   */
  surface->autogen_filter = filter;
  return true;
};

static bool vmsvga3d_state_query_begin(struct vmsvga_state_s *s,
                                       uint32_t cid,
                                       SVGA3dQueryType type) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  VMSVGA3DQuery *query;

  if (context == NULL || type != SVGA3D_QUERYTYPE_OCCLUSION) {
    return false;
  };
  query = &context->occlusion;
  query->defined = true;
  query->state = VMSVGA3D_QUERY_BUILDING;
  query->result = 0;
  return true;
};

static bool vmsvga3d_state_query_end(struct vmsvga_state_s *s,
                                     uint32_t cid,
                                     SVGA3dQueryType type) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  VMSVGA3DQuery *query;

  if (context == NULL || type != SVGA3D_QUERYTYPE_OCCLUSION) {
    return false;
  };
  query = &context->occlusion;
  if (!query->defined) {
    return false;
  };
  query->state = VMSVGA3D_QUERY_ISSUED;
  return true;
};

bool vmsvga3d_state_query_complete(struct vmsvga_state_s *s,
                                    uint32_t cid, SVGA3dQueryType type,
                                    uint32_t result) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  VMSVGA3DQuery *query;

  if (context == NULL || type != SVGA3D_QUERYTYPE_OCCLUSION) {
    return false;
  };
  query = &context->occlusion;
  if (!query->defined || query->state != VMSVGA3D_QUERY_ISSUED) {
    return false;
  };
  query->state = VMSVGA3D_QUERY_SIGNALED;
  query->result += result;
  return true;
};

static VMSVGA3DQueryWaitStatus vmsvga3d_state_query_wait(
    struct vmsvga_state_s *s, uint32_t cid, SVGA3dQueryType type,
    uint32_t *result) {
  VMSVGA3DContext *context = vmsvga3d_context(s, cid);
  VMSVGA3DQuery *query;

  if (result != NULL) {
    *result = 0;
  };
  if (context == NULL || type != SVGA3D_QUERYTYPE_OCCLUSION) {
    return VMSVGA3D_QUERY_WAIT_FAILED;
  };
  query = &context->occlusion;
  if (!query->defined) {
    return VMSVGA3D_QUERY_WAIT_FAILED;
  };
  if (query->state == VMSVGA3D_QUERY_ISSUED) {
    return VMSVGA3D_QUERY_WAIT_NEEDS_RENDERER;
  };
  if (result != NULL) {
    *result = query->result;
  };
  return VMSVGA3D_QUERY_WAIT_READY;
};

static bool vmsvga3d_state_shader_define(
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

static bool vmsvga3d_state_shader_destroy(struct vmsvga_state_s *s,
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

static bool vmsvga3d_state_set_shader(struct vmsvga_state_s *s,
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

static bool vmsvga3d_state_set_shader_const(
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

