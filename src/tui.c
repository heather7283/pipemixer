#include <assert.h>
#include <math.h>
#include <wchar.h>

#include "tui.h"
#include "macros.h"
#include "log.h"
#include "xmalloc.h"
#include "utils.h"
#include "config.h"
#include "macros.h"
#include "eventloop.h"
#include "pw/events.h"
#include "collections/string.h"

#define TUI_ACTIVE_TAB (tui.tabs[tui.tab])
#define FOR_EACH_TAB(var) for (enum tui_tab var = TUI_TAB_FIRST; var <= TUI_TAB_LAST; var++)

enum color_pair {
    DEFAULT = 0,
    GREEN = 1,
    YELLOW = 2,
    RED = 3,
};

struct tui tui = {0};

static void tui_repaint(bool draw_unconditionally);

static enum tui_tab media_class_to_tui_tab(enum media_class class) {
    switch (class) {
    case STREAM_OUTPUT_AUDIO: return PLAYBACK;
    case STREAM_INPUT_AUDIO: return RECORDING;
    case AUDIO_SOURCE: return INPUT_DEVICES;
    case AUDIO_SINK: return OUTPUT_DEVICES;
    default: assert(0 && "Invalid media class passed to media_class_to_tui_tab");
    }
}

static const char *tui_tab_name(enum tui_tab tab) {
    switch (tab) {
    case PLAYBACK: return "Playback";
    case RECORDING: return "Recording";
    case OUTPUT_DEVICES: return "Output Devices";
    case INPUT_DEVICES: return "Input Devices";
    default: return "INVALID";
    }
}

static void tui_menu_free(struct tui_menu *menu) {
    for (unsigned int i = 0; i < menu->n_items; i++) {
        string_free(&menu->items[i].str);
    }
    string_free(&menu->header);
    free(menu);
}

