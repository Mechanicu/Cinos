#include "log.h"
#include "node.h"
#include "unix_domain.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    // input
    int           domain     = AF_UNIX;
    int           type       = SOCK_STREAM;
    int           protocol   = 0;
    char          sockpath[] = "./unix_domain_test";
    int           back_log   = 16;
    us_connect_t *ud         = unix_socket_listen(domain, type, protocol, sockpath, back_log);
    node_ep_t     monitor_ep =
        {.ep_type = EP_SOCKET,
         .epfd    = ud->socket_fd,
         .node_id = 0};
    node_addr_t monitor_addr = {.node_path = "./unix_domain_test"};

    node_ep_list_t *eplist   = DSDB_MEM_ALLOC(
        sizeof(node_ep_list_t) + DSDB_DEFAULT_MAX_NODE_COUNT * sizeof(node_ep_t), NULL);
    node_addr_t *epaddr = DSDB_MEM_ALLOC(sizeof(node_addr_t) * DSDB_DEFAULT_MAX_NODE_COUNT, NULL);
    if (!eplist || !epaddr) {
        LOG_ERROR("MONITOR, alloc ep list or ep addr cache failed");
        exit(1);
    }
    eplist->ep_count     = 0;
    eplist->ep_addr_list = epaddr;

    insert_node_ep(eplist, &monitor_ep, &monitor_addr);
    LOG_DEBUG("MONITOR, start up, ep addr cache size:%lu, node id:%lu, ep list size:%lu, monitor fd:%d",
              DSDB_DEFAULT_MAX_NODE_COUNT, monitor_ep.node_id, DSDB_DEFAULT_MAX_NODE_COUNT, monitor_ep.epfd);
    server_node_register(eplist, &monitor_ep, NULL);
    return 0;
}