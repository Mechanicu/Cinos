#include "log.h"
#include "node.h"
#include "unix_domain.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int           domain     = AF_UNIX;
    int           type       = SOCK_STREAM;
    int           protocol   = 0;
    char          sockpath[] = "./unix_domain_test";
    int           back_log   = 16;
    us_connect_t *ud         = unix_socket_connect(domain, type, protocol, sockpath, back_log);

    node_ep_t monitor        = {
               .ep_type = EP_SOCKET,
               .epfd    = ud->socket_fd};
    node_addr_t     curnode = {.node_path = "192.168.0.2"};
    node_ep_list_t *eplist  = check_nodes_list(&monitor, &curnode, NULL);
    for (int i = 0; i < eplist->ep_count; i++) {
        unsigned long epid = eplist->eplist[i].node_id;
        LOG_DEBUG("CHECK NODE LIST, node id:%lu, node path:%s",
                  epid, eplist->ep_addr_list[epid].node_path);
    }
    return 0;
}