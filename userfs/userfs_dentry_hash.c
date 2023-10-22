#include "userfs_dentry_hash.h"
#include "inode.h"
#include "log.h"
#include "vnode.h"
#include <stdint.h>
#include <string.h>

#define USERFS_DHASH_MEM_ALLOC(size) malloc(size)
#define USERFS_DHASH_MEM_FREE(ptr)  \
    while ((void *)(ptr) != NULL) { \
        free(ptr);                  \
        (ptr) = NULL;               \
    }

static inline void linkhash_bucket_init(
    hlist_bucket_t *bucket)
{
    HASH_BUCKET_LOCK_INIT(&(bucket->bucket_lock));
    bucket->bucket_start = NULL;
    atomic_set(&(bucket->refcount), 0);
}

static inline void linkhash_init(
    const unsigned long bucket_count,
    linkhash_t         *hashtable)
{
    hashtable->bucket_count = bucket_count;
    atomic_store(&(hashtable->obj_count), 0);
    INIT_LIST_HEAD(&(hashtable->hlist_head));
    // don't init bucket head when create linkhash, init it when using
    memset(hashtable->bucket, 0, bucket_count * sizeof(hlist_bucket_t));
    for (int i = 0; i < bucket_count; i++) {
        linkhash_bucket_init(hashtable->bucket + i);
    }
}

linkhash_t *userfs_dentry_hash_create(
    const unsigned long bucket_count)
{
    if (!bucket_count || bucket_count > LINKHASH_MAX_BUCKET_COUNT) {
        return NULL;
    }

    //
    linkhash_t *hashtable = USERFS_DHASH_MEM_ALLOC(sizeof(linkhash_t) + sizeof(hlist_bucket_t) * bucket_count);
    if (!hashtable) {
        return NULL;
    }
    linkhash_init(bucket_count, hashtable);
    return hashtable;
}

void userfs_dentry_hash_destroy(
    linkhash_t *hashtable)
{
    if (!hashtable) {
        return;
    }
    list_t *hashlist = &(hashtable->hlist_head);
    for (list_t *hashlist = &(hashtable->hlist_head); hashlist->next != hashlist;) {
        hash_obj_t *tmp = container_of(hashlist->next, hash_obj_t, chain);
        LOG_DEBUG("HASHLIST destroy, dentry name:%s, itype:%lu, iaddr:0x%lx",
                  tmp->name.name, USERFS_INODETYPE_GET(tmp->val), USERFS_INODEADDR_GET(tmp->val));
        list_del_init(&(tmp->chain));
        hlist_t *cur_hash_obj = container_of(tmp, hlist_t, obj);
        USERFS_DHASH_MEM_FREE(cur_hash_obj);
    }
    USERFS_DHASH_MEM_FREE(hashtable);
}

static hlist_t *check_if_exists(
    hlist_t       *conflict_list,
    const char    *name,
    const uint32_t name_len)
{
    while (conflict_list != NULL) {
        if (!strncmp(conflict_list->obj.name.name, name, name_len)) {
            return conflict_list;
        }
        conflict_list = conflict_list->hlist;
    }
    return NULL;
}

