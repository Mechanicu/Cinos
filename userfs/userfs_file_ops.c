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
    userfs_bbuf_t           *dentry_bbuf,
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
    userfs_dentry_table_t *dentry_table = USERFS_MBLOCK(dentry_bbuf->b_data)->dentry_table;
    uint32_t               dentry_pos   = userfs_alloc_dentry(dentry_table);
    if (dentry_pos == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE CREATE", "Alloc dentry for new file failed");
        return NULL;
    }
    /*alloc inode for new file*/
    userfs_bbuf_t *new_inode = userfs_get_new_inode(sb, bgd_idx_list, dblock_shard_size, &file_create_tp);
    if (!new_inode) {
        LOG_DESC(DBG, "USERFS FILE CREATE", "Alloc new inode failed");
        userfs_free_dentry(dentry_table, dentry_pos);
        return NULL;
    }
    strncpy(dentry_table->dentry[dentry_pos].d_name.name, name, name_len + 1);
    dentry_table->dentry[dentry_pos].d_first_dblock = new_inode->b_blocknr;
    /*insert new dentry in dentry hash table*/
    hash_obj_t *new_hash_dentry =
        userfs_dentry_hash_insert(name, name_len, USERFS_NAME2INODE, new_inode->b_blocknr, dentry_hashtable);
    if (!new_hash_dentry) {
        LOG_DESC(DBG, "USERFS FILE CREATE", "Insert in dentry hash table failed");
        userfs_free_dentry(dentry_table, dentry_pos);
        userfs_free_used_inode();
        return NULL;
    }

    return new_inode;
}

int userfs_file_delete(

)
{
}