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

#include <limits.h>

#define MOJOSHADER_NO_VERSION_INCLUDE 1
#define SUPPORT_PROFILE_D3D 0
#define SUPPORT_PROFILE_BYTECODE 0
#define SUPPORT_PROFILE_HLSL 0
#define SUPPORT_PROFILE_GLSL 0
#define SUPPORT_PROFILE_GLSL120 0
#define SUPPORT_PROFILE_GLSLES 0
#define SUPPORT_PROFILE_GLSLES3 0
#define SUPPORT_PROFILE_ARB1 0
#define SUPPORT_PROFILE_ARB1_NV 0
#define SUPPORT_PROFILE_METAL 0
#define SUPPORT_PROFILE_SPIRV 1
#define SUPPORT_PROFILE_GLSPIRV 0

/*
 * Keep this order. mojoshader.c supplies declarations used by later profile
 * sources when all four files are compiled into vmware_vga.c as one unit.
 */
#include "mojoshader/mojoshader.c"
#include "mojoshader/mojoshader_common.c"
#include "mojoshader/profiles/mojoshader_profile_common.c"
#include "mojoshader/profiles/mojoshader_profile_spirv.c"

typedef struct vmsvga_shader_mojo_translation_s {
  const MOJOSHADER_parseData *parse;
} VMSVGAShaderMojoTranslation;

/* MojoShader treats a NULL result from a zero-byte allocation as OOM. */
static char vmsvga_shader_mojo_zero_alloc;

static void *vmsvga_shader_mojo_malloc(int bytes, void *data) {
  (void)data;
  if (bytes < 0) {
    return NULL;
  };
  if (bytes == 0) {
    return &vmsvga_shader_mojo_zero_alloc;
  };
  return g_try_malloc((size_t)bytes);
};

static void vmsvga_shader_mojo_free(void *ptr, void *data) {
  (void)data;
  if (ptr != &vmsvga_shader_mojo_zero_alloc) {
    g_free(ptr);
  };
};

static VMSVGAShaderStatus vmsvga_shader_mojo_create(void **backend_data) {
  if (backend_data == NULL) {
    return VMSVGA_SHADER_INVALID_ARGUMENT;
  };
  *backend_data = NULL;
  return VMSVGA_SHADER_OK;
};

static void vmsvga_shader_mojo_destroy(void *backend_data) {
  (void)backend_data;
};

static MOJOSHADER_shaderType
vmsvga_shader_mojo_expected_type(VMSVGAShaderStage stage) {
  switch (stage) {
  case VMSVGA_SHADER_STAGE_VERTEX:
    return MOJOSHADER_TYPE_VERTEX;
  case VMSVGA_SHADER_STAGE_PIXEL:
    return MOJOSHADER_TYPE_PIXEL;
  default:
    return MOJOSHADER_TYPE_UNKNOWN;
  };
};

static VMSVGAShaderStatus
vmsvga_shader_mojo_translate(void *backend_data,
                             const VMSVGAShaderTranslateRequest *request,
                             void **translation_data) {
  VMSVGAShaderMojoTranslation *translation;
  const MOJOSHADER_parseData *parse;
  MOJOSHADER_shaderType expected;

  (void)backend_data;
  if (request == NULL || translation_data == NULL || request->bytecode == NULL ||
      request->bytecode_size == 0 || request->bytecode_size > UINT_MAX) {
    return VMSVGA_SHADER_INVALID_ARGUMENT;
  };
  *translation_data = NULL;
  expected = vmsvga_shader_mojo_expected_type(request->stage);
  if (expected == MOJOSHADER_TYPE_UNKNOWN) {
    return VMSVGA_SHADER_INVALID_ARGUMENT;
  };

  parse = MOJOSHADER_parse(MOJOSHADER_PROFILE_SPIRV, NULL,
                           (const unsigned char *)request->bytecode,
                           (unsigned int)request->bytecode_size,
                           NULL, 0, NULL, 0,
                           vmsvga_shader_mojo_malloc,
                           vmsvga_shader_mojo_free, NULL);
  if (parse == NULL || parse->error_count != 0 || parse->output == NULL ||
      parse->output_len <= 0 || parse->shader_type != expected) {
    MOJOSHADER_freeParseData(parse);
    return VMSVGA_SHADER_TRANSLATION_FAILED;
  };

  translation = g_try_new0(VMSVGAShaderMojoTranslation, 1);
  if (translation == NULL) {
    MOJOSHADER_freeParseData(parse);
    return VMSVGA_SHADER_NO_MEMORY;
  };
  translation->parse = parse;
  *translation_data = translation;
  return VMSVGA_SHADER_OK;
};

