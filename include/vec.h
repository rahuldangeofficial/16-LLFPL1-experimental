/*
 * vec.h — Public interface for the Vector Execution Context (VEC).
 *
 * VEC is the execution engine of the virtual machine. It holds the
 * runtime state, virtual registers, and clock counter.
 * The struct is perfectly encapsulated as an opaque pointer.
 */

#ifndef VEC_H
#define VEC_H

#include "vcl.h"

/* ── Opaque handle ─────────────────────────────────────────────── */

typedef struct VEC VEC;

/* ── API ───────────────────────────────────────────────────────── */

VEC* vec_init    (HardwareProfile profile);
void vec_shutdown(VEC* v);

/* ── Hardware Mutators ─────────────────────────────────────────── */

uint16_t vec_get_reg_count (VEC* v);
void     vec_write_register(VEC* v, uint16_t reg, double val);
double   vec_read_register (VEC* v, uint16_t reg);
void     vec_tick_clock    (VEC* v, uint64_t cycles);
uint64_t vec_get_clock     (VEC* v);
uint16_t vec_get_dirty_mask(VEC* v);

#endif /* VEC_H */
