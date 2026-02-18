#pragma once

#include <stdint.h>

struct map_entry {
    uint32_t key;
    void *val;
    struct map_entry *next;
};

struct map {
    struct map_entry **buckets;
    uint32_t n_buckets, n_entries;
};

void map_insert(struct map *map, uint32_t key, void *val);
void *map_get(struct map *map, uint32_t key);
void *map_remove(struct map *map, uint32_t key);
void map_free(struct map *map);

struct map_iter_state {
    unsigned state;
    uint32_t i;
    struct map_entry *entry;
};

void *map_iter_next(struct map *map, struct map_iter_state *s);

#define MAP_FOREACH(pmap, ppvar) \
    for (struct map_iter_state _iter = {0}; (*(ppvar) = map_iter_next((pmap), &_iter));)