hash_obj_t *userfs_dentry_hash_insert(
    const char    *name,
    const uint32_t name_len,
    const uint8_t  inodeaddr_type,
    unsigned long  inodeaddr,
    linkhash_t    *table)
{
    unsigned int hash      = str2hash_u32(name, name_len);
    unsigned int bucket_id = hash & (table->bucket_count - 1);
    LOG_DEBUG("HASHLIST insert, dentry name:%s, hash:%u, bucket:%u", name, hash, bucket_id);
    hlist_bucket_t *bucket = &(table->bucket[bucket_id]);

    // hash bucket itself doesn't store val, it only point to hash_obj list which stores val
    hlist_t *new           = USERFS_DHASH_MEM_ALLOC(sizeof(hlist_t));
    if (!new) {
        LOG_DEBUG("USERFS_DHASH_MEM_ALLOC failed!");
        return NULL;
    }
    memset(new, 0, sizeof(hlist_t));
    unsigned long val = USERFS_INODEADDRINFO_CAL(inodeaddr_type, inodeaddr);
    memcpy(&(new->obj.val), &val, sizeof(unsigned long));
    memcpy(new->obj.name.name, name, name_len);
    new->obj.name.name[name_len + 1] = 0;

    // insert in conflict solved chain
    hlist_t *conflict_list           = bucket->bucket_start;
    // check if current dentry already exists
    conflict_list                    = check_if_exists(conflict_list, name, name_len);
    if (conflict_list != NULL) {
        LOG_DESC(ERR, "DENRY HASH INSERT", "Dentry already exist, dentry name:%s, hash:%u, bucket:%u",
                 name, hash, bucket_id);
        USERFS_DHASH_MEM_FREE(new);
        return NULL;
    }
    new->hlist           = bucket->bucket_start;
    bucket->bucket_start = new;

    atomic_add(&(bucket->refcount), 1);
    // insert in table list
    list_add(&(new->obj.chain), &(table->hlist_head));
    atomic_add(&(table->obj_count), 1);
    LOG_DESC(ERR, "DENRY HASH INSERT", "Insert new dentry, dentry name:%s, itype:%lu, iaddr:0x%lx, hash:%u, bucket:%u, obj count:%u, bucket list len:%u",
             name, USERFS_INODETYPE_GET(new->obj.val), USERFS_INODEADDR_GET(new->obj.val), hash, bucket_id, atomic_get(&(table->obj_count)), atomic_get(&(bucket->refcount)));
    return &(new->obj);
}

hash_obj_t *userfs_dentry_hash_update(
    const char    *name,
    const uint32_t name_len,
    const uint8_t  inodeaddr_type,
    unsigned long  inodeaddr,
    linkhash_t    *table)
{
    unsigned int    hash      = str2hash_u32(name, name_len);
    unsigned int    bucket_id = hash & (table->bucket_count - 1);
    hlist_bucket_t *bucket    = &(table->bucket[bucket_id]);

    // insert in conflict solved chain
    hlist_t *conflict_list    = bucket->bucket_start;
    if (conflict_list != NULL) {
        // check if current dentry exists
        conflict_list = check_if_exists(conflict_list, name, name_len);
        if (conflict_list == NULL) {
            LOG_DESC(ERR, "DENRY HASH UPDATE", "Dentry NOT exist, dentry name:%s, hash:%u, bucket:%u",
                     name, hash, bucket_id);
            return NULL;
        }
        // update
        LOG_DESC(DBG, "DENRY HASH UPDATE", "Update dentry,, dentry name:%s, old itype:%lu, old iaddr:0x%lx, new itype:%lu, new iaddr:0x%lx, hash:%u, bucket:%u",
                 name, USERFS_INODETYPE_GET(conflict_list->obj.val), USERFS_INODEADDR_GET(conflict_list->obj.val), (unsigned long)inodeaddr_type, inodeaddr, hash, bucket_id)
        conflict_list->obj.val = USERFS_INODEADDRINFO_CAL(inodeaddr_type, inodeaddr);
        return &(conflict_list->obj);
    }
}

userfs_dhtable_inodeaddr_t userfs_dentry_hash_get(
    const char    *name,
    const uint32_t name_len,
    linkhash_t    *hashtable)
{
    userfs_dhtable_inodeaddr_t inodeaddr = 0;
    if (!hashtable) {
        return inodeaddr;
    }
    unsigned int    hash          = str2hash_u32(name, name_len);
    unsigned int    bucket_id     = hash & (hashtable->bucket_count - 1);
    hlist_bucket_t *bucket        = &(hashtable->bucket[bucket_id]);
    hlist_t        *conflict_list = bucket->bucket_start;
    while (conflict_list != NULL) {
        if (!strncmp(conflict_list->obj.name.name, name, name_len)) {
            LOG_DESC(DBG, "DENRY HASH GET", "Get dentry, dentry name:%s, hash:%u, bucket:%u, itype:%lu, iaddr:0x%lx",
                     name, hash, bucket_id, USERFS_INODETYPE_GET(conflict_list->obj.val), USERFS_INODEADDR_GET(conflict_list->obj.val));
            return conflict_list->obj.val;
        }
        conflict_list = conflict_list->hlist;
    }
    return inodeaddr;
}

