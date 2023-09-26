#ifndef MEMPOOL_H
#define MEMPOOL_H
#include "list.h"

typedef struct mempool_ops {
    void *(*palloc)(void *ctrl, const unsigned long size);
    void  (*pfree)(void *ctrl, void *ptr, const unsigned long size);
} mempool_ops_t;

typedef struct mempool_ctrl {
    /**/
    mempool_ops_t  ops;
    unsigned long  magic;
    /**/
    char          *svaddr;
    unsigned long  totalsize;
    char          *rvaddr;
    unsigned long  remainsize;
    /**/
    list_t         hook;
    unsigned long  slab_count;
    unsigned long *slabs[0];
} mempool_ctrl_t;

mempool_ctrl_t *mempool_ctrl_init(
    void         *vaddr,
    unsigned long size,
    unsigned long slabs_count,
    void         *(*palloc)(void *ctrl, const unsigned long size),
    void          (*pfree)(void *ctrl, void *ptr, const unsigned long size));

void mempool_ctrl_destroy(
    mempool_ctrl_t **ctrl);

#endif