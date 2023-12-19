#define BPF_NO_GLOBAL_DATA
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

typedef unsigned int u32;
typedef unsigned long long u64;
typedef int pid_t;
const pid_t pid_filter = 0;

char LISENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tp/syscalls/sys_enter_write")
int handle_tp(void* ctx)
{
    u64 id = bpf_get_current_pid_tgid();
    pid_t pid = (u32)id;
    pid_t tgid = (u32)(id >> 32);
    if (pid_filter != 0 && pid != pid_filter)
        return 0;
    bpf_printk("BPF trigger tgid:%x, pid:%x, p&tgid:%llx\n", tgid, pid, id);
    return 0;
}