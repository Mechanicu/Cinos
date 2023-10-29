#include "bits.h"
#include "disk_ops.h"
#include "inode.h"
#include "log.h"
#include "pthread_spinlock.h"
#include "userfs_block_rw.h"
#include "userfs_dentry_hash.h"
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

#define USERFS_STATIC

#define USERFS_DEFAULT_MAX_MBBUF_FREE_LIST_LEN 256

static userfs_super_block_t *g_sb;
static pthread_spinlock_t    mbbuf_lock          = 1;
static userfs_bbuf_t        *free_mbbuf_list     = NULL;
static atomic_t              free_mbbuf_list_len = {0};

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

USERFS_STATIC userfs_bbuf_t *userfs_alloc_mbbuf(
    uint32_t metablock_size)
{
    userfs_bbuf_t *mb_buf;
    /*to avoid frequently memory alloc, check free mbbuf list first*/
    if (free_mbbuf_list != NULL) {
        pthread_spin_lock(&mbbuf_lock);
        mb_buf          = free_mbbuf_list;
        free_mbbuf_list = free_mbbuf_list->b_this_page;
        pthread_spin_unlock(&mbbuf_lock);
        atomic_sub(&free_mbbuf_list_len, 1);
        LOG_DESC(DBG, "ALLOC MBLOCK BUF", "Alloc mbbuf:%p, next free one:%p",
                 mb_buf, free_mbbuf_list);
        memset(mb_buf, 0, sizeof(userfs_bbuf_t));
        mb_buf->b_size     = metablock_size;
        mb_buf->b_list_len = 1;
        return mb_buf;
    }
    /*if no avaliable mbbuf, then alloc one*/
    mb_buf = USERFS_MEM_ALLOC(sizeof(userfs_bbuf_t));
    if (!mb_buf) {
        LOG_DESC(ERR, "ALLOC MBLOCK BUF", "Alloc metadata block failed");
        return NULL;
    }
    memset(mb_buf, 0, sizeof(userfs_bbuf_t));
    mb_buf->b_data = USERFS_MEM_ALLOC(metablock_size);
    if (!(mb_buf->b_data)) {
        LOG_DESC(ERR, "ALLOC MBLOCK BUF", "Alloc metadata block data page failed");
        USERFS_MEM_FREE(mb_buf);
        return NULL;
    }
    mb_buf->b_size     = metablock_size;
    mb_buf->b_list_len = 1;
    return mb_buf;
}

USERFS_STATIC void userfs_free_mbbuf(
    userfs_bbuf_t *mb_buf)
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

userfs_bbuf_t *userfs_get_new_metadata_block(
    userfs_super_block_t *sb,
    const uint8_t         mb_type)
{
    userfs_bbuf_t *mb_buf = userfs_alloc_mbbuf(sb->s_metablock_size);
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
    mb_buf->b_type        = mb_type;
    LOG_DESC(DBG, "GET NEW METADATA BLOCK", "New mblock, block id:%u, block type:%u, mbbuf:%p, free mblock count:%u",
             mb_buf->b_blocknr, mb_buf->b_type, mb_buf, sb->s_free_metablock_count);
    return mb_buf;
}

