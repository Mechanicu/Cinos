#include "include/test_rpcservice.h"
#include "include/rpc_service.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

static unsigned long gettid()
{
    return syscall(SYS_gettid);
}

void *rpc_userdefine_req_with_rsp_handler(const void *arg)
{
    rpc_srv_params_t *params = (rpc_srv_params_t *)arg;
    LOG_DEBUG("client_id:0x%lx, shm_vaddr:%p", params->client_id, params->rpc_shm_vaddr);
    test_rpcservice_params_t *file_params = (test_rpcservice_params_t *)(params->rpc_shm_vaddr);
    int                       fd          = open(file_params->file_name, O_RDWR, 0777);
    if (fd < 0) {
        LOG_ERROR("open file fail:%s", file_params->file_name);
        return (void *)0;
    }
    size_t read_size = read(fd, file_params->file_content, file_params->str_len);
    LOG_DEBUG("Client read from file:%s, read_size:%lx, content:%s", file_params->file_name, read_size, file_params->file_content);
    file_params->str_len = read_size;
    close(fd);
    return read_size;
}

void *rpc_userdefine_req_without_rsp_handler(const void *arg)
{
    rpc_srv_params_t *params = (rpc_srv_params_t *)arg;
    LOG_DEBUG("client_id:0x%lx, shm_vaddr:%p", params->client_id, params->rpc_shm_vaddr);
    test_rpcservice_params_t *file_params = (test_rpcservice_params_t *)(params->rpc_shm_vaddr);
    int                       fd          = open(file_params->file_name, O_RDWR | O_CREAT | O_APPEND, 0777);
    if (fd < 0) {
        LOG_ERROR("open file fail:%s", file_params->file_name);
        return (void *)0;
    }
    size_t write_size = write(fd, file_params->file_content, file_params->str_len);
    LOG_DEBUG("Client write to file:%s, write_size:%lx, content:%s", file_params->file_name, write_size, file_params->file_content);
    close(fd);
    return write_size;
}

int main(int argc, char **argv)
{
    int            service_id  = 0;
    cptr_t         service_cap = rpc_service_register(service_id, gettid());
    rpc_service_t *service     = (rpc_service_t *)service_cap;
    LOG_DEBUG("RPC SERVICE register, server_id:0x%lx, service_id:0x%lx, self:0x%lx", service_id, service->server_id, gettid());
    rpc_service_handlers_t rpc_service_handlers = {.handlers[CLIENT_REQ_SERVICE_WITH_RSP]    = rpc_userdefine_req_with_rsp_handler,
                                                   .handlers[CLIENT_REQ_SERVICE_WITHOUT_RSP] = rpc_userdefine_req_without_rsp_handler};
    rpc_server_thread_t    server_id            = rpc_service_start(service_cap, &rpc_service_handlers);

    LOG_DEBUG("client_thread begin");
    rpc_client_t *rpc_client = rpc_client_get_service(service_id);
    LOG_DEBUG("RPC client get_service, service_cap:0x%lx", rpc_client->service_cap);
    void *shmaddr = rpc_client_request_service(rpc_client, CLIENT_GET_SERVICE);
    LOG_DEBUG("RPC client get rpc_shm:%p\n", shmaddr);
    shmaddr = rpc_client_request_service(rpc_client, CLIENT_GET_SERVICE);
    LOG_DEBUG("RPC client get rpc_shm:%p\n", shmaddr);

    //
    for (int i = 0; i < 8; i++) {
        test_rpcservice_params_t *file_params      = (test_rpcservice_params_t *)shmaddr;
        char                      file_name[]      = "rpc_service_test";
        //
        char                      file_content[64] = {0};
        sprintf(file_content, "This is a string%d which is written to a file\n", i);
        memcpy(file_params->file_content, file_content, strlen(file_content));
        memcpy(file_params->file_name, file_name, strlen(file_name) + 1);
        file_params->str_len = strlen(file_content);
        LOG_DEBUG("Client request write to file:%s, read_size:%lx, content:%s", file_params->file_name, file_params->str_len, file_params->file_content);
        rpc_client_request_service(rpc_client, CLIENT_REQ_SERVICE_WITHOUT_RSP);
        sleep(1);
    }

    //
    for (int i = 0; i < 16; i++) {
        test_rpcservice_params_t *file_params = (test_rpcservice_params_t *)shmaddr;
        char                      file_name[] = "rpc_service_test";
        //
        memcpy(file_params->file_name, file_name, strlen(file_name) + 1);
        file_params->str_len = 45;
        rpc_client_request_service(rpc_client, CLIENT_REQ_SERVICE_WITH_RSP);
        LOG_DEBUG("Client request read from file:%s, read_size:%lx, content:%s", file_params->file_name, file_params->str_len, file_params->file_content);

        // sleep(1);
    }
    pthread_join(server_id, NULL);
    return 0;
}