#ifndef HW_I386_VMPORT_VMVGA_H
#define HW_I386_VMPORT_VMVGA_H

typedef bool VMPortSVGACapabilityFunc(void *opaque, uint32_t type,
                                      uint32_t *value);

bool vmport_register_svga_capability_provider(
    VMPortSVGACapabilityFunc *func, void *opaque);

#endif
