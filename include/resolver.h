/*
 * resolver.h — The Zero-AST Recursive Descent Resolver.
 *
 * Evaluates nested expressions by collapsing them directly into
 * VEC shadow registers. No heap allocation. No AST overhead.
 * Uses Active Register Recycling via an explicit free_reg tracker.
 */

#ifndef RESOLVER_H
#define RESOLVER_H

#include "scanner.h"
#include "registry.h"
#include "vec.h"

/* ── API ───────────────────────────────────────────────────────── */

double resolver_evaluate(Scanner* s, Registry* reg, VEC* v,
                         uint16_t target_reg, uint16_t free_reg);

#endif /* RESOLVER_H */
