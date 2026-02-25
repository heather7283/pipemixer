#pragma once

#include <stdbool.h>
#include <wchar.h>
#include <spa/param/audio/raw.h>

#define CHANNEL_NAME_LENGTH_MAX 5 /* without null terminator */
const char *channel_name_from_enum(enum spa_audio_channel chan);

const char *key_name_from_key_code(wint_t code);
bool key_code_from_key_name(const char *name, wint_t *keycode);

char *read_string_from_fd(int fd, size_t *len);

/* compares two strings, safely handles NULL (NULL is distinct from NULL) */
bool streq(const char *a, const char *b);
bool strneq(const char *a, const char *b, size_t len);

