#pragma once

#include <ncurses.h>

#include "tui/menu.h"
#include "collections/list.h"
#include "collections/wstring.h"
#include "events.h"

enum tui_tab_type {
    PLAYBACK,
    RECORDING,
    INPUT_DEVICES,
    OUTPUT_DEVICES,
    CARDS,

    TUI_TAB_TYPE_COUNT,
};

struct tui_tab {
    enum tui_tab_type type;
    struct list items;
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

    int tabs_count, tab_index;
    struct tui_tab *tabs;

    struct spa_source *stdin_source;
    bool update_triggered;
    struct spa_source *update_source;
    bool resize_triggered;
    struct spa_source *resize_source;

    struct event_hook *pipewire_hook;
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
        struct tui_tab_item_node_data {
            uint32_t id;
            struct node *node;

            bool is_default;

            struct wstring info, description;

            bool muted;

            unsigned n_channels;
            bool unlocked_channels;
            unsigned focused_channel;
            struct channel_info {
                const char *name;
                float volume;
            } *channels;

            unsigned n_routes;
            struct route_info {
                int32_t index;
                struct wstring name, description;
            } *routes;
            struct route_info *active_route;
        } node;
        struct tui_tab_item_device_data {
            uint32_t id;
            struct device *dev;

            struct wstring info, description;

            unsigned n_profiles;
            struct profile_info {
                int32_t index;
                struct wstring name, description;
            } *profiles;
            struct profile_info *active_profile;
        } device;
    } as;

    struct event_hook *hook;

    int tab_index;
    struct list link;
};

extern struct tui tui;

bool tui_init(void);
void tui_cleanup(void);

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
void tui_bind_focus_first(union tui_bind_data data);
void tui_bind_focus_last(union tui_bind_data data);
void tui_bind_confirm_selection(union tui_bind_data data);
void tui_bind_cancel_selection(union tui_bind_data data);
void tui_bind_quit_or_cancel_selection(union tui_bind_data data);
void tui_bind_quit(union tui_bind_data data);

void tui_bind_set_default(union tui_bind_data data);
void tui_bind_select_route(union tui_bind_data data);
void tui_bind_select_profile(union tui_bind_data data);

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

