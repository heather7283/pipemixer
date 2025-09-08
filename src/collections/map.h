#ifndef SRC_COLLECTIONS_MAP_H
#define SRC_COLLECTIONS_MAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct map_entry_generic {
    uint64_t key;
    struct map_entry_generic *next;
    char data[];
};

struct map_generic {
    uint64_t n_entries, n_buckets;
    struct map_entry_generic **buckets;
};

#define MAP(type) \
    struct { \
        size_t n_entries, n_buckets; \
        struct { \
            uint64_t key; \
            struct map_entry_generic *next; \
            type data; \
        } **buckets; \
    }

#define MAP_SIZE(pmap) ((pmap)->n_entries)

void *map_insert_or_replace_generic(struct map_generic *map, uint64_t key,
                                        const void *item, size_t item_size, bool zero_init);

#define MAP_INSERT(pmap, key, pitem) \
    map_insert_or_replace_generic((struct map_generic *)(pmap), (key), (pitem), \
                                      sizeof(**(pmap)->buckets) - \
                                          sizeof(struct map_entry_generic), \
                                      false)

#define MAP_EMPLACE(pmap, key) \
    ((__typeof__(&(*(pmap)->buckets)->data)) \
    map_insert_or_replace_generic((struct map_generic *)(pmap), (key), NULL, \
                                      sizeof(**(pmap)->buckets) - \
                                          sizeof(struct map_entry_generic), \
                                      false))

#define MAP_EMPLACE_ZEROED(pmap, key) \
    ((__typeof__(&(*(pmap)->buckets)->data)) \
    map_insert_or_replace_generic((struct map_generic *)(pmap), (key), NULL, \
                                      sizeof(**(pmap)->buckets) - \
                                          sizeof(struct map_entry_generic), \
                                      true))

void *map_get_generic(struct map_generic *map, uint64_t key);

#define MAP_GET(pmap, key) \
    ((__typeof__(&(*(pmap)->buckets)->data)) \
    (map_get_generic((struct map_generic *)(pmap), (key))))

#define MAP_EXISTS(pmap, key) (MAP_GET(pmap, key) != NULL)

void map_remove_generic(struct map_generic *map, uint64_t key);

#define MAP_REMOVE(pmap, key) \
    map_remove_generic((struct map_generic *)(pmap), (key))

void map_free_generic(struct map_generic *map);

#define MAP_FREE(pmap) \
    map_free_generic((struct map_generic *)(pmap))

struct map_iter_state {
    uint64_t bucket;
    struct map_entry_generic *entry;
    struct map_entry_generic *next;
};

struct map_iter_state map_iter_init(const struct map_generic *map, void **itervar);
bool map_iter_is_valid(const struct map_generic *map, struct map_iter_state *state);
void map_iter_next(const struct map_generic *map, struct map_iter_state *state, void **itervar);

#define MAP_FOREACH(pmap, pvar) \
    for (struct map_iter_state iter_state = \
            map_iter_init((struct map_generic *)(pmap), (void **)(pvar)); \
         map_iter_is_valid((struct map_generic *)(pmap), &iter_state); \
         map_iter_next((struct map_generic *)(pmap), &iter_state, (void **)(pvar)))

#endif /* #ifndef SRC_COLLECTIONS_MAP_H */

