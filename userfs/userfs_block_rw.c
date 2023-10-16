#include "userfs_block_rw.h"
#include "disk_ops.h"
#include "inode.h"
#include "log.h"
#include "vnode.h"

int userfs_mbbuf_list_flush(
    uint32_t first_mblock,
    uint32_t metablock_size,
    userfs_bbuf_t *buf_list,
    uint16_t buf_list_len)
{
    userfs_bbuf_t *cur_flush_buf = buf_list;
    for (int i = 0; i < buf_list_len; i++) {
        off_t    woff      = metablock_size * (buf_list->b_blocknr + first_mblock);
        uint32_t expect_wb = buf_list->b_size;
        uint32_t real_wb;
        if ((real_wb = user_disk_write(buf_list->b_data, expect_wb, woff)) != expect_wb) {
            LOG_DESC(ERR, "MBLOCK BUF FLUSH", "Flush failed, mblock id:%u, woff:%luB, expect wbytes:%uB, real wbytes:%uB",
                     buf_list->b_blocknr, woff, expect_wb, real_wb);
            return -1;
        };
        LOG_DESC(DBG, "MBLOCK BUF FLUSH", "mblock id:%u, woff:%luB, expect wbytes:%uB, real wbytes:%uB",
                 buf_list->b_blocknr, woff, expect_wb, real_wb);
        buf_list = buf_list->b_this_page;
    }
    return 0;
}

int userfs_mbbuf_list_read(
    uint32_t first_mblock,
    uint32_t metablock_size,
    userfs_bbuf_t *buf_list,
    uint16_t buf_list_len)
{
    userfs_bbuf_t *cur_flush_buf = buf_list;
    for (int i = 0; i < buf_list_len; i++) {
        off_t    roff      = metablock_size * (buf_list->b_blocknr + first_mblock);
        uint32_t expect_rb = buf_list->b_size;
        uint32_t real_rb;
        if ((real_rb = user_disk_read(buf_list->b_data, expect_rb, roff)) != expect_rb) {
            LOG_DESC(ERR, "MBLOCK BUF READ", "Read failed, mblock id:%u, roff:%luB, expect rbytes:%uB, real rbytes:%uB",
                     buf_list->b_blocknr, roff, expect_rb, real_rb);
            return -1;
        };
        LOG_DESC(DBG, "MBLOCK BUF READ", "mblock id:%u, roff:%luB, expect rbytes:%uB, real rbytes:%uB",
                 buf_list->b_blocknr, roff, expect_rb, real_rb);
        buf_list = buf_list->b_this_page;
    }
    return 0;
}

int userfs_read_metadata_block(
    uint32_t first_mblock,
    userfs_bbuf_t *mb_buf,
    uint32_t metablock_size,
    uint32_t target_mb_id)
{
    uint32_t expect_rb = metablock_size;
    mb_buf->b_blocknr  = target_mb_id;
    mb_buf->b_size     = metablock_size;

    uint32_t res       = userfs_mbbuf_list_read(first_mblock, metablock_size, mb_buf, 1);
    if (res < 0) {
        LOG_DESC(ERR, "READ METADATA BLOCK", "Read fail, read mblock id:%u, expect rbytes:%uB",
                 target_mb_id, expect_rb);
        return -1;
    }
    mb_buf->b_type = USERFS_MBLOCK(mb_buf->b_data)->block_type;
    return 0;
}