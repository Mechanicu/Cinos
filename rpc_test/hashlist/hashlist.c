#include "../include/hashlist.h"
#include "../include/log.h"
#include <string.h>

#define HASH_OBJ_ALLOC(size) malloc(size)
#define HASH_OBJ_FREE(ptr)   free(ptr)

static inline void linkhash_init(const unsigned long bucket_count, linkhash_t *hashtable)
{
    hashtable->bucket_count = bucket_count;
    hashtable->bucket       = hashtable->_bucket;
    atomic_store(&(hashtable->obj_count), 0);
    INIT_LIST_HEAD(&(hashtable->hlist_head));
    // don't init bucket head when create linkhash, init it when using
    memset(hashtable->bucket, 0, bucket_count * sizeof(hlist_bucket_t));
}

static inline void linkhash_bucket_init(hlist_bucket_t *bucket)
{
    HASH_BUCKET_LOCK_INIT(&(bucket->bucket_lock));
    INIT_LIST_HEAD(&(bucket->buckte_start));
    atomic_set(&(bucket->refcount), 0);
}

linkhash_t *linkhash_create(const unsigned long bucket_count)
{
    if (!bucket_count || bucket_count > LINKHASH_MAX_BUCKET_COUNT) {
        return NULL;
    }

    //
    linkhash_t *hashtable = HASH_OBJ_ALLOC(sizeof(linkhash_t) + sizeof(hlist_bucket_t) * bucket_count);
    if (!hashtable) {
        return NULL;
    }
    linkhash_init(bucket_count, hashtable);
    return hashtable;
}

int linkhash_add(unsigned long key, void *val, linkhash_t *table)
{
    if (!val || !table) {
        return -1;
    }

    unsigned int hash      = hash_32bkey((unsigned int)key);
    unsigned int bucket_id = hash & (table->bucket_count - 1);
    LOG_DEBUG("HASHLIST add, key:%llu, val:%p, hash:%u, bucket:%u\n", key, val, hash, bucket_id);
    hlist_bucket_t *bucket = &(table->bucket[bucket_id]);

    // hash bucket itself doesn't store val, it only point to hash_obj list which stores val
    hlist_t *new           = HASH_OBJ_ALLOC(sizeof(hlist_t));
    if (!new) {
        LOG_DEBUG("HASH_OBJ_ALLOC failed!\n");
        return -1;
    }
    new->obj.val = val;
    new->obj.key = key;
    if (bucket->buckte_start.next == NULL) {
        INIT_LIST_HEAD(&(bucket->buckte_start));
    }
    atomic_add(&(bucket->refcount), 1);
    // insert in conflict solved chain
    list_add(&(new->obj.chain), &(bucket->buckte_start));
    // insert in table list
    list_add(&(new->hlist), &(table->hlist_head));
    atomic_add(&(table->obj_count), 1);
    return 0;
}

void linkhash_destroy(linkhash_t *hashtable)
{
    if (!hashtable) {
        return;
    }
    list_t *hashlist = &(hashtable->hlist_head);
    while (hashlist->next != hashlist) {
        hlist_t *tmp = container_of(hashlist->next, hlist_t, hlist);
        LOG_DEBUG("HASHLIST destroy, obj:%p, key:%lx, val:%p\n", tmp, tmp->obj.key, tmp->obj.val);
        list_del_init(&(tmp->hlist));
        HASH_OBJ_FREE(tmp);
    }
    HASH_OBJ_FREE(hashtable);
}

hlist_t *linkhash_get(unsigned long key, linkhash_t *hashtable)
{
    if (!hashtable) {
        return NULL;
    }
}

hlist_t *linkhash_remove(unsigned long key, linkhash_t *hashtable)
{
}
