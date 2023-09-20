#include "mempool.h"
#include "bits.h"
#include "list.h"
#include "log.h"
#include <malloc.h>
#include <string.h>

#define MEMPOOL_CTRL_MAGIC       0x12345678
#define MEMPOOL_CTRL_ALLOC(size) malloc(size)
#define MEMPOOL_CTRL_FREE(ptr)   free(ptr)

#define MEMPOOL_MIN_SLAB_SIZE    8ull
#define MEMPOOL_MIN_SLAB_BITS    3ull

static inline unsigned long get_real_size_bits(unsigned long size)
{
    u32 slot = ulog2l(size);

    if ((size -= 1ul << slot) != 0) {
        slot++;
    }
    LOG_INFO("slot:%u, size:%u", slot, size);
    return slot > MEMPOOL_MIN_SLAB_BITS ? slot : MEMPOOL_MIN_SLAB_BITS;
}

static void *mempool_ctrl_alloc_default(void *ctrl, const unsigned long size)
{
    if (!ctrl) {
        return (void *)(0 - 1);
    }
    if (!size) {
        return NULL;
    }

    mempool_ctrl_t *handler   = (mempool_ctrl_t *)ctrl;
    unsigned long   slot      = get_real_size_bits(size);
    unsigned long   real_size = 1ull << slot;
    if (real_size > handler->remainsize || slot > handler->slab_count) {
        return NULL;
    }

    handler->remainsize -= real_size;
    LOG_INFO("expect size:%lu, real size:%lu, bitcount:%lu, remain size:%lu",
             size, real_size, slot, handler->remainsize);

    /* check slab*/
    unsigned long *alloc;
    if (handler->slabs[slot] != NULL) {
        alloc                = handler->slabs[slot];
        handler->slabs[slot] = (unsigned long *)(*alloc);
        LOG_DEBUG("alloc addr:%p, next slot:%p", alloc, handler->slabs[slot]);
        return (void *)alloc;
    }

    /* if no suitable slabs, then return*/
    alloc            = (unsigned long *)(handler->rvaddr);
    handler->rvaddr += real_size;
    LOG_DEBUG("alloc addr:%p, remain addr:%p", alloc, handler->rvaddr);
    return (void *)alloc;
}

static void mempool_ctrl_free_default(void *ctrl, void *ptr, const unsigned long size)
{
    if (!ctrl || !size) {
        return;
    }

    mempool_ctrl_t *handler = (mempool_ctrl_t *)ctrl;
    if ((unsigned long)ptr < (unsigned long)handler->svaddr ||
        (unsigned long)ptr > (unsigned long)(handler->rvaddr + handler->totalsize)) {
        return;
    }
    unsigned long slot       = get_real_size_bits(size);
    unsigned long real_size  = 1ull << slot;

    *(unsigned long *)ptr    = (unsigned long)(handler->slabs[slot]);
    handler->slabs[slot]     = ptr;
    handler->remainsize     += real_size;
    LOG_DEBUG("size:%lu, slot:%lu, ptr:%p, cur slab:%p, next slab:%lx",
              size, slot, ptr, handler->slabs[slot], *(unsigned long *)ptr);
}

mempool_ctrl_t *mempool_ctrl_init(
    void         *vaddr,
    unsigned long size,
    unsigned long slabs_count,
    void         *(*palloc)(void *ctrl, const unsigned long size),
    void          (*pfree)(void *ctrl, void *ptr, const unsigned long size))
{
    //
    mempool_ctrl_t *mpctrl =
        (mempool_ctrl_t *)MEMPOOL_CTRL_ALLOC(sizeof(mempool_ctrl_t) + slabs_count * sizeof(void *));
    memset(mpctrl, 0, sizeof(mempool_ctrl_t) + slabs_count * sizeof(void *));

    if (mpctrl == NULL) {
        perror("Alloc mempool ctrl failed");
        return NULL;
    }
    mpctrl->magic      = MEMPOOL_CTRL_MAGIC;
    mpctrl->rvaddr     = vaddr;
    mpctrl->svaddr     = vaddr;
    mpctrl->totalsize  = size;
    mpctrl->remainsize = size;
    mpctrl->slab_count = slabs_count;
    if (!pfree || !palloc) {
        mpctrl->ops.palloc = mempool_ctrl_alloc_default;
        mpctrl->ops.pfree  = mempool_ctrl_free_default;
    } else {
        mpctrl->ops.palloc = palloc;
        mpctrl->ops.pfree  = pfree;
    }
    list_init(&mpctrl->hook);

    LOG_DEBUG("Alloc mempool ctrl at %p, addr:%p, totalsize:%lu, slab count:%lu, pfree:%p, palloc:%p, magic:%lx",
              mpctrl, (void *)mpctrl->svaddr, mpctrl->totalsize, mpctrl->slab_count, mpctrl->ops.pfree,
              mpctrl->ops.palloc, mpctrl->magic);
    return mpctrl;
}

void mempool_ctrl_destroy(mempool_ctrl_t **ctrl)
{
    MEMPOOL_CTRL_FREE(*ctrl);
    *ctrl = NULL;
}

mempool_ctrl_t *mempool_ctrl_create(
    unsigned long pages,
    unsigned long slabs_count,
    void         *(*palloc)(void *ctrl, const unsigned long size),
    void          (*pfree)(void *ctrl, void *ptr, const unsigned long size))
{
}