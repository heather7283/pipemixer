#include <sys/eventfd.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>

#include "events.h"
#include "collections/vec.h"
#include "eventloop.h"
#include "xmalloc.h"
#include "macros.h"
#include "log.h"

struct event_emitter {
    struct list hooks;

    VEC(struct event {
        uint64_t id;
        union event_data data;
        struct event_hook *hook;
    }) events;
    event_dispatcher_t *dispatcher;

    struct list link;

    bool in_callback;
    bool released;
};

static struct pollen_event_source *efd_source = NULL;

static struct list pending_emitters = { &pending_emitters, &pending_emitters };

static void event_emitter_free(struct event_emitter *e);

static void event_emitter_dispatch_events(struct event_emitter *emitter) {
    VEC_FOREACH(&emitter->events, i) {
        struct event *event = &emitter->events.data[i];

        if (event->hook) {
            TRACE("event %zu for emitter %p: id=%lu disp=%p data=%lu hook=%p (unicast)",
                  i, emitter, event->id, emitter->dispatcher, event->data.u, event->hook);

            emitter->dispatcher(event->id, event->data, event->hook);
        } else {
            LIST_FOREACH(elem, &emitter->hooks) {
                struct event_hook *hook = CONTAINER_OF(elem, struct event_hook, link);

                TRACE("event %zu for emitter %p: id=%lu disp=%p data=%lu hook=%p (broadcast)",
                      i, emitter, event->id, emitter->dispatcher, event->data.u, hook);

                emitter->dispatcher(event->id, event->data, hook);
            }
        }
    }
    VEC_CLEAR(&emitter->events);
}

static int on_efd_triggered(struct pollen_event_source *_, uint64_t _, void *_) {
    while (!list_is_empty(&pending_emitters)) {
        struct event_emitter *emitter = CONTAINER_OF(list_remove(pending_emitters.next),
                                                     struct event_emitter, link);

        emitter->in_callback = true;
        TRACE("processing events for emitter %p", (void *)emitter);
        event_emitter_dispatch_events(emitter);
        emitter->in_callback = false;

        if (emitter->released) {
            event_emitter_free(emitter);
        }
    }

    return 0;
}

bool events_global_init(void) {
    efd_source = pollen_loop_add_efd(event_loop, on_efd_triggered, NULL);
    return efd_source != NULL;
}

struct event_emitter *event_emitter_create(event_dispatcher_t *dispatcher) {
    struct event_emitter *e = xmalloc(sizeof(*e));

    *e = (struct event_emitter){
        .dispatcher = dispatcher,
        .hooks = list_init(&e->hooks),
        .link = list_init(&e->link),
    };

    return e;
}

static void event_emitter_free(struct event_emitter *e) {
    LIST_FOREACH(elem, &e->hooks) {
        struct event_hook *hook = CONTAINER_OF(elem, struct event_hook, link);
        event_hook_remove(hook);
    }
    VEC_FREE(&e->events);
    list_remove(&e->link);
    free(e);
}

void event_emitter_release(struct event_emitter *e) {
    if (e->in_callback) {
        e->released = true;
    } else {
        event_emitter_free(e);
    }
}

void event_emitter_add_hook(struct event_emitter *emitter, struct event_hook *hook) {
    TRACE("event_emitter_add_hook(emitter=%p, hook=%p)", emitter, hook);

    list_insert_before(&emitter->hooks, &hook->link);
}

void event_hook_remove(struct event_hook *hook) {
    if (hook->link.next) {
        list_remove(&hook->link);
        if (hook->remove) {
            hook->remove(hook);
        }
        *hook = (struct event_hook){0};
    }
}

static void event_emit_internal(struct event_emitter *emitter, struct event_hook *hook,
                                uint64_t id, union event_data data) {
    struct event *ev = VEC_EMPLACE_BACK(&emitter->events);
    *ev = (struct event){
        .id = id,
        .data = data,
        .hook = hook,
    };

    if (list_is_empty(&emitter->link)) {
        const bool first = list_is_empty(&pending_emitters);

        list_insert_before(&pending_emitters, &emitter->link);

        if (first) {
            pollen_efd_trigger(efd_source);
        }
    }
}

void event_emit(struct event_emitter *emitter, struct event_hook *hook,
                uint64_t id, int type, ...) {
    va_list arg;
    va_start(arg, type);

    union event_data data = {0};
    switch (type) {
    case 'p': data.p = va_arg(arg, void *); break;
    case 'u': data.u = va_arg(arg, uint64_t); break;
    case 'i': data.i = va_arg(arg, int64_t); break;
    case 'd': data.d = va_arg(arg, double); break;
    case 'b': data.b = va_arg(arg, int); break;
    case '0': /* special case - empty data */ break;
    default: assert(0 && "invalid type passed to event_emit");
    }

    va_end(arg);

    event_emit_internal(emitter, hook, id, data);
}

