#ifndef USERFS_DENTRY_HASH
#define USERFS_DENTRY_HASH

#include "atomic.h"
#include "inode.h"
#include "list.h"
#include "vnode.h"
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

#define USERFS_INODEADDRINFO_CAL(type, addr) ((((unsigned long)(type) << ((sizeof(unsigned long) << 3) - 1)) | (unsigned long)addr))
#define USERFS_INODETYPE_GET(val)            ((unsigned long)(val) >> ((sizeof(unsigned long) << 3) - 1))
#define USERFS_INODEADDR_GET(val)            ((unsigned long)(val) & (UINT64_MAX >> 1))

typedef atomic_t hlist_bucket_lock_t;

typedef struct hash_object {
    list_t                     chain;
    struct userfs_dentry_name  name;
    uint32_t                   dentry_pos;
    userfs_dhtable_inodeaddr_t val;
} hash_obj_t;

typedef struct hashlist_object {
    struct hashlist_object *hlist;
    hash_obj_t              obj;
} hlist_t;

typedef struct hashlist_bucket {
    atomic_t refcount;
    hlist_t *bucket_start;
#if ENABLE_LINKHASH_BUCKET_LOCK == 1
    hlist_bucket_lock_t bucket_lock;
#endif
} hlist_bucket_t;

typedef struct linkhash {
    unsigned long  bucket_count;
    atomic_t       obj_count;
    list_t         hlist_head;
    hlist_bucket_t bucket[0];
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

static inline unsigned int str2hash_u32(const char *name, uint32_t len)
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

linkhash_t *userfs_dentry_hash_create(
    const unsigned long bucket_count);

void userfs_dentry_hash_destroy(
    linkhash_t *hashtable);

hash_obj_t *userfs_dentry_hash_insert(
    const char    *name,
    const uint32_t name_len,
    const uint8_t  inodeaddr_type,
    unsigned long  inodeaddr,
    const uint32_t dentry_pos,
    linkhash_t    *table);

hash_obj_t *userfs_dentry_hash_update(
    const char    *name,
    const uint32_t name_len,
    const uint8_t  inodeaddr_type,
    unsigned long  inodeaddr,
    linkhash_t    *table);

userfs_dhtable_inodeaddr_t userfs_dentry_hash_get(
    const char    *name,
    const uint32_t name_len,
    uint32_t      *dentry_pos,
    linkhash_t    *hashtable);

userfs_dhtable_inodeaddr_t userfs_dentry_hash_remove(
    const char    *name,
    const uint32_t name_len,
    uint32_t      *dentry_pos,
    linkhash_t    *hashtable);

#endif