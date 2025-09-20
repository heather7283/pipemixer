#include <sys/eventfd.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "signals.h"
#include "eventloop.h"
#include "macros.h"
#include "log.h"

static int signal_emitter_dispatch_events(struct pollen_callback *_,
                                          int fd, uint32_t _, void *data) {
    struct signal_emitter *emitter = data;

    /* drain the efd */
    uint64_t dummy;
    eventfd_read(fd, &dummy);

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

    emitter->efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (emitter->efd < 0) {
        ERROR("failed to create eventfd: %s", strerror(errno));
        goto err;
    }

    emitter->efd_callback = pollen_loop_add_fd(event_loop, emitter->efd, EPOLLIN, true,
                                               signal_emitter_dispatch_events, emitter);
    if (emitter->efd_callback == NULL) {
        goto err;
    }

    return true;

err:
    if (emitter->efd > 0) {
        close(emitter->efd);
        emitter->efd = -1;
    }
    if (emitter->efd_callback != NULL) {
        pollen_loop_remove_callback(emitter->efd_callback);
        emitter->efd_callback = NULL;
    }
    return false;
}

void signal_emitter_cleanup(struct signal_emitter *emitter) {
    pollen_loop_remove_callback(emitter->efd_callback);
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

static void signal_emit_internal(const struct signal_emitter *emitter,
                                 uint64_t id, uint64_t event, struct signal_data *data) {
    struct signal_queued_event *ev = VEC_EMPLACE_BACK(&emitter->queued_events);
    *ev = (struct signal_queued_event){
        .id = id,
        .event = event,
        .data = *data,
    };

    eventfd_write(emitter->efd, 1);
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

