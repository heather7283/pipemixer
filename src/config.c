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

struct pipemixer_config config = {
    .volume_step = 0.01,
    .borders = {
        .ls = L"│",
        .rs = L"│",
        .ts = L"─",
        .bs = L"─",
        .tl = L"┌",
        .tr = L"┐",
        .bl = L"└",
        .br = L"┘",
        .lc = L"├",
        .rc = L"┤",
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

static bool str_to_ulong(const char *str, unsigned long *res) {
    char *endptr = NULL;

    errno = 0;
    unsigned long res_tmp = strtoul(str, &endptr, 10);

    if (errno == 0 && *endptr == '\0') {
        *res = res_tmp;
        return true;
    } else {
        return false;
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

static int key_value_handler(void *data, const char *s, const char *k, const char *v, int l) {
    #define CONFIG_LOG(fmt, ...) \
        fprintf(stderr, "config:%d:%s:%s: "fmt"\n", l, s, k, ##__VA_ARGS__)

    #define CONFIG_GET_WCHAR(dst) \
        if (!get_first_wchar(v, dst)) CONFIG_LOG("invalid or incomplete multibyte sequence")

    if (STREQ(s, "main")) {
        if (STREQ(k, "volume-step")) {
            unsigned long step;
            if (str_to_ulong(v, &step) && step > 0) {
                config.volume_step = (float)step * 0.01;
            } else {
                CONFIG_LOG("%s is not a positive integer", v);
            }
        } else {
            CONFIG_LOG("unknown key %s in section %s", k, s);
        }
    } else if (STREQ(s, "borders")) {
        if (STREQ(k, "left")) {
            CONFIG_GET_WCHAR(&config.borders.ls[0]);
        } else if (STREQ(k, "right")) {
            CONFIG_GET_WCHAR(&config.borders.rs[0]);
        } else if (STREQ(k, "top")) {
            CONFIG_GET_WCHAR(&config.borders.ts[0]);
        } else if (STREQ(k, "bottom")) {
            CONFIG_GET_WCHAR(&config.borders.bs[0]);
        } else if (STREQ(k, "top-left")) {
            CONFIG_GET_WCHAR(&config.borders.tl[0]);
        } else if (STREQ(k, "top-right")) {
            CONFIG_GET_WCHAR(&config.borders.tr[0]);
        } else if (STREQ(k, "bottom-left")) {
            CONFIG_GET_WCHAR(&config.borders.bl[0]);
        } else if (STREQ(k, "bottom-right")) {
            CONFIG_GET_WCHAR(&config.borders.br[0]);
        } else if (STREQ(k, "center-left")) {
            CONFIG_GET_WCHAR(&config.borders.lc[0]);
        } else if (STREQ(k, "center-right")) {
            CONFIG_GET_WCHAR(&config.borders.rc[0]);
        } else {
            CONFIG_LOG("unknown key %s in section %s", k, s);
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
    #define ADD_BIND(key, function, data_type, data_value) \
        do { \
            struct tui_bind bind; \
            bind.func = function; \
            bind.data.data_type = data_value; \
            stbds_hmput(config.binds, key, bind); \
        } while (0)

    ADD_BIND('j', tui_change_focus, direction, DOWN);
    ADD_BIND(KEY_DOWN, tui_change_focus, direction, DOWN);
    ADD_BIND('k', tui_change_focus, direction, UP);
    ADD_BIND(KEY_UP, tui_change_focus, direction, UP);

    ADD_BIND('l', tui_change_volume, direction, UP);
    ADD_BIND(KEY_RIGHT, tui_change_volume, direction, UP);
    ADD_BIND('h', tui_change_volume, direction, DOWN);
    ADD_BIND(KEY_LEFT, tui_change_volume, direction, DOWN);

    ADD_BIND('t', tui_change_tab, tab, NEXT);
    ADD_BIND('\t', tui_change_tab, tab, NEXT);
    ADD_BIND('T', tui_change_tab, tab, PREV);
    ADD_BIND(KEY_BTAB, tui_change_tab, tab, PREV);

    ADD_BIND('1', tui_change_tab, tab, PLAYBACK);
    ADD_BIND('2', tui_change_tab, tab, RECORDING);
    ADD_BIND('3', tui_change_tab, tab, INPUT_DEVICES);
    ADD_BIND('4', tui_change_tab, tab, OUTPUT_DEVICES);

    ADD_BIND('m', tui_change_mute, change_mode, TOGGLE);
    ADD_BIND(' ', tui_change_channel_lock, change_mode, TOGGLE);

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

