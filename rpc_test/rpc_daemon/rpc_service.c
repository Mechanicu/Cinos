#include "../include/rpc_service.h"
#include "../include/hashlist.h"
#include "../include/list.h"
#include "../include/log.h"
#include <pthread.h>

linkhash_t *service_cap_hash = NULL;

static inline cptr_t mk_rpc_service_cap(const unsigned long server_id)
{
    rpc_service_t *service_cap = RPC_SERVICE_MALLOC(sizeof(rpc_service_t));
    if (service_cap != NULL) {
        INIT_LIST_HEAD(&(service_cap->req_list));
        service_cap->server_id = server_id;
        sem_init(&(service_cap->server_sem), 0, 0);
        pthread_spin_init(&(service_cap->lock), 0);
    }
    return (cptr_t)service_cap;
}

static inline void del_rpc_service_cap(cptr_t ptr)
{
    RPC_SERVICE_FREE((void *)ptr);
}

cptr_t rpc_service_register(const unsigned long service_id, const cptr_t server_id)
{
    //
    if (!service_cap_hash) {
        service_cap_hash = linkhash_create(MAX_RPC_SERVICE_BUCKET_COUNT);
        if (NULL == service_cap_hash) {
            LOG_ERROR("Service cap linkhash create failed.");
            return 0;
        }
    }

    // check whether service_id already exsists
    unsigned long service_check = 0;
    if ((service_check = linkhash_get(service_id, service_cap_hash)) != 0) {
        LOG_WARING("RPC service %lu already exists, service_cap:%lu.", server_id, service_check);
        return service_check;
    }

    // create cap for current service, and store service in service cap hash table
    cptr_t service_cap = mk_rpc_service_cap(server_id);
    if (!service_cap) {
        LOG_ERROR("mk_rpc_service_cap failed.");
    }

    int res = linkhash_add(service_id, (void *)service_cap, service_cap_hash);
    if (res == -1) {
        LOG_ERROR("Add hash failed.");
        del_rpc_service_cap(service_cap);
        return 0;
    }
    LOG_DEBUG("Register new server id:%lu, cptr:%lx.", service_id, service_cap);
    return service_cap;
}

void rpc_service_unregister(const unsigned long service_id)
{
    if (!service_cap_hash) {
        LOG_WARING("RPC server cap hash table not created");
    }
    cptr_t service_cap = 0;
    if ((service_cap = linkhash_remove(service_id, service_cap_hash))) {
        del_rpc_service_cap(service_cap);
        return;
    }
    LOG_WARING("RPC service%lu not found in hash table.", service_id);
}

cptr_t rpc_client_security_check(const unsigned long service_id)
{
    pthread_t cur_tid     = pthread_self();
    cptr_t    service_cap = linkhash_get(service_id, service_cap_hash);
    LOG_DEBUG("client thread id:%lx, srv_cptr:%lx, srv_id:%lu", cur_tid, service_cap, service_id);
    return service_cap;
}

void *rpc_get_capobj_bycptr(const cptr_t service_cap)
{
    pthread_t cur_tid = pthread_self();
    LOG_DEBUG("RPC GET CAP_OBJ, thread id:%lx, srv_cptr:%lx, srv_cap_obj:%p", cur_tid, service_cap, (void *)service_cap);
    return ((void *)service_cap);
}