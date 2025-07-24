#ifndef SRC_COLLECTIONS_HASHMAP_H
#define SRC_COLLECTIONS_HASHMAP_H

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "macros.h"

/*
 * Very dumb hashmap with integer keys and fixed bucket size
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

#define HASHMAP_INITIALISER { .n_items = 0, .buckets = {0} }
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

/* Returns true if inserted, false if entry with the same key already exists */
#define HASHMAP_INSERT(map, key, entry) \
    ({ \
        struct hashmap_entry **b = HASHMAP_GET_BUCKET_FROM_KEY(map, key); \
        bool key_already_exists = false; \
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
                /* don't insert */ \
                key_already_exists = true; \
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
        key_already_exists == false; \
    })

/* Returns true if replacement happened, false otherwise. Replaced item will be put in old */
#define HASHMAP_INSERT_OR_REPLACE(map, key, entry, old, member) \
    ({ \
        struct hashmap_entry **b = HASHMAP_GET_BUCKET_FROM_KEY(map, key); \
        bool replaced = false; \
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
                /* entry with the same key was found. Replace it */ \
                old = CONTAINER_OF(e, old, member); \
                if (e->prev == NULL && e->next == NULL) { \
                    /* e is the only entry in a bucket */ \
                    *b = (entry); \
                    (entry)->prev = NULL; \
                    (entry)->next = NULL; \
                } else if (e->prev == NULL) { \
                    /* e is the first entry in a bucket */ \
                    *b = (entry); \
                    e->next->prev = (entry); \
                    (entry)->next = e->next; \
                    e->next = NULL; \
                } else if (e->next == NULL) { \
                    /* e is the last entry in a bucket */ \
                    e->prev->next = (entry); \
                    (entry)->prev = e->prev; \
                    e->prev = NULL; \
                } else { \
                    /* e is somewhere in the middle */ \
                    e->prev->next = (entry); \
                    e->next->prev = (entry); \
                    (entry)->prev = e->prev; \
                    (entry)->next = e->next; \
                    e->prev = e->next = NULL; \
                } \
                replaced = true; \
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
        replaced; \
    })

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
#define HASHMAP_EXISTS(map, key) \
    ({ \
        struct hashmap_entry **b = HASHMAP_GET_BUCKET_FROM_KEY(map, key); \
        struct hashmap_entry *e = HASHMAP_LOOKUP_ENTRY_IN_BUCKET(b, key); \
        e != NULL; \
    })

/* Returns true if entry was found, false if not */
#define HASHMAP_DELETE(map, key) \
    ({ \
        struct hashmap_entry **b = HASHMAP_GET_BUCKET_FROM_KEY(map, key); \
        struct hashmap_entry *e = HASHMAP_LOOKUP_ENTRY_IN_BUCKET(b, key); \
        if (e != NULL) { \
            if (e->prev == NULL && e->next == NULL) { \
                /* e is the only entry in a bucket */ \
                *b = NULL; \
            } else if (e->prev == NULL) { \
                /* e is the first entry in a bucket */ \
                *b = e->next; \
                e->next->prev = NULL; \
                e->next = NULL; \
            } else if (e->next == NULL) { \
                /* e is the last entry in a bucket */ \
                e->prev->next = NULL; \
                e->prev = NULL; \
            } else { \
                /* e is somewhere in the middle */ \
                e->prev->next = e->next; \
                e->next->prev = e->prev; \
                e->prev = e->next = NULL; \
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

#endif /* #ifndef SRC_COLLECTIONS_HASHMAP_H */

