#include "../include/rpc_service.h"
#include <string.h>
#include <syscall.h>

// for client

extern linkhash_t *service_cap_hash;

static unsigned long gettid()
{
    return syscall(SYS_gettid);
}

static inline void *rpc_client_init(const cptr_t service_cap)
{
    rpc_client_t *rpc_client = (rpc_client_t *)RPC_SERVICE_MALLOC(sizeof(rpc_client_t));
    if (rpc_client) {
        INIT_LIST_HEAD(&(rpc_client->service_hook));
        sem_init(&(rpc_client->client_sem), 0, 0);
        rpc_client->service_cap          = service_cap;
        rpc_client->rpc_params.client_id = gettid();
    }
    return rpc_client;
}

rpc_client_t *rpc_client_get_service(const unsigned long service_id)
{
    if (!service_cap_hash) {
        LOG_WARING("RPC server cap hash table not created");
        return NULL;
    }
    // security check, if client can get cap, then return cptr of cap
    cptr_t service_cap = 0;
    if (!(service_cap = rpc_client_security_check(service_id))) {
        LOG_ERROR("Client can't get cap of rpc:%lu", (unsigned long)service_id);
        return NULL;
    }
    // init rpc client for request service
    rpc_client_t *rpc_client = NULL;
    if (!(rpc_client = rpc_client_init(service_cap))) {
        LOG_ERROR("Client init rpc client failed");
    }
    LOG_DEBUG("Client get cap of rpc, service_id:%lu, service_cap:0x%lx, client:%p", service_id, service_cap, rpc_client);
    return rpc_client;
}

void rpc_client_stop_service(const unsigned long service_id)
{
    return;
}

static inline void rpc_client_package_params(rpc_client_t *rpc_client, const int service_type)
{
    rpc_client->rpc_params.req_type = service_type;
}

static inline void rpc_client_send_request(rpc_service_t *service, rpc_client_t *rpc_client)
{
    pthread_spin_lock(&(service->lock));
    // insert current client in service client list
    list_add_tail(&(rpc_client->service_hook), &(service->req_list));
    pthread_spin_unlock(&(service->lock));
    // change context from client to server
    sem_post(&(service->server_sem));
    LOG_DEBUG("RPC CLIENT send request, server_id:0x%lx, client_id:0x%lx, service_type:%ld\n", service->server_id, rpc_client->rpc_params.client_id, rpc_client->rpc_params.req_type);
    sem_wait(&(rpc_client->client_sem));
}

void *rpc_client_request_service(rpc_client_t *rpc_client, const unsigned int service_type)
{
    if (!rpc_client) {
        return NULL;
    }
    // get capability by cptr
    rpc_service_t *service = (rpc_service_t *)rpc_get_capobj_bycptr(rpc_client->service_cap);
    if (NULL == service) {
        LOG_ERROR("Client can't get cap of rpc:0x%lx", rpc_client->service_cap);
        return NULL;
    }
    // send request basic on request service type
    void *res = NULL;
    if (service_type > RPC_CLIENT_REQ_TOTAL_TYPE_COUNT) {
        LOG_DEBUG("Unknown request type:%d", service_type);
        return NULL;
    }

    // send rpc request
    rpc_client_package_params(rpc_client, service_type);
    rpc_client_send_request(service, rpc_client);
    // proc response
    LOG_DEBUG("Client request finished");
    switch (service_type) {
        case CLIENT_GET_SERVICE:
            LOG_DEBUG("RPC client get service, shmaddr:%p, size:0x%lx", (void *)(rpc_client->rpc_params.rpc_shm_vaddr), rpc_client->rpc_params.param);
        case CLIENT_REQ_SERVICE_WITH_RSP:
        case CLIENT_REQ_SERVICE_WITHOUT_RSP:
        case CLIENT_STOP_SERVICE:
            return (void *)(rpc_client->rpc_params.rpc_shm_vaddr);
    }
    return NULL;
}