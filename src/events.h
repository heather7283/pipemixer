#pragma once

#include <stdint.h>
#include <stdbool.h>

struct event_hook;
struct event_emitter;

union event_data {
    void *p;
    uint64_t u;
    int64_t i;
    double d;
    bool b;
};

int events_global_init(void);
void events_dispatch(void);

typedef void event_dispatcher_t(uint64_t id, union event_data data,
                                const void *callbacks, void *callbacks_data,
                                void *private);

#define EVENT_DISPATCH(fn, ...) if (fn) fn(__VA_ARGS__);

struct event_emitter *event_emitter_create(event_dispatcher_t *dispatcher);
void event_emitter_release(struct event_emitter *emitter);

struct event_hook *event_emitter_add_hook(struct event_emitter *emitter,
                                          const void *callbacks, void *callbacks_data,
                                          void (*remove)(void *private_data), void *private_data);
void event_hook_release(struct event_hook *hook);

/* type of one of p, u, i, d, b, or 0 for empty data */
void event_emit(struct event_emitter *emitter, struct event_hook *hook,
                uint64_t id, void (*after)(union event_data data),
                int type, ...);

