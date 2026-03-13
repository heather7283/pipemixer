#include <sys/eventfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

#include "events.h"
#include "collections/list.h"
#include "macros.h"
#include "xmalloc.h"

struct event_hook {
    const void *callbacks_table;
    void *callbacks_data;

    void (*remove)(void *private_data);
    void *private_data;

    /* hook should never see events that fired before its creation */
    uint64_t birth_seq;

    /* hook is freed when released == true && refcnt == 0 */
    bool released;
    int refcnt;

    struct list link;
};

struct event_emitter {
    event_dispatcher_t *dispatcher;
    struct list hooks;

    /* emitter is freed when released == true && refcnt == 0 */
    bool released;
    int refcnt;
};

struct event {
    uint64_t id;
    union event_data data;
    uint64_t seq;
    void (*after)(union event_data);

    /* owned references */
    struct event_emitter *emitter;
    struct event_hook *hook;
};

/* global state */
static struct {
    int efd;
    bool efd_triggered;

    uint64_t seq;

    struct event_queue {
        struct event *ring;
        size_t size, write, read;
    } queue;
} g = {
    .efd = -1,
};

static bool queue_is_empty(const struct event_queue *queue) {
    return queue->read == queue->write;
}

static bool queue_is_full(const struct event_queue *queue) {
    return !queue->size || (queue->write + 1) % queue->size == queue->read;
}

static void queue_grow(struct event_queue *queue) {
    const size_t new_size = queue->size ? queue->size * 2 : 16;
    struct event *new_ring = xcalloc(new_size, sizeof(new_ring[0]));

    size_t i;
    for (i = 0; queue->read != queue->write; i++) {
        new_ring[i] = queue->ring[queue->read];
        queue->read = (queue->read + 1) % queue->size;
    }

    free(queue->ring);
    queue->ring = new_ring;
    queue->size = new_size;
    queue->read = 0;
    queue->write = i;
}

static struct event *queue_push(struct event_queue *queue) {
    if (queue_is_full(queue)) {
        queue_grow(queue);
    }

    struct event *event = &queue->ring[queue->write];
    queue->write = (queue->write + 1) % queue->size;

    return event;
}

static struct event *queue_pop(struct event_queue *queue) {
    if (queue_is_empty(queue)) {
        return NULL;
    }

    struct event *event = &queue->ring[queue->read];
    queue->read = (queue->read + 1) % queue->size;

    return event;
}

static void hook_free(struct event_hook *hook) {
    free(hook);
}

static void hook_unref(struct event_hook *hook) {
    if (--hook->refcnt == 0 && hook->released) {
        hook_free(hook);
    }
}

static struct event_hook *hook_ref(struct event_hook *hook) {
    hook->refcnt += 1;
    return hook;
}

static void emitter_free(struct event_emitter *emitter) {
    free(emitter);
}

static void emitter_unref(struct event_emitter *emitter) {
    if (--emitter->refcnt == 0 && emitter->released) {
        emitter_free(emitter);
    }
}

static struct event_emitter *emitter_ref(struct event_emitter *emitter) {
    emitter->refcnt += 1;
    return emitter;
}

int events_global_init(void) {
    g.efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    return g.efd;
}

void events_dispatch(void) {
    eventfd_read(g.efd, &(uint64_t){});
    g.efd_triggered = false;

    struct event *event;
    while ((event = queue_pop(&g.queue))) {
        struct event_emitter *emitter = event->emitter;

        if (!event->hook) {
            /* broadcast */
            LIST_FOREACH(elem, &emitter->hooks) {
                struct event_hook *hook = CONTAINER_OF(elem, struct event_hook, link);
                if (!hook->released && hook->birth_seq < event->seq) {
                    emitter->dispatcher(event->id, event->data,
                                        hook->callbacks_table, hook->callbacks_data,
                                        hook->private_data);
                }
            }
        } else {
            /* unicast */
            struct event_hook *hook = event->hook;
            if (!hook->released) {
                emitter->dispatcher(event->id, event->data,
                                    hook->callbacks_table, hook->callbacks_data,
                                    hook->private_data);
            }
            hook_unref(hook);
        }

        if (event->after) {
            event->after(event->data);
        }

        emitter_unref(emitter);
    }
}

struct event_emitter *event_emitter_create(event_dispatcher_t *dispatcher) {
    struct event_emitter *emitter = xmalloc(sizeof(*emitter));
    *emitter = (struct event_emitter){
        .dispatcher = dispatcher,
        .hooks = { &emitter->hooks, &emitter->hooks },
    };

    return emitter;
}

void event_emitter_release(struct event_emitter *emitter) {
    if (!emitter) {
        return;
    }

    emitter->released = true;
    if (emitter->refcnt == 0) {
        emitter_free(emitter);
    }
}

struct event_hook *event_emitter_add_hook(struct event_emitter *emitter,
                                          const void *callbacks_table, void *callbacks_data,
                                          void (*remove)(void *private_data), void *private_data) {
    struct event_hook *hook = xmalloc(sizeof(*hook));
    *hook = (struct event_hook){
        .callbacks_table = callbacks_table,
        .callbacks_data = callbacks_data,
        .remove = remove,
        .private_data = private_data,
        .birth_seq = g.seq,
    };

    /* prepend to emitter's hook list */
    list_insert_after(&emitter->hooks, &hook->link);

    return hook;
}

void event_hook_release(struct event_hook *hook) {
    if (!hook) {
        return;
    }

    /* remove from emitter's hook list */
    list_remove(&hook->link);

    /* since no events will be delivered for this hook anymore, it's safe to
     * call remove right now and not wait for hook to actually be destroyed */
    if (hook->remove) {
        hook->remove(hook->private_data);
    }

    hook->released = true;
    if (hook->refcnt == 0) {
        hook_free(hook);
    }
}

static void event_emit_internal(struct event_emitter *emitter, struct event_hook *hook,
                                uint64_t id, void (*after)(union event_data data),
                                union event_data data) {
    struct event *event = queue_push(&g.queue);
    *event = (struct event) {
        .id = id,
        .data = data,
        .seq = ++g.seq,
        .after = after,
        .emitter = emitter_ref(emitter),
        .hook = hook ? hook_ref(hook) : NULL,
    };

    if (!g.efd_triggered) {
        eventfd_write(g.efd, 1);
        g.efd_triggered = true;
    }
}

void event_emit(struct event_emitter *emitter, struct event_hook *hook,
                uint64_t id, void (*after)(union event_data data), int type, ...) {
    union event_data data;

    va_list ap;
    va_start(ap, type);
    switch (type) {
    case 'p': data.p = va_arg(ap, void *); break;
    case 'u': data.u = va_arg(ap, uint64_t); break;
    case 'i': data.i = va_arg(ap, int64_t); break;
    case 'd': data.d = va_arg(ap, double); break;
    case 'b': data.b = va_arg(ap, int); break;
    default:  data.u = 0; break;
    }
    va_end(ap);

    event_emit_internal(emitter, hook, id, after, data);
}