userfs_bbuf_t *userfs_get_used_metadata_block(
    const uint32_t metablock_size,
    const uint32_t first_mb_id,
    const uint32_t mb_id)
{
    userfs_bbuf_t *mb_buf = userfs_alloc_mbbuf(metablock_size);
    if (!mb_buf) {
        LOG_DESC(ERR, "GET USED METADATA BLOCK", "Alloc mbbuf failed");
        return NULL;
    }
    uint32_t target_mb_id = mb_id;
    /*if request used block, then must read from disk to avoid data overwrite*/
    if (userfs_read_metadata_block(mb_buf, first_mb_id, metablock_size, target_mb_id) < 0) {
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
    userfs_bbuf_t       **mb_buf_list)
{
    userfs_bbuf_t dummy;
    *mb_buf_list            = &dummy;
    uint32_t real_get_count = 0;
    for (int i = 0; i < mb_id_count; i++) {
        userfs_bbuf_t *mb_buf = userfs_alloc_mbbuf(sb->s_metablock_size);
        if (!mb_buf) {
            LOG_DESC(ERR, "GET USED METADATA BLOCK LIST", "Alloc mbbuf failed");
            break;
        }
        uint32_t target_mb_id = mb_id[i];
        mb_buf->b_block_s_off = 0;
        /*if request used block, then must read from disk to avoid data overwrite*/
        if (userfs_read_metadata_block(mb_buf, sb->s_first_metablock, sb->s_metablock_size, target_mb_id) < 0) {
            LOG_DESC(ERR, "GET USED METADATA BLOCK LIST", "Read metadata on disk failed");
            userfs_free_mbbuf(mb_buf);
            continue;
        }
        LOG_DESC(DBG, "GET USED METADATA BLOCK LIST", "Used mblock, block id:%u, block type:%u, mbbuf:%p",
                 mb_buf->b_blocknr, mb_buf->b_type, mb_buf);
        mb_buf->b_list_len          = real_get_count;
        (*mb_buf_list)->b_this_page = mb_buf;
        *mb_buf_list                = mb_buf;
        real_get_count++;
    }
    *mb_buf_list               = dummy.b_this_page;
    (*mb_buf_list)->b_list_len = real_get_count;
    return real_get_count;
}

/*userfs super block create*/
userfs_super_block_t *userfs_suber_block_alloc(
    const uint32_t  block_size,
    const uint32_t  metadata_block_size,
    const uint64_t  disk_size,
    uint32_t       *r_block_size,
    userfs_bbuf_t **sb_bbuf)
{
    uint32_t real_block_size      = get_real_block_size(block_size);
    uint64_t total_block_count    = disk_size / real_block_size;
    uint32_t metadata_block_count = real_block_size / metadata_block_size;

    /*alloc block buffer for super block*/
    userfs_bbuf_t *sb_block_buf   = userfs_alloc_mbbuf(metadata_block_size);
    if (!sb_block_buf) {
        LOG_DESC(ERR, "USERFS INIT", "alloc block buffer for super block failed");
        return NULL;
    }

    /*WARING: because using zero-len bitmap, when alloc super block, must calculate
    bytes that bitmap needs and alloc*/
    userfs_super_block_t *sb    = USERFS_MBLOCK(sb_block_buf->b_data)->sb;
    /*data block*/
    sb->s_first_datablock       = 1;
    sb->s_data_block_count      = total_block_count - 1;
    sb->s_free_data_block_count = sb->s_data_block_count - 1;
    sb->s_data_block_size       = real_block_size;
    /*metadata block*/
    bitmap_init(&sb->s_metadata_block_bitmap, metadata_block_count >> 3);
    sb->s_metablock_count      = metadata_block_count;
    sb->s_metablock_size       = metadata_block_size;
    sb->s_free_metablock_count = sb->s_metablock_count;
    sb->s_first_metablock      = 0;
    /*dentry table init*/
    sb->s_first_dentry_mblock  = 0;
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
    *sb_bbuf                                        = sb_block_buf;
    /*init free block buf list and lock*/
    pthread_spin_init(&mbbuf_lock, 0);
    return g_sb;
}

USERFS_STATIC void userfs_bgroup_desc_block_init(
    const userfs_bbuf_t *bgroup_desc_bbuf,
    const uint32_t       blocks_per_group,
    const uint32_t       bgroup_desc_per_mb_count,
    const uint32_t       bgroup_desc_size,
    const uint32_t       prev_block,
    const uint32_t       next_block)
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
        cur_bg_desc =
            (userfs_bgroup_desc_t *)((char *)cur_bg_desc + bgroup_desc_size);
    }
}

