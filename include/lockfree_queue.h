#ifndef LOCKFREE_QUEUE
#define LOCKFREE_QUEUE
#include "atomic.h"
#include <stdint.h>

#define LOCKFREE_PAGE_SIZE           (1 << 10 << 2)

#define LOCKFREE_BUFOBJ_DEFAULT_SIZE sizeof(void *)

struct lockfree_buffer_obj {
    void *val;
};

struct lockfree_buffer {
    atomic_t                   lfb_ffree;
    atomic_t                   lfb_fused;
    atomic_t                   lfb_used_count;
    struct lockfree_buffer_obj lfb_buf[0];
};

struct lockfree_queue {
    uint32_t                lfq_bufsize;
    struct lockfree_buffer *lfq_buf;
};

typedef struct lockfree_buffer_obj lockfree_obj_t;
typedef struct lockfree_buffer     lockfree_buf_t;
typedef struct lockfree_queue      lockfree_que_t;
#endif