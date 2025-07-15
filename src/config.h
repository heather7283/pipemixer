#ifndef CONFIG_H
#define CONFIG_H

#include <curses.h>

#include "collections.h"

struct pipemixer_config {
    float volume_step;
    float volume_min, volume_max;

    bool wraparound;
    bool display_ids;

    wchar_t bar_full_char[2], bar_empty_char[2];
    struct {
        wchar_t tl[2], tr[2], bl[2], br[2], cl[2], cr[2], ml[2], mr[2], f[2];
    } volume_frame;
    struct {
        /* see curs_border(3x) */
        wchar_t ls[2], rs[2], ts[2], bs[2], tl[2], tr[2], bl[2], br[2];
    } borders;

    HASHMAP_HEAD(64) binds;
};

extern struct pipemixer_config config;

void load_config(const char *config_path);
void config_cleanup(void);

#endif /* #ifndef CONFIG_H */

