#ifndef _VDSO_H
#define _VDSO_H

#define PAGE_SIZE 4096

typedef struct _vdso_data {
    /* data */
    int val;
} vdso_data_t;

typedef struct _vdso_info {
    /* data */
    void *vdso_code_vaddr;
    void *vdso_data_vaddr;
} vdso_info_t;

#endif
