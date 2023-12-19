/*
ATTENTION:bpf_printk can only allow 3 output args, more args can pass compiling,
but cannot run
*/
#include "../vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "../config.h"

SEC(SEC_FENTRY(do_unlinkat))
int BPF_PROG(do_unlinkat, int dfd, struct filename *filename)
{
    u64 id = bpf_get_current_pid_tgid();
    pid_t pid = (u32)id;
    pid_t tgid = (u32)(id >> 32);
    bpf_printk("unlinkat: pid:%d, tgid:%d, filename:%s\n", pid, tgid, filename->name);
    return 0;
}

SEC(SEC_FEXITPROBE(do_unlinkat))
int BPF_PROG(do_unlinkat_exit, int dfd, struct filename *filename, long ret)
{
    u64 id = bpf_get_current_pid_tgid();
    pid_t pid = (u32)id;
    pid_t tgid = (u32)(id >> 32);
/* 
wrong example:
bpf_printk("unlinkat_exit: pid:%d, tgid:%d, filename:%s, ret:%ld\n",
        pid, tgid, filename->name, ret);
*/
    bpf_printk("unlinkat_exit: pid:%d, filename:%s, ret:%ld\n", pid, filename->name, ret);
    return 0;
}