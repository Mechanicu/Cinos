#include "../config.h"
#include "vmlinux.h"
#include "exitsnoop.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 8 << 10);
} ringbuffer_map SEC(".maps");
SEC(SEC_SCHED_TRACE(sched_process_exit))
int handle_exit(struct trace_event_raw_sched_process_template *ctx)
{
    struct task_struct *task;
    kevent_t *event;
    pid_t tid;
    pid_t pid;
    u64 tgid_pid;
    u64 ts;
    u64 *start_ts;
    u64 start_time;

    tgid_pid = bpf_get_current_pid_tgid();
    tid = (u32)tgid_pid;
    pid = (u32)(tgid_pid >> 32);

    if (pid != tid)
    {
        return 0;
    }

    event = bpf_ringbuf_reserve(&ringbuffer_map, sizeof(*event), 0);
    if (!event)
    {
        return 0;
    }

    task = (struct task_struct *)bpf_get_current_task();
    start_time = BPF_CORE_READ(task, start_time);

    event->duration_ns = bpf_ktime_get_ns() - start_time;
    event->pid = pid;
    event->ppid = BPF_CORE_READ(task, real_parent, tgid);
    event->exit_code = (BPF_CORE_READ(task, exit_code) >> 8) & 0xff;
    bpf_get_current_comm(event->comm, TASK_COMM_LEN);

    bpf_ringbuf_submit(event, 0);
    return 0;
}