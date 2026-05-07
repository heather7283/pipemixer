#pragma once

#include "collections/dict.h"
#include "collections/wstring.h"

struct format *format_parse(const char *src, char **error);
void format_render(const struct format *format, const struct dict *dict, struct wstring *result);
void format_free(struct format *format);

