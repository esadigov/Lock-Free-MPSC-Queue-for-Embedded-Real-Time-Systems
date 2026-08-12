# Lock-Free MPSC Queue (Vyukov)

A bounded, lock-free **Multi-Producer / Single-Consumer** queue in portable C11,
built for embedded and real-time systems. Based on Dmitry Vyukov's bounded-queue
algorithm, specialized for the MPSC case (the single consumer skips the CAS on the
read path).

## Features

- **Lock-free** enqueue via C atomic CAS with correct acquire/release ordering — no mutexes, no blocking.
- **Zero allocation** — the caller supplies the backing buffer; works with no heap.
- **ISR-safe** `push_isr` variant with a bounded retry count for interrupt context.
- **Cache-line aware** — `head`/`tail` are padded onto separate lines and the struct is cache-line aligned to avoid false sharing (line size configurable in `mpsc_config.h`).
- **Portable** across 32-/64-bit hosts, x86-64, and SPARC LEON3 (GR712RC).

## API

```c
mpsc_queue_e mpsc_queue_init(mpsc_queue_t* q, mpsc_cell_t* buffer, uint32_t capacity);
mpsc_queue_e mpsc_queue_push(mpsc_queue_t* q, uintptr_t data);
mpsc_queue_e mpsc_queue_push_isr(mpsc_queue_t* q, uintptr_t data, uint32_t retries);
uintptr_t    mpsc_queue_pop(mpsc_queue_t* q);   /* returns UINTPTR_MAX when empty */
```

`capacity` must be a power of two. `UINTPTR_MAX` is reserved as the empty sentinel
and rejected as input.

## Usage

```c
mpsc_cell_t  buf[16];
mpsc_queue_t q;
mpsc_queue_init(&q, buf, 16);

mpsc_queue_push(&q, (uintptr_t)ptr);       /* producer thread(s) */

uintptr_t v = mpsc_queue_pop(&q);          /* single consumer */
if (v != UINTPTR_MAX) { /* got an item */ }
```

## Requirements

C11 toolchain with GCC/Clang atomic builtins. Lock-free operation requires an
architecture with native atomics (Cortex-M3/M4/M7 and up — **not** M0). Set the
platform macro (e.g. `-DMPSC_PLATFORM_X86_64`) to select the right cache-line size.
