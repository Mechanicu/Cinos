#ifndef _RUNQLAT_BPF_H_
#define _RUNQLAT_BPF_H_
#include "../vmlinux.h"

#define TASK_COMM_LEN 16
#define MAX_TIME_POWER 16

typedef struct
{
    u32 slots[MAX_TIME_POWER + 1];
    char comm[TASK_COMM_LEN];
} hist_t;


#endif