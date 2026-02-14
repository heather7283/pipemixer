#include <sys/eventfd.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>

#include "events.h"
#include "collections/vec.h"
#include "eventloop.h"
#include "macros.h"

static struct pollen_event_source *efd_source = NULL;

static struct list pending_emitters = { &pending_emitters, &pending_emitters };

static void event_emitter_dispatch_events(struct event_emitter *emitter) {
    VEC_FOREACH(&emitter->events, i) {
        struct event *event = &emitter->events.data[i];

        LIST_FOREACH(elem, &emitter->hooks) {
            struct event_hook *hook = CONTAINER_OF(elem, struct event_hook, link);
            emitter->dispatcher(event->id, event->data, hook);
        }
    }
    VEC_CLEAR(&emitter->events);
}

static int on_efd_triggered(struct pollen_event_source *_, uint64_t _, void *_) {
    LIST_FOREACH(elem, &pending_emitters) {
        struct event_emitter *emitter = CONTAINER_OF(elem, struct event_emitter, link);
        event_emitter_dispatch_events(emitter);
        list_remove(&emitter->link);
    }

    return 0;
}

bool events_global_init(void) {
    efd_source = pollen_loop_add_efd(event_loop, on_efd_triggered, NULL);
    return efd_source != NULL;
}

void event_emitter_init(struct event_emitter *e, event_dispatcher_t *dispatcher) {
    *e = (struct event_emitter){
        .dispatcher = dispatcher,
        .hooks = list_init(&e->hooks),
        .link = list_init(&e->link),
    };
}

void event_emitter_cleanup(struct event_emitter *e) {
    LIST_FOREACH(elem, &e->hooks) {
        struct event_hook *hook = CONTAINER_OF(elem, struct event_hook, link);
        event_hook_remove(hook);
    }
    VEC_FREE(&e->events);
    list_remove(&e->link);
}

void event_emitter_add_hook(struct event_emitter *emitter, struct event_hook *hook) {
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

static void event_emit_internal(struct event_emitter *emitter, uint64_t id, union event_data data) {
    struct event *ev = VEC_EMPLACE_BACK(&emitter->events);
    *ev = (struct event){
        .id = id,
        .data = data,
    };

    if (list_is_empty(&emitter->link)) {
        const bool first = list_is_empty(&pending_emitters);

        list_insert_before(&pending_emitters, &emitter->link);

        if (first) {
            pollen_efd_trigger(efd_source);
        }
    }
}

void event_emit(struct event_emitter *emitter, uint64_t id, int type, ...) {
    va_list arg;
    va_start(arg, type);

    union event_data data = {0};
    switch (type) {
    case 'p': data.p = va_arg(arg, void *); break;
    case 'u': data.u = va_arg(arg, uint64_t); break;
    case 'i': data.i = va_arg(arg, int64_t); break;
    case 'd': data.d = va_arg(arg, double); break;
    case 'b': data.b = va_arg(arg, bool); break;
    case '0': /* special case - empty data */ break;
    default: assert(0 && "invalid type passed to event_emit");
    }

    va_end(arg);

    event_emit_internal(emitter, id, data);
}