/*userfs block group descriptor table init*/
userfs_bbuf_t *userfs_bgroup_desc_table_init(
    userfs_super_block_t *sb,
    const uint32_t        blocks_per_group)
{
    /*calculate mblock that bgroup descriptors table needs*/
    userfs_bbuf_t *bgroup_desc_table_list   = NULL;
    userfs_bbuf_t *bgroup_desc_table_lh     = NULL;
    uint32_t       bitmap_len               = (blocks_per_group + 8) >> 3;
    uint32_t       bgroup_desc_size         = sizeof(userfs_bgroup_desc_t) + bitmap_len;
    uint32_t       bgroup_desc_per_mb_count = (sb->s_metablock_size - sizeof(userfs_mblock_t)) / bgroup_desc_size;
    uint32_t       bgroup_desc_count        = (sb->s_data_block_count + blocks_per_group - 1) / blocks_per_group;
    uint32_t       bgroup_desc_mb_count     = (bgroup_desc_count + bgroup_desc_per_mb_count - 1) / bgroup_desc_per_mb_count;
    LOG_DESC(DBG, "BGROUP DESC TABLE INIT", "bg_desc count:%u, dblocks per bg_desc:%u, bg_desc size:%uB, bg_desc per mb:%u, mb bg_desc needs:%u",
             bgroup_desc_count,
             blocks_per_group,
             bgroup_desc_size,
             bgroup_desc_per_mb_count,
             bgroup_desc_mb_count);

    /*alloc mblock buffer list for bgroup descriptors table*/
    uint32_t prev_block    = sb->s_first_metablock;
    uint32_t next_block    = sb->s_first_metablock;
    bgroup_desc_table_list = userfs_get_new_metadata_block(sb, USERFS_BTYPE_BGROUP_DESC);
    if (!bgroup_desc_table_list) {
        LOG_DESC(ERR, "BGROUP DESC TABLE INIT", "Alloc mbbuf for bgroup_desc_table failed");
        return NULL;
    }
    bgroup_desc_table_list->b_list_len = 1;
    userfs_bgroup_desc_block_init(bgroup_desc_table_list, blocks_per_group,
                                  bgroup_desc_per_mb_count, bgroup_desc_size, prev_block, next_block);
    prev_block                     = bgroup_desc_table_list->b_blocknr;
    sb->s_first_bgroup_desc_mblock = bgroup_desc_table_list->b_blocknr;
    bgroup_desc_table_lh           = bgroup_desc_table_list;

    for (int i = 1; i < bgroup_desc_mb_count; i++, bgroup_desc_table_lh->b_list_len++) {
        userfs_bbuf_t *new_bbuf = userfs_get_new_metadata_block(sb, USERFS_BTYPE_BGROUP_DESC);
        if (!new_bbuf) {
            LOG_DESC(ERR, "BGROUP DESC TABLE INIT", "Alloc mbbuf for bgroup_desc_table failed, current alloc mbbuf:%d",
                     bgroup_desc_table_list->b_list_len);
            break;
        }
        userfs_bgroup_desc_block_init(new_bbuf, blocks_per_group,
                                      bgroup_desc_per_mb_count, bgroup_desc_size, prev_block, next_block);
        prev_block                                            = new_bbuf->b_blocknr;
        USERFS_MBLOCK(bgroup_desc_table_list->b_data)->h.next = new_bbuf->b_blocknr;

        bgroup_desc_table_list->b_this_page                   = new_bbuf;
        bgroup_desc_table_list                                = new_bbuf;
    }
    sb->s_data_block_group_count      = bgroup_desc_count;
    sb->s_data_block_per_group        = blocks_per_group;
    sb->s_free_data_block_group_count = sb->s_data_block_group_count;
    sb->s_bgroup_desc_per_mb_count    = bgroup_desc_per_mb_count;
    sb->s_bgroup_desc_size            = bgroup_desc_size;
    LOG_DESC(DBG, "BGROUP DESC TABLE INIT", "Alloc mbbuf count:%hu for bgd_table, bgroup count:%u, bgd per mblock count:%u, bgd size:%u",
             bgroup_desc_table_list->b_list_len, sb->s_data_block_group_count, sb->s_bgroup_desc_per_mb_count, sb->s_bgroup_desc_size);

    return bgroup_desc_table_lh;
}

