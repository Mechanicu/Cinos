#include "lockfree_queue.h"
#include "atomic.h"
#include "log.h"
#include <string.h>

lockfree_que_t *lockfree_que_create(
    const uint32_t que_max_obj_count)
{
    /*alloc lockfree queue*/
    lockfree_que_t *que = LOCKFREEQUE_MEM_ALLOC(sizeof(lockfree_que_t));
    if (!que) {
        return NULL;
    }

    /*alloc buffer for lockfree queue*/
    que->lfq_buf = LOCKFREEQUE_MEM_ALLOC(
        sizeof(lockfree_buf_t) + sizeof(lockfree_obj_t) * que_max_obj_count);
    if (!(que->lfq_buf)) {
        return NULL;
    }

    /*init*/
    que->lfq_max_obj_count      = que_max_obj_count;
    que->lfq_buf->lfb_read_pos  = 0;
    que->lfq_buf->lfb_write_pos = 0;
    return que;
}

int lockfree_enque(
    lockfree_que_t *que,
    lockfree_obj_t *wval)
{
    lockfree_buf_t *quebuf         = que->lfq_buf;
    uint32_t        write_pos      = 0;
    uint32_t        read_pos       = 0;
    uint32_t        next_write_pos = 0;
    /*get write position to store data for current producer*/
    do {
        write_pos      = quebuf->lfb_write_pos;
        read_pos       = quebuf->lfb_read_pos;
        next_write_pos = (write_pos + 1) % que->lfq_max_obj_count;
        /*no more space to write*/
        if (next_write_pos == read_pos) {
            return -1;
        }
    } while (!__sync_bool_compare_and_swap(&(quebuf->lfb_write_pos), write_pos, next_write_pos));

    /*write new obj val to write position*/
    quebuf->lfb_buf[write_pos].val = wval->val;
    /*ATTENTION:Its unsafe if only use code above. There is a situation that after updating write pos, the data hasn't
    finished write yet, and consumer started read the position. This situation is very danger, but considering that
    copying size is equal to register size, it may be hard to trigger such error*/

    // update the maximum read index after saving the data. It wouldn't fail if there is only one thread
    // inserting in the queue. It might fail if there are more than 1 producer threads because this
    // operation has to be done in the same order as the previous CAS
    while (!ATOMIC_CAS(&quebuf->lfb_max_read_pos, write_pos, next_write_pos)) {
        // this is a good place to yield the thread in case there are more
        // software threads than hardware processors and you have more
        // than 1 producer thread
        // have a look at sched_yield (POSIX.1b)
        sched_yield();
    }
    atomic_add(&(quebuf->count), 1);
    return 0;
}

int lockfree_deque(
    lockfree_que_t *que,
    lockfree_obj_t *rval)
{
    lockfree_buf_t *quebuf        = que->lfq_buf;
    uint32_t        read_pos      = 0;
    uint32_t        max_read_pos  = 0;
    uint32_t        write_pos     = 0;
    uint32_t        next_read_pos = 0;

    /*get current first read position*/
    do {
        // to ensure thread-safety when there is more than 1 producer thread
        // a second index is defined (m_maximumReadIndex)
        read_pos     = quebuf->lfb_read_pos;
        max_read_pos = quebuf->lfb_max_read_pos;

        if (read_pos == max_read_pos)   // 如果不为空，获取到读索引的位置
        {
            // the queue is empty or
            // a producer thread has allocate space in the queue but is
            // waiting to commit the data into it
            return -1;
        }
        // retrieve the data from the queue
        rval->val     = quebuf->lfb_buf[read_pos].val;

        // try to perfrom now the CAS operation on the read index. If we succeed
        // a_data already contains what m_readIndex pointed to before we
        // increased it
        next_read_pos = (read_pos + 1) % que->lfq_max_obj_count;
        if (ATOMIC_CAS(&(quebuf->lfb_read_pos), read_pos, next_read_pos)) {
            atomic_sub(&(quebuf->count), 1);   // 真正读取到了数据
            return 0;
        }
    } while (1);

    /*code should not reach here*/
    return -1;
}
