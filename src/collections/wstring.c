#include <stdarg.h>
#include <string.h>

#include "collections/wstring.h"
#include "xmalloc.h"
#include "macros.h"

/* make space for len wide characters and a null terminator */
static void make_space(struct wstring *ws, size_t _len) {
    const size_t len = _len + 1;
    if (ws->cap < len) {
        ws->cap = ws->cap * 2 < len ? len : ws->cap * 2;
        ws->data = xreallocarray(ws->data, ws->cap, sizeof(ws->data[0]));
    }
}

bool wstring_appendc(struct wstring *s, char c) {
    return wstring_appendwc(s, c);
}

bool wstring_appendwc(struct wstring *s, wchar_t c) {
    make_space(s, s->len + 1);
    s->data[s->len++] = c;
    s->data[s->len] = '\0';

    return true;
}

bool wstring_appendsn(struct wstring *s, const char *suff, size_t suff_len) {
    const size_t suff_wchars = mbsnrtowcs(NULL, &(const char *){suff},
                                          suff_len, 0, &(mbstate_t){0});
    if (suff_len == (size_t)-1) {
        return false;
    }

    make_space(s, s->len + suff_wchars);
    mbsnrtowcs(&s->data[s->len], &suff, suff_len, suff_wchars, &(mbstate_t){0});
    s->len += suff_wchars;
    s->data[s->len] = L'\0';

    return true;
}

bool wstring_appendsz(struct wstring *s, const char *suff) {
    return wstring_appendsn(s, suff, strlen(suff));
}

bool wstring_appendwsn(struct wstring *s, const wchar_t *suff, size_t suff_len) {
    make_space(s, s->len + suff_len);
    wmemcpy(&s->data[s->len], suff, suff_len);
    s->len += suff_len;
    s->data[s->len] = L'\0';

    return true;
}

bool wstring_appendwsz(struct wstring *s, const wchar_t *suff) {
    return wstring_appendwsn(s, suff, wcslen(suff));
}

int wstring_printf(struct wstring *ws, const wchar_t *fmt, ...) {
    /* fuck this shitty libc API. Surely this is enough :clueless: */
    static wchar_t buf[1024];

    va_list args;
    va_start(args, fmt);
    const int len = vswprintf(buf, SIZEOF_ARRAY(buf), fmt, args);
    va_end(args);

    if (len < 0) {
        return -1;
    }

    make_space(ws, ws->len + len);
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

