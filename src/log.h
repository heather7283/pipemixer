#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdbool.h>

#define LOG_ANSI_COLORS_ERROR "\033[31m"
#define LOG_ANSI_COLORS_WARN  "\033[33m"
#define LOG_ANSI_COLORS_DEBUG "\033[90m"
#define LOG_ANSI_COLORS_RESET "\033[0m"

enum log_loglevel {
    LOG_INVALID,
    LOG_QUIET,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
};

enum log_loglevel log_str_to_loglevel(const char *str);

void log_init(FILE *stream, enum log_loglevel level, bool force_colors);
void log_print(enum log_loglevel level, char *msg, ...);

/* TODO: replace usage of those macros with log_print */

#define debug(fmt, ...) \
    do { \
        log_print(LOG_DEBUG, \
                  "%s:%-3d " fmt, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define info(fmt, ...) \
    do { \
        log_print(LOG_INFO, \
                  "%s:%-3d " fmt, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)


#define warn(fmt, ...) \
    do { \
        log_print(LOG_WARN, \
                  "%s:%-3d " fmt, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define err(fmt, ...) \
    do { \
        log_print(LOG_ERROR, \
                  "%s:%-3d " fmt, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#endif /* ifndef LOG_H */

