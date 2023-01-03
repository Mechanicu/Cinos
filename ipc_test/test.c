#include <stdio.h>
#include <malloc.h>
#include "ipc_test.h"

int main(int argc, char **argv)
{
    ep_ipc_attr test = {0};
    unsigned long pages = init_ep_ipc_attr(&test, 3, 0);
    check_ipcque_size(pages);

    void *shmaddr = malloc(pages * PAGE_SIZE);
    test.shmaddr = shmaddr;
    unsigned long bufcount = init_ep_ipc_que(&test);
    _ep_ipc_header *que = test.shmaddr;
    printf("header size:%lu\n", sizeof(ep_ipc_header));
    printf("bufcount:%lu %d %d\n", bufcount, que->slist_len[FREE_IDX], que->slist_len[FULL_IDX]);
    for (int i = 0; i < bufcount; i++)
    {
        printf("idx:%d\tnext:%d\n", i, que->ipcinfo[i].next_buf_idx);
    }
    putchar('\n');
    for (int tmp = 0; tmp < 2; tmp++)
    {
        char buf[64] = "hello world";
        unsigned char buf_recv[32] = {0};
        memset(buf, 0xff, 64);
        unsigned long count = _ipc_send(&test, buf, strlen(buf));
        printf("count:%lu | freeidx:%d | fullidx:%d\n", count, que->slist[FREE_IDX], que->slist[FULL_IDX]);
        unsigned long size = _ipc_recv(&test, buf_recv, 0);
        printf("count:%lu | freeidx:%d | fullidx:%d\n", size, que->slist[FREE_IDX], que->slist[FULL_IDX]);
        for (int i = 0; i < bufcount; i++)
        {
            printf("idx:%d\tnext:%d\n", i, que->ipcinfo[i].next_buf_idx);
        }
    }

    free(shmaddr);
    return 0;
}