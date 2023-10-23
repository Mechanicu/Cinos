#include "bitmap.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

#define USERFS_BITMAP_LOG_LEVEL        INF

#define BITMAP_SET_POS(bitmap, val)    (bitmap[pos >> 3] |= (val << (pos & 7)))
#define BITMAP_POS_ISNULL(bitmap, pos) (bitmap[pos >> 3] & (1 << (pos & 7)))
#define BITMAP_MAX_BYTES               256
#define BITMAP_ALLOC(bytes)            malloc(bytes + sizeof(bitmap_t))
#define BITMAP_FREE(ptr)            \
    while ((void *)(ptr) != NULL) { \
        free(ptr);                  \
        (ptr) = NULL;               \
    }

void bitmap_init(bitmap_t *b, const unsigned int bytes)
{
    b->bytes = bytes;
    memset(b->bitmap, 0, bytes);
    atomic_set(&(b->used_bits_count), 0);
    atomic_set(&(b->first_free_pos), 0);
}

bitmap_t *bitmap_create(const unsigned int bytes)
{
    if (bytes > BITMAP_MAX_BYTES) {
        return NULL;
    }
    bitmap_t *bm = BITMAP_ALLOC(bytes);
    if (bm) {
        bitmap_init(bm, bytes);
    }
    return bm;
}

void bitmap_destroy(bitmap_t *bitmap)
{
    if (bitmap) {
        BITMAP_FREE(bitmap);
    }
}

static inline int bitmap_set_val(bitmap_t *bitmap, const unsigned int pos, const unsigned char val)
{
    if (!bitmap) {
        return -1;
    }
    if (pos > (bitmap->bytes << 3)) {
        LOG_DESC(USERFS_BITMAP_LOG_LEVEL, "BITMAP SET VAL", "Bitmap position %u is out of range, max range:%u.",
                 pos, bitmap->bytes << 3);
        return -1;
    }
    bitmap->bitmap[pos >> 3] |= (val << (pos & 7));
    LOG_DESC(USERFS_BITMAP_LOG_LEVEL, "BITMAP SET VAL", "set_val: bitmap_t at:%u set to:%u, res:%hhx",
             pos, val, bitmap->bitmap[pos >> 3]);
    return 0;
}

int bitmap_clear(bitmap_t *bitmap, const unsigned int pos)
{
    if (!(bitmap->bitmap[pos >> 3] |= (0 << (pos & 7)))) {
        LOG_DESC(ERR, "BITMAP CLEAR", "bitmap:%p at pos:%u already clean",
                 bitmap, pos);
        return -1;
    }
    atomic_sub(&(bitmap->used_bits_count), 1);
    if (atomic_get(&(bitmap->first_free_pos)) > pos) {
        atomic_set(&(bitmap->first_free_pos), pos);
    }
    return bitmap_set_val(bitmap, pos, 0);
}

int bitmap_set(bitmap_t *bitmap, const unsigned int pos)
{
    if (!(bitmap->bitmap[pos >> 3] |= (1 << (pos & 7)))) {
        LOG_DESC(ERR, "BITMAP SET", "bitmap:%p at pos:%u already set",
                 bitmap, pos);
        return -1;
    }
    atomic_add(&(bitmap->used_bits_count), 1);
    return bitmap_set_val(bitmap, pos, 1);
}

int bitmap_get_first_free(bitmap_t *bitmap)
{
    if (!bitmap) {
        return -1;
    }
    int first_pos = atomic_get(&(bitmap->first_free_pos));
    if (first_pos == bitmap->bytes << 3) {
        LOG_DESC(ERR, "BITMAP GET FIRST FREE", "bitmap full");
        return -1;
    }

    bitmap_set(bitmap, first_pos);
    int pos = (first_pos + 1) % (bitmap->bytes << 3);
    for (; pos < bitmap->bytes << 3, BITMAP_POS_ISNULL(bitmap->bitmap, pos); pos++)
        ;
    LOG_DESC(USERFS_BITMAP_LOG_LEVEL, "BITMAP GET FIRST FREE", "bitmap_get_first_free:%d, current_first:%d",
             first_pos, pos);
    atomic_set(&(bitmap->first_free_pos), pos);
    return first_pos;
}