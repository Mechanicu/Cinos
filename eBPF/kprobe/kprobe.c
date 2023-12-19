/*
for vmlinux contain all type defined, it must
be included first
*/
#include "../vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "../config.h"

SEC(SEC_KPROBE(do_unlinkat))
int BPF_KPROBE(do_unlinkat, int dfd, struct filename *name)
{
    pid_t pid;
    const char *filename;
    pid = (u32)bpf_get_current_pid_tgid();
    filename = BPF_CORE_READ(name, name);
    bpf_printk("KPROBE ENTRY pid:%x, filename:%s\n", pid, filename);
    return 0;
}

SEC(SEC_KRETPROBE(do_unlinkat))
int BPF_KRETPROBE(do_unlinkat_exit, long ret)
{
    pid_t pid;
    pid = (u32)bpf_get_current_pid_tgid();
    bpf_printk("KRETPROBE ENTRY pid:%x\n", pid);
    return 0;
}