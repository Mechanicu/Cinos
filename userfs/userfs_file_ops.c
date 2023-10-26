#include "userfs_file_ops.h"
#include "atomic.h"
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
        LOG_DESC(ERR, "USERFS FILE CREATE", "File name too long, name len:%u, max len:%u",
                 name_len, USERFS_MAX_FILE_NAME_LEN);
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
        userfs_dentry_hash_insert(name, name_len, USERFS_NAME2INODEBUF, (unsigned long)new_inode_bbuf, dentry_pos, dentry_hashtable);
    if (!new_hash_dentry) {
        LOG_DESC(DBG, "USERFS FILE CREATE", "Insert in dentry hash table failed");
        userfs_free_dentry(dentry_table, dentry_pos);
        userfs_free_used_inode();
        return NULL;
    }
    /*init inode*/
    userfs_inode_t *new_inode      = USERFS_DBLOCK(new_inode_bbuf->b_data)->inode;
    new_inode->i_ctime             = file_create_tp.tv_sec;
    new_inode->i_v2pnode_table[0]  = new_inode_bbuf->b_blocknr;
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
        LOG_DESC(ERR, "USERFS FILE OPEN", "File name too long, name len:%u, max len:%u",
                 name_len, USERFS_MAX_FILE_NAME_LEN);
        return NULL;
    }
    /*get current time as file latest access time stamp*/
    struct timeval file_open_tp = {0};
    if (gettimeofday(&file_open_tp, NULL) < 0) {
        perror("USERFS FILE OPEN");
        return NULL;
    }

    uint32_t                   dentry_pos = 0;
    userfs_dhtable_inodeaddr_t inodeaddr  = userfs_dentry_hash_get(name, name_len, &dentry_pos, dentry_hashtable);
    if (inodeaddr == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE OPEN", "Cannot find file:%s", name);
        return NULL;
    }
    LOG_DESC(DBG, "USERFS FILE OPEN", "Get dentry name:%s, dentry pos:%u", name, dentry_pos);

    /*if file doesn't been open yet, then dentry hashtable store inode number, otherwise store block buf
    address*/
    userfs_bbuf_t  *ff_bbuf    = NULL;
    uint32_t        ff_block   = 0;
    userfs_inode_t *open_inode = NULL;
    if (USERFS_INODETYPE_GET(inodeaddr) == USERFS_NAME2INODE) {
        /*haven't been opened yet, read inode*/
        ff_block = USERFS_INODEADDR_GET(inodeaddr);
        ff_bbuf  = userfs_get_used_inode(sb, ff_block, dblock_shard_size);
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
        open_inode = USERFS_DBLOCK(ff_bbuf->b_data)->inode;
        atomic_add(&(open_inode->ref_count), 1);
        LOG_DESC(DBG, "USERFS FILE OPEN", "Open file success, file:%s, first block:%u, record first block:%lu, dblock buf:%p, access time:0x%lx, refcount:%d",
                 name, ff_bbuf->b_blocknr, open_inode->i_v2pnode_table[0], ff_bbuf, file_open_tp.tv_sec, atomic_get(&(open_inode->ref_count)));
    } else {
        ff_bbuf    = (userfs_bbuf_t *)USERFS_INODEADDR_GET(inodeaddr);
        ff_block   = ff_bbuf->b_blocknr;
        open_inode = USERFS_DBLOCK(ff_bbuf->b_data)->inode;
        atomic_add(&(open_inode->ref_count), 1);
        LOG_DESC(DBG, "USERFS FILE OPEN", "Refering file success, file:%s, first block:%u, dblock buf:%p, access time:0x%lx, refcount:%d",
                 name, ff_bbuf->b_blocknr, ff_bbuf, file_open_tp.tv_sec, atomic_get(&(open_inode->ref_count)));
    }
    open_inode->i_atime            = file_open_tp.tv_sec;
    open_inode->i_v2pnode_table[0] = (unsigned long)ff_bbuf;
    return ff_bbuf;
}

