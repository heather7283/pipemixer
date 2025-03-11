#include <stdlib.h>
#include "xmalloc.h"
#define EVENT_LOOP_CALLOC(n, size) xcalloc(n, size)
#define EVENT_LOOP_FREE(ptr) free(ptr)

#include "log.h"
#define EVENT_LOOP_LOG_DEBUG(fmt, ...) debug("event loop: " fmt, ##__VA_ARGS__)
#define EVENT_LOOP_LOG_WARN(fmt, ...) warn("event loop: " fmt, ##__VA_ARGS__)
#define EVENT_LOOP_LOG_ERR(fmt, ...) err("event loop: " fmt, ##__VA_ARGS__)

#define EVENT_LOOP_IMPLEMENTATION
#include "thirdparty/event_loop.h"

