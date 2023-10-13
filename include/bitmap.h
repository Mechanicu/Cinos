#ifndef _BITMAP_H_
#define _BITMAP_H_
#include "atomic.h"
#include <stdint.h>

typedef struct bitmap {
    uint32_t bytes;
    atomic_t used_bits_count;
    atomic_t first_free_pos;
    uint8_t  bitmap[0];
} bitmap_t;

// ctrl
bitmap_t *bitmap_create(const unsigned int bytes);
void      bitmap_destroy(bitmap_t *bitmap);
// ops
int       bitmap_set(bitmap_t *bitmap, const unsigned int pos);
int       bitmap_clear(bitmap_t *bitmap, const unsigned int pos);
int       bitmap_get_first_free(bitmap_t *bitmap);
void      bitmap_init(bitmap_t *b, const unsigned int bytes);

#endif