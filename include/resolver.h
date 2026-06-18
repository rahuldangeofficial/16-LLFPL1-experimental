/*
 * resolver.h — The Zero-AST Recursive Descent Resolver.
 *
 * Evaluates nested expressions by collapsing them directly into
 * VEC shadow registers. Supports template invocation via CodeSegment
 * and parameter substitution via execution Frames.
 */

#ifndef RESOLVER_H
#define RESOLVER_H

#include "scanner.h"
#include "registry.h"
#include "template.h"
#include "vec.h"

/* ── API ───────────────────────────────────────────────────────── */

double resolver_evaluate(Scanner* s, Registry* reg, CodeSegment* cs,
                         VEC* v, Frame* frame,
                         uint16_t target_reg, uint16_t free_reg);

#endif /* RESOLVER_H */
