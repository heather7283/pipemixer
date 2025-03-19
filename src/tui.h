#ifndef TUI_H
#define TUI_H

#include <ncurses.h>
#include <spa/utils/list.h>

#include "pw.h"
#include "thirdparty/event_loop.h"

struct tui {
    int term_height, term_width;

    WINDOW *bar_win;
    enum media_class active_tab;

    WINDOW *pad_win;
    int pad_pos;

    struct spa_list node_displays;
    struct tui_node_display *focused_node_display;

    bool needs_redo_layout;
    bool needs_resize;
};

struct tui_node_display {
    uint32_t node_id;
    WINDOW *win;

    int pos, height;
    bool focused;

    struct spa_list link;
};

extern struct tui tui;

int tui_init(void);
int tui_cleanup(void);

int tui_repaint_all(void);
int tui_create_layout(void);

int tui_update(struct event_loop_item *loop_item);
int tui_handle_resize(struct event_loop_item *item, int signal);
int tui_handle_keyboard(struct event_loop_item *item, uint32_t events);

/* true if focus changed */
bool tui_focus_next(void);
/* true if focus changed */
bool tui_focus_prev(void);

void tui_next_tab(void);

#endif /* #ifndef TUI_H */

