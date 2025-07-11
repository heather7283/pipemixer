#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdbool.h>

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

/* ncurses has a trace macro so make this one uppercase, TODO: make others uppercase too */
#define TRACE(fmt, ...) \
    do { \
        log_print(LOG_TRACE, \
                  "%s:%-3d " fmt, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define DEBUG(fmt, ...) \
    do { \
        log_print(LOG_DEBUG, \
                  "%s:%-3d " fmt, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define INFO(fmt, ...) \
    do { \
        log_print(LOG_INFO, \
                  "%s:%-3d " fmt, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)


#define WARN(fmt, ...) \
    do { \
        log_print(LOG_WARN, \
                  "%s:%-3d " fmt, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define ERROR(fmt, ...) \
    do { \
        log_print(LOG_ERROR, \
                  "%s:%-3d " fmt, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#undef PRINTF

#endif /* ifndef LOG_H */

