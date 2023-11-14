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
    int           pid        = getpid();
    while (1) {
        sleep(1);
        pid++;
        int bytes = write(ud->socket_fd, &pid, sizeof(pid));
        if (bytes < sizeof(pid)) {
            perror("read");
            continue;
        }
    }
    return 0;
}