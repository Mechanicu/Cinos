#include "inode.h"
#include "disk_ops.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int g_disk_fd = 0;

uint64_t user_disk_write(const void *buf, uint64_t size, uint64_t disk_off)
{
    return pwrite(g_disk_fd, buf, size, disk_off);
}

uint64_t user_disk_read(void *buf, uint64_t size, uint64_t disk_off)
{
    return pread(g_disk_fd, buf, size, disk_off);
}

int userfs_disk_open(const char *pathname)
{
    g_disk_fd = open(pathname, O_RDWR);
    if (g_disk_fd < 0) {
        return g_disk_fd;
    }
    return -1;
}

int user_disk_close(void)
{
    return close(g_disk_fd);
}