static void vmsvga_shader_mojo_free_translation(void *translation_data) {
  VMSVGAShaderMojoTranslation *translation = translation_data;

  if (translation == NULL) {
    return;
  };
  MOJOSHADER_freeParseData(translation->parse);
  g_free(translation);
};

static bool
vmsvga_shader_mojo_usage(VMSVGAShaderSemantic semantic,
                         MOJOSHADER_usage *usage) {
  static const MOJOSHADER_usage usage_map[VMSVGA_SHADER_SEMANTIC_MAX] = {
      [VMSVGA_SHADER_SEMANTIC_POSITION] = MOJOSHADER_USAGE_POSITION,
      [VMSVGA_SHADER_SEMANTIC_BLEND_WEIGHT] = MOJOSHADER_USAGE_BLENDWEIGHT,
      [VMSVGA_SHADER_SEMANTIC_BLEND_INDICES] = MOJOSHADER_USAGE_BLENDINDICES,
      [VMSVGA_SHADER_SEMANTIC_NORMAL] = MOJOSHADER_USAGE_NORMAL,
      [VMSVGA_SHADER_SEMANTIC_POINT_SIZE] = MOJOSHADER_USAGE_POINTSIZE,
      [VMSVGA_SHADER_SEMANTIC_TEXCOORD] = MOJOSHADER_USAGE_TEXCOORD,
      [VMSVGA_SHADER_SEMANTIC_TANGENT] = MOJOSHADER_USAGE_TANGENT,
      [VMSVGA_SHADER_SEMANTIC_BINORMAL] = MOJOSHADER_USAGE_BINORMAL,
      [VMSVGA_SHADER_SEMANTIC_TESS_FACTOR] = MOJOSHADER_USAGE_TESSFACTOR,
      [VMSVGA_SHADER_SEMANTIC_POSITIONT] = MOJOSHADER_USAGE_POSITIONT,
      [VMSVGA_SHADER_SEMANTIC_COLOR] = MOJOSHADER_USAGE_COLOR,
      [VMSVGA_SHADER_SEMANTIC_FOG] = MOJOSHADER_USAGE_FOG,
      [VMSVGA_SHADER_SEMANTIC_DEPTH] = MOJOSHADER_USAGE_DEPTH,
      [VMSVGA_SHADER_SEMANTIC_SAMPLE] = MOJOSHADER_USAGE_SAMPLE,
  };

  if (usage == NULL || semantic >= VMSVGA_SHADER_SEMANTIC_MAX) {
    return false;
  };
  *usage = usage_map[semantic];
  return true;
};

static bool
vmsvga_shader_mojo_vertex_format(VMSVGAShaderVertexFormat format,
                                 MOJOSHADER_vertexElementFormat *mojo_format) {
  if (mojo_format == NULL) {
    return false;
  };
  switch (format) {
  case VMSVGA_SHADER_VERTEX_FORMAT_FLOAT1:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_SINGLE;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_FLOAT2:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_VECTOR2;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_FLOAT3:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_VECTOR3;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_FLOAT4:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_VECTOR4;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_D3DCOLOR:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_COLOR;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_UBYTE4:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_BYTE4;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_SHORT2:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_SHORT2;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_SHORT4:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_SHORT4;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_SHORT2N:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_NORMALIZEDSHORT2;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_SHORT4N:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_NORMALIZEDSHORT4;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_FLOAT16_2:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_HALFVECTOR2;
    return true;
  case VMSVGA_SHADER_VERTEX_FORMAT_FLOAT16_4:
    *mojo_format = MOJOSHADER_VERTEXELEMENTFORMAT_HALFVECTOR4;
    return true;
  default:
    return false;
  };
};

