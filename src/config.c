#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>

#include "config.h"
#include "utils.h"
#include "macros.h"
#include "ini.h"
#include "thirdparty/stb_ds.h"

#define ADD_BIND(key, function, data_type, data_value) \
    do { \
        struct tui_bind bind; \
        bind.func = function; \
        bind.data.data_type = data_value; \
        stbds_hmput(config.binds, key, bind); \
    } while (0)

struct pipemixer_config config = {
    .volume_step = 0.01,
    .volume_min = 0.00,
    .volume_max = 1.50,

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

    .binds = NULL,
};

static const char *get_default_config_path(void) {
    static char path[PATH_MAX];

    const char *home = getenv("HOME");
    const char *xdg_config_home = getenv("XDG_CONFIG_HOME");

    if (xdg_config_home != NULL) {
        snprintf(path, sizeof(path), "%s/pipemixer/config.ini", xdg_config_home);
        return path;
    } else if (home != NULL) {
        snprintf(path, sizeof(path), "%s/.config/pipemixer/config.ini", home);
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

static int key_value_handler(void *data, const char *s, const char *k, const char *v, int l) {
    #define CONFIG_LOG(fmt, ...) \
        fprintf(stderr, "config:%d: (%s::%s) "fmt"\n", l, s, k, ##__VA_ARGS__)

    #define CONFIG_GET_WCHAR(dst) \
        if (!get_first_wchar(v, dst)) CONFIG_LOG("invalid or incomplete multibyte sequence")

    #define CONFIG_GET_PERCENTAGE(dst) \
        if (!get_percentage(v, dst)) CONFIG_LOG("invalid percentage value")

    if (STREQ(s, "main")) {
        if (STREQ(k, "volume-step")) {
            CONFIG_GET_PERCENTAGE(&config.volume_step);
        } else if (STREQ(k, "volume-min")) {
            CONFIG_GET_PERCENTAGE(&config.volume_min);
        } else if (STREQ(k, "volume-max")) {
            CONFIG_GET_PERCENTAGE(&config.volume_max);
        } else {
            CONFIG_LOG("unknown key %s in section %s", k, s);
        }
    } else if (STREQ(s, "interface")) {
        if (STREQ(k, "border-left")) {
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
        int keycode = key_code_from_key_name(v);

        if (keycode == ERR) {
            CONFIG_LOG("invalid keycode: %s", v);
        } else {
            const char *prefix = NULL;
            if (prefix = "focus-", STRSTARTSWITH(k, prefix)) {
                if (STREQ(k + strlen(prefix), "up")) {
                    ADD_BIND(keycode, tui_bind_change_focus, direction, UP);
                } else if (STREQ(k + strlen(prefix), "down")) {
                    ADD_BIND(keycode, tui_bind_change_focus, direction, DOWN);
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
                    ADD_BIND(keycode, tui_bind_change_tab, tab, NEXT);
                } else if (STREQ(k + strlen(prefix), "prev")) {
                    ADD_BIND(keycode, tui_bind_change_tab, tab, PREV);
                } else if (STREQ(k + strlen(prefix), "playback")) {
                    ADD_BIND(keycode, tui_bind_change_tab, tab, PLAYBACK);
                } else if (STREQ(k + strlen(prefix), "recording")) {
                    ADD_BIND(keycode, tui_bind_change_tab, tab, RECORDING);
                } else if (STREQ(k + strlen(prefix), "input-devices")) {
                    ADD_BIND(keycode, tui_bind_change_tab, tab, INPUT_DEVICES);
                } else if (STREQ(k + strlen(prefix), "output-devices")) {
                    ADD_BIND(keycode, tui_bind_change_tab, tab, OUTPUT_DEVICES);
                } else {
                    CONFIG_LOG("unknown action: %s", k);
                }
            } else if (STREQ(k, "quit")) {
                ADD_BIND(keycode, TUI_BIND_QUIT, nothing, NOTHING);
            } else {
                CONFIG_LOG("unknown action: %s", k);
            }
        }
    } else {
        CONFIG_LOG("unknown section %s", s);
    }

    return 0;

    #undef CONFIG_LOG
    #undef CONFIG_GET_WCHAR
}

static void parse_config(const char *config) {
    ini_parse_string(config, key_value_handler, NULL);
}

static void add_default_binds(void) {
    ADD_BIND('j', tui_bind_change_focus, direction, DOWN);
    ADD_BIND(KEY_DOWN, tui_bind_change_focus, direction, DOWN);
    ADD_BIND('k', tui_bind_change_focus, direction, UP);
    ADD_BIND(KEY_UP, tui_bind_change_focus, direction, UP);

    ADD_BIND('l', tui_bind_change_volume, direction, UP);
    ADD_BIND(KEY_RIGHT, tui_bind_change_volume, direction, UP);
    ADD_BIND('h', tui_bind_change_volume, direction, DOWN);
    ADD_BIND(KEY_LEFT, tui_bind_change_volume, direction, DOWN);

    ADD_BIND('t', tui_bind_change_tab, tab, NEXT);
    ADD_BIND('\t', tui_bind_change_tab, tab, NEXT);
    ADD_BIND('T', tui_bind_change_tab, tab, PREV);
    ADD_BIND(KEY_BTAB, tui_bind_change_tab, tab, PREV);

    ADD_BIND('1', tui_bind_change_tab, tab, PLAYBACK);
    ADD_BIND('2', tui_bind_change_tab, tab, RECORDING);
    ADD_BIND('3', tui_bind_change_tab, tab, INPUT_DEVICES);
    ADD_BIND('4', tui_bind_change_tab, tab, OUTPUT_DEVICES);

    ADD_BIND('m', tui_bind_change_mute, change_mode, TOGGLE);
    ADD_BIND(' ', tui_bind_change_channel_lock, change_mode, TOGGLE);

    ADD_BIND('q', TUI_BIND_QUIT, nothing, NOTHING);
}

void load_config(const char *config_path) {
    int fd = -1;
    char *config_str = NULL;

    if (config_path == NULL) {
        config_path = get_default_config_path();
    }

    if (config_path != NULL) {
        fd = open(config_path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "config: failed to open %s: %s\n", config_path, strerror(errno));
        }

        config_str = read_string_from_fd(fd, NULL);
        if (config_str == NULL) {
            fprintf(stderr, "config: failed to read config file: %s\n", strerror(errno));
        }

        close(fd);
    }

    add_default_binds();

    if (config_str != NULL) {
        parse_config(config_str);
        free(config_str);
    }
}

void config_cleanup(void) {
    stbds_hmfree(config.binds);
}

