# Template Mapping (Phase 6) — Complete Technical Documentation

> **Source**: [`template.h`](../include/template.h) · [`template.c`](../src/template.c) · [`resolver.c`](../src/resolver.c)
>
> Implements Zero-AST user-defined functions via memory coordinate
> snapshots. No abstract syntax tree is ever built for function bodies.

---

## Table of Contents

1. [Overview](#1-overview)
2. [The Memory Coordinate Snapshot](#2-memory-coordinate-snapshot)
3. [Four-Layer Resolution Priority](#3-four-layer-resolution)
4. [Execution Frame Mechanics](#4-execution-frame)
5. [Scanner Opacity Extensions](#5-scanner-extensions)
6. [Execution Trace: Mathematical Proof](#6-execution-trace)
7. [Tradeoff Summary Matrix](#7-tradeoff-matrix)

---

## 1. Overview

In a traditional compiler, defining a function creates a massive AST on the
heap — nodes for the function name, parameter list, and body tree, all
connected by pointers that cause L1 cache misses during execution.

LLFPL1 completely bypasses this. When the engine reads `Map(Calculate, x, plus(x, Base_Offset))`,
it does **not** parse or execute the body. It simply saves a `Scanner*` clone —
a snapshot of the exact byte position in the `mmap`'d file where the body starts.
When `Calculate(50.5)` is later called, we clone that snapshot, build a tiny
stack-local `Frame` to bind `x = 50.5`, and pipe it through the same resolver.

---

## 2. The Memory Coordinate Snapshot

### Definition Phase

```
Source file (mmap'd):  "Map(Calculate, x, plus(x, Base_Offset))"
                                          ^
                                          |
                              scanner_clone() captures this exact pointer
```

The `cs_define_map` function:
1. Consumes `(`, reads the name `Calculate`, consumes `,`
2. Reads parameters (`x`) until it detects a VERB followed by `(` (the body)
3. Calls `scanner_clone(s)` to snapshot the body position
4. Fast-forwards the main scanner past the entire definition using a parenthesis
   balance counter

### Execution Phase

When `Calculate(50.5)` is called:
1. The resolver finds the template via `cs_resolve`
2. It evaluates the argument `50.5` and builds a `Frame { keys=["x"], values=[50.5] }`
3. It clones the body scanner: `Scanner* temp = scanner_clone(tmpl->body_stream)`
4. It calls `resolver_evaluate` with the cloned scanner and the frame
5. The frame-aware resolver substitutes `x → 50.5` during evaluation
6. The cloned scanner is destroyed after use

**Cost**: One `malloc(sizeof(Scanner))` per template invocation. The Scanner
struct is only 16 bytes (two pointers + one uint32_t). This is negligible
compared to a traditional AST which would allocate dozens of nodes.

---

## 3. Four-Layer Resolution Priority

When the resolver encounters a VERB, it checks in strict order:

| Priority | Layer            | Example        | Action                              |
|----------|------------------|----------------|-------------------------------------|
| 1        | Primitive Op     | `plus`         | Initiates structural induction      |
| 2        | Execution Frame  | `x`            | Returns frame-local parameter value |
| 3        | Code Segment     | `Calculate`    | Spawns sub-scanner from template    |
| 4        | Global Registry  | `Base_Offset`  | Returns static Identity value       |

This priority order is critical. A local parameter `x` must shadow a global
Identity named `x`. And a user template must be checked before falling back
to the global registry.

---

## 4. Execution Frame Mechanics

```c
typedef struct {
    char    keys[4][20];
    double  values[4];
    uint8_t count;
} Frame;
```

The Frame is a stack-local struct (zero heap allocation). It maps parameter
names to their evaluated values. When the resolver evaluates a template body,
it passes the Frame down. Any VERB that matches a Frame key returns the
corresponding value immediately, without touching the Registry or CodeSegment.

### Limit: 4 Parameters

The L1 MVP supports up to 4 parameters per template. This is sufficient for
all standard mathematical operations (`f(a, b, c, d)`). The limit can be
increased trivially by changing the array sizes.

---

## 5. Scanner Opacity Extensions

Phase 6 required two new Scanner operations that preserve opaque encapsulation:

| Function          | Purpose                                              |
|-------------------|------------------------------------------------------|
| `scanner_peek`    | Non-destructive lookahead (stack-copies internally)   |
| `scanner_clone`   | Heap-allocated copy for template body snapshots       |

Both functions are implemented **inside** `scanner.c` where the struct is
visible. External callers never see the Scanner internals. The `*s` struct
copy is only legal within `scanner.c`.

---

## 6. Execution Trace: Mathematical Proof

**Input** (`tests/main.LLFPL1`):
```
Identity(Base_Offset, 100)
Map(Calculate, x, plus(x, Base_Offset))
Commit(Calculate(50.5))
```

**Step-by-step**:

| Step | Action                                       | R[0]   | Clock |
|------|----------------------------------------------|--------|-------|
| 1    | `Identity(Base_Offset, 100)` → Registry bind | —      | 0     |
| 2    | `Map(Calculate, x, ...)` → Snapshot saved    | —      | 0     |
| 3    | `Commit(Calculate(50.5))` → Enter resolver   | —      | 0     |
| 4    | Resolve "Calculate" → template found          | —      | 0     |
| 5    | Evaluate arg: 50.5 → Frame{x=50.5}           | —      | 0     |
| 6    | Clone body scanner → evaluate `plus(x, Base_Offset)` | — | 0  |
| 7    | Resolve "x" → Frame lookup → 50.5            | —      | 0     |
| 8    | Resolve "Base_Offset" → Registry → 100.0     | —      | 0     |
| 9    | `plus(50.5, 100.0)` → R[0]                   | 150.50 | 1     |

**Final State**: `R[0] = 150.50`, `Clock = 1` cycle. ✓

---

## 7. Tradeoff Summary Matrix

| Decision                           | What We Gain                            | What We Accept                           |
|------------------------------------|-----------------------------------------|------------------------------------------|
| Memory coordinate snapshots        | Zero-AST, O(1) function definition      | Body re-parsed on every call             |
| `scanner_clone` for body storage   | Preserves opaque pointer encapsulation   | 16-byte heap alloc per template          |
| Stack-local `Frame` struct         | Zero heap alloc for parameter binding    | Max 4 parameters per template            |
| Four-layer resolution priority     | Local params shadow globals correctly    | Linear scan through Frame keys           |
| Balanced fast-forward              | Main scanner skips bodies cleanly        | Requires matching parentheses            |
