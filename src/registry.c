/*
 * registry.c — Coalesced-chaining hash table for identity storage.
 *
 * Layout:  [ primary zone (880 slots) | cellar (144 slots) ]
 *          Slots 0–879 are addressed by hash.
 *          Slots 880–1023 absorb collisions via a cellar cursor.
 *
 * Design:  Each Identity is exactly 32 bytes so two entries fit
 *          one 64-byte cache line.  The entry slab is allocated
 *          with posix_memalign to guarantee cache-line alignment.
 *
 * Axiom rule: once bound, a name is immutable — rebinding is a no-op.
 */

#define _POSIX_C_SOURCE 200112L

#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Internal constants ────────────────────────────────────────── */

#define CAPACITY      1024
#define PRIMARY_SLOTS  880
#define CACHE_LINE      64
#define NAME_MAX_LEN    19
#define NO_NEXT     0xFFFF

_Static_assert(CAPACITY < NO_NEXT,
               "CAPACITY must stay below NO_NEXT to avoid sentinel collision");

/* ── Internal types ────────────────────────────────────────────── */

typedef struct {
    ValueData value;         /*  8 bytes */
    uint16_t  next;          /*  2 bytes */
    uint8_t   active;        /*  1 byte  */
    uint8_t   type;          /*  1 byte  */
    char      name[NAME_MAX_LEN + 1]; /* 20 bytes → 32 total */
} Entry;

struct Registry {
    Entry    *entries;
    uint32_t  count;
    uint32_t  cellar;        /* next candidate slot in the cellar */
};

/* ── Helpers ───────────────────────────────────────────────────── */

static void write_name(Entry *e, const char *name) {
    strncpy(e->name, name, NAME_MAX_LEN);
    e->name[NAME_MAX_LEN] = '\0';
}

static int names_match(const Entry *e, const char *name) {
    return strncmp(e->name, name, NAME_MAX_LEN) == 0;
}

static uint32_t hash(const char *s) {
    uint32_t h = 2166136261u;
    int n = NAME_MAX_LEN;
    while (*s && n-- > 0) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }
    return h % PRIMARY_SLOTS;
}

static void fill_entry(Entry *e, const char *name,
                       ValueType type, ValueData data) {
    e->value  = data;
    e->type   = (uint8_t)type;
    e->active = 1;
    write_name(e, name);
}

/* ── Lifecycle ─────────────────────────────────────────────────── */

Registry *registry_init(void) {
    Registry *reg = malloc(sizeof *reg);
    if (!reg) return NULL;

    void *mem = NULL;
    if (posix_memalign(&mem, CACHE_LINE, sizeof(Entry) * CAPACITY) != 0) {
        free(reg);
        return NULL;
    }
    reg->entries = mem;

    for (int i = 0; i < CAPACITY; i++) {
        reg->entries[i] = (Entry){
            .active = 0,
            .type   = TYPE_UNDEFINED,
            .next   = NO_NEXT,
        };
    }

    reg->count  = 0;
    reg->cellar = PRIMARY_SLOTS;
    return reg;
}

void registry_shutdown(Registry *reg) {
    if (!reg) return;
    free(reg->entries);
    free(reg);
}

/* ── Bind ──────────────────────────────────────────────────────── */

void registry_bind(Registry *reg, const char *name,
                   ValueType type, ValueData data) {

    if (reg->count >= CAPACITY) {
        printf("[FATAL] Registry slab exhausted.\n");
        return;
    }

    uint32_t slot = hash(name);

    /* Fast path: primary slot is free. */
    if (!reg->entries[slot].active) {
        fill_entry(&reg->entries[slot], name, type, data);
        reg->count++;
        return;
    }

    /* Walk chain — bail if name already bound (immutability rule). */
    uint32_t cur = slot;
    while (1) {
        if (names_match(&reg->entries[cur], name)) return;
        if (reg->entries[cur].next == NO_NEXT)     break;
        cur = reg->entries[cur].next;
    }

    /* Grab the next free cellar slot. */
    uint32_t dest = CAPACITY;
    while (reg->cellar < CAPACITY) {
        if (!reg->entries[reg->cellar].active) {
            dest = reg->cellar++;
            break;
        }
        reg->cellar++;
    }
    if (dest == CAPACITY) {
        printf("[FATAL] Cellar exhausted.\n");
        return;
    }

    fill_entry(&reg->entries[dest], name, type, data);
    reg->entries[cur].next = dest;
    reg->count++;
}

/* ── Resolve ───────────────────────────────────────────────────── */

ValueData registry_resolve(Registry *reg, const char *name,
                           ValueType *out_type) {

    for (uint32_t cur = hash(name);
         cur != NO_NEXT && reg->entries[cur].active;
         cur = reg->entries[cur].next) {

        if (names_match(&reg->entries[cur], name)) {
            if (out_type) *out_type = (ValueType)reg->entries[cur].type;
            return reg->entries[cur].value;
        }
    }

    if (out_type) *out_type = TYPE_UNDEFINED;
    return (ValueData){ .u64 = 0 };
}