static void tui_menu_resize(struct tui_menu *menu, int term_width, int term_height) {
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

static struct tui_menu *tui_menu_create(unsigned int n_items) {
    struct tui_menu *menu = xcalloc(1, sizeof(*menu) + (sizeof(*menu->items) * n_items));
    menu->n_items = n_items;

    return menu;
}

static void tui_menu_change_focus(enum tui_direction direction) {
    struct tui_menu *menu = tui.menu;

    bool change = false;
    switch (direction) {
    case DOWN:
        if (menu->selected < menu->n_items - 1) {
            menu->selected += 1;
            change = true;
        } else if (config.wraparound) {
            menu->selected = 0;
            change = true;
        }
        break;
    case UP:
        if (menu->selected > 0) {
            menu->selected -= 1;
            change = true;
        } else if (config.wraparound) {
            menu->selected = menu->n_items - 1;
            change = true;
        }
        break;
    }

    if (change) {
        tui_repaint(false);
    }
}

static void tui_menu_draw(const struct tui_menu *menu) {
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

    mvwaddnstr(win, 0, 1, menu->header.data, menu->w - 2);
    for (unsigned int i = 0; i < menu->n_items; i++) {
        if (i == menu->selected) {
            wattron(win, A_BOLD);
        }

        mvwaddnstr(win, 1 + i, 1, menu->items[i].str.data, menu->w - 2);

        wattroff(win, A_BOLD);
    }

    wnoutrefresh(win);
}

static void tui_draw_node(const struct tui_tab_item *item, bool draw_unconditionally) {
    #define DRAW_IF(...) if (draw_unconditionally || __VA_ARGS__)

    const struct node *node = node_lookup(item->node_id);

    const int usable_width = tui.term_width - 2; /* account for box borders */
    const int two_thirds_usable_width = usable_width / 3 * 2;
    /* 5 for channel name, 1 space, 3 volume, 1 space, 4 more for decorations = 14 */
    const int volume_bar_width_max = two_thirds_usable_width - 14;
    const int volume_bar_width = (volume_bar_width_max / 15) * 15;
    const int volume_area_width = volume_bar_width + 14;
    const int info_area_width = usable_width - volume_area_width - 1; /* leave a space */
    const int info_area_start = 1; /* right after box border */
    const int volume_area_start = info_area_start + info_area_width + 1;
    const int volume_bar_start = volume_area_start + 12; /* minus two decorations at the end */

    const bool focused = item->focused;
    const bool muted = node->mute;

    /* TODO: this "damage tracking" is getting really messy really fast...
     * Is it even needed? Modern terminal emulators are very fast,
     * and ncurses should technically optimise writes... Needs testing though.
     */
    const bool focus_changed = item->change & TUI_TAB_ITEM_CHANGE_FOCUS;
    const bool unlocked_channels_changed = item->change & TUI_TAB_ITEM_CHANGE_CHANNEL_LOCK;
    const bool mute_changed = item->change & TUI_TAB_ITEM_CHANGE_MUTE;
    const bool info_changed = item->change & TUI_TAB_ITEM_CHANGE_INFO;
    const bool volume_changed = item->change & TUI_TAB_ITEM_CHANGE_VOLUME;
    const bool size_changed = item->change & TUI_TAB_ITEM_CHANGE_SIZE;
    const bool port_changed = item->change & TUI_TAB_ITEM_CHANGE_PORT;

    TRACE("tui_draw_node: id %d item_change "BYTE_BINARY_FORMAT" draw_unconditionally %d",
          node->id, BYTE_BINARY_ARGS(item->change), draw_unconditionally);

    WINDOW *const win = tui.pad_win;

    /* prevents leftover box artifacts */
    DRAW_IF(size_changed) {
        for (int i = 0; i < item->height; i++) {
            wmove(win, item->pos + i, 0);
            wclrtoeol(win);
        }
    }

    if (focused) {
        wattron(win, A_BOLD);
    }

    wchar_t line[usable_width];

    /* first line displays node name and media name and spans across the entire screen */
    DRAW_IF(focus_changed || info_changed) {
        if (config.display_ids) {
            swprintf(line, SIZEOF_ARRAY(line), L"(%d) %ls%s%ls%-*s",
                     node->id,
                     node->node_name.data,
                     wstring_is_empty(&node->media_name) ? "" : ": ",
                     wstring_is_empty(&node->media_name) ? L"" : node->media_name.data,
                     usable_width, "");
        } else {
            swprintf(line, SIZEOF_ARRAY(line), L"%ls%s%ls%-*s",
                     node->node_name.data,
                     wstring_is_empty(&node->media_name) ? "" : ": ",
                     wstring_is_empty(&node->media_name) ? L"" : node->media_name.data,
                     usable_width, "");
        }
        wcstrimcols(line, usable_width);
        mvwaddnwstr(win, item->pos + 1, info_area_start, line, usable_width);
    }

    DRAW_IF(focus_changed || volume_changed || mute_changed) {
        /* draw info about each channel */
        if (muted) {
            wattron(win, A_DIM);
        }

        for (uint32_t i = 0; i < VEC_SIZE(&node->channels); i++) {
            const int pos = item->pos + i + 2;

            const int vol_int = (int)roundf(VEC_AT(&node->channels, i)->volume * 100);

            mvwprintw(win, pos, volume_area_start, "%5s %-3d ",
                      VEC_AT(&node->channels, i)->name, vol_int);

            /* draw volume bar */
            int pair = DEFAULT;
            const int step = volume_bar_width / 3;
            const int thresh = vol_int * volume_bar_width / 150;
            for (int j = 0; j < volume_bar_width; j++) {
                cchar_t cc;
                if (j % step == 0 && !node->mute) {
                    pair += 1;
                }
                setcchar(&cc, (j < thresh) ? config.bar_full_char : config.bar_empty_char,
                         0, pair, NULL);
                mvwadd_wch(win, pos, volume_bar_start + j, &cc);
            }
        }

        wattroff(win, A_DIM);
    }

    /* draw decorations (also focused markers) */
    DRAW_IF(focus_changed || unlocked_channels_changed || mute_changed) {
        /* draw info about each channel */
        if (muted) {
            wattron(win, A_DIM);
        }

        for (uint32_t i = 0; i < VEC_SIZE(&node->channels); i++) {
            const int pos = item->pos + i + 2;

            const wchar_t *wchar_left, *wchar_right;
            cchar_t cchar_left, cchar_right;

            if (VEC_SIZE(&node->channels) == 1) {
                wchar_left = config.volume_frame.ml;
                wchar_right = config.volume_frame.mr;
            } else if (i == 0) {
                wchar_left = config.volume_frame.tl;
                wchar_right = config.volume_frame.tr;
            } else if (i == VEC_SIZE(&node->channels) - 1) {
                wchar_left = config.volume_frame.bl;
                wchar_right = config.volume_frame.br;
            } else {
                wchar_left = config.volume_frame.cl;
                wchar_right = config.volume_frame.cr;
            }
            setcchar(&cchar_left, wchar_left, 0, DEFAULT, NULL);
            setcchar(&cchar_right, wchar_right, 0, DEFAULT, NULL);
            mvwadd_wch(win, pos, volume_bar_start - 1, &cchar_left);
            mvwadd_wch(win, pos, volume_bar_start + volume_bar_width, &cchar_right);

            const wchar_t *wchar_focus;
            if (focused && (!item->unlocked_channels || item->focused_channel == i)) {
                wchar_focus = config.volume_frame.f;
            } else {
                wchar_focus = L" ";
            }
            cchar_t cchar_focus;
            setcchar(&cchar_focus, wchar_focus, 0, DEFAULT, NULL);
            mvwadd_wch(win, pos, volume_bar_start - 2, &cchar_focus);
            mvwadd_wch(win, pos, volume_bar_start + volume_bar_width + 1, &cchar_focus);
        }

        wattroff(win, A_DIM);
    }

    DRAW_IF(focus_changed || port_changed) {
        char buf[usable_width];
        const int ports_line_pos = item->pos + item->height - 2;

        if (node->device_id != 0) {
            int written = 0, chars;
            chars = snprintf(buf, usable_width - written, "Routes: ");
            mvwaddnstr(win, ports_line_pos, 1 + written, buf, usable_width - written);
            written = (written + chars > usable_width) ? usable_width : (written + chars);

            const struct route *active_route = node_get_active_route(node);
            const struct route *const *routes;
            const size_t nroutes = node_get_available_routes(node, &routes);

            if (active_route != NULL) {
                /* draw active route first */
                chars = snprintf(buf, usable_width - written, "%s",
                                 active_route->description);
                mvwaddnstr(win, ports_line_pos, 1 + written, buf, usable_width - written);
                written = (written + chars > usable_width) ? usable_width : (written + chars);

                wattron(win, A_DIM);
                for (size_t i = 0; i < nroutes; i++) {
                    const struct route *route = routes[i];
                    if (active_route != NULL && route->index == active_route->index) {
                        continue;
                    }

                    chars = snprintf(buf, usable_width - written, "%s%s",
                                     config.routes_separator, route->description);
                    mvwaddnstr(win, ports_line_pos, 1 + written, buf, usable_width - written);
                    written = (written + chars > usable_width) ? usable_width : (written + chars);
                }
            } else if (nroutes > 0) {
                wattron(win, A_DIM);
                for (size_t i = 0; i < nroutes; i++) {
                    const struct route *route = routes[i];
                    if (i == 0) {
                        chars = snprintf(buf, usable_width - written, "%s",
                                         route->description);
                    } else {
                        chars = snprintf(buf, usable_width - written, "%s%s",
                                         config.routes_separator, route->description);
                    }
                    mvwaddnstr(win, ports_line_pos, 1 + written, buf, usable_width - written);
                    written = (written + chars > usable_width) ? usable_width : (written + chars);
                }
            } else {
                wattron(win, A_DIM);
                chars = snprintf(buf, usable_width - written, "(none)");
                mvwaddnstr(win, ports_line_pos, 1 + written, buf, usable_width - written);
                written = (written + chars > usable_width) ? usable_width : (written + chars);
            }

            if (written >= usable_width) {
                cchar_t cc;
                setcchar(&cc, L"…", 0, DEFAULT, NULL);
                mvwadd_wch(win, ports_line_pos, usable_width, &cc);
            } else {
                mvwhline(win, ports_line_pos, written + 1, ' ', usable_width - written);
            }

            wattroff(win, A_DIM);
        }
    }

    /* TODO: nuke tui_repaint and call this function for each node to avoid this nonsense */
    DRAW_IF((item->change == TUI_TAB_ITEM_CHANGE_EVERYTHING)) {
        /* box */
        wmove(win, item->pos, 0);
        waddwstr(win, config.borders.tl);
        for (int x = 1; x < tui.term_width - 1; x++) {
            waddwstr(win, config.borders.ts);
        }
        waddwstr(win, config.borders.tr);

        wmove(win, item->pos + item->height - 1, 0);
        waddwstr(win, config.borders.bl);
        for (int x = 1; x < tui.term_width - 1; x++) {
            waddwstr(win, config.borders.bs);
        }
        waddwstr(win, config.borders.br);

        for (int y = 1; y < item->height - 1; y++) {
            wmove(win, item->pos + y, 0);
            waddwstr(win, config.borders.ls);
        }

        for (int y = 1; y < item->height - 1; y++) {
            wmove(win, item->pos + y, tui.term_width - 1);
            waddwstr(win, config.borders.ls);
        }
    }

    wattroff(win, A_BOLD);

    #undef DRAW_IF
}

static void tui_draw_status_bar(void) {
    wmove(tui.bar_win, 0, 0);

    FOR_EACH_TAB(tab) {
        if (tab != tui.tab) {
            wattron(tui.bar_win, A_DIM);
        } else {
            wattron(tui.bar_win, A_BOLD);
        }
        waddstr(tui.bar_win, tui_tab_name(tab));
        if (tab != tui.tab) {
            wattroff(tui.bar_win, A_DIM);
        } else {
            wattroff(tui.bar_win, A_BOLD);
        }
        waddstr(tui.bar_win, "   ");
    }

    wclrtoeol(tui.bar_win);
    wnoutrefresh(tui.bar_win);
}

static void tui_repaint(bool draw_unconditionally) {
    TRACE("tui_repaint: draw_unconditionally %d", draw_unconditionally);
    /* TODO: maybe add a function to redraw a single node instead of doing this? */
    int bottom = 0;
    struct tui_tab_item *tab_item;
    LIST_FOR_EACH(tab_item, &TUI_ACTIVE_TAB.items, link) {
        if (draw_unconditionally || tab_item->change != TUI_TAB_ITEM_CHANGE_NOTHING) {
            tui_draw_node(tab_item, draw_unconditionally);
        }

        tab_item->change = TUI_TAB_ITEM_CHANGE_NOTHING;
        bottom += tab_item->height;
    }

    /* TODO: inefficient? Only clear on tab change/node removal? */
    TRACE("clearing pad from %d to bottom", bottom);
    if (wmove(tui.pad_win, bottom, 0) != OK) {
        WARN("wmove(tui.pad_win, %d, 0) failed!", bottom);
    } else {
        wclrtobot(tui.pad_win);
    }

    if (bottom == 0) {
        /* empty tab */
        const char *empty = "Empty";
        wattron(tui.pad_win, A_DIM);
        mvwaddstr(tui.pad_win,
                  (tui.term_height - 1) / 2, (tui.term_width / 2) - (strlen(empty) / 2),
                  empty);
        wattroff(tui.pad_win, A_DIM);
    }

    pnoutrefresh(tui.pad_win,
                 TUI_ACTIVE_TAB.scroll_pos, 0,
                 1, 0,
                 tui.term_height - 1, tui.term_width - 1);

    tui_draw_status_bar();

    if (tui.menu_active) {
        TRACE("tui_repaint: menu");
        tui_menu_draw(tui.menu);
    }

    doupdate();
}

static void tui_tab_item_ensure_visible(enum tui_tab tab, const struct tui_tab_item *item) {
    const int visible_height = tui.term_height - 1 /* minus top bar */;

    if (tui.tabs[tab].scroll_pos > item->pos) /* item is above screen space */ {
        tui.tabs[tab].scroll_pos = item->pos;
    /*                  a + b             =                         c + d               */
    } else if ((item->pos + item->height) > (tui.tabs[tab].scroll_pos + visible_height)) {
        /*                     c =          a + b             - d            */
        tui.tabs[tab].scroll_pos = (item->pos + item->height) - visible_height;
    }
}

/* true if focus changed */
static bool tui_tab_item_set_focused(enum tui_tab tab, struct tui_tab_item *item) {
    if (item->focused) {
        return false;
    }

    if (tui.tabs[tab].focused != NULL) {
        tui.tabs[tab].focused->focused = false;
        tui.tabs[tab].focused->change |= TUI_TAB_ITEM_CHANGE_FOCUS;
    }

    tui.tabs[tab].focused = item;
    item->focused = true;
    item->change |= TUI_TAB_ITEM_CHANGE_FOCUS;

    tui_tab_item_ensure_visible(tab, item);

    return true;
}

void tui_bind_change_focus(union tui_bind_data data) {
    enum tui_direction direction = data.direction;

    if (tui.menu_active) {
        tui_menu_change_focus(direction);
        return;
    }

    if (TUI_ACTIVE_TAB.focused == NULL) {
        return;
    }
    struct tui_tab_item *const focused = TUI_ACTIVE_TAB.focused;
    struct node *const focused_node = node_lookup(focused->node_id);

    TUI_ACTIVE_TAB.user_changed_focus = true;

    bool change = false;
    switch (direction) {
    case DOWN:
        if (focused->unlocked_channels && focused->focused_channel < VEC_SIZE(&focused_node->channels) - 1) {
            focused->focused_channel += 1;
            focused->change |= TUI_TAB_ITEM_CHANGE_CHANNEL_LOCK;
            change = true;
        } else {
            if (!LIST_IS_LAST(&TUI_ACTIVE_TAB.items, &focused->link)) {
                struct tui_tab_item *next;
                LIST_GET(next, LIST_NEXT(&focused->link), link);
                if (next->unlocked_channels) {
                    next->focused_channel = 0;
                }
                change = tui_tab_item_set_focused(tui.tab, next);
            } else if (config.wraparound) {
                struct tui_tab_item *first;
                LIST_GET_FIRST(first, &TUI_ACTIVE_TAB.items, link);
                if (first->unlocked_channels) {
                    first->focused_channel = 0;
                }
                change = tui_tab_item_set_focused(tui.tab, first);
            }
        }
        break;
    case UP:
        if (focused->unlocked_channels && focused->focused_channel > 0) {
            focused->focused_channel -= 1;
            focused->change |= TUI_TAB_ITEM_CHANGE_CHANNEL_LOCK;
            change = true;
        } else {
            struct tui_tab_item *next_item = NULL;

            if (!LIST_IS_FIRST(&TUI_ACTIVE_TAB.items, &focused->link)) {
                LIST_GET(next_item, LIST_PREV(&focused->link), link);
            } else if (config.wraparound) {
                LIST_GET_LAST(next_item, &TUI_ACTIVE_TAB.items, link);
            }

            if (next_item != NULL) {
                struct node *const next_node = node_lookup(next_item->node_id);
                if (next_item->unlocked_channels) {
                    next_item->focused_channel = VEC_SIZE(&next_node->channels) - 1;
                }
                change = tui_tab_item_set_focused(tui.tab, next_item);
            }
        }
        break;
    }

    if (change) {
        tui_repaint(false);
    }
}

void tui_bind_focus_last(union tui_bind_data data) {
    if (LIST_IS_EMPTY(&TUI_ACTIVE_TAB.items)) {
        return;
    }

    TUI_ACTIVE_TAB.user_changed_focus = true;

    struct tui_tab_item *first;
    LIST_GET_LAST(first, &TUI_ACTIVE_TAB.items, link);
    if (tui_tab_item_set_focused(tui.tab, first)) {
        tui_repaint(false);
    }
}

void tui_bind_focus_first(union tui_bind_data data) {
    if (LIST_IS_EMPTY(&TUI_ACTIVE_TAB.items)) {
        return;
    }

    TUI_ACTIVE_TAB.user_changed_focus = true;

    struct tui_tab_item *last;
    LIST_GET_FIRST(last, &TUI_ACTIVE_TAB.items, link);
    if (tui_tab_item_set_focused(tui.tab, last)) {
        tui_repaint(false);
    }
}

void tui_bind_change_volume(union tui_bind_data data) {
    enum tui_direction direction = data.direction;

    if (TUI_ACTIVE_TAB.focused == NULL || tui.menu_active) {
        return;
    }

    float delta = (direction == UP) ? config.volume_step : -config.volume_step;

    const struct node *const node = node_lookup(TUI_ACTIVE_TAB.focused->node_id);
    if (TUI_ACTIVE_TAB.focused->unlocked_channels) {
        node_change_volume(node, false, delta, TUI_ACTIVE_TAB.focused->focused_channel);
    } else {
        node_change_volume(node, false, delta, ALL_CHANNELS);
    }
}

void tui_bind_set_volume(union tui_bind_data data) {
    float vol = data.volume;

    if (TUI_ACTIVE_TAB.focused == NULL || tui.menu_active) {
        return;
    }

    const struct node *const node = node_lookup(TUI_ACTIVE_TAB.focused->node_id);
    if (TUI_ACTIVE_TAB.focused->unlocked_channels) {
        node_change_volume(node, true, vol, TUI_ACTIVE_TAB.focused->focused_channel);
    } else {
        node_change_volume(node, true, vol, ALL_CHANNELS);
    }
}

void tui_bind_change_mute(union tui_bind_data data) {
    enum tui_change_mode mode = data.change_mode;

    if (TUI_ACTIVE_TAB.focused == NULL || tui.menu_active) {
        return;
    }

    const struct node *const node = node_lookup(TUI_ACTIVE_TAB.focused->node_id);
    switch (mode) {
    case ENABLE:
        node_set_mute(node, true);
        break;
    case DISABLE:
        node_set_mute(node, false);
        break;
    case TOGGLE:
        node_set_mute(node, !node->mute);
        break;
    }
}

void tui_bind_change_channel_lock(union tui_bind_data data) {
    enum tui_change_mode mode = data.change_mode;

    if (TUI_ACTIVE_TAB.focused == NULL || tui.menu_active) {
        return;
    }

    bool change = false;
    switch (mode) {
    case ENABLE:
        if (!TUI_ACTIVE_TAB.focused->unlocked_channels) {
            TUI_ACTIVE_TAB.focused->unlocked_channels = true;
            TUI_ACTIVE_TAB.focused->change |= TUI_TAB_ITEM_CHANGE_CHANNEL_LOCK;
            change = true;
        }
        break;
    case DISABLE:
        if (TUI_ACTIVE_TAB.focused->unlocked_channels) {
            TUI_ACTIVE_TAB.focused->unlocked_channels = false;
            TUI_ACTIVE_TAB.focused->change |= TUI_TAB_ITEM_CHANGE_CHANNEL_LOCK;
            change = true;
        }
        break;
    case TOGGLE:
        TUI_ACTIVE_TAB.focused->unlocked_channels = !TUI_ACTIVE_TAB.focused->unlocked_channels;
        TUI_ACTIVE_TAB.focused->change |= TUI_TAB_ITEM_CHANGE_CHANNEL_LOCK;
        change = true;
        break;
    }

    if (change) {
        tui_repaint(false);
    }
}

void tui_bind_change_tab(union tui_bind_data data) {
    if (tui.menu_active) {
        return;
    }

    switch (data.direction) {
    case UP:
        if (tui.tab++ == TUI_TAB_LAST) {
            tui.tab = TUI_TAB_FIRST;
        }
        break;
    case DOWN:
        if (tui.tab-- == TUI_TAB_FIRST) {
            tui.tab = TUI_TAB_LAST;
        }
        break;
    default:
        assert(0 && "Invalid tab enum value passed to tui_bind_change_tab");
    }

    tui_repaint(true);
}

void tui_bind_set_tab(union tui_bind_data data) {
    enum tui_tab tab = data.tab;

    if (tui.menu_active) {
        return;
    }

    bool tab_changed = false;
    switch (tab) {
    case PLAYBACK:
        if (tui.tab != PLAYBACK) {
            tui.tab = PLAYBACK;
            tab_changed = true;
        }
        break;
    case RECORDING:
        if (tui.tab != RECORDING) {
            tui.tab = RECORDING;
            tab_changed = true;
        }
        break;
    case INPUT_DEVICES:
        if (tui.tab != INPUT_DEVICES) {
            tui.tab = INPUT_DEVICES;
            tab_changed = true;
        }
        break;
    case OUTPUT_DEVICES:
        if (tui.tab != OUTPUT_DEVICES) {
            tui.tab = OUTPUT_DEVICES;
            tab_changed = true;
        }
        break;
    default:
        assert(0 && "Invalid tab enum value passed to tui_bind_change_tab");
    }

    if (tab_changed) {
        tui_repaint(true);
    }
}

static void on_port_selection_done(struct tui_menu *menu, struct tui_menu_item *pick) {
    const uint32_t node_id = menu->data.uint;
    const uint32_t route_id = pick->data.uint;

    TRACE("on_port_selection_done: node_id %d route_id %d", node_id, route_id);
    struct node *node = node_lookup(node_id);
    if (node == NULL) {
        WARN("on_port_selection_done: node with id %d does not exist", node_id);
    }
    node_set_route(node, route_id);

    tui_menu_free(menu);
    tui.menu_active = false;
    tui_repaint(true);
}

void tui_bind_select_route(union tui_bind_data data) {
    if (TUI_ACTIVE_TAB.focused == NULL || tui.menu_active) {
        return;
    }

    const struct node *const node = node_lookup(TUI_ACTIVE_TAB.focused->node_id);
    const struct route *active_route = node_get_active_route(node);
    const struct route *const *routes;
    const size_t nroutes = node_get_available_routes(node, &routes);

    if (nroutes < 2) {
        return;
    }

    tui.menu = tui_menu_create(nroutes);
    tui.menu->callback = on_port_selection_done;
    tui.menu->data.uint = node->id;

    tui_menu_resize(tui.menu, tui.term_width, tui.term_height);

    string_printf(&tui.menu->header, "Select route for %ls", node->node_name.data);

    for (size_t i = 0; i < nroutes; i++) {
        const struct route *route = routes[i];
        struct tui_menu_item *item = &tui.menu->items[i];
        string_printf(&item->str, "%d. %s (%s)",
                      route->index, route->description, route->name);
        item->data.uint = route->index;

        if (active_route != NULL && route->index == active_route->index) {
            tui.menu->selected = i;
        }
    }

    tui.menu_active = true;
    tui_repaint(false);
}

void tui_bind_cancel_selection(union tui_bind_data data) {
    if (!tui.menu_active) {
        return;
    }

    tui_menu_free(tui.menu);
    tui.menu_active = false;
    tui_repaint(true);
}

void tui_bind_confirm_selection(union tui_bind_data data) {
    if (!tui.menu_active) {
        return;
    }

    struct tui_menu *menu = tui.menu;
    menu->callback(menu, &menu->items[menu->selected]);
}

static WINDOW *tui_resize_pad(WINDOW *pad, int y, int x, bool keep_contents) {
    TRACE("tui_resize_pad: y %d x %d", y, x);

    WINDOW *new_pad = newpad(y, x);

    /* TODO: not the best place to put those functions */
    nodelay(new_pad, TRUE); /* getch() will fail instead of blocking waiting for input */
    keypad(new_pad, TRUE);

    if (keep_contents && pad != NULL) {
        copywin(pad, new_pad, 0, 0, 0, 0, y, x, FALSE);
        delwin(pad);
    }

    return new_pad;
}

/* 0 to leave dimension as is */
enum tui_set_pad_size_policy { EXACTLY, AT_LEAST };
static void tui_set_pad_size(enum tui_set_pad_size_policy y_policy, int y,
                             enum tui_set_pad_size_policy x_policy, int x,
                             bool keep_contents) {
    TRACE("tui_set_pad_size: y %s %d x %s %d",
          y_policy == EXACTLY ? "exactly" : "at least", y,
          x_policy == EXACTLY ? "exactly" : "at least", x);

    if (tui.pad_win == NULL) {
        tui.pad_win = tui_resize_pad(tui.pad_win, y, x, keep_contents);
    } else {
        bool need_resize = false;
        int new_y, new_x;

        int max_y = getmaxy(tui.pad_win);
        int max_x = getmaxx(tui.pad_win);

        switch (y_policy) {
        case EXACTLY:
            if (y != max_y) {
                need_resize = true;
            }
            new_y = y;
            break;
        case AT_LEAST:
            if (y > max_y) {
                need_resize = true;
            }
            new_y = MAX(max_y, y);
            break;
        }

        switch (x_policy) {
        case EXACTLY:
            if (x != max_x) {
                need_resize = true;
            }
            new_x = x;
            break;
        case AT_LEAST:
            if (x > max_x) {
                need_resize = true;
            }
            new_x = MAX(max_x, x);
            break;
        }

        if (need_resize) {
            tui.pad_win = tui_resize_pad(tui.pad_win, new_y, new_x, keep_contents);
        }
    }
}

/* Change size (height) of item to (new_height),
 * while also adjusting positions of other items in the same tab as needed.
 * Does not repaint. The caller must repaint.
 */
static void tui_tab_item_resize(enum tui_tab tab, struct tui_tab_item *item, int new_height) {
    const int diff = new_height - item->height;
    if (diff == 0) {
        return;
    }

    TRACE("tui_tab_item_resize: resizing item %p from %d to %d",
          (void *)item, item->height, item->height + diff);
    item->height += diff;
    item->change = TUI_TAB_ITEM_CHANGE_EVERYTHING;

    struct tui_tab_item *next;
    LIST_FOR_EACH_AFTER(next, &tui.tabs[tab].items, &item->link, link) {
        TRACE("tui_tab_item_resize: shifting item %p from %d to %d",
              (void *)next, next->pos, next->pos + diff);
        next->pos += diff;
        next->change = TUI_TAB_ITEM_CHANGE_EVERYTHING;
    }

    struct tui_tab_item *last;
    LIST_GET_LAST(last, &tui.tabs[tab].items, link);
    tui_set_pad_size(AT_LEAST, last->pos + last->height, AT_LEAST, tui.term_width, true);
}

void tui_tab_item_on_node_remove(struct tui_tab_item *const item) {
    TRACE("tui_on_node_removed: id %d", item->node_id);

    /* no need to unsubscribe from node, it does not exist anymore */
    signal_unsubscribe(&item->device_listener);

    const enum tui_tab tab = item->tab;

    tui_tab_item_resize(tab, item, 0);
    LIST_REMOVE(&item->link);

    if (!LIST_IS_EMPTY(&tui.tabs[tab].items) && item->focused) {
        struct tui_tab_item *first;
        LIST_GET_FIRST(first, &tui.tabs[tab].items, link);
        first->focused = true;
        tui.tabs[tab].focused = first;
    }

    if (tab == tui.tab) {
        tui_repaint(false);
    }
}

void tui_tab_item_on_node_change(struct tui_tab_item *const item,
                                 const struct node *const node) {
    TRACE("tui_on_node_changed: id %d", node->id);

    const enum tui_tab tab = item->tab;

    if (node->changed & NODE_CHANGE_INFO) {
        item->change |= TUI_TAB_ITEM_CHANGE_INFO;
    }
    if (node->changed & NODE_CHANGE_MUTE) {
        item->change |= TUI_TAB_ITEM_CHANGE_MUTE;
    }
    if (node->changed & NODE_CHANGE_VOLUME) {
        item->change |= TUI_TAB_ITEM_CHANGE_VOLUME;
    }
    if (node->changed & NODE_CHANGE_CHANNEL_COUNT) {
        int new_item_height = VEC_SIZE(&node->channels) + 3;
        if (node->device_id != 0) {
            new_item_height += 1;
        }
        tui_tab_item_resize(tab, item, new_item_height);
    }

    if (tab == tui.tab) {
        tui_repaint(false);
    }
}

static void tui_tab_item_on_device_change(struct tui_tab_item *const item,
                                          const struct device *const dev) {
    TRACE("tui_on_device_changed: id %d", dev->id);

    item->change |= TUI_TAB_ITEM_CHANGE_PORT;
    tui_repaint(false);
}

static void tui_tab_item_on_device_events(uint64_t id, uint64_t events,
                                          const struct signal_data *data, void *userdata) {
    struct tui_tab_item *const item = userdata;

    switch ((enum device_event_types)events) {
    case DEVICE_EVENT_CHANGE: {
        const struct device *const dev = device_lookup(id);
        if (dev != NULL) {
            tui_tab_item_on_device_change(item, dev);
        }
        break;
    }
    default:
        break;
    }
}

static void tui_tab_item_on_node_events(uint64_t id, uint64_t events,
                                        const struct signal_data *data, void *userdata) {
    struct tui_tab_item *const item = userdata;

    switch ((enum node_event_types)events) {
    case NODE_EVENT_CHANGE: {
        const struct node *const node = node_lookup(id);
        if (node != NULL) {
            tui_tab_item_on_node_change(item, node);
        }
        break;
    }
    case NODE_EVENT_REMOVE: {
        tui_tab_item_on_node_remove(item);
        break;
    }
    default:
        break;
    }
}

void tui_on_node_added(const struct node *node) {
    TRACE("tui_on_node_added: id %d", node->id);

    const enum tui_tab tab = media_class_to_tui_tab(node->media_class);

    struct tui_tab_item *new_item = xzalloc(sizeof(*new_item));
    new_item->tab = tab;
    new_item->node_id = node->id;
    new_item->pos = 0;
    new_item->change = TUI_TAB_ITEM_CHANGE_EVERYTHING;

    node_events_subscribe(&new_item->node_listener,
                          node->id, NODE_EVENT_ANY,
                          tui_tab_item_on_node_events, new_item);
    if (node->device_id != 0) {
        device_events_subscribe(&new_item->device_listener,
                                node->device_id, DEVICE_EVENT_ANY,
                                tui_tab_item_on_device_events, new_item);
    }

    if (tui.tabs[tab].focused == NULL || !tui.tabs[tab].user_changed_focus) {
        tui_tab_item_set_focused(tab, new_item);
    }
    LIST_INSERT(&tui.tabs[tab].items, &new_item->link);

    int new_item_height = VEC_SIZE(&node->channels) + 3;
    if (node->device_id != 0) {
        new_item_height += 1;
    }
    tui_tab_item_resize(tab, new_item, new_item_height);

    if (tab == tui.tab) {
        tui_repaint(false);
    }
}

static void tui_on_pipewire_object_added(uint64_t id, uint64_t event,
                                         const struct signal_data *data, void *_) {
    switch ((enum pipewire_event_types)event) {
    case PIPEWIRE_EVENT_NODE_ADDED: {
        const struct node *node = node_lookup(data->as.u64);
        if (node != NULL) {
            tui_on_node_added(node);
        }
        break;
    }
    case PIPEWIRE_EVENT_DEVICE_ADDED: {
        //const struct device *device = device_lookup(data->as.u64);
        //if (device != NULL) {
        //    tui_on_device_added(device);
        //}
        break;
    }
    default:
        break;
    }
}

static int tui_handle_sigwinch(struct pollen_callback *callback, int signal, void *data) {
    struct winsize winsize;
    if (ioctl(0 /* stdin */, TIOCGWINSZ, &winsize) < 0) {
        ERROR("failed to get new window size: %s", strerror(errno));
        return -1;
    }

    resize_term(winsize.ws_row, winsize.ws_col);
    tui.term_height = getmaxy(stdscr);
    tui.term_width = getmaxx(stdscr);
    DEBUG("new window dimensions %d lines %d columns", tui.term_height, tui.term_width);

    tui_set_pad_size(AT_LEAST, tui.term_height, EXACTLY, tui.term_width, false);
    if (TUI_ACTIVE_TAB.focused != NULL) {
        tui_tab_item_ensure_visible(tui.tab, TUI_ACTIVE_TAB.focused);
    }

    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
    }
    tui.bar_win = newwin(1, tui.term_width, 0, 0);

    if (tui.menu_active) {
        tui_menu_resize(tui.menu, tui.term_width, tui.term_height);
    }

    tui_repaint(true);

    return 0;
}

