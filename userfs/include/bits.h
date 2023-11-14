#ifndef BITS_H
#define BITS_H

typedef char               s8;
typedef unsigned char      u8;
typedef short              s16;
typedef unsigned short     u16;
typedef int                s32;
typedef unsigned int       u32;
typedef long long          s64;
typedef unsigned long long u64;

static inline u32 ulog2(u32 v)
{
    u32 shift;
    u32 r;
    /*if v more than 0xffff, then use high 16b, otherwise use the low*/
    r       = (v > 0xffff) << 4;
    v     >>= r;
    /*like the first*/
    shift   = (v > 0xff) << 3;
    v     >>= shift;
    r      |= shift;

    shift   = (v > 0xf) << 2;
    v     >>= shift;
    r      |= shift;
    shift   = (v > 0x3) << 1;
    v     >>= shift;
    r      |= shift;
    r      |= (v >> 1);
    return r;
}

static inline u32 ulog2l(u64 v)
{
    u32 hi = v >> 32;
    if (hi) {
        return ulog2(hi) + 32;
    }
    return ulog2(v);
}

#endif