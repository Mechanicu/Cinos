#ifndef USERFS_FILE_OPS
#define USERFS_FILE_OPS
#include "inode.h"
#include "log.h"
#include "userfs_dentry_hash.h"
#include "userfs_file_ctrl.h"
#include "userfs_heap.h"
#include "vnode.h"

userfs_bbuf_t *userfs_file_create(
    const char              *name,
    const uint32_t           name_len,
    const uint32_t           dblock_shard_size,
    userfs_super_block_t    *sb,
    linkhash_t              *dentry_hashtable,
    userfs_bbuf_t           *dentry_bbuf,
    userfs_bgd_index_list_t *bgd_idx_list);

userfs_bbuf_t *userfs_file_open(
    const char           *name,
    const uint32_t        name_len,
    const uint32_t        dblock_shard_size,
    userfs_super_block_t *sb,
    linkhash_t           *dentry_hashtable);

uint32_t userfs_file_write(
    const char              *buf,
    const uint32_t           woff,
    const uint32_t           size,
    const uint32_t           dblock_shard_size,
    userfs_bbuf_t           *inodebbuf,
    userfs_super_block_t    *sb,
    userfs_bgd_index_list_t *bgd_idx_list);

uint32_t userfs_file_read(
    char                 *buf,
    const uint32_t        roff,
    const uint32_t        size,
    const uint32_t        dblock_shard_size,
    userfs_bbuf_t        *inodebbuf,
    userfs_super_block_t *sb);

int userfs_file_close(
    const char           *name,
    const uint32_t        name_len,
    const uint32_t        dblock_shard_size,
    userfs_super_block_t *sb,
    linkhash_t           *dentry_hashtable);

int userfs_file_delete(
    const char            *name,
    const uint32_t         name_len,
    userfs_super_block_t  *sb,
    userfs_dentry_table_t *dentry_table,
    linkhash_t            *dentry_hashtable);
#endif