static int tui_handle_stdin(struct pollen_callback *callback,
                            int fd, uint32_t events, void *data) {
    wint_t ch;
    while (errno = 0, wget_wch(tui.pad_win, &ch) != ERR || errno == EINTR) {
        struct tui_bind *bind = MAP_GET(&config.binds, ch);
        if (bind == NULL) {
            DEBUG("unhandled key %s (%d)", key_name_from_key_code(ch), ch);
        } else if (bind->func == TUI_BIND_QUIT) {
            pollen_loop_quit(pollen_callback_get_loop(callback), 0);
        } else {
            bind->func(bind->data);
        }
    }

    return 0;
}

int tui_init(void) {
    initscr();
    refresh(); /* https://stackoverflow.com/a/22121866 */
    cbreak();
    noecho();
    curs_set(0);
    ESCDELAY = 50 /* ms */;

    start_color();
    use_default_colors();
    init_pair(GREEN, COLOR_GREEN, -1);
    init_pair(YELLOW, COLOR_YELLOW, -1);
    init_pair(RED, COLOR_RED, -1);

    FOR_EACH_TAB(tab) {
        LIST_INIT(&tui.tabs[tab].items);
    }

    pollen_loop_add_fd(event_loop, 0 /* stdin */, EPOLLIN, false, tui_handle_stdin, NULL);
    pollen_loop_add_signal(event_loop, SIGWINCH, tui_handle_sigwinch, NULL);

    /* manually trigger resize handler to pick up initial terminal size */
    tui_handle_sigwinch(NULL, 0xBAD, NULL);

    tui.tab = TUI_TAB_FIRST;
    tui_repaint(true);

    pipewire_events_subscribe(&tui.pipewire_listener,
                              PIPEWIRE_EVENT_ID_CORE, PIPEWIRE_EVENT_ANY,
                              tui_on_pipewire_object_added, NULL);

    return 0;
}

int tui_cleanup(void) {
    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
    }
    if (tui.pad_win != NULL) {
        delwin(tui.pad_win);
    }

    endwin();

    return 0;
}

