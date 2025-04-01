#ifndef UTILS_H
#define UTILS_H

#include <wchar.h>
#include <spa/param/audio/raw.h>

const char *channel_name_from_enum(enum spa_audio_channel chan);

/* modifies string in place! */
size_t wcstrimcols(wchar_t *str, size_t col);

#define wcsempty(str) ((str) == NULL || *(str) == L'\0')

#endif /* #ifndef UTILS_H */

