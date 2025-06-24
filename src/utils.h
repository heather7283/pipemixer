#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <wchar.h>
#include <spa/param/audio/raw.h>

#define CHANNEL_NAME_LENGTH_MAX 5 /* without null terminator */
const char *channel_name_from_enum(enum spa_audio_channel chan);

const char *key_name_from_key_code(wint_t code);
bool key_code_from_key_name(const char *name, wint_t *keycode);

bool str_to_ulong(const char *str, unsigned long *res);
bool str_to_long(const char *str, unsigned long *res);
bool str_to_u32(const char *str, uint32_t *res);
bool str_to_i32(const char *str, int32_t *res);

/* modifies string in place! */
size_t wcstrimcols(wchar_t *str, size_t col);

char *read_string_from_fd(int fd, size_t *len);

#endif /* #ifndef UTILS_H */

