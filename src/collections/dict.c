#include <stdbool.h>
#include <string.h>

#include "collections/dict.h"
#include "xmalloc.h"

void dict_reserve(struct dict *dict, size_t cap) {
    if (dict->cap >= cap) {
        return;
    }

    dict->cap = dict->cap * 2 < cap ? cap : dict->cap * 2;
    dict->items = xreallocarray(dict->items, dict->cap, sizeof(dict->items[0]));
}

void dict_clear(struct dict *dict) {
    for (size_t i = 0; i < dict->size; i++) {
        struct dict_item *item = &dict->items[i];
        free(item->key);
        free(item->val);
    }

    dict->size = 0;
}

void dict_free(struct dict *dict) {
    for (size_t i = 0; i < dict->size; i++) {
        struct dict_item *item = &dict->items[i];
        free(item->key);
        free(item->val);
    }
    free(dict->items);

    *dict = (struct dict){0};
}

static bool search(const struct dict *dict, const char *key, size_t *index) {
    ssize_t l = 0, r = dict->size - 1;

    while (l <= r) {
        const ssize_t m = l + (r - l) / 2;
        const struct dict_item *item = &dict->items[m];

        const int cmp = strcmp(item->key, key);
        if (cmp < 0) {
            l = m + 1;
        } else if (cmp > 0) {
            r = m - 1;
        } else {
            if (index) {
                *index = m;
            }
            return true;
        }
    }

    if (index) {
        *index = l;
    }
    return false;
}

void dict_insert(struct dict *dict, const char *key, const char *val) {
    size_t index;
    const bool present = search(dict, key, &index);

    if (!present) {
        dict_reserve(dict, dict->size + 1);

        struct dict_item *pos = &dict->items[index];
        memmove(pos + 1, pos, sizeof(*pos) * (dict->size - index));
        dict->size += 1;

        pos->key = xstrdup(key);
        pos->val = xstrdup(val);
    } else {
        struct dict_item *item = &dict->items[index];
        if (strcmp(item->val, val)) {
            free(item->val);
            item->val = xstrdup(val);
        }
    }
}

const char *dict_get(const struct dict *dict, const char *key) {
    size_t index;
    if (search(dict, key, &index)) {
        return dict->items[index].val;
    }

    return NULL;
}

void dict_remove(struct dict *dict, const char *key) {
    size_t index;
    const bool present = search(dict, key, &index);
    struct dict_item *pos = &dict->items[index];

    if (!present) {
        return;
    }

    free(pos->key);
    free(pos->val);

    memmove(pos, pos + 1, sizeof(*pos) * (dict->size - index - 1));

    dict->size -= 1;
}

