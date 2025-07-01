#include <stdlib.h>
#include "xmalloc.h"
#define EVENT_LOOP_CALLOC(n, size) xcalloc(n, size)
#define EVENT_LOOP_FREE(ptr) free(ptr)

#include "log.h"
//#define EVENT_LOOP_LOG_DEBUG(fmt, ...) DEBUG("event loop: " fmt, ##__VA_ARGS__)
#define EVENT_LOOP_LOG_INFO(fmt, ...) INFO("event loop: " fmt, ##__VA_ARGS__)
#define EVENT_LOOP_LOG_WARN(fmt, ...) WARN("event loop: " fmt, ##__VA_ARGS__)
#define EVENT_LOOP_LOG_ERROR(fmt, ...) ERR("event loop: " fmt, ##__VA_ARGS__)

#define EVENT_LOOP_IMPLEMENTATION
#include "thirdparty/event_loop.h"

