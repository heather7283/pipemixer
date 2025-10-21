#pragma once

#include <stddef.h>
#include <stdbool.h>

struct vec_generic {
    size_t size, capacity;
    void *data;
};

#define VEC(type) \
    struct { \
        size_t size, capacity; \
        type *data; \
    }

#define VEC_INITALISER {0}

#define VEC_INIT(pvec) \
    do { \
        (pvec)->size = (pvec)->capacity = 0; \
        (pvec)->data = NULL; \
    } while (0)

#define TYPECHECK_VEC(pvec) \
    ({ \
        (void)(pvec)->size; (void)(pvec)->capacity; (void)(pvec)->data; 1; \
    })

#define TYPECHECK(a, b) \
    ({ \
        TYPEOF(a) dummy_a; \
        TYPEOF(b) dummy_b; \
        (void)(&dummy_a == &dummy_b); \
        1; \
    })

#define VEC_SIZE(pvec) ((pvec)->size)
#define VEC_DATA(pvec) ((pvec)->data)

/* Accepts python-style array index and returns real index */
size_t vec_normalise_index_generic(const struct vec_generic *vec, ptrdiff_t index);

#define VEC_NORMALISE_INDEX(pvec, index) \
    (vec_normalise_index_generic((struct vec_generic *)(pvec), index))

/*
 * Insert elem_count elements, each of size elem_size, in vec at index.
 * If elems is not NULL, elements are initialised from elems.
 * If elems is NULL and zero_init is true, memory is zero-initialised.
 * If elems is NULL and zero_init is false, memory is NOT initialised.
 * Returns address of the first inserted element.
 * Supports python-style negative indexing.
 * Dumps core on OOB access.
 */
void *vec_insert_generic(struct vec_generic *vec, ptrdiff_t index,
                         const void *elems, size_t elem_size, size_t elem_count,
                         bool zero_init);

#define VEC_INSERT_N(pvec, index, pelem, nelem) \
    ({ \
        TYPECHECK_VEC(pvec); \
        TYPECHECK(*(pvec)->data, *(pelem)); \
        vec_insert_generic((struct vec_generic *)(pvec), (index), \
                           (pelem), sizeof(*(pvec)->data), (nelem), false); \
    })

#define VEC_INSERT(pvec, index, pelem) \
    VEC_INSERT_N(pvec, index, pelem, 1)

#define VEC_EMPLACE_INTERNAL_DO_NOT_USE(pvec, index, nelem, zeroed) \
    ({ \
        TYPECHECK_VEC(pvec); \
        (TYPEOF((pvec)->data))vec_insert_generic((struct vec_generic *)(pvec), (index), \
                                                 NULL, sizeof(*(pvec)->data), (nelem), \
                                                 (zeroed)); \
    })

#define VEC_EMPLACE_N(pvec, index, nelem) \
    VEC_EMPLACE_INTERNAL_DO_NOT_USE(pvec, index, nelem, false)

#define VEC_EMPLACE(pvec, index) \
    VEC_EMPLACE_N(pvec, index, 1)

#define VEC_EMPLACE_N_ZEROED(pvec, index, nelem) \
    VEC_EMPLACE_INTERNAL_DO_NOT_USE(pvec, index, nelem, true)

#define VEC_EMPLACE_ZEROED(pvec, index) \
    VEC_EMPLACE_N_ZEROED(pvec, index, 1)

/*
 * Appends elem_count elements, each of size elem_size, to the end of vec.
 * If elems is not NULL, elements are initialised from elems.
 * If elems is NULL and zero_init is true, memory is zero-initialised.
 * If elems is NULL and zero_init is false, memory is NOT initialised.
 * Returns address of the first appended element.
 */
void *vec_append_generic(struct vec_generic *vec, const void *elems,
                         size_t elem_size, size_t elem_count, bool zero_init);

#define VEC_APPEND_N(pvec, pelem, nelem) \
    ({ \
        TYPECHECK_VEC(pvec); \
        TYPECHECK(*(pvec)->data, *(pelem)); \
        vec_append_generic((struct vec_generic *)(pvec), (pelem), \
                           sizeof(*(pvec)->data), (nelem), false); \
    })

