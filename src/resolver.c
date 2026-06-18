/*
 * resolver.c — Zero-AST Recursive Descent Resolver.
 *
 * ── Design Decisions ────────────────────────────────────────────
 * 1. Active Register Recycling: Explicit free_reg tracking bounds
 *    register pressure to O(depth + 1).
 * 2. Four-Layer Resolution: When a VERB is encountered, it is
 *    checked in strict priority order:
 *      (a) Primitive math op  (plus, minus, multiply, divide)
 *      (b) Execution Frame    (local template parameter like 'x')
 *      (c) Code Segment       (user-defined Map template)
 *      (d) Global Registry    (static Identity binding)
 * 3. Template Execution: Clones the template's body scanner,
 *    builds a sub-frame, and pipes through this same resolver.
 *    Zero heap allocation per math operation.
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

double resolver_evaluate(Scanner* s, Registry* reg, CodeSegment* cs,
                         VEC* v, Frame* frame,
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

        /* (a) Check if it's a primitive math verb */
        PrimitiveOp op = reduction_resolve_op(buffer);

        if (op == OP_UNKNOWN) {
            /* (b) Check Execution Frame (local parameter like 'x') */
            if (frame) {
                for (uint8_t i = 0; i < frame->count; i++) {
                    if (strcmp(frame->keys[i], buffer) == 0) {
                        vec_write_register(v, target_reg, frame->values[i]);
                        return frame->values[i];
                    }
                }
            }

            /* (c) Check Code Segment (user-defined template like 'Calculate') */
            Template* tmpl = cs ? cs_resolve(cs, buffer) : NULL;
            if (tmpl) {
                scanner_next(s);  /* Consume '(' */

                /* Build the execution frame for this template call */
                Frame sub_frame;
                memset(&sub_frame, 0, sizeof(Frame));
                sub_frame.count = tmpl->param_count;
                for (uint8_t i = 0; i < tmpl->param_count; i++) {
                    strncpy(sub_frame.keys[i], tmpl->params[i], 19);
                    sub_frame.keys[i][19] = '\0';
                    sub_frame.values[i] = resolver_evaluate(
                        s, reg, cs, v, frame, target_reg, free_reg);
                    if (i < tmpl->param_count - 1) {
                        scanner_next(s);  /* Consume ',' */
                    }
                }
                scanner_next(s);  /* Consume ')' */

                /* Execute the template using its saved memory coordinate */
                Scanner* temp_s = scanner_clone(tmpl->body_stream);
                double result = resolver_evaluate(
                    temp_s, reg, cs, v, &sub_frame, target_reg, free_reg);
                scanner_destroy(temp_s);
                return result;
            }

            /* (d) Fallback to Global Registry (static Identity) */
            ValueType out_type;
            ValueData data = registry_resolve(reg, buffer, &out_type);
            double val = (out_type == TYPE_F64) ? data.f64 : 0.0;
            vec_write_register(v, target_reg, val);
            return val;
        }

        /* ── Primitive Math Induction (Phase 5 Logic) ──────────── */
        Atom open_paren = scanner_next(s);
        if (open_paren.type != ATOM_OPEN) {
            printf("[FATAL] Syntax Geometry Error: Expected '(' at line %u\n", a.line);
            return 0.0;
        }

        uint16_t arg1_reg = free_reg;
        double val1 = resolver_evaluate(s, reg, cs, v, frame, arg1_reg, free_reg + 1);

        Atom comma = scanner_next(s);
        if (comma.type != ATOM_COMMA) {
            printf("[FATAL] Syntax Geometry Error: Expected ',' at line %u\n", a.line);
            return 0.0;
        }

        uint16_t arg2_reg = arg1_reg + 1;
        double val2 = resolver_evaluate(s, reg, cs, v, frame, arg2_reg, arg1_reg + 2);

        Atom close_paren = scanner_next(s);
        if (close_paren.type != ATOM_CLOSE) {
            printf("[FATAL] Syntax Geometry Error: Expected ')' at line %u\n", a.line);
            return 0.0;
        }

        return reduction_apply(v, op, val1, val2, target_reg);
    }

    return 0.0;
}
