#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int fd;
    int ret;

    do {
        fd = open("./test.txt", O_RDWR | O_CREAT | O_TRUNC, 0777);
        if (fd < 0)
        {
            printf("open file failed\n");
            return -1;
        }
        close(fd);
        sleep(1);
    } while(1);
    return 0;
}