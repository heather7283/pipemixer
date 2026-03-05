#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "collections/vec.h"
#include "xmalloc.h"

static void vec_ensure_capacity(struct vec_generic *vec, size_t elem_size, size_t cap) {
    if (cap > vec->cap) {
        size_t new_cap = vec->cap * 2;
        if (new_cap < cap) {
            new_cap = cap;
        }

        vec->cap = new_cap;
        vec->data = xreallocarray(vec->data, vec->cap, elem_size);
    }
}

void *vec_append_generic(struct vec_generic *vec, size_t elem_size) {
    vec_ensure_capacity(vec, elem_size, vec->size + 1);
    return (uint8_t *)vec->data + ((vec->size++) * elem_size);
}

void vec_clear_generic(struct vec_generic *vec) {
    vec->size = 0;
}

void vec_free_generic(struct vec_generic *vec) {
    free(vec->data);
    *vec = (struct vec_generic){0};
}

void vec_reserve_generic(struct vec_generic *vec, size_t elem_size, size_t elem_count) {
    vec_ensure_capacity(vec, elem_size, elem_count);
}

void vec_exchange_generic(struct vec_generic *v1, struct vec_generic *v2) {
    const struct vec_generic tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
}

