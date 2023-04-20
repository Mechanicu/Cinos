#ifndef _LOCKFREE_QUEUE_H_
#define _LOCKFREE_QUEUE_H_

#include "assert.h"
#include "list.h"
#include "log.h"
#include <atomic>

#define LOCKFREE_CHUNK_ALLOC(size) malloc(size)
#define LOCKFREE_CHUNK_FREE(ptr)   free(ptr)

template <typename T>
class atomic_ptr_t
{
public:
    // unatomic, set val to ptr
    inline void set(T *ptr);
    // atomic,  set new val and return old val
    inline T   *xchg(T *val);
    //  Perform atomic 'compare and swap' operation on the pointer.
    //  The pointer is compared to 'cmp' argument and if they are
    //  equal, its value is set to 'val_'. Old value of the pointer
    //  is returned.
    inline T   *cas(T *cmp, T *val);
private:
    volatile T *ptr;
};

// queue only data and ops, not threads safe
template <typename T, int N>
class yqueue_t
{
public:
    inline yqueue_t();
    inline ~yqueue_t();

    // return reference of first element in queue
    inline T &front()
    {
        return begin_chunk->value[begin_pos];
    }

    // Returns reference to the back element of the queue.
    // If the queue is empty, behaviour is undefined.
    inline T &back()
    {
        return back_chunk->value[back_pos];
    }

    // make back_pos/chunk point to next avaliable pos
    inline void push()
    {
        back_chunk = end_chunk;
        back_pos   = end_pos;

        // if cur chunk still have space
        if (++end_pos != N) {
            return;
        }

        // check if this is spare_chunk
        chunk_t *sc = last_spare_chunk.xchg(NULL);
        if (!sc) {
            // if no spare chunk, then alloc new one
            sc = (chunk_t *)LOCKFREE_CHUNK_ALLOC(sizeof(chunk_t));
            ALLOC_ASSERT(sc)
        }
        list_add(&(sc->hook), &(end_chunk->hook));

        end_chunk = container_of(end_chunk->hook.next, chunk_t, hook);
        end_pos   = 0;
    }

    // pop first element from queue
    inline void pop()
    {
        if (++begin_pos == N) {
            // remove previous spare chunk, if non-null, then free it
            chunk_t *oc = last_spare_chunk.xchg(begin_chunk);
            LOG_DEBUG("pre sc:%p, cur sc:%p", oc, begin_chunk);
            if (oc) {
                LOCKFREE_CHUNK_FREE(oc);
            }
            oc          = begin_chunk;
            begin_chunk = container_of(&(oc->hook), chunk_t, tail);
            // remove cur spare chunk from list
            list_del_init(&(oc->hook));
            begin_pos = 0;
        }
    }

    inline void unpush()
    {
        return;
    }
private:
    // to avoid too much free/alloc ops, alloc N member each time
    // malloc/free itself also need lock, this will cause more time
    struct chunk_t {
        list_t hook;
        T      value[0];
    };

    // to first chunk
    chunk_t *begin_chunk;
    // to last chunk, ususally the same as back_chunk
    chunk_t *end_chunk;
    // to chunk which last element in
    chunk_t *back_chunk;
    // pos of first element of whole queue in begin_chunk
    int      begin_pos;
    // mostly equal to back_pos + 1, used to judge if need to alloc new chunk or not
    int      end_pos;
    // pos of last element of whole queue in last chunck
    int      back_pos;

    // last spare chunk, hasn't been freed
    atomic_ptr_t<chunk_t> last_spare_chunk;

    // disable copying of yqueue
    yqueue_t(const yqueue_t &);
    yqueue_t &operator=(const yqueue_t &);
};

// single read-write safe lockfree queue
template <typename T, size_t N>
class ypipe_t
{
public:
    inline ypipe_t();
    inline virtual ~ypipe_t();

    //  Write an item to the pipe.  Don't flush it yet. If incomplete is
    //  set to true the item is assumed to be continued by items
    //  subsequently written to the pipe. Incomplete items are neverflushed down the stream.
    inline void write(const T &data, bool incomplete)
    {
        //  Place the value to the queue, add new terminator element.
        LOG_DEBUG("write pos:%p, type:%d", &queue.back(), incomplete);
        queue.back() = data;
        queue.push();

        // if write is incomplete point flush_p to newest data
        if (!incomplete_) {
            f = &queue.back();   // 记录要刷新的位置
            LOG_DEBUG("write next flush pos:%p", f);
        }
    }

    //  Pop an incomplete item from the pipe. Returns true is such
    //  item exists, false otherwise.
    inline bool unwrite(T &data)
    {
        return;
    }

    //  Flush all the completed items into the pipe. Returns false if
    //  the reader thread is sleeping. In that case, caller is obliged to
    //  wake the reader up before using the pipe again.
    inline bool flush()
    {
        // if there are no un-flushed items, do nothing
        if (write_p == flush_p) {
            return;
        }

        // tru to set c to flush_p
        if (c.cas(write_p, flush_p) != write_p)

    }

    //  Check whether item is available for reading.
    inline bool check_read()
    {
        return;
    }

    //  Reads item from the pipe. Returns false if there is no value.
    //  available.
    inline bool read(T &data)
    {
        return;
    }
protected:
    //  Allocation-efficient queue to store pipe items.
    //  Front of the queue points to the first prefetched item, back of
    //  the pipe points to last un-flushed item. Front is used only by
    //  reader thread, while back is used only by writer thread.
    // ATTENTION:standard producer-consumer type
    yqueue_t<T, N> queue;

    //  Points to the first un-flushed item. This variable is used
    //  exclusively by writer thread.
    T *write_p;
    //  Points to the first un-prefetched item. This variable is used
    //  exclusively by reader thread.
    T *read_p;
    //  Points to the first item to be flushed in the future.
    T *flush_p;

    //  The single point of contention between writer and reader thread.
    //  Points past the last flushed item. If it is NULL,
    //  reader is asleep. This pointer should be always accessed using
    //  atomic operations.
    atomic_ptr_t<T> c;

    //  Disable copying of ypipe object.
    ypipe_t(const ypipe_t &);
    const ypipe_t &operator=(const ypipe_t &);
};
#endif