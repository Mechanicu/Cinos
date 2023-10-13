#ifndef INODE_H
#define INODE_H
#include "bitmap.h"
#include <stdint.h>
#include <sys/types.h>

#define USERFS_BGROUP_DESC(ptr)  ((userfs_bgroup_desc_t *)(ptr))
#define USERFS_DENTRY(ptr)       ((userfs_dentry_t *)(ptr))
#define USERFS_MBLOCK(ptr)       ((userfs_mblock_t *)(ptr))

#define USERFS_MAX_FILE_NAME_LEN 256

enum {
    USERFS_FREE,
    USERFS_SUPER,
    USERFS_DENTRY,
    USERFS_BGROUP_DESC,
    USERFS_DATA
} block_type;

struct userfs_super_block {
    /*timestamp*/
    uint32_t s_wtime;
    /*data block alloc by block group*/
    uint32_t s_first_datablock_off;
    uint32_t s_data_block_size;
    uint32_t s_data_block_count;
    uint32_t s_free_data_block_count;
    /*data block group*/
    uint32_t s_data_block_per_group;
    uint32_t s_data_block_group_count;
    uint32_t s_free_data_block_group_count;
    /*metablock alloc by super block*/
    uint32_t s_first_metablock_off;
    uint32_t s_metablock_count;
    uint32_t s_free_metablock_count;
    uint32_t s_metablock_size;
    bitmap_t s_metadata_block_bitmap;
};

struct userfs_inode {
    /**/
    uint32_t i_size;
    uint32_t i_blocks;
    /**/
    uint32_t i_first_block;
    uint32_t i_last_block;
    uint32_t i_lastest_bgroup;
    /*timestamp*/
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
};

struct userfs_block_header {
    uint32_t prev;
    uint32_t next;
};

struct userfs_data_block {
    struct userfs_block_header h;
    uint8_t                    data[0];
};

struct userfs_block_group_descriptor {
    uint16_t bg_block_count;
    uint16_t bg_free_block_count;
    bitmap_t block_bm;
};

struct userfs_dentry_name {
    uint8_t namelen;
    int8_t  name[3];
};

struct userfs_dentry {
    uint32_t                  inode_num;
    struct userfs_dentry_name name;
};

struct userfs_metadata_block {
    struct userfs_block_header h;
    uint8_t                    block_type;
    union {
        struct userfs_block_group_descriptor bg_desc[0];
        struct userfs_dentry                 dentry_table[0];
        struct userfs_super_block            sb[0];
    };
};

typedef struct userfs_super_block            userfs_super_block_t;
typedef struct userfs_inode                  userfs_inode_t;
typedef struct userfs_dentry                 userfs_dentry_t;
typedef struct userfs_block_group_descriptor userfs_bgroup_desc_t;
typedef struct userfs_metadata_block         userfs_mblock_t;
#endif