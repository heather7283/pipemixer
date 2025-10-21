#pragma once

#include "macros.h"

/*
 * Linked list.
 * In the head, next points to the first list elem, prev points to the last.
 * In the list element, next points to the next elem, prev points to the previous elem.
 * In the last element, next points to the head. In the first element, prev points to the head.
 * If the list is empty, next and prev point to the head itself.
 */

struct list {
    struct list *next;
    struct list *prev;
};

#define LIST_HEAD struct list
#define LIST_ENTRY struct list

#define LIST_INITIALISER(head) { .next = head, .prev = head }
#define LIST_INIT(head) ((head)->next = (head)->prev = (head))

#define LIST_IS_EMPTY(head) ((head)->next == (head) && (head)->prev == (head))
#define LIST_IS_FIRST(head, elem) ((head)->next == (elem))
#define LIST_IS_LAST(head, elem) ((head)->prev == (elem))

#define LIST_FIRST(head) ((head)->next)
#define LIST_LAST(head) ((head)->prev)

#define LIST_GET(var, elem, member) ((var) = CONTAINER_OF(elem, var, member))
#define LIST_GET_FIRST(var, head, member) ((var) = CONTAINER_OF(LIST_FIRST(head), var, member))
#define LIST_GET_LAST(var, head, member) ((var) = CONTAINER_OF(LIST_LAST(head), var, member))

#define LIST_PREV(elem) ((elem)->prev)
#define LIST_NEXT(elem) ((elem)->next)

/* Inserts new after elem. */
#define LIST_INSERT(elem, new) \
    do { \
        (elem)->next->prev = (new); \
        (new)->next = (elem)->next; \
        (elem)->next = (new); \
        (new)->prev = (elem); \
    } while (0)

#define LIST_REMOVE(elem) \
    do { \
        (elem)->prev->next = (elem)->next; \
        (elem)->next->prev = (elem)->prev; \
    } while (0)

#define LIST_POP(var, elem, member) \
    do { \
        (var) = CONTAINER_OF(elem, var, member); \
        (elem)->prev->next = (elem)->next; \
        (elem)->next->prev = (elem)->prev; \
    } while (0)

#define LIST_FOR_EACH_AFTER_INTERNAL(var, head, elem, member, direction) \
    for ( \
        struct { struct list *cur, *direction; } iter = { \
            .cur = (elem)->direction, .direction = (elem)->direction->direction \
        }; \
        \
        ({ \
            bool keep_going = true; \
            if (iter.cur == (head)) { \
                keep_going = false; \
            } else { \
                (var) = CONTAINER_OF(iter.cur, var, member); \
            } \
            keep_going; \
        }); \
        \
        iter.cur = iter.direction, \
        iter.direction = iter.direction->direction \
    )

#define LIST_FOR_EACH_AFTER(var, head, elem, member) \
    LIST_FOR_EACH_AFTER_INTERNAL(var, head, elem, member, next)

#define LIST_FOR_EACH(var, head, member) \
    LIST_FOR_EACH_AFTER(var, head, head, member)

#define LIST_FOR_EACH_REVERSE_BEFORE(var, head, elem, member) \
    LIST_FOR_EACH_AFTER_INTERNAL(var, head, elem, member, prev)

#define LIST_FOR_EACH_REVERSE(var, head, member) \
    LIST_FOR_EACH_REVERSE_BEFORE(var, head, head, member)

#define LIST_SWAP_HEADS(head1, head2) \
    do { \
        if ((head1) == (head2)) { \
            break; \
        } \
        \
        bool empty1 = LIST_IS_EMPTY(head1); \
        bool empty2 = LIST_IS_EMPTY(head2); \
        if (!empty1 && !empty2) { \
            struct list *h1_next = (head1)->next; \
            struct list *h1_prev = (head1)->prev; \
            struct list *h2_next = (head2)->next; \
            struct list *h2_prev = (head2)->prev; \
            \
            (head1)->next = h2_next; \
            (head1)->prev = h2_prev; \
            h2_next->prev = (head1); \
            h2_prev->next = (head1); \
            \
            (head2)->next = h1_next; \
            (head2)->prev = h1_prev; \
            h1_next->prev = (head2); \
            h1_prev->next = (head2); \
        } else if (empty1 && !empty2) { \
            (head1)->next = (head2)->next; \
            (head1)->prev = (head2)->prev; \
            (head1)->next->prev = (head1); \
            (head1)->prev->next = (head1); \
            LIST_INIT(head2); \
        } else if (!empty1 && empty2) { \
            (head2)->next = (head1)->next; \
            (head2)->prev = (head1)->prev; \
            (head2)->next->prev = (head2); \
            (head2)->prev->next = (head2); \
            LIST_INIT(head1); \
        } \
    } while (0)

