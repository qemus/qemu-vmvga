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


#ifndef HW_DISPLAY_VMWARE_VGA_3D_SHADER_H
#define HW_DISPLAY_VMWARE_VGA_3D_SHADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vmsvga_shader_backend_e {
  VMSVGA_SHADER_BACKEND_AUTO = 0,
  VMSVGA_SHADER_BACKEND_NONE,
  VMSVGA_SHADER_BACKEND_MOJOSHADER,
  VMSVGA_SHADER_BACKEND_DXBC_SPIRV,
} VMSVGAShaderBackend;

typedef enum vmsvga_shader_stage_e {
  VMSVGA_SHADER_STAGE_INVALID = -1,
  VMSVGA_SHADER_STAGE_VERTEX = 0,
  VMSVGA_SHADER_STAGE_PIXEL,
} VMSVGAShaderStage;

typedef enum vmsvga_shader_status_e {
  VMSVGA_SHADER_OK = 0,
  VMSVGA_SHADER_UNAVAILABLE,
  VMSVGA_SHADER_INVALID_ARGUMENT,
  VMSVGA_SHADER_NO_MEMORY,
  VMSVGA_SHADER_UNSUPPORTED,
  VMSVGA_SHADER_TRANSLATION_FAILED,
  VMSVGA_SHADER_LINK_FAILED,
} VMSVGAShaderStatus;

typedef enum vmsvga_shader_resource_type_e {
  VMSVGA_SHADER_RESOURCE_CONSTANT_BLOCK = 0,
  VMSVGA_SHADER_RESOURCE_COMBINED_IMAGE_SAMPLER,
  VMSVGA_SHADER_RESOURCE_SAMPLED_IMAGE,
  VMSVGA_SHADER_RESOURCE_SAMPLER,
} VMSVGAShaderResourceType;

typedef enum vmsvga_shader_sampler_type_e {
  VMSVGA_SHADER_SAMPLER_UNKNOWN = 0,
  VMSVGA_SHADER_SAMPLER_2D,
  VMSVGA_SHADER_SAMPLER_CUBE,
  VMSVGA_SHADER_SAMPLER_VOLUME,
} VMSVGAShaderSamplerType;

typedef enum vmsvga_shader_semantic_e {
  VMSVGA_SHADER_SEMANTIC_POSITION = 0,
  VMSVGA_SHADER_SEMANTIC_BLEND_WEIGHT,
  VMSVGA_SHADER_SEMANTIC_BLEND_INDICES,
  VMSVGA_SHADER_SEMANTIC_NORMAL,
  VMSVGA_SHADER_SEMANTIC_POINT_SIZE,
  VMSVGA_SHADER_SEMANTIC_TEXCOORD,
  VMSVGA_SHADER_SEMANTIC_TANGENT,
  VMSVGA_SHADER_SEMANTIC_BINORMAL,
  VMSVGA_SHADER_SEMANTIC_TESS_FACTOR,
  VMSVGA_SHADER_SEMANTIC_POSITIONT,
  VMSVGA_SHADER_SEMANTIC_COLOR,
  VMSVGA_SHADER_SEMANTIC_FOG,
  VMSVGA_SHADER_SEMANTIC_DEPTH,
  VMSVGA_SHADER_SEMANTIC_SAMPLE,
  VMSVGA_SHADER_SEMANTIC_MAX,
} VMSVGAShaderSemantic;

typedef enum vmsvga_shader_vertex_format_e {
  VMSVGA_SHADER_VERTEX_FORMAT_FLOAT1 = 0,
  VMSVGA_SHADER_VERTEX_FORMAT_FLOAT2,
  VMSVGA_SHADER_VERTEX_FORMAT_FLOAT3,
  VMSVGA_SHADER_VERTEX_FORMAT_FLOAT4,
  VMSVGA_SHADER_VERTEX_FORMAT_D3DCOLOR,
  VMSVGA_SHADER_VERTEX_FORMAT_UBYTE4,
  VMSVGA_SHADER_VERTEX_FORMAT_SHORT2,
  VMSVGA_SHADER_VERTEX_FORMAT_SHORT4,
  VMSVGA_SHADER_VERTEX_FORMAT_UBYTE4N,
  VMSVGA_SHADER_VERTEX_FORMAT_SHORT2N,
  VMSVGA_SHADER_VERTEX_FORMAT_SHORT4N,
  VMSVGA_SHADER_VERTEX_FORMAT_USHORT2N,
  VMSVGA_SHADER_VERTEX_FORMAT_USHORT4N,
  VMSVGA_SHADER_VERTEX_FORMAT_UDEC3,
  VMSVGA_SHADER_VERTEX_FORMAT_DEC3N,
  VMSVGA_SHADER_VERTEX_FORMAT_FLOAT16_2,
  VMSVGA_SHADER_VERTEX_FORMAT_FLOAT16_4,
  VMSVGA_SHADER_VERTEX_FORMAT_MAX,
} VMSVGAShaderVertexFormat;

