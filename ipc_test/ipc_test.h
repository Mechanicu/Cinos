#ifndef _IPC_TEST_H_
#define _IPC_TEST_H_
#include "../log.h"
#include "dep.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define ENABLE_IPC_SHM 1
#if ENABLE_IPC_SHM == 1
#ifndef PAGE_SIZE_SHIFT
#define PAGE_SIZE_SHIFT (12)
#endif
// runtime error
#define EIPCINVALIDARGS        1
#define EIPCMSGTOOLONG         1
#define EIPCINVALIDOPS         1
#define EIPCOUTIPCBUF          1
// init error
#define EIPCREQSHM             0
#define EIPCCREATEP            0
#define EIPCINVALIDPAGES       0
#define EIPCINVALIDBUFSIZE     0
#define EIPCCREATATTRNODE      0

#define IPC_MIN_BUFBYTES       32
#define IPC_MAX_BUFBYTES       512
#define IPC_MAX_MSGBYTES       ((sizeof(unsigned long) << 8) - sizeof(unsigned long))
#define IPC_MAX_MSGCOUNT       252
#define IPC_SHM_HEADER_BYTES   512

// debug macro
#define IPC_SHM_DATA_ONLY_TEST 1
#define IPC_SHM_TEST_LOG       0
#endif

#if ENABLE_IPC_SHM == 1
#define ENABLE_IPC_SHM_SMSG      0
#define EP_IPC_TEST_COUNT        1

#define EP_LOCK_INIT(lock, flag) pthread_spin_init(lock, flag)
#define EP_LOCK(lock)            pthread_spin_lock(lock)
#define EP_TRYLOCK(lock)         pthread_spin_trylock(lock)
#define EP_UNLOCK(lock)          pthread_spin_unlock(lock)

#if (IPC_SHM_DATA_ONLY_TEST == 1)
#define IPC_SEND_CTRL(cptr, tag, idx) set_tag(cptr, tag, idx)
#define IPC_RECV_CTRL(cptr, tag, idx) get_tag(cptr, tag, idx)
#else
#define IPC_SEND_CTRL(cptr, tag, block) __doipc1(cptr, tag, block)
#define IPC_RECV_CTRL(cptr, tag, block) __doipc1(cptr, tag, block)
#endif
#endif

#if ENABLE_IPC_SHM == 1
#if (IPC_SHM_DATA_ONLY_TEST == 1)
// only for test
volatile unsigned int g_tag[IPC_MAX_MSGCOUNT] = {0};

unsigned int get_tag(unsigned long cptr, unsigned int tag, unsigned int idx)
{
    return g_tag[idx];
}
unsigned int set_tag(unsigned long cptr, unsigned int tag, unsigned int idx)
{
    g_tag[idx] = tag;
    return tag;
}
#endif

typedef pthread_spinlock_t spinlock_t;
typedef unsigned long      cptr_t;
enum {
    FREE_IDX,
    FULL_IDX
};

typedef struct ep_ipc_attr {
    cptr_t ep;        // cptr of endpoint
    void  *shmaddr;   // viraddr of shm
} ep_ipc_attr_t;

typedef struct ep_ipc_bufinfo {
    volatile unsigned char next_buf_idx;   // next buf in current static list
    volatile unsigned char msg_count;      // msg used bufs count
} ep_ipc_bufinfo_t;

typedef struct _ep_ipc_header {
    volatile int            rwlock;         // spinlock for shm header
    volatile unsigned char  slist[1];       // first buf_idx in current list
    volatile unsigned char  slist_len[1];   // current list length
    volatile unsigned short userbufsize;    // user defined each ipcbuf bytes
    struct ep_ipc_bufinfo   ipcinfo[IPC_MAX_MSGCOUNT];
} _ep_ipc_header_t;

typedef union ep_ipc_header {
    struct _ep_ipc_header header;
    char                  format[IPC_SHM_HEADER_BYTES];   // make sure shm header use 512B
} ep_ipc_header_t;

