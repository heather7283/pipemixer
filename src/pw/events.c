#include "pw/common.h"

void pipewire_events_subscribe(struct signal_listener *listener, uint64_t id, uint64_t events,
                               signal_callback_func_t callback, void *callback_data) {
    signal_subscribe(&pw.core_emitter, listener, id, events, callback, callback_data);
}

