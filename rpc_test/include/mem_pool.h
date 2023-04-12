// 基于C实现内存池

#ifndef __MEMORY_POOL_H__
#define __MEMORY_POOL_H__

#include "atomic.h"
#include "list.h"
#include "log.h"
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    FREE_MEMBLOCK_LH,
    USED_MEMBLOCK_LH,
    MEMBLOCK_LH_COUNT,
};

typedef struct mempool_block_s {
    list_t        hook;
    unsigned long size;
    void         *block_start;
} mempool_block_t;

typedef struct mempool_s {
    list_t        block_lh[MEMBLOCK_LH_COUNT];
    list_t        pool_hook;
    atomic_t      list_len[MEMBLOCK_LH_COUNT];
    unsigned long pool_free_size;
    unsigned long pool_size;
    void         *pool_start;
} mempool_t;

mempool_t       *mempool_create(unsigned long block_size, unsigned long block_count, void *(*block_alloc)(unsigned long *size));
int              mempool_destroy(mempool_t *pool, void (*block_free)(void *block_start));
mempool_block_t *mempool_alloc(mempool_t *pool);
void             mempool_free(mempool_t *pool, mempool_block_t *block);

#endif   // __MEMORY_POOL_H__