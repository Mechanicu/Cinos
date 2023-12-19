#ifndef _MAPS_BPF_H_
#define _MAPS_BPF_H_
#include <bpf/bpf_helpers.h>

#define EEXIST 17

static __always_inline void *
bpf_map_lookup_or_try_init(void *map, const void *key, const void *init)
{
    void *val;
    long err;

    val = bpf_map_lookup_elem(map, key);
    if (val)
        return val;

    err = bpf_map_update_elem(map, key, init, BPF_NOEXIST);
    /*other threads may init after current lookup*/
    if (err && err != -EEXIST)
        return 0;
    /*return initialized address*/
    return bpf_map_lookup_elem(map, key);
}

#endif