static VMSVGAShaderSemantic
vmsvga_shader_mojo_semantic(MOJOSHADER_usage usage) {
  switch (usage) {
  case MOJOSHADER_USAGE_POSITION:
    return VMSVGA_SHADER_SEMANTIC_POSITION;
  case MOJOSHADER_USAGE_BLENDWEIGHT:
    return VMSVGA_SHADER_SEMANTIC_BLEND_WEIGHT;
  case MOJOSHADER_USAGE_BLENDINDICES:
    return VMSVGA_SHADER_SEMANTIC_BLEND_INDICES;
  case MOJOSHADER_USAGE_NORMAL:
    return VMSVGA_SHADER_SEMANTIC_NORMAL;
  case MOJOSHADER_USAGE_POINTSIZE:
    return VMSVGA_SHADER_SEMANTIC_POINT_SIZE;
  case MOJOSHADER_USAGE_TEXCOORD:
    return VMSVGA_SHADER_SEMANTIC_TEXCOORD;
  case MOJOSHADER_USAGE_TANGENT:
    return VMSVGA_SHADER_SEMANTIC_TANGENT;
  case MOJOSHADER_USAGE_BINORMAL:
    return VMSVGA_SHADER_SEMANTIC_BINORMAL;
  case MOJOSHADER_USAGE_TESSFACTOR:
    return VMSVGA_SHADER_SEMANTIC_TESS_FACTOR;
  case MOJOSHADER_USAGE_POSITIONT:
    return VMSVGA_SHADER_SEMANTIC_POSITIONT;
  case MOJOSHADER_USAGE_COLOR:
    return VMSVGA_SHADER_SEMANTIC_COLOR;
  case MOJOSHADER_USAGE_FOG:
    return VMSVGA_SHADER_SEMANTIC_FOG;
  case MOJOSHADER_USAGE_DEPTH:
    return VMSVGA_SHADER_SEMANTIC_DEPTH;
  case MOJOSHADER_USAGE_SAMPLE:
    return VMSVGA_SHADER_SEMANTIC_SAMPLE;
  default:
    return VMSVGA_SHADER_SEMANTIC_MAX;
  };
};

static VMSVGAShaderSamplerType
vmsvga_shader_mojo_sampler_type(MOJOSHADER_samplerType type) {
  switch (type) {
  case MOJOSHADER_SAMPLER_2D:
    return VMSVGA_SHADER_SAMPLER_2D;
  case MOJOSHADER_SAMPLER_CUBE:
    return VMSVGA_SHADER_SAMPLER_CUBE;
  case MOJOSHADER_SAMPLER_VOLUME:
    return VMSVGA_SHADER_SAMPLER_VOLUME;
  default:
    return VMSVGA_SHADER_SAMPLER_UNKNOWN;
  };
};

static void vmsvga_shader_mojo_mask_range(uint32_t *mask, size_t mask_words,
                                          uint32_t first, uint32_t count) {
  uint32_t i;

  for (i = 0; i < count; i++) {
    uint32_t index = first + i;
    uint32_t word = index / 32;
    uint32_t bit = index % 32;

    if (word >= mask_words) {
      break;
    };
    mask[word] |= UINT32_C(1) << bit;
  };
};

