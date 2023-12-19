#include "../vmlinux.h"
#include "../config.h"
#include "bpf/bpf_helpers.h"
#include "bpf/bpf_tracing.h"

#define MAX_MAP_ENTRIES 1024u
#define PERF_MAX_STACK_DEPTH 127u

typedef struct proc_trace_key
{
    u32 tgid;
    u32 pid;
} ptrace_key_t;

typedef struct proc_sched_trace_val
{
    u64 start_time;
    u64 user_stack_id;
    u64 kernel_stack_id;
    char comm[TASK_COMM_LEN];
} psched_trace_val_t;

typedef struct proc_sched_stack_key
{
    ptrace_key_t pid_tgid;
    u64 user_stack_id;
    u64 kernel_stack_id;
    char comm[TASK_COMM_LEN];
} psched_stack_key_t;

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_MAP_ENTRIES);
    __type(key, ptrace_key_t);
    __type(value, psched_trace_val_t);
} ptrace_sched_trace_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_STACK_TRACE);
    __uint(max_entries, MAX_MAP_ENTRIES);
    __type(key, u32);
    __type(value, PERF_MAX_STACK_DEPTH * sizeof(unsigned long));
} stack_trace_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_MAP_ENTRIES);
    __type(key, psched_stack_key_t);
    __type(value, u64);
} stack_count_map SEC(".maps");

static volatile u32 target_tgid = 0;

inline void try_record_start(void *ctx, u32 prev_pid, u32 prev_tgid)
{
    if (prev_pid == 0 || prev_tgid == 0 || prev_tgid != target_tgid)
        return;

    psched_trace_val_t val;
    val.start_time = bpf_ktime_get_ns();
    val.user_stack_id = bpf_get_stackid(ctx, &stack_trace_map, BPF_F_USER_STACK);
    val.kernel_stack_id = bpf_get_stackid(ctx, &stack_trace_map, 0);
    bpf_get_current_comm(val.comm, TASK_COMM_LEN);

    ptrace_key_t key = {.tgid = prev_tgid, .pid = prev_pid};
    bpf_map_update_elem(&ptrace_sched_trace_map, &key, &val, BPF_ANY);
}

void increment_us(u32 pid, u32 tgid, u64 off_time_us, psched_trace_val_t *val)
{
    psched_stack_key_t key =
        {
            .pid_tgid.pid = pid,
            .pid_tgid.tgid = tgid,
            .kernel_stack_id = val->kernel_stack_id,
            .user_stack_id = val->user_stack_id,
        };
    __builtin_memcpy(key.comm, val->comm, TASK_COMM_LEN);
    u64 *total_off_time_us = bpf_map_lookup_elem(&stack_count_map, &key);
    u64 result = off_time_us;
    if (total_off_time_us != NULL)
        result += *total_off_time_us;
    
    bpf_map_update_elem(&stack_count_map, &key, &result, BPF_ANY);
}

inline void try_record_end(u32 next_pid, u32 next_tgid)
{
    if (next_pid == 0 || next_tgid != target_tgid)
        return;

    ptrace_key_t key = {.tgid = next_tgid, .pid = next_pid};
    psched_trace_val_t *val = bpf_map_lookup_elem(&ptrace_sched_trace_map, &key);
    if (!val)
        return;

    u64 end_time = bpf_ktime_get_ns();
    u64 off_time = end_time - val->start_time;
    u64 off_time_us = off_time / 1000;
    increment_us(next_pid, next_tgid, off_time_us, val);
}

SEC(SEC_TP_BPF(sched_switch))
int BPF_PROG(sched_switch, struct task_struct *prev, struct task_struct *next)
{
    pid_t prev_pid = prev->pid;
    pid_t prev_tgid = prev->tgid;

    pid_t next_pid = next->pid;
    pid_t next_tgid = next->tgid;

    return 0;
}
