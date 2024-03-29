#include "unix_domain.h"
#include "log.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>

#define UNIX_SOCKET_ALLOC(size) (malloc(size))
#define UNIX_SOCKET_FREE(ptr) \
    while ((ptr)) {           \
        free(ptr);            \
        (ptr) = NULL;         \
    }

static int unix_socket_init(
    us_connect_t *ud_socket,
    const int     domain,
    const int     type,
    const int     protocol,
    const char   *sockpath,
    const short   port)
{
    // create socket fd
    // set socket type
    ud_socket->type.domain   = domain;
    ud_socket->type.type     = type;
    ud_socket->type.protocol = protocol;

    // bind address
    switch (domain) {
        case AF_UNIX:
            ud_socket->addr.addr_un.sun_family = domain;
            if (strlen(sockpath) > sizeof(ud_socket->addr.addr_un.sun_path)) {
                return -1;
            }
            strncpy(ud_socket->addr.addr_un.sun_path, sockpath, sizeof(ud_socket->addr.addr_un.sun_path) - 1);
            break;
        case AF_INET:
            ud_socket->addr.addr_in.sin_family = domain;
            if (inet_pton(domain, sockpath, &(ud_socket->addr.addr_in.sin_addr)) < 1) {
                perror("converse ipv4 addr");
                return -1;
            }
            ud_socket->addr.addr_in.sin_port = htons(port);
            break;
        default:
            LOG(ERR, "unknown domain type");
            break;
    }

    ud_socket->socket_fd = socket(domain, type, protocol);
    if (ud_socket->socket_fd == -1) {
        perror("open socket");
        return -1;
    }
    return 0;
}

us_connect_t *unix_socket_listen(
    const int   domain,
    const int   type,
    const int   protocol,
    const char *sockpath,
    const short port,
    const int   back_log)
{
    us_connect_t *ud_socket = UNIX_SOCKET_ALLOC(sizeof(us_connect_t));
    if (ud_socket == NULL) {
        return NULL;
    }

    int res = unix_socket_init(ud_socket, domain, type, protocol, sockpath, port);
    if (res == -1) {
        UNIX_SOCKET_FREE(ud_socket);
        perror("unix socket init");
        return NULL;
    }

    // bind socket fd with address
    switch (domain) {
        case AF_UNIX:
            // remove previous exist file, it's ok if not exist
            if (remove(sockpath) == -1 && errno != ENOENT) {
                return NULL;
            }
            res = bind(ud_socket->socket_fd, (struct sockaddr *)&(ud_socket->addr.addr_un), sizeof(ud_socket->addr.addr_un));
            LOG(DBG, "Bind fd on unix socket, path:%s", ud_socket->addr.addr_un.sun_path);
            break;
        case AF_INET:
            res = bind(ud_socket->socket_fd, (struct sockaddr *)&(ud_socket->addr.addr_in), sizeof(ud_socket->addr.addr_in));
            LOG(DBG, "Bind fd on ipv4 socket, ip:%s, port:%hu", sockpath, port);
        default:
            break;
    }
    if (res == -1) {
        UNIX_SOCKET_FREE(ud_socket);
        perror("bind");
        return NULL;
    }
    // listen on the socket fd
    if (listen(ud_socket->socket_fd, back_log) == -1) {
        UNIX_SOCKET_FREE(ud_socket);
        perror("listen");
        return NULL;
    }
    return ud_socket;
}

us_connect_t *unix_socket_connect(
    const int   domain,
    const int   type,
    const int   protocol,
    const char *sockpath,
    const short port,
    const int   back_log)
{
    us_connect_t *ud_socket = UNIX_SOCKET_ALLOC(sizeof(us_connect_t));
    if (ud_socket == NULL) {
        return NULL;
    }

    // init client socket
    int res = unix_socket_init(ud_socket, domain, type, protocol, sockpath, port);
    if (res == -1) {
        UNIX_SOCKET_FREE(ud_socket);
        perror("unix socket init");
        return NULL;
    }

    // connect
    switch (domain) {
        case AF_UNIX:
            res = connect(ud_socket->socket_fd, (struct sockaddr *)&(ud_socket->addr), sizeof(ud_socket->addr.addr_un));
            LOG(DBG, "connect to unix socket, path:%s", ud_socket->addr.addr_un.sun_path);
            break;
        case AF_INET:
            res = connect(ud_socket->socket_fd, (struct sockaddr *)&(ud_socket->addr), sizeof(ud_socket->addr.addr_un));
            LOG(DBG, "connect to ipv4 socket, ip:%s, port:%hu", sockpath, port);
            break;
        default:
            break;
    }
    if (res < 0) {
        UNIX_SOCKET_FREE(ud_socket);
        perror("connect");
        return NULL;
    }

    return ud_socket;
}