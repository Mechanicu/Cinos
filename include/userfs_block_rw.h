#ifndef USERFS_BLOCK_RW_H
#define USERFS_BLOCK_RW_H
#include "vnode.h"
#include <stdint.h>

int userfs_mbbuf_list_flush(
    uint32_t       first_mblock,
    uint32_t       metablock_size,
    userfs_bbuf_t *buf_list,
    uint16_t       buf_list_len);

int userfs_mbbuf_list_read(
    uint32_t       first_mblock,
    uint32_t       metablock_size,
    userfs_bbuf_t *buf_list,
    uint16_t       buf_list_len);

int userfs_dbbuf_list_flush(
    uint32_t       first_dblock,
    uint32_t       dblock_size,
    userfs_bbuf_t *buf_list,
    uint16_t       buf_list_len);

int userfs_dbbuf_list_read(
    uint32_t       first_dblock,
    uint32_t       dblock_size,
    userfs_bbuf_t *buf_list,
    uint16_t       buf_list_len);

int userfs_read_metadata_block(
    userfs_bbuf_t *mb_buf,
    uint32_t       first_mblock,
    uint32_t       metablock_size,
    uint32_t       target_mb_id);

int userfs_read_data_block(
    userfs_bbuf_t *db_buf,
    uint32_t       first_dblock,
    uint32_t       dblock_size);
#endif