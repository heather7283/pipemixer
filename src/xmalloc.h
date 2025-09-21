#ifndef XMALLOC_H
#define XMALLOC_H

#include <stddef.h>
#include <stdlib.h>

/* malloc, but aborts on alloc fail */
void *xmalloc(size_t size);
/* xmalloc that also zero initialises memory */
void *xzalloc(size_t size);
/* calloc, but aborts on alloc fail */
void *xcalloc(size_t n, size_t size);
/* realloc, but aborts on alloc fail */
void *xrealloc(void *ptr, size_t size);
/* reallocarray, but aborts on alloc fail */
void *xreallocarray(void *ptr, size_t nmemb, size_t size);

/* strdup, but aborts on alloc fail and returns NULL when called on NULL */
char *xstrdup(const char *s);

#endif /* #ifndef XMALLOC_H */

