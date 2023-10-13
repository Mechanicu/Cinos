#include "bits.h"
#include "disk_ops.h"
#include "inode.h"
#include "log.h"
#include "pthread_spinlock.h"
#include "userfs_block_rw.h"
#include "vnode.h"
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define USERFS_MAX_BLOCK_SIZE  (1 << 10 << 10 << 6)
#define USERFS_MIN_BLOCK_SIZE  (1 << 10 << 10)
#define USERFS_MAX_FILE_SIZE32 UINT32_MAX

#if ENABLE_USERFS_MEMPOOL == 1
#include "mempool.h"
mempool_ctrl_t userfs_mem_pool;
#else
#define USERFS_MEM_ALLOC(size) malloc(size)
#define USERFS_MEM_FREE(ptr)        \
    while ((void *)(ptr) != NULL) { \
        free(ptr);                  \
        ptr = NULL;                 \
    }
#endif

#define USERFS_STATIC
#define USERFS_LOG_DESC                        "USERFS INIT"

#define USERFS_DEFAULT_MAX_MBBUF_FREE_LIST_LEN 256
#define USERFS_DEFAULT_MAX_DBBUF_FREE_LIST_LEN 256

userfs_super_block_t *g_sb;
pthread_spinlock_t    mbbuf_lock          = 1;
b_buf_t              *free_mbbuf_list     = NULL;
atomic_t              free_mbbuf_list_len = {0};
pthread_spinlock_t    dbbuf_lock          = 1;
b_buf_t              *free_dbbuf_list     = NULL;
atomic_t              free_dbbuf_list_len = {0};

USERFS_STATIC uint32_t get_real_block_size(
    const uint32_t expect_block_size)
{
    if (expect_block_size > USERFS_MAX_BLOCK_SIZE) {
        return USERFS_MAX_BLOCK_SIZE;
    }
    if (expect_block_size < USERFS_MIN_BLOCK_SIZE) {
        return USERFS_MIN_BLOCK_SIZE;
    }
    return ((uint32_t)1 << (ulog2(expect_block_size)));
}

USERFS_STATIC b_buf_t *userfs_alloc_mbbuf(
    uint32_t metablock_size)
{
    b_buf_t *mb_buf;
    /*to avoid frequently memory alloc, check free mbbuf list first*/
    if (free_mbbuf_list != NULL) {
        pthread_spin_lock(&mbbuf_lock);
        mb_buf          = free_mbbuf_list;
        free_mbbuf_list = free_mbbuf_list->b_this_page;
        pthread_spin_unlock(&mbbuf_lock);
        atomic_sub(&free_mbbuf_list_len, 1);
        LOG_DESC(DBG, "ALLOC MBLOCK BUF", "Alloc mbbuf:%p, next free one:%p",
                 mb_buf, free_mbbuf_list);
        return mb_buf;
    }
    /*if no avaliable mbbuf, then alloc one*/
    mb_buf = USERFS_MEM_ALLOC(sizeof(b_buf_t));
    if (!mb_buf) {
        LOG_DESC(ERR, "ALLOC MBLOCK BUF", "Alloc metadata block failed");
        return NULL;
    }
    memset(mb_buf, 0, sizeof(b_buf_t));
    mb_buf->b_data = USERFS_MEM_ALLOC(metablock_size);
    if (!(mb_buf->b_data)) {
        LOG_DESC(ERR, "ALLOC MBLOCK BUF", "Alloc metadata block data page failed");
        USERFS_MEM_FREE(mb_buf);
        return NULL;
    }
    memset(mb_buf->b_data, 0, metablock_size);
    return mb_buf;
}

USERFS_STATIC void userfs_free_mbbuf(
    b_buf_t *mb_buf)
{
    /**/
    int free_mbbuf_count = atomic_get(&free_mbbuf_list_len);
    if (free_mbbuf_count > USERFS_DEFAULT_MAX_MBBUF_FREE_LIST_LEN) {
        LOG_DESC(DBG, "FREE MBLOCK BUF", "Free mblock list already full, len:%u", free_mbbuf_count);
        USERFS_MEM_FREE(mb_buf->b_data);
        USERFS_MEM_FREE(mb_buf);
        return;
    }
    /*try to insert in free mbuf list*/
    atomic_add(&free_mbbuf_list_len, 1);
    pthread_spin_lock(&mbbuf_lock);
    mb_buf->b_this_page = free_mbbuf_list;
    free_mbbuf_list     = mb_buf->b_this_page;
    pthread_spin_unlock(&mbbuf_lock);
    LOG_DESC(DBG, "FREE MBBUF", "Current free mbbuf list len:%d", free_mbbuf_count + 1);
    return;
}

