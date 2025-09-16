#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>

#include <ini.h>

#include "xmalloc.h"
#include "tui.h"
#include "config.h"
#include "utils.h"
#include "macros.h"

#define ADD_BIND(keycode, function, data_type, data_value) \
    do { \
        struct tui_bind bind = { \
            .func = function, \
            .data.data_type = data_value, \
        }; \
        MAP_INSERT(&config.binds, keycode, &bind); \
    } while (0)

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
    .routes_separator = ", ",
};

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

static bool get_first_wchar(const char *str, wchar_t *res) {
    size_t len = strlen(str);
    wchar_t res_tmp;

    mbtowc(NULL, NULL, 0); /* reset mbtowc state */
    if (mbtowc(&res_tmp, str, len) < 1) {
        return false;
    }

    *res = res_tmp;
    return true;
}

static bool get_percentage(const char *str, float *res) {
    unsigned long tmp;

    if (!str_to_ulong(str, &tmp)) {
        return false;
    }

    *res = (float)tmp * 0.01;
    return true;
}

static bool get_bool(const char *str, bool *res) {
    if (STREQ(str, "1") || STRCASEEQ(str, "yes") || STRCASEEQ(str, "true")) {
        *res = true;
        return true;
    } else if (STREQ(str, "0") || STRCASEEQ(str, "no") || STRCASEEQ(str, "false")) {
        *res = false;
        return true;
    } else {
        return false;
    }
}

static enum tui_tab_type tui_tab_from_name(const char *name) {
    if (STRCASEEQ(name, "playback")) return PLAYBACK;
    else if (STRCASEEQ(name, "recording")) return RECORDING;
    else if (STRCASEEQ(name, "input-devices")) return INPUT_DEVICES;
    else if (STRCASEEQ(name, "output-devices")) return OUTPUT_DEVICES;
    else if (STRCASEEQ(name, "cards")) return CARDS;
    else return TUI_TAB_INVALID;
}

static bool get_tab_order(const char *_str) {
    char *str = xstrdup(_str);
    bool ret = true;
    int map_index_to_enum[TUI_TAB_COUNT];
    int map_enum_to_index[TUI_TAB_COUNT];
    bool seen[TUI_TAB_COUNT];

    memset(seen, false, sizeof(seen));

    int index = 0;
    for (char *tok = strtok(str, ","); tok != NULL; tok = strtok(NULL, ",")) {
        const enum tui_tab_type tab = tui_tab_from_name(tok);
        if (tab == TUI_TAB_INVALID) {
            ret = false;
            goto out;
        }

        seen[tab] = true;
        map_index_to_enum[index] = tab;
        map_enum_to_index[tab] = index;
        index += 1;
    }

    for (int i = 0; i < TUI_TAB_COUNT; i++) {
        if (!seen[i]) {
            ret = false;
            goto out;
        }
    }

    memcpy(&config.tab_map_index_to_enum, map_index_to_enum, sizeof(config.tab_map_index_to_enum));
    memcpy(&config.tab_map_enum_to_index, map_enum_to_index, sizeof(config.tab_map_enum_to_index));

out:
    free(str);
    return ret;
}

