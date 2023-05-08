#include "../log.h"
#include "vdso.h"
// #define __USE_XOPEN2K
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

void  vdso_init_from_sysinfo_ehdr(unsigned long base);
void *vdso_sym(const char *version, const char *name);
void  _kernel_set_info(void *vdso_data, void *data);

const char *vdso_syms[] = {
    "_kernel_get_info",
    "_kernel_get_info_null",
};

typedef void (*vdso_get_info_t)(const void *vdso_data, void *data);
typedef void (*vdso_get_info_n_t)(void);

extern char vdso_start[];
extern char vdso_end[];

void *client_pthread(void *arg)
{
    vdso_info_t *info      = (vdso_info_t *)arg;
    void        *vdso_code = info->vdso_code_vaddr;
    void        *vdso_data = info->vdso_data_vaddr;
    LOG_DEBUG("VDSO client vdso_code_vaddr:%p, vdso_data_vaddr:%p", vdso_code, vdso_data);
    vdso_init_from_sysinfo_ehdr((unsigned long)vdso_code);
    vdso_get_info_t   vgetinfo   = (vdso_get_info_t)vdso_sym("1.1", vdso_syms[0]);
    vdso_get_info_n_t vgetinfo_n = (vdso_get_info_n_t)vdso_sym("1.1", vdso_syms[1]);
    LOG_DEBUG("VDSO client vgetinfo:%p, vgetinfo_n:%p", vgetinfo, vgetinfo_n);
    int data = 0;
    while (1) {
        vgetinfo(vdso_data, &data);
        sleep(1);
        LOG_DEBUG("VDSO client read_data:%d", data);
    }
    return NULL;
}

vdso_data_t vdso_data = {.val = 0};

int main(int argc, char *argv[])
{
    LOG_DEBUG("vdso_start:%p, vdso_end:%p", vdso_start, vdso_end);
    vdso_info_t vdso_info = {.vdso_code_vaddr = vdso_start,
                             .vdso_data_vaddr = &vdso_data};
    // pthread_t   client    = 0;
    // pthread_create(&client, NULL, client_pthread, &vdso_info);

    if (!fork()) {
        // child process as client
        client_pthread(&vdso_info);
    } else {
        int i = 1;
        while (1) {
            _kernel_set_info(&vdso_data, &i);
            i++;
            sleep(1);
        }
    }
    // vgetinfo(&vdso_test_data, &data);
    // vgetinfo_n();
    // LOG_DEBUG("vdso_data:%u, data:%u", vdso_test_data.val, data);
    return 0;
}