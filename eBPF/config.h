#ifndef CONFIG_EBPF_H
#define CONFIG_EBPF_H
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#define SEC_STRING(section) #section

#define SEC_KPROBE(mount) SEC_STRING(kprobe/mount)
#define SEC_KRETPROBE(mount) SEC_STRING(kretprobe/mount)

#define SEC_FENTRY(mount) SEC_STRING(fentry/mount)
#define SEC_FEXITPROBE(mount) SEC_STRING(fexit/mount)

#define SEC_TRACEPOINT(mount) SEC_STRING(tracepoint/mount)
#define SEC_SYSCALL_TRACE(mount) SEC_TRACEPOINT(syscalls/mount)
#define SEC_SCHED_TRACE(mount) SEC_TRACEPOINT(sched/mount)

#define SEC_TP_BPF(mount) SEC_STRING(tp_bpf/mount)

#define SEC_URETPROBE(binpath, function) SEC_STRING(uretprobe/binpath:function)
#define SEC_URETPROBE_OFF(binpath, function, offset) SEC_STRING(uretprobe/binpath:function+offset)
#define SEC_UPROBE(binpath, function)
#define SEC_UPROBE_OFF(binpath, function, offset)

char LISENSE[] SEC("license") = "Dual BSD/GPL";

#endif