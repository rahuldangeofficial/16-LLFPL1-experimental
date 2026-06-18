# Resolver (Recursive Descent) — Complete Technical Documentation

> **Source**: [`resolver.h`](../include/resolver.h) · [`resolver.c`](../src/resolver.c)
>
> The Zero-AST Recursive Descent Resolver collapses nested mathematical
> expressions directly into hardware-aligned VEC registers. No heap
> allocation. No abstract syntax tree. Zero garbage collection.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Why No AST](#2-why-no-ast)
3. [Active Register Recycling](#3-active-register-recycling)
4. [Algorithmic Complexity](#4-algorithmic-complexity)
5. [Depth Limit & Safety Bounds](#5-depth-limit)
6. [Execution Trace: Mathematical Proof](#6-execution-trace)
7. [Tradeoff Summary Matrix](#7-tradeoff-matrix)

---

## 1. Overview

The `Resolver` closes the loop between the Zero-Copy Scanner (Phase 3)
and the Reduction Core (Phase 4). It reads the token stream produced by
the `Scanner`, and for every nested expression like
`plus(50.5, multiply(10, Base_Offset))`, it recursively collapses the
inner operations first, then applies the outer operation — all without
allocating a single byte on the heap.

---

## 2. Why No AST

Traditional compilers parse source code into an Abstract Syntax Tree:
a tree of heap-allocated nodes where each node holds an operator and
pointers to its children. For `plus(50.5, multiply(10, 100))`, a
typical AST would `malloc` 3 nodes (one for `plus`, one for `multiply`,
one implicit grouping) plus string copies of the operands.

**The Problem**: Every `malloc` triggers a system call. Every pointer
chase between nodes causes an L1 cache miss. For deeply nested
expressions, the heap fragments and the CPU spends more time fetching
node addresses than doing actual math.

**Our Solution**: We use the C call stack itself as an implicit tree.
Each recursive call to `resolver_evaluate` is a "node." The function
arguments (`target_reg`, `free_reg`) act as the "child pointers." When
the function returns, the node is destroyed — for free, with zero
`free()` calls.

---

## 3. Active Register Recycling

### The Problem with Naive Sliding Windows

An earlier prototype used hardcoded offsets:
```c
uint32_t arg1_reg = target_reg + 1;
uint32_t arg2_reg = target_reg + 2;
```

This caused register consumption to scale as `O(2 × depth)`, because
every right-child subtree shifted the window upward permanently, even
though lower registers from completed left-siblings were no longer live.
With 16 registers, maximum nesting depth was approximately 5.

### The Fix: Explicit `free_reg` Tracking

The refactored resolver passes an explicit `free_reg` parameter that
tracks the precise boundary of scratch space:

```c
double resolver_evaluate(Scanner* s, Registry* reg, VEC* v,
                         uint16_t target_reg, uint16_t free_reg);
```

When evaluating `op(A, B)`:
1. Left child `A` is evaluated into `free_reg`. Its scratch starts at `free_reg + 1`.
2. Right child `B` is evaluated into `free_reg + 1`. Its scratch starts at `free_reg + 2`.
3. Once both collapse, registers beyond `free_reg + 1` are fully recyclable.

This bounds register pressure to `O(depth + 1)` — strictly proportional
to tree depth rather than tree width.

### Visual Comparison

For `plus(1, plus(2, plus(3, plus(4, plus(5, plus(6, plus(7, 8)))))))`:

| Model                     | Max Register Used | Fits in 16? |
|---------------------------|-------------------|-------------|
| Naive Sliding Window      | R[15]+            | Barely / No |
| Active Register Recycling | R[8]              | Yes ✓       |

---

## 4. Algorithmic Complexity

| Metric         | Bound        | Explanation                                       |
|----------------|--------------|---------------------------------------------------|
| Time           | `O(N)`       | Every token is consumed exactly once               |
| Heap Space     | `O(1)`       | Zero `malloc` calls during evaluation              |
| Stack Space    | `O(D)`       | Where D = nesting depth, bounded by 16 registers   |
| Register Use   | `O(D + 1)`   | Active Recycling reclaims dead sibling registers   |

Because `D ≤ 15` (hard cap from the VEC register count minus the target),
the maximum stack depth is a compile-time constant. This makes the
resolver's memory consumption effectively `O(1)` in practice.

---

## 5. Depth Limit & Safety Bounds

The resolver enforces a strict depth guard at the top of every call:

```c
if (free_reg >= vec_get_reg_count(v)) {
    printf("[FATAL] Register overflow...\n");
    return 0.0;
}
```

With Active Register Recycling and 16 virtual registers, the maximum
safe nesting depth is approximately **14 levels deep** (target occupies
R[0], each depth adds ~1 register). This is sufficient for all
reasonable template compositions in Phase 6.

---

## 6. Execution Trace: Mathematical Proof

**Input**: `plus(50.5, multiply(10, Base_Offset))`
where `Base_Offset` is bound to `100.0` in the Registry.

**Call**: `resolver_evaluate(s, reg, v, target=0, free=1)`

| Step | Action                          | target | free | R[0]   | R[1] | R[2]  | R[3] | R[4] | Clock |
|------|---------------------------------|--------|------|--------|------|-------|------|------|-------|
| 1    | Enter plus, arg1_reg = 1        | 0      | 1    | 0      | 0    | 0     | 0    | 0    | 0     |
| 2    | Load 50.5 → R[1]               | 1      | 2    | 0      | 50.5 | 0     | 0    | 0    | 0     |
| 3    | Enter multiply, arg1_reg = 3    | 2      | 3    | 0      | 50.5 | 0     | 0    | 0    | 0     |
| 4    | Load 10 → R[3]                  | 3      | 4    | 0      | 50.5 | 0     | 10   | 0    | 0     |
| 5    | Resolve Base_Offset → R[4]      | 4      | 5    | 0      | 50.5 | 0     | 10   | 100  | 0     |
| 6    | multiply(10, 100) → R[2]        | 2      | —    | 0      | 50.5 | 1000  | 10   | 100  | 1     |
| 7    | plus(50.5, 1000) → R[0]         | 0      | —    | 1050.5 | 50.5 | 1000  | 10   | 100  | 2     |

**Final State**: `R[0] = 1050.50`, `Clock = 2` cycles. ✓

**Depth Stress Test**: `plus(1, plus(2, plus(3, plus(4, plus(5, plus(6, plus(7, 8)))))))`
**Result**: `R[0] = 36.00`, `Clock = 7` cycles, max register = R[8]. ✓

---

## 7. Tradeoff Summary Matrix

| Decision                           | What We Gain                            | What We Accept                           |
|------------------------------------|-----------------------------------------|------------------------------------------|
| Zero-AST recursive descent         | No heap allocation, no GC pressure      | Single-pass only, no re-optimization     |
| Active Register Recycling          | O(depth+1) register use, ~14 levels max | Slightly more complex parameter passing  |
| Depth guard via `vec_get_reg_count` | Prevents silent register overflow        | Deep expressions fail-fast               |
| `atom_to_str` with bounded copy    | Prevents buffer overflow on long tokens  | Tokens truncated to 63 characters max    |
| Opaque VEC access throughout       | Structural safety of register state      | Slightly more function call overhead     |