b_buf_t *userfs_get_new_metadata_block(
    userfs_super_block_t *sb,
    const uint8_t         mb_type)
{
    b_buf_t *mb_buf = userfs_alloc_mbbuf(sb->s_metablock_size);
    if (!mb_buf) {
        LOG_DESC(ERR, "GET NEW METADATA BLOCK", "Alloc mbbuf failed");
        return NULL;
    }
    uint32_t target_mb_id = bitmap_get_first_free(&sb->s_metadata_block_bitmap);
    /*if request new block, get one from super block's bitmap*/
    if (target_mb_id == (uint32_t)-1) {
        LOG_DESC(ERR, "GET NEW METADATA BLOCK", "No free mblock on disk");
        userfs_free_mbbuf(mb_buf);
        return NULL;
    }
    sb->s_free_metablock_count--;
    mb_buf->b_block_s_off = 0;
    mb_buf->b_blocknr     = target_mb_id;
    mb_buf->b_size        = sb->s_metablock_size;
    mb_buf->b_type        = mb_type;
    LOG_DESC(DBG, "GET NEW METADATA BLOCK", "New mblock, block id:%u, block type:%u, mbbuf:%p, free mblock count:%u",
             mb_buf->b_blocknr, mb_buf->b_type, mb_buf, sb->s_free_metablock_count);
    return mb_buf;
}

b_buf_t *userfs_get_used_metadata_block(
    const uint32_t metablock_size,
    const uint32_t first_mb_id,
    const uint32_t mb_id)
{
    b_buf_t *mb_buf = userfs_alloc_mbbuf(metablock_size);
    if (!mb_buf) {
        LOG_DESC(ERR, "GET USED METADATA BLOCK", "Alloc mbbuf failed");
        return NULL;
    }
    uint32_t target_mb_id = mb_id;
    /*if request used block, then must read from disk to avoid data overwrite*/
    if (userfs_read_metadata_block(first_mb_id, mb_buf, metablock_size, target_mb_id) < 0) {
        LOG_DESC(ERR, "GET USED METADATA BLOCK", "Read metadata on disk failed");
        userfs_free_mbbuf(mb_buf);
        return NULL;
    }
    LOG_DESC(DBG, "GET USED METADATA BLOCK", "Used mblock, block id:%u, block type:%u, mbbuf:%p",
             mb_buf->b_blocknr, mb_buf->b_type, mb_buf);
    return mb_buf;
}

uint32_t userfs_get_used_metadata_blocklist(
    userfs_super_block_t *sb,
    const uint32_t       *mb_id,
    const uint16_t        mb_id_count,
    b_buf_t             **mb_buf_list)
{
    *mb_buf_list            = NULL;
    uint32_t real_get_count = 0;
    for (int i = 0; i < mb_id_count; i++) {
        b_buf_t *mb_buf = userfs_alloc_mbbuf(sb->s_metablock_size);
        if (!mb_buf) {
            LOG_DESC(ERR, "GET USED METADATA BLOCK LIST", "Alloc mbbuf failed");
            break;
        }
        uint32_t target_mb_id = mb_id[i];
        mb_buf->b_block_s_off = 0;
        mb_buf->b_size        = sb->s_metablock_size;
        /*if request used block, then must read from disk to avoid data overwrite*/
        if (userfs_read_metadata_block(sb->s_first_metablock, mb_buf, sb->s_metablock_size, target_mb_id) < 0) {
            LOG_DESC(ERR, "GET USED METADATA BLOCK LIST", "Read metadata on disk failed");
            userfs_free_mbbuf(mb_buf);
            continue;
        }
        LOG_DESC(DBG, "GET USED METADATA BLOCK LIST", "Used mblock, block id:%u, block type:%u, mbbuf:%p",
                 mb_buf->b_blocknr, mb_buf->b_type, mb_buf);
        mb_buf->b_list_len  = real_get_count;
        mb_buf->b_this_page = *mb_buf_list;
        *mb_buf_list        = mb_buf;
        real_get_count++;
    }
    return real_get_count;
}

