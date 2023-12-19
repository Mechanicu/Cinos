#ifndef EXECSNOOP_H
#define EXECSNOOP_H

#define TASK_COMM_LEN 16
typedef struct kernel_event {
    int pid;
    int ppid;
    int uid;
    int ret;
    char is_exit;
    char comm[TASK_COMM_LEN];
} kevent_t;

#endif