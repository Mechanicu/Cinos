#ifndef _HASHLIST_H_
#define _HASHLIST_H_

#include "atomic.h"
#include "kernel_list.h"
#include <pthread.h>

#define LINKHASH_MAX_BUCKET_COUNT   128

#define HASH_BUCKET_LOCK_INIT(lock) atomic_set(lock, 0)
#define HASH_BUCKET_LOCK(lock) \
    do {                       \
        atomic_set(lock, 0);   \
    } while (!atomic_get(lock))
#define HASH_BUCKET_UNLOCK(lock) atomic_set(lock, 1)

typedef atomic_t hlist_bucket_lock_t;

typedef struct hash_object {
    list_t        chain;
    unsigned long key;
    void         *val;
} hash_obj_t;

typedef struct hashlist_object {
    list_t     hlist;
    hash_obj_t obj;
} hlist_t;

typedef struct hashlist_bucket {
    atomic_t            refcount;
    list_t              buckte_start;
    hlist_bucket_lock_t bucket_lock;
} hlist_bucket_t;

typedef struct linkhash {
    unsigned long   bucket_count;
    atomic_t        obj_count;
    list_t          hlist_head;
    hlist_bucket_t *bucket;
    hlist_bucket_t  _bucket[0];
} linkhash_t;

static inline unsigned int hash_32bkey(const unsigned int key)
{
    unsigned int hash_key = key;
    hash_key              = (~hash_key) + (hash_key << 18); /* hash_key = (hash_key << 18) - hash_key - 1; */
    hash_key              = hash_key ^ (hash_key >> 31);
    hash_key              = hash_key * 21; /* hash_key = (hash_key + (hash_key << 2)) + (hash_key << 4); */
    hash_key              = hash_key ^ (hash_key >> 11);
    hash_key              = hash_key + (hash_key << 6);
    hash_key              = hash_key ^ (hash_key >> 22);
    return hash_key;
}

linkhash_t *linkhash_create(const unsigned long bucket_count);
void        linkhash_destroy(linkhash_t *hashtable);
int         linkhash_add(unsigned long key, void *val, linkhash_t *table);
int         hash_get(unsigned long long key);
void        hash_remove(unsigned long long key);

#endif