// struct to manage shm
typedef struct ep_ipc_que {
    union ep_ipc_header header;   // ipcbufs info
    char                que[0];   // ipcbufs
} ep_ipc_que_t;

// static list ops
// insert a continuous seq in @type's list
static inline void seq_in_static_list(struct _ep_ipc_header *header, int start, int end, int type, int bufcount)
{
    if (bufcount) {
        header->ipcinfo[end].next_buf_idx  = header->slist[type];
        header->slist[type]                = start;
        //
        header->slist_len[type]           += bufcount;
    }
}

// get and delete a continuous seq from @type's list
// return start index of seq
static inline int seq_out_static_list(struct _ep_ipc_header *header, int type, int bufcount)
{
    int first = header->slist[type];
    int next  = first;
    for (int i = 0; i < bufcount; i++) {
        first = header->ipcinfo[first].next_buf_idx;
    }
    header->slist_len[type] -= bufcount;
    header->slist[type]      = first;
    return next;
}

// calculate total ipcbuf in ep
static inline unsigned long calculate_ipcbuf_count(const unsigned long pages, const unsigned long bufsize)
{
    unsigned long bufcount  = pages << PAGE_SIZE_SHIFT;
    bufcount               -= sizeof(union ep_ipc_header);
    return (bufcount / bufsize);
}

// init ipc header in shm of ep
static inline unsigned long init_ep_ipc_que_header(void *shmaddr, const unsigned long pages,
                                                   const unsigned long bufsize)
{
    struct _ep_ipc_header *ipc_header = (struct _ep_ipc_header *)shmaddr;
    // init spinlock
    EP_LOCK_INIT((spinlock_t *)&(ipc_header->rwlock), 0);

    // init ipcinfo
    // cal buf count
    unsigned long bufcount = calculate_ipcbuf_count(pages, bufsize);

    for (int i = 0; i < bufcount - 1; i++) {
        ipc_header->ipcinfo[i].next_buf_idx = i + 1;
    }

    ipc_header->slist[FREE_IDX]                    = 0;
    ipc_header->ipcinfo[bufcount - 1].next_buf_idx = -1;
    ipc_header->slist_len[FREE_IDX]                = bufcount;
    ipc_header->userbufsize                        = bufsize;

    return bufcount;
}

unsigned long init_ep_ipc_que(void *shmaddr, const unsigned long pages, const unsigned long bufsize)
{
    return init_ep_ipc_que_header(shmaddr, pages, bufsize);
}

static int _ipc_shm_send_data(void *shmaddr, const void *buf, const unsigned long size)
{
    // calculate bufs count for current ipc
    struct _ep_ipc_header *header   = (struct _ep_ipc_header *)shmaddr;
    unsigned short         bufsize  = header->userbufsize;
    int                    len      = (size + sizeof(unsigned long) - 1) / sizeof(unsigned long);
    int                    bufcount = (size + bufsize - 1) / bufsize;

    // lock shm header and get a continuous bufs list from free list
    EP_LOCK((spinlock_t *)&(header->rwlock));
    if (bufcount > header->slist_len[FREE_IDX]) {
        EP_UNLOCK((spinlock_t *)&(header->rwlock));
        return -EIPCOUTIPCBUF;
    }
    int start = seq_out_static_list(header, FREE_IDX, bufcount);
    EP_UNLOCK((spinlock_t *)&(header->rwlock));

    // make tag with index of start buf, opcode and msg length
    int tag                          = msgtag_make(len, 0, start);
    header->ipcinfo[start].msg_count = bufcount;

    // copy sender buf to shm
    char         *que                = ((struct ep_ipc_que *)header)->que;
    unsigned long send_size          = size;
    for (int i = 0; i < bufcount; i++) {
        memcpy(que + bufsize * start, buf, send_size > bufsize ? bufsize : send_size);
        buf       += bufsize;
        send_size -= bufsize;
        //
        start      = header->ipcinfo[start].next_buf_idx;
    }
#if IPC_SHM_TEST_LOG == 1
    printf("reg size:%lu | send size:%ld | len:%d | bufcount:%d | start_buf:%d | list_head:%d | next:%d\n",
           sizeof(unsigned long), size, len, bufcount, start, header->slist[FREE_IDX],
           header->ipcinfo[start].next_buf_idx);
#endif
    return tag;
}

