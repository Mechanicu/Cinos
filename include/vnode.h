#ifndef VNODE_H
#define VNODE_H
#include "list.h"
#include "userfs_heap.h"
#include <stdint.h>

struct block_buffer {
    struct block_buffer *b_this_page; /* circular list of page's buffers */
    uint16_t             b_list_len;
    uint32_t             b_type;    /*block type:metadata block or data block*/
    uint32_t             b_blocknr; /* start block number */
    uint32_t             b_block_s_off;
    uint32_t             b_size;
    uint8_t             *b_data; /* pointer to data within the page */
};

typedef struct block_buffer                  userfs_bbuf_t;
typedef struct block_group_descriptors_index userfs_bgd_index_t;

struct block_group_descriptors_index_list {
    uint32_t                                       bgi_bgd_mb_count;
    userfs_bbuf_t                                **bgi_blocknr2bbuf;
    userfs_mrheap_t                               *bgi_maxroot_heap;
    struct block_group_descriptors_index_list_ops *bgi_list_ops;
};

struct block_group_descriptors_index_list_ops {
    void (*bgi_list_init)(
        struct block_group_descriptors_index_list *bgi_list);
    void (*bgi_list_add)(
        struct block_group_descriptors_index_list *bgi_list,
        struct block_group_descriptors_index      *bgi);
    void (*bgi_list_del)(
        struct block_group_descriptors_index_list *bgi_list,
        struct block_group_descriptors_index      *bgi);
    struct block_group_descriptors_index *(*bgi_list_get)(
        struct block_group_descriptors_index_list *bgi_list,
        uint32_t                                   bgi_bgroup_nr);
};

#define USERFS_NAME2INODE    1
#define USERFS_NAME2INODEBUF 0

typedef struct block_group_descriptors_index_list     userfs_bgd_index_list_t;
typedef struct block_group_descriptors_index_list_ops userfs_bgd_index_list_ops_t;
typedef unsigned long                                 userfs_dhtable_inodeaddr_t;
#endif