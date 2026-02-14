#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "collections/list.h"
#include "collections/vec.h"

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

struct event_emitter {
    struct list hooks;

    VEC(struct event {
        uint64_t id;
        union event_data data;
    }) events;
    event_dispatcher_t *dispatcher;

    struct list link;
};

/* events system uses one global eventfd that is created with this call */
bool events_global_init(void);

void event_emitter_init(struct event_emitter *e, event_dispatcher_t *dispatcher);
void event_emitter_cleanup(struct event_emitter *e);

void event_emitter_add_hook(struct event_emitter *e, struct event_hook *hook);

void event_hook_remove(struct event_hook *hook);

/* type of one of p, u, i, d, b, or 0 for empty data */
void event_emit(struct event_emitter *emitter, uint64_t id, int type, ...);

