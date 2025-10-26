#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "collections/list.h"

enum signal_data_type {
    SIGNAL_DATA_TYPE_PTR,
    SIGNAL_DATA_TYPE_STR,
    SIGNAL_DATA_TYPE_U64,
    SIGNAL_DATA_TYPE_I64,
    SIGNAL_DATA_TYPE_F64,
    SIGNAL_DATA_TYPE_BOOL,
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

typedef void (*signal_callback_t)(uint64_t event, const struct signal_data *data, void *userdata);

struct signal_listener {
    /* bitmask */
    uint64_t events;

    signal_callback_t callback;
    void *callback_data;

    LIST_ENTRY link;
};

struct signal_emitter;

/*
 * signals system uses one global eventfd that is created with this call
 */
bool signals_global_init(void);

/*
 * to prevent events outliving their emitter and causing UAF,
 * make emitter a separate allocation with lazy free semantics
 */
struct signal_emitter *signal_emitter_create(void);
void signal_emitter_release(struct signal_emitter *emitter);


void signal_listener_subscribe(struct signal_listener *listener,
                               struct signal_emitter *emitter, uint64_t events,
                               signal_callback_t callback, void *callback_data);
void signal_listener_unsubscribe(struct signal_listener *listener);


void signal_emit_ptr(struct signal_emitter *emitter, uint64_t event, void *ptr);
void signal_emit_str(struct signal_emitter *emitter, uint64_t event, char *str);
void signal_emit_u64(struct signal_emitter *emitter, uint64_t event, uint64_t u64);
void signal_emit_i64(struct signal_emitter *emitter, uint64_t event, int64_t i64);
void signal_emit_f64(struct signal_emitter *emitter, uint64_t event, double f64);
void signal_emit_bool(struct signal_emitter *emitter, uint64_t event, bool boolean);

