#include "include/hashlist.h"
#include "include/list.h"
#include "include/log.h"
#include <limits.h>

#define MAX_HASH_TABLE_COUNT 128

unsigned long long list[MAX_HASH_TABLE_COUNT] = {0};

int main(int argc, char *argv[])
{
    // for (int i = 0; i < UINT_MAX >> 16; i++) {
    //     list[hash_32bkey(i) % MAX_HASH_TABLE_COUNT] += 1;
    // }
    // for (int i = 0; i < MAX_HASH_TABLE_COUNT; i++){
    //     LOG_DEBUG("list[%d]:%llu\n", i, list[i]);
    // }
    linkhash_t *hashtable = linkhash_create(MAX_HASH_TABLE_COUNT);
    LOG_DEBUG("hashtable:%p, bucket_start:%p, bucket_count:%lu, obj_count:%d\n",
              hashtable,
              hashtable->bucket,
              hashtable->bucket_count,
              atomic_load(&(hashtable->obj_count)));
    for (int i = 0; i < MAX_HASH_TABLE_COUNT; i++) {
        linkhash_add(i, ULONG_MAX - i, hashtable);
    }
    // hash_obj_t *hlist_obj = linkhash_get(MAX_HASH_TABLE_COUNT >> 2, hashtable);
    // unsigned long val = linkhash_remove(MAX_HASH_TABLE_COUNT >> 2, hashtable);
    // LOG_DEBUG("hashtable remove, val:%lx\n", val);
    linkhash_destroy(hashtable);
    return 0;
}