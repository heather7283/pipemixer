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
    }
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

static int key_value_handler(void *data, const char *s, const char *k, const char *v, int l) {
    #define CONFIG_LOG(fmt, ...) \
        fprintf(stderr, "config:%d:%s:%s: "fmt"\n", l, s, k, ##__VA_ARGS__)

    #define CONFIG_GET_WCHAR(dst) \
        do { \
            size_t len = strlen(v); \
            mbtowc(NULL, NULL, 0); /* reset mbtowc state */ \
            if (mbtowc((dst), v, len) < 1) { \
                CONFIG_LOG("invalid character sequence"); \
            } \
        } while (0)

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
            goto out;
        }

        config_str = read_string_from_fd(fd, NULL);
        if (config_str == NULL) {
            fprintf(stderr, "config: failed to read config file: %s\n", strerror(errno));
            goto out;
        }
    }

    parse_config(config_str);

out:
    if (fd > 0) {
        close(fd);
    }
    free(config_str);
}

