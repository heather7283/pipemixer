#include <sys/eventfd.h>
#include <unistd.h>

#include "signals.h"
#include "eventloop.h"
#include "macros.h"

static int signal_emitter_dispatch_events(struct pollen_event_source *_, uint64_t _, void *data) {
    struct signal_emitter *emitter = data;

    VEC_FOREACH(&emitter->queued_events, i) {
        const struct signal_queued_event *ev = VEC_AT(&emitter->queued_events, i);

        const struct signal_listener *listener;
        LIST_FOR_EACH(listener, &emitter->listeners, link) {
            if (ev->id == listener->id && ev->event & listener->events) {
                listener->callback(ev->id, ev->event, &ev->data, listener->callback_data);
            }
        }
    }
    VEC_CLEAR(&emitter->queued_events);

    return 0;
}

bool signal_emitter_init(struct signal_emitter *emitter) {
    LIST_INIT(&emitter->listeners);
    VEC_INIT(&emitter->queued_events);

    emitter->efd_source = pollen_loop_add_efd(event_loop, signal_emitter_dispatch_events, emitter);
    if (emitter->efd_source == NULL) {
        return false;
    }

    return true;
}

void signal_emitter_cleanup(struct signal_emitter *emitter) {
    pollen_event_source_remove(emitter->efd_source);
    VEC_FREE(&emitter->queued_events);
}

void signal_subscribe(struct signal_emitter *emitter, struct signal_listener *listener,
                      uint64_t id, uint64_t events,
                      signal_callback_func_t callback, void *callback_data) {
    listener->id = id;
    listener->events = events;
    listener->callback = callback;
    listener->callback_data = callback_data;

    LIST_INSERT(&emitter->listeners, &listener->link);
}

void signal_unsubscribe(struct signal_listener *listener) {
    if (listener->link.next != NULL) {
        LIST_REMOVE(&listener->link);
        listener->link = (struct list){0};
    }
}

bool signal_listener_is_subscribed(const struct signal_listener *const listener) {
    return listener->link.next == &listener->link;
}

static void signal_emit_internal(const struct signal_emitter *emitter,
                                 uint64_t id, uint64_t event, struct signal_data *data) {
    struct signal_queued_event *ev = VEC_EMPLACE_BACK(&emitter->queued_events);
    *ev = (struct signal_queued_event){
        .id = id,
        .event = event,
        .data = *data,
    };

    pollen_efd_trigger(emitter->efd_source);
}

void signal_emit_ptr(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, void *ptr) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_PTR,
        .as.ptr = ptr,
    };
    signal_emit_internal(emitter, id, event, &data);
}

void signal_emit_str(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, char *str) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_STR,
        .as.str = str,
    };
    signal_emit_internal(emitter, id, event, &data);
}

void signal_emit_u64(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, uint64_t u64) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_U64,
        .as.u64 = u64,
    };
    signal_emit_internal(emitter, id, event, &data);
}

void signal_emit_i64(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, int64_t i64) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_I64,
        .as.i64 = i64,
    };
    signal_emit_internal(emitter, id, event, &data);
}

void signal_emit_f64(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, double f64) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_F64,
        .as.f64 = f64,
    };
    signal_emit_internal(emitter, id, event, &data);
}

void signal_emit_bool(const struct signal_emitter *emitter,
                      uint64_t id, uint64_t event, bool boolean) {
    struct signal_data data = {
        .type = SIGNAL_DATA_TYPE_BOOLEAN,
        .as.boolean = boolean,
    };
    signal_emit_internal(emitter, id, event, &data);
}

