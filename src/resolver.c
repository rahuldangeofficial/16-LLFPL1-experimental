/*
 * resolver.c — Zero-AST Recursive Descent Resolver.
 *
 * ── Design Decisions ────────────────────────────────────────────
 * 1. Active Register Recycling: Instead of naive sliding-window
 *    offsets (target + 1, target + 2), we pass an explicit free_reg
 *    tracker. This bounds register pressure to O(depth + 1) instead
 *    of O(2^depth), preserving the 16-register hardware ceiling for
 *    deeply nested template compositions in Phase 6.
 * 2. No Heap Allocation: The C call stack is the implicit tree.
 * 3. Opaque Pointer Safety: All VEC writes go through safe mutators.
 * 4. Depth Limit: Bounded by vec_get_reg_count (16 registers).
 */

#include "resolver.h"
#include "reduction.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Private Helpers ───────────────────────────────────────────── */

static void atom_to_str(Atom a, char* buffer, size_t buf_size) {
    size_t copy_len = (a.len < buf_size - 1) ? a.len : buf_size - 1;
    memcpy(buffer, a.start, copy_len);
    buffer[copy_len] = '\0';
}

/* ── API ───────────────────────────────────────────────────────── */

double resolver_evaluate(Scanner* s, Registry* reg, VEC* v,
                         uint16_t target_reg, uint16_t free_reg) {

    /* Depth guard: prevent register overflow beyond VEC capacity */
    if (free_reg >= vec_get_reg_count(v)) {
        printf("[FATAL] Register overflow: nesting depth exceeds %u virtual registers.\n",
               vec_get_reg_count(v));
        return 0.0;
    }

    Atom a = scanner_next(s);
    if (a.type == ATOM_EOF || a.type == ATOM_ERROR) return 0.0;

    char buffer[64];

    /* BASE CASE 1: Pure Number (Noun) */
    if (a.type == ATOM_NOUN) {
        atom_to_str(a, buffer, sizeof(buffer));
        double val = atof(buffer);
        vec_write_register(v, target_reg, val);
        return val;
    }

    /* BASE CASE 2 & INDUCTION: Verb or Identity */
    if (a.type == ATOM_VERB) {
        atom_to_str(a, buffer, sizeof(buffer));

        PrimitiveOp op = reduction_resolve_op(buffer);

        /* If it's not a primitive math verb, it must be an Identity */
        if (op == OP_UNKNOWN) {
            ValueType out_type;
            ValueData data = registry_resolve(reg, buffer, &out_type);
            double val = (out_type == TYPE_F64) ? data.f64 : 0.0;
            vec_write_register(v, target_reg, val);
            return val;
        }

        /* It IS a primitive verb. Initiate Structural Induction. */
        Atom open_paren = scanner_next(s);
        if (open_paren.type != ATOM_OPEN) {
            printf("[FATAL] Syntax Geometry Error: Expected '(' at line %u\n", a.line);
            return 0.0;
        }

        /* Evaluate Left Child into the first free register slot */
        uint16_t arg1_reg = free_reg;
        double val1 = resolver_evaluate(s, reg, v, arg1_reg, free_reg + 1);

        Atom comma = scanner_next(s);
        if (comma.type != ATOM_COMMA) {
            printf("[FATAL] Syntax Geometry Error: Expected ',' at line %u\n", a.line);
            return 0.0;
        }

        /*
         * Evaluate Right Child.
         * CRITICAL: arg1_reg is now occupied holding the left result.
         * The right child's destination is arg1_reg + 1.
         * Its internal scratch space starts at arg1_reg + 2.
         */
        uint16_t arg2_reg = arg1_reg + 1;
        double val2 = resolver_evaluate(s, reg, v, arg2_reg, arg1_reg + 2);

        Atom close_paren = scanner_next(s);
        if (close_paren.type != ATOM_CLOSE) {
            printf("[FATAL] Syntax Geometry Error: Expected ')' at line %u\n", a.line);
            return 0.0;
        }

        /* Both sides collapsed. Apply reduction into target register. */
        return reduction_apply(v, op, val1, val2, target_reg);
    }

    return 0.0;
}