USERFS_STATIC void userfs_dentry_table_block_init(
    const userfs_bbuf_t *dentry_table_bbuf,
    const uint32_t       dentry_per_mb_count,
    const uint32_t       prev_block,
    const uint32_t       next_block)
{
    userfs_mblock_t *block                = USERFS_MBLOCK(dentry_table_bbuf->b_data);
    block->h.next                         = next_block;
    block->h.prev                         = prev_block;
    block->block_type                     = USERFS_BTYPE_DENTRY;
    userfs_dentry_table_t *dentry_table   = block->dentry_table;
    dentry_table->h.dfd_dentry_count      = dentry_per_mb_count;
    dentry_table->h.dfd_first_dentry      = 0;
    dentry_table->h.dfd_first_free_dentry = 0;
    dentry_table->h.dfd_used_dentry_count = 0;
}

userfs_bbuf_t *userfs_root_dentry_table_init(
    userfs_super_block_t *sb)
{
    /*ISSUE: when dentry alloc reach last two obj of dentry table,
    read/write dentry table will cause incorrect data, UNKNOWN REASON,
    MAYBE HAVE TO DO WITH ILLEGAL WRITE? JUST DO NOT USE THEM CAN AVOID
    ISSUE FOR NOW*/
    uint32_t dentry_per_mb_count = sb->s_metablock_size / sizeof(userfs_dentry_t) - 2;
    uint32_t prev_block          = sb->s_first_metablock;
    uint32_t next_block          = sb->s_first_metablock;
    LOG_DESC(DBG, "ROOT DENTRY TABLE INIT", "dentry size:0x%xB, max filename len:%u, dentry per mb:%u",
             (uint32_t)sizeof(userfs_dentry_t), (uint32_t)USERFS_MAX_FILE_NAME_LEN, dentry_per_mb_count);

    userfs_bbuf_t *dentry_table_bbuf = userfs_get_new_metadata_block(sb, USERFS_BTYPE_DENTRY);
    if (!dentry_table_bbuf) {
        LOG_DESC(ERR, "ROOT DENTRY TABLE INIT", "Alloc mbbuf for dentry table failed");
        return NULL;
    }

    dentry_table_bbuf->b_list_len = 1;
    userfs_dentry_table_block_init(dentry_table_bbuf, dentry_per_mb_count, prev_block, next_block);
    sb->s_first_dentry_mblock  = dentry_table_bbuf->b_blocknr;
    sb->s_free_dentry_count   += dentry_per_mb_count;

    LOG_DESC(DBG, "ROOT DENTRY TABLE INIT", "Alloc mbbuf count:%hu for root_dentry_table, block nr:%u, available dentry count:%u",
             dentry_table_bbuf->b_list_len, dentry_table_bbuf->b_blocknr, sb->s_free_dentry_count);

    return dentry_table_bbuf;
}

