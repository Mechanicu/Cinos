#include "include/atomic.h"
#include "include/bitmap.h"
#include "include/log.h"

int main(int argc, char **argv)
{
    int       i;
    bitmap_t *bmp = bitmap_create(64);
    LOG_DEBUG("bitmap used_bits:%d, first_po:%d, bytes:%u", atomic_get(&(bmp->used_bits_count)),
              atomic_get(&(bmp->first_free_pos)),
              bmp->bytes);
    for (int i = 0; i < 3; i++) {
        bitmap_get_first_free(bmp);
        LOG_DEBUG("bitmap used_bits:%d, first_po:%d, bytes:%u", atomic_get(&(bmp->used_bits_count)),
                  atomic_get(&(bmp->first_free_pos)),
                  bmp->bytes);
        bitmap_clear(bmp, i);
    }
    bitmap_destroy(bmp);
    return 0;
}