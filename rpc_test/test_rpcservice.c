#include "include/rpc_service.h"
#include <pthread.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int            service_id  = 0;
    cptr_t         service_cap = rpc_service_register(service_id, pthread_self());
    rpc_service_t *service     = (rpc_service_t *)service_cap;
    LOG_DEBUG("RPC SERVICE register, server_id:0x%lx, service_id:0x%lx, self:0x%lx", service_id, service->server_id, pthread_self());
    rpc_server_thread_t server_id = rpc_service_start(service_cap, NULL);

    LOG_DEBUG("client_thread begin");
    rpc_client_t *rpc_client = rpc_client_get_service(service_id);
    LOG_DEBUG("RPC client get_service, service_cap:0x%lx", rpc_client->service_cap);
    void* shmaddr = rpc_client_request_service(rpc_client, CLIENT_GET_SERVICE);
    LOG_DEBUG("RPC client get rpc_shm:%p\n", shmaddr);

    // 
    
    pthread_join(server_id, NULL);
    return 0;
}