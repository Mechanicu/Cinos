#ifndef USERFS_BLOCK_RW_H
#define USERFS_BLOCK_RW_H
#include "vnode.h"
#include <stdint.h>

int userfs_mbbuf_list_flush(
    uint32_t first_mblock,
    uint32_t metablock_size,
    userfs_bbuf_t *buf_list,
    uint16_t buf_list_len);

int userfs_mbbuf_list_read(
    uint32_t first_mblock,
    uint32_t metablock_size,
    userfs_bbuf_t *buf_list,
    uint16_t buf_list_len);

int userfs_read_metadata_block(
    uint32_t first_mblock,
    userfs_bbuf_t *mb_buf,
    uint32_t metablock_size,
    uint32_t target_mb_id);
#endif