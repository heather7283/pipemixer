#ifndef XMALLOC_H
#define XMALLOC_H

#include <stddef.h>

/* malloc, but aborts on alloc fail */
void *xmalloc(size_t size);
/* calloc, but aborts on alloc fail */
void *xcalloc(size_t n, size_t size);
/* realloc, but aborts on alloc fail */
void *xrealloc(void *ptr, size_t size);

/* strdup, but aborts on alloc fail and returns NULL when called with NULL */
char *xstrdup(const char *s);

#endif /* #ifndef XMALLOC_H */

