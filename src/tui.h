#ifndef TUI_H
#define TUI_H

#include <ncurses.h>
#include <spa/utils/list.h>

#include "pipemixer.h"

#define PAD_SIZE 1000 /* number of lines in the pad */
#define MAX_SCREEN_WIDTH 512  /* surely nobody will have terminal window wider than that */

enum tui_active_tab {
    SOURCES, SINKS, OUTPUT_STREAMS, INPUT_STREAMS,
};

struct tui {
    int term_height, term_width;

    WINDOW *bar_win;
    enum tui_active_tab active_tab;

    WINDOW *pad_win;
    int pad_pos;

    struct spa_list node_displays;

    bool needs_redo_layout;
    bool needs_resize;
};

struct tui_node_display {
    uint32_t node_id;
    WINDOW *win;

    struct spa_list link;
};

extern struct tui tui;

int tui_init(void);
int tui_cleanup(void);

int tui_repaint_all(void);
int tui_handle_resize(void);
int tui_create_layout(void);

#endif /* #ifndef TUI_H */