/*userfs super block create*/
userfs_super_block_t *userfs_suber_block_alloc(
    const uint32_t block_size,
    const uint32_t metadata_block_size,
    const uint64_t disk_size,
    uint32_t      *r_block_size,
    b_buf_t      **sb_bbuf)
{
    uint32_t real_block_size      = get_real_block_size(block_size);
    uint64_t total_block_count    = disk_size / real_block_size;
    uint32_t metadata_block_count = real_block_size / metadata_block_size;

    /*alloc block buffer for super block*/
    b_buf_t *sb_block_buf         = userfs_alloc_mbbuf(metadata_block_size);
    if (!sb_block_buf) {
        LOG_DESC(ERR, USERFS_LOG_DESC, "alloc block buffer for super block failed");
        return NULL;
    }

    /*WARING: because using zero-len bitmap, when alloc super block, must calculate
    bytes that bitmap needs and alloc*/
    userfs_super_block_t *sb    = USERFS_MBLOCK(sb_block_buf->b_data)->sb;
    /*block size*/
    sb->s_data_block_count      = total_block_count - 1;
    sb->s_free_data_block_count = sb->s_data_block_count;
    sb->s_data_block_size       = real_block_size;
    /*metadata block*/
    bitmap_init(&sb->s_metadata_block_bitmap, metadata_block_count >> 3);
    sb->s_metablock_count      = metadata_block_count;
    sb->s_metablock_size       = metadata_block_size;
    sb->s_free_metablock_count = sb->s_metablock_count;
    sb->s_first_metablock      = 0;
    if (r_block_size != NULL) {
        *r_block_size = real_block_size;
    }
    g_sb                                            = sb;

    /**/
    USERFS_MBLOCK(sb_block_buf->b_data)->block_type = USERFS_BTYPE_SUPER;
    USERFS_MBLOCK(sb_block_buf->b_data)->h.next     = sb->s_first_metablock;
    USERFS_MBLOCK(sb_block_buf->b_data)->h.prev     = sb->s_first_metablock;
    /**/
    sb_block_buf->b_type                            = USERFS_BTYPE_SUPER;
    sb_block_buf->b_blocknr                         = bitmap_get_first_free(&sb->s_metadata_block_bitmap);
    sb_block_buf->b_size                            = metadata_block_size;
    *sb_bbuf                                        = sb_block_buf;
    /*init free block buf list and lock*/
    pthread_spin_init(&mbbuf_lock, 0);
    pthread_spin_init(&dbbuf_lock, 0);
    return g_sb;
}

USERFS_STATIC void userfs_bgroup_desc_block_init(
    const b_buf_t *bgroup_desc_bbuf,
    const uint32_t blocks_per_group,
    const uint32_t bgroup_desc_per_mb_count,
    const uint32_t prev_block,
    const uint32_t next_block)
{
    userfs_mblock_t *block            = USERFS_MBLOCK(bgroup_desc_bbuf->b_data);
    block->h.next                     = next_block;
    block->h.prev                     = prev_block;
    block->block_type                 = USERFS_BTYPE_BGROUP_DESC;
    userfs_bgroup_desc_t *cur_bg_desc = block->bg_desc_table;
    for (int i = 0; i < bgroup_desc_per_mb_count; i++) {
        cur_bg_desc->bg_block_count      = blocks_per_group;
        cur_bg_desc->bg_free_block_count = cur_bg_desc->bg_block_count;
        bitmap_init(&cur_bg_desc->block_bm, blocks_per_group >> 3);
    }
}

