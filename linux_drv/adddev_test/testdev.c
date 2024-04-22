#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char const *argv[])
{
    int fd = open("/dev/adddev", O_RDWR);
    if (fd < 0)
    {
        perror("open");
        return 1;
    }
    /*parent write*/
    unsigned int buf = 0x12345678;

    int ret = 0;
    printf("buf:0x%p\n", &buf);
    ret = write(fd, &buf, sizeof(buf));
    printf("Parent write ret = %d, in:0x%x\n", ret, buf);

    if (!fork())
    {
        /*child read*/
        ret = read(fd, &buf, sizeof(buf));
        printf("Child read ret = %d, out:0x%x\n", ret, buf);
    }

    close(fd);
    return 0;
}