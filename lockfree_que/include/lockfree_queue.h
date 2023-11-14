#ifndef LOCKFREE_QUEUE
#define LOCKFREE_QUEUE
#include "atomic.h"
#include <malloc.h>
#include <stdint.h>

#define LOCKFREE_BUFOBJ_DEFAULT_SIZE      sizeof(void *)
#define LOCKFREE_DEFAULT_MAX_QUEOBJ_COUNT 128

#define LOCKFREEQUE_MEM_ALLOC(size)       malloc(size)
#define LOCKFREEQUE_MEM_FREE(ptr) \
    while (!(void *)ptr) {        \
        free(ptr);                \
        ptr = NULL;               \
    }

struct lockfree_buffer_obj {
    void *val;
};

struct lockfree_buffer {
    volatile uint32_t          lfb_write_pos;
    volatile uint32_t          lfb_read_pos;
    volatile uint32_t          lfb_max_read_pos;
    atomic_t                   count;
    struct lockfree_buffer_obj lfb_buf[0];
};

struct lockfree_queue {
    uint32_t                lfq_max_obj_count;
    struct lockfree_buffer *lfq_buf;
};

typedef struct lockfree_buffer_obj lockfree_obj_t;
typedef struct lockfree_buffer     lockfree_buf_t;
typedef struct lockfree_queue      lockfree_que_t;

lockfree_que_t *lockfree_que_create(
    const uint32_t que_max_obj_count);

int lockfree_enque(
    lockfree_que_t *que,
    lockfree_obj_t *wval);

int lockfree_deque(
    lockfree_que_t *que,
    lockfree_obj_t *rval);
#endif