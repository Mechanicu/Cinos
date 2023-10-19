#ifndef USERFS_FILE_CTRL_H
#define USERFS_FILE_CTRL_H
#include "inode.h"
#include "userfs_block_rw.h"
#include "vnode.h"

#define USERFS_DEFAULT_DATA_BLOCK_SHARD_SIZE (1 << 10 << 10 << 2)

uint32_t userfs_alloc_dentry(
    userfs_dentry_table_t *dentry_table);
int userfs_free_dentry(
    userfs_dentry_table_t *dentry_table,
    uint32_t               dentry_pos);
userfs_bbuf_t *userfs_get_new_dblock(
    userfs_super_block_t    *sb,
    userfs_bgd_index_list_t *bgd_idx_list,
    uint32_t                 dblock_shard_size);
userfs_bbuf_t *userfs_get_used_dblock(
    userfs_super_block_t *sb,
    uint32_t              dblock_nr,
    uint32_t              dblock_off,
    uint32_t              dblock_shard_size);
    
#endif