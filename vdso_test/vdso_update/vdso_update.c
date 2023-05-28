#include "../vdso.h"
#include "../vdso_srv.h"

void _kernel_set_info(void *vdso_data, void *data)
{
    srv_vdso_data_t *vdso_data_p = (srv_vdso_data_t *)vdso_data;
    vdso_data_p->val         = *(int *)data;
}