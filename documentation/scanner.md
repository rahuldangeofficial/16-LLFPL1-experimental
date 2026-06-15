# Scanner (Lexer) — Complete Technical Documentation

> **Source**: [`scanner.h`](../include/scanner.h) · [`scanner.c`](../src/scanner.c)
>
> Single source of truth for the Lexical Scanner's state machine, algorithmic complexity,
> security constraints, and data structure proofs.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Opaque Pointer Encapsulation](#2-opaque-pointer)
3. [Algorithmic Proof: The Non-Recursive State Machine](#3-algorithmic-proof)
4. [Integer Bounds Safety](#4-integer-bounds)
5. [The `make_atom` Design](#5-make-atom)
6. [Tradeoff Summary Matrix](#6-tradeoff-matrix)

---

## 1. Overview

The `Scanner` (often called a Lexer) is the first pipeline of the frontend compiler layer. 
It takes raw, unstructured string data from a source file and translates it into discrete, 
structurally typed `Atom` tokens. 

The execution engine reads these atoms to build its abstract syntax trees or perform immediate 
execution. The scanner is designed to be **Zero-Copy**: it does not allocate strings on the 
heap. Instead, it yields an `Atom` containing a memory pointer to the start of the token and 
the length of the token.

---

## 2. Opaque Pointer Encapsulation

```c
typedef struct Scanner Scanner;
```

To guarantee that the parsing cursor state can never be illegally modified by a caller 
(which would result in catastrophic parsing failures), the `Scanner` struct is entirely 
hidden inside `scanner.c`.

Callers must strictly interface via:
1. `Scanner* scanner_create(const char* source);`
2. `Atom scanner_next(Scanner* s);`
3. `void scanner_destroy(Scanner* s);`

This adheres strictly to the SOLID Open/Closed principle and enforces state isolation.

---

## 3. Algorithmic Proof: The Non-Recursive State Machine

### The Flaw with Recursive Scanning
In early prototypes, the scanner skipped comments using recursion:
```c
/* Legacy Vulnerable Code */
if (c == '-' && *(s->cursor + 1) == '-') {
    while (*s->cursor != '\n' && *s->cursor != '\0') s->cursor++;
    return scanner_next(s); /* CWE-674 Vulnerability */
}
```
If an attacker supplied an LLFPL1 file containing 100,000 consecutive comment lines, the 
stack frames would overflow, crashing the interpreter abruptly.

### The O(1) Iterative Solution
The refactored `scanner.c` uses a strict `while(1)` state loop to strip all noise (whitespace 
and comments) before parsing an atom.

```c
while (1) {
    /* Strip whitespace */
    while (isspace(*s->cursor)) { s->cursor++; }

    /* Strip comments */
    if (*s->cursor == '-' && *(s->cursor + 1) == '-') {
        while (*s->cursor != '\n' && *s->cursor != '\0') { s->cursor++; }
        continue; /* Loop back iteratively, NO recursion */
    }
    
    break; /* Noise cleared, ready to parse */
}
```

**Algorithmic Complexity Proof**:
- **Time Complexity**: `O(N)` where N is the length of the string. Every character is evaluated at most twice (once in the noise loop, once in the parsing switch).
- **Space Complexity**: `O(1)`. Because recursion was eliminated, parsing a file of 10 lines uses the exact same stack memory as a file of 10 million lines. Memory consumption is completely flat and mathematically safe against Stack Overflows.

---

## 4. Integer Bounds Safety

An `Atom` tracks the line number it was found on for error reporting.

```c
typedef struct {
    ...
    uint32_t line;
} Atom;
```

A standard signed `int` overflows at 2.14 billion. An attacker generating a massive synthetic 
script could overflow this to a negative number, leading to out-of-bounds array reads if 
downstream parsers use the line number as an array index. 

By upgrading to `uint32_t`, we mathematically eliminate signed integer overflow behavior and 
support a theoretical limit of 4.29 billion lines per file.

---

## 5. The `make_atom` Design

The scanner uses a private static helper to construct the return tokens:

```c
static Atom make_atom(Scanner* s, AtomType type, size_t length)
```

**Why is this elegant?**
1. **Zero-copy strings**: It captures `s->cursor` directly into `a.start`.
2. **State mutator**: It automatically advances the cursor by `length`.
3. **Consistency**: Every single path inside `scanner_next` converges into this one function, 
   ensuring that line tracking, cursor advancement, and struct packing all happen exactly the 
   same way.

---

## 6. Tradeoff Summary Matrix

| Decision                           | What We Gain                            | What We Accept                           |
|------------------------------------|-----------------------------------------|------------------------------------------|
| Opaque `Scanner` pointer           | State safety from external tampering    | Requires heap allocation `malloc(*s)`    |
| Iterative state machine (`while`)  | O(1) Memory, eliminates Stack Overflow  | Slightly more nesting in `scanner.c`     |
| `uint32_t` line tracking           | Prevents signed integer overflow        | Memory alignment pads `Atom` struct      |
| Zero-copy pointer string mapping   | Maximum speed, no `malloc` per token    | Strings are not null-terminated `\0`     |
| `make_atom` cursor mutation        | DRY code, guarantees cursor advances    | Implicit side-effects inside a helper    |
