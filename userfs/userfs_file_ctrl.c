#include "userfs_file_ctrl.h"
#include "log.h"
#include "pthread_spinlock.h"
#include "userfs_heap.h"
#include "vnode.h"
#include <string.h>

// #define USERFS_DEFAULT_DATA_BLOCK_SHARD_SIZE   (1 << 10 << 10 << 2)
#define USERFS_DEFAULT_DATA_BLOCK_SHARD_SIZE   1
#define USERFS_DEFAULT_MAX_DBBUF_FREE_LIST_LEN 256

static pthread_spinlock_t dbbuf_lock          = 1;
static userfs_bbuf_t     *free_dbbuf_list     = NULL;
static atomic_t           free_dbbuf_list_len = {0};

userfs_bbuf_t *userfs_alloc_dbbuf(
    uint32_t dblock_shardsize)
{
    userfs_bbuf_t *db_buf;
    /*to avoid frequently memory alloc, check free dbbuf list first*/
    if (free_dbbuf_list != NULL) {
        pthread_spin_lock(&dbbuf_lock);
        db_buf          = free_dbbuf_list;
        free_dbbuf_list = free_dbbuf_list->b_this_page;
        pthread_spin_unlock(&dbbuf_lock);
        atomic_sub(&free_dbbuf_list_len, 1);
        LOG_DESC(DBG, "ALLOC MBLOCK BUF", "Alloc dbbuf:%p, next free one:%p",
                 db_buf, free_dbbuf_list);
        memset(db_buf, 0, sizeof(userfs_bbuf_t));
        db_buf->b_size = dblock_shardsize;
        return db_buf;
    }
    /*if no avaliable dbbuf, then alloc one*/
    db_buf = USERFS_MEM_ALLOC(sizeof(userfs_bbuf_t));
    if (!db_buf) {
        LOG_DESC(ERR, "ALLOC MBLOCK BUF", "Alloc metadata block failed");
        return NULL;
    }
    memset(db_buf, 0, sizeof(userfs_bbuf_t));
    db_buf->b_data = USERFS_MEM_ALLOC(dblock_shardsize);
    if (!(db_buf->b_data)) {
        LOG_DESC(ERR, "ALLOC MBLOCK BUF", "Alloc metadata block data page failed");
        USERFS_MEM_FREE(db_buf);
        return NULL;
    }
    db_buf->b_size = dblock_shardsize;
    return db_buf;
}

void userfs_free_dbbuf(
    userfs_bbuf_t *db_buf)
{
    /**/
    int free_dbbuf_count = atomic_get(&free_dbbuf_list_len);
    if (free_dbbuf_count > USERFS_DEFAULT_MAX_DBBUF_FREE_LIST_LEN) {
        LOG_DESC(DBG, "FREE MBLOCK BUF", "Free mblock list already full, len:%u", free_dbbuf_count);
        USERFS_MEM_FREE(db_buf->b_data);
        USERFS_MEM_FREE(db_buf);
        return;
    }
    /*try to insert in free mbuf list*/
    atomic_add(&free_dbbuf_list_len, 1);
    pthread_spin_lock(&dbbuf_lock);
    db_buf->b_this_page = free_dbbuf_list;
    free_dbbuf_list     = db_buf->b_this_page;
    pthread_spin_unlock(&dbbuf_lock);
    LOG_DESC(DBG, "FREE dbbuf", "Current free dbbuf list len:%d", free_dbbuf_count + 1);
    return;
}

userfs_bgroup_desc_t *userfs_bgdidx2bgd(
    userfs_bgd_index_list_t *bgd_idx_list,
    const uint32_t           bgd_per_mb_count,
    const uint32_t           bgd_nr,
    const uint32_t           bgd_size)
{
    uint32_t       bgd_mb_nr  = bgd_nr / bgd_per_mb_count;
    uint32_t       bgd_off    = bgd_nr % bgd_per_mb_count;
    userfs_bbuf_t *bgd_mb     = bgd_idx_list->bgi_blocknr2bbuf[bgd_mb_nr];

    userfs_bgroup_desc_t *bgd = USERFS_MBLOCK(bgd_mb->b_data)->bg_desc_table;
    bgd                       = (userfs_bgroup_desc_t *)((char *)bgd + bgd_size * bgd_off);
    LOG_DESC(DBG, "BGD INDEX TO BGD", "bgd mb nr:%u, bgd off:%u, block count:%u, free block count:%u",
             bgd_mb_nr, bgd_off, bgd->bg_block_count, bgd->bg_free_block_count);
    return bgd;
}

