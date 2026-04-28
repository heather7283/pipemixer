#pragma once

#include <stddef.h>
#include <stdint.h>

struct dict_item {
    size_t key_len, val_len;
    char *key, *val;
};

struct dict {
    size_t size, cap;
    struct dict_item *items;
};

void dict_reserve(struct dict *dict, size_t size);
void dict_clear(struct dict *dict);
void dict_free(struct dict *dict);

void dict_insert(struct dict *dict, const char *key, const char *val);
const char *dict_get(const struct dict *dict, const char *key);
void dict_remove(struct dict *dict, const char *key);

