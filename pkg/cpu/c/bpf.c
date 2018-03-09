#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

#define MINBLOCK_US {{ .MinBlockUS }}
#define MAXBLCOK_US {{ .MaxBlockUS }}

struct proc_key_t {
    u32 pid;
    u32 tgid;
    int user_stack_id;
    int kernel_stack_id;
    char name[{{ .TaskCommLen }}];
};

BPF_HASH(counts, struct proc_key_t);
BPF_HASH(start, u32);
BPF_STACK_TRACE(stack_traces, {{ .StackStorageSize }});

int oncpu(struct pt_regs *ctx, struct task_struct *prev) {
    u32 pid = prev->pid;
    u32 tgid = prev->tgid;
    u64 ts, *tsp;
    // record previous thread sleep time
    if (({{ .ThreadFilter }}) && ({{ .StateFilter }})) {
        ts = bpf_ktime_get_ns();
        start.update(&pid, &ts);								    
    }

    // get the current thread's start time
    pid = bpf_get_current_pid_tgid();
    tgid = bpf_get_current_pid_tgid() >> 32;
    tsp = start.lookup(&pid);
    if (tsp == 0) {
        return 0;        // missed start or filtered					    
    }

    // calculate current thread's delta time
    u64 delta = bpf_ktime_get_ns() - *tsp;
    start.delete(&pid);
    delta = delta / 1000;
    if (((int)(delta) < {{ .MinBlockUS }}) || ((int)(delta) > {{ .MaxBlockUS }})) {
        return 0;					    
    }

    // create map key
    u64 zero = 0, *val;
    struct proc_key_t key = {};

    key.pid = pid;
    key.tgid = tgid;
    key.user_stack_id = {{ .UserStackGet }};
    key.kernel_stack_id = {{ .KernelStackGet }};
    bpf_get_current_comm(&key.name, sizeof(key.name));

    val = counts.lookup_or_init(&key, &zero);
    (*val) += delta;
    return 0;
}