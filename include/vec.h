#ifndef VEC_H
#define VEC_H

#include "vcl.h"

typedef struct{
	double* r;
	uint32_t reg_count;
	uint16_t dirty_mask;
	uint64_t clock;
}VEC;

VEC* vec_init(HardwareProfile profile);
void vec_shutdown(VEC* v);

#endif
