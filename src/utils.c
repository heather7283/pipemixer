#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

#include <ncurses.h>
#include <spa/utils/string.h>

#include "utils.h"
#include "macros.h"

const char *key_name_from_key_code(wint_t code) {
    static char name[16];

    if (iswgraph(code)) {
        snprintf(name, sizeof(name), "%lc", code);
    } else {
        snprintf(name, sizeof(name), "0x%X", code);
    }

    return name;
}

/* TODO: this function is probably wrong in some way */
bool key_code_from_key_name(const char *name, wint_t *keycode) {
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    if (name[1] == '\0' && isgraph(name[0])) {
        /* printable ascii */
        *keycode = name[0];
        return true;
    } else if (name[2] == '\0' || name[3] == '\0' || name[4] == '\0') {
        /* a single utf-8 char */
        wchar_t res;
        int ret = mbrtowc(&res, name, 4, &(mbstate_t){0});
        if (ret > 0 && name[ret] == '\0' && iswgraph(res)) {
            *keycode = res;
            return true;
        }
    }

    static const struct {
        const char *name;
        wint_t keycode;
    } names[] = {
        {        "up", KEY_UP        },
        {      "down", KEY_DOWN      },
        {      "left", KEY_LEFT      },
        {     "right", KEY_RIGHT     },
        {     "enter", '\n'          },
        {       "tab", '\t'          },
        {   "backtab", KEY_BTAB      },
        {     "space", ' '           },
        { "backspace", KEY_BACKSPACE },
        {    "escape", '\e'          },
    };
    for (unsigned i = 0; i < SIZEOF_ARRAY(names); i++) {
        if (streq(name, names[i].name)) {
            *keycode = names[i].keycode;
            return true;
        }
    }

    const char prefix[] = "code:";
    if (STRSTARTSWITH(name, prefix)) {
        const char *num = name + strlen(prefix);
        uint32_t code;
        if (spa_atou32(num, &code, 10) && code < INT_MAX) {
            *keycode = code;
            return true;
        }
    }

    return false;
}

bool streq(const char *a, const char *b) {
    if (a == NULL || b == NULL) {
        return false;
    } else {
        return strcmp(a, b) == 0;
    }
}

bool strneq(const char *a, const char *b, size_t len) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return strncmp(a, b, len) == 0;
}

