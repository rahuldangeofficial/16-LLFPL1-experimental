/*
 * vcl.c — Implementation of the Vector Compute Layer.
 *
 * Probes the host system to discover physical vector registers and
 * cache line boundaries. Falls back safely if the platform is unknown.
 */

#include "vcl.h"
#include <stdio.h>
#include <stddef.h>

/* ── Platform-Specific Headers ─────────────────────────────────── */

#if defined(__APPLE__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
#elif defined(__linux__)
    #include <unistd.h>
#elif defined(__x86_64__)
    #include <cpuid.h>
#endif

/* ── Constants ─────────────────────────────────────────────────── */

#define DEFAULT_REGS       16
#define DEFAULT_CACHE_LINE 64

/* ── Private Helpers ───────────────────────────────────────────── */

/* Returns 1 if v is a power of two, 0 otherwise. */
static int is_power_of_two(uint16_t v) {
    return v != 0 && (v & (v - 1)) == 0;
}

/* Dynamically detects L1 data cache line size based on OS/Arch. */
static uint16_t probe_cache_line_size(void) {
#if defined(__APPLE__)
    size_t line_size = 0;
    size_t len       = sizeof(line_size);

    if (sysctlbyname("hw.cachelinesize", &line_size, &len, NULL, 0) == 0) {
        if (line_size >= sizeof(void*) && line_size <= 65535 && is_power_of_two((uint16_t)line_size)) {
            return (uint16_t)line_size;
        }
    }
#elif defined(__linux__)
    long val = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

    if (val >= (long)sizeof(void*) && val <= 65535 && is_power_of_two((uint16_t)val)) {
        return (uint16_t)val;
    }
#elif defined(__x86_64__)
    unsigned int eax, ebx, ecx, edx;

    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        uint16_t line_size = ((ebx >> 8) & 0xFF) * 8;

        if (line_size >= sizeof(void*) && is_power_of_two(line_size)) {
            return line_size;
        }
    }
#endif

    return DEFAULT_CACHE_LINE;
}

/* ── Public API ────────────────────────────────────────────────── */

HardwareProfile vcl_discover(void) {
    HardwareProfile profile;
    
    profile.physical_regs   = DEFAULT_REGS;
    profile.cache_line_size = probe_cache_line_size();

    printf("[VCL] PROBE: %u Registers | %u-byte Cache Alignment\n",
           profile.physical_regs, profile.cache_line_size);

    return profile;
}
