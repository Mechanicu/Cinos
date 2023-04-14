#include "include/rpc_service.h"
#include <pthread.h>

int main(int argc, char **argv)
{
    int service_id = 0;
    cptr_t service_cap = rpc_service_register(service_id, pthread_self());
    rpc_service_t* service = (rpc_service_t*)service_cap;
    LOG_DEBUG("RPC SERVICE register, server_id:%lx, service_id:%lx, self:%lx\n", service_id, service->server_id, pthread_self());
    rpc_client_t* rpc_client = rpc_client_get_service(service_id);
    LOG_DEBUG("RPC client get_service, service_cap:%lx\n", rpc_client->service_cap);
    rpc_client_request_service(rpc_client, CLIENT_REQUEST_SERVICE);
    return 0;
}