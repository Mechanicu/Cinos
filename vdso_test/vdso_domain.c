#include "vdso_domain.h"
#include "../log.h"
#include <malloc.h>
#include <sys/types.h>

unsigned long gettid()
{
    return syscall(SYS_gettid);
}

tcb_t *tcb_create()
{
    tcb_t *tcb = malloc(sizeof(tcb_t));
    if (tcb) {
        INIT_LIST_HEAD(&tcb->domain_hook);
        INIT_LIST_HEAD(&tcb->endpoint_hook);
        sem_init(&tcb->schedule, 0, 0);
        tcb->tid = gettid();
        LOG_DEBUG("TCB CREATE: tid:%lx", tcb->tid);
    }
    return tcb;
}

domain_t *domain_create(const void *vdso_data_addr, tcb_t *creator)
{
    if (!vdso_data_addr || !creator) {
        LOG_ERROR("DOMAIN domain init: invalid arguments");
        return;
    }
    domain_t *domain         = malloc(sizeof(domain_t));
    domain->domain_data_addr = vdso_data_addr;
    domain->owner            = creator;
    INIT_LIST_HEAD(&(domain->domain_client));
    list_add(&(creator->domain_hook), &(domain->domain_client));
    LOG_DEBUG("DOMAIN INIT: domain:%p, owner:%lx, vdso_data_addr:%p", domain, domain->owner->tid, domain->domain_data_addr);
    return domain;
}

void *domain_refer(domain_t *domain, tcb_t *referrer)
{
    if (!domain || !referrer) {
        LOG_DEBUG("DOMAINS REFER: domain or referrer is invalid");
        return NULL;
    }
    list_add(&(referrer->domain_hook), &(domain->domain_client));
    LOG_DEBUG("DOMAINS REFER: domain:%p, referrer:%lx", domain, referrer->tid);

    tcb_t *tmp = NULL;
    list_for_each_entry(tmp, &(domain->domain_client), tcb_t, domain_hook)
    {
        LOG_DEBUG("DOMAIN REFER: client list:%lx", tmp->tid);
    }
    return domain->domain_data_addr;
}

endpoint_t *endpoint_create(tcb_t *creator)
{
    if (!creator) {
        LOG_ERROR("ENDPOINT CREATE:invalid argument");
        return NULL;
    }
    endpoint_t *endpoint = malloc(sizeof(endpoint_t));
    if (endpoint) {
        INIT_LIST_HEAD(&endpoint->client);
        endpoint->server = creator;
        LOG_DEBUG("ENDPOINT CREATE: endpoint:%lx, server:%lx", endpoint, endpoint->server->tid);
    }
    return endpoint;
}

unsigned long endpoint_send(endpoint_t *service, tcb_t *client, unsigned long param, unsigned long extra)
{
    if (!service || !client) {
        LOG_ERROR("ENDPOINT SEND:invalid argument");
        return 0;
    }
    client->reg_params = param;
    list_add_tail(&client->endpoint_hook, &service->client);
    client->extra = extra;
    LOG_DEBUG("ENDPOINT SEND: client:%lx, server:%lx, params:%lx, extra:%lx",
              client->tid, service->server->tid, client->reg_params, client->extra);
    //
    sem_post(&service->server->schedule);
    sem_wait(&client->schedule);
    return client->reg_params;
}

unsigned long endpoint_recv(endpoint_t *service, unsigned long *client_tid, unsigned long *extra)
{
    if (!service) {
        LOG_ERROR("ENDPOINT RECV:invalid argument");
        return 0;
    }
    sem_wait(&service->server->schedule);
    tcb_t *client = NULL;
    list_for_each_entry(client, &service->client, tcb_t, endpoint_hook)
    {
        LOG_DEBUG("ENDPOINT RECV: client:%lx, server:%lx, param:%lx, extra:%lx",
                  client->tid, service->server->tid, client->reg_params, client->extra);
        *client_tid = client->tid;
        *extra      = client->extra;
        break;
    }
    return client->reg_params;
}

void endpoint_reply(endpoint_t *service, unsigned long params)
{
    if (!params || !service) {
        LOG_ERROR("ENDPOINT REPLY: invalid argument");
        return;
    }
    tcb_t *client = NULL;
    list_for_each_entry(client, &service->client, tcb_t, endpoint_hook)
    {
        LOG_DEBUG("ENDPOINT REPLY: client:%lx, server:%lx, params:%lx", client->tid, service->server->tid, params);
        client->reg_params = params;
        list_del_init(&client->endpoint_hook);
        sem_post(&client->schedule);
        break;
    }
}