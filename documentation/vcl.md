# VCL (Vector Compute Layer) — Complete Technical Documentation

> **Source**: [`vcl.h`](../include/vcl.h) · [`vcl.c`](../src/vcl.c)
>
> Single source of truth for the hardware discovery layer's architecture,
> algorithm, security model, correctness proofs, and tradeoffs.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture: Layered Platform Abstraction](#2-architecture)
3. [Structural Optimisation: HardwareProfile Packing](#3-structural-optimisation)
4. [Detection Algorithm: Cascading Platform Probe](#4-detection-algorithm)
5. [Power-of-Two Validation Guard](#5-power-of-two-guard)
6. [Encapsulation & API Design](#6-encapsulation)
7. [Correctness Proofs](#7-correctness-proofs)
8. [Complexity Analysis](#8-complexity-analysis)
9. [Security Analysis](#9-security-analysis)
10. [Failure Modes & Fault Boundaries](#10-failure-modes)
11. [Non-Obvious Design Decisions](#11-non-obvious)
12. [Platform Coverage Matrix](#12-platform-coverage)
13. [Tradeoff Summary Matrix](#13-tradeoff-matrix)

---

## 1. Overview

VCL is the hardware discovery layer for the LLFPL1 interpreter. It probes the
host system at boot to produce a `HardwareProfile` — a snapshot of the two
physical parameters that all downstream subsystems depend on:

- **`physical_regs`** — number of vector registers the execution engine virtualises
- **`cache_line_size`** — the alignment boundary for all cache-sensitive allocations

These values flow through the entire system:

```
vcl_discover() → HardwareProfile → vec_init()  → register array alignment
                                 → registry.c  → entry slab alignment (posix_memalign)
```

### Design Requirements

- **Zero runtime overhead** — probe runs once at boot; no cost during execution
- **Cross-platform correctness** — must return valid values on macOS, Linux, and
  any POSIX system, on both x86_64 and ARM64
- **Fail-safe defaults** — if any probe fails, fall back to safe, conservative
  values that are correct on the widest range of hardware
- **Downstream safety** — `cache_line_size` is used as the alignment argument to
  `posix_memalign`, which requires a power of two ≥ `sizeof(void*)`

---

## 2. Architecture

### Problem: Why Is Hardware Detection Non-Trivial?

Each CPU architecture uses a fundamentally different mechanism to expose its
hardware parameters:

| Architecture | Detection Mechanism       | Accessibility                  |
|:-------------|:--------------------------|:-------------------------------|
| x86_64       | `CPUID` assembly instruction | User-mode, direct hardware query |
| ARM64 (macOS)| `sysctlbyname` kernel API | User-mode, OS-mediated          |
| ARM64 (Linux)| `sysconf` POSIX API       | User-mode, OS-mediated          |
| ARM64 (bare) | `CTR_EL0` system register | Requires EL1 (kernel mode)     |
| RISC-V       | Device tree / CSR reads   | Platform-dependent              |

A single detection strategy cannot cover all platforms. The solution is a
**cascading platform probe** — a compile-time dispatch to the correct OS-level
API, with a guaranteed safe fallback.

### Solution: Three-Tier Detection Cascade

```
┌─────────────────────────────────────────────────────────────┐
│  Tier 1: OS-Level API (macOS / Linux)                       │
│  • macOS: sysctlbyname("hw.cachelinesize")                  │
│  • Linux: sysconf(_SC_LEVEL1_DCACHE_LINESIZE)               │
│  Most reliable — OS has direct kernel access to CPU data     │
├─────────────────────────────────────────────────────────────┤
│  Tier 2: Architecture-Level Instruction (x86_64)            │
│  • __get_cpuid(1, ...) → CLFLUSH line size from EBX         │
│  Used only when no OS-level API is available                 │
├─────────────────────────────────────────────────────────────┤
│  Tier 3: Compile-Time Constants                             │
│  • DEFAULT_CACHE_LINE = 64                                  │
│  • DEFAULT_REGS = 16                                        │
│  Unconditionally safe fallback — never causes incorrect     │
│  behaviour, at most causes suboptimal alignment              │
└─────────────────────────────────────────────────────────────┘
```

The cascade is resolved entirely at **compile time** via `#if`/`#elif` — zero
runtime branching overhead. Only one tier's code exists in the final binary.

### Why OS-Level APIs Take Priority Over CPUID

On macOS x86_64 (Intel Macs), both `__APPLE__` and `__x86_64__` are defined.
The `#if defined(__APPLE__)` check appears first, so `sysctlbyname` is used
instead of `CPUID`. This is correct because:

1. `sysctlbyname` returns the value the OS actually uses for alignment — which
   may differ from the raw CPUID value on virtualised or emulated systems
2. On Apple Silicon (ARM64), `__x86_64__` is not defined, so the macOS path is
   the *only* path — making the priority moot but consistent

---

## 3. Structural Optimisation

### HardwareProfile Struct Layout

```
Offset  Size  Field            Type
──────  ────  ──────────────── ─────────
 0       2    physical_regs    uint16_t
 2       2    cache_line_size  uint16_t
──────  ────
        4     TOTAL — zero padding bytes
```

The struct is exactly **4 bytes** with zero padding, because both fields are
`uint16_t` (2-byte aligned) and are placed contiguously.

### Why `uint16_t` and Not `uint32_t` or `size_t`?

| Type      | Size (LP64)  | Size (ILP32) | Max Value       |
|-----------|:-------------|:-------------|:----------------|
| `size_t`  | 8 bytes      | 4 bytes      | 2⁶⁴ or 2³²      |
| `uint32_t`| 4 bytes      | 4 bytes      | 4,294,967,295    |
| `uint16_t`| 2 bytes      | 2 bytes      | 65,535           |

Cache line sizes will never exceed 256 bytes (currently 64-128). Register counts
will never exceed 32 on modern architectures. `uint16_t` provides massive headroom
while cutting the struct size in half to 4 bytes compared to `uint32_t` (8 bytes).

### Return-by-Value

`HardwareProfile` is 4 bytes — smaller than a pointer + dereference on 64-bit
systems. Returning by value avoids heap allocation, avoids ownership ambiguity,
and allows the compiler to return the struct in registers (SysV AMD64 ABI
returns structs ≤ 16 bytes in `RAX:RDX`).

---

## 4. Detection Algorithm

### 4.1 — macOS: `sysctlbyname`

```c
sysctlbyname("hw.cachelinesize", &line_size, &len, NULL, 0)
```

The XNU kernel exposes hardware parameters via the sysctl MIB namespace.
`"hw.cachelinesize"` reads the L1 data cache line size directly from the
kernel's hardware description, populated at boot from IOKit device tree data.

**Validation**: The result must be (a) non-zero and (b) a power of two.

### 4.2 — Linux: `sysconf`

```c
sysconf(_SC_LEVEL1_DCACHE_LINESIZE)
```

POSIX `sysconf` reads from `/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size`
on modern Linux kernels. This is architecture-independent — works on x86_64,
ARM64 (aarch64), RISC-V, MIPS, and any platform Linux supports.

**Validation**: The result must be (a) positive (`long > 0`) and (b) a power of two.

### 4.3 — x86_64 Fallback: `CPUID`

```c
__get_cpuid(1, &eax, &ebx, &ecx, &edx)
cache_line_size = ((ebx >> 8) & 0xFF) * 8
```

CPUID leaf 1, bits 15:8 of EBX contain the CLFLUSH line size in units of 8 bytes.
Multiplying by 8 gives the line size in bytes.

**Validation**: The result must be (a) non-zero and (b) a power of two.

### 4.4 — Universal Fallback

If all platform-specific probes fail or are unavailable (e.g., FreeBSD on ARM),
the function returns `DEFAULT_CACHE_LINE = 64`.

**Why 64?** It is the most common cache line size across all modern architectures:

| Architecture     | Typical Cache Line Size |
|:-----------------|:-----------------------:|
| Intel x86_64     | 64 bytes                |
| AMD x86_64       | 64 bytes                |
| Apple M1/M2/M3/M4| 128 bytes              |
| ARM Cortex-A     | 64 bytes                |
| RISC-V           | 64 bytes (common)       |

Using 64 as default means we are correct or conservatively under-aligned. An
under-aligned allocation never causes **incorrect** behaviour — it may only
cause slightly suboptimal cache utilisation. An over-aligned allocation wastes
memory but is also safe.

---

## 5. Power-of-Two Validation Guard

### The Critical Invariant

`cache_line_size` is passed downstream to `posix_memalign(ptr, alignment, size)`.
POSIX.1-2001 §posix_memalign states:

> The value of alignment shall be a power of two **and** a multiple of
> `sizeof(void*)`.

If `cache_line_size` is not a power of two, `posix_memalign` returns `EINVAL`
and the allocation fails. This could crash the interpreter.

### The Guard

```c
static int is_power_of_two(uint16_t v)
{
    return v != 0 && (v & (v - 1)) == 0;
}
```

**Mathematical Proof** that `v & (v - 1) == 0` ⟺ `v` is a power of two:

Let v = 2ᵏ for some k ≥ 0.

- **Binary representation**: v = `1` followed by k zeros: `100...0`
- **v − 1**: flips the leading 1 and sets all lower bits: `011...1`
- **v & (v − 1)**: every bit position differs → result is `000...0` = 0 ✓

Conversely, if v is not a power of two, it has at least two set bits. Subtracting
1 cannot clear all of them. Therefore `v & (v − 1) ≠ 0` ✓

The `v != 0` check handles the edge case: 0 is not a power of two, but
`0 & (0 − 1) = 0 & 0xFFFFFFFF = 0`, which would falsely pass without the guard.

### Downstream Safety Chain

```
probe returns value → is_power_of_two validates → passed to posix_memalign
         ↓ (fail)                                          ↓ (guaranteed valid)
  DEFAULT_CACHE_LINE (64)                           alignment is safe ✓
```

No invalid alignment can reach `posix_memalign`. This invariant is enforced by
construction, not by documentation.

---

## 6. Encapsulation

### Visibility Table

| Symbol                  | Visibility | Location  |
|-------------------------|:----------:|:---------:|
| `HardwareProfile`       | **Public** | `vcl.h`   |
| `vcl_discover()`        | **Public** | `vcl.h`   |
| `DEFAULT_REGS`          | Private    | `vcl.c`   |
| `DEFAULT_CACHE_LINE`    | Private    | `vcl.c`   |
| `is_power_of_two()`     | `static`   | `vcl.c`   |
| `probe_cache_line_size()`| `static`  | `vcl.c`   |

**Consequence**: The detection strategy (which OS APIs, which fallback values,
which validation) can change without recompiling any other file. Only `vcl.c`
needs recompilation.

### Single Responsibility (SOLID — S)

| Function                  | Single Responsibility                        |
|---------------------------|----------------------------------------------|
| `is_power_of_two()`       | Validate a single integer property           |
| `probe_cache_line_size()` | Detect cache line size from platform APIs     |
| `vcl_discover()`          | Assemble and return the complete profile      |

No function does two things. `vcl_discover` does not probe — it delegates to
`probe_cache_line_size`. `probe_cache_line_size` does not validate the final
profile — it only returns a validated cache line value.

### Open/Closed Principle (SOLID — O)

Adding support for a new platform (e.g., FreeBSD) requires adding one `#elif`
block inside `probe_cache_line_size`. No existing code is modified. The function
signature, return type, and validation logic remain identical.

### `(void)` Function Signatures

```c
HardwareProfile vcl_discover(void);   /* correct */
HardwareProfile vcl_discover();       /* deprecated */
```

In C (unlike C++), empty parentheses `()` mean "unspecified number and type of
arguments" — not "no arguments" (C11 §6.11.6). This is a deprecated feature
scheduled for removal. Using `(void)` explicitly states "this function takes
zero arguments", enabling the compiler to diagnose accidental argument passing.

---

## 7. Correctness Proofs

### 7.1 — Probe Always Returns a Valid Power of Two

**Claim**: `probe_cache_line_size()` returns a value that is a power of two ≥ 1.

**Proof**:
- **Case A** (OS probe succeeds and passes validation): The `is_power_of_two`
  check guarantees the returned value satisfies `v > 0 ∧ v & (v−1) = 0`.
  By the biconditional proof in §5, this means v = 2ᵏ for some k ≥ 0. ✓
- **Case B** (OS probe fails or returns invalid value): The function returns
  `DEFAULT_CACHE_LINE = 64 = 2⁶`. Since 64 is a power of two, the invariant
  holds. ✓
- **Case C** (no platform macro defined): No probe code is compiled. The function
  body is empty except `return DEFAULT_CACHE_LINE`. Same as Case B. ✓

∴ For all possible execution paths, the return value is a power of two. ∎

### 7.2 — `posix_memalign` Preconditions Are Met

**Claim**: Every call to `posix_memalign(ptr, profile.cache_line_size, size)` in
the system receives a valid alignment argument.

**Proof**: `profile.cache_line_size` is set by `probe_cache_line_size()`, which
(by §7.1) always returns a power of two. The minimum returned value is 64, and
`sizeof(void*) ∈ {4, 8}` on all supported platforms. Since 64 ≥ 8 ≥ 4 and
64 is a multiple of both, the POSIX alignment requirements are satisfied. ✓

### 7.3 — No Undefined Behaviour

| Potential UB              | Why It Cannot Occur                              |
|---------------------------|--------------------------------------------------|
| Integer overflow in `v-1` | `is_power_of_two` checks `v != 0` first          |
| Signed integer overflow   | All values are `uint16_t` (unsigned)              |
| Shift overflow in CPUID   | `(ebx >> 8)` shifts a 32-bit value by 8 — valid  |
| NULL dereference          | No pointers are dereferenced                      |
| Uninitialised memory      | All locals are initialised before use             |

### 7.4 — Return-by-Value Safety

**Claim**: Returning `HardwareProfile` by value cannot leak, alias, or dangle.

**Proof**: The struct is 8 bytes, contains no pointers, and is fully initialised
before return. The caller receives a copy. There is no shared mutable state,
no heap allocation, and no lifetime dependency. ✓

---

## 8. Complexity Analysis

### Time

| Operation                  | Complexity   | Rationale                          |
|----------------------------|:------------:|:-----------------------------------|
| `is_power_of_two()`        | O(1)         | Two arithmetic ops, one comparison |
| `probe_cache_line_size()`  | O(1)         | Single syscall or instruction      |
| `vcl_discover()`           | O(1)         | One probe + one printf             |
| **Total boot cost**        | **O(1)**     | Called exactly once                |

### Space

| Component            | Size                            |
|----------------------|---------------------------------|
| `HardwareProfile`    | 4 bytes (stack, returned by value) |
| Probe locals         | ≤ 24 bytes (stack)              |
| Static code          | ~200 bytes (one platform path)  |
| **Total**            | **< 256 bytes**                 |

### Runtime Overhead During Execution

**Zero**. `vcl_discover` is called once during boot. The returned
`HardwareProfile` is a plain value on the stack. No function pointer,
no virtual dispatch, no global state lookup occurs during interpreter execution.

---

## 9. Security Analysis

### 9.1 — Trusted Input Only

`vcl_discover` takes no input from the user or the source file. It reads only
from the kernel (via syscall) or the CPU (via CPUID). Both are trusted sources
that cannot be influenced by untrusted LLFPL1 source code.

### 9.2 — Denial of Service via Hardware Spoofing

**Threat**: A compromised kernel returns `cache_line_size = 0` or a non-power-of-two.

**Mitigation**: The `is_power_of_two` guard rejects invalid values and falls back
to the safe default. Even under kernel compromise, the interpreter does not crash.

### 9.3 — Information Leakage

The `printf` output reveals the host's cache line size. This is standard
diagnostic output. In a security-sensitive deployment, the print could be
removed or guarded behind a verbosity flag without changing any behaviour.

### 9.4 — No Dynamic Memory

`vcl_discover` allocates nothing on the heap. There are no buffers, no strings,
no user-controlled sizes. The attack surface is zero.

---

## 10. Failure Modes

| Failure                       | Detection              | Behaviour                | Recovery  |
|-------------------------------|------------------------|--------------------------|:---------:|
| `sysctlbyname` returns error | Return value ≠ 0       | Falls back to default 64 | ✓         |
| `sysctlbyname` returns 0     | `line_size > 0` check  | Falls back to default 64 | ✓         |
| `sysctlbyname` returns 48    | `is_power_of_two` fails| Falls back to default 64 | ✓         |
| `sysconf` returns -1         | `val > 0` check        | Falls back to default 64 | ✓         |
| `sysconf` returns 0          | `val > 0` check        | Falls back to default 64 | ✓         |
| `__get_cpuid` returns 0      | Return value check     | Falls back to default 64 | ✓         |
| CPUID returns 0 in EBX[15:8] | `line_size > 0` check  | Falls back to default 64 | ✓         |
| No platform macro defined    | Compile-time           | Falls back to default 64 | ✓         |

**Fail-safe principle**: Every failure path produces the same safe output
(64-byte cache line). There is no failure mode that causes incorrect behaviour —
only suboptimal performance on exotic hardware.

---

## 11. Non-Obvious Design Decisions

### 11.1 — `probe_cache_line_size` Is a Separate `static` Function

The probe logic could live directly inside `vcl_discover`. Separating it:
1. Makes `vcl_discover` a pure assembler of the profile — it does not know *how*
   the cache line is detected
2. Allows testing the probe in isolation (in a future test harness)
3. Keeps the `#if`/`#elif` preprocessor maze out of the public-facing function

### 11.2 — `physical_regs = 16` Is Hardcoded, Not Probed

The number 16 is the register count of the *virtual machine*, not the host CPU.
On x86_64, there are 16 GPRs (RAX-R15). On ARM64, there are 31 GPRs (X0-X30)
and 32 SIMD registers (V0-V31). The virtual machine standardises on 16 to match
the minimum common denominator. Dynamic detection would require the VEC to
handle variable register counts — unnecessary complexity for the current
single-architecture virtual machine design.

### 11.3 — `#if defined()` Instead of `#ifdef`

`#if defined(X)` and `#ifdef X` are functionally identical for a single macro.
However, `#if defined()` composes with `&&` and `||`:

```c
#if defined(__APPLE__) && defined(__arm64__)   /* valid   */
#ifdef __APPLE__ && __arm64__                  /* invalid */
```

Using `#if defined()` consistently throughout the file avoids mixing styles and
makes future multi-condition checks trivial.

### 11.4 — Why Not `getauxval(AT_DCACHEBSIZE)` on Linux?

`getauxval(AT_DCACHEBSIZE)` reads from the ELF auxiliary vector — closer to the
hardware than `sysconf`. However:
- It is Linux-specific (not POSIX)
- `AT_DCACHEBSIZE` is only defined on PowerPC in some glibc versions
- `sysconf(_SC_LEVEL1_DCACHE_LINESIZE)` is POSIX-portable and works on all Linux
  architectures

### 11.5 — Apple Silicon Returns 128, Not 64

Apple M-series processors (M1, M2, M3, M4) use **128-byte cache lines** — double
the x86_64 standard. Our dynamic probe correctly detects this. If we had only
the x86_64 CPUID path, we would incorrectly hardcode 64 on Apple Silicon,
causing every `posix_memalign` to under-align by 50%, wasting half of each cache
line fetch.

This was empirically confirmed:
```
[VCL] PROBE: 16 Registers | 128-byte Cache Alignment
```

---

## 12. Platform Coverage Matrix

| Platform              | Arch    | Probe Tier | Detected Value  | Status |
|:----------------------|:--------|:-----------|:---------------|:------:|
| macOS (Apple Silicon) | ARM64   | Tier 1     | 128 (dynamic)  | ✓      |
| macOS (Intel)         | x86_64  | Tier 1     | 64 (dynamic)   | ✓      |
| Linux (Intel/AMD)     | x86_64  | Tier 1     | 64 (dynamic)   | ✓      |
| Linux (Raspberry Pi)  | ARM64   | Tier 1     | 64 (dynamic)   | ✓      |
| Linux (RISC-V)        | RISC-V  | Tier 1     | 64 (dynamic)   | ✓      |
| FreeBSD (x86_64)      | x86_64  | Tier 2     | 64 (CPUID)     | ✓      |
| FreeBSD (ARM64)       | ARM64   | Tier 3     | 64 (default)   | ✓*     |
| Windows (x86_64)      | x86_64  | Tier 2     | 64 (CPUID)     | ✓      |
| Windows (ARM64)       | ARM64   | Tier 3     | 64 (default)   | ✓*     |
| Unknown OS / Arch     | Any     | Tier 3     | 64 (default)   | ✓*     |

`*` = correct for most hardware; conservative on Apple Silicon

---

## 13. Tradeoff Summary Matrix

| Decision                           | What We Gain                            | What We Accept                           |
|------------------------------------|-----------------------------------------|------------------------------------------|
| OS-level API over raw CPUID        | Works on all architectures per OS       | Platform-specific `#if` blocks           |
| Cascading `#if`/`#elif` dispatch   | Zero runtime branching overhead         | Only one tier compiled per binary        |
| Power-of-two validation guard      | Guarantees `posix_memalign` safety      | Rejects exotic (valid but non-pow2) sizes|
| Default 64-byte fallback           | Safe on 95%+ of hardware                | Under-aligned on Apple Silicon if probe fails |
| Hardcoded `physical_regs = 16`     | Simple, deterministic VM design         | Not adaptive to ARM64's 31 GPRs          |
| Return by value (4 bytes)          | No heap, no ownership, no lifetime      | Struct copied on return (trivial cost)   |
| `(void)` function signatures       | C11 correct, compiler-enforced safety   | Slightly more verbose declarations       |
| `static` helper functions          | Full encapsulation in `vcl.c`           | Cannot be called from test files directly|
| `printf` for diagnostics           | Zero dependencies                       | Not suitable for library extraction      |
| Separate `probe_cache_line_size()` | SRP, testable, isolates #if complexity  | One extra function call (inlined by -O2) |
