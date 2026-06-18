/*
 * scanner.h — Public interface for the Lexical Scanner.
 *
 * Translates raw source code strings into discrete Atoms.
 * Uses an opaque pointer to strictly encapsulate the parsing
 * cursor and prevent external state corruption.
 */

#ifndef SCANNER_H
#define SCANNER_H

#include <stddef.h>
#include <stdint.h>

/* ── Atom Types ────────────────────────────────────────────────── */

typedef enum {
    ATOM_VERB,    /* Functions, identifiers */
    ATOM_NOUN,    /* Numbers, literals      */
    ATOM_OPEN,    /* '('                    */
    ATOM_CLOSE,   /* ')'                    */
    ATOM_COMMA,   /* ','                    */
    ATOM_EOF,     /* End of file            */
    ATOM_ERROR    /* Unrecognized token     */
} AtomType;

/* ── Atom Structure ────────────────────────────────────────────── */

typedef struct {
    const char* start;
    size_t      len;
    AtomType    type;
    uint32_t    line;   /* Prevents overflow on files > 2.1B lines */
} Atom;

/* ── Opaque handle ─────────────────────────────────────────────── */

typedef struct Scanner Scanner;

/* ── API ───────────────────────────────────────────────────────── */

Scanner* scanner_create (const char* source);
void     scanner_destroy(Scanner* s);

Atom     scanner_next   (Scanner* s);
Atom     scanner_peek   (Scanner* s);
Scanner* scanner_clone  (Scanner* s);

#endif /* SCANNER_H */
