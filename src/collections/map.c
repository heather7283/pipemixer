#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "map.h"

/* make sure this is 2^n where n > 0 */
#define INITIAL_BUCKET_COUNT 32
/* 0.75 */
#define REHASH_LOAD_THRESHOLD 75

__attribute__((unused))
static void print_info(struct map_generic *map) {
    uint64_t occupied_buckets = 0;
    int longest_chain = 0;
    for (uint64_t i = 0; i < map->n_buckets; i++) {
        struct map_entry_generic **const bucket = &map->buckets[i];
        struct map_entry_generic *entry = *bucket;
        if (entry != NULL) {
            occupied_buckets += 1;
        }
        int chain = 0;
        while (entry != NULL) {
            chain += 1;
            entry = entry->next;
        }
        if (chain > longest_chain) {
            longest_chain = chain;
        }
    }

    fprintf(stderr,
            "MAP %lu buckets (%lu occupied, %.2f%%) %lu items (%.2f%% load) longest chain %d\n",
            map->n_buckets, occupied_buckets, (double)occupied_buckets / map->n_buckets * 100,
            map->n_entries, (double)map->n_entries / map->n_buckets * 100, longest_chain);
}

static inline uint64_t get_index(uint64_t key, uint64_t n_buckets) {
    return key & (n_buckets - 1);
}

static void map_init_generic(struct map_generic *map) {
    map->n_buckets = INITIAL_BUCKET_COUNT;
    map->buckets = calloc(map->n_buckets, sizeof(*map->buckets));
    map->n_entries = 0;
}

static void map_rehash_generic(struct map_generic *map, size_t item_size) {
    const uint64_t old_n_buckets = map->n_buckets;
    struct map_entry_generic **const old_buckets = map->buckets;

    map->n_buckets *= 2;
    map->buckets = calloc(map->n_buckets, sizeof(*map->buckets));

    for (uint64_t i = 0; i < old_n_buckets; i++) {
        struct map_entry_generic **const old_bucket = &old_buckets[i];
        if (*old_bucket == NULL) {
            continue;
        }

        struct map_entry_generic *old_entry = *old_bucket;
        while (old_entry != NULL) {
            struct map_entry_generic *const next_old_entry = old_entry->next;

            const uint64_t new_index = get_index(old_entry->key, map->n_buckets);
            struct map_entry_generic **const new_bucket = &map->buckets[new_index];
            struct map_entry_generic *new_entry;

            if (*new_bucket == NULL) {
                *new_bucket = malloc(sizeof(struct map_entry_generic) + item_size);
                new_entry = *new_bucket;
            } else {
                struct map_entry_generic *last;
                for (last = *new_bucket; last->next != NULL; last = last->next);
                last->next = malloc(sizeof(struct map_entry_generic) + item_size);
                new_entry = last->next;
            }
            new_entry->next = NULL;
            new_entry->key = old_entry->key;
            memcpy(&new_entry->data, &old_entry->data, item_size);

            free(old_entry);

            old_entry = next_old_entry;
        }
    }

    free(old_buckets);
}

void *map_insert_or_replace_generic(struct map_generic *map, uint64_t key,
                                        const void *item, size_t item_size, bool zero_init) {
    if (map->n_buckets == 0) {
        map_init_generic(map);
    } else if ((map->n_entries * 100 / map->n_buckets) >= REHASH_LOAD_THRESHOLD) {
        map_rehash_generic(map, item_size);
    }

    const uint64_t index = get_index(key, map->n_buckets);
    struct map_entry_generic **const bucket = &map->buckets[index];
    struct map_entry_generic *new_entry;
    if (*bucket == NULL) {
        /* empty bucket */
        *bucket = malloc(sizeof(struct map_entry_generic) + item_size);
        new_entry = *bucket;
        new_entry->next = NULL;

        map->n_entries += 1;
    } else {
        /* walk the linked list */
        bool found = false;
        struct map_entry_generic *entry = *bucket;
        struct map_entry_generic *prev = NULL;
        while (entry != NULL) {
            if (entry->key == key) {
                found = true;
                break;
            }
            prev = entry;
            entry = entry->next;
        }
        if (found) {
            /* replace existing entry */
            new_entry = entry;
        } else {
            /* insert at the end of linked list */
            prev->next = malloc(sizeof(struct map_entry_generic) + item_size);
            new_entry = prev->next;
            new_entry->next = NULL;

            map->n_entries += 1;
        }
    }
    new_entry->key = key;

    if (item != NULL) {
        memcpy(&new_entry->data, item, item_size);
    } else if (zero_init) {
        memset(&new_entry->data, '\0', item_size);
    }

    return (void *)&new_entry->data;
}

void *map_get_generic(struct map_generic *map, uint64_t key) {
    if (map->n_entries == 0) {
        return NULL;
    }

    const uint64_t index = get_index(key, map->n_buckets);
    struct map_entry_generic **const bucket = &map->buckets[index];
    if (*bucket == NULL) {
        return NULL;
    }

    bool found = false;
    struct map_entry_generic *entry = *bucket;
    while (entry != NULL) {
        if (entry->key == key) {
            found = true;
            break;
        }

        entry = entry->next;
    }
    if (!found) {
        return NULL;
    }

    return (void *)&entry->data;
}

void map_remove_generic(struct map_generic *map, uint64_t key) {
    if (map->n_entries == 0) {
        return;
    }

    const uint64_t index = get_index(key, map->n_buckets);
    struct map_entry_generic **const bucket = &map->buckets[index];
    if (*bucket == NULL) {
        return;
    }

    bool found = false;
    struct map_entry_generic *entry = *bucket;
    struct map_entry_generic *prev = NULL;
    while (entry != NULL) {
        if (entry->key == key) {
            found = true;
            break;
        }

        prev = entry;
        entry = entry->next;
    }
    if (!found) {
        return;
    }

    if (prev == NULL) {
        *bucket = entry->next;
    } else {
        prev->next = entry->next;
    }
    free(entry);

    map->n_entries -= 1;
}

void map_free_generic(struct map_generic *map) {
    for (uint64_t i = 0; i < map->n_buckets; i++) {
        struct map_entry_generic **const bucket = &map->buckets[i];
        struct map_entry_generic *entry = *bucket;
        while (entry != NULL) {
            struct map_entry_generic *const next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(map->buckets);
    map->buckets = NULL;
    map->n_buckets = 0;
    map->n_entries = 0;
}

struct map_iter_state map_iter_init(const struct map_generic *map, void **pitervar) {
    struct map_iter_state state = {0};

    while (state.bucket < map->n_buckets) {
        if (map->buckets[state.bucket] != NULL) {
            break;
        }
    }

    if (state.bucket < map->n_buckets) {
        state.entry = map->buckets[state.bucket];
        state.next = state.entry->next;

        *pitervar = &state.entry->data;

    }

    return state;
}

bool map_iter_is_valid(const struct map_generic *map, struct map_iter_state *state) {
    return state->bucket < map->n_buckets;
}

void map_iter_next(const struct map_generic *map,
                       struct map_iter_state *state, void **itervar) {
    if (state->next != NULL) {
        state->entry = state->next;
        state->next = state->entry->next;

        *itervar = &state->entry->data;
    } else {
        while (++state->bucket < map->n_buckets) {
            if (map->buckets[state->bucket] != NULL) {
                state->entry = map->buckets[state->bucket];
                state->next = state->entry->next;

                *itervar = &state->entry->data;

                break;
            }
        }
    }
}

