#ifndef HEAP_H
#define HEAP_H
#include <malloc.h>
#include <stdint.h>

#define USERFS_HEAP_MEM_ALLOC(size) malloc(size)
#define USERFS_HEAP_MEM_FREE(ptr) \
    while (ptr) {                 \
        free(ptr);                \
        ptr = NULL;               \
    }

struct block_group_descriptors_index {
    uint32_t bgi_free_blocks_count;
    uint32_t bgi_bgroup_nr;
};

struct userfs_min_root_heap {
    struct block_group_descriptors_index **heap;
    uint32_t                               capacity;
    uint32_t                               size;
};

typedef struct block_group_descriptors_index userfs_mrheap_elem_t;
typedef struct userfs_min_root_heap          userfs_mrheap_t;

userfs_mrheap_elem_t *userfs_mrheap_extract_heaptop(
    userfs_mrheap_t *minHeap);
userfs_mrheap_elem_t *userfs_mrheap_get_heaptop(
    userfs_mrheap_t *minHeap);
userfs_mrheap_elem_t *userfs_mrheap_insert(
    userfs_mrheap_t *minHeap,
    uint32_t         block_nr,
    uint32_t         remain_blocks);
userfs_mrheap_t *userfs_mrheap_create(
    uint32_t capacity);
void userfs_mrheap_destroy(
    userfs_mrheap_t *minHeap);
#endif