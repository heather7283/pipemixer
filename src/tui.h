#pragma once

#include <ncurses.h>

#include "collections/list.h"
#include "signals.h"
#include "menu.h"

enum tui_tab_type {
    PLAYBACK,
    RECORDING,
    INPUT_DEVICES,
    OUTPUT_DEVICES,
    CARDS,

    TUI_TAB_COUNT,

    TUI_TAB_INVALID,
};

struct tui_tab {
    LIST_HEAD items;
    struct tui_tab_item *focused;
    int scroll_pos;
    bool user_changed_focus;
};

struct tui {
    int term_height, term_width;

    WINDOW *bar_win;
    WINDOW *pad_win;

    bool menu_active;
    struct tui_menu *menu;

    int tab_index;
    struct tui_tab tabs[TUI_TAB_COUNT];

    bool efd_triggered;
    struct pollen_event_source *efd_source;

    struct signal_listener pipewire_listener;
};

enum tui_tab_item_draw_mask {
    TUI_TAB_ITEM_DRAW_NOTHING = 0,
    TUI_TAB_ITEM_DRAW_EVERYTHING = ~0,

    TUI_TAB_ITEM_DRAW_DESCRIPTION = 1 << 0,
    TUI_TAB_ITEM_DRAW_DECORATIONS = 1 << 1,
    TUI_TAB_ITEM_DRAW_CHANNELS = 1 << 2,
    TUI_TAB_ITEM_DRAW_ROUTES = 1 << 3,
    TUI_TAB_ITEM_DRAW_PROFILES = 1 << 4,
    TUI_TAB_ITEM_DRAW_BORDERS = 1 << 5,
    TUI_TAB_ITEM_DRAW_BLANKS = 1 << 6,
};

enum tui_tab_item_type {
    TUI_TAB_ITEM_TYPE_NODE,
    TUI_TAB_ITEM_TYPE_DEVICE,
};

struct tui_tab_item {
    int pos, height;
    bool focused;

    enum tui_tab_item_type type;
    union {
        struct {
            uint32_t node_id;

            uint32_t n_channels;
            bool unlocked_channels;
            uint32_t focused_channel;
        } node;
        struct {
            uint32_t device_id;
        } device;
    } as;

    struct signal_listener device_listener;
    struct signal_listener node_listener;

    int tab_index;
    LIST_ENTRY link;
};

extern struct tui tui;

int tui_init(void);
int tui_cleanup(void);

/* binds */
union tui_bind_data;
typedef void (*tui_bind_func_t)(union tui_bind_data data);

enum tui_direction { UP, DOWN };
void tui_bind_change_focus(union tui_bind_data data);
void tui_bind_change_volume(union tui_bind_data data);
void tui_bind_change_tab(union tui_bind_data data);

void tui_bind_set_volume(union tui_bind_data data);
void tui_bind_set_tab(union tui_bind_data data);
void tui_bind_set_tab_index(union tui_bind_data data);

enum tui_change_mode { ENABLE, DISABLE, TOGGLE };
void tui_bind_change_mute(union tui_bind_data data);
void tui_bind_change_channel_lock(union tui_bind_data data);

enum tui_nothing { NOTHING };
/* TODO: find a more sane way to do this lol */
#define TUI_BIND_QUIT ((tui_bind_func_t)0xDEAD)
void tui_bind_focus_first(union tui_bind_data data);
void tui_bind_focus_last(union tui_bind_data data);

void tui_bind_set_default(union tui_bind_data data);
void tui_bind_select_route(union tui_bind_data data);
void tui_bind_select_profile(union tui_bind_data data);
void tui_bind_confirm_selection(union tui_bind_data data);
void tui_bind_cancel_selection(union tui_bind_data data);

union tui_bind_data {
    enum tui_direction direction;
    enum tui_change_mode change_mode;
    enum tui_tab_type tab;
    enum tui_nothing nothing;
    float volume;
    int index;
};

struct tui_bind {
    union tui_bind_data data;
    tui_bind_func_t func;
};

