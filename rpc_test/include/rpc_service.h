#ifndef _RPC_SERVICE_H_
#define _RPC_SERVER_H_

#include "hashlist.h"
#include "log.h"
#include "mem_pool.h"
#include <pthread.h>
#include <semaphore.h>

#define RPC_SERVICE_MALLOC(size)     malloc(size)
#define RPC_SERVICE_FREE(ptr)        free(ptr)

#define MAX_RPC_SERVICE_BUCKET_COUNT 128
#define MAX_RPC_SHM_BLOCK_COUNT      256
#define MAX_RPC_PARAMS_COUNT         2

#define PAGE_SIZE                    4096

typedef pthread_t rpc_server_thread_t;

typedef unsigned long cptr_t;

// for service control
enum rpc_service_ctrl {
    SERVER_REGISTER_SERVICE,
    SERVER_UNREGISTER_SERVICE,
};

enum rpc_client_ctrl {
    CLIENT_GET_SERVICE,
    CLIENT_STOP_SERVICE,
    CLIENT_REQUEST_SERVICE,
};

typedef struct rpc_service {
    cptr_t             server_id;
    sem_t              server_sem;
    list_t             req_list;
    list_t             rsp_list;
    pthread_spinlock_t lock;
} rpc_service_t;

typedef struct rpc_service_params {
    cptr_t        client_id;
    unsigned long req_type;
    void         *rpc_shm_vaddr;
    unsigned long param;
} rpc_srv_params_t;
typedef struct rpc_client {
    list_t           service_hook;
    cptr_t           service_cap;
    sem_t            client_sem;
    rpc_srv_params_t rpc_params;
} rpc_client_t;

void         *rpc_get_capobj_bycptr(const cptr_t service_cap);
// for server
cptr_t        rpc_service_register(const unsigned long service_id, const cptr_t server_id);
void          rpc_service_unregister(const unsigned long service_id);
cptr_t        rpc_service_start(const cptr_t service_cap, void *service_handler);
// for client
rpc_client_t *rpc_client_get_service(const unsigned long service_id);
void         *rpc_client_request_service(rpc_client_t *rpc_client, int service_type);
void          rpc_client_stop_service(const unsigned long service_id);
cptr_t        rpc_client_security_check(const unsigned long service_id);

#endif