static bool
vmsvga_shader_mojo_fill_reflection(const MOJOSHADER_parseData *parse,
                                   VMSVGAShaderStage stage,
                                   VMSVGAShaderBinary *binary) {
  VMSVGAShaderResource *resources = NULL;
  uint32_t sampler_set;
  uint32_t uniform_set;
  size_t resource_count = 0;
  bool has_uniforms = false;
  int i;

  if (parse == NULL || binary == NULL) {
    return false;
  };
  sampler_set = stage == VMSVGA_SHADER_STAGE_VERTEX ? 0 : 2;
  uniform_set = stage == VMSVGA_SHADER_STAGE_VERTEX ? 1 : 3;

  for (i = 0; i < parse->uniform_count; i++) {
    const MOJOSHADER_uniform *uniform = &parse->uniforms[i];
    uint32_t count = uniform->array_count > 0 ? (uint32_t)uniform->array_count : 1;

    if (uniform->index < 0) {
      continue;
    };
    switch (uniform->type) {
    case MOJOSHADER_UNIFORM_FLOAT:
      vmsvga_shader_mojo_mask_range(binary->reflection.float_constant_mask, 8,
                                    (uint32_t)uniform->index, count);
      has_uniforms = true;
      break;
    case MOJOSHADER_UNIFORM_INT:
      vmsvga_shader_mojo_mask_range(&binary->reflection.int_constant_mask, 1,
                                    (uint32_t)uniform->index, count);
      has_uniforms = true;
      break;
    case MOJOSHADER_UNIFORM_BOOL:
      vmsvga_shader_mojo_mask_range(&binary->reflection.bool_constant_mask, 1,
                                    (uint32_t)uniform->index, count);
      has_uniforms = true;
      break;
    default:
      break;
    };
  };

  for (i = 0; i < parse->attribute_count; i++) {
    VMSVGAShaderSemantic semantic =
        vmsvga_shader_mojo_semantic(parse->attributes[i].usage);
    int index = parse->attributes[i].index;

    if (semantic < VMSVGA_SHADER_SEMANTIC_MAX && index >= 0 && index < 32) {
      binary->reflection.input_semantic_mask[semantic] |= UINT32_C(1) << index;
    };
  };
  for (i = 0; i < parse->output_count; i++) {
    VMSVGAShaderSemantic semantic =
        vmsvga_shader_mojo_semantic(parse->outputs[i].usage);
    int index = parse->outputs[i].index;

    if (semantic < VMSVGA_SHADER_SEMANTIC_MAX && index >= 0 && index < 32) {
      binary->reflection.output_semantic_mask[semantic] |= UINT32_C(1) << index;
    };
  };

  for (i = 0; i < parse->sampler_count; i++) {
    if (parse->samplers[i].index >= 0 && parse->samplers[i].index < 32) {
      binary->reflection.sampler_mask |=
          UINT32_C(1) << (uint32_t)parse->samplers[i].index;
      resource_count++;
    };
  };
  if (has_uniforms) {
    resource_count++;
  };

  if (resource_count != 0) {
    resources = g_try_new0(VMSVGAShaderResource, resource_count);
    if (resources == NULL) {
      return false;
    };
  };
  resource_count = 0;
  if (has_uniforms) {
    resources[resource_count++] = (VMSVGAShaderResource){
        .type = VMSVGA_SHADER_RESOURCE_CONSTANT_BLOCK,
        .sampler_type = VMSVGA_SHADER_SAMPLER_UNKNOWN,
        .guest_slot = 0,
        .descriptor_set = uniform_set,
        .binding = 0,
        .count = 1,
    };
  };
  for (i = 0; i < parse->sampler_count; i++) {
    const MOJOSHADER_sampler *sampler = &parse->samplers[i];

    if (sampler->index < 0 || sampler->index >= 32) {
      continue;
    };
    resources[resource_count++] = (VMSVGAShaderResource){
        .type = VMSVGA_SHADER_RESOURCE_COMBINED_IMAGE_SAMPLER,
        .sampler_type = vmsvga_shader_mojo_sampler_type(sampler->type),
        .guest_slot = (uint32_t)sampler->index,
        .descriptor_set = sampler_set,
        .binding = (uint32_t)sampler->index,
        .count = 1,
    };
  };
  binary->resources = resources;
  binary->resource_count = resource_count;
  return true;
};

static bool
vmsvga_shader_mojo_copy_final(const MOJOSHADER_parseData *parse,
                              int patch_size, VMSVGAShaderStage stage,
                              VMSVGAShaderBinary *binary) {
  size_t final_size;

  if (parse == NULL || binary == NULL || patch_size <= 0 ||
      patch_size >= parse->output_len) {
    return false;
  };
  final_size = (size_t)parse->output_len - (size_t)patch_size;
  if (final_size == 0 || final_size % sizeof(uint32_t) != 0) {
    return false;
  };
  binary->spirv = g_try_malloc(final_size);
  if (binary->spirv == NULL) {
    return false;
  };
  memcpy(binary->spirv, parse->output, final_size);
  binary->spirv_word_count = final_size / sizeof(uint32_t);
  if (binary->spirv_word_count == 0 || binary->spirv[0] != UINT32_C(0x07230203) ||
      !vmsvga_shader_mojo_fill_reflection(parse, stage, binary)) {
    vmsvga_shader_binary_reset(binary);
    return false;
  };
  return true;
};

