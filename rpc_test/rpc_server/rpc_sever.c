#include "../include/rpc_service.h"

#define RPC_SERVER_THREAD_CREATE(thread_t, attr, handler, arg) pthread_create(thread_t, attr, handler, arg)
#define RPC_SERVER_THREAD_DESTORY(thread_t)                    pthread_cancel(thread_t)

//
static rpc_srv_params_t rpc_service_get_request(rpc_service_t *service)
{
    sem_wait(&(service->server_sem));
    list_t *request = service->req_list.next;
    list_del_init(request);
    rpc_client_t *client = container_of(request, rpc_client_t, service_hook);
    LOG_DEBUG("RPC SERVER get request, client_id:%lx, req_type:%ld", client->client_id, client->rpc_params.req_type);
    if (client->rpc_params.req_type == CLIENT_GET_SERVICE) {
        list_add_tail(&(client->service_hook), &(service->waiter_list));
    }
    return (client->rpc_params);
}

static void rpc_service_send_response(rpc_service_t *service, rpc_srv_params_t *rsp_params)
{
    list_t *waiter = service->waiter_list.next;
    list_del_init(waiter);
    rpc_client_t *client = container_of(waiter, rpc_client_t, service_hook);
    LOG_DEBUG("RPC SERVER send response, client_id:%lx, shmaddr:%p\n", client->client_id, (void *)(rsp_params->params[0]));
    // store response params in clinet struct
    client->rpc_params.params[0] = rsp_params->params[0];
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

void *rpc_service_default_handler(void *arg)
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
        params = rpc_service_get_request(service);
        switch (params.req_type) {
            case CLIENT_GET_SERVICE:
                rpc_client_shm = mempool_alloc(rpc_shm_pool);
                if (rpc_client_shm) {
                    linkhash_add((unsigned long)(rpc_client_shm->block_start), rpc_client_shm, rpc_shm_hash);
                    params.params[0] = (unsigned long)rpc_client_shm;
                    rpc_service_send_response(service, &params);
                }
                break;
            case CLIENT_REQUEST_SERVICE:
                break;
            case CLIENT_STOP_SERVICE:
                break;
            default:
                break;
        }
    }
exit:
    linkhash_destroy(rpc_shm_hash);
    mempool_destroy(rpc_shm_pool, rpc_shm_block_free);
}

rpc_server_thread_t rpc_service_start(const cptr_t service_cap, void *service_handler)
{
    // check cap
    rpc_service_t *service = NULL;
    if (!(service = rpc_get_capobj_bycptr(service_cap))) {
        LOG_ERROR("RPC SERVER START, invalid cptr");
        return 0;
    }
    void *real_service_handler = service_handler;
    if (!real_service_handler) {
        LOG_WARING("RPC SERVER START, use default service handler");
        real_service_handler = rpc_service_default_handler;
    }

    // start rpc_server
    rpc_server_thread_t server = 0;
    if (RPC_SERVER_THREAD_CREATE(&server, NULL, real_service_handler, service) != 0) {
        LOG_ERROR("RPC SERVER START, failed to start rpc_server");
    }
    LOG_DEBUG("RPC SERVER START, rpc_server:%lx", server);
    return server;
}