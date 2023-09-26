#include "log.h"
#include "mempool.h"
#include "node.h"
#include "unix_domain.h"
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static node_ep_list_t *check_node_list_bysocket(
    const node_ep_t *monitor,
    node_addr_t     *newnode,
    mempool_ctrl_t  *mempool)
{
    // register new node to monitor
    node_ep_list_t *eplist = NULL;
    if (xwrite(monitor->epfd, newnode, sizeof(node_addr_t)) == -1) {
        perror("CHECK NODE LIST, failed to notify monitor");
        return eplist;
    }

    // read ep count first, then read ep list
    unsigned long ep_count;
    if (xread(monitor->epfd, &ep_count, sizeof(unsigned long)) == -1) {
        perror("CHECK NODE LIST, fail to get ep count from monitor");
        return eplist;
    }
    ep_count++;
    // check ep count
    if (ep_count > DSDB_DEFAULT_MAX_NODE_COUNT) {
        LOG_ERROR("CHECK NODE LIST, too much nodes:%lu", ep_count);
        return eplist;
    }
    LOG_DEBUG("CHECK NODE LIST, node count:%lu", ep_count);

    unsigned long eplist_size       = sizeof(node_ep_list_t) + DSDB_DEFAULT_MAX_NODE_COUNT * sizeof(node_ep_t);
    unsigned long epaddr_cache_size = sizeof(node_addr_t) * DSDB_DEFAULT_MAX_NODE_COUNT;
    unsigned long current_ep_id     = ep_count - 1;
    eplist                          = DSDB_MEM_ALLOC(eplist_size, mempool);
    node_addr_t *epaddr_cache       = DSDB_MEM_ALLOC(epaddr_cache_size, mempool);
    if (!epaddr_cache || !eplist) {
        LOG_ERROR("CHECK NODE LIST, alloc epaddr cache or ep list failed");
        goto recycle;
    }
    LOG_DEBUG("CHECK NODE LIST, eplist size:%lu, addr:%p, epaddr cache size:%lu, addr:%p",
              eplist_size, eplist, epaddr_cache_size, epaddr_cache);
    // read ep list by ep count
    if (xread(monitor->epfd, epaddr_cache, ep_count * sizeof(node_addr_t)) == -1) {
        perror("CHECK NODE LIST, read ep list failed");
        goto recycle;
    }

    //
    for (int i = 0; i < ep_count; i++) {
        eplist->eplist[i].node_id = i;
    }
    eplist->ep_count     = ep_count;
    eplist->ep_addr_list = epaddr_cache;
    return eplist;
recycle:
    DSDB_MEM_FREE(eplist, eplist_size, mempool);
    DSDB_MEM_FREE(epaddr_cache, epaddr_cache_size, mempool);
    return eplist;
}

node_ep_list_t *check_nodes_list(
    node_ep_t      *monitor,
    node_addr_t    *newnode,
    mempool_ctrl_t *mempool)
{
    if (monitor == NULL || newnode == NULL) {
        LOG_ERROR("CHECK NODE LIST, invalid args");
        return NULL;
    }
    // read list from monitor server
    switch (monitor->ep_type) {
        case EP_FILE:
        case EP_IP:
        case EP_SOCKET:
            return check_node_list_bysocket(monitor, newnode, mempool);
        default:
            LOG_ERROR("CHECK NODE LIST, unknown ep_type");
            return NULL;
            break;
    }
}

static int server_node_register_bysocket(
    node_ep_list_t *global_eplist,
    node_ep_t      *monitor,
    mempool_ctrl_t *mempool)
{
    // only one thread handle node init request now
    int           connect_fd;
    unsigned long expect_ep_id;
    while (1) {
        // accept node init request
        connect_fd = accept(monitor->epfd, NULL, NULL);
        if (connect_fd == -1) {
            perror("SERVER REG NODE, accept error");
            break;
        }
        LOG_DEBUG("SERVER REG NODE, accept request on fd:%d", connect_fd);
        // handle node init request
        expect_ep_id = global_eplist->ep_count;
        if (global_eplist->ep_count > DSDB_DEFAULT_MAX_NODE_COUNT) {
            LOG_ERROR("SERVER REG NODE, reach max node count, current node count:%lu, max node count:%lu",
                      global_eplist->ep_count, (unsigned long)DSDB_DEFAULT_MAX_NODE_COUNT);
            close(connect_fd);
            continue;
        }
        LOG_DEBUG("SERVER REG NODE, current node count:%lu, next node id:%lu",
                  global_eplist->ep_count + 1, expect_ep_id);
        node_addr_t *expect_slot = &(global_eplist->ep_addr_list[expect_ep_id]);
        // recv new node addr
        if ((xread(connect_fd, &(expect_slot->node_path), sizeof(node_addr_t)) == -1)) {
            perror("SERVER REG NODE, read new node addr failed");
            close(connect_fd);
            continue;
        }
        global_eplist->eplist[expect_ep_id].node_id = expect_ep_id;
        LOG_DEBUG("SERVER REG NODE, new node id:%lu, new node addr:%s",
                  global_eplist->eplist[expect_ep_id].node_id, expect_slot->node_path);
        // write new ep count
        if ((xwrite(connect_fd, &expect_ep_id, sizeof(unsigned long))) == -1) {
            perror("SERVER REG NODE, send new node count failed");
            close(connect_fd);
            continue;
        }
        // write ep list
        if ((xwrite(connect_fd, global_eplist->ep_addr_list, sizeof(node_addr_t) * (global_eplist->ep_count + 1))) == -1) {
            perror("SERVER REG NODE, send ep list failed");
            close(connect_fd);
            continue;
        }
        // update global ep list
        global_eplist->ep_count++;
        close(connect_fd);
    }
clean:
    close(connect_fd);
    return -1;
}

int server_node_register(
    node_ep_list_t *global_eplist,
    node_ep_t      *monitor,
    mempool_ctrl_t *mempool)
{
    if (!monitor) {
        LOG_ERROR("SERVER REG NODE, invalid args");
        return -1;
    }
    // read list from monitor server
    switch (monitor->ep_type) {
        case EP_FILE:
        case EP_IP:
        case EP_SOCKET:
            return server_node_register_bysocket(global_eplist, monitor, mempool);
        default:
            LOG_ERROR("unknown ep_type");
            return -1;
            break;
    }
}

int insert_node_ep(
    node_ep_list_t *eplist,
    node_ep_t      *ep,
    node_addr_t    *epaddr)
{
    if (eplist == NULL || epaddr == NULL) {
        LOG_ERROR("INSERT NODE ADDR, invalid args");
        return -1;
    }
    unsigned long new_ep_id = eplist->ep_count;
    if (new_ep_id > DSDB_DEFAULT_MAX_NODE_COUNT) {
        LOG_ERROR("INSERT NODE ADDR, reach max node count");
        return -1;
    }
    memset(&eplist->eplist[new_ep_id], 0, sizeof(node_ep_t));
    strncpy((char *)&eplist->eplist[new_ep_id], (char *)ep, sizeof(node_ep_t));
    strncpy((char *)&eplist->ep_addr_list[new_ep_id], (char *)epaddr, sizeof(node_addr_t));
    eplist->eplist[new_ep_id].node_id = new_ep_id;
    ep->node_id                       = new_ep_id;
    LOG_DEBUG("INSERT NODE ADDR, ep id:%lu, ep fd:%x, ep addr:%s",
              eplist->eplist[new_ep_id].node_id, eplist->eplist[new_ep_id].epfd, eplist->ep_addr_list[new_ep_id].node_path);
    eplist->ep_count++;
    return 0;
}