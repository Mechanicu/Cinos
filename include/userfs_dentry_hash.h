#ifndef USERFS_DENTRY_HASH
#define USERFS_DENTRY_HASH

#ifndef _HASHLIST_H_
#define _HASHLIST_H_

#include "atomic.h"
#include "list.h"
#include <pthread.h>

#define LINKHASH_MAX_BUCKET_COUNT   128
#define ENABLE_LINKHASH_BUCKET_LOCK 0

#if ENABLE_LINKHASH_BUCKET_LOCK == 1
#define HASH_BUCKET_LOCK_INIT(lock) atomic_set(lock, 0)
#define HASH_BUCKET_LOCK(lock) \
    do {                       \
        atomic_set(lock, 0);   \
    } while (!atomic_get(lock))
#define HASH_BUCKET_UNLOCK(lock) atomic_set(lock, 1)
#else
#define HASH_BUCKET_LOCK_INIT(lock)
#define HASH_BUCKET_LOCK(lock)
#define HASH_BUCKET_UNLOCK(lock)
#endif

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
    atomic_t refcount;
    list_t   bucket_start;
#if ENABLE_LINKHASH_BUCKET_LOCK == 1
    hlist_bucket_lock_t bucket_lock;
#endif
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

static inline unsigned int str2hash_u32(const char *name, int len)
{
    unsigned int         hash;
    unsigned int         hash0 = 0x12a3fe2d;
    unsigned int         hash1 = 0x37abe8f9;
    const unsigned char *ucp   = (const unsigned char *)name;

    while (len--) {
        hash = hash1 + (hash0 ^ (((int)*ucp++) * 7152373));

        if (hash & 0x80000000) {
            hash -= 0x7fffffff;
        }
        hash1 = hash0;
        hash0 = hash;
    }
    return hash0 << 1;
}

#define HASH_KEY_PREPROC(name, len) str2hash_u32(name, len)
#define HASH_GET_KEY(key)           hash_32bkey(key)

linkhash_t *linkhash_create(const unsigned long bucket_count);
void        linkhash_destroy(linkhash_t *hashtable);
int         linkhash_add(unsigned long key, void *val, linkhash_t *table);
long        linkhash_get(unsigned long key, linkhash_t *hashtable);
long        linkhash_remove(unsigned long key, linkhash_t *hashtable);

#endif

#endif