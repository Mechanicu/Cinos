#ifndef UNIX_DOMAIN_H
#define UNIX_DOMAIN_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct unix_socket_type {
    int domain;
    int type;
    int protocol;
} us_type_t;

typedef struct unix_socket_connect {
    int       socket_fd;
    us_type_t type;
    union {
        struct sockaddr_un addr_un;
        struct sockaddr_in addr_in;
    } addr;
} us_connect_t;

us_connect_t *unix_socket_listen(
    const int   domain,
    const int   type,
    const int   protocol,
    const char *sockpath,
    const short port,
    const int   back_log);

us_connect_t *unix_socket_connect(
    const int   domain,
    const int   type,
    const int   protocol,
    const char *sockpath,
    const short port,
    const int   back_log);

static unsigned long xwrite(const int fd, void *buf, const unsigned long size)
{
    unsigned long total = 0;
    unsigned long current;
    while (((current = write(fd, buf + total, size - total)) != -1)) {
        if ((total += current) == size) {
            return total;
        }
    }
    return current;
}

static unsigned long xread(const int fd, void *buf, const unsigned long size)
{
    unsigned long total = 0;
    unsigned long current;
    while (((current = read(fd, buf + total, size - total)) != -1)) {
        if ((total += current) == size) {
            return total;
        }
    }
    return current;
}
#endif