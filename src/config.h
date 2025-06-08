#ifndef CONFIG_H
#define CONFIG_H

#include <curses.h>

#include "tui.h"

struct pipemixer_config_bind {
    int key; /* ncurses keycode, see curs_getch(3x) */
    struct tui_bind value;
};

struct pipemixer_config {
    float volume_step;
    wchar_t bar_full_char[2], bar_empty_char[2];
    struct {
        /* see curs_border(3x) */
        wchar_t ls[2], rs[2], ts[2], bs[2], tl[2], tr[2], bl[2], br[2], lc[2], rc[2];
    } borders;
    struct pipemixer_config_bind *binds; /* stb_ds hashmap */
};

extern struct pipemixer_config config;

void load_config(const char *config_path);
void config_cleanup(void);

#endif /* #ifndef CONFIG_H */