/*mount init functions*/
userfs_bbuf_t *userfs_mount_init(
    const uint32_t  metablock_size,
    const uint32_t  first_metablock_id,
    userfs_bbuf_t **_bg_desc_table_bbuf,
    userfs_bbuf_t **_dentry_table_bbuf)
{
    /*read super block from disk*/
    userfs_bbuf_t *sb_buf = userfs_get_used_metadata_block(metablock_size, first_metablock_id, 0);
    if (!sb_buf || sb_buf->b_type != USERFS_BTYPE_SUPER) {
        LOG_DESC(ERR, "USERFS MOUNT INIT", "Get super block from disk failed");
        userfs_free_mbbuf(sb_buf);
        return NULL;
    }
    userfs_super_block_t *sb = USERFS_MBLOCK(sb_buf->b_data)->sb;

    /*read block group descriptors blocks from disk*/
    userfs_bbuf_t  dummy;
    userfs_bbuf_t *bg_desc_block_list = &dummy;
    uint32_t       bbuf_list_len      = 1;
    uint32_t       next_mblock        = sb->s_first_bgroup_desc_mblock;
    while (next_mblock != sb->s_first_metablock) {
        userfs_bbuf_t *cur_mb_buf = userfs_get_used_metadata_block(metablock_size, first_metablock_id, next_mblock);
        if (!cur_mb_buf) {
            LOG_DESC(ERR, "USERFS MOUNT INIT", "Fail to get bgroup descriptors table mblock:%u", next_mblock);
            break;
        }
        cur_mb_buf->b_list_len = bbuf_list_len;
        bbuf_list_len++;
        bg_desc_block_list->b_this_page = cur_mb_buf;
        bg_desc_block_list              = cur_mb_buf;
        LOG_DESC(DBG, "USERFS MOUNT INIT", "Get bgroup descriptors table mblock:%u, type:%u, count:%u",
                 next_mblock, cur_mb_buf->b_type, cur_mb_buf->b_list_len);
        next_mblock = USERFS_MBLOCK(cur_mb_buf->b_data)->h.next;
    }
    *_bg_desc_table_bbuf                   = dummy.b_this_page;

    /*also need to read dentry table blocks*/
    userfs_bbuf_t *dentry_table_block_list = &dummy;
    bbuf_list_len                          = 1;
    next_mblock                            = sb->s_first_dentry_mblock;
    while (next_mblock != sb->s_first_metablock) {
        userfs_bbuf_t *cur_mb_buf = userfs_get_used_metadata_block(metablock_size, first_metablock_id, next_mblock);
        if (!cur_mb_buf) {
            LOG_DESC(ERR, "USERFS MOUNT INIT", "Fail to get dentry table mblock:%u", next_mblock);
            break;
        }
        cur_mb_buf->b_list_len = bbuf_list_len;
        bbuf_list_len++;
        dentry_table_block_list->b_this_page = cur_mb_buf;
        dentry_table_block_list              = cur_mb_buf;
        LOG_DESC(DBG, "USERFS MOUNT INIT", "Get get dentry table mblock:%u, type:%u, count:%u",
                 next_mblock, cur_mb_buf->b_type, cur_mb_buf->b_list_len);
        next_mblock = USERFS_MBLOCK(cur_mb_buf->b_data)->h.next;
    }
    *_dentry_table_bbuf = dummy.b_this_page;
    return sb_buf;
}

userfs_bgd_index_list_t *userfs_mount_bgdindex_list_init(
    userfs_bbuf_t               *bgroup_desc_bbuf_list,
    const userfs_super_block_t  *sb,
    userfs_bgd_index_list_ops_t *bgd_index_list_ops)
{
    if (bgroup_desc_bbuf_list->b_type != USERFS_BTYPE_BGROUP_DESC) {
        LOG_DESC(ERR, "USERFS BGROUP DESC INDEX LIST INIT", "Invalid bgroup desc type:%u", bgroup_desc_bbuf_list->b_type);
        return NULL;
    }

    userfs_bgd_index_list_t *bg_index_list = USERFS_MEM_ALLOC(sizeof(userfs_bgd_index_list_t));
    if (!bg_index_list) {
        LOG_DESC(ERR, "USERFS BGROUP DESC INDEX LIST INIT", "Fail to allocate bgroup desc index list");
        return NULL;
    }
    memset(bg_index_list, 0, sizeof(userfs_bgd_index_list_t));
    /**/
    uint32_t bgroup_idx_count       = sb->s_data_block_group_count;
    uint32_t bgroup_per_mb_count    = sb->s_bgroup_desc_per_mb_count;
    bg_index_list->bgi_list_ops     = bgd_index_list_ops;
    bg_index_list->bgi_blocknr2bbuf = USERFS_MEM_ALLOC(bgroup_idx_count / bgroup_per_mb_count);
    bg_index_list->bgi_maxroot_heap = userfs_mrheap_create(sizeof(userfs_bbuf_t *) * bgroup_idx_count);

    uint32_t       bgroup_id        = 0;
    uint32_t       bgroup_mb        = 0;
    uint32_t       bgroup_desc_size = sb->s_bgroup_desc_size;
    userfs_bbuf_t *cur_bg_desc_bbuf = bgroup_desc_bbuf_list;
    while (cur_bg_desc_bbuf != NULL) {
        bg_index_list->bgi_blocknr2bbuf[bgroup_mb++] = cur_bg_desc_bbuf;
        uint32_t cur_mb_bgd_count =
            bgroup_idx_count > sb->s_bgroup_desc_per_mb_count ? sb->s_bgroup_desc_per_mb_count : bgroup_idx_count;
        userfs_bgroup_desc_t *cur_bg_desc_table = USERFS_MBLOCK(cur_bg_desc_bbuf->b_data)->bg_desc_table;
        for (int i = 0; i < cur_mb_bgd_count; i++) {
            if (userfs_mrheap_insert(bg_index_list->bgi_maxroot_heap, bgroup_id, cur_bg_desc_table->bg_free_block_count, NULL) == NULL) {
                LOG_DESC(ERR, "USERFS BGROUP DESC INDEX LIST INIT", "Bgroup index heap insert failed, block id:%u, f_block_count:%u",
                         bgroup_id, cur_bg_desc_table->bg_free_block_count);
                goto clean_up;
            }
            cur_bg_desc_table = (userfs_bgroup_desc_t *)((char *)cur_bg_desc_table + bgroup_desc_size);
            bgroup_id++;
        }
        bgroup_idx_count -= cur_mb_bgd_count;
        cur_bg_desc_bbuf  = cur_bg_desc_bbuf->b_this_page;
    }
    bg_index_list->bgi_bgd_mb_count = bgroup_mb;
    return bg_index_list;

clean_up:
    userfs_mrheap_destroy(bg_index_list->bgi_maxroot_heap);
    USERFS_MEM_FREE(bg_index_list->bgi_blocknr2bbuf);
    USERFS_MEM_FREE(bg_index_list);
    return NULL;
}

