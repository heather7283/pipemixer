#ifndef UTILS_H
#define UTILS_H

#include <wchar.h>
#include <spa/param/audio/raw.h>

#define MAX_CHANNEL_NAME_LEN 5

const char *channel_name_from_enum(enum spa_audio_channel chan);

wchar_t *mbstowcsdup(const char *src);

#endif /* #ifndef UTILS_H */

