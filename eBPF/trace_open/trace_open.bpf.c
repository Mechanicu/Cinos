#include "../config.h"

const volatile pid_t pid_filter = 0;

SEC(SEC_SYSCALL_TRACE(sys_enter_openat))
int tracepoint__syscalls_sys_enter_openat(struct trace_event_raw_sys_enter *ctx)
{
    u64 id = bpf_get_current_pid_tgid();
    pid_t pid = (u32)id;
    if (pid_filter != 0 && pid != pid_filter)
        return 0;
    
    bpf_printk("Process:%d enter sys openat\n", pid);
    return 0;
}
