#include "../config.h"

#define TASK_COMM_LEN 16
#define MAX_LINE_SIZE 256

/* Format of u[ret]probe section definition supporting auto-attach:
 * u[ret]probe/binary:function[+offset]
 *
 * binary can be an absolute/relative path or a filename; the latter is resolved to a
 * full binary path via bpf_program__attach_uprobe_opts.
 *
 * Specifying uprobe+ ensures we carry out strict matching; either "uprobe" must be
 * specified (and auto-attach is not possible) or the above format is specified for
 * auto-attach.
 */
SEC(SEC_URETPROBE(/bin/bash, readline))
int BPF_KRETPROBE(printret, const void *ret)
{
    char str[MAX_LINE_SIZE] ={0};
    char comm[TASK_COMM_LEN] = {0};
    u32 pid = 0;
    if (!ret)
        return 0;
    
    bpf_get_current_comm(comm, TASK_COMM_LEN);
    pid = (u32)bpf_get_current_pid_tgid();

    bpf_probe_read_user_str(str, MAX_LINE_SIZE, ret);
    bpf_printk("Pid:%d (%s) read:%s", pid, comm, str);
    return 0;
}