#ifndef _VDSO_DOMAIN_H_
#define _VDSO_DOMAIN_H_
#define _GNU_SOURCE
#include "../log.h"
#include "include/kernel_list.h"
#include <semaphore.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct vdso_tcb_cap {
    list_t        domain_hook;
    list_t        endpoint_hook;
    sem_t         schedule;
    unsigned long tid;
    unsigned long reg_params;
    unsigned long extra;
} tcb_t;

typedef struct vdso_cap {
    list_t domain_client;
    tcb_t *owner;
    void  *domain_data_addr;
} domain_t;

typedef struct vdso_service_cap {
    tcb_t *server;
    list_t client;
} endpoint_t;

unsigned long gettid();
tcb_t        *tcb_create();
domain_t     *domain_create(const void *vdso_data_addr, tcb_t *creator);
void         *domain_refer(domain_t *domain, tcb_t *referrer);
endpoint_t   *endpoint_create(tcb_t *creator);
unsigned long endpoint_send(endpoint_t *service, tcb_t *client, unsigned long param, unsigned long extra);
unsigned long endpoint_recv(endpoint_t *service, unsigned long *client_tid, unsigned long* extra);
void          endpoint_reply(endpoint_t *service, unsigned long params);
#endif