int userfs_file_close(
    const char           *name,
    const uint32_t        name_len,
    const uint32_t        dblock_shard_size,
    userfs_super_block_t *sb,
    linkhash_t           *dentry_hashtable)
{
    /*file name should less than 27B*/
    if (name_len > USERFS_MAX_FILE_NAME_LEN) {
        LOG_DESC(ERR, "USERFS FILE CLOSE", "File name too long, name len:%u, max len:%u",
                 name_len, USERFS_MAX_FILE_NAME_LEN);
        return -1;
    }

    uint32_t                   dentry_pos = 0;
    userfs_dhtable_inodeaddr_t inodeaddr  = userfs_dentry_hash_get(name, name_len, &dentry_pos, dentry_hashtable);
    if (inodeaddr == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE CLOSE", "Cannot find file:%s", name);
        return -1;
    }

    /*if file hasn't been opened yet, then dentry hashtable store inode number,
    otherwise store block buf address.
    If file hasn't been opened yet, then return*/
    if (USERFS_INODETYPE_GET(inodeaddr) == USERFS_NAME2INODE) {
        /*haven't been opened yet, not need to close*/
        LOG_DESC(ERR, "USERFS FILE CLOSE", "File hasn't been open, file:%s, first block:%lu",
                 name, USERFS_INODEADDR_GET(inodeaddr));
        return -1;
    }

    /*get current time as file latest access time stamp*/
    struct timeval file_close_tp = {0};
    if (gettimeofday(&file_close_tp, NULL) < 0) {
        perror("USERFS FILE CLOSE");
        return -1;
    }

    userfs_bbuf_t  *ff_bbuf     = (userfs_bbuf_t *)USERFS_INODEADDR_GET(inodeaddr);
    uint32_t        ff_block    = ff_bbuf->b_blocknr;
    userfs_inode_t *close_inode = USERFS_DBLOCK(ff_bbuf->b_data)->inode;
    LOG_DESC(DBG, "USERFS FILE CLOSE", "inode nr:%u, file blocks:%u", ff_bbuf->b_blocknr, close_inode->i_blocks);
    int res = atomic_sub(&(close_inode->ref_count), 1);
    /*if file still been opened by others, then doesn't close it*/
    if (res > 0) {
        LOG_DESC(DBG, "USERFS FILE CLOSE", "Dereferring file success, file:%s, first block:%u, dblock buf:%p, close time:0x%lx, refcount:%d",
                 name, ff_bbuf->b_blocknr, ff_bbuf, file_close_tp.tv_sec, res);
        return 0;
    }
    /*if file doesn't be opened by others, then close it*/
    /*update dentry hashtable from block buf address to inode number*/
    if (!userfs_dentry_hash_update(name, name_len, USERFS_NAME2INODE, ff_block, dentry_hashtable)) {
        LOG_DESC(ERR, "USERFS FILE CLOSE", "Update dentry hash table failed, file:%s, first block:%u, dblock buf:%p",
                 name, ff_bbuf->b_blocknr, ff_bbuf);
        res = -1;
    }
    /*rewrite data block number to inode block buf*/
    for (int i = 1; i < close_inode->i_blocks; i++) {
        userfs_bbuf_t *cur_dbbuf = (userfs_bbuf_t *)(close_inode->i_v2pnode_table[i]);
        if (cur_dbbuf != NULL) {
            close_inode->i_v2pnode_table[i] = cur_dbbuf->b_blocknr;
        }
    }
    /*flush all data to disk and free block buffer*/
    userfs_bbuf_t *next = ff_bbuf;
    userfs_bbuf_t *prev = NULL;
    while (next != NULL) {
        if ((res = userfs_dbbuf_list_flush(sb->s_first_datablock, sb->s_data_block_size, next, 1)) < 0) {
            LOG_DESC(ERR, "USERFS FILE CLOSE", "Flush data to disk failed, dblock nr:%u, in block off:0x%x, dblock size:0x%x",
                     next->b_blocknr, next->b_block_s_off, next->b_size);
        }
        prev = next;
        next = next->b_this_page;
        userfs_free_dbbuf(prev);
    }
    LOG_DESC(DBG, "USERFS FILE CLOSE", "Close file success, file:%s, first block:%u, dblock buf:%p, close time:0x%lx",
             name, ff_bbuf->b_blocknr, ff_bbuf, file_close_tp.tv_sec);
    return res;
}

