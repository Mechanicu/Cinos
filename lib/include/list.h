// 实现linux内核链表
#ifndef ____LIST_H__
#define ____LIST_H__
#include <stdlib.h>

typedef struct list_head {
    struct list_head *next;
    struct list_head *prev;
} list_t;

#define LIST_HEAD_INIT(name) \
    {                        \
        &(name), &(name)     \
    }

#define offsetof(type, member) \
    (size_t)(&(((type *)0)->member))

#define container_of(ptr, type, member) \
    (type *)((char *)(ptr) - offsetof(type, member))

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_for_each_entry(pos, head, type, member)   \
    for (pos = list_entry((head)->next, type, member); \
         &pos->member != (head);                       \
         pos = list_entry(pos->member.next, type, member))

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *n, struct list_head *prev, struct list_head *next)
{
    next->prev = n;
    n->next    = next;
    n->prev    = prev;
    prev->next = n;
}

static inline void list_add(struct list_head *n, struct list_head *head)
{
    __list_add(n, head, head->next);
}

static inline void list_add_tail(struct list_head *n, struct list_head *head)
{
    __list_add(n, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void __list_del_entry(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}

static inline void list_del_init(struct list_head *entry)
{
    __list_del_entry(entry);
    INIT_LIST_HEAD(entry);
}

static inline void list_init(list_t *list)
{
    list->next = list;
    list->prev = list;
}

static inline void list_insert_after(list_t *prev, list_t *node)
{
    node->prev       = prev;
    node->next       = prev->next;
    prev->next->prev = node;
    prev->next       = node;
}

static inline void list_remove(list_t *pos)
{
    pos->prev->next = pos->next;
    pos->next->prev = pos->prev;
}

#endif