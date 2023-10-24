#ifndef INODE_H
#define INODE_H
#include "bitmap.h"
#include <stdint.h>
#include <sys/types.h>

#define USERFS_BGROUP_DESC(ptr)  ((userfs_bgroup_desc_t *)(ptr))
#define USERFS_DENTRY(ptr)       ((userfs_dentry_t *)(ptr))
#define USERFS_MBLOCK(ptr)       ((userfs_mblock_t *)(ptr))
#define USERFS_DBLOCK(ptr)       ((userfs_dblock_t *)(ptr))

#define USERFS_INODE_SIZE        128
#define USERFS_DENTRY_SIZE       32
#define USERFS_MAX_FILE_NAME_LEN (USERFS_DENTRY_SIZE - sizeof(uint32_t) - 1)

#define USERFS_BTYPE_FREE        (0x12345678u)
#define USERFS_BTYPE_SUPER       (83u)
#define USERFS_BTYPE_DENTRY      (69u)
#define USERFS_BTYPE_BGROUP_DESC (71u)
#define USERFS_BTYPE_DATA        (68u)

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

struct userfs_super_block {
    /*timestamp*/
    uint32_t s_wtime;
    /*data block alloc by block group*/
    uint32_t s_first_datablock;
    uint32_t s_data_block_size;
    uint32_t s_data_block_count;
    uint32_t s_free_data_block_count;
    /*data block group*/
    uint32_t s_data_block_per_group;
    uint32_t s_data_block_group_count;
    uint32_t s_free_data_block_group_count;
    /*metablock alloc by super block*/
    /**/
    uint32_t s_first_dentry_mblock;
    uint32_t s_free_dentry_count;
    /**/
    uint32_t s_first_bgroup_desc_mblock;
    uint32_t s_bgroup_desc_size;
    uint32_t s_bgroup_desc_per_mb_count;
    /**/
    uint32_t s_first_metablock;
    uint32_t s_metablock_count;
    uint32_t s_free_metablock_count;
    uint32_t s_metablock_size;
    bitmap_t s_metadata_block_bitmap;
};

struct userfs_inode {
    /*timestamp*/
    time_t   i_atime;
    time_t   i_ctime;
    time_t   i_mtime;
    time_t   i_dtime;
    /**/
    uint32_t i_size;
    uint32_t i_blocks;
    /**/
    uint32_t i_lastest_bgroup;
    atomic_t ref_count;
    uint32_t i_v2pnode_table[0];
};

struct userfs_block_header {
    uint32_t prev;
    uint32_t next;
};

struct userfs_data_block {
    union {
        struct userfs_inode inode[0];
        uint8_t             fnblock[USERFS_INODE_SIZE];
    };
    uint8_t fhblock[0];
};

struct userfs_block_group_descriptor {
    uint16_t bg_block_count;
    uint16_t bg_free_block_count;
    bitmap_t block_bm;
};

struct userfs_dentry_name {
    char name[USERFS_MAX_FILE_NAME_LEN];
    char zero[1];
};
struct userfs_dentry {
    uint32_t                  d_first_dblock;
    struct userfs_dentry_name d_name;
};

struct userfs_dentry_table_header {
    uint32_t dfd_dentry_count;
    uint32_t dfd_used_dentry_count;
    uint32_t dfd_first_dentry;
    uint32_t dfd_first_free_dentry;
};

struct userfs_dentry_table {
    union {
        struct userfs_dentry_table_header h;
        char                              align[sizeof(struct userfs_dentry)];
    };
    struct userfs_dentry dentry[0];
};

struct userfs_metadata_block {
    struct userfs_block_header h;
    uint32_t                   block_type;
    union {
        struct userfs_block_group_descriptor bg_desc_table[0];
        struct userfs_dentry_table           dentry_table[0];
        struct userfs_super_block            sb[0];
    };
};

typedef struct userfs_super_block            userfs_super_block_t;
typedef struct userfs_inode                  userfs_inode_t;
typedef struct userfs_dentry                 userfs_dentry_t;
typedef struct userfs_dentry_table_header    userfs_dentry_table_header_t;
typedef struct userfs_dentry_table           userfs_dentry_table_t;
typedef struct userfs_block_group_descriptor userfs_bgroup_desc_t;
typedef struct userfs_metadata_block         userfs_mblock_t;
typedef struct userfs_data_block             userfs_dblock_t;
#endif