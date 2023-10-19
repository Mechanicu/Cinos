#include "userfs_file_ctrl.h"
#include "log.h"
#include "pthread_spinlock.h"
#include "userfs_block_rw.h"
#include "userfs_dentry_hash.h"
#include "userfs_heap.h"
#include "vnode.h"
#include <string.h>
#include <time.h>

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
    userfs_bgd_index_list_t *bgd_idx_list,
    uint32_t                 dblock_shard_size)
{
    /*alloc block buf from new dblock*/
    userfs_bbuf_t *db_buf = userfs_alloc_dbbuf(dblock_shard_size);
    if (!db_buf) {
        LOG_DESC(ERR, "USERFS GET NEW DBLOCK", "Alloc dblock buf failed");
        return NULL;
    };
    /*get lightest load bgroup descriptors index from heap*/
    userfs_bgd_index_t *cur_lightest_load_bgd_idx = userfs_mrheap_get_heaptop(bgd_idx_list->bgi_maxroot_heap);
    if (!cur_lightest_load_bgd_idx || cur_lightest_load_bgd_idx->bgi_free_blocks_count == 0) {
        LOG_DESC(ERR, "USERFS GET NEW DBLOCK", "Cannot extract heap top bgd_index or no free bgroup:%u",
                 cur_lightest_load_bgd_idx->bgi_bgroup_nr);
        userfs_free_dbbuf(db_buf);
        return NULL;
    }

    LOG_DESC(DBG, "USERFS GET NEW DBLOCK", "Get lightest load bgroup, bgroup id:%u, free blocks:%u",
             cur_lightest_load_bgd_idx->bgi_bgroup_nr, cur_lightest_load_bgd_idx->bgi_free_blocks_count);
    userfs_bgroup_desc_t *cur_lightest_load_bgd = userfs_bgdidx2bgd(
        bgd_idx_list, sb->s_bgroup_desc_per_mb_count, cur_lightest_load_bgd_idx->bgi_bgroup_nr, sb->s_bgroup_desc_size);

    /*alloc dblock from bgroup descriptors*/
    uint32_t bg_dblock_nr = bitmap_get_first_free(&cur_lightest_load_bgd->block_bm);
    if (bg_dblock_nr == (uint32_t)(-1)) {
        userfs_free_dbbuf(db_buf);
        LOG_DESC(DBG, "USERFS GET NEW DBLOCK", "No free block in current bgroup");
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
    LOG_DESC(DBG, "USERFS GET NEW DBLOCK", "bgroup nr:%u, block in bgroup nr:%u, block nr:%u, db buf size:0x%xB, total free dblock:%u",
             dblock_nr, bg_dblock_nr, dblock_nr + bg_dblock_nr, db_buf->b_size, sb->s_free_data_block_count);

    return db_buf;
}

userfs_bbuf_t *userfs_get_used_dblock(
    userfs_super_block_t *sb,
    uint32_t              target_dblock_id,
    uint32_t              dblock_off,
    uint32_t              dblock_shard_size)
{
    /*alloc block buf from new dblock*/
    userfs_bbuf_t *db_buf = userfs_alloc_dbbuf(dblock_shard_size);
    if (!db_buf) {
        LOG_DESC(ERR, "USERFS GET USED DBLOCK", "Alloc dblock buf failed");
        return NULL;
    };

    db_buf->b_blocknr     = target_dblock_id;
    db_buf->b_block_s_off = dblock_off;
    db_buf->b_size        = dblock_shard_size;
    /*read into data block buf from target data block
    and start from @dblock_off to @dblock_shard_size*/
    if (userfs_read_data_block(db_buf, sb->s_first_datablock, sb->s_data_block_size) != 0) {
        LOG_DESC(ERR, "USERFS GET USED DBLOCK", "Read dblock failed");
        return NULL;
    }
}

userfs_bbuf_t *userfs_free_used_dblock()
{
}

static inline void userfs_dentry_exchange(
    userfs_dentry_t *first,
    userfs_dentry_t *second)
{
    userfs_dentry_t tmp;
    memcpy(&tmp, first, sizeof(userfs_dentry_t));
    memcpy(first, second, sizeof(userfs_dentry_t));
    memcpy(second, &tmp, sizeof(userfs_dentry_t));
}

uint32_t userfs_alloc_dentry(
    userfs_dentry_table_t *dentry_table)
{
    userfs_dentry_table_header_t *dt_h = &(dentry_table->h);
    if (dt_h->dfd_used_dentry_count == dt_h->dfd_dentry_count) {
        LOG_DESC(ERR, "USERFS DENTRY ALLOC", "dentry table is full");
        return UINT32_MAX;
    }
    dt_h->dfd_used_dentry_count++;
    uint32_t new_dentry_pos     = dt_h->dfd_first_free_dentry;
    dt_h->dfd_first_free_dentry = (dt_h->dfd_first_free_dentry + 1) % dt_h->dfd_dentry_count;
    return new_dentry_pos;
}

int userfs_free_dentry(
    userfs_dentry_table_t *dentry_table,
    uint32_t               dentry_pos)
{
    userfs_dentry_table_header_t *dt_h = &(dentry_table->h);
    if (dentry_pos < dt_h->dfd_first_dentry && dentry_pos >= dt_h->dfd_first_free_dentry) {
        LOG_DESC(ERR, "USERFS DENTRY FREE", "Dentry pos:%u isn't allocated, first used:%u, first free:%u",
                 dentry_pos, dt_h->dfd_first_dentry, dt_h->dfd_first_free_dentry);
        return -1;
    }
    if (dentry_pos != dt_h->dfd_first_dentry) {
        userfs_dentry_exchange(&(dentry_table->dentry[dentry_pos]), &(dentry_table->dentry[dt_h->dfd_first_dentry]));
    }
    dt_h->dfd_first_dentry = (dt_h->dfd_first_dentry + 1) % dt_h->dfd_dentry_count;
    dt_h->dfd_used_dentry_count--;
    return 0;
}

userfs_bbuf_t *userfs_alloc_inode(
    userfs_super_block_t    *sb,
    userfs_bgd_index_list_t *bgd_idx_list,
    uint32_t                 dblock_shard_size)
{
    /*init inode*/
    struct timespec inode_tp = {0};
    if (clock_gettime(CLOCK_REALTIME, &inode_tp) < 0) {
        LOG_DESC(ERR, "USERFS INODE ALLOC", "Failed to get current time");
        return NULL;
    }
    /*get new data block as first data block for new file, which contain inode*/
    userfs_bbuf_t  *inode_bbuf = userfs_get_new_dblock(sb, bgd_idx_list, dblock_shard_size);
    userfs_inode_t *inode      = (userfs_inode_t *)(inode_bbuf->b_data);
    if (inode_bbuf == NULL) {
        LOG_DESC(ERR, "USERFS INODE ALLOC", "Failed to allocate inode block");
        return NULL;
    }

    inode->i_ctime  = inode_tp.tv_sec;
    inode->i_atime  = inode_tp.tv_sec;
    inode->i_mtime  = inode_tp.tv_sec;
    inode->i_dtime  = 0;
    inode->i_size   = sb->s_data_block_size;
    inode->i_blocks = 1;

    return inode_bbuf;
}

uint32_t userfs_name2inode(

)
{
}