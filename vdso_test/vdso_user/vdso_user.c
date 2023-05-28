#include "../vdso.h"
#include "../vdso_srv.h"

void* _kernel_get_info(const void *vdso_data)
{
    return ((srv_vdso_data_t *)vdso_data)->val;
}

void _kernel_get_info_null(void)
{
    return;
}