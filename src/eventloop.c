#include <stdlib.h>
#include "xmalloc.h"
#define POLLEN_CALLOC(n, size) xcalloc(n, size)
#define POLLEN_FREE(ptr) free(ptr)

#include "log.h"
//#define POLLEN_LOG_DEBUG(fmt, ...) DEBUG("event loop: " fmt, ##__VA_ARGS__)
#define POLLEN_LOG_INFO(fmt, ...) INFO("event loop: " fmt, ##__VA_ARGS__)
#define POLLEN_LOG_WARN(fmt, ...) WARN("event loop: " fmt, ##__VA_ARGS__)
#define POLLEN_LOG_ERR(fmt, ...) ERROR("event loop: " fmt, ##__VA_ARGS__)

#define POLLEN_IMPLEMENTATION
#include "lib/pollen/pollen.h"

struct pollen_loop *event_loop = NULL;

