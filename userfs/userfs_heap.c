#include "userfs_heap.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USERFS_HEAP_LOG_LEVEL INF

userfs_mrheap_t *userfs_mrheap_create(
    uint32_t capacity)
{
    userfs_mrheap_t *minHeap = (userfs_mrheap_t *)USERFS_HEAP_MEM_ALLOC(sizeof(userfs_mrheap_t));
    if (!minHeap) {
        LOG_DESC(ERR, "USERFS CREATE MINROOT HEAP", "Failed to allocate memory for minroot heap");
        return NULL;
    }
    minHeap->heap = (userfs_mrheap_elem_t **)USERFS_HEAP_MEM_ALLOC(capacity * sizeof(userfs_mrheap_elem_t *));
    if (!minHeap->heap) {
        LOG_DESC(ERR, "USERFS CREATE MINROOT HEAP", "Failed to allocate element array for minroot heap");
        USERFS_HEAP_MEM_FREE(minHeap);
        return NULL;
    }
    minHeap->capacity = capacity;
    minHeap->size     = 0;
    return minHeap;
}

void userfs_mrheap_destroy(
    userfs_mrheap_t *minHeap)
{
    USERFS_HEAP_MEM_FREE(minHeap->heap);
    USERFS_HEAP_MEM_FREE(minHeap);
}

static void swap(
    userfs_mrheap_elem_t **a,
    userfs_mrheap_elem_t **b)
{
    void *tmp = *a;
    *a        = *b;
    *b        = tmp;
}

/*current compare function is for max root heap
for min root heap, change compare direction*/
static int element_compare(
    const userfs_mrheap_elem_t *first,
    const userfs_mrheap_elem_t *second)
{
    return (first->bgi_free_blocks_count < second->bgi_free_blocks_count);
}

static void heap_up(
    userfs_mrheap_t *minHeap,
    uint32_t         index)
{
    uint32_t parent = (index - 1) / 2;
    while (index > 0 && element_compare(minHeap->heap[parent], minHeap->heap[index])) {
        swap(&(minHeap->heap[parent]), &(minHeap->heap[index]));
        index  = parent;
        parent = (index - 1) / 2;
    }
}

static void heap_down(userfs_mrheap_t *minHeap, uint32_t index)
{
    uint32_t smallest    = index;
    uint32_t left_child  = 2 * index + 1;
    uint32_t right_child = 2 * index + 2;

    if (left_child < minHeap->size && element_compare(minHeap->heap[smallest], minHeap->heap[left_child])) {
        smallest = left_child;
    }

    if (right_child < minHeap->size && element_compare(minHeap->heap[smallest], minHeap->heap[right_child])) {
        smallest = right_child;
    }

    if (smallest != index) {
        swap(&minHeap->heap[smallest], &minHeap->heap[index]);
        heap_down(minHeap, smallest);
    }
}

userfs_mrheap_elem_t *userfs_mrheap_insert(
    userfs_mrheap_t      *minHeap,
    uint32_t              bgi_bgroup_nr,
    uint32_t              bgi_free_blocks_count,
    userfs_mrheap_elem_t *elem_ptr)
{
    if (minHeap->size == minHeap->capacity) {
        LOG_DESC(ERR, "USERFS HEAP INSERT", "Heap is empty");
        return NULL;
    }
    userfs_mrheap_elem_t *value =
        !elem_ptr ? (userfs_mrheap_elem_t *)USERFS_HEAP_MEM_ALLOC(sizeof(userfs_mrheap_elem_t)) : elem_ptr;
    if (value == NULL) {
        LOG_DESC(ERR, "USERFS HEAP INSERT", "Heap is empty");
        return NULL;
    }
    value->bgi_bgroup_nr         = bgi_bgroup_nr;
    value->bgi_free_blocks_count = bgi_free_blocks_count;

    minHeap->heap[minHeap->size] = value;
    heap_up(minHeap, minHeap->size);
    minHeap->size++;
    LOG_DESC(USERFS_HEAP_LOG_LEVEL, "USERFS HEAP INSERT", "Insert new bgd index, bgroup number:%u, remain block:%u, heap size:%u",
             value->bgi_bgroup_nr, value->bgi_free_blocks_count, minHeap->size);
    return value;
}

userfs_mrheap_elem_t *userfs_mrheap_get_heaptop(
    userfs_mrheap_t *minHeap)
{
    if (minHeap->size == 0) {
        LOG_DESC(ERR, "USERFS GET HEAP TOP", "Heap is empty");
        return NULL;
    }
    return minHeap->heap[0];
}

userfs_mrheap_elem_t *userfs_mrheap_extract_heaptop(
    userfs_mrheap_t *minHeap)
{
    userfs_mrheap_elem_t *min = userfs_mrheap_get_heaptop(minHeap);
    if (!min) {
        LOG_DESC(ERR, "USERFS EXTRACT HEAP TOP", "Heap is empty");
        return min;
    }

    minHeap->heap[0] = minHeap->heap[minHeap->size - 1];
    minHeap->size--;
    heap_down(minHeap, 0);
    LOG_DESC(USERFS_HEAP_LOG_LEVEL, "USERFS EXTRACT HEAP TOP", "Extract lightest load bgd index, block number:%u, remain block:%u, heap size:%u",
             min->bgi_bgroup_nr, min->bgi_free_blocks_count, minHeap->size);
    return min;
}

static void userfs_mrheap_debug_function(void)
{
    uint32_t         heap_size = 16;
    userfs_mrheap_t *minHeap   = userfs_mrheap_create(heap_size);

    for (int i = 0; i < heap_size; i++) {
        userfs_mrheap_elem_t *elem = userfs_mrheap_insert(minHeap, i, heap_size - i, NULL);
    }

    for (int i = 0; i < heap_size; i++) {
        userfs_mrheap_elem_t *elem = userfs_mrheap_extract_heaptop(minHeap);
        USERFS_HEAP_MEM_FREE(elem);
    }
    USERFS_HEAP_MEM_FREE(minHeap);
}