#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>

#include <ini.h>
#include <spa/utils/string.h>

#include "xmalloc.h"
#include "config.h"
#include "utils.h"
#include "macros.h"

struct pipemixer_config config = {
    .volume_step = 0.01,
    .volume_min = 0.00,
    .volume_max = 1.50,

    .wraparound = false,
    .display_ids = false,

    .default_tab = PLAYBACK,

    .bar_full_char = L"#",
    .bar_empty_char = L"-",
    .volume_frame = {
        .tl = L"┌",
        .tr = L"┐",
        .bl = L"└",
        .br = L"┘",
        .cl = L"├",
        .cr = L"┤",
        .ml = L"╶",
        .mr = L"╴",
        .f  = L"─",
    },
    .borders = {
        .ls = L"│",
        .rs = L"│",
        .ts = L"─",
        .bs = L"─",
        .tl = L"┌",
        .tr = L"┐",
        .bl = L"└",
        .br = L"┘",
    },
    .routes_separator = L", ",
    .profiles_separator = L", ",
};

static void add_bind(uint32_t keycode, struct tui_bind _bind) {
    struct tui_bind *bind = xmalloc(sizeof(*bind));
    *bind = _bind;
    struct tui_bind *old_bind = map_insert(&config.binds, keycode, bind);
    if (old_bind) {
        free(old_bind);
    }
}

#define ADD_BIND(keycode, function, type, value) \
    add_bind((keycode), (struct tui_bind){ .func = (function), .data.type = (value) })

static const char *get_default_config_path(void) {
    static char path[PATH_MAX];

    const char *home = getenv("HOME");
    const char *xdg_config_home = getenv("XDG_CONFIG_HOME");

    if (xdg_config_home != NULL) {
        snprintf(path, sizeof(path), "%s/pipemixer/pipemixer.ini", xdg_config_home);
        return path;
    } else if (home != NULL) {
        snprintf(path, sizeof(path), "%s/.config/pipemixer/pipemixer.ini", home);
        return path;
    } else {
        /* config will be parsed before initalising curses so printfing here is fine */
        fprintf(stderr,
                "config: HOME and XDG_CONFIG_HOME are unset, cannot determine config path\n");
        return NULL;
    }
}

static bool tui_tab_type_from_name(const char *name, enum tui_tab_type *type) {
    if (streq(name, "playback")) *type = PLAYBACK;
    else if (streq(name, "recording")) *type = RECORDING;
    else if (streq(name, "input-devices")) *type = INPUT_DEVICES;
    else if (streq(name, "output-devices")) *type = OUTPUT_DEVICES;
    else if (streq(name, "cards")) *type = CARDS;
    else return false;

    return true;
}

struct parser_context {
    const char *sect, *key, *val;
};

