#include <stdarg.h>

#include "collections/wstring.h"
#include "xmalloc.h"
#include "macros.h"

int wstring_printf(struct wstring *ws, const wchar_t *fmt, ...) {
    /* fuck this shitty libc API */
    static wchar_t buf[1024];

    va_list args;
    va_start(args, fmt);
    const int len = vswprintf(buf, SIZEOF_ARRAY(buf), fmt, args);
    va_end(args);

    if (len < 0) {
        return -1;
    }

    if (ws->cap < ws->len + len + 1) {
        ws->cap = ws->len + len + 1;
        ws->data = xreallocarray(ws->data, ws->cap, sizeof(ws->data[0]));
    }

    wmemcpy(&ws->data[ws->len], buf, len);
    ws->len += len;
    ws->data[ws->len] = L'\0';

    return len;
}

void wstring_init(struct wstring *ws) {
    *ws = (struct wstring){0};
}

void wstring_clear(struct wstring *ws) {
    ws->len = 0;
    if (ws->data) {
        ws->data[0] = L'\0';
    }
}

void wstring_free(struct wstring *ws) {
    free(ws->data);
    wstring_init(ws);
}

