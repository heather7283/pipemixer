#ifndef SRC_PW_EVENTS_H
#define SRC_PW_EVENTS_H

#include <stdint.h>

#include "signals.h"

enum pipewire_events: uint64_t {
    PIPEWIRE_EVENT_NODE_ADDED = 1 << 0, /* node id as u64 */
    PIPEWIRE_EVENT_NODE_CHANGED = 1 << 1, /* node id as u64 */
    PIPEWIRE_EVENT_NODE_REMOVED = 1 << 2, /* node id as u64 */

    PIPEWIRE_EVENT_DEVICE_CHANGED = 1 << 16, /* device id as u64 */
};

void pipewire_events_subscribe(struct signal_listener *listener, uint64_t events,
                               signal_callback_func_t callback, void *callback_data);

#endif /* #ifndef SRC_PW_EVENTS_H */

