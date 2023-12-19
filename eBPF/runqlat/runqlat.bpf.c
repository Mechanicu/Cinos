#include "../vmlinux.h"
#include "../config.h"
#include "core_fix.bpf.h"
#include "maps.bpf.h"
#include "runqlat.bpf.h"
#include "bits.bpf.h"
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

#define MAX_ENTRIES 10240
#define TASK_RUNNING 0

const volatile char filter_cg = 0;
const volatile char targ_per_process = 0;
const volatile char targ_per_thread = 0;
const volatile char targ_per_pidns = 0;
const volatile char targ_ms = 0;
const volatile pid_t targ_pid = 0;

struct bpf_map_def
{
    __uint(type, BPF_MAP_TYPE_CGROUP_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, u32);
} cgroup_map SEC(".map");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, u32);
    __type(value, u64);
} start SEC(".map");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, u32);
    __type(value, sizeof(hist_t));
} hists SEC(".map");

static int trace_enque(u32 pid, u32 tid)
{
    u64 ns;
    if (!tid || (targ_pid && targ_pid != pid))
        return 0;
    ns = bpf_ktime_get_ns();
    return bpf_map_update_elem(&start, &tid, &ns, BPF_ANY);
}

static u32 pid_namespace(struct task_struct *task)
{
    struct pid *pid;
    struct upid upid;
    u32 level;
    u32 inum;

    pid = BPF_CORE_READ(task, thread_pid);
    level = BPF_CORE_READ(pid, level);
    bpf_core_read(&upid, sizeof(upid), &(pid->numbers[level]));
    inum = BPF_CORE_READ(upid.ns, ns.inum);

    return inum;
}

static int handle_switch(u8 preempt, struct task_struct *prev, struct task_struct *next)
{
    /*filter task belonging to the cgroup*/
    if (filter_cg && !bpf_current_task_under_cgroup(&cgroup_map, 0))
        return 0;
    /*record current running thread starting waiting time*/
    /*
    ATTENTION:
    WILL PREV AND NEXT BE THE SAME ONE?
    */
    if (get_task_state(prev) == TASK_RUNNING)
        trace_enque(BPF_CORE_READ(prev, tgid), BPF_CORE_READ(prev, pid));

    /*if record next running thread starting waiting time, calculate delay time*/
    u32 tid = BPF_CORE_READ(next, pid);
    if (BPF_CORE_READ(prev, pid) == tid)
        bpf_printk("BPF prev same to next\n");

    u64 *timestamp = bpf_map_lookup_elem(&start, &tid);
    if (!timestamp)
        return 0;
    s64 delaytime = bpf_ktime_get_ns() - *timestamp;
    /*if next task delayed too much, then just return*/
    if (delaytime < 0)
        goto clean;

    /*record delaytime*/
    u32 hkey;
    if (targ_per_process)
        hkey = BPF_CORE_READ(next, tgid);
    else if (targ_per_thread)
        hkey = BPF_CORE_READ(next, pid);
    else if (targ_per_pidns)
        hkey = pid_namespace(next);
    else
        hkey = -1;

    hist_t dummy = {0};
    hist_t *hist_p = bpf_map_lookup_or_try_init(&hists, &hkey, &dummy);
    if (!hist_p)
        return 0;
    if (!hist_p->comm[0])
        bpf_probe_read_kernel_str(hist_p->comm, TASK_COMM_LEN, next->comm);

    delaytime /= 1000u;
    if (targ_ms)
        delaytime /= 1000u;
    u8 power = log2l(delaytime);
    if (power >= MAX_TIME_POWER)
        power = MAX_TIME_POWER;

    __sync_fetch_and_add(&(hist_p->slots[power]), 1);
clean:
    bpf_map_delete_elem(&start, &tid);
    return 0;
}
