#include "disk_ops.h"
#include "inode.h"
#include "log.h"
#include "userfs_block_rw.h"
#include "vnode.h"
#include <malloc.h>
#include <stdint.h>

userfs_super_block_t *userfs_suber_block_alloc(
    const uint32_t  block_size,
    const uint32_t  metadata_block_size,
    const uint64_t  disk_size,
    uint32_t       *r_block_size,
    userfs_bbuf_t **sb_bbuf);

userfs_bbuf_t *userfs_get_metadata_block(
    userfs_super_block_t *sb,
    const uint8_t         mb_type,
    const uint8_t         is_new,
    const uint32_t        mb_id);

userfs_bbuf_t *userfs_bgroup_desc_table_init(
    userfs_super_block_t *sb,
    const uint32_t        blocks_per_group);

uint32_t userfs_get_used_metadata_blocklist(
    userfs_super_block_t *sb,
    const uint32_t       *mb_id,
    const uint16_t        mb_id_count,
    userfs_bbuf_t       **mb_buf_list);

userfs_bbuf_t *userfs_mount_init(
    const uint32_t  metablock_size,
    const uint32_t  first_metablock_id,
    userfs_bbuf_t **_bg_desc_table_bbuf,
    userfs_bbuf_t **_dentry_table_bbuf);

userfs_bgd_index_list_t *userfs_bgroupdesc_index_list_init(
    userfs_bbuf_t               *bgroup_desc_bbuf_list,
    const userfs_super_block_t  *sb,
    userfs_bgd_index_list_ops_t *bgd_index_list_ops);

#define UFS_BLOCK_SIZE       (1ul << 10 << 10 << 6)
#define UFS_METABLOCK_SIZE   (1ul << 10 << 2)
#define UFS_DISK_SIZE        (1ul << 10 << 10 << 10 << 10)
#define UFS_BLOCKS_PER_GROUP 64

int main(int argc, char **argv)
{
    userfs_disk_open("./user_disk");
    uint32_t       real_block_size;
    userfs_bbuf_t *sb_bbuf;

    /*init super block*/
    userfs_super_block_t *g_sb = userfs_suber_block_alloc(UFS_BLOCK_SIZE, UFS_METABLOCK_SIZE, UFS_DISK_SIZE, &real_block_size, &sb_bbuf);
    LOG_DESC(DBG, "Main", "dblock count:%u, dblock size:%uB, mblock size:%uB, mblock count:%u, f_mblock count:%u, first mblock:%u, mblock bitmap len:%u",
             g_sb->s_data_block_count,
             g_sb->s_data_block_size,
             g_sb->s_metablock_size,
             g_sb->s_metablock_count,
             g_sb->s_free_metablock_count,
             g_sb->s_first_metablock,
             g_sb->s_metadata_block_bitmap.bytes);

    /*init block group table*/
    userfs_bbuf_t *bgdesc_table_list = userfs_bgroup_desc_table_init(g_sb, UFS_BLOCKS_PER_GROUP);
    if (!bgdesc_table_list) {
        exit(1);
    }

    userfs_bbuf_t *check_bg_mb = bgdesc_table_list;
    uint32_t       md_id[128];
    for (int i = 0; i <= bgdesc_table_list->b_list_len; i++) {
        LOG_DESC(DBG, "Main", "mblock id:%u, mblock type:%hhu, prev:%u, next:%u, list len:%u",
                 check_bg_mb->b_blocknr,
                 USERFS_MBLOCK(check_bg_mb->b_data)->block_type,
                 USERFS_MBLOCK(check_bg_mb->b_data)->h.prev,
                 USERFS_MBLOCK(check_bg_mb->b_data)->h.next,
                 check_bg_mb->b_list_len);
        md_id[i]    = check_bg_mb->b_blocknr;
        check_bg_mb = check_bg_mb->b_this_page;
    }

    /**/
    if (userfs_mbbuf_list_flush(g_sb->s_first_metablock, g_sb->s_metablock_size, bgdesc_table_list, bgdesc_table_list->b_list_len + 1) < 0) {
        exit(0);
    }
    userfs_mbbuf_list_flush(g_sb->s_first_metablock, g_sb->s_metablock_size, sb_bbuf, 1);
#ifdef USERFS_MBLOCK_READ_CHECK
    userfs_bbuf_t *check_read;
    uint32_t       real_list_len = userfs_get_used_metadata_blocklist(g_sb, md_id, bgdesc_table_list->b_list_len + 1, &check_read);
    for (int i = 0; i < real_list_len; i++) {
        LOG_DESC(DBG, "check mblock for bgroup", "mblock id:%u, mblock type:%hhu, prev:%u, next:%u, list len:%u",
                 check_read->b_blocknr,
                 USERFS_MBLOCK(check_read->b_data)->block_type,
                 USERFS_MBLOCK(check_read->b_data)->h.prev,
                 USERFS_MBLOCK(check_read->b_data)->h.next,
                 check_read->b_list_len);
        check_read = check_read->b_this_page;
    }
#endif

    userfs_bbuf_t *mount_bg_desc_table = NULL;
    userfs_bbuf_t *mount_dentry_table  = NULL;
    userfs_bbuf_t *mount_sb_buf =
        userfs_mount_init(g_sb->s_metablock_size, g_sb->s_first_metablock, &mount_bg_desc_table, &mount_dentry_table);
    userfs_super_block_t *mount_sb = USERFS_MBLOCK(mount_sb_buf->b_data)->sb;
    LOG_DESC(DBG, "Main", "dblock count:%u, dblock size:%uB, mblock size:%uB, mblock count:%u, f_mblock count:%u, first mblock:%u, mblock bitmap len:%u",
             mount_sb->s_data_block_count,
             mount_sb->s_data_block_size,
             mount_sb->s_metablock_size,
             mount_sb->s_metablock_count,
             mount_sb->s_free_metablock_count,
             mount_sb->s_first_metablock,
             mount_sb->s_metadata_block_bitmap.bytes);

    userfs_bgd_index_list_ops_t mount_bgi_list_ops = {0};
    userfs_bgd_index_list_t    *mount_bgd_idx_list = userfs_bgroupdesc_index_list_init(mount_bg_desc_table, mount_sb, &mount_bgi_list_ops);
    userfs_mrheap_t            *mount_bgd_idx_heap = mount_bgd_idx_list->bgi_maxroot_heap;
    LOG_DESC(DBG, "Main", "Heap capacity:%u, heap size:%u",
             mount_bgd_idx_list->bgi_maxroot_heap->capacity, mount_bgd_idx_list->bgi_maxroot_heap->size);
    userfs_bbuf_t *tmp = mount_bg_desc_table;
    for (int i = 0; i < mount_bgd_idx_list->bgi_bgd_mb_count; i++, tmp = tmp->b_this_page) {
        LOG_DESC(DBG, "Main", "mblock:%u, expect bbuf:%p, real bbuf:%p",
                 mount_bgd_idx_list->bgi_bgd_mb_count, mount_bgd_idx_list->bgi_blocknr2bbuf[i], tmp);
    }

    user_disk_close();
    return 0;
}