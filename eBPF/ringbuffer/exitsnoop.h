#ifndef EXITSNOOP_H
#define EXITSNOOP_H

#define TASK_COMM_LEN 16
typedef struct kernel_event {
    int pid;
    int ppid;
    u32 exit_code;
    u64 duration_ns;
    char comm[TASK_COMM_LEN];
} kevent_t;
#endif