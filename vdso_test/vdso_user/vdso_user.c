#include "../vdso.h"

void _kernel_get_info(const void *vdso_data, void *data)
{
    *(int *)data = ((vdso_data_t *)vdso_data)->val;
}

void _kernel_get_info_null(void)
{
    return;
}