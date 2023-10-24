#include "inode.h"
#include "log.h"
#include "userfs_dentry_hash.h"
#include "userfs_file_ctrl.h"
#include "userfs_heap.h"
#include "vnode.h"
#include <string.h>
#include <sys/time.h>

userfs_bbuf_t *userfs_file_create(
    const char              *name,
    const uint32_t           name_len,
    const uint32_t           dblock_shard_size,
    userfs_super_block_t    *sb,
    linkhash_t              *dentry_hashtable,
    userfs_bbuf_t           *dentry_table_bbuf,
    userfs_bgd_index_list_t *bgd_idx_list)
{
    /*file name should less than 27B*/
    if (name_len > USERFS_MAX_FILE_NAME_LEN) {
        LOG_DESC(ERR, "USERFS FILE CREATE", "Alloc dentry for new file failed");
        return NULL;
    }
    /*get current time as file create time stamp*/
    struct timeval file_create_tp = {0};
    if (gettimeofday(&file_create_tp, NULL) < 0) {
        perror("USERFS FILE CREATE");
        return NULL;
    }
    /*alloc dentry for new inode*/
    userfs_dentry_table_t *dentry_table = USERFS_MBLOCK(dentry_table_bbuf->b_data)->dentry_table;
    uint32_t               dentry_pos   = userfs_alloc_dentry(dentry_table);
    if (dentry_pos == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE CREATE", "Alloc dentry for new file failed");
        return NULL;
    }
    LOG_DESC(DBG, "USERFS FILE CREATE", "New dentry pos:%u, first dentry pos:%u, used dentry count:%u",
             dentry_pos, dentry_table->h.dfd_first_dentry, dentry_table->h.dfd_used_dentry_count);
    /*alloc inode for new file*/
    userfs_bbuf_t *new_inode_bbuf = userfs_get_new_inode(sb, bgd_idx_list, dblock_shard_size, &file_create_tp);
    if (!new_inode_bbuf) {
        LOG_DESC(DBG, "USERFS FILE CREATE", "Alloc new inode failed");
        userfs_free_dentry(dentry_table, dentry_pos);
        return NULL;
    }
    strncpy(dentry_table->dentry[dentry_pos].d_name.name, name, name_len + 1);
    dentry_table->dentry[dentry_pos].d_first_dblock = new_inode_bbuf->b_blocknr;
    /*insert new dentry in dentry hash table*/
    hash_obj_t *new_hash_dentry =
        userfs_dentry_hash_insert(name, name_len, USERFS_NAME2INODEBUF, (unsigned long)new_inode_bbuf, dentry_hashtable);
    if (!new_hash_dentry) {
        LOG_DESC(DBG, "USERFS FILE CREATE", "Insert in dentry hash table failed");
        userfs_free_dentry(dentry_table, dentry_pos);
        userfs_free_used_inode();
        return NULL;
    }
    userfs_inode_t *new_inode = USERFS_DBLOCK(new_inode_bbuf)->inode;
    atomic_add(&(new_inode->ref_count), 1);
    LOG_DESC(DBG, "USERFS FILE CREATE", "Create file success, file:%s, first block:%u, dblock buf:%p, create time:0x%lx, refcount:%d",
             name, new_inode_bbuf->b_blocknr, new_inode_bbuf, file_create_tp.tv_sec, atomic_get(&(new_inode->ref_count)));
    return new_inode_bbuf;
}

userfs_bbuf_t *userfs_file_open(
    const char           *name,
    const uint32_t        name_len,
    const uint32_t        dblock_shard_size,
    userfs_super_block_t *sb,
    linkhash_t           *dentry_hashtable)
{
    /*file name should less than 27B*/
    if (name_len > USERFS_MAX_FILE_NAME_LEN) {
        LOG_DESC(ERR, "USERFS FILE OPEN", "Alloc dentry for new file failed");
        return NULL;
    }
    /*get current time as file latest access time stamp*/
    struct timeval file_open_tp = {0};
    if (gettimeofday(&file_open_tp, NULL) < 0) {
        perror("USERFS FILE OPEN");
        return NULL;
    }

    userfs_dhtable_inodeaddr_t inodeaddr = userfs_dentry_hash_get(name, name_len, dentry_hashtable);
    if (inodeaddr == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE OPEN", "Cannot find file:%s", name);
        return NULL;
    }

    /*if file doesn't been open yet, then dentry hashtable store inode number, otherwise store block buf
    address*/
    userfs_bbuf_t  *ff_bbuf    = NULL;
    uint32_t        ff_block   = 0;
    userfs_inode_t *open_inode = NULL;
    if (USERFS_INODETYPE_GET(inodeaddr) == USERFS_NAME2INODE) {
        /*haven't been opened yet, read inode*/
        ff_block = USERFS_INODEADDR_GET(inodeaddr);
        ff_bbuf  = userfs_get_used_dblock(sb, ff_block, 0, dblock_shard_size);
        if (!ff_bbuf) {
            LOG_DESC(DBG, "USERFS FILE OPEN", "Read inode failed, inode number:%u", ff_block);
            return NULL;
        }
        /*update inode number in dentry hash table to open block buf address in memory*/
        hash_obj_t *new_hash_dentry =
            userfs_dentry_hash_update(name, name_len, USERFS_NAME2INODEBUF, (unsigned long)ff_bbuf, dentry_hashtable);
        if (!new_hash_dentry) {
            LOG_DESC(DBG, "USERFS FILE OPEN", "Update dentry hash failed, file:%s, inode number:%u, dblock buf:%p",
                     name, ff_block, ff_bbuf);
            userfs_free_dbbuf(ff_bbuf);
            return NULL;
        }
        open_inode = USERFS_DBLOCK(ff_bbuf)->inode;
        atomic_add(&(open_inode->ref_count), 1);
        LOG_DESC(DBG, "USERFS FILE OPEN", "Open file success, file:%s, first block:%u, dblock buf:%p, create time:0x%lx, refcount:%d",
                 name, ff_bbuf->b_blocknr, ff_bbuf, file_open_tp.tv_sec, atomic_get(&(open_inode->ref_count)));
    } else {
        ff_bbuf    = (userfs_bbuf_t *)USERFS_INODEADDR_GET(inodeaddr);
        ff_block   = ff_bbuf->b_blocknr;
        open_inode = USERFS_DBLOCK(ff_bbuf)->inode;
        atomic_add(&(open_inode->ref_count), 1);
        LOG_DESC(DBG, "USERFS FILE OPEN", "Refering file success, file:%s, first block:%u, dblock buf:%p, create time:0x%lx, refcount:%d",
                 name, ff_bbuf->b_blocknr, ff_bbuf, file_open_tp.tv_sec, atomic_get(&(open_inode->ref_count)));
    }

    return ff_bbuf;
}

int userfs_file_close(

)
{
}

int userfs_file_delete(

)
{
}