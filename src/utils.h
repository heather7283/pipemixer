#ifndef UTILS_H
#define UTILS_H

#include <wchar.h>
#include <spa/param/audio/raw.h>

#define MAX_CHANNEL_NAME_LEN 5

const char *channel_name_from_enum(enum spa_audio_channel chan);

wchar_t *mbstowcsdup(const char *src);

/* modifies string in place! */
size_t wcstrimcols(wchar_t *str, size_t col);

#endif /* #ifndef UTILS_H */

