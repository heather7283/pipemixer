#ifndef SRC_PW_ROUNDTRIP_H
#define SRC_PW_ROUNDTRIP_H

#include <pipewire/pipewire.h>

typedef void (*roundtrip_async_callback_t)(void *data);

void roundtrip_async(struct pw_core *core, roundtrip_async_callback_t callback, void *data);

#endif /* #ifndef SRC_PW_ROUNDTRIP_H */

