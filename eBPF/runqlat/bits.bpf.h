#ifndef _BITS_BPF_H_
#define _BITS_BPF_H_
#include "../vmlinux.h"
#include <bpf/bpf_helpers.h>

#define READ_ONCE(x) (*(volatile typedef(x) *)&(x))
#define WRITE_ONCE(x, val) ((*(volatile typedef(x) *)&(x)) = val)

/*the funtion is for calculating maximum int no more than log_2^v*/
static __always_inline u64 log2(u32 v)
{
    u32 shift;
    u32 r;
    /*if v more than 0xffff, then use high 16b, otherwise use the low*/
    r = (v > 0xffff) << 4; v >>= r;
    /*like the first*/
    shift = (v > 0xFF) << 3; v >>= shift; r |= shift;
	shift = (v > 0xF) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);
    return r;
}

static __always_inline u64 log2l(u64 v)
{
	u32 hi = v >> 32;

	if (hi)
		return log2(hi) + 32;
	else
		return log2(v);
}

#endif