typedef struct vmsvga_shader_resource_s {
  VMSVGAShaderResourceType type;
  VMSVGAShaderSamplerType sampler_type;
  uint32_t guest_slot;
  uint32_t descriptor_set;
  uint32_t binding;
  uint32_t count;
} VMSVGAShaderResource;

typedef struct vmsvga_shader_reflection_s {
  uint32_t input_semantic_mask[VMSVGA_SHADER_SEMANTIC_MAX];
  uint32_t output_semantic_mask[VMSVGA_SHADER_SEMANTIC_MAX];
  uint32_t sampler_mask;
  uint32_t float_constant_mask[8];
  uint32_t int_constant_mask;
  uint32_t bool_constant_mask;
} VMSVGAShaderReflection;

typedef struct vmsvga_shader_translate_request_s {
  VMSVGAShaderStage stage;
  const uint32_t *bytecode;
  size_t bytecode_size;
} VMSVGAShaderTranslateRequest;

typedef struct vmsvga_shader_vertex_input_s {
  VMSVGAShaderSemantic semantic;
  uint32_t semantic_index;
  VMSVGAShaderVertexFormat format;
} VMSVGAShaderVertexInput;

typedef struct vmsvga_shader_binary_s {
  uint32_t *spirv;
  size_t spirv_word_count;
  VMSVGAShaderReflection reflection;
  VMSVGAShaderResource *resources;
  size_t resource_count;
} VMSVGAShaderBinary;

typedef struct vmsvga_shader_program_s {
  VMSVGAShaderBinary vertex;
  VMSVGAShaderBinary pixel;
} VMSVGAShaderProgram;

/*
 * A translation is backend-private and immutable after creation. It can be
 * linked repeatedly with different shader pairs and vertex declarations.
 */
typedef struct vmsvga_shader_translation_s VMSVGAShaderTranslation;

/*
 * Link/finalize a translated vertex/pixel pair. The returned program contains
 * final SPIR-V only; backend-private patch/link data must not be exposed here.
 */
typedef struct vmsvga_shader_link_request_s {
  const VMSVGAShaderTranslation *vertex;
  const VMSVGAShaderTranslation *pixel;
  const VMSVGAShaderVertexInput *vertex_inputs;
  size_t vertex_input_count;
} VMSVGAShaderLinkRequest;

typedef struct vmsvga_shader_backend_ops_s {
  VMSVGAShaderBackend backend;
  const char *name;
  bool available;
  VMSVGAShaderStatus (*create)(void **backend_data);
  void (*destroy)(void *backend_data);
  VMSVGAShaderStatus (*translate)(void *backend_data,
                                  const VMSVGAShaderTranslateRequest *request,
                                  void **translation_data);
  void (*free_translation)(void *translation_data);
  VMSVGAShaderStatus (*link)(void *backend_data,
                             const void *vertex_translation,
                             const void *pixel_translation,
                             const VMSVGAShaderVertexInput *vertex_inputs,
                             size_t vertex_input_count,
                             VMSVGAShaderProgram *program);
} VMSVGAShaderBackendOps;

typedef struct vmsvga_shader_compiler_s VMSVGAShaderCompiler;

#ifdef CONFIG_VMWARE_VGA_MOJOSHADER
extern const VMSVGAShaderBackendOps vmsvga_shader_mojoshader_ops;
#endif

#ifdef CONFIG_VMWARE_VGA_DXBC_SPIRV
extern const VMSVGAShaderBackendOps vmsvga_shader_dxbc_spirv_ops;
#endif

const char *vmsvga_shader_backend_name(VMSVGAShaderBackend backend);
bool vmsvga_shader_backend_available(VMSVGAShaderBackend backend);
VMSVGAShaderBackend vmsvga_shader_backend_resolve(VMSVGAShaderBackend backend);

VMSVGAShaderCompiler *vmsvga_shader_compiler_new(VMSVGAShaderBackend backend);
void vmsvga_shader_compiler_free(VMSVGAShaderCompiler *compiler);
VMSVGAShaderBackend
vmsvga_shader_compiler_backend(const VMSVGAShaderCompiler *compiler);

VMSVGAShaderStatus
vmsvga_shader_translate(VMSVGAShaderCompiler *compiler,
                        const VMSVGAShaderTranslateRequest *request,
                        VMSVGAShaderTranslation **translation);
VMSVGAShaderStage
vmsvga_shader_translation_stage(const VMSVGAShaderTranslation *translation);
void vmsvga_shader_translation_free(VMSVGAShaderTranslation *translation);

VMSVGAShaderStatus
vmsvga_shader_link(VMSVGAShaderCompiler *compiler,
                   const VMSVGAShaderLinkRequest *request,
                   VMSVGAShaderProgram *program);

void vmsvga_shader_binary_reset(VMSVGAShaderBinary *binary);
void vmsvga_shader_program_reset(VMSVGAShaderProgram *program);
const char *vmsvga_shader_status_name(VMSVGAShaderStatus status);

#ifdef __cplusplus
}
#endif

#endif
