#ifndef CONFIG_H
#define CONFIG_H

#include <curses.h>

#include "collections/map.h"
#include "tui.h"

struct pipemixer_config {
    float volume_step;
    float volume_min, volume_max;

    /* value of 60 means use 60% of available screen width */
    int volume_bar_width_percentage;
    /* upper limit of volume bar */
    float volume_display_max;

    bool wraparound;
    bool display_ids;

    enum tui_tab_type default_tab;

    wchar_t bar_full_char[2], bar_empty_char[2];
    struct {
        wchar_t tl[2], tr[2], bl[2], br[2], cl[2], cr[2], ml[2], mr[2], f[2];
    } volume_frame;
    struct {
        /* see curs_border(3x) */
        wchar_t ls[2], rs[2], ts[2], bs[2], tl[2], tr[2], bl[2], br[2];
    } borders;
    char *routes_separator;

    /* tab_map_index_to_enum[i] returns enum tui_tab, i is tab position in the ui */
    enum tui_tab_type tab_map_index_to_enum[TUI_TAB_COUNT];
    /* tab_map_enum_to_index[enum tui_tab] returns i, i is tab position in the ui */
    int tab_map_enum_to_index[TUI_TAB_COUNT];

    MAP(struct tui_bind) binds;
};

extern struct pipemixer_config config;

void load_config(const char *config_path);

#endif /* #ifndef CONFIG_H */