static int key_value_handler(void *data, const char *s, const char *k, const char *v) {
    #define CONFIG_LOG(fmt, ...) \
        fprintf(stderr, "config: (%s::%s) "fmt"\n", s, k, ##__VA_ARGS__)

    #define CONFIG_GET_WCHAR(dst) \
        if (!get_first_wchar(v, dst)) CONFIG_LOG("invalid or incomplete multibyte sequence")

    #define CONFIG_GET_PERCENTAGE(dst) \
        if (!get_percentage(v, dst)) CONFIG_LOG("invalid percentage value")

    #define CONFIG_GET_BOOL(dst) \
        if (!get_bool(v, dst)) CONFIG_LOG("invalid boolean: %s", v)

    if (STREQ(s, "main")) {
        if (STREQ(k, "volume-step")) {
            CONFIG_GET_PERCENTAGE(&config.volume_step);
        } else if (STREQ(k, "volume-min")) {
            CONFIG_GET_PERCENTAGE(&config.volume_min);
        } else if (STREQ(k, "volume-max")) {
            CONFIG_GET_PERCENTAGE(&config.volume_max);
        } else if (STREQ(k, "wraparound")) {
            CONFIG_GET_BOOL(&config.wraparound);
        } else if (STREQ(k, "display-ids")) {
            CONFIG_GET_BOOL(&config.display_ids);
        } else if (STREQ(k, "tab-order")) {
            if (!get_tab_order(v)) CONFIG_LOG("invalid tab order string: %s", v);
        } else if (STREQ(k, "default-tab")) {
            enum tui_tab_type tab = tui_tab_from_name(v);
            if (tab == TUI_TAB_INVALID) {
                CONFIG_LOG("invalid tab name: %s", v);
            } else {
                config.default_tab = tab;
            }
        } else {
            CONFIG_LOG("unknown key %s in section %s", k, s);
        }
    } else if (STREQ(s, "interface")) {
        if (STREQ(k, "ports-separator")) {
            config.routes_separator = xstrdup(v);
        } else if (STREQ(k, "border-left")) {
            CONFIG_GET_WCHAR(&config.borders.ls[0]);
        } else if (STREQ(k, "border-right")) {
            CONFIG_GET_WCHAR(&config.borders.rs[0]);
        } else if (STREQ(k, "border-top")) {
            CONFIG_GET_WCHAR(&config.borders.ts[0]);
        } else if (STREQ(k, "border-bottom")) {
            CONFIG_GET_WCHAR(&config.borders.bs[0]);
        } else if (STREQ(k, "border-top-left")) {
            CONFIG_GET_WCHAR(&config.borders.tl[0]);
        } else if (STREQ(k, "border-top-right")) {
            CONFIG_GET_WCHAR(&config.borders.tr[0]);
        } else if (STREQ(k, "border-bottom-left")) {
            CONFIG_GET_WCHAR(&config.borders.bl[0]);
        } else if (STREQ(k, "border-bottom-right")) {
            CONFIG_GET_WCHAR(&config.borders.br[0]);
        } else if (STREQ(k, "volume-frame-center-left")) {
            CONFIG_GET_WCHAR(&config.volume_frame.cl[0]);
        } else if (STREQ(k, "volume-frame-center-right")) {
            CONFIG_GET_WCHAR(&config.volume_frame.cr[0]);
        } else if (STREQ(k, "volume-frame-top-left")) {
            CONFIG_GET_WCHAR(&config.volume_frame.tl[0]);
        } else if (STREQ(k, "volume-frame-top-right")) {
            CONFIG_GET_WCHAR(&config.volume_frame.tr[0]);
        } else if (STREQ(k, "volume-frame-bottom-left")) {
            CONFIG_GET_WCHAR(&config.volume_frame.bl[0]);
        } else if (STREQ(k, "volume-frame-bottom-right")) {
            CONFIG_GET_WCHAR(&config.volume_frame.br[0]);
        } else if (STREQ(k, "volume-frame-mono-left")) {
            CONFIG_GET_WCHAR(&config.volume_frame.ml[0]);
        } else if (STREQ(k, "volume-frame-mono-right")) {
            CONFIG_GET_WCHAR(&config.volume_frame.mr[0]);
        } else if (STREQ(k, "volume-frame-focus")) {
            CONFIG_GET_WCHAR(&config.volume_frame.f[0]);
        } else if (STREQ(k, "bar-full-char")) {
            CONFIG_GET_WCHAR(&config.bar_full_char[0]);
        } else if (STREQ(k, "bar-empty-char")) {
            CONFIG_GET_WCHAR(&config.bar_empty_char[0]);
        } else {
            CONFIG_LOG("unknown key %s in section %s", k, s);
        }
    } else if (STREQ(s, "binds")) {
        wint_t keycode;

        if (!key_code_from_key_name(v, &keycode)) {
            CONFIG_LOG("invalid keycode: %s", v);
        } else {
            const char *prefix = NULL;
            if (prefix = "focus-", STRSTARTSWITH(k, prefix)) {
                if (STREQ(k + strlen(prefix), "up")) {
                    ADD_BIND(keycode, tui_bind_change_focus, direction, UP);
                } else if (STREQ(k + strlen(prefix), "down")) {
                    ADD_BIND(keycode, tui_bind_change_focus, direction, DOWN);
                } else if (STREQ(k + strlen(prefix), "first")) {
                    ADD_BIND(keycode, tui_bind_focus_first, nothing, NOTHING);
                } else if (STREQ(k + strlen(prefix), "last")) {
                    ADD_BIND(keycode, tui_bind_focus_last, nothing, NOTHING);
                } else {
                    CONFIG_LOG("unknown action: %s", k);
                }
            } else if (prefix = "volume-set-", STRSTARTSWITH(k, prefix)) {
                const char *vol_str = k + strlen(prefix);
                unsigned long vol;
                if (!str_to_ulong(vol_str, &vol)) {
                    CONFIG_LOG("%s is not a valid integer", vol_str);
                } else {
                    ADD_BIND(keycode, tui_bind_set_volume, volume, (float)vol * 0.01);
                }
            } else if (prefix = "volume-", STRSTARTSWITH(k, prefix)) {
                if (STREQ(k + strlen(prefix), "up")) {
                    ADD_BIND(keycode, tui_bind_change_volume, direction, UP);
                } else if (STREQ(k + strlen(prefix), "down")) {
                    ADD_BIND(keycode, tui_bind_change_volume, direction, DOWN);
                } else {
                    CONFIG_LOG("unknown action: %s", k);
                }
            } else if (prefix = "mute-", STRSTARTSWITH(k, prefix)) {
                if (STREQ(k + strlen(prefix), "enable")) {
                    ADD_BIND(keycode, tui_bind_change_mute, change_mode, ENABLE);
                } else if (STREQ(k + strlen(prefix), "disable")) {
                    ADD_BIND(keycode, tui_bind_change_mute, change_mode, DISABLE);
                } else if (STREQ(k + strlen(prefix), "toggle")) {
                    ADD_BIND(keycode, tui_bind_change_mute, change_mode, TOGGLE);
                } else {
                    CONFIG_LOG("unknown action: %s", k);
                }
            } else if (prefix = "channel-lock-", STRSTARTSWITH(k, prefix)) {
                if (STREQ(k + strlen(prefix), "enable")) {
                    ADD_BIND(keycode, tui_bind_change_channel_lock, change_mode, ENABLE);
                } else if (STREQ(k + strlen(prefix), "disable")) {
                    ADD_BIND(keycode, tui_bind_change_channel_lock, change_mode, DISABLE);
                } else if (STREQ(k + strlen(prefix), "toggle")) {
                    ADD_BIND(keycode, tui_bind_change_channel_lock, change_mode, TOGGLE);
                } else {
                    CONFIG_LOG("unknown action: %s", k);
                }
            } else if (prefix = "tab-", STRSTARTSWITH(k, prefix)) {
                if (STREQ(k + strlen(prefix), "next")) {
                    ADD_BIND(keycode, tui_bind_change_tab, direction, UP);
                } else if (STREQ(k + strlen(prefix), "prev")) {
                    ADD_BIND(keycode, tui_bind_change_tab, direction, DOWN);
                } else if (STREQ(k + strlen(prefix), "playback")) {
                    ADD_BIND(keycode, tui_bind_set_tab, tab, PLAYBACK);
                } else if (STREQ(k + strlen(prefix), "recording")) {
                    ADD_BIND(keycode, tui_bind_set_tab, tab, RECORDING);
                } else if (STREQ(k + strlen(prefix), "input-devices")) {
                    ADD_BIND(keycode, tui_bind_set_tab, tab, INPUT_DEVICES);
                } else if (STREQ(k + strlen(prefix), "output-devices")) {
                    ADD_BIND(keycode, tui_bind_set_tab, tab, OUTPUT_DEVICES);
                } else {
                    uint32_t index;
                    if (str_to_u32(k + strlen(prefix), &index)
                        && index >= 1 && index <= TUI_TAB_COUNT) {
                        ADD_BIND(keycode, tui_bind_set_tab_index, index, index - 1);
                    } else {
                        CONFIG_LOG("unknown action: %s", k);
                    }
                }
            } else if (STREQ(k, "select-route")) {
                ADD_BIND(keycode, tui_bind_select_route, nothing, NOTHING);
            } else if (STREQ(k, "select-profile")) {
                ADD_BIND(keycode, tui_bind_select_profile, nothing, NOTHING);
            } else if (STREQ(k, "confirm-selection")) {
                ADD_BIND(keycode, tui_bind_confirm_selection, nothing, NOTHING);
            } else if (STREQ(k, "cancel-selection")) {
                ADD_BIND(keycode, tui_bind_cancel_selection, nothing, NOTHING);
            } else if (STREQ(k, "quit")) {
                ADD_BIND(keycode, TUI_BIND_QUIT, nothing, NOTHING);
            } else if (STREQ(k, "unbind")) {
                MAP_REMOVE(&config.binds, keycode);
            } else {
                CONFIG_LOG("unknown action: %s", k);
            }
        }
    } else {
        CONFIG_LOG("unknown section %s", s);
    }

    return 0;

    #undef CONFIG_GET_BOOL
    #undef CONFIG_GET_PERCENTAGE
    #undef CONFIG_GET_WCHAR
    #undef CONFIG_LOG
}

static void parse_config(const char *config) {
    ini_parse_string(config, key_value_handler, NULL);
}

static void add_default_config(void) {
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
        "confirm-selection=enter\n"
        "cancel-selection=escape\n"
        "quit=q\n";

    parse_config(default_config);
}

void load_config(const char *config_path) {
    int fd = -1;
    char *config_str = NULL;

    add_default_config();

    if (config_path == NULL) {
        config_path = get_default_config_path();
    }

    if (config_path != NULL) {
        fd = open(config_path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "config: failed to open %s: %s\n", config_path, strerror(errno));
            goto out;
        }

        config_str = read_string_from_fd(fd, NULL);
        if (config_str == NULL) {
            fprintf(stderr, "config: failed to read config file: %s\n", strerror(errno));
            goto out;
        }

        parse_config(config_str);
    }

out:
    if (fd > 0) {
        close(fd);
    }
    if (config_str != NULL) {
        free(config_str);
    }
}

