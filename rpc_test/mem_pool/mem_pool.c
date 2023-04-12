// 实现内存池
#include "../include/mem_pool.h"
#include <pthread.h>
#include <stdlib.h>

#define MEMPOOL_BLOCK_ALLOC(size) malloc(size)
#define MEMPOOL_BLOCK_FREE(ptr)   free(ptr)

#define MEMPOOL_ALLOC(size)       malloc(size)
#define MEMPOOL_FREE(ptr) \
    do {                  \
        free(ptr);        \
        (ptr) = NULL;     \
    } while (0)

static inline mempool_block_t *mempool_block_alloc(void *block_start, const unsigned long size)
{
    mempool_block_t *block = (mempool_block_t *)MEMPOOL_ALLOC(sizeof(mempool_block_t));
    if (block != NULL) {
        block->size        = size;
        block->block_start = block_start;
    }
    return block;
}

static inline mempool_block_t *mempool_block_insert_head(mempool_t *pool, mempool_block_t *block, const unsigned int type)
{
    list_insert_after(&(pool->block_lh[type]), &(block->hook));
    atomic_add(&(pool->list_len[type]), 1);
    return block;
}

static inline mempool_block_t *mempool_block_remove_head(mempool_t *pool, const unsigned int type)
{
    list_t          *head  = &(pool->block_lh[type]);
    // must avoid mulit threads request same block!!!!
    mempool_block_t *block = (mempool_block_t *)(head->next);
    if ((void *)block != (void *)head) {
        list_remove(&(block->hook));
        atomic_sub(&(pool->list_len[type]), 1);
        return block;
    }
    return NULL;
}

static inline void mempool_init(mempool_t *pool, const unsigned long pool_size)
{
    //
    LOG_DEBUG("mempool_init\n");
    for (int i = 0; i < MEMBLOCK_LH_COUNT; i++) {
        LOG_DEBUG("init mempool:%d\n", i);
        atomic_init(&(pool->list_len[i]));
        list_init(&(pool->block_lh[i]));
    }
    list_init(&(pool->pool_hook));
    pool->pool_free_size = 0;
    pool->pool_size      = pool_size;
    pool->pool_start     = 0;
}

mempool_t *mempool_create(const unsigned long block_size, unsigned long block_count, void *(*block_alloc)(unsigned long *size))
{
    if (!block_size || !block_count || !block_alloc) {
        return NULL;
    }
    unsigned long real_block_size = block_size;
    mempool_t    *pool            = (mempool_t *)MEMPOOL_ALLOC(sizeof(mempool_t));
    if (pool) {
        LOG_DEBUG("create mempool, block_size: %lu, block_count: %lu\n", block_size, block_count);
        mempool_init(pool, block_count * block_size);
        for (int i = 0; i < block_count; i++) {
            void            *block_start = block_alloc(&real_block_size);
            mempool_block_t *block       = mempool_block_alloc(block_start, real_block_size);
            if (!mempool_block_insert_head(pool, block, FREE_MEMBLOCK_LH)) {
                LOG_WARING("Alloc new memblock failed, cur block count:%d\n", atomic_get(&(pool->list_len[FREE_MEMBLOCK_LH])));
                break;
            }
            LOG_DEBUG("Alloc new memblock success, cur block count:%d\n", atomic_get(&(pool->list_len[FREE_MEMBLOCK_LH])));
            pool->pool_free_size += real_block_size;
        }
        atomic_add(&(pool->list_len[USED_MEMBLOCK_LH]), 1);
    }
    return pool;
}

#define EMEMPOOLUSING 1
#define EMEMPOOLINV   1

int mempool_destroy(mempool_t *pool, void (*block_free)(void *block_start))
{
    if (!pool) {
        return -EMEMPOOLINV;
    }
    //
    atomic_sub(&(pool->list_len[USED_MEMBLOCK_LH]), 1);
    if (!atomic_is_null(&(pool->list_len[USED_MEMBLOCK_LH]))) {
        LOG_ERROR("Mempool_destroy failed, pool is using, cur used block count:%d\n", atomic_get(&(pool->list_len[USED_MEMBLOCK_LH])) - 1);
        atomic_add(&(pool->list_len[USED_MEMBLOCK_LH]), 1);
        return -EMEMPOOLUSING;
    }
    //
    while (!atomic_is_null(&(pool->list_len[FREE_MEMBLOCK_LH]))) {
        mempool_block_t *block = mempool_block_remove_head(pool, FREE_MEMBLOCK_LH);
        if (block != NULL) {
            LOG_DEBUG("Mempool_destroy:block free, block size:%lu, block:%p, block_start:%p\n", block->size, block, block->block_start);
            block_free(block->block_start);
            MEMPOOL_FREE(block);
        }
    }
    MEMPOOL_FREE(pool);
    return 0;
}

mempool_block_t *mempool_alloc(mempool_t *pool)
{
    if (!pool) {
        LOG_ERROR("mempool_block_alloc failed, pool:%p\n", pool);
        return NULL;
    }
    // remind others that someone using pool
    atomic_add(&(pool->list_len[USED_MEMBLOCK_LH]), 1);

    // requese free block
    atomic_sub(&(pool->list_len[FREE_MEMBLOCK_LH]), 1);
    return mempool_block_remove_head(pool, FREE_MEMBLOCK_LH);
}

void mempool_free(mempool_t *pool, mempool_block_t *block)
{
    if (!pool || !block) {
        LOG_ERROR("mempool_block_free failed, pool:%p, block:%p\n", pool, block);
        return;
    }
    //
    atomic_add(&(pool->list_len[FREE_MEMBLOCK_LH]), 1);

    // return free block
    atomic_sub(&(pool->list_len[USED_MEMBLOCK_LH]), 1);
    mempool_block_insert_head(pool, block, FREE_MEMBLOCK_LH);
}
