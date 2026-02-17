#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define PRINTF(fmt_index, first_arg_index) \
    __attribute__((format(printf, fmt_index, first_arg_index)))

enum log_loglevel {
    LOG_INVALID,
    LOG_QUIET,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
};

enum log_loglevel log_str_to_loglevel(const char *str);

void log_init(FILE *stream, enum log_loglevel level, bool force_colors);
PRINTF(2, 3) void log_print(enum log_loglevel level, char *msg, ...);

#define TRACE(fmt, ...) log_print(LOG_TRACE, "%s:%-3d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define DEBUG(fmt, ...) log_print(LOG_DEBUG, "%s:%-3d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define INFO(fmt, ...) log_print(LOG_INFO, "%s:%-3d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(fmt, ...) log_print(LOG_WARN, "%s:%-3d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(fmt, ...) log_print(LOG_WARN, "%s:%-3d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define ERROR(fmt, ...) log_print(LOG_ERROR, "%s:%-3d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

#define ASSERT(expr) ({ \
    if (!(expr)) { \
        ERROR("assertion failed in %s: %s", __func__, #expr); \
        abort(); \
    } \
})

#define ABORT(fmt, ...) ({ \
    ERROR(fmt, ##__VA_ARGS__); \
    abort(); \
})

#undef PRINTF

