#include "../config.h"
#define MAX_ENTRIES 10240
#define TASK_COMM_LEN 16

struct sig_event
{
    /* data */
    u32 pid;
    u32 tpid;
    int sig;
    int ret;
    char comm[TASK_COMM_LEN];
};

/*What's these macros means?*/
struct
{
    __type(key, u32);
    __type(value, struct sig_event);
    __uint(max_entries, MAX_ENTRIES);
    __uint(type, BPF_MAP_TYPE_HASH);
} values SEC(".maps");

static int probe_entry(pid_t tpid, int sig)
{
    struct sig_event event = {0};
    u64 pid_tgid;
    u32 tgid;

    pid_tgid = bpf_get_current_pid_tgid();
    tgid = (u32)pid_tgid;
    event.pid = (u32)(pid_tgid >> 32);
    event.tpid = tpid;
    event.sig = sig;

    bpf_get_current_comm(event.comm, TASK_COMM_LEN);
    bpf_map_update_elem(&values, &tgid, &event, BPF_ANY);
    return 0;
}

static int probe_exit(void *ctx, int ret)
{
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tgid = (u32)pid_tgid;
    struct sig_event *eventp;

    eventp = bpf_map_lookup_elem(&values, &tgid);
    if (!eventp)
        return 0;

    eventp->ret = ret;
    bpf_printk("PID:%d(%s) sent signal:%d ",
               eventp->pid, eventp->comm, eventp->sig);
    bpf_printk("to PID:%d, ret:%d\n", eventp->tpid, eventp->ret);
cleanup:
    bpf_map_delete_elem(&values, &tgid);
    return 0;
}

SEC(SEC_SYSCALL_TRACE(sys_enter_kill))
int kill_entry(struct trace_event_raw_sys_enter *ctx)
{
    pid_t tpid = (pid_t)(ctx->args[0]);
    int sig = (int)(ctx->args[1]);
    return probe_entry(tpid, sig);
}

SEC(SEC_SYSCALL_TRACE(sys_exit_kill))
int kill_exit(struct trace_event_raw_sys_exit *ctx)
{
    return probe_exit(ctx, ctx->ret);
}