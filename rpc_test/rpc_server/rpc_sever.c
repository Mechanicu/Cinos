#include "../include/rpc_service.h"

#define RPC_SERVER_THREAD_CREATE(thread_t, attr, handler, arg) pthread_create(thread_t, attr, handler, arg)
#define RPC_SERVER_THREAD_DESTORY(thread_t)                    pthread_cancel(thread_t)

typedef pthread_t rpc_server_thread_t;

void *rpc_service_default_handler(void *arg)
{
    LOG_DEBUG("RPC SERVICE DEFAULT HANDLER START\n");
    rpc_service_t* service = (rpc_service_t*)arg;
}

void rpc_service_start(const cptr_t service_cap, void *service_handler)
{
    // check cap
    rpc_service_t *service = NULL;
    if (!(service = rpc_get_capobj_bycptr(service_cap))) {
        LOG_ERROR("RPC SERVER START, invalid cptr\n");
        return;
    }
    void *real_service_handler = service_handler;
    if (!real_service_handler) {
        LOG_WARING("RPC SERVER START, use default service handler\n");
        real_service_handler = rpc_service_default_handler;
    }

    // start rpc_server
    LOG_DEBUG("RPC SERVER start\n");
    rpc_server_thread_t server = 0;
    if (RPC_SERVER_THREAD_CREATE(&server, NULL, real_service_handler, service) != 0) {
        LOG_ERROR("RPC SERVER START, failed to start rpc_server\n");
        return;
    }
}