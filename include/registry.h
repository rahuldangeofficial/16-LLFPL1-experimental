#ifndef REGISTRY_H
#define REGISTRY_H

#include<stdio.h>
#include<stdint.h>

#define MAX_IDENTITIES 1024
// x86_64 standard L1 cache line size in bytes
#define SYS_CACHE_LINE 64

typedef struct{
    char name[20];      // The string key
    double value;       // The truth state
    uint8_t active;     // 1 if occupied
    uint8_t psl;        // Robin Hood: Probe Sequence Length
    uint8_t padding[2]; // Math forces perfectly 32-byte alignment
}Identity;

typedef struct{
	Identity* entries;
	uint32_t count;
}Registry;

Registry* registry_init();

void registry_bind(Registry* reg, const char* name, double value);

double registry_resolve(Registry* reg, const char* name);

void registry_shutdown(Registry* reg);

#endif
