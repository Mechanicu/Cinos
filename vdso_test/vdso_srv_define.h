#ifndef _VDSO_SRV_DEFINE_H_ 
#define _VDSO_SRV_DEFINE_H_

const char* srv_vdso_ver = "1.1";
const char* srv_vdso_sym_name[] = {
    "_kernel_get_info",
    "_kernel_get_info_null",
};

typedef void* (*srv_vdso_get_info_t)(const void *vdso_data);
typedef void (*srv_vdso_get_info_n_t)(void);
#endif