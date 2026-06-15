/*
 * vcl.h — Public interface for the Vector Compute Layer (VCL).
 *
 * Provides a dynamic hardware profile of the host system.
 * The implementation handles OS-specific and CPU-specific
 * probes, while the caller receives only the safe, normalized
 * HardwareProfile result.
 */

#ifndef VCL_H
#define VCL_H

#include <stdint.h>

/* ── Hardware limits ───────────────────────────────────────────── */

typedef struct {
    uint16_t virtual_regs;      /* Virtualized register count  */
    uint16_t cache_line_size;   /* Byte alignment boundary     */
} HardwareProfile;

/* ── API ───────────────────────────────────────────────────────── */

HardwareProfile vcl_discover(void);

#endif /* VCL_H */
