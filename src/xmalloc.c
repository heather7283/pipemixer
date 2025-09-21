#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xmalloc.h"

static void *check_alloc(void *const alloc) {
    if (alloc == NULL) {
        fprintf(stderr, "memory allocation failed, buy more ram lol\n");
        fflush(stderr);
        abort();
    }
    return alloc;
}

void *xmalloc(size_t size) {
    return check_alloc(malloc(size));
}

void *xzalloc(size_t size) {
    return memset(check_alloc(malloc(size)), '\0', size);
}

void *xcalloc(size_t n, size_t size) {
    return check_alloc(calloc(n, size));
}

void *xrealloc(void *ptr, size_t size) {
    return check_alloc(realloc(ptr, size));
}

void *xreallocarray(void *ptr, size_t nmemb, size_t size) {
    return check_alloc(reallocarray(ptr, nmemb, size));
}

char *xstrdup(const char *s) {
    return (s == NULL) ? NULL : check_alloc(strdup(s));
}