static int _ipc_shm_recv_data(void *attr, void *buf, unsigned int tag, unsigned long maxsize)
{
    // receiver get start_buf_idx and buf_len from tag
    int                    start    = msgtag_get_extra(tag);
    struct _ep_ipc_header *header   = (struct _ep_ipc_header *)attr;
    unsigned short         bufsize  = header->userbufsize;
    int                    bufcount = header->ipcinfo[start].msg_count;

    // record start_buf_idx and end_buf_idx to simplify returning buf ops
    int next                        = start;
    int end                         = start;

    char         *que               = ((struct ep_ipc_que *)header)->que;
    unsigned long size              = msgtag_get_len(tag) * sizeof(unsigned long);
    unsigned long tmpsize           = size;
#if IPC_SHM_TEST_LOG == 1
    printf("reg size:%lu | recv size:%lu| bufcount:%d | start_buf:%d | list_head:%d\n", sizeof(unsigned long), size,
           bufcount, start, header->slist[FREE_IDX]);
#endif

    // copy data from shm to receiver buf
    for (int i = 0; i < bufcount; i++) {
        memcpy(buf, que + bufsize * next, tmpsize > bufsize ? bufsize : tmpsize);
        buf     += bufsize;
        tmpsize -= bufsize;
        end      = next;
        next     = header->ipcinfo[next].next_buf_idx;
    }

    // lock shm header to return bufs currently used
    EP_LOCK((spinlock_t *)&(header->rwlock));
    seq_in_static_list(header, start, end, FREE_IDX, bufcount);
    EP_UNLOCK((spinlock_t *)&(header->rwlock));
    return size;
}

unsigned int ipc_shm_send(cptr_t ep, void *shmaddr, const void *buf, const unsigned long size, const char block)
{
    // check params
    if (!shmaddr || !buf) {
        return -EIPCINVALIDARGS;
    }
    if (size > IPC_MAX_MSGBYTES) {
        return -EIPCMSGTOOLONG;
    }

    // send data
    // short msg and long msg use different way to send, also means they received in different ways
    int tag = 0;
#if ENABLE_IPC_SHM_SMSG == 1
    if (size > MAX_IPC_SHM_SMSG_BYTES) {
#endif
        // copy data to shm and return tag for receiver
        tag = _ipc_shm_send_data(shmaddr, buf, size);
        if (tag == -EIPCOUTIPCBUF) {
            return tag;
        }
#if ENABLE_IPC_SHM_SMSG == 1
    } else {
        tag = _ipc_shm_send_short_data(buf, size);
    }
#endif

    // control flow, send tag to receiver
#if IPC_SHM_TEST_LOG == 1
    printf("send tag:%x\n", tag);
#endif
    return IPC_SEND_CTRL(ep, tag, block);
}

unsigned int ipc_shm_recv(cptr_t ep, void *shmaddr, void *buf, const unsigned long maxsize, const char block)
{
    // check params
    if (!shmaddr || !buf) {
        return -EIPCINVALIDARGS;
    }

    // recv tag from sender
    // short msg and long msg use different way to send, also means they received in different ways
    unsigned int tag = msgtag_make(0, 0, 0);
    tag              = IPC_RECV_CTRL(ep, tag, block);
    if (tag == -1) {
        return tag;
    }
#if IPC_SHM_TEST_LOG == 1
    printf("recv tag:%x\n", tag);
#endif

#if ENABLE_IPC_SHM_SMSG == 1
    if (msgtag_get_extra(tag) != 0xff) {
#endif
        // used tag from sender to received data from shm
        tag = _ipc_shm_recv_data(shmaddr, buf, tag, maxsize);
#if ENABLE_IPC_SHM_SMSG == 1
    } else {
        tag = _ipc_shm_recv_short_data(tag, buf, maxsize);
    }
#endif
    return tag;
}

#endif
#endif