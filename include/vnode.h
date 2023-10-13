#ifndef VNODE_H
#define VNODE_H
#include "list.h"
#include <stdint.h>

struct block_buffer {
    struct block_buffer *b_this_page; /* circular list of page's buffers */
    uint16_t             b_list_len;
    uint8_t              b_type;    /*block type:metadata block or data block*/
    uint32_t             b_blocknr; /* start block number */
    uint32_t             b_block_s_off;
    uint32_t             b_size;
    uint8_t             *b_data; /* pointer to data within the page */
};

typedef struct block_buffer b_buf_t;
#endif