userfs_bbuf_t *userfs_get_new_dblock(
    userfs_super_block_t    *sb,
    userfs_bgd_index_list_t *bgd_idx_list)
{
    /*alloc block buf from new dblock*/
    userfs_bbuf_t *db_buf = userfs_alloc_dbbuf(USERFS_DEFAULT_DATA_BLOCK_SHARD_SIZE);
    if (!db_buf) {
        LOG_DESC(ERR, "USERFS DBLOCK ALLOC", "Alloc dblock buf failed");
        return NULL;
    };
    /*get lightest load bgroup descriptors index from heap*/
    userfs_bgd_index_t *cur_lightest_load_bgd_idx = userfs_mrheap_get_heaptop(bgd_idx_list->bgi_maxroot_heap);
    if (!cur_lightest_load_bgd_idx || cur_lightest_load_bgd_idx->bgi_free_blocks_count == 0) {
        LOG_DESC(ERR, "USERFS DBLOCK ALLOC", "Cannot extract heap top bgd_index or no free bgroup:%u",
                 cur_lightest_load_bgd_idx->bgi_bgroup_nr);
        userfs_free_dbbuf(db_buf);
        return NULL;
    }

    LOG_DESC(DBG, "USERFS DBLOCK ALLOC", "Get lightest load bgroup, bgroup id:%u, free blocks:%u",
             cur_lightest_load_bgd_idx->bgi_bgroup_nr, cur_lightest_load_bgd_idx->bgi_free_blocks_count);
    userfs_bgroup_desc_t *cur_lightest_load_bgd = userfs_bgdidx2bgd(
        bgd_idx_list, sb->s_bgroup_desc_per_mb_count, cur_lightest_load_bgd_idx->bgi_bgroup_nr, sb->s_bgroup_desc_size);

    /*alloc dblock from bgroup descriptors*/
    uint32_t bg_dblock_nr = bitmap_get_first_free(&cur_lightest_load_bgd->block_bm);
    if (bg_dblock_nr == (uint32_t)(-1)) {
        userfs_free_dbbuf(db_buf);
        LOG_DESC(DBG, "USERFS DBLOCK ALLOC", "No free block in current bgroup");
        return NULL;
    }
    cur_lightest_load_bgd_idx = userfs_mrheap_extract_heaptop(bgd_idx_list->bgi_maxroot_heap);

    /*alloc successfully*/
    sb->s_free_data_block_count--;
    cur_lightest_load_bgd->bg_free_block_count--;
    userfs_mrheap_insert(bgd_idx_list->bgi_maxroot_heap,
                         cur_lightest_load_bgd_idx->bgi_bgroup_nr,
                         cur_lightest_load_bgd_idx->bgi_free_blocks_count - 1,
                         cur_lightest_load_bgd_idx);

    uint32_t dblock_nr    = cur_lightest_load_bgd_idx->bgi_bgroup_nr * sb->s_data_block_per_group;
    db_buf->b_block_s_off = 0;
    db_buf->b_blocknr     = dblock_nr + bg_dblock_nr;
    db_buf->b_type        = USERFS_BTYPE_DATA;
    LOG_DESC(DBG, "USERFS DBLOCK ALLOC", "bgroup nr:%u, block in bgroup nr:%u, block nr:%u, db buf size:0x%xB",
             dblock_nr, bg_dblock_nr, dblock_nr + bg_dblock_nr, db_buf->b_size);

    return db_buf;
}

userfs_bbuf_t *userfs_free_used_dblock()
{
}