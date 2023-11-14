#include "unix_domain.h"
#include <fcntl.h>
#include <stdio.h>
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
    int           connect_fd = accept(ud->socket_fd, NULL, NULL);
    if (connect_fd == -1) {
        perror("accept");
    }
    char buf[1024];
    while (1) {
        int bytes = read(connect_fd, buf, sizeof(buf));
        if (bytes < 0) {
            perror("read");
            continue;
        }
        printf("server %d recv: %d\n", getpid(), *(int *)buf);
    }
    close(ud->socket_fd);
    return 0;
}