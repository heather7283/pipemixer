#ifndef SRC_COLLECTIONS_H
#define SRC_COLLECTIONS_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Below is the most cursed code I've ever written, but it was super fun lol */

#define CONTAINER_OF(member_ptr, container_ptr, member_name) \
    ((__typeof__(container_ptr))((char *)(member_ptr) - \
     offsetof(__typeof__(*container_ptr), member_name)))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

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

#define LIST_INITALISER(head) { .next = head, .prev = head }
#define LIST_INIT(head) head->next = head->prev = head

#define LIST_IS_EMPTY(head) (head->next == head)

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

#define LIST_FOR_EACH(var, head, member) \
    for (var = CONTAINER_OF((head)->next, var, member); \
         &var->member != (head); \
         var = CONTAINER_OF(var->member.next, var, member))

#define LIST_FOR_EACH_REVERSE(var, head, member) \
    for (var = CONTAINER_OF((head)->prev, var, member); \
         &var->member != (head); \
         var = CONTAINER_OF(var->member.prev, var, member))

#define LIST_FOR_EACH_SAFE(var, tmp, head, member) \
    for (var = CONTAINER_OF((head)->next, var, member), \
         tmp = CONTAINER_OF((var)->member.next, tmp, member); \
         &var->member != (head); \
         var = tmp, \
         tmp = CONTAINER_OF(var->member.next, tmp, member))

/*
 * Hashmap.
 */

struct hashmap_entry {
    uint64_t _key;
    /* TODO: just next is probably enough */
    struct hashmap_entry *next, *prev;
};

struct hashmap {
    size_t n_items;
    struct hashmap_entry *buckets[];
};

#define HASHMAP_HEAD(bucket_count) \
    struct { \
        static_assert(bucket_count > 0 && ((bucket_count) & ((bucket_count) - 1)) == 0, \
                      "Bucket count must be a power of 2"); \
        size_t n_items; \
        struct hashmap_entry *buckets[bucket_count]; \
    }
#define HASHMAP_ENTRY struct hashmap_entry

static_assert((sizeof(struct hashmap) + sizeof(struct hashmap_entry *)) == sizeof(HASHMAP_HEAD(1)),
              "Definition mismatch between struct hashmap and HASHMAP macro");
static_assert(offsetof(struct hashmap, buckets) == offsetof(__typeof__(HASHMAP_HEAD(1)), buckets),
              "Definition mismatch between struct hashmap and HASHMAP macro");

#define HASHMAP_INITIALISER(hashmap) {0}
#define HASHMAP_INIT(hashmap) \
    do { \
        for (size_t i = 0; i < ARRAY_SIZE((hashmap)->buckets); i++) { \
            (hashmap)->buckets[i] = NULL; \
        } \
    }

#define HASHMAP_SIZE(hashmap) ((hashmap)->n_items)

#define HASHMAP_GET_BUCKET_FROM_KEY(map, key) \
    ({ \
        size_t index = ((uint64_t)(key) * 0x9E3779B97F4A7C15ULL) >> \
                        (64 - __builtin_ctzll(ARRAY_SIZE((map)->buckets))); \
        &(map)->buckets[index]; \
    })

#define HASHMAP_LOOKUP_ENTRY_IN_BUCKET(bucket, key) \
    ({ \
        bool found = false; \
        struct hashmap_entry *e = *(bucket); \
        while (e != NULL) { \
            if (e->_key == (key)) { \
                found = true; \
                break; \
            } \
            e = e->next; \
        } \
        found ? e : NULL; \
    })

#define HASHMAP_INSERT(map, key, entry) \
    do { \
        struct hashmap_entry **b = HASHMAP_GET_BUCKET_FROM_KEY(map, key); \
        if (*b == NULL) { \
            /* bucket is empty - just insert the entry */ \
            *b = (entry); \
            (entry)->prev = NULL; \
            (entry)->next = NULL; \
            (map)->n_items += 1; \
        } else { \
            /* bucket is not empty - check if entry with the same key already exists */ \
            struct hashmap_entry *e = HASHMAP_LOOKUP_ENTRY_IN_BUCKET(b, key); \
            if (e != NULL) { \
                /* TODO: handle already existing entries */ \
            } else { \
                /* no entry with the same key was found, so insert a new one at the beginning */ \
                e = *b; /* e IS THE FIRST ENTRY IN A BUCKET */ \
                e->prev = (entry); \
                (entry)->next = e; \
                (entry)->prev = NULL; \
                *b = (entry); /* replace first entry with new entry */ \
                (map)->n_items += 1; \
            } \
        } \
        (entry)->_key = key; \
    } while (0)

/* Returns true if entry was found, false if not */
#define HASHMAP_GET(var, map, key, member) \
    ({ \
        struct hashmap_entry **b = HASHMAP_GET_BUCKET_FROM_KEY(map, key); \
        struct hashmap_entry *e = HASHMAP_LOOKUP_ENTRY_IN_BUCKET(b, key); \
        if (e != NULL) { \
            (var) = CONTAINER_OF(e, (var), member); \
        } else { \
            (var) = NULL; \
        } \
        e != NULL; \
    })

/* Returns true if entry was found, false if not */
#define HASHMAP_DELETE(map, key) \
    ({ \
        struct hashmap_entry **b = HASHMAP_GET_BUCKET_FROM_KEY(map, key); \
        struct hashmap_entry *e = HASHMAP_LOOKUP_ENTRY_IN_BUCKET(b, key); \
        if (e != NULL) { \
            if (e->prev == NULL && e->next == NULL) { \
                *b = NULL; \
                e->prev = NULL; \
                e->next = NULL; \
            } else if (e->prev == NULL) { \
                *b = e->next; \
                e->next->prev = NULL; \
                e->next = NULL; \
            } else /* if (e->next == NULL) */ { \
                e->prev->next = NULL; \
                e->prev = NULL; \
            } \
            (map)->n_items -= 1; \
        } \
        e != NULL; \
    })

/* Works even if current entry is freed/modified mid-iteration */
#define HASHMAP_FOR_EACH(var, map, member) \
    for ( \
        struct { \
            struct hashmap_entry **b, *e, *next; \
        } iter = { .b = (map)->buckets, .e = (map)->buckets[0] }; \
        \
        ({ \
            bool keep_going = true; \
            while (iter.e == NULL) { \
                if ((++iter.b - (map)->buckets) >= (ptrdiff_t)ARRAY_SIZE((map)->buckets)) { \
                    keep_going = false; \
                    break; \
                } \
                iter.e = *iter.b; \
            } \
            if (keep_going) { \
                iter.next = iter.e->next; /* to not shit itself if entry gets freed */ \
                (var) = CONTAINER_OF(iter.e, (var), member); \
            } \
            keep_going; \
        }); \
        \
        ({ iter.e = iter.next; }) \
    )

#endif /* #ifndef SRC_COLLECTIONS_H */

