#include "scanner.h"
#include <ctype.h>

void scanner_init(Scanner* s, const char* source) {
    s->source = source;
    s->cursor = source;
    s->line = 1;
}

static Atom make_atom(Scanner* s, AtomType type, size_t length) {
    Atom a;
    a.start = s->cursor;
    a.len = length;
    a.type = type;
    a.line = s->line;
    s->cursor += length;
    return a;
}

Atom scanner_next(Scanner* s) {
    // Strip noise and tracks streams
    while (*s->cursor != '\0' && isspace(*s->cursor)) {
        if (*s->cursor == '\n') s->line++;
        s->cursor++;
    }

    if (*s->cursor == '\0') return make_atom(s, ATOM_EOF, 0);

    char c = *s->cursor;

    // Comments handling: double dash style '--'
    if (c == '-' && *(s->cursor + 1) == '-') {
        while (*s->cursor != '\n' && *s->cursor != '\0') {
            s->cursor++;
        }
        return scanner_next(s); // Recurse to pass comment line
    }

    if (isalpha(c) || c == '_') {
        size_t len = 0;
        while (isalnum(s->cursor[len]) || s->cursor[len] == '_') len++;
        return make_atom(s, ATOM_VERB, len);
    }

    if (isdigit(c)) {
        size_t len = 0;
        while (isdigit(s->cursor[len]) || s->cursor[len] == '.') len++;
        return make_atom(s, ATOM_NOUN, len);
    }

    switch (c) {
        case '(': return make_atom(s, ATOM_OPEN, 1);
        case ')': return make_atom(s, ATOM_CLOSE, 1);
        case ',': return make_atom(s, ATOM_COMMA, 1);
    }

    return make_atom(s, ATOM_ERROR, 1);
}

