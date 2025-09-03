#include <stdio.h>
#include <string.h>

#include "collections/vec.h"
#include "macros.h"
#include "xmalloc.h"

#define GROWTH_FACTOR 1.5

static void vec_ensure_capacity(struct vec_generic *vec, size_t elem_size, size_t cap) {
    if (cap > vec->capacity) {
        const size_t new_cap = MAX(cap, vec->capacity * GROWTH_FACTOR);
        vec->data = xreallocarray(vec->data, new_cap, elem_size);
        vec->capacity = new_cap;
    }
}

static size_t vec_bound_check(const struct vec_generic *vec, size_t index) {
    if (index >= vec->size) {
        fprintf(stderr, "Index %zu is out of bounds of vec of size %zu", index, vec->size);
        fflush(stderr);
        abort();
    }
    return index;
}

size_t vec_normalise_index_generic(const struct vec_generic *vec, ptrdiff_t index) {
    if (index < 0) {
        return vec_bound_check(vec, vec->size - -index);
    } else {
        return vec_bound_check(vec, index);
    }
}

void *vec_insert_generic(struct vec_generic *vec, ptrdiff_t _index,
                         const void *elems, size_t elem_size, size_t elem_count,
                         bool zero_init) {
    size_t index = vec_normalise_index_generic(vec, _index);
    vec_ensure_capacity(vec, elem_size, vec->size + elem_count);

    /* shift existing elements to make space for new ones */
    memmove((char *)vec->data + ((index + elem_count) * elem_size),
            (char *)vec->data + (index * elem_size),
            (vec->size - index) * elem_size);

    /* copy new elements to vec */
    if (elems != NULL) {
        memcpy((char *)vec->data + (index * elem_size), elems, elem_size * elem_count);
    } else if (zero_init) {
        memset((char *)vec->data + (index * elem_size), '\0', elem_size * elem_count);
    }
    vec->size += elem_count;

    return (char *)vec->data + (index * elem_size);
}

void *vec_append_generic(struct vec_generic *vec, const void *elems,
                         size_t elem_size, size_t elem_count, bool zero_init) {
    vec_ensure_capacity(vec, elem_size, vec->size + elem_count);

    /* append new elements to the end */
    if (elems != NULL) {
        memcpy((char *)vec->data + (vec->size * elem_size), elems, elem_size * elem_count);
    } else if (zero_init) {
        memset((char *)vec->data + (vec->size * elem_size), '\0', elem_size * elem_count);
    }
    vec->size += elem_count;

    return (char *)vec->data + ((vec->size - elem_count) * elem_size);
}

void vec_erase_generic(struct vec_generic *vec, ptrdiff_t _index,
                       size_t elem_size, size_t elem_count) {
    const size_t index = vec_normalise_index_generic(vec, _index);
    vec_bound_check(vec, index + elem_count - 1);

    memmove((char *)vec->data + (index * elem_size),
            (char *)vec->data + ((index + elem_count) * elem_size),
            (vec->size - index - elem_count) * elem_size);
    vec->size -= elem_count;
}

void *vec_at_generic(struct vec_generic *vec, ptrdiff_t _index, size_t elem_size) {
    size_t index = vec_normalise_index_generic(vec, _index);
    return (char *)vec->data + (index * elem_size);
}

void vec_clear_generic(struct vec_generic *vec) {
    vec->size = 0;
}

void vec_free_generic(struct vec_generic *vec) {
    vec->size = 0;
    vec->capacity = 0;
    free(vec->data);
    vec->data = NULL;
}

void vec_reserve_generic(struct vec_generic *vec, size_t elem_size, size_t elem_count) {
    vec_ensure_capacity(vec, elem_size, elem_count);
}

void vec_exchange_generic(struct vec_generic *v1, struct vec_generic *v2) {
    struct vec_generic tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
}

