/*
 * reduction.c — Implementation of the Primitive Execution Engine.
 *
 * ── Design Decisions ────────────────────────────────────────────
 * 1. Safe Hardware Mutators: Bypasses direct access to VEC internals,
 *    relying strictly on safe mutator APIs to manage dirty_mask and bounds.
 * 2. Hardware-Safe Division: Prevents OS-level SIGFPE interrupts by 
 *    safeguarding division-by-zero at the VM layer.
 * 3. Thermodynamic Cost: Automatically tracks execution cycle limits 
 *    using vec_tick_clock.
 */

#include "reduction.h"
#include <string.h>

/* ── API ───────────────────────────────────────────────────────── */

PrimitiveOp reduction_resolve_op(const char* verb) {
    /* 
     * For maximum O(1) speed in later phases, this should be upgraded 
     * to a Compile-Time Perfect Hash. For the L1 MVP, an aligned string 
     * comparison isolates the logic perfectly.
     */
    if (strcmp(verb, "plus") == 0) return OP_PLUS;
    if (strcmp(verb, "minus") == 0) return OP_MINUS;
    if (strcmp(verb, "multiply") == 0) return OP_MULTIPLY;
    if (strcmp(verb, "divide") == 0) return OP_DIVIDE;
    
    return OP_UNKNOWN;
}

double reduction_apply(VEC* v, PrimitiveOp op, double a, double b, uint16_t target_reg) {
    /* The VEC mutator APIs inherently handle bounds checking, but we verify here as well */
    if (!v || target_reg >= vec_get_reg_count(v)) return 0.0;

    double result = 0.0;
    
    /* The actual hardware-level logic collapse */
    switch (op) {
        case OP_PLUS:     result = a + b; break;
        case OP_MINUS:    result = a - b; break;
        case OP_MULTIPLY: result = a * b; break;
        case OP_DIVIDE:   
            /* Hardware-safe division. Bypasses OS-level SIGFPE interrupts. */
            result = (b != 0.0) ? (a / b) : 0.0; 
            break;
        default: return 0.0;
    }

    /* 1 & 2. Mutate the shadow register state (automatically sets dirty_mask) */
    vec_write_register(v, target_reg, result);
    
    /* 3. Track thermodynamic cycle cost (1 cycle for primitive ALUs) */
    vec_tick_clock(v, 1);

    return result;
}