/*userfs block group descriptor table init*/
b_buf_t *userfs_bgroup_desc_table_init(
    userfs_super_block_t *sb,
    const uint32_t        blocks_per_group)
{
    /*calculate mblock that bgroup descriptors table needs*/
    b_buf_t *bgroup_desc_table_list   = NULL;
    uint32_t bitmap_len               = (blocks_per_group + 8) >> 3;
    uint32_t bgroup_desc_size         = sizeof(userfs_bgroup_desc_t) + bitmap_len;
    uint32_t bgroup_desc_per_mb_count = (sb->s_metablock_size - sizeof(userfs_mblock_t)) / bgroup_desc_size;
    uint32_t bgroup_desc_count        = (sb->s_data_block_count + blocks_per_group - 1) / blocks_per_group;
    uint32_t bgroup_desc_mb_count     = (bgroup_desc_count + bgroup_desc_per_mb_count - 1) / bgroup_desc_per_mb_count;
    LOG_DESC(DBG, "BGROUP DESC TABLE INIT", "bg_desc count:%u, \
bg_desc size:%uB, bg_desc per mb:%u, mb bg_desc needs:%u",
             bgroup_desc_count,
             bgroup_desc_size,
             bgroup_desc_per_mb_count,
             bgroup_desc_mb_count);

    /*alloc mblock buffer list for bgroup descriptors table*/
    uint32_t prev_block    = sb->s_first_metablock;
    bgroup_desc_table_list = userfs_get_new_metadata_block(sb, USERFS_BTYPE_BGROUP_DESC);
    if (!bgroup_desc_table_list) {
        LOG_DESC(ERR, "BGROUP DESC TABLE INIT", "Alloc mbbuf for bgroup_desc_table failed");
        return NULL;
    }
    bgroup_desc_table_list->b_list_len = 0;
    userfs_bgroup_desc_block_init(bgroup_desc_table_list, blocks_per_group,
                                  bgroup_desc_per_mb_count, prev_block, sb->s_first_metablock);
    prev_block                     = bgroup_desc_table_list->b_blocknr;
    sb->s_first_bgroup_desc_mblock = bgroup_desc_table_list->b_blocknr;

    for (int i = 1; i < bgroup_desc_mb_count; i++) {
        b_buf_t *new_bbuf = userfs_get_new_metadata_block(sb, USERFS_BTYPE_BGROUP_DESC);
        if (!new_bbuf) {
            LOG_DESC(ERR, "BGROUP DESC TABLE INIT", "Alloc mbbuf for bgroup_desc_table failed, current alloc mbbuf:%d",
                     bgroup_desc_table_list->b_list_len);
            break;
        }
        userfs_bgroup_desc_block_init(new_bbuf, blocks_per_group,
                                      bgroup_desc_per_mb_count, prev_block, sb->s_first_metablock);
        prev_block                                            = new_bbuf->b_blocknr;
        USERFS_MBLOCK(bgroup_desc_table_list->b_data)->h.next = new_bbuf->b_blocknr;

        new_bbuf->b_list_len                                  = i;
        new_bbuf->b_this_page                                 = bgroup_desc_table_list;
        bgroup_desc_table_list                                = new_bbuf;
    }
    LOG_DESC(DBG, "BGROUP DESC TABLE INIT", "Alloc mbbuf count:%hu for bgroup_desc_table", bgroup_desc_table_list->b_list_len);
    sb->s_data_block_group_count      = bgroup_desc_count;
    sb->s_data_block_per_group        = blocks_per_group;
    sb->s_free_data_block_group_count = sb->s_data_block_group_count;

    return bgroup_desc_table_list;
}

b_buf_t *userfs_mount_init(
    const uint32_t metablock_size,
    const uint32_t first_metablock_id)
{
    /*read super block from disk*/
    b_buf_t *sb_buf = userfs_get_used_metadata_block(metablock_size, first_metablock_id, 0);
    if (!sb_buf || sb_buf->b_type != USERFS_BTYPE_SUPER) {
        LOG_DESC(ERR, "USERFS MOUNT INIT", "Get super block from disk failed");
        userfs_free_mbbuf(sb_buf);
        return NULL;
    }
    userfs_super_block_t *sb    = USERFS_MBLOCK(sb_buf->b_data)->sb;

    /*read block group descriptors blocks from disk*/
    b_buf_t *bg_desc_block_list = NULL;
    uint32_t bg_desc_list_len   = 0;
    uint32_t next_bg_desc_block = sb->s_first_bgroup_desc_mblock;
    while (next_bg_desc_block != sb->s_first_metablock) {
        b_buf_t *cur_mb_buf = userfs_get_used_metadata_block(metablock_size, first_metablock_id, next_bg_desc_block);
        if (!cur_mb_buf) {
            LOG_DESC(ERR, "USERFS MOUNT INIT", "Fail to get mblock:%u", next_bg_desc_block);
            break;
        }
        cur_mb_buf->b_list_len = bg_desc_list_len;
        bg_desc_list_len++;
        cur_mb_buf->b_this_page = bg_desc_block_list;
        bg_desc_block_list      = cur_mb_buf;
        next_bg_desc_block      = USERFS_MBLOCK(cur_mb_buf->b_data)->h.next;
        LOG_DESC(DBG, "USERFS MOUNT INIT", "Get used mblock:%u, type:%u, count:%u",
                 next_bg_desc_block, cur_mb_buf->b_type, bg_desc_list_len);
    }
    sb_buf->b_this_page = bg_desc_block_list;
    return sb_buf;
}