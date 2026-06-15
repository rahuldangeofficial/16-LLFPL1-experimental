/*
 * vcl.c — Implementation of the Vector Compute Layer.
 *
 * Probes the host system to discover physical vector registers and
 * cache line boundaries. Falls back safely if the platform is unknown.
 *
 * ── Design Decisions ────────────────────────────────────────────
 * 1. virtual_regs = 16: The VM normalizes to 16 virtual registers
 *    to map cleanly to x86_64 hardware limits. This avoids spilling.
 * 2. OS APIs > CPUID: On macOS/Linux, we prefer asking the kernel
 *    for cache lines instead of querying the CPU directly. This is
 *    because the OS may virtualize or override hardware values.
 *    For example, Apple Silicon M-series use 128-byte cache lines.
 * 3. Fallback to 64: 64 is safe on 95% of hardware. If the probe
 *    fails, under-aligning on 128-byte systems is safe but suboptimal,
 *    while over-aligning on 32-byte systems is just wasted memory.
 */

#include "vcl.h"
#include <stddef.h>

/* ── Platform-Specific Headers ─────────────────────────────────── */

/* 
 * Note on ordering: macOS x86_64 defines both __APPLE__ and __x86_64__.
 * We check __APPLE__ first to prefer the OS-level sysctl probe over raw 
 * CPUID, since the OS provides the actual active alignment.
 */
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

/* Compile-time guarantees for the fallback constant */
_Static_assert((DEFAULT_CACHE_LINE & (DEFAULT_CACHE_LINE - 1)) == 0,
               "DEFAULT_CACHE_LINE must be a power of two");
_Static_assert(DEFAULT_CACHE_LINE >= sizeof(void*),
               "DEFAULT_CACHE_LINE must satisfy posix_memalign minimums");

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
    
    profile.virtual_regs    = DEFAULT_REGS;
    profile.cache_line_size = probe_cache_line_size();

    return profile;
}
