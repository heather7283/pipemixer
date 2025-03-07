#ifndef TUI_H
#define TUI_H

#include <ncurses.h>

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
};

extern struct tui tui;

void tui_repaint_all(void);

#endif /* #ifndef TUI_H */

