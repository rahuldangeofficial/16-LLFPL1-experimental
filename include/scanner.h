#ifndef SCANNER_H
#define SCANNER_H

#include <stddef.h>

typedef enum {
    ATOM_VERB,    // e.g., Map, Identity, plus, multiply
    ATOM_NOUN,    // e.g., 10, 3.14159
    ATOM_OPEN,    // (
    ATOM_CLOSE,   // )
    ATOM_COMMA,   // ,
    ATOM_EOF,
    ATOM_ERROR
} AtomType;

typedef struct {
    const char* start;
    size_t len;
    AtomType type;
    int line;
} Atom;

typedef struct {
    const char* source;
    const char* cursor;
    int line;
} Scanner;

void scanner_init(Scanner* s, const char* source);
Atom scanner_next(Scanner* s);

#endif
