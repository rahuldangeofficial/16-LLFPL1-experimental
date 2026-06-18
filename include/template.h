/*
 * template.h — The Code Segment for Template (Map) definitions.
 *
 * When the engine reads a Map definition, it snapshots the Scanner's
 * exact memory coordinate (via scanner_clone) into a Template entry.
 * When the Map is later called, we clone that snapshot, build a
 * temporary execution Frame, and pipe it through the resolver.
 * Zero heap allocation per call. O(1) function pointers.
 */

#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "scanner.h"
#include <stdint.h>

/* ── Template Entry ────────────────────────────────────────────── */

typedef struct {
    char      name[20];
    char      params[4][20];    /* Up to 4 parameters for L1 MVP */
    uint8_t   param_count;
    Scanner*  body_stream;      /* Cloned scanner at body start   */
    uint8_t   active;
} Template;

/* ── Code Segment ──────────────────────────────────────────────── */

typedef struct {
    Template entries[256];      /* Fixed L1 code segment size     */
    uint32_t count;
} CodeSegment;

/* ── Execution Frame ───────────────────────────────────────────── */

typedef struct {
    char    keys[4][20];
    double  values[4];
    uint8_t count;
} Frame;

/* ── API ───────────────────────────────────────────────────────── */

CodeSegment* cs_init    (void);
void         cs_shutdown(CodeSegment* cs);
void         cs_define_map(CodeSegment* cs, Scanner* s);
Template*    cs_resolve (CodeSegment* cs, const char* name);

#endif /* TEMPLATE_H */
