#ifndef SRC_PW_EVENTS_H
#define SRC_PW_EVENTS_H

#include "signals.h"

enum pipewire_events {
    PIPEWIRE_EVENT_NODE_ADDED = 1 << 0, /* new node id as u64 */
    PIPEWIRE_EVENT_DEVICE_ADDED = 1 << 1, /* new device id as u64 */
};

void pipewire_events_subscribe(struct signal_listener *listener, uint64_t events,
                               signal_callback_t callback, void *callback_data);

#endif /* #ifndef SRC_PW_EVENTS_H */

