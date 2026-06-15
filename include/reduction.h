/*
 * reduction.h — The Primitive Execution Engine.
 *
 * Defines the mathematical operations the virtual machine natively 
 * supports. This module bypasses traditional AST execution by collapsing 
 * operations directly into the hardware-aligned VEC registers.
 */

#ifndef REDUCTION_H
#define REDUCTION_H

#include "vec.h"
#include <stdint.h>

/* ── Operation Types ───────────────────────────────────────────── */

typedef enum {
    OP_PLUS,
    OP_MINUS,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_UNKNOWN
} PrimitiveOp;

/* ── API ───────────────────────────────────────────────────────── */

PrimitiveOp reduction_resolve_op(const char* verb);

double reduction_apply(VEC* v, PrimitiveOp op, double a, double b, uint16_t target_reg);

#endif /* REDUCTION_H */
