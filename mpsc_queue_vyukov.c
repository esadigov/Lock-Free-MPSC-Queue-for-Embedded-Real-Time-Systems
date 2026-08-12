#include "mpsc_queue_vyukov.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static inline int is_pow2(uint32_t x) { return x && !(x & (x - 1)); }

mpsc_queue_e mpsc_queue_init(mpsc_queue_t* q, mpsc_cell_t* buffer, uint32_t capacity) {
    if(q == NULL || buffer == NULL) {
        return MPSC_QUEUE_STATUS_NULL_PARAM;
    }

    if (!is_pow2(capacity)) {
        return MPSC_QUEUE_INIT_FAIL_POWER_2_REQUIRED;
    }

    q->buffer = buffer;
    q->capacity = capacity;
    q->mask = capacity - 1;
    q->head = 0;
    q->tail = 0;

    for (uint32_t i = 0; i < capacity; ++i) {
        q->buffer[i].data = 0;
        q->buffer[i].seq = i;
    }

    return MPSC_QUEUE_OK;
}

mpsc_queue_e mpsc_queue_push(mpsc_queue_t* q, uintptr_t data) {
    if(q == NULL) {
        return MPSC_QUEUE_STATUS_NULL_PARAM;
    }

    if(data == UINTPTR_MAX) {
        return MPSC_QUEUE_STATUS_DATA_CORRUPTED;
    }

    uint32_t pos;
    mpsc_cell_t* cell;

    for(;;) {
        pos = __atomic_load_n(&q->tail, __ATOMIC_RELAXED);
        cell = &q->buffer[pos & q->mask];

        uint32_t seq = __atomic_load_n(&cell->seq, __ATOMIC_ACQUIRE);
        /* Compute the distance in the counter's own width so it wraps
         * correctly at 2^32 regardless of intptr_t width (32- vs 64-bit). */
        int32_t diff = (int32_t)(seq - pos);

        if(diff == 0) {
            uint32_t new_pos = pos + 1;
            if(__atomic_compare_exchange_n(&q->tail, &pos, new_pos, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
                break;
            }
        } else if(diff < 0) {
            return MPSC_QUEUE_FULL;
        }
    }

    cell->data = data;

    __atomic_store_n(&cell->seq, pos + 1, __ATOMIC_RELEASE);

    return MPSC_QUEUE_OK;
}

mpsc_queue_e mpsc_queue_push_isr(mpsc_queue_t* q, uintptr_t data, uint32_t retries) {
    if(q == NULL) {
        return MPSC_QUEUE_STATUS_NULL_PARAM;
    }

    if(data == UINTPTR_MAX) {
        return MPSC_QUEUE_STATUS_DATA_CORRUPTED;
    }

    uint32_t pos;
    mpsc_cell_t* cell;

    for (uint32_t i = 0; i < retries; i++) {
        pos = __atomic_load_n(&q->tail, __ATOMIC_RELAXED);
        cell = &q->buffer[pos & q->mask];

        uint32_t seq = __atomic_load_n(&cell->seq, __ATOMIC_ACQUIRE);
        int32_t diff = (int32_t)(seq - pos);

        if (diff == 0) {
            uint32_t new_pos = pos + 1;

            if (__atomic_compare_exchange_n(&q->tail, &pos, new_pos, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
                cell->data = data;
                __atomic_store_n(&cell->seq, new_pos, __ATOMIC_RELEASE);
                return MPSC_QUEUE_OK;
            }
        } else if (diff < 0) {
            return MPSC_QUEUE_FULL;
        }
    }

    return MPSC_QUEUE_PUSH_FAILED_ISR;
}


uintptr_t mpsc_queue_pop(mpsc_queue_t* q) {
    if(q == NULL) {
        return UINTPTR_MAX;
    }

    uint32_t pos = q->head;
    mpsc_cell_t* cell = &q->buffer[q->mask & pos];

    uint32_t seq = __atomic_load_n(&cell->seq, __ATOMIC_ACQUIRE);
    int32_t diff = (int32_t)(seq - (pos + 1));

    if (diff == 0) {
        uintptr_t data = cell->data;
        
        __atomic_store_n(&cell->seq, pos + q->capacity, __ATOMIC_RELEASE);

        q->head = pos + 1;

        return data;
    }

    return UINTPTR_MAX;
}
