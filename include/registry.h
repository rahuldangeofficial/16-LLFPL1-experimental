/*
 * registry.h — Public interface for the identity registry.
 *
 * Callers see only an opaque Registry pointer and the value types.
 * All storage layout, hashing, and collision strategy are private
 * to registry.c — change them freely without touching any caller.
 */

#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdint.h>

/* ── Value types ───────────────────────────────────────────────── */

typedef enum {
    TYPE_UNDEFINED = 0,
    TYPE_F64       = 1,
    TYPE_I64       = 2,
    TYPE_U64       = 3,
    TYPE_PTR       = 4,
    TYPE_BOOL      = 5
} ValueType;

typedef union {
    double    f64;
    int64_t   i64;
    uint64_t  u64;
    void     *ptr;
    uint8_t   boolean;
} ValueData;

/* ── Opaque handle ─────────────────────────────────────────────── */

typedef struct Registry Registry;

/* ── API ───────────────────────────────────────────────────────── */

Registry *registry_init    (void);
void      registry_shutdown(Registry *reg);

void      registry_bind    (Registry *reg, const char *name,
                            ValueType type, ValueData data);

ValueData registry_resolve (Registry *reg, const char *name,
                            ValueType *out_type);

#endif /* REGISTRY_H */
