#ifndef DISK_OPS
#define DISK_OPS
#include "inode.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

off_t user_disk_write(const void *buf, off_t size, off_t disk_off);
off_t user_disk_read(void *buf, off_t size, off_t disk_off);
int   userfs_disk_open(const char *pathname);
int   user_disk_close(int fd);

#endif