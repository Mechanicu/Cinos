#include "../log.h"
#include "ipc_test.h"
#include <malloc.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    ep_ipc_attr_t test         = {0};
    unsigned long pages        = 1;

    void *shmaddr              = malloc(pages * 4096);
    test.shmaddr               = shmaddr;
    unsigned long     bufcount = init_ep_ipc_que(shmaddr, pages, 512);
    _ep_ipc_header_t *que      = test.shmaddr;
    LOG_DEBUG("bufcount:%lu %d", bufcount, que->slist_len[FREE_IDX]);
    for (int i = 0; i < bufcount; i++) {
        LOG_DEBUG("idx:%d\tnext:%d", i, que->ipcinfo[i].next_buf_idx);
    }
    putchar('\n');
    // for (int tmp = 0; tmp < 2; tmp++) {
    char          buf[128]     = "hello world";
    unsigned char buf_recv[32] = {0};
    unsigned long count        = ipc_shm_send(test.ep, test.shmaddr, buf, 128, 0);
    LOG_DEBUG("IPC send buf:%s", buf);
    LOG_DEBUG("tag:%x | freeidx:%d", count, que->slist[FREE_IDX]);
    for (int i = 0; i < bufcount; i++) {
        LOG_DEBUG("idx:%d\tnext:%d", i, que->ipcinfo[i].next_buf_idx);
    }
    putchar('\n');
    count = ipc_shm_send(test.ep, test.shmaddr, buf, 128, 1);
    LOG_DEBUG("IPC send buf:%s", buf);
    LOG_DEBUG("tag:%x | freeidx:%d", count, que->slist[FREE_IDX]);
    for (int i = 0; i < bufcount; i++) {
        LOG_DEBUG("idx:%d\tnext:%d", i, que->ipcinfo[i].next_buf_idx);
    }
    putchar('\n');

    unsigned long size = ipc_shm_recv(test.ep, test.shmaddr, buf_recv, 0, 0);
    LOG_DEBUG("IPC recv buf:%s", buf_recv);
    LOG_DEBUG("count:%lu | freeidx:%d", size, que->slist[FREE_IDX]);
    for (int i = 0; i < bufcount; i++) {
        LOG_DEBUG("idx:%d\tnext:%d", i, que->ipcinfo[i].next_buf_idx);
    }
    putchar('\n');

    size = ipc_shm_recv(test.ep, test.shmaddr, buf_recv, 0, 1);
    LOG_DEBUG("IPC recv buf:%s", buf_recv);
    LOG_DEBUG("count:%lu | freeidx:%d", size, que->slist[FREE_IDX]);
    for (int i = 0; i < bufcount; i++) {
        LOG_DEBUG("idx:%d\tnext:%d", i, que->ipcinfo[i].next_buf_idx);
    }
    putchar('\n');

    // }

    free(shmaddr);
    return 0;
}