#include "userfs_block_rw.h"
#include "disk_ops.h"
#include "inode.h"
#include "log.h"
#include "vnode.h"

int userfs_mbbuf_list_flush(
    uint32_t       first_mblock,
    uint32_t       metablock_size,
    userfs_bbuf_t *buf_list,
    uint16_t       buf_list_len)
{
    for (int i = 0; i < buf_list_len; i++) {
        uint64_t woff      = metablock_size * (buf_list->b_blocknr + first_mblock);
        uint32_t expect_wb = buf_list->b_size;
        uint32_t real_wb;
        if ((real_wb = user_disk_write(buf_list->b_data, expect_wb, woff)) != expect_wb) {
            LOG_DESC(ERR, "MBLOCK BUF FLUSH", "Flush failed, mblock id:%u, woff:0x%lxB, expect wbytes:0x%xB, real wbytes:0x%xB",
                     buf_list->b_blocknr, woff, expect_wb, real_wb);
            return -1;
        };
        LOG_DESC(DBG, "MBLOCK BUF FLUSH", "mblock id:%u, woff:0x%lxB, expect wbytes:0x%xB, real wbytes:0x%xB",
                 buf_list->b_blocknr, woff, expect_wb, real_wb);
        buf_list = buf_list->b_this_page;
    }
    return 0;
}

int userfs_mbbuf_list_read(
    uint32_t       first_mblock,
    uint32_t       metablock_size,
    userfs_bbuf_t *buf_list,
    uint16_t       buf_list_len)
{
    sizeof(userfs_inode_t);
    for (int i = 0; i < buf_list_len; i++) {
        uint64_t roff      = metablock_size * (buf_list->b_blocknr + first_mblock);
        uint32_t expect_rb = buf_list->b_size;
        uint32_t real_rb;
        if ((real_rb = user_disk_read(buf_list->b_data, expect_rb, roff)) != expect_rb) {
            LOG_DESC(ERR, "MBLOCK BUF READ", "Read failed, mblock id:%u, roff:0x%lxB, expect rbytes:0x%xB, real rbytes:0x%xB",
                     buf_list->b_blocknr, roff, expect_rb, real_rb);
            return -1;
        };
        LOG_DESC(DBG, "MBLOCK BUF READ", "mblock id:%u, roff:0x%lxB, expect rbytes:0x%xB, real rbytes:0x%xB",
                 buf_list->b_blocknr, roff, expect_rb, real_rb);
        buf_list = buf_list->b_this_page;
    }
    return 0;
}

int userfs_read_metadata_block(
    userfs_bbuf_t *mb_buf,
    uint32_t       first_mblock,
    uint32_t       metablock_size,
    uint32_t       target_mb_id)
{
    uint32_t expect_rb = metablock_size;
    mb_buf->b_blocknr  = target_mb_id;
    mb_buf->b_size     = metablock_size;

    uint32_t res       = userfs_mbbuf_list_read(first_mblock, metablock_size, mb_buf, 1);
    if (res < 0) {
        LOG_DESC(ERR, "READ METADATA BLOCK", "Read fail, read mblock id:%u, expect rbytes:0x%xB",
                 target_mb_id, expect_rb);
        return -1;
    }
    mb_buf->b_type = USERFS_MBLOCK(mb_buf->b_data)->block_type;
    return 0;
}

int userfs_dbbuf_list_flush(
    uint32_t       first_dblock,
    uint32_t       dblock_size,
    userfs_bbuf_t *buf_list,
    uint16_t       buf_list_len)
{
    for (int i = 0; i < buf_list_len; i++) {
        uint64_t woff = buf_list->b_block_s_off +
                        (uint64_t)(buf_list->b_blocknr + first_dblock) * (uint64_t)dblock_size;
        uint32_t expect_wb = buf_list->b_size;
        uint32_t real_wb;
        if ((real_wb = user_disk_write(buf_list->b_data, expect_wb, woff)) != expect_wb) {
            LOG_DESC(ERR, "MBLOCK BUF FLUSH", "Flush failed, mblock id:%u, woff:0x%lxB, in dblock off:0x%x, expect wbytes:0x%xB, real wbytes:0x%xB",
                     buf_list->b_blocknr, woff, buf_list->b_block_s_off, expect_wb, real_wb);
            return -1;
        };
        LOG_DESC(DBG, "MBLOCK BUF FLUSH", "mblock id:%u, woff:0x%lxB, in dblock off:0x%x, expect wbytes:0x%xB, real wbytes:0x%xB",
                 buf_list->b_blocknr, woff, buf_list->b_block_s_off, expect_wb, real_wb);
        buf_list = buf_list->b_this_page;
    }
    return 0;
}

int userfs_dbbuf_list_read(
    uint32_t       first_dblock,
    uint32_t       dblock_size,
    userfs_bbuf_t *buf_list,
    uint16_t       buf_list_len)
{
    for (int i = 0; i < buf_list_len; i++) {
        uint64_t roff = buf_list->b_block_s_off +
                        (uint64_t)dblock_size * (uint64_t)(buf_list->b_blocknr + first_dblock);
        uint32_t expect_rb = buf_list->b_size;
        uint32_t real_rb;
        if ((real_rb = user_disk_read(buf_list->b_data, expect_rb, roff)) != expect_rb) {
            LOG_DESC(ERR, "MBLOCK BUF READ", "Read failed, mblock id:%u, roff:0x%lxB, in dblock off:0x%x, expect rbytes:0x%xB, real rbytes:0x%xB",
                     buf_list->b_blocknr, roff, buf_list->b_block_s_off, expect_rb, real_rb);
            return -1;
        };
        LOG_DESC(DBG, "MBLOCK BUF READ", "mblock id:%u, roff:0x%lxB, expect rbytes:0x%xB, in dblock off:0x%x, real rbytes:0x%xB",
                 buf_list->b_blocknr, roff, buf_list->b_block_s_off, expect_rb, real_rb);
        buf_list = buf_list->b_this_page;
    }
    return 0;
}

int userfs_read_data_block(
    userfs_bbuf_t *db_buf,
    uint32_t       first_dblock,
    uint32_t       dblock_size)
{
    uint32_t expect_rb = db_buf->b_size;
    uint32_t res       = userfs_dbbuf_list_read(first_dblock, dblock_size, db_buf, db_buf->b_list_len);
    if (res < 0) {
        LOG_DESC(ERR, "READ METADATA BLOCK", "Read fail, read mblock id:%u, expect rbytes:0x%xB",
                 db_buf->b_blocknr, expect_rb);
        return -1;
    }
    db_buf->b_type = USERFS_MBLOCK(db_buf->b_data)->block_type;
    return 0;
}