linkhash_t *userfs_mount_dentry_hashtable_init(
    const uint32_t metablock_size,
    const uint32_t hash_bucket_count,
    userfs_bbuf_t *dentry_table_bbuf)
{
    /**/
    linkhash_t *dentry_hashtable = userfs_dentry_hash_create(hash_bucket_count);
    if (!dentry_hashtable) {
        LOG_DESC(ERR, "USERFS DENTRY HASHTABLE INIT", "Create dentry hashtable failed");
        return NULL;
    }
    userfs_dentry_table_t *dentry_table           = USERFS_MBLOCK(dentry_table_bbuf->b_data)->dentry_table;
    uint32_t               userd_rootdentry_count = dentry_table->h.dfd_used_dentry_count;
    LOG_DESC(DBG, "USERFS DENTRY HASHTABLE INIT", "Root dentry count:%u, first root dentry:%u, used root dentry:%u",
             dentry_table->h.dfd_dentry_count, dentry_table->h.dfd_first_dentry, userd_rootdentry_count);

    /*insert every dentry into dentry hash table*/
    for (uint32_t i = 0; i < userd_rootdentry_count; i++) {
        uint32_t         cur_dentry_pos = (i + dentry_table->h.dfd_first_dentry) % dentry_table->h.dfd_dentry_count;
        userfs_dentry_t *cur_dentry     = &(dentry_table->dentry[cur_dentry_pos]);
        if (!userfs_dentry_hash_insert(cur_dentry->d_name.name, strlen(cur_dentry->d_name.name),
                                       USERFS_NAME2INODE, cur_dentry->d_first_dblock, cur_dentry_pos, dentry_hashtable)) {
            LOG_DESC(ERR, "USERFS DENTRY HASHTABLE INIT", "Insert dentry:%u failed", i + dentry_table->h.dfd_first_dentry);
            userfs_dentry_hash_destroy(dentry_hashtable);
            return NULL;
        }
        LOG_DESC(DBG, "USERFS DENTRY HASHTABLE INIT", "Insert dentry:%u, dentry name:%s, first dblock:%u",
                 i, cur_dentry->d_name.name, cur_dentry->d_first_dblock);
    }
    return dentry_hashtable;
}