#define VEC_APPEND(pvec, pelem) \
    VEC_APPEND_N(pvec, pelem, 1)

#define VEC_EMPLACE_BACK_INTERNAL_DO_NOT_USE(pvec, nelem, zeroed) \
    ({ \
        TYPECHECK_VEC(pvec); \
        (TYPEOF((pvec)->data))vec_append_generic((struct vec_generic *)(pvec), NULL, \
                                                 sizeof(*(pvec)->data), (nelem), (zeroed)); \
    })

#define VEC_EMPLACE_BACK_N(pvec, nelem) \
    VEC_EMPLACE_BACK_INTERNAL_DO_NOT_USE(pvec, nelem, false)

#define VEC_EMPLACE_BACK(pvec) \
    VEC_EMPLACE_BACK_N(pvec, 1)

#define VEC_EMPLACE_BACK_N_ZEROED(pvec, nelem) \
    VEC_EMPLACE_BACK_INTERNAL_DO_NOT_USE(pvec, nelem, true)

#define VEC_EMPLACE_BACK_ZEROED(pvec) \
    VEC_EMPLACE_BACK_N_ZEROED(pvec, 1)

/*
 * Removes elem_count elements, each of size elem_size, at index from vec.
 * Support python-style negative indexing.
 * Dumps core on OOB access.
 */
void vec_erase_generic(struct vec_generic *vec, ptrdiff_t index,
                         size_t elem_size, size_t elem_count);

#define VEC_ERASE_N(pvec, index, count) \
    ({ \
        TYPECHECK_VEC(pvec); \
        vec_erase_generic((struct vec_generic *)(pvec), \
                          (index), sizeof(*(pvec)->data), (count)); \
    })

#define VEC_ERASE(pvec, index) \
    VEC_ERASE_N(pvec, index, 1)

/*
 * Returns pointer to element of vec at index.
 * Supports python-style negative indexing.
 * Dumps core on OOB access.
 */
void *vec_at_generic(struct vec_generic *vec, ptrdiff_t index, size_t elem_size);

#define VEC_AT(pvec, index) \
    ({ \
        TYPECHECK_VEC(pvec); \
        (TYPEOF((pvec)->data))vec_at_generic((struct vec_generic *)(pvec), \
                                             (index), sizeof(*(pvec)->data)); \
    })

/*
 * Sets size to 0 but does not free memory.
 */
void vec_clear_generic(struct vec_generic *vec);

#define VEC_CLEAR(pvec) \
    ({ \
        TYPECHECK_VEC(pvec); \
        vec_clear_generic((struct vec_generic *)(pvec)); \
    })

/*
 * Frees all memory and makes vec ready for reuse.
 */
void vec_free_generic(struct vec_generic *vec);

#define VEC_FREE(pvec) \
    ({ \
        TYPECHECK_VEC(pvec); \
        vec_free_generic((struct vec_generic *)(pvec)); \
    })

/*
 * Reserves memory for elem_count elements, each of size elem_size.
 */
void vec_reserve_generic(struct vec_generic *vec, size_t elem_size, size_t elem_count);

#define VEC_RESERVE(pvec, count) \
    ({ \
        TYPECHECK_VEC(pvec); \
        vec_reserve_generic((struct vec_generic *)(pvec), sizeof(*(pvec)->data), (count)); \
    })

/*
 * Swaps contents of v1 and v2
 */
void vec_exchange_generic(struct vec_generic *v1, struct vec_generic *v2);

#define VEC_EXCHANGE(pvec1, pvec2) \
    ({ \
        TYPECHECK((pvec1)->data, (pvec2)->data); \
        vec_exchange_generic((struct vec_generic *)(pvec1), (struct vec_generic *)(pvec2)); \
    })

#define VEC_FOREACH(pvec, iter) \
    for (size_t iter = 0; iter < (pvec)->size; iter++)

#define VEC_FOREACH_REVERSE(pvec, iter) \
    for (size_t iter = (pvec)->size; iter-- > 0; )