static VMSVGAShaderStatus
vmsvga_shader_mojo_link(void *backend_data, const void *vertex_translation,
                        const void *pixel_translation,
                        const VMSVGAShaderVertexInput *vertex_inputs,
                        size_t vertex_input_count,
                        VMSVGAShaderProgram *program) {
  const VMSVGAShaderMojoTranslation *vertex = vertex_translation;
  const VMSVGAShaderMojoTranslation *pixel = pixel_translation;
  MOJOSHADER_parseData vertex_copy;
  MOJOSHADER_parseData pixel_copy;
  MOJOSHADER_vertexAttribute attributes[SVGA3D_MAX_VERTEX_ARRAYS];
  int patch_size;
  size_t i;

  (void)backend_data;
  if (vertex == NULL || pixel == NULL || vertex->parse == NULL ||
      pixel->parse == NULL || program == NULL ||
      vertex_input_count > SVGA3D_MAX_VERTEX_ARRAYS ||
      (vertex_input_count != 0 && vertex_inputs == NULL)) {
    return VMSVGA_SHADER_INVALID_ARGUMENT;
  };

  for (i = 0; i < vertex_input_count; i++) {
    MOJOSHADER_usage usage;
    MOJOSHADER_vertexElementFormat format;

    if (!vmsvga_shader_mojo_usage(vertex_inputs[i].semantic, &usage) ||
        !vmsvga_shader_mojo_vertex_format(vertex_inputs[i].format, &format) ||
        vertex_inputs[i].semantic_index > INT_MAX) {
      return VMSVGA_SHADER_UNSUPPORTED;
    };
    attributes[i].usage = usage;
    attributes[i].usageIndex = (int)vertex_inputs[i].semantic_index;
    attributes[i].vertexElementFormat = format;
  };

  vertex_copy = *vertex->parse;
  pixel_copy = *pixel->parse;
  vertex_copy.output = g_try_malloc((size_t)vertex->parse->output_len);
  pixel_copy.output = g_try_malloc((size_t)pixel->parse->output_len);
  if (vertex_copy.output == NULL || pixel_copy.output == NULL) {
    g_free((void *)vertex_copy.output);
    g_free((void *)pixel_copy.output);
    return VMSVGA_SHADER_NO_MEMORY;
  };
  memcpy((void *)vertex_copy.output, vertex->parse->output,
         (size_t)vertex->parse->output_len);
  memcpy((void *)pixel_copy.output, pixel->parse->output,
         (size_t)pixel->parse->output_len);

  patch_size = MOJOSHADER_linkSPIRVShaders(&vertex_copy, &pixel_copy,
                                           attributes,
                                           (int)vertex_input_count);
  if (patch_size <= 0 ||
      !vmsvga_shader_mojo_copy_final(&vertex_copy, patch_size,
                                     VMSVGA_SHADER_STAGE_VERTEX,
                                     &program->vertex) ||
      !vmsvga_shader_mojo_copy_final(&pixel_copy, patch_size,
                                     VMSVGA_SHADER_STAGE_PIXEL,
                                     &program->pixel)) {
    g_free((void *)vertex_copy.output);
    g_free((void *)pixel_copy.output);
    vmsvga_shader_program_reset(program);
    return VMSVGA_SHADER_LINK_FAILED;
  };

  g_free((void *)vertex_copy.output);
  g_free((void *)pixel_copy.output);
  return VMSVGA_SHADER_OK;
};

const VMSVGAShaderBackendOps vmsvga_shader_mojoshader_ops = {
    .backend = VMSVGA_SHADER_BACKEND_MOJOSHADER,
    .name = "mojoshader",
    .available = true,
    .create = vmsvga_shader_mojo_create,
    .destroy = vmsvga_shader_mojo_destroy,
    .translate = vmsvga_shader_mojo_translate,
    .free_translation = vmsvga_shader_mojo_free_translation,
    .link = vmsvga_shader_mojo_link,
};
