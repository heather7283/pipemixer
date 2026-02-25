#include <stdlib.h>
#include <stdarg.h>

#include "collections/wstring.h"

bool wstring_puts(struct wstring *ws, const wchar_t *str) {
    if (!ws->stream) {
        wstring_init(ws);
    }

    const int ret = fputws(str, ws->stream);
    fflush(ws->stream);

    return ret >= 0;
}

bool wstring_printf(struct wstring *ws, const wchar_t *fmt, ...) {
    if (!ws->stream) {
        wstring_init(ws);
    }

    va_list args;

    va_start(args, fmt);
    const int ret = vfwprintf(ws->stream, fmt, args);
    va_end(args);

    fflush(ws->stream);

    return ret >= 0;
}

bool wstring_init(struct wstring *ws) {
    ws->stream = open_wmemstream(&ws->data, &ws->len);
    return ws->stream;
}

void wstring_clear(struct wstring *ws) {
    /* TODO: stop using cursed open_wmemstream and make this useful */
    wstring_free(ws);
}

void wstring_free(struct wstring *ws) {
    if (ws->stream) {
        fclose(ws->stream);
    }
    free(ws->data);
    *ws = (struct wstring){0};
}

