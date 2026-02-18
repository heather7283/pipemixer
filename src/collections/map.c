#include "collections/map.h"
#include "xmalloc.h"

static uint32_t index(uint32_t key, uint32_t n_buckets) {
    const uint32_t p = __builtin_ctz(n_buckets);
    const uint32_t knuth = 2654435769;
    return (key * knuth) >> (sizeof(key) * 8 - p);
}

static void resize(struct map *map) {
    const uint32_t old_n_buckets = map->n_buckets;
    struct map_entry **old_buckets = map->buckets;

    map->n_buckets = map->n_buckets ? map->n_buckets * 2 : 32;
    map->buckets = calloc(map->n_buckets, sizeof(map->buckets[0]));

    for (uint32_t i = 0; i < old_n_buckets; i++) {
        struct map_entry *old_entry = old_buckets[i], *old_next;
        while (old_entry) {
            old_next = old_entry->next;

            struct map_entry **pentry;
            const uint32_t idx = index(old_entry->key, map->n_buckets);
            for (pentry = &map->buckets[idx]; *pentry; pentry = &(*pentry)->next);

            *pentry = old_entry;
            old_entry->next = NULL;

            old_entry = old_next;
        }
    }

    free(old_buckets);
}

void map_insert(struct map *map, uint32_t key, void *val) {
    if (!map->buckets || map->n_entries >= map->n_buckets) {
        resize(map);
    }

    struct map_entry **pentry;
    const uint32_t idx = index(key, map->n_buckets);
    for (pentry = &map->buckets[idx]; *pentry; pentry = &(*pentry)->next);

    *pentry = xmalloc(sizeof(**pentry));
    **pentry = (struct map_entry){
        .key = key,
        .val = val,
    };

    map->n_entries += 1;
}

void *map_get(struct map *map, uint32_t key) {
    if (!map->buckets) {
        return NULL;
    }

    const uint32_t idx = index(key, map->n_buckets);
    for (struct map_entry *entry = map->buckets[idx]; entry; entry = entry->next) {
        if (entry->key == key) {
            return entry->val;
        }
    }

    return NULL;
}

void *map_remove(struct map *map, uint32_t key) {
    if (!map->buckets) {
        return NULL;
    }

    const uint32_t idx = index(key, map->n_buckets);
    for (struct map_entry **pentry = &map->buckets[idx]; *pentry; pentry = &(*pentry)->next) {
        if ((*pentry)->key == key) {
            void *val = (*pentry)->val;
            struct map_entry *next = (*pentry)->next;

            free(*pentry);
            *pentry = next;

            map->n_entries -= 1;
            return val;
        }
    }

    return NULL;
}

void map_free(struct map *map) {
    for (uint32_t i = 0; i < map->n_buckets; i++) {
        struct map_entry *entry = map->buckets[i], *next;
        while (entry) {
            next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(map->buckets);
    *map = (struct map){0};
}

void *map_iter_next(struct map *map, struct map_iter_state *s) {
    switch (s->state) {
    case 0:
    s->state = 1;
    for (s->i = 0; s->i < map->n_buckets; s->i++) {
        for (s->entry = map->buckets[s->i]; s->entry; s->entry = s->entry->next) {
            return s->entry->val;
    case 1:;
        }
    }
    }

    return NULL;
}

