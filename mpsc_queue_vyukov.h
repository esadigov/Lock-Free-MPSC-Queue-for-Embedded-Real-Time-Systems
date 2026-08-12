#pragma once

#include <stdint.h>
#include "mpsc_config.h"

typedef struct mpsc_cell_t
{
    uint32_t seq;
    uintptr_t data;
} mpsc_cell_t;

typedef struct __attribute__((aligned(MPSC_DCACHE_LINE_SIZE))) mpsc_queue_t {
    /* Mostly read-only after init */
    mpsc_cell_t* buffer;
    uint32_t     capacity;
    uint32_t     mask;

    /* Consumer-only, hot */
    uint32_t     head;
    uint8_t      _pad_head[MPSC_DCACHE_LINE_SIZE - sizeof(uint32_t)];
    /* Now 'tail' will be exactly one cache line after 'head'. */

    /* Producers shared, hot */
    uint32_t     tail;
    uint8_t      _pad_tail[MPSC_DCACHE_LINE_SIZE - sizeof(uint32_t)];
    /* Extra cache line of "breathing room" after tail (optional but cheap). */
} mpsc_queue_t;

typedef enum mpsc_queue_e {
    MPSC_QUEUE_OK,
    MPSC_QUEUE_FULL,
    MPSC_QUEUE_INIT_FAIL_POWER_2_REQUIRED,
    MPSC_QUEUE_INIT_FAIL_NULLPTR,
    MPSC_QUEUE_STATUS_NULL_PARAM,
    MPSC_QUEUE_STATUS_DATA_CORRUPTED,
    MPSC_QUEUE_PUSH_FAILED_ISR
} mpsc_queue_e;

mpsc_queue_e mpsc_queue_init(mpsc_queue_t* q, mpsc_cell_t* buffer, uint32_t capacity);

mpsc_queue_e mpsc_queue_push(mpsc_queue_t* q, uintptr_t data);

mpsc_queue_e mpsc_queue_push_isr(mpsc_queue_t* q, uintptr_t data, uint32_t retries);

uintptr_t mpsc_queue_pop(mpsc_queue_t* q);
