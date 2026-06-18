/*
 * template.c — Implementation of the Code Segment.
 *
 * ── Design Decisions ────────────────────────────────────────────
 * 1. Memory Coordinate Snapshots: When a Map is defined, we clone
 *    the Scanner at the exact byte where the body starts. No AST
 *    is built. The template is a pointer into the mmap'd file.
 * 2. Lookahead Detection: To distinguish parameters from the body
 *    tree, we peek two tokens ahead. If a VERB is followed by '(',
 *    it is a function call (the body), not a parameter name.
 * 3. Balanced Fast-Forward: After snapshotting, the main scanner
 *    is advanced past the entire Map definition using a parenthesis
 *    balance counter.
 */

#include "template.h"
#include <stdlib.h>
#include <string.h>

/* ── Private Helpers ───────────────────────────────────────────── */

static void copy_atom_str(Atom a, char* dest, size_t dest_size) {
    size_t copy_len = (a.len < dest_size - 1) ? a.len : dest_size - 1;
    memcpy(dest, a.start, copy_len);
    dest[copy_len] = '\0';
}

/* ── API ───────────────────────────────────────────────────────── */

CodeSegment* cs_init(void) {
    CodeSegment* cs = malloc(sizeof(*cs));
    if (!cs) return NULL;
    cs->count = 0;
    memset(cs->entries, 0, sizeof(Template) * 256);
    return cs;
}

void cs_shutdown(CodeSegment* cs) {
    if (!cs) return;
    /* Free all cloned body scanners */
    for (uint32_t i = 0; i < cs->count; i++) {
        scanner_destroy(cs->entries[i].body_stream);
    }
    free(cs);
}

void cs_define_map(CodeSegment* cs, Scanner* s) {
    scanner_next(s);  /* Consume '(' */
    Atom name_atom = scanner_next(s);
    scanner_next(s);  /* Consume ',' */

    Template tmpl;
    memset(&tmpl, 0, sizeof(Template));
    copy_atom_str(name_atom, tmpl.name, sizeof(tmpl.name));

    /* Parse parameters until we detect the start of the body tree */
    while (1) {
        Atom peek1 = scanner_peek(s);

        /* If next atom is a verb followed by '(', it is the body tree */
        if (peek1.type == ATOM_VERB) {
            Scanner* temp = scanner_clone(s);
            scanner_next(temp);
            Atom peek2 = scanner_next(temp);
            scanner_destroy(temp);
            if (peek2.type == ATOM_OPEN) break;  /* Reached body tree */
        }

        /* Otherwise, it's a parameter name */
        Atom param = scanner_next(s);
        copy_atom_str(param, tmpl.params[tmpl.param_count], sizeof(tmpl.params[0]));
        tmpl.param_count++;
        scanner_next(s);  /* Consume ',' */
    }

    /* Snapshot the exact memory coordinate of the body */
    tmpl.body_stream = scanner_clone(s);
    tmpl.active = 1;

    /* Fast-forward the physical scanner past the Map definition.
     * Balance starts at 1 to account for Map's outer '(' already consumed. */
    int balance = 1;
    while (balance > 0) {
        Atom fwd = scanner_next(s);
        if (fwd.type == ATOM_OPEN)  balance++;
        if (fwd.type == ATOM_CLOSE) balance--;
        if (fwd.type == ATOM_EOF)   break;
    }

    cs->entries[cs->count++] = tmpl;
}

Template* cs_resolve(CodeSegment* cs, const char* name) {
    for (uint32_t i = 0; i < cs->count; i++) {
        if (strcmp(cs->entries[i].name, name) == 0) {
            return &cs->entries[i];
        }
    }
    return NULL;
}
