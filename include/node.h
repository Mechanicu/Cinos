#ifndef NODE_H
#define NODE_H

#include "mempool.h"

#define DSDB_MAX_NODE_EP_PATH_LEN  (64)
#define DSDB_MEM_ALLOC(size, pool) malloc(size)
#define DSDB_MEM_FREE(ptr, size, pool) \
    while (ptr) {                      \
        free(ptr);                     \
        ptr = NULL;                    \
    }
#define DSDB_FILE_LOCK_WAIT(us)     usleep(us);
#define DSDB_DEFAULT_SLEEP_US       1000ul
#define DSDB_DEFAULT_MAX_NODE_COUNT 1024ul

enum {
    EP_FILE,
    EP_IP,
    EP_SOCKET,
} node_ep_type;

typedef struct node_addr {
    char node_path[DSDB_MAX_NODE_EP_PATH_LEN];
} node_addr_t;

typedef struct node_endpoint {
    void         *ep_connector;
    unsigned long ep_type;
    unsigned long node_id;
    int           epfd;
} node_ep_t;

typedef struct node_endpoint_list {
    volatile unsigned long ep_count;
    node_addr_t           *ep_addr_list;
    node_ep_t              eplist[0];
} node_ep_list_t;

node_ep_list_t *check_nodes_list(
    node_ep_t      *monitor,
    node_addr_t    *newnode,
    mempool_ctrl_t *mempool);

int server_node_register(
    node_ep_list_t *global_eplist,
    node_ep_t      *monitor,
    mempool_ctrl_t *mempool);

int insert_node_ep(
    node_ep_list_t *eplist,
    node_ep_t      *ep,
    node_addr_t    *epaddr);
#endif