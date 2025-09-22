#include "menu.h"
#include "xmalloc.h"
#include "config.h"
#include "log.h"

void tui_menu_resize(struct tui_menu *const menu, int term_width, int term_height) {
    menu->x = 1;
    menu->y = 2;
    menu->w = term_width - 2;
    menu->h = term_height - 4;
    if (menu->win == NULL) {
        menu->win = newwin(menu->h, menu->w, menu->y, menu->x);
    } else {
        wresize(menu->win, menu->h, menu->w);
    }
}

struct tui_menu *tui_menu_create(unsigned int n_items) {
    struct tui_menu *menu = xzalloc(sizeof(*menu) + (sizeof(*menu->items) * n_items));
    menu->n_items = n_items;

    return menu;
}

bool tui_menu_change_focus(struct tui_menu *const menu, int direction) {
    bool change = false;
    if (direction < 0) {
        if (menu->selected > 0) {
            menu->selected -= 1;
            change = true;
        } else if (config.wraparound) {
            menu->selected = menu->n_items - 1;
            change = true;
        }
    } else {
        if (menu->selected < menu->n_items - 1) {
            menu->selected += 1;
            change = true;
        } else if (config.wraparound) {
            menu->selected = 0;
            change = true;
        }
    }

    return change;
}

void tui_menu_draw(const struct tui_menu *const menu) {
    TRACE("tui_draw_menu: %dx%d at %dx%d", menu->w, menu->h, menu->x, menu->y);
    WINDOW *win = menu->win;

    werase(win);

    /* box */
    wmove(win, 0, 0);
    waddwstr(win, config.borders.tl);
    for (int x = 1; x < menu->w - 1; x++) {
        waddwstr(win, config.borders.ts);
    }
    waddwstr(win, config.borders.tr);

    wmove(win, menu->h - 1, 0);
    waddwstr(win, config.borders.bl);
    for (int x = 1; x < menu->w - 1; x++) {
        waddwstr(win, config.borders.bs);
    }
    waddwstr(win, config.borders.br);

    for (int y = 1; y < menu->h - 1; y++) {
        wmove(win, y, 0);
        waddwstr(win, config.borders.ls);
        wmove(win, y, menu->w - 1);
        waddwstr(win, config.borders.rs);
    }

    mvwaddnstr(win, 0, 1, menu->header, menu->w - 2);
    for (unsigned int i = 0; i < menu->n_items; i++) {
        if (i == menu->selected) {
            wattron(win, A_BOLD);
        }

        mvwaddnstr(win, 1 + i, 1, menu->items[i].str, menu->w - 2);

        wattroff(win, A_BOLD);
    }

    wnoutrefresh(win);
}