userfs_dhtable_inodeaddr_t userfs_dentry_hash_remove(
    const char    *name,
    const uint32_t name_len,
    linkhash_t    *hashtable)
{
    userfs_dhtable_inodeaddr_t inodeaddr = 0;
    if (!hashtable) {
        return inodeaddr;
    }
    unsigned int    hash      = str2hash_u32(name, name_len);
    unsigned int    bucket_id = hash & (hashtable->bucket_count - 1);
    hlist_bucket_t *bucket    = &(hashtable->bucket[bucket_id]);

    hlist_t *cur              = bucket->bucket_start;
    hlist_t *prev             = NULL;
    while (cur != NULL) {
        if (!strncmp(cur->obj.name.name, name, name_len)) {
            //
            atomic_sub(&(bucket->refcount), 1);
            atomic_sub(&(hashtable->obj_count), 1);
            // remove from hashtable list
            list_del_init(&(cur->obj.chain));

            if (!prev) {
                bucket->bucket_start = cur->hlist;
            } else {
                prev->hlist = cur->hlist;
            }
            memcpy(&inodeaddr, &(cur->obj.val), sizeof(userfs_dhtable_inodeaddr_t));
            LOG_DESC(DBG, "DENTRY HASH REMOVE", "dentry name:%s, hash:%u, bucket:%u, itype:%lu, iaddr:0x%lx",
                     cur->obj.name.name, hash, bucket_id, USERFS_INODETYPE_GET(inodeaddr), USERFS_INODEADDR_GET(inodeaddr));
            USERFS_DHASH_MEM_FREE(cur);
            return inodeaddr;
        }
        prev = cur;
        cur  = cur->hlist;
    }
    LOG_DESC(ERR, "DENTRY HASH REMOVE", "Target dentry doesn't exists, dentry name:%s",
             name);
    return inodeaddr;
}

void userfs_dentry_hash_debug_intf(void)
{
    uint32_t      filecount      = 32;
    linkhash_t   *table          = userfs_dentry_hash_create(LINKHASH_MAX_BUCKET_COUNT);
    char          filename[]     = "testfile.<1234567890>.txt";
    unsigned long inodeaddr      = 0x7234567878563412;
    uint32_t      inodeaddr_type = 1;
    unsigned long val            = USERFS_INODEADDRINFO_CAL(inodeaddr_type, inodeaddr);
    LOG_DESC(DBG, "", "0x%lx, %lx, %lx", val, USERFS_INODETYPE_GET(val), USERFS_INODEADDR_GET(val));
    // return 0;

    for (int i = 0; i < filecount; i++) {
        snprintf(filename, 26, "testfile.<1234 %d>.txt", i);
        userfs_dentry_hash_insert(filename, strlen(filename), 1, i, table);
        userfs_dentry_hash_insert(filename, strlen(filename), 1, i, table);
    }

    for (int i = 0; i < filecount; i++) {
        snprintf(filename, 26, "testfile.<1234 %d>.txt", i);
        userfs_dhtable_inodeaddr_t tmp =
            userfs_dentry_hash_get(filename, strlen(filename), table);
    }

    for (int i = 0; i < filecount; i++) {
        snprintf(filename, 26, "testfile.<1234 %d>.txt", i);
        userfs_dentry_hash_update(filename, strlen(filename), 0, i + 32, table);
    }

    userfs_dentry_hash_destroy(table);
}

int main(void)
{
    userfs_dentry_hash_debug_intf();
    return 0;
}
