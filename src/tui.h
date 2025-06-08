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
    struct tui_node_display *focused;

    bool need_redo_layout;
};

struct tui_node_display {
    struct node *node;

    WINDOW *win;

    int pos, height;
    bool focused;
    bool focus_changed;

    bool unlocked_channels;
    uint32_t focused_channel;
    bool unlocked_channels_changed;

    struct spa_list link;
};

extern struct tui tui;

int tui_init(void);
int tui_cleanup(void);

int tui_update(struct event_loop_item *loop_item);
int tui_handle_resize(struct event_loop_item *item, int signal);
int tui_handle_keyboard(struct event_loop_item *item, uint32_t events);

/* binds */
union tui_bind_data;
typedef void (*tui_bind_func_t)(union tui_bind_data data);

enum tui_direction { UP, DOWN };
void tui_bind_change_focus(union tui_bind_data data);
void tui_bind_change_volume(union tui_bind_data data);

enum tui_change_mode { ENABLE, DISABLE, TOGGLE };
void tui_bind_change_mute(union tui_bind_data data);
void tui_bind_change_channel_lock(union tui_bind_data data);

enum tui_tab { NEXT, PREV, PLAYBACK, RECORDING, INPUT_DEVICES, OUTPUT_DEVICES };
void tui_bind_change_tab(union tui_bind_data data);

/* TODO: find a more sane way to do this lol */
enum tui_nothing { NOTHING };
#define TUI_BIND_QUIT ((tui_bind_func_t)0xDEAD)

union tui_bind_data {
    enum tui_direction direction;
    enum tui_change_mode change_mode;
    enum tui_tab tab;
    enum tui_nothing nothing;
};

struct tui_bind {
    union tui_bind_data data;
    tui_bind_func_t func;
};

#endif /* #ifndef TUI_H */

