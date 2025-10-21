#pragma once

#include <ncurses.h>

struct tui_menu;
struct tui_menu_item;

typedef void (*tui_menu_callback_t)(struct tui_menu *menu, struct tui_menu_item *pick);

struct tui_menu_item {
    char *str;

    union {
        void *ptr;
        uintptr_t uint;
    } data;
};

struct tui_menu {
    WINDOW *win;
    int x, y, w, h;
    char *header;
    tui_menu_callback_t callback;

    union {
        void *ptr;
        uintptr_t uint;
    } data;

    unsigned int n_items;
    unsigned int selected;
    struct tui_menu_item items[];
};

struct tui_menu *tui_menu_create(unsigned int n_items);
void tui_menu_resize(struct tui_menu *menu, int term_width, int term_height);
void tui_menu_free(struct tui_menu *menu);

void tui_menu_draw(const struct tui_menu *menu);

bool tui_menu_change_focus(struct tui_menu *menu, int direction);

