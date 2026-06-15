/*
 * vec.c — Implementation of the Vector Execution Context.
 *
 * ── Design Decisions ────────────────────────────────────────────
 * 1. Opaque Encapsulation: VEC is completely hidden from callers.
 *    No one can illegally modify the dirty_mask, clock, or array.
 * 2. Perfect Cache Alignment: The virtual register array (r) is
 *    allocated using posix_memalign, bounded perfectly by the
 *    hardware's actual cache_line_size (probed by VCL). This
 *    eliminates cache tearing and false sharing.
 * 3. 16-bit Limits: The VM is statically bound to 16 virtual
 *    registers. This matches our uint16_t dirty_mask precisely.
 *    A runtime check enforces this coupling mathematically.
 */

#define _POSIX_C_SOURCE 200112L
#include "vec.h"
#include <stdlib.h>
#include <string.h>

/* ── Internal State ────────────────────────────────────────────── */

struct VEC {
    double*  r;             /* Virtual register array (aligned) */
    uint64_t clock;         /* Cycle counter for execution      */
    uint16_t reg_count;     /* Number of virtual registers      */
    uint16_t dirty_mask;    /* Bitmask of modified registers    */
};

/* ── API ───────────────────────────────────────────────────────── */

VEC* vec_init(HardwareProfile profile) {
    /* 
     * Safety Guard: The dirty_mask is uint16_t (16 bits). 
     * It is mathematically impossible to track more than 16 registers.
     */
    if (profile.virtual_regs > 16) {
        return NULL;
    }

    VEC* v = malloc(sizeof(*v));
    if (!v) {
        return NULL;
    }

    v->reg_count  = profile.virtual_regs;
    v->dirty_mask = 0;
    v->clock      = 0;
    
    /* Allocate the perfectly aligned register array */
    if (posix_memalign((void**)&v->r, profile.cache_line_size, sizeof(double) * v->reg_count) != 0) {
        free(v);
        return NULL; 
    }
    
    /* Zero-initialize the registers */
    memset(v->r, 0, sizeof(double) * v->reg_count);
    
    return v;
}

void vec_shutdown(VEC* v) {
    if (v) {
        free(v->r); /* free(NULL) is a safe no-op in C */
        free(v);
    }
}

/* ── Hardware Mutators ─────────────────────────────────────────── */

uint16_t vec_get_reg_count(VEC* v) {
    return v ? v->reg_count : 0;
}

void vec_write_register(VEC* v, uint16_t reg, double val) {
    if (!v || reg >= v->reg_count) return;
    v->r[reg] = val;
    v->dirty_mask |= (1 << reg);
}

double vec_read_register(VEC* v, uint16_t reg) {
    if (!v || reg >= v->reg_count) return 0.0;
    return v->r[reg];
}

void vec_tick_clock(VEC* v, uint64_t cycles) {
    if (v) v->clock += cycles;
}

uint64_t vec_get_clock(VEC* v) {
    return v ? v->clock : 0;
}

uint16_t vec_get_dirty_mask(VEC* v) {
    return v ? v->dirty_mask : 0;
}
