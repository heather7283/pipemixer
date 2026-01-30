#pragma once

#include <stddef.h>
#include <stdbool.h>

struct list {
    struct list *next, *prev;
};

// this function returns `struct list` instead of `struct list *`
// to allow for inline initialisation. Both cases below are valid:
//
// struct foo foo = (struct foo){ .list = list_init(&foo.list) };
//
// struct foo foo;
// list_init(&foo.list);
static inline struct list list_init(struct list *list) {
    list->next = list->prev = list;
    return *list;
};

static inline bool list_is_empty(const struct list *list) {
    return list->next == list;
}

static inline struct list *list_insert_after(struct list *old, struct list *new) {
    new->next = old->next;
    new->prev = old;
    old->next->prev = new;
    old->next = new;
    return new;
}

static inline struct list *list_insert_before(struct list *old, struct list *new) {
    new->prev = old->prev;
    new->next = old;
    old->prev->next = new;
    old->prev = new;
    return new;
}

static inline struct list *list_remove(struct list *elem) {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    elem->prev = elem->next = elem;
    return elem;
}

#define LIST_FOREACH(iter, head) \
    for (struct list *iter = (head)->next, *_iter_next = (head)->next->next; \
         iter != (head); \
         iter = _iter_next, _iter_next = iter->next)

#define LIST_FOREACH_REVERSE(iter, head) \
    for (struct list *iter = (head)->prev, *_iter_prev = (head)->prev->prev; \
         iter != (head); \
         iter = _iter_prev, _iter_prev = iter->prev)

