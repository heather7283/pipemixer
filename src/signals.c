#include <sys/eventfd.h>
#include <unistd.h>

#include "signals.h"
#include "eventloop.h"
#include "macros.h"
#include "xmalloc.h"
#include "collections/vec.h"

struct queued_event {
    uint64_t event;
    struct signal_data data;
};

struct signal_emitter {
    LIST_HEAD listeners;

    VEC(struct queued_event) queued_events;

    bool added_to_pending;
    bool released;
};

static struct pollen_event_source *efd_source = NULL;

static VEC(struct signal_emitter *) pending_emitters = {0};

static void signal_emitter_dispatch_events(struct signal_emitter *emitter) {
    VEC_FOREACH(&emitter->queued_events, i) {
        const struct queued_event *event = &emitter->queued_events.data[i];

        const struct signal_listener *listener;
        LIST_FOR_EACH(listener, &emitter->listeners, link) {
            if (event->event & listener->events) {
                listener->callback(event->event, &event->data, listener->callback_data);
            }
        }
    }
    VEC_CLEAR(&emitter->queued_events);
}

struct signal_emitter *signal_emitter_create(void) {
    struct signal_emitter *e = xzalloc(sizeof(*e));

    LIST_INIT(&e->listeners);

    return e;
}

static void signal_emitter_free(struct signal_emitter *emitter) {
    VEC_FREE(&emitter->queued_events);
    free(emitter);
}

void signal_emitter_release(struct signal_emitter *emitter) {
    if (emitter->added_to_pending) {
        emitter->released = true;
    } else {
        signal_emitter_free(emitter);
    }
}

void signal_listener_subscribe(struct signal_listener *listener,
                               struct signal_emitter *emitter, uint64_t events,
                               signal_callback_t callback, void *callback_data) {
    *listener = (struct signal_listener){
        .events = events,
        .callback = callback,
        .callback_data = callback_data
    };
    LIST_INSERT(&emitter->listeners, &listener->link);
}

void signal_listener_unsubscribe(struct signal_listener *listener) {
    if (listener->link.next != NULL) {
        LIST_REMOVE(&listener->link);
        listener->link = (struct list){0};
    }
}

static void signal_emit_internal(struct signal_emitter *emitter,
                                 uint64_t event, struct signal_data *data) {
    struct queued_event *ev = VEC_EMPLACE_BACK(&emitter->queued_events);
    *ev = (struct queued_event){
        .event = event,
        .data = *data,
    };

    if (!emitter->added_to_pending) {
        VEC_APPEND(&pending_emitters, &emitter);
        emitter->added_to_pending = true;

        if (VEC_SIZE(&pending_emitters) == 1) {
            pollen_efd_trigger(efd_source);
        }
    }
}

void signal_emit_ptr(struct signal_emitter *emitter, uint64_t event, void *ptr) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_PTR,
        .as.ptr = ptr,
    };
    signal_emit_internal(emitter, event, &data);
}

void signal_emit_str(struct signal_emitter *emitter, uint64_t event, char *str) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_STR,
        .as.str = str,
    };
    signal_emit_internal(emitter, event, &data);
}

void signal_emit_u64(struct signal_emitter *emitter, uint64_t event, uint64_t u64) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_U64,
        .as.u64 = u64,
    };
    signal_emit_internal(emitter, event, &data);
}

void signal_emit_i64(struct signal_emitter *emitter, uint64_t event, int64_t i64) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_I64,
        .as.i64 = i64,
    };
    signal_emit_internal(emitter, event, &data);
}

void signal_emit_f64(struct signal_emitter *emitter, uint64_t event, double f64) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_F64,
        .as.f64 = f64,
    };
    signal_emit_internal(emitter, event, &data);
}

void signal_emit_bool(struct signal_emitter *emitter, uint64_t event, bool boolean) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_BOOL,
        .as.boolean = boolean,
    };
    signal_emit_internal(emitter, event, &data);
}

static int on_efd_triggered(struct pollen_event_source *_, uint64_t _, void *_) {
    VEC_FOREACH(&pending_emitters, i) {
        struct signal_emitter *emitter = pending_emitters.data[i];

        signal_emitter_dispatch_events(emitter);

        emitter->added_to_pending = false;
        if (emitter->released) {
            signal_emitter_free(emitter);
        }
    }
    VEC_CLEAR(&pending_emitters);

    return 0;
}

bool signals_global_init(void) {
    efd_source = pollen_loop_add_efd(event_loop, on_efd_triggered, NULL);
    return efd_source != NULL;
}

