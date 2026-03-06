#pragma once

#include <stdbool.h>
#include <wchar.h>

const char *key_name_from_key_code(wint_t code);
bool key_code_from_key_name(const char *name, wint_t *keycode);

/* compares two strings, safely handles NULL (NULL is distinct from NULL) */
bool streq(const char *a, const char *b);
bool strneq(const char *a, const char *b, size_t len);

/* returns true if str begins with prefix and puts the rest in suffix */
bool cut_prefix(const char *str, const char *prefix, const char **suffix);

