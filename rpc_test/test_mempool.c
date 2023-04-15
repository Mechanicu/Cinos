#include "include/list.h"
#include "include/mem_pool.h"
#include <pthread.h>

#define PAGE_SIZE      4096
#define MAX_BLOCK_SIZE (PAGE_SIZE << 4)

void *block_alloc_func(unsigned long *size)
{
    if (*size > MAX_BLOCK_SIZE) {
        return NULL;
    }
    unsigned long real_size = *size;
    real_size               += PAGE_SIZE - 1;
    real_size               &= ~(PAGE_SIZE - 1);
    LOG_DEBUG("Wanted %lu bytes, Allocate %lu bytes", *size, real_size);
    *size                   = real_size;

    return malloc(real_size);
}

void block_free_func(void *block_start)
{
    if (block_start) {
        free(block_start);
    }
}

int main()
{
    mempool_t *pool = mempool_create(1024, 16, block_alloc_func);
    mempool_block_t* block = mempool_alloc(pool);
    LOG_DEBUG("block:%p, size:%lu, start:%p", block, block->size, block->block_start);
    mempool_free(pool, 0);
    mempool_destroy(pool, block_free_func);
    mempool_free(pool, block);
    mempool_destroy(pool, block_free_func);
    return 0;
}