int userfs_file_delete(
    const char            *name,
    const uint32_t         name_len,
    userfs_super_block_t  *sb,
    userfs_dentry_table_t *dentry_table,
    linkhash_t            *dentry_hashtable)
{
    /*file name should less than 27B*/
    if (name_len > USERFS_MAX_FILE_NAME_LEN) {
        LOG_DESC(ERR, "USERFS FILE DELETE", "File name too long, name len:%u, max len:%u",
                 name_len, USERFS_MAX_FILE_NAME_LEN);
        return -1;
    }

    /*delete and free dentry, considering dentry free strategy, also need to update dentry pos*/
    uint32_t                   dentry_pos = 0;
    userfs_dhtable_inodeaddr_t inodeaddr  = userfs_dentry_hash_remove(name, name_len, &dentry_pos, dentry_hashtable);
    if (inodeaddr == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE DELETE", "Cannot find file:%s", name);
        return -1;
    }
    LOG_DESC(DBG, "USERFS FILE DELETE", "Delete dentry from dentry hashtable, file:%s, pos:%u", name, dentry_pos);

    /*free dentry and update dentry pos for moved dentry*/
    int res = userfs_free_dentry(dentry_table, dentry_pos);
    if (!res) {
        userfs_dentry_t *moved_dentry = &(dentry_table->dentry[dentry_pos]);
        if (!userfs_dentry_hash_update_dentrypos(
                moved_dentry->d_name.name, strlen(moved_dentry->d_name.name), dentry_pos, dentry_hashtable)) {
            LOG_DESC(ERR, "USERFS FILE DELETE", "Update dentry pos for moved dentry failed, file:%s, new pos:%u", moved_dentry->d_name.name, dentry_pos);
            return -1;
        }
        LOG_DESC(DBG, "USERFS FILE DELETE", "Update dentry pos for moved dentry, file:%s, new pos:%u", moved_dentry->d_name.name, dentry_pos);
    }

    /*if file hasn't been opened yet, then dentry hashtable store inode number,
    otherwise store block buf address.
    If file isn't opened , then open inode and delete it*/
    userfs_bbuf_t *inodebbuf = NULL;
    if (USERFS_INODETYPE_GET(inodeaddr) == USERFS_NAME2INODEBUF) {
        /*haven't been opened yet, not need to close*/
        LOG_DESC(DBG, "USERFS FILE DELETE", "File is opened, file:%s, block buf addr:0x%lx",
                 name, USERFS_INODEADDR_GET(inodeaddr));
        inodebbuf = (userfs_bbuf_t *)USERFS_INODEADDR_GET(inodeaddr);
    } else {
        /*read inode from disk to free data blocks*/
        uint32_t inode_nr = (uint32_t)USERFS_INODEADDR_GET(inodeaddr);
        inodebbuf         = userfs_get_used_inode(sb, inode_nr, USERFS_PAGE_SIZE);
        if (!inodebbuf) {
            LOG_DESC(DBG, "USERFS FILE DELETE", "Read inode from disk failed, file:%s, inode number:%u",
                     name, inode_nr);
            return -1;
        }
    }

    /*get current time as file delete time stamp*/
    struct timeval file_delete_ops = {0};
    if (gettimeofday(&file_delete_ops, NULL) < 0) {
        perror("USERFS FILE CLOSE");
        return -1;
    }

    /*free data blocks alloc for file*/
    userfs_inode_t *delete_inode = USERFS_DBLOCK(inodebbuf->b_data)->inode;
    LOG_DESC(DBG, "USERFS FILE DELETE", "File inode info, inode number:%u, file sizes:%u, blocks count:%u, create time:0x%lx, last access time:0x%lx, delete time:0x%lx",
             inodebbuf->b_blocknr, delete_inode->i_size, delete_inode->i_blocks, delete_inode->i_ctime, delete_inode->i_atime, file_delete_ops.tv_sec);
    for (int i = 0; i < delete_inode->i_blocks; i++) {
        userfs_free_used_dblock();
    }
    userfs_bbuf_t *next = inodebbuf;
    userfs_bbuf_t *prev = NULL;
    while (next != NULL) {
        prev = next;
        next = prev->b_this_page;
        userfs_free_dbbuf(prev);
    }

    return 0;
}