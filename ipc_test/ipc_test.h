#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include "dep.h"

#define EOUTIPCBUF 1

#define PAGE_SIZE 4096
#define PAGE_BITS 12
#define IPC_MAX_MSGQUEPAGES 2
#define IPC_MAX_MSGBYTES 512
#define IPC_MAX_MSGBITS 9
#define IPC_MAX_MSGCOUNT 64

#define ULONG_MAX (0ul - 1)
#if (ULONG_MAX == (0xffffffff))
#define ARM_REG_SIZE 4
#else
#define ARM_REG_SIZE 8
#endif
#define IPC_MAX_MSGWORDS ((IPC_MAX_MSGBYTES / ARM_REG_SIZE))

#define EP_LOCK_INIT(lock, flag) pthread_spin_init(lock, flag)
#define EP_LOCK(lock) pthread_spin_lock(lock)
#define EP_TRYLOCK(lock) pthread_spin_trylock(lock)
#define EP_UNLOCK(lock) pthread_spin_unlock(lock)

#define SYSCALL_SEND(cptr, tag) set_tag(cptr, tag);
#define SYSCALL_RECV(cptr, tag) get_tag(cptr, tag);
volatile unsigned int g_tag = 0;

static inline unsigned int get_tag(unsigned long cptr, unsigned int tag)
{
    return g_tag;
}
static inline unsigned int set_tag(unsigned long cptr, unsigned int tag)
{
    g_tag = tag;
}

typedef pthread_spinlock_t spinlock_t;
typedef unsigned long cptr_t;

typedef struct ep_ipc_attr
{
    cptr_t ep;
    size_t pages;
    void *shmaddr;
    unsigned int flag;
} ep_ipc_attr;

enum
{
    FREE_IDX,
    FULL_IDX
};

typedef struct _ep_ipc_header
{
    spinlock_t rwlock;
    volatile unsigned char slist[2];
    volatile unsigned char slist_len[2];
    unsigned int tag[IPC_MAX_MSGCOUNT];
    struct
    {
        volatile unsigned char next_buf_idx; // next buf in current static list
        volatile unsigned char msg_count;    // msg used bufs count
    } ipcinfo[IPC_MAX_MSGCOUNT];
} _ep_ipc_header;

typedef union ep_ipc_header
{
    struct _ep_ipc_header header;
    char format[IPC_MAX_MSGBYTES];
} ep_ipc_header;

typedef struct ep_ipc_buffer
{
    unsigned long msgs[IPC_MAX_MSGWORDS]; // msg contents
} ep_ipc_buffer;

typedef struct ep_ipc_que
{
    union ep_ipc_header header;
    struct ep_ipc_buffer que[0];
} ep_ipc_que;

// static list ops
static inline void seq_in_static_list(struct _ep_ipc_header *header, int start, int end, int type, int bufcount)
{
    int first = header->slist[type];
    int next = header->ipcinfo[first].next_buf_idx;
    header->ipcinfo[end].next_buf_idx = next;
    header->ipcinfo[first].next_buf_idx = start;

    header->slist_len[type] += bufcount;
}

static inline int seq_out_static_list(struct _ep_ipc_header *header, int type, int bufcount)
{
    int first = header->slist[type];
    int next = first;
    int pre = 0;
    for (int i = 0; i < bufcount; i++)
    {
        pre = first;
        first = header->ipcinfo[first].next_buf_idx;
    }
    header->slist_len[type] -= bufcount;
    header->slist[type] = first;
    header->ipcinfo[pre].next_buf_idx = -1;
    printf("start:%d\tend:%d\n", next, pre);
    return next;
}

// init
// 1
unsigned long init_ep_ipc_attr(struct ep_ipc_attr *ipc_attr, unsigned long pages, unsigned long flag)
{
    if (!pages || pages > IPC_MAX_MSGQUEPAGES)
    {
        pages = IPC_MAX_MSGQUEPAGES;
    }
    ipc_attr->pages = pages;
    return pages;
}

// 2
static inline unsigned long calculate_ipcbuf_count(unsigned long pages)
{
    unsigned long bufcount = pages << PAGE_BITS;
    bufcount -= sizeof(union ep_ipc_header);
    return (bufcount >> IPC_MAX_MSGBITS);
}

