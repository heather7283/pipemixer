#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "collections/list.h"

struct event_hook;
struct event_emitter;

typedef void event_hook_remove_t(struct event_hook *hook);

struct event_hook {
    /* type erased dispatch table and opaque user pointer passed to callbacks */
    const void *callbacks;
    void *callbacks_data;

    void (*remove)(struct event_hook *hook);
    void *private_data;

    struct list link;
};

union event_data {
    void *p;
    uint64_t u;
    int64_t i;
    double d;
    bool b;
};

typedef void event_dispatcher_t(uint64_t id, union event_data data, struct event_hook *hook);

#define EVENT_DISPATCH(fn, ...) if (fn) fn(__VA_ARGS__);

/* events system uses one global eventfd that is created with this call */
bool events_global_init(void);

struct event_emitter *event_emitter_create(event_dispatcher_t *dispatcher);
void event_emitter_release(struct event_emitter *emitter);

void event_emitter_add_hook(struct event_emitter *e, struct event_hook *hook);

void event_hook_remove(struct event_hook *hook);

/* type of one of p, u, i, d, b, or 0 for empty data */
void event_emit(struct event_emitter *emitter,
                struct event_hook *hook,
                uint64_t id, int type, ...);

