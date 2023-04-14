#include "../include/rpc_service.h"
#include <string.h>

// for client

extern linkhash_t *service_cap_hash;

static inline void *rpc_client_init(const cptr_t service_cap)
{
    rpc_client_t *rpc_client = (rpc_client_t *)RPC_SERVICE_MALLOC(sizeof(rpc_client_t));
    if (rpc_client) {
        INIT_LIST_HEAD(&(rpc_client->service_hook));
        sem_init(&(rpc_client->client_sem), 0, 0);
        rpc_client->service_cap = service_cap;
    }
    return rpc_client;
}

rpc_client_t *rpc_client_get_service(const unsigned long service_id)
{
    if (!service_cap_hash) {
        LOG_WARING("RPC server cap hash table not created\n");
        return NULL;
    }
    // security check, if client can get cap, then return cptr of cap
    cptr_t service_cap = 0;
    if (!(service_cap = rpc_client_security_check(service_id))) {
        LOG_ERROR("Client can't get cap of rpc:%lu\n", (unsigned long)service_id);
        return NULL;
    }
    // init rpc client for request service
    rpc_client_t *rpc_client = NULL;
    if (!(rpc_client = rpc_client_init(service_cap))) {
        LOG_ERROR("Client init rpc client failed\n");
    }
    LOG_DEBUG("Client get cap of rpc, service_id:%lu, service_cap:%lu, client:%p\n", service_id, service_cap, rpc_client);
    return rpc_client;
}

void rpc_client_stop_service(const unsigned long service_id)
{
    return;
}

static inline void rpc_client_package_params(rpc_client_t *rpc_client, const int service_type)
{
    rpc_client->params[0] = service_type;
}

static inline int rpc_client_send_request(rpc_service_t *service, rpc_client_t *rpc_client)
{
    pthread_spin_lock(&(service->lock));
    // insert current client in service client list
    list_add_tail(&(rpc_client->service_hook), &(service->client_list));
    pthread_spin_unlock(&(service->lock));
    // change context from client to server
    sem_post(&(service->server_sem));
    sem_wait(&(rpc_client->client_sem));
}

void *rpc_client_request_service(rpc_client_t *rpc_client, const int service_type)
{
    if (!rpc_client) {
        return NULL;
    }
    // get capability by cptr
    rpc_service_t *service = (rpc_service_t *)rpc_get_capobj_bycptr(rpc_client->service_cap);
    if (NULL == service) {
        LOG_ERROR("Client can't get cap of rpc:%lx\n", rpc_client->service_cap);
        return NULL;
    }

    // send request basic on request service type
    void *res = NULL;
    switch (service_type) {
        case CLIENT_REQ_MEM_FROM_SERVER:
            rpc_client_package_params(rpc_client, service_type);
            rpc_client_send_request(service, rpc_client);
            return (unsigned long)(rpc_client->params[0]);
        case CLIENT_REQ_DATA_TRANSFER_TO_SERVER:
            break;
        default:
            LOG_DEBUG("Unknown request type:%d\n", service_type);
            break;
    }
    return NULL;
}