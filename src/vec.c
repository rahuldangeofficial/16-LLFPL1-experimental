#define _POSIX_C_SOURCE 200112L
#include "vec.h"
#include <stdlib.h>
#include <string.h>

VEC* vec_init(HardwareProfile profile) {
    VEC* v = (VEC*)malloc(sizeof(VEC));
    if (!v) {
        return NULL;
    }

    v->reg_count = profile.physical_regs;
    
    if (posix_memalign((void**)&v->r, profile.cache_line_size, sizeof(double) * v->reg_count) != 0) {
        free(v);
        return NULL; 
    }
    
    memset(v->r, 0, sizeof(double) * v->reg_count);
    v->dirty_mask = 0;
    v->clock = 0;
    
    return v;
}

void vec_shutdown(VEC* v) {
    free(v->r);
    free(v);
}
