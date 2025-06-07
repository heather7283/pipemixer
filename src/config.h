#ifndef CONFIG_H
#define CONFIG_H

#include <curses.h>

struct pipemixer_config {
    struct {
        /* see curs_border(3x) */
        wchar_t ls[2], rs[2], ts[2], bs[2], tl[2], tr[2], bl[2], br[2];
    } borders;
};

extern struct pipemixer_config config;

void load_config(const char *config_path);

#endif /* #ifndef CONFIG_H */

