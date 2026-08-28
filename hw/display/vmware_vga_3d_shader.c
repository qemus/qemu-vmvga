/*

 QEMU VMware Super Video Graphics Array 2 [SVGA-II]

 Copyright (c) 2023-2026 Christopher Eric Lentocha
 <christopherericlentocha@gmail.com>

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

#include "include/vmware_vga_3d_shader.h"

struct vmsvga_shader_compiler_s {
  const VMSVGAShaderBackendOps *ops;
  void *backend_data;
};

static VMSVGAShaderStatus vmsvga_shader_none_create(void **backend_data) {
  *backend_data = NULL;
  return VMSVGA_SHADER_OK;
};

static void vmsvga_shader_none_destroy(void *backend_data) {
  (void)backend_data;
};

static VMSVGAShaderStatus
vmsvga_shader_none_compile(void *backend_data,
                           const VMSVGAShaderCompileRequest *request,
                           VMSVGAShaderBinary *binary) {
  (void)backend_data;
  (void)request;
  (void)binary;
  return VMSVGA_SHADER_UNAVAILABLE;
};

static const VMSVGAShaderBackendOps vmsvga_shader_none_ops = {
    .backend = VMSVGA_SHADER_BACKEND_NONE,
    .name = "none",
    .available = false,
    .create = vmsvga_shader_none_create,
    .destroy = vmsvga_shader_none_destroy,
    .compile = vmsvga_shader_none_compile,
};

static const VMSVGAShaderBackendOps *
vmsvga_shader_backend_ops(VMSVGAShaderBackend backend) {
  switch (backend) {
  case VMSVGA_SHADER_BACKEND_NONE:
    return &vmsvga_shader_none_ops;
#ifdef CONFIG_VMWARE_VGA_MOJOSHADER
  case VMSVGA_SHADER_BACKEND_MOJOSHADER:
    return &vmsvga_shader_mojoshader_ops;
#endif
#ifdef CONFIG_VMWARE_VGA_DXBC_SPIRV
  case VMSVGA_SHADER_BACKEND_DXBC_SPIRV:
    return &vmsvga_shader_dxbc_spirv_ops;
#endif
  default:
    return NULL;
  };
};

VMSVGAShaderBackend vmsvga_shader_backend_resolve(VMSVGAShaderBackend backend) {
  const VMSVGAShaderBackendOps *ops;

  if (backend == VMSVGA_SHADER_BACKEND_AUTO) {
#ifdef CONFIG_VMWARE_VGA_MOJOSHADER
    if (vmsvga_shader_backend_available(VMSVGA_SHADER_BACKEND_MOJOSHADER)) {
      return VMSVGA_SHADER_BACKEND_MOJOSHADER;
    };
#endif
#ifdef CONFIG_VMWARE_VGA_DXBC_SPIRV
    if (vmsvga_shader_backend_available(VMSVGA_SHADER_BACKEND_DXBC_SPIRV)) {
      return VMSVGA_SHADER_BACKEND_DXBC_SPIRV;
    };
#endif
    return VMSVGA_SHADER_BACKEND_NONE;
  };

  ops = vmsvga_shader_backend_ops(backend);
  if (ops == NULL || !ops->available) {
    return VMSVGA_SHADER_BACKEND_NONE;
  };
  return backend;
};

const char *vmsvga_shader_backend_name(VMSVGAShaderBackend backend) {
  const VMSVGAShaderBackendOps *ops;

  switch (backend) {
  case VMSVGA_SHADER_BACKEND_AUTO:
    return "auto";
  case VMSVGA_SHADER_BACKEND_MOJOSHADER:
    return "mojoshader";
  case VMSVGA_SHADER_BACKEND_DXBC_SPIRV:
    return "dxbc-spirv";
  default:
    break;
  };

  ops = vmsvga_shader_backend_ops(backend);
  return ops != NULL ? ops->name : "unknown";
};

bool vmsvga_shader_backend_available(VMSVGAShaderBackend backend) {
  const VMSVGAShaderBackendOps *ops;

  if (backend == VMSVGA_SHADER_BACKEND_AUTO) {
    return vmsvga_shader_backend_resolve(backend) != VMSVGA_SHADER_BACKEND_NONE;
  };
  ops = vmsvga_shader_backend_ops(backend);
  return ops != NULL && ops->available;
};

VMSVGAShaderCompiler *vmsvga_shader_compiler_new(VMSVGAShaderBackend backend) {
  VMSVGAShaderCompiler *compiler;
  const VMSVGAShaderBackendOps *ops;
  VMSVGAShaderBackend resolved;

  resolved = vmsvga_shader_backend_resolve(backend);
  if (backend != VMSVGA_SHADER_BACKEND_AUTO &&
      backend != VMSVGA_SHADER_BACKEND_NONE &&
      resolved == VMSVGA_SHADER_BACKEND_NONE) {
    return NULL;
  };

  ops = vmsvga_shader_backend_ops(resolved);
  if (ops == NULL) {
    return NULL;
  };

  compiler = g_try_new0(VMSVGAShaderCompiler, 1);
  if (compiler == NULL) {
    return NULL;
  };
  compiler->ops = ops;
  if (ops->create(&compiler->backend_data) != VMSVGA_SHADER_OK) {
    g_free(compiler);
    return NULL;
  };
  return compiler;
};

void vmsvga_shader_compiler_free(VMSVGAShaderCompiler *compiler) {
  if (compiler == NULL) {
    return;
  };
  compiler->ops->destroy(compiler->backend_data);
  g_free(compiler);
};

VMSVGAShaderBackend
vmsvga_shader_compiler_backend(const VMSVGAShaderCompiler *compiler) {
  if (compiler == NULL || compiler->ops == NULL) {
    return VMSVGA_SHADER_BACKEND_NONE;
  };
  return compiler->ops->backend;
};

void vmsvga_shader_binary_reset(VMSVGAShaderBinary *binary) {
  if (binary == NULL) {
    return;
  };
  g_free(binary->spirv);
  g_free(binary->resources);
  memset(binary, 0, sizeof(*binary));
};

VMSVGAShaderStatus
vmsvga_shader_compile(VMSVGAShaderCompiler *compiler,
                      const VMSVGAShaderCompileRequest *request,
                      VMSVGAShaderBinary *binary) {
  VMSVGAShaderStatus status;

  if (binary == NULL) {
    return VMSVGA_SHADER_INVALID_ARGUMENT;
  };
  vmsvga_shader_binary_reset(binary);

  if (compiler == NULL || compiler->ops == NULL || request == NULL ||
      request->bytecode == NULL || request->bytecode_size == 0 ||
      request->bytecode_size % sizeof(uint32_t) != 0 ||
      (request->stage != VMSVGA_SHADER_STAGE_VERTEX &&
       request->stage != VMSVGA_SHADER_STAGE_PIXEL)) {
    return VMSVGA_SHADER_INVALID_ARGUMENT;
  };

  status = compiler->ops->compile(compiler->backend_data, request, binary);
  if (status != VMSVGA_SHADER_OK) {
    vmsvga_shader_binary_reset(binary);
  };
  return status;
};

const char *vmsvga_shader_status_name(VMSVGAShaderStatus status) {
  switch (status) {
  case VMSVGA_SHADER_OK:
    return "ok";
  case VMSVGA_SHADER_UNAVAILABLE:
    return "unavailable";
  case VMSVGA_SHADER_INVALID_ARGUMENT:
    return "invalid argument";
  case VMSVGA_SHADER_NO_MEMORY:
    return "out of memory";
  case VMSVGA_SHADER_TRANSLATION_FAILED:
    return "translation failed";
  default:
    return "unknown";
  };
};
