#include "../include/rpc_service.h"
#include <string.h>

#define RPC_SERVER_THREAD_CREATE(thread_t, attr, handler, arg) pthread_create(thread_t, attr, handler, arg)
#define RPC_SERVER_THREAD_DESTORY(thread_t)                    pthread_cancel(thread_t)

// default rpc service handlers
static void rpc_default_req_with_rsp_handler(void)
{
    LOG_WARING("RPC service req_with_rsp.");
}

static void rpc_default_req_without_rsp_handler(void)
{
    LOG_WARING("RPC service req_without_rsp.");
}

rpc_service_handlers_t rpc_default_handlers = {
    .handlers[CLIENT_REQ_SERVICE_WITH_RSP]    = &rpc_default_req_with_rsp_handler,
    .handlers[CLIENT_REQ_SERVICE_WITHOUT_RSP] = &rpc_default_req_without_rsp_handler,
};

//
static rpc_srv_params_t rpc_service_get_request(rpc_service_t *service)
{
    sem_wait(&(service->server_sem));
    pthread_spin_lock(&(service->lock));
    list_t *request = service->req_list.next;
    list_del_init(request);
    pthread_spin_unlock(&(service->lock));

    rpc_client_t *client = container_of(request, rpc_client_t, service_hook);
    LOG_DEBUG("RPC SERVER get request, client_id:0x%lx, req_type:%ld", client->rpc_params.client_id, client->rpc_params.req_type);
    if (client->rpc_params.req_type == CLIENT_GET_SERVICE) {
        list_add_tail(&(client->service_hook), &(service->rsp_list));
    }
    return (client->rpc_params);
}

static void rpc_service_send_response(rpc_service_t *service, rpc_srv_params_t *rsp_params)
{
    list_t *waiter = service->rsp_list.next;
    list_del_init(waiter);
    rpc_client_t *client             = container_of(waiter, rpc_client_t, service_hook);
    client->rpc_params.param         = rsp_params->param;
    client->rpc_params.rpc_shm_vaddr = rsp_params->rpc_shm_vaddr;
    LOG_DEBUG("RPC SERVER send response, client_id:0x%lx, shmaddr:%p\n", client->rpc_params.client_id, client->rpc_params.rpc_shm_vaddr);
    // store response params in clinet struct
    // change context from server to client
    sem_post(&(client->client_sem));
}

static void *rpc_shm_block_alloc(unsigned long *size)
{
    if (*size > PAGE_SIZE) {
        return NULL;
    }
    unsigned long real_size = *size;
    real_size               += PAGE_SIZE - 1;
    real_size               &= ~(PAGE_SIZE - 1);
    LOG_DEBUG("Wanted %lu bytes, Allocate %lu bytes", *size, real_size);
    *size = real_size;

    return malloc(real_size);
}

static void rpc_shm_block_free(void *block_start)
{
    if (block_start) {
        free(block_start);
    }
}

void *rpc_service_handler(void *arg)
{
    LOG_DEBUG("RPC SERVICE DEFAULT HANDLER START");
    rpc_service_t *service      = (rpc_service_t *)arg;
    mempool_t     *rpc_shm_pool = mempool_create(PAGE_SIZE, MAX_RPC_SHM_BLOCK_COUNT, rpc_shm_block_alloc);
    if (!rpc_shm_pool) {
        LOG_ERROR("RPC SHM POOL CREATED FAILED");
        return NULL;
    }
    linkhash_t *rpc_shm_hash = linkhash_create(MAX_RPC_SERVICE_BUCKET_COUNT);
    if (!rpc_shm_hash) {
        LOG_ERROR("RPC SHM HASH CREATED FAILED");
        return NULL;
    }
    rpc_srv_params_t params         = {0};
    mempool_block_t *rpc_client_shm = NULL;
    while (1) {
        LOG_DEBUG("RPC SERVER wait client");
        params         = rpc_service_get_request(service);
        rpc_client_shm = (mempool_block_t *)linkhash_get((unsigned long)(params.client_id), rpc_shm_hash);
        switch (params.req_type) {
            case CLIENT_GET_SERVICE:
                // check if client already request
                if (rpc_client_shm != NULL) {
                    LOG_DEBUG("Client%lu already requested rpc_client_shm", params.client_id);
                } else {
                    // if not, then request one for client
                    rpc_client_shm = mempool_alloc(rpc_shm_pool);
                    if (rpc_client_shm != NULL) {
                        linkhash_add((unsigned long)(params.client_id), rpc_client_shm, rpc_shm_hash);
                    } else {
                        LOG_ERROR("RPC SERVER alloc shm for client failed");
                    }
                }
                // map or send rpc_shm info to client
                params.rpc_shm_vaddr = rpc_client_shm->block_start;
                params.param         = rpc_client_shm->size;
                LOG_ERROR("RPC SERVER alloc client_shm:%p, size:0x%lx", params.rpc_shm_vaddr, params.param);
                rpc_service_send_response(service, &params);
                break;
            case CLIENT_REQ_SERVICE_WITH_RSP:

                break;
            case CLIENT_REQ_SERVICE_WITHOUT_RSP:
                if (rpc_client_shm != NULL) {
                    LOG_DEBUG("RPC SERVER service for client_id:%lx, shm_vaddr:%p", params.client_id, rpc_client_shm->block_start);
                } else {
                    LOG_ERROR("RPC SERVER service for client_id:%lx not found", params.client_id);
                }
                break;
            case CLIENT_STOP_SERVICE:
                break;
            default:
                LOG_WARING("RPC SERVER UNKNOWN REQUEST");
                break;
        }
    }
exit:
    linkhash_destroy(rpc_shm_hash);
    mempool_destroy(rpc_shm_pool, rpc_shm_block_free);
}

rpc_server_thread_t rpc_service_start(const cptr_t service_cap, rpc_service_handlers_t *service_handlers)
{
    // check cap
    rpc_service_t *service = NULL;
    if (!(service = rpc_get_capobj_bycptr(service_cap))) {
        LOG_ERROR("RPC SERVER START, invalid cptr");
        return 0;
    }

    // record rpc handlers for each request type in service
    rpc_service_handlers_t *real_service_handlers = service_handlers;
    if (!service_handlers) {
        LOG_WARING("RPC SERVER START, use default service handler");
        real_service_handlers = &rpc_default_handlers;
    }
    memcpy(&(service->srv_handlers), real_service_handlers, sizeof(void *) * RPC_CLIENT_REQ_TYPE_COUNT);

    // start rpc_server
    rpc_server_thread_t server = 0;
    if (RPC_SERVER_THREAD_CREATE(&server, NULL, rpc_service_handler, service) != 0) {
        LOG_ERROR("RPC SERVER START, failed to start rpc_server");
    }
    LOG_DEBUG("RPC SERVER START, rpc_server:0x%lx", server);
    return server;
}