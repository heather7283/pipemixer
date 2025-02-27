#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xmalloc.h"

static void handle_alloc_failure(void) {
    fprintf(stderr, "memory allocation failed, buy more ram lol\n");
    fflush(stderr);
    abort();
}

void *xmalloc(size_t size) {
    void *alloc = malloc(size);
    if (alloc == NULL) {
        handle_alloc_failure();
    }
    return alloc;
}

void *xcalloc(size_t n, size_t size) {
    void *alloc = calloc(n, size);
    if (alloc == NULL) {
        handle_alloc_failure();
    }
    return alloc;
}

void *xrealloc(void *ptr, size_t size) {
    void *alloc = realloc(ptr, size);
    if (alloc == NULL) {
        handle_alloc_failure();
    }
    return alloc;
}

char *xstrdup(const char *s) {
    if (s == NULL) {
        return NULL;
    }

    char *alloc = strdup(s);
    if (alloc == NULL) {
        handle_alloc_failure();
    }
    return alloc;
}

