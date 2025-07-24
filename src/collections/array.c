#include <string.h>

#include "collections/array.h"
#include "macros.h"
#include "xmalloc.h"

#define GROWTH_FACTOR 1.5

static void array_ensure_capacity(struct array_generic *arr, size_t elem_size, size_t cap) {
    if (cap > arr->capacity) {
        const size_t new_cap = MAX(cap, arr->capacity * GROWTH_FACTOR);
        arr->data = xreallocarray(arr->data, new_cap, elem_size);
        arr->capacity = new_cap;
    }
}

void array_extend_generic(struct array_generic *arr, void *elems,
                          size_t elem_size, size_t elem_count) {
    array_ensure_capacity(arr, elem_size, arr->size + elem_count);

    memcpy((char *)arr->data + (arr->size * elem_size), elems, elem_size * elem_count);
    arr->size += elem_count;
}

void *array_emplace_generic(struct array_generic *arr, size_t elem_size, bool zero_init) {
    array_ensure_capacity(arr, elem_size, arr->size + 1);

    if (zero_init) {
        memset(&((char *)arr->data)[elem_size * arr->size], '\0', elem_size);
    }

    return &((char *)arr->data)[elem_size * arr->size++];
}

void array_clear_generic(struct array_generic *arr) {
    arr->size = 0;
}

void array_free_generic(struct array_generic *arr) {
    arr->size = 0;
    arr->capacity = 0;
    free(arr->data);
    arr->data = NULL;
}

