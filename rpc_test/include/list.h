// 实现linux内核链表
#ifndef ____LIST_H__
#define ____LIST_H__

typedef struct list
{
    struct list *next;
    struct list *prev;
} list_t;

static inline void list_init(list_t *list)
{
    list->next = list;
    list->prev = list;
}

static inline void list_insert_after(list_t *prev, list_t *node)
{
    node->prev = prev;
    node->next = prev->next;
    prev->next->prev = node;
    prev->next = node;
}

static inline void list_remove(list_t *pos)
{
    pos->prev->next = pos->next;
    pos->next->prev = pos->prev;
}

#endif