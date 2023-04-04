#include "../log.h"
#include "vdso.h"
#include <stdlib.h>

void  vdso_init_from_sysinfo_ehdr(unsigned long base);
void *vdso_sym(const char *version, const char *name);

const char *vdso_syms[] = {
    "_kernel_get_info",
    "_kernel_get_info_null",
};

typedef void (*vdso_get_info_t)(const void *vdso_data, void *data);
typedef void (*vdso_get_info_n_t)(void);

typedef struct _vdso_data {
    /* data */
    int val;
} vdso_data_t;

extern char vdso_start[];
extern char vdso_end[];

int main(int argc, char *argv[])
{
    LOG_DEBUG("vdso_start:%p, vdso_end:%p\n", vdso_start, vdso_end);
    vdso_init_from_sysinfo_ehdr((unsigned long)vdso_start);
    vdso_get_info_t   vgetinfo       = (vdso_get_info_t)vdso_sym("1.1", vdso_syms[0]);
    vdso_get_info_n_t vgetinfo_n     = (vdso_get_info_n_t)vdso_sym("1.1", vdso_syms[1]);
    vdso_data_t       vdso_test_data = {.val = 0xff};
    int               data           = 0;
    LOG_DEBUG("vgetinfo:%p, vgetinfo_n:%p\n", vgetinfo, vgetinfo_n);
    // vgetinfo(&vdso_test_data, &data);
    vgetinfo_n();
    LOG_DEBUG("vdso_data:%u, data:%u\n", vdso_test_data.val, data);
    return 0;
}