static inline unsigned long init_ep_ipc_que_header(void *shmaddr, unsigned long pages)
{
    struct _ep_ipc_header *ipc_header = (struct _ep_ipc_header *)shmaddr;
    // init spinlock
    EP_LOCK_INIT(&(ipc_header->rwlock), PTHREAD_PROCESS_SHARED);

    ipc_header->slist[FREE_IDX] = 0;
    ipc_header->slist[FULL_IDX] = -1;

    // init ipcinfo
    // cal buf count
    unsigned long bufcount = calculate_ipcbuf_count(pages);

    for (int i = 0; i < bufcount - 1; i++)
    {
        ipc_header->ipcinfo[i].next_buf_idx = i + 1;
    }

    ipc_header->ipcinfo[bufcount - 1].next_buf_idx = -1;
    ipc_header->slist_len[FREE_IDX] = bufcount;
    ipc_header->slist_len[FULL_IDX] = 0;

    return bufcount;
}

static inline int check_ipcque_size(size_t pages)
{
    if (!pages || pages > IPC_MAX_MSGQUEPAGES)
    {
        return -1;
    }
    return 0;
}

unsigned long init_ep_ipc_que(struct ep_ipc_attr *ipc_attr)
{
    void *shmaddr = ipc_attr->shmaddr;
    unsigned long bufcount = init_ep_ipc_que_header(shmaddr, ipc_attr->pages);
    return bufcount;
}

// send
unsigned long _ipc_send(struct ep_ipc_attr *attr, void *buf, unsigned long size)
{
    // data flow
    struct _ep_ipc_header *header = (struct _ep_ipc_header *)(attr->shmaddr);
    int len = (size + ARM_REG_SIZE - 1) / ARM_REG_SIZE;
    int bufcount = size / IPC_MAX_MSGBYTES;

    // get bufs from free list
    EP_LOCK(&(header->rwlock));
    if (bufcount > header->slist_len[FREE_IDX])
    {
        EP_UNLOCK(&(header->rwlock));
        return -EOUTIPCBUF;
    }
    int start = seq_out_static_list(header, FREE_IDX, bufcount);
    EP_UNLOCK(&(header->rwlock));

    //
    int tag = msgtag_make(len, 0, start);
    header->tag[start] = tag;
    header->ipcinfo[start].msg_count = bufcount;

    struct ep_ipc_que *que = (struct ep_ipc_que *)header;
    int end = start;
    for (int i = 0; i < bufcount; i++)
    {
        printf("buf:%p | size:%lu | start:%d\n", buf, size, end);
        memcpy(&(que->que[end]), buf, size > IPC_MAX_MSGBYTES ? IPC_MAX_MSGBYTES : size);
        buf += IPC_MAX_MSGBYTES;
        size -= IPC_MAX_MSGBYTES;
        //
        end = header->ipcinfo[end].next_buf_idx;
    }

#ifdef ENABLE_FULL_LIST
    EP_LOCK(&(header->rwlock));
    seq_in_static_list(header, start, end, FULL_IDX, bufcount);
    EP_UNLOCK(&(header->rwlock));
#endif

    // control flow
    printf("sendtag:%x\n", tag);
    SYSCALL_SEND(attr->ep, tag);

    return bufcount;
}

unsigned long _ipc_recv(struct ep_ipc_attr *attr, void *buf, unsigned long maxsize)
{
    // get tag
    int tag = SYSCALL_RECV(attr->ep, tag);
    int start = msgtag_get_extra(tag);
    printf("recvidx:%x\n", start);

    //
    struct _ep_ipc_header *header = (struct _ep_ipc_header *)(attr->shmaddr);
    int bufcount = header->ipcinfo[start].msg_count;
    int next = start;
    int end = 0;

    //
    struct ep_ipc_que *que = (struct ep_ipc_que *)header;
    unsigned long size = msgtag_get_len(tag) * ARM_REG_SIZE;
    unsigned long tmpsize = size;
    printf("recvsize:%lu\n", size);

    for (int i = 0; i < bufcount; i++)
    {
        printf("buf:%p | size:%lu | start:%d\n", buf, tmpsize, next);
        memcpy(buf, &(que->que[next]), tmpsize > IPC_MAX_MSGBYTES ? IPC_MAX_MSGBYTES : tmpsize);
        buf += IPC_MAX_MSGBYTES;
        tmpsize -= IPC_MAX_MSGBYTES;
        end = next;
        next = header->ipcinfo[next].next_buf_idx;
    }
    printf("start:%d\tend:%d\n", start, end);
    //
    EP_LOCK(&(header->rwlock));
    seq_in_static_list(header, start, end, FREE_IDX, bufcount);
    EP_UNLOCK(&(header->rwlock));

    return size;
}