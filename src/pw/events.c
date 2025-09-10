#include "pw/common.h"

void pipewire_events_subscribe(struct signal_listener *listener, uint64_t events,
                               signal_callback_func_t callback, void *callback_data) {
    signal_subscribe(&pw.emitter, listener, events, callback, callback_data);
}

