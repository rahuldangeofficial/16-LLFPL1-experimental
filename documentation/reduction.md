# Reduction Core (L1) — Complete Technical Documentation

> **Source**: [`reduction.h`](../include/reduction.h) · [`reduction.c`](../src/reduction.c)
>
> The Primitive Execution Engine bypasses traditional AST trees by collapsing mathematical
> reductions directly into hardware-aligned virtual registers.

---

## Table of Contents

1. [Overview](#1-overview)
2. [O(1) Verb Resolution](#2-verb-resolution)
3. [Thermodynamic Cost Tracking](#3-thermodynamic-cost)
4. [Hardware Safe Mutation](#4-hardware-safe-mutation)
5. [Hardware Safe Division](#5-hardware-safe-division)

---

## 1. Overview

The `Reduction Core` acts as the Mathematical Arithmetic Logic Unit (ALU) for the virtual machine.
It takes a recognized verb (e.g., "plus", "divide"), evaluates the mathematical payload, and safely 
mutates the state of the underlying `VEC` registers. 

By executing the logic here and writing back via safe hardware mutators (`vec_write_register`), 
we prevent the creation of highly-nested, memory-heavy Abstract Syntax Trees (ASTs). Math occurs 
entirely within the L1 cache boundaries defined by `VCL`.

---

## 2. O(1) Verb Resolution

```c
PrimitiveOp reduction_resolve_op(const char* verb);
```

Currently, verb resolution relies on aligned memory string comparisons (`strcmp`). 

> [!TIP]
> **Future Roadmap**
> For maximum `O(1)` speed in Phase 5, this function is designed to be upgraded to a **Compile-Time Perfect Hash** (such as `gperf`). This will compile strings like `"plus"` into mathematical integers (e.g., `0`) at build time, completely bypassing string comparisons during the execution hot-loop.

---

## 3. Thermodynamic Cost Tracking

Every instruction in LLFPL1 carries a Thermodynamic Cost. This mathematically models execution 
time without relying on the host machine's physical timer, making the VM highly deterministic.

```c
vec_tick_clock(v, 1);
```

The primitive ALUs (`plus`, `minus`, `multiply`, `divide`) are currently weighted with a standard cost 
of `1` cycle. This deterministic clock tracks precisely how much work the script is doing, making infinite 
loop detection perfectly predictable.

---

## 4. Hardware Safe Mutation

To honor the strict **Opaque Pointer** encapsulation of `VEC`, the Reduction Core never reaches into 
the `VEC` struct directly. 

Instead of doing:
```c
v->r[target] = result; /* UNSAFE, breaks OOP walls */
```

We do:
```c
vec_write_register(v, target_reg, result); /* SAFE */
```

This guarantees that `vec.c` can independently perform out-of-bounds checks, memory alignment audits, 
and state bitmasking (`dirty_mask`) safely without the Reduction Core needing to know how memory is laid out.

---

## 5. Hardware Safe Division

Standard division by zero in C triggers a SIGFPE (Signal Floating-Point Exception), which interrupts 
the host OS and abruptly crashes the entire runtime environment.

```c
result = (b != 0.0) ? (a / b) : 0.0; 
```

By safely absorbing `b == 0.0` at the software layer, we ensure that malicious or improperly formatted 
scripts can never crash the parent interpreter.
