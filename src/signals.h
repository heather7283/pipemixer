#pragma once

#include <stdint.h>

#include "collections/list.h"
#include "collections/vec.h"

enum signal_data_type {
    SIGNAL_DATA_TYPE_PTR,
    SIGNAL_DATA_TYPE_STR,
    SIGNAL_DATA_TYPE_U64,
    SIGNAL_DATA_TYPE_I64,
    SIGNAL_DATA_TYPE_F64,
    SIGNAL_DATA_TYPE_BOOLEAN,
};

struct signal_data {
    enum signal_data_type type;
    union {
        void *ptr;
        char *str;
        uint64_t u64;
        int64_t i64;
        double f64;
        bool boolean;
    } as;
};

typedef void (*signal_callback_func_t)(uint64_t id, uint64_t event,
                                       const struct signal_data *data,
                                       void *userdata);

struct signal_listener {
    signal_callback_func_t callback;
    void *callback_data;

    uint64_t id, events;

    LIST_ENTRY link;
};

struct signal_queued_event {
    uint64_t id, event;
    struct signal_data data;
};

struct signal_emitter {
    struct pollen_event_source *efd_source;

    VEC(struct signal_queued_event) queued_events;

    LIST_HEAD listeners;
};

bool signal_emitter_init(struct signal_emitter *emitter);
void signal_emitter_cleanup(struct signal_emitter *emitter);

bool signal_listener_is_subscribed(const struct signal_listener *const listener);

void signal_subscribe(struct signal_emitter *emitter, struct signal_listener *listener,
                      uint64_t id, uint64_t events,
                      signal_callback_func_t callback, void *callback_data);
void signal_unsubscribe(struct signal_listener *listener);

void signal_emit_ptr(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, void *ptr);
void signal_emit_str(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, char *str);
void signal_emit_u64(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, uint64_t u64);
void signal_emit_i64(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, int64_t i64);
void signal_emit_f64(const struct signal_emitter *emitter,
                     uint64_t id, uint64_t event, double f64);
void signal_emit_bool(const struct signal_emitter *emitter,
                      uint64_t id, uint64_t event, bool boolean);

