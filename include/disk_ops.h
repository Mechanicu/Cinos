#ifndef DISK_OPS
#define DISK_OPS
#include "inode.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

uint64_t user_disk_write(const void *buf, uint64_t size, uint64_t disk_off);
uint64_t user_disk_read(void *buf, uint64_t size, uint64_t disk_off);
int      userfs_disk_open(const char *pathname);
int      user_disk_close(void);

#endif