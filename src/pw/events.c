#include "pw/events.h"
#include "pw/common.h"

void pipewire_events_subscribe(struct signal_listener *listener, uint64_t events,
                               signal_callback_t callback, void *callback_data) {
    signal_listener_subscribe(listener, pw.emitter, events, callback, callback_data);
}