#define PARSER_ERROR(ctx, fmt, ...) \
    fprintf(stderr, "config: (%s::%s) "fmt"\n", (ctx).sect, (ctx).key, ##__VA_ARGS__)

static bool percentage_parser(struct parser_context ctx, void *_out) {
    float *out = _out;

    uint32_t val;
    if (!spa_atou32(ctx.val, &val, 10)) {
        PARSER_ERROR(ctx, "invalid integer");
        return false;
    }

    *out = (float)val * 0.01;
    return true;
}

static bool tab_parser(struct parser_context ctx, void *_out) {
    enum tui_tab_type *out = _out;

    if (!tui_tab_type_from_name(ctx.val, out)) {
        PARSER_ERROR(ctx, "invalid tab name: %s", ctx.val);
        return false;
    }

    return true;
}

static bool tab_order_parser(struct parser_context ctx, void *_out) {
    enum tui_tab_type (*out)[TUI_TAB_TYPE_COUNT] = _out;

    char *str = xstrdup(ctx.val);
    enum tui_tab_type res[TUI_TAB_TYPE_COUNT];
    bool ret = true;

    bool any = false;
    bool seen[TUI_TAB_TYPE_COUNT];
    memset(seen, '\0', sizeof(seen));

    int index = 0;
    for (char *tok = strtok(str, ","); tok; tok = strtok(NULL, ",")) {
        enum tui_tab_type type;
        if (!tui_tab_type_from_name(tok, &type)) {
            PARSER_ERROR(ctx, "invalid tab name: %s", tok);
            ret = false;
            goto out;
        } else if (seen[type]) {
            PARSER_ERROR(ctx, "duplicate tab: %s", tok);
            ret = false;
            goto out;
        }

        any = true;
        seen[type] = true;
        res[index++] = type;
    }
    if (!any) {
        PARSER_ERROR(ctx, "no tabs specified");
        ret = false;
        goto out;
    }

    memcpy(out, res, sizeof(res));
    config.tabs_count = index;

out:
    free(str);
    return ret;
}

static bool wstring_parser(struct parser_context ctx, void *_out) {
    wchar_t **out = _out;

    const size_t wlen = mbsrtowcs(NULL, (const char **){&ctx.val}, 0, &(mbstate_t){0});
    if (wlen == (size_t)-1) {
        PARSER_ERROR(ctx, "invalid string");
        return false;
    }

    *out = xmalloc((wlen + 1) * sizeof((*out)[0]));
    mbsrtowcs(*out, (const char **){&ctx.val}, wlen + 1, &(mbstate_t){0});
    (*out)[wlen] = L'\0';
    return true;
}

static bool wchar_parser(struct parser_context ctx, void *_out) {
    wchar_t *out = _out;

    const size_t len = strlen(ctx.val);

    const size_t ret = mbrtowc(out, ctx.val, len, &(mbstate_t){0});
    if (ret == (size_t)-1 || ret == (size_t)-2) {
        PARSER_ERROR(ctx, "invalid character");
        return false;
    } else if (len > ret) {
        PARSER_ERROR(ctx, "too many characters");
        return false;
    }

    return true;
}

static bool bool_parser(struct parser_context ctx, void *_out) {
    bool *out = _out;

    if (streq(ctx.val, "1") || streq(ctx.val, "yes") || streq(ctx.val, "true")) {
        *out = true;
        return true;
    } else if (streq(ctx.val, "0") || streq(ctx.val, "no") || streq(ctx.val, "false")) {
        *out = false;
        return true;
    } else {
        PARSER_ERROR(ctx, "invalid boolean: %s", ctx.val);
        return false;
    }
}

struct key_handler {
    const char *key;
    bool (*parser)(struct parser_context ctx, void *out);
    void *out;
};

struct section_handler {
    const char *section;
    const struct key_handler *key_handlers;
};

static const struct section_handler section_handlers[] = {
    {
        "main", (const struct key_handler[]){
            { "volume-step", percentage_parser, &config.volume_step },
            { "volume-min", percentage_parser, &config.volume_min },
            { "volume-max", percentage_parser, &config.volume_max },
            { "wraparound", bool_parser, &config.wraparound },
            { "display-ids", bool_parser, &config.display_ids },
            { "tab-order", tab_order_parser, &config.tabs },
            { "default-tab", tab_parser, &config.default_tab },
            { 0 }
        }
    },
    {
        "interface", (const struct key_handler[]){
            { "routes-separator", wstring_parser, &config.routes_separator },
            { "profiles-separator", wstring_parser, &config.profiles_separator },
            { "border-left", wchar_parser, &config.borders.ls[0] },
            { "border-right", wchar_parser, &config.borders.rs[0] },
            { "border-top", wchar_parser, &config.borders.ts[0] },
            { "border-bottom", wchar_parser, &config.borders.bs[0] },
            { "border-top-left", wchar_parser, &config.borders.tl[0] },
            { "border-top-right", wchar_parser, &config.borders.tr[0] },
            { "border-bottom-left", wchar_parser, &config.borders.bl[0] },
            { "border-bottom-right", wchar_parser, &config.borders.br[0] },
            { "volume-frame-center-left", wchar_parser, &config.volume_frame.cl[0] },
            { "volume-frame-center-right", wchar_parser, &config.volume_frame.cr[0] },
            { "volume-frame-top-left", wchar_parser, &config.volume_frame.tl[0] },
            { "volume-frame-top-right", wchar_parser, &config.volume_frame.tr[0] },
            { "volume-frame-bottom-left", wchar_parser, &config.volume_frame.bl[0] },
            { "volume-frame-bottom-right", wchar_parser, &config.volume_frame.br[0] },
            { "volume-frame-mono-left", wchar_parser, &config.volume_frame.ml[0] },
            { "volume-frame-mono-right", wchar_parser, &config.volume_frame.mr[0] },
            { "volume-frame-focus", wchar_parser, &config.volume_frame.f[0] },
            { "bar-full-char", wchar_parser, &config.bar_full_char[0] },
            { "bar-empty-char", wchar_parser, &config.bar_empty_char[0] },
            { 0 }
        }
    },
};

static bool parse_bind(struct parser_context ctx) {
    wint_t keycode;
    if (!key_code_from_key_name(ctx.val, &keycode)) {
        PARSER_ERROR(ctx, "invalid key");
        return false;
    }

    if (streq(ctx.key, "unbind")) {
        struct tui_bind *bind = map_remove(&config.binds, keycode);
        if (bind) {
            free(bind);
        }
        return true;
    }

    const char *suffix;
    if (cut_prefix(ctx.key, "focus-", &suffix)) {
        if (streq(suffix, "up")) {
            ADD_BIND(keycode, tui_bind_change_focus, direction, UP);
        } else if (streq(suffix, "down")) {
            ADD_BIND(keycode, tui_bind_change_focus, direction, DOWN);
        } else if (streq(suffix, "first")) {
            ADD_BIND(keycode, tui_bind_focus_first, nothing, NOTHING);
        } else if (streq(suffix, "last")) {
            ADD_BIND(keycode, tui_bind_focus_last, nothing, NOTHING);
        } else {
            goto bad;
        }
    } else if (cut_prefix(ctx.key, "volume-set-", &suffix)) {
        uint32_t vol;
        if (spa_atou32(suffix, &vol, 10)) {
            ADD_BIND(keycode, tui_bind_set_volume, volume, (float)vol * 0.01);
        } else {
            goto bad;
        }
    } else if (cut_prefix(ctx.key, "volume-", &suffix)) {
        if (streq(suffix, "up")) {
            ADD_BIND(keycode, tui_bind_change_volume, direction, UP);
        } else if (streq(suffix, "down")) {
            ADD_BIND(keycode, tui_bind_change_volume, direction, DOWN);
        } else {
            goto bad;
        }
    } else if (cut_prefix(ctx.key, "mute-", &suffix)) {
        if (streq(suffix, "enable")) {
            ADD_BIND(keycode, tui_bind_change_mute, change_mode, ENABLE);
        } else if (streq(suffix, "disable")) {
            ADD_BIND(keycode, tui_bind_change_mute, change_mode, DISABLE);
        } else if (streq(suffix, "toggle")) {
            ADD_BIND(keycode, tui_bind_change_mute, change_mode, TOGGLE);
        } else {
            goto bad;
        }
    } else if (cut_prefix(ctx.key, "channel-lock-", &suffix)) {
        if (streq(suffix, "enable")) {
            ADD_BIND(keycode, tui_bind_change_channel_lock, change_mode, ENABLE);
        } else if (streq(suffix, "disable")) {
            ADD_BIND(keycode, tui_bind_change_channel_lock, change_mode, DISABLE);
        } else if (streq(suffix, "toggle")) {
            ADD_BIND(keycode, tui_bind_change_channel_lock, change_mode, TOGGLE);
        } else {
            goto bad;
        }
    } else if (cut_prefix(ctx.key, "tab-", &suffix)) {
        uint32_t tab_index;
        enum tui_tab_type tab_type;
        if (streq(suffix, "next")) {
            ADD_BIND(keycode, tui_bind_change_tab, direction, UP);
        } else if (streq(suffix, "prev")) {
            ADD_BIND(keycode, tui_bind_change_tab, direction, DOWN);
        } else if (tui_tab_type_from_name(suffix, &tab_type)) {
            ADD_BIND(keycode, tui_bind_set_tab, tab, tab_type);
        } else if (spa_atou32(suffix, &tab_index, 10) && tab_index > 0) {
            ADD_BIND(keycode, tui_bind_set_tab_index, index, tab_index - 1);
        } else {
            goto bad;
        }
    } else if (streq(ctx.key, "set-default")) {
        ADD_BIND(keycode, tui_bind_set_default, nothing, NOTHING);
    } else if (streq(ctx.key, "select-route")) {
        ADD_BIND(keycode, tui_bind_select_route, nothing, NOTHING);
    } else if (streq(ctx.key, "select-profile")) {
        ADD_BIND(keycode, tui_bind_select_profile, nothing, NOTHING);
    } else if (streq(ctx.key, "confirm-selection")) {
        ADD_BIND(keycode, tui_bind_confirm_selection, nothing, NOTHING);
    } else if (streq(ctx.key, "cancel-selection")) {
        ADD_BIND(keycode, tui_bind_cancel_selection, nothing, NOTHING);
    } else if (streq(ctx.key, "quit-or-cancel-selection")) {
        ADD_BIND(keycode, tui_bind_quit_or_cancel_selection, nothing, NOTHING);
    } else if (streq(ctx.key, "quit")) {
        ADD_BIND(keycode, tui_bind_quit, nothing, NOTHING);
    } else {
        goto bad;
    }

    return true;

bad:
    PARSER_ERROR(ctx, "invalid action");
    return false;
}

static bool parse(struct parser_context ctx) {
    if (streq(ctx.sect, "binds")) {
        return parse_bind(ctx);
    }

    const struct section_handler *section_handler = NULL;
    for (unsigned i = 0; i < SIZEOF_ARRAY(section_handlers); i++) {
        if (streq(ctx.sect, section_handlers[i].section)) {
            section_handler = &section_handlers[i];
            break;
        }
    }
    if (!section_handler) {
        PARSER_ERROR(ctx, "unknown section");
        return false;
    }

    const struct key_handler *key_handler = NULL;
    for (unsigned i = 0; section_handler->key_handlers[i].key; i++) {
        if (streq(ctx.key, section_handler->key_handlers[i].key)) {
            key_handler = &section_handler->key_handlers[i];
            break;
        }
    }
    if (!key_handler) {
        PARSER_ERROR(ctx, "unknown key");
        return false;
    }

    return key_handler->parser(ctx, key_handler->out);
}

static int key_value_handler(void *_, const char *s, const char *k, const char *v) {
    return parse((struct parser_context){ s, k, v });
}

static void load_default_config(void) {
    static const char default_config[] =
        "[main]\n"
        "tab-order=playback,recording,output-devices,input-devices,cards\n"
        "[binds]\n"
        "focus-down=j\n"
        "focus-down=down\n"
        "focus-up=k\n"
        "focus-up=up\n"
        "focus-first=g\n"
        "focus-last=G\n"
        "volume-up=l\n"
        "volume-up=right\n"
        "volume-down=h\n"
        "volume-down=left\n"
        "tab-next=t\n"
        "tab-next=tab\n"
        "tab-prev=T\n"
        "tab-prev=backtab\n"
        "tab-1=1\n"
        "tab-2=2\n"
        "tab-3=3\n"
        "tab-4=4\n"
        "tab-5=5\n"
        "mute-toggle=m\n"
        "channel-lock-toggle=space\n"
        "select-route=p\n"
        "select-profile=P\n"
        "set-default=D\n"
        "confirm-selection=enter\n"
        "quit-or-cancel-selection=escape\n"
        "quit=q\n"
    ;

    ini_parse_string(default_config, key_value_handler, NULL);
}

void load_config(const char *config_path) {
    load_default_config();

    config_path = config_path ?: get_default_config_path();
    if (config_path) {
        switch (ini_parse(config_path, key_value_handler, NULL)) {
        case -1:
            fprintf(stderr, "config: failed to open config file at %s", config_path);
            break;
        case -2:
            fprintf(stderr, "config: memory allocation failure while parsing config");
            break;
        };
    }
}

