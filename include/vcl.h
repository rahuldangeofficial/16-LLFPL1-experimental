#ifndef VCL_H
#define VCL_H

#include <stdint.h>

typedef struct {
    uint32_t physical_regs;
    uint32_t cache_line_size;
} HardwareProfile;

HardwareProfile vcl_discover();

#endif
