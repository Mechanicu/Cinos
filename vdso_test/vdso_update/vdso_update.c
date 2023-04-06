#include "../vdso.h"

void _kernel_set_info(void *vdso_data, void *data)
{
    vdso_data_t *vdso_data_p = (vdso_data_t *)vdso_data;
    vdso_data_p->val         = *(int *)data;
}