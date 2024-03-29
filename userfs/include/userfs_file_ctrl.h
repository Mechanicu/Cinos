#ifndef USERFS_FILE_CTRL_H
#define USERFS_FILE_CTRL_H
#include "inode.h"
#include "userfs_block_rw.h"
#include "userfs_dentry_hash.h"
#include "vnode.h"
#include <sys/time.h>

#define USERFS_DEFAULT_DATA_BLOCK_SHARD_SIZE (1 << 10 << 10 << 2)

void userfs_free_dbbuf(
    userfs_bbuf_t *db_buf);

userfs_bbuf_t *userfs_alloc_dbbuf(
    uint32_t dblock_shardsize);

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

userfs_bbuf_t *userfs_get_new_inode(
    userfs_super_block_t    *sb,
    userfs_bgd_index_list_t *bgd_idx_list,
    uint32_t                 dblock_shard_size,
    struct timeval          *file_create_tp);

userfs_bbuf_t *userfs_get_used_inode(
    userfs_super_block_t *sb,
    uint32_t              inode_dblock_nr,
    uint32_t              dblock_shard_size);

void userfs_free_used_inode();

userfs_dhtable_inodeaddr_t userfs_name2inode(
    linkhash_t    *dentry_hashtable,
    const char    *name,
    uint32_t      *dentry_pos,
    const uint32_t name_len);

userfs_bbuf_t *userfs_free_used_dblock();
#endif