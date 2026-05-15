#define _POSIX_C_SOURCE 200112L
#include "registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uint32_t hash_identity(const char* str) {
    uint32_t hash = 0;
    while (*str) {
        hash += *str++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % MAX_IDENTITIES;
}

Registry* registry_init() {
    Registry* reg = (Registry*)malloc(sizeof(Registry));
    if (!reg) return NULL;

    if (posix_memalign((void**)&reg->entries, 64, sizeof(Identity) * MAX_IDENTITIES) != 0) {
        free(reg);
        return NULL;
    }

    memset(reg->entries, 0, sizeof(Identity) * MAX_IDENTITIES);
    reg->count = 0;
    return reg;
}

void registry_bind(Registry* reg, const char* name, double value) {
    if (reg->count >= MAX_IDENTITIES) {
        fprintf(stderr, "[REGISTRY] Error: Registry full, cannot bind '%s'\n", name);
        return;
    }

    uint32_t slot = hash_identity(name);
    uint32_t start_slot = slot;

    while (reg->entries[slot].active) {
        if (strcmp(reg->entries[slot].name, name) == 0) break;
        slot = (slot + 1) % MAX_IDENTITIES;
        if (slot == start_slot) return; // Should not happen given count check
    }

    if (!reg->entries[slot].active) {
        strncpy(reg->entries[slot].name, name, 19);
        reg->entries[slot].name[19] = '\0'; // Ensure null-termination
        reg->entries[slot].active = 1;
        reg->count++;
    }
    
    reg->entries[slot].value = value;
}

double registry_resolve(Registry* reg, const char* name) {
    uint32_t slot = hash_identity(name);
    uint32_t start_slot = slot;

    while (reg->entries[slot].active) {
        if (strcmp(reg->entries[slot].name, name) == 0) {
            return reg->entries[slot].value;
        }
        slot = (slot + 1) % MAX_IDENTITIES;
        if (slot == start_slot) break;
    }
    return 0.0;
}

void registry_shutdown(Registry* reg) {
    if (reg) {
        free(reg->entries);
        free(reg);
    }
}
