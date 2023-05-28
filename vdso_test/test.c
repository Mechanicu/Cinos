#include "../log.h"
#include "vdso.h"
#include "vdso_domain.h"
#include "vdso_srv.h"
#include "vdso_srv_define.h"
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

static inline unsigned int hash_32bkey(const unsigned int key)
{
    unsigned int hash_key = key;
    hash_key              = (~hash_key) + (hash_key << 18); /* hash_key = (hash_key << 18) - hash_key - 1; */
    hash_key              = hash_key ^ (hash_key >> 31);
    hash_key              = hash_key * 21;                  /* hash_key = (hash_key + (hash_key << 2)) + (hash_key << 4); */
    hash_key              = hash_key ^ (hash_key >> 11);
    hash_key              = hash_key + (hash_key << 6);
    hash_key              = hash_key ^ (hash_key >> 22);
    return hash_key;
}

void _kernel_set_info(void *vdso_data, void *data);

extern char vdso_start[];
extern char vdso_end[];

void *vdso_test_client(void *arg)
{
    vdso_addr_t *info      = (vdso_addr_t *)arg;
    void        *vdso_code = info->vdso_code_vaddr;
    void        *vdso_data = info->vdso_data_vaddr;
    LOG_DEBUG("VDSO client vdso_code_vaddr:%p, vdso_data_vaddr:%p", vdso_code, vdso_data);

    vdso_info_t           vdso_info  = {0};
    srv_vdso_get_info_t   vgetinfo   = (srv_vdso_get_info_t)vdso_sym(vdso_code, "1.1", srv_vdso_sym_name[0], &vdso_info);
    srv_vdso_get_info_n_t vgetinfo_n = (srv_vdso_get_info_n_t)vdso_sym(vdso_code, "1.1", srv_vdso_sym_name[1], NULL);
    vgetinfo_n                       = (srv_vdso_get_info_n_t)vdso_sym(NULL, "1.1", srv_vdso_sym_name[1], &vdso_info);

    LOG_DEBUG("VDSO client vgetinfo:%p, vgetinfo_n:%p", vgetinfo, vgetinfo_n);
    int data = 0;
    while (1) {
        vgetinfo(vdso_data);
        sleep(1);
        LOG_DEBUG("VDSO client read_data:%d", (unsigned int)vgetinfo(vdso_data));
    }
    return NULL;
}

domain_t *domain_shared = NULL;

void *vdso_client1(void *arg)
{
    endpoint_t *service = (endpoint_t *)arg;
    tcb_t      *client  = tcb_create();

    domain_shared       = domain_create(malloc(PAGE_SIZE), client);
    LOG_DEBUG("vdso client, tid:%lx, shared_vdso_data_addr:%p", client->tid, domain_shared->domain_data_addr);

    //
    unsigned long update_time = 1;
    unsigned long vdso_code   = endpoint_send(service, client, domain_shared->domain_data_addr, update_time);
    LOG_DEBUG("vdso client, tid:%lx, vdso_code_base:%lx", client->tid, vdso_code);

    vdso_info_t         vdso_info = {0};
    srv_vdso_get_info_t vgetinfo  = (srv_vdso_get_info_t)vdso_sym(vdso_code, "1.1", srv_vdso_sym_name[0], &vdso_info);
    LOG_DEBUG("VDSO client get vdso func:tid:%lx, name:%s, addr:%p", client->tid, srv_vdso_sym_name[0], vgetinfo);

    while (1) {
        LOG_DEBUG("VDSO client read shared data:tid:%lx, val:%lx",
                  client->tid, (unsigned int)vgetinfo(domain_shared->domain_data_addr));
        sleep(1);
    }
}

void *vdso_client2(void *arg)
{
    endpoint_t *service        = (endpoint_t *)arg;
    tcb_t      *client         = tcb_create();

    domain_t *domain_private   = domain_create(malloc(PAGE_SIZE), client);
    void     *shared_vdso_data = domain_refer(domain_shared, client);
    LOG_DEBUG("vdso client, tid:%lx, private_vdso_data_addr:%p, shared_vdso_data_addr:%p",
              client->tid, domain_private->domain_data_addr, shared_vdso_data);

    //
    unsigned long update_time = 10;
    unsigned long vdso_code   = endpoint_send(service, client, domain_private->domain_data_addr, update_time);
    LOG_DEBUG("vdso client, tid:%lx, vdso_code_base:%lx", client->tid, vdso_code);

    vdso_info_t         vdso_info = {0};
    srv_vdso_get_info_t vgetinfo  = (srv_vdso_get_info_t)vdso_sym(vdso_code, "1.1", srv_vdso_sym_name[0], &vdso_info);
    LOG_DEBUG("VDSO client get vdso func:tid:%lx, name:%s, addr:%p", client->tid, srv_vdso_sym_name[0], vgetinfo);

    while (1) {
        LOG_DEBUG("VDSO client read shared data:tid:%lx, val:%lx",
                  client->tid, (unsigned int)vgetinfo(domain_shared->domain_data_addr));
        LOG_DEBUG("VDSO client read private data:tid:%lx, val:%lx",
                  client->tid, (unsigned int)vgetinfo(domain_private->domain_data_addr));
        sleep(1);
    }
}

int main(int argc, char *argv[])
{
    LOG_DEBUG("vdso_start:%p, vdso_end:%p", vdso_start, vdso_end);
    tcb_t      *server  = tcb_create();
    endpoint_t *service = endpoint_create(server);

    pthread_t client    = 0;
    pthread_create(&client, NULL, vdso_client1, service);
    sleep(1);
    pthread_create(&client, NULL, vdso_client2, service);
    sleep(1);

    //
#define HASH_SIZE 16
    unsigned long hash[HASH_SIZE]    = {0};
    unsigned long client_tid         = 0;
    unsigned long hashkey            = 0;

    unsigned long shared_update_time = 0;
    void         *vdso_data_shared   = (void *)endpoint_recv(service, &client_tid, &shared_update_time);
    LOG_DEBUG("vdso client, tid:%lx, hash:%x, vdso_data_shared_addr:%p, update_time:%lx",
              client_tid, hashkey = hash_32bkey((unsigned int)client_tid) % HASH_SIZE, vdso_data_shared, shared_update_time);
    hash[hashkey] = vdso_data_shared;
    endpoint_reply(service, vdso_start);
    sleep(2);

    unsigned long private_update_time = 0;
    void         *vdso_data_private   = (void *)endpoint_recv(service, &client_tid, &private_update_time);
    LOG_DEBUG("vdso client, tid:%lx, hash:%lx, vdso_data_private_addr:%lx, update_time:%lx",
              client_tid, hashkey = hash_32bkey((unsigned int)client_tid) % HASH_SIZE, vdso_data_private, private_update_time);
    hash[hashkey] = vdso_data_private;
    endpoint_reply(service, vdso_start);

    int          i       = 1;
    unsigned int shared  = 0;
    unsigned int private = 0;
    LOG_DEBUG("vdso_shared_data:%p, vdso_private_data:%p", vdso_data_shared, vdso_data_private);
    while (1) {
        sleep(1);
        i++;
        if (!(i % shared_update_time)) {
            shared++;
            _kernel_set_info(vdso_data_shared, &shared);
        }
        if (!(i % private_update_time)) {
            private++;
            _kernel_set_info(vdso_data_private, &private);
        }
    }

    return 0;
}