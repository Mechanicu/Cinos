// 实现C的原子变量及其操作
#ifndef ____C_ATOMIC_H__
#define ____C_ATOMIC_H__
#include <pthread.h>
#include <unistd.h>

typedef struct atomic
{
    /* data */
    volatile int value;
} atomic_t;

static inline void atomic_set(atomic_t *var, int val)
{
    int old;
    do
    {
        old = var->value;
    } while (!__sync_bool_compare_and_swap(&(var->value), old, val));
}

static inline int atomic_get(atomic_t *var)
{
    return var->value;
}

static inline void atomic_add(atomic_t *var, int val)
{
    atomic_set(var, var->value + val);
}

static inline void atomic_sub(atomic_t *var, int val)
{
    atomic_set(var, var->value - val);
}

static inline void atomic_init(atomic_t *var)
{
    atomic_set(var, 0);
}

static inline int atomic_is_null(atomic_t *var)
{
    if (var->value == 0)
        return 1;
    return 0;
}
#endif