#pragma once

#include <stddef.h>
#include <stdbool.h>

struct vec_generic {
    void *data;
    size_t size, cap;
};

#define VEC(type) \
    struct { \
        type *data; \
        size_t size, cap; \
    }

void *vec_append_generic(struct vec_generic *vec, size_t elem_size);

#define VEC_APPEND(pvec) (typeof((pvec)->data))vec_append_generic((struct vec_generic *)(pvec), \
                                                                  sizeof((pvec)->data[0]))

void vec_clear_generic(struct vec_generic *vec);

#define VEC_CLEAR(pvec) vec_clear_generic((struct vec_generic *)(pvec))

void vec_free_generic(struct vec_generic *vec);

#define VEC_FREE(pvec) vec_free_generic((struct vec_generic *)(pvec))

void vec_reserve_generic(struct vec_generic *vec, size_t elem_size, size_t elem_count);

#define VEC_RESERVE(pvec, count) vec_reserve_generic((struct vec_generic *)(pvec), \
                                                     sizeof((pvec)->data[0]), (count))

void vec_exchange_generic(struct vec_generic *v1, struct vec_generic *v2);

#define VEC_EXCHANGE(pvec1, pvec2) vec_exchange_generic((struct vec_generic *)(pvec1), \
                                                        (struct vec_generic *)(pvec2))

#define VEC_FOREACH(pvec, iter) \
    for (size_t iter = 0; iter < (pvec)->size; iter++)

