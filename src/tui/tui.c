#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <poll.h>
#include <math.h>
#include <wchar.h>
#include <stdio.h>

#include "tui/tui.h"
#include "tui/pad.h"
#include "macros.h"
#include "log.h"
#include "xmalloc.h"
#include "utils.h"
#include "config.h"
#include "macros.h"
#include "eventloop.h"
#include "pw/common.h"

#define FOR_EACH_TAB(var) for (int var = 0; var < tui.tabs_count; var++)

enum color_pair {
    DEFAULT = 0,
    GREEN = 1,
    YELLOW = 2,
    RED = 3,
};

struct tui tui = {0};

static enum tui_tab_type media_class_to_tui_tab(enum media_class class) {
    switch (class) {
    case STREAM_OUTPUT_AUDIO: return PLAYBACK;
    case STREAM_INPUT_AUDIO: return RECORDING;
    case AUDIO_SOURCE: return INPUT_DEVICES;
    case AUDIO_SINK: return OUTPUT_DEVICES;
    default: ABORT("Invalid media class passed to media_class_to_tui_tab");
    }
}

static const char *tui_tab_name(enum tui_tab_type tab) {
    switch (tab) {
    case PLAYBACK: return "Playback";
    case RECORDING: return "Recording";
    case OUTPUT_DEVICES: return "Output Devices";
    case INPUT_DEVICES: return "Input Devices";
    case CARDS: return "Cards";
    default: ABORT("Invalid tab type passed to tui_tab_name");
    }
}

static int find_tab(enum tui_tab_type type) {
    FOR_EACH_TAB(i) {
        if (tui.tabs[i].type == type) {
            return i;
        }
    }

    return -1;
}

static void trigger_update(void) {
    if (!tui.update_triggered) {
        if (pw_loop_signal_event(event_loop, tui.update_source) < 0) {
            ERROR("failed to trigger ui update");
        } else {
            tui.update_triggered = true;
        }
    }
}

static void trigger_resize(void) {
    if (!tui.resize_triggered) {
        if (pw_loop_signal_event(event_loop, tui.resize_source) < 0) {
            ERROR("failed to trigger ui resize");
        } else {
            tui.resize_triggered = true;
        }
    }
}

static int print_with_ellipsis(WINDOW *win, int y, int x,
                               const wchar_t string[], int string_len,
                               int max_columns) {
    if (max_columns <= 0 || string_len <= 0 || wmove(win, y, x) != OK) {
        return 0;
    }

    int columns = 0;
    for (int i = 0; i < string_len; i++) {
        const wchar_t wc[2] = { string[i], L'\0' };
        const int width = MAX(0, wcwidth(wc[0]));

        const bool truncate = (i < string_len - 1 && columns + width >= max_columns)
                           || (i == string_len - 1 && columns + width > max_columns);

        cchar_t cc;
        if (!truncate) {
            setcchar(&cc, wc, 0, 0, NULL);
            wadd_wch(win, &cc);
            columns += width;
        } else {
            setcchar(&cc, L"…", 0, 0, NULL);
            wadd_wch(win, &cc);
            columns += 1;
            break;
        }
    }

    return columns;
}

static void tui_tab_item_draw_node(const struct tui_tab_item *const item,
                                   enum tui_tab_item_draw_mask mask) {
    #define DRAW(element) if (mask & TUI_TAB_ITEM_DRAW_##element)

    const struct tui_tab_item_node_data *d = &item->as.node;

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
    const bool muted = d->muted;

    TRACE("tui_draw_node: id %d mask %x", d->id, mask);

    WINDOW *const win = tui.pad_win;

    /* prevents leftover artifacts */
    DRAW(BLANKS) {
        for (int i = 0; i < item->height; i++) {
            wmove(win, item->pos + i, 0);
            wclrtoeol(win);
        }
    }

    if (focused) {
        wattron(win, A_BOLD);
    }

    DRAW(DESCRIPTION) {
        int cols = 0;
        if (d->is_default) {
            cols += print_with_ellipsis(win, item->pos + 1, info_area_start,
                                        L"[*] ", wcslen(L"[*] "), usable_width);
        }
        cols += print_with_ellipsis(win, item->pos + 1, info_area_start + cols,
                                    d->info.data, d->info.len, usable_width - cols);

        for (int i = cols; i < usable_width; i++) {
            waddch(win, ' ');
        }
    }

    DRAW(CHANNELS) {
        if (muted) {
            wattron(win, A_DIM);
        }

        for (unsigned i = 0; i < d->n_channels; i++) {
            const struct channel_info *c = &d->channels[i];

            const int pos = item->pos + i + 2;

            const int vol_int = (int)roundf(c->volume * 100);

            mvwprintw(win, pos, volume_area_start, "%5s %-3d ", c->name, vol_int);

            /* draw volume bar */
            int pair = DEFAULT;
            const int step = volume_bar_width / 3;
            const int thresh = vol_int * volume_bar_width / 150;
            for (int j = 0; j < volume_bar_width; j++) {
                cchar_t cc;
                if (j % step == 0 && !muted) {
                    pair += 1;
                }
                setcchar(&cc, (j < thresh) ? config.bar_full_char : config.bar_empty_char,
                         0, pair, NULL);
                mvwadd_wch(win, pos, volume_bar_start + j, &cc);
            }
        }

        wattroff(win, A_DIM);
    }

    DRAW(DECORATIONS) {
        if (muted) {
            wattron(win, A_DIM);
        }

        for (unsigned i = 0; i < d->n_channels; i++) {
            const int pos = item->pos + i + 2;

            const wchar_t *wchar_left, *wchar_right;
            cchar_t cchar_left, cchar_right;

            if (d->n_channels == 1) {
                wchar_left = config.volume_frame.ml;
                wchar_right = config.volume_frame.mr;
            } else if (i == 0) {
                wchar_left = config.volume_frame.tl;
                wchar_right = config.volume_frame.tr;
            } else if (i == d->n_channels - 1) {
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
            if (focused && (!item->as.node.unlocked_channels
                            || item->as.node.focused_channel == i)) {
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

    DRAW(ROUTES) {
        if (!d->n_routes) {
            goto routes_end;
        }

        const int routes_line_pos = item->pos + item->height - 2;

        int cols = 0;
        cols += print_with_ellipsis(win, routes_line_pos, 1,
                                    L"Routes: ", wcslen(L"Routes: "),
                                    usable_width);

        if (!d->n_routes) {
            wattron(win, A_DIM);
            cols += print_with_ellipsis(win, routes_line_pos, 1 + cols,
                                        L"(none)", wcslen(L"(none)"),
                                        usable_width - cols);
        } else {
            if (d->active_route) {
                /* draw active route first */
                cols += print_with_ellipsis(win, routes_line_pos, 1 + cols,
                                            d->active_route->description.data,
                                            d->active_route->description.len,
                                            usable_width - cols);
            }

            wattron(win, A_DIM);
            for (unsigned i = 0; i < d->n_routes; i++) {
                const struct route_info *p = &d->routes[i];
                if (p == d->active_route) {
                    continue;
                }

                if (i > 0 || d->active_route) {
                    cols += print_with_ellipsis(win, routes_line_pos, 1 + cols,
                                                config.routes_separator,
                                                wcslen(config.routes_separator),
                                                usable_width - cols);
                }

                cols += print_with_ellipsis(win, routes_line_pos, 1 + cols,
                                            p->description.data, p->description.len,
                                            usable_width - cols);
            }
        }

        wattroff(win, A_DIM);
    }
routes_end:

    DRAW(BORDERS) {
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

    #undef DRAW
}

static void tui_tab_item_draw_device(const struct tui_tab_item *const item,
                                     enum tui_tab_item_draw_mask mask) {
    #define DRAW(element) if (mask & TUI_TAB_ITEM_DRAW_##element)

    const struct tui_tab_item_device_data *d = &item->as.device;
    const struct device *dev = item->as.device.dev;

    const int usable_width = tui.term_width - 2; /* account for box borders */

    const bool focused = item->focused;

    TRACE("tui_draw_device: id %d mask %x", device_id(dev), mask);

    WINDOW *const win = tui.pad_win;

    DRAW(BLANKS) {
        for (int i = 0; i < item->height; i++) {
            wmove(win, item->pos + i, 0);
            wclrtoeol(win);
        }
    }

    if (focused) {
        wattron(win, A_BOLD);
    }

    DRAW(DESCRIPTION) {
        int cols = print_with_ellipsis(win, item->pos + 1, 1,
                                       d->info.data, d->info.len, usable_width);

        for (int i = cols; i < usable_width; i++) {
            waddch(win, ' ');
        }
    }

    DRAW(PROFILES) {
        /* draw profiles */
        const int profiles_line_pos = item->pos + item->height - 2;

        int cols = 0;
        cols += print_with_ellipsis(win, profiles_line_pos, 1,
                                    L"Profiles: ", wcslen(L"Profiles: "),
                                    usable_width);

        if (!d->n_profiles) {
            wattron(win, A_DIM);
            cols += print_with_ellipsis(win, profiles_line_pos, 1 + cols,
                                        L"(none)", wcslen(L"(none)"),
                                        usable_width - cols);
        } else {
            if (d->active_profile) {
                /* draw active profile first */
                cols += print_with_ellipsis(win, profiles_line_pos, 1 + cols,
                                            d->active_profile->description.data,
                                            d->active_profile->description.len,
                                            usable_width - cols);
            }

            wattron(win, A_DIM);
            for (unsigned i = 0; i < d->n_profiles; i++) {
                const struct profile_info *p = &d->profiles[i];
                if (p == d->active_profile) {
                    continue;
                }

                cols += print_with_ellipsis(win, profiles_line_pos, 1 + cols,
                                            config.profiles_separator,
                                            wcslen(config.profiles_separator),
                                            usable_width - cols);

                cols += print_with_ellipsis(win, profiles_line_pos, 1 + cols,
                                            p->description.data, p->description.len,
                                            usable_width - cols);
            }
        }

        wattroff(win, A_DIM);
    }

    DRAW(BORDERS) {
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

    #undef DRAW
}

static void tui_tab_item_draw(const struct tui_tab_item *const item,
                              enum tui_tab_item_draw_mask mask) {
    if (item->tab_index != tui.tab_index) {
        return;
    }

    switch (item->type) {
    case TUI_TAB_ITEM_TYPE_NODE:
        tui_tab_item_draw_node(item, mask);
        break;
    case TUI_TAB_ITEM_TYPE_DEVICE:
        tui_tab_item_draw_device(item, mask);
        break;
    }
}

/* this only updates scroll pos and does not actually draw anything */
static void tui_tab_item_ensure_visible(const struct tui_tab_item *const item) {
    struct tui_tab *const tab = &tui.tabs[item->tab_index];

    /* minus top bar */
    const int visible_height = tui.term_height - 1;

    if (tab->scroll_pos > item->pos) {
        tab->scroll_pos = item->pos;
    } else if ((item->pos + item->height) > (tab->scroll_pos + visible_height)) {
        /* a + b = c + d <=> c = a + b - d */
        tab->scroll_pos = (item->pos + item->height) - visible_height;
    }
}

static void tui_tab_item_focus(struct tui_tab_item *const item, bool draw, bool user) {
    struct tui_tab *const tab = &tui.tabs[item->tab_index];
    if (user) {
        tab->user_changed_focus = true;
    }

    if (tab->focused == item) {
        return;
    } else if (tab->focused != NULL) {
        tab->focused->focused = false;
        if (draw) {
            tui_tab_item_draw(tab->focused, TUI_TAB_ITEM_DRAW_EVERYTHING);
        }
    }

    tab->focused = item;
    item->focused = true;
    if (draw) {
        tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_EVERYTHING);
    }

    tui_tab_item_ensure_visible(item);
}

static void tui_tab_item_unfocus(struct tui_tab_item *const item, bool draw) {
    struct tui_tab *const tab = &tui.tabs[item->tab_index];

    if (tab->focused != item) {
        WARN("tui_tab_item_unfocus called on unfocused item");
        return;
    }

    struct tui_tab_item *next = NULL;
    if (item->link.next != &tab->items) {
        next = CONTAINER_OF(item->link.next, struct tui_tab_item, link);
    } else if (item->link.prev != &tab->items) {
        next = CONTAINER_OF(item->link.prev, struct tui_tab_item, link);
    }

    if (next != NULL) {
        tui_tab_item_focus(next, draw, false);
    } else {
        tab->focused = NULL;
    }
}

void tui_bind_change_focus(union tui_bind_data data) {
    enum tui_direction direction = data.direction;

    if (tui.menu_active) {
        tui_menu_change_focus(tui.menu, (direction == UP) ? -1 : 1);
        return;
    }

    struct tui_tab *const tab = &tui.tabs[tui.tab_index];
    struct tui_tab_item *const f = tab->focused;
    if (f == NULL) {
        return;
    }

    switch (direction) {
    case DOWN: {
        const bool channel = f->type == TUI_TAB_ITEM_TYPE_NODE
                          && f->as.node.unlocked_channels
                          && f->as.node.focused_channel < f->as.node.n_channels - 1;
        if (channel) {
            f->as.node.focused_channel += 1;
            tui_tab_item_draw(f, TUI_TAB_ITEM_DRAW_DECORATIONS);
        } else {
            struct tui_tab_item *next = NULL;

            if (&f->link != tab->items.prev) {
                // f is not the last element
                next = CONTAINER_OF(f->link.next, struct tui_tab_item, link);
            } else if (config.wraparound) {
                // f is last, wrap around to the beginning
                next = CONTAINER_OF(tab->items.next, struct tui_tab_item, link);
            } else {
                break;
            }

            if (next->type == TUI_TAB_ITEM_TYPE_NODE && next->as.node.unlocked_channels) {
                next->as.node.focused_channel = 0;
            }
            tui_tab_item_focus(next, true, true);
        }
        break;
    }
    case UP: {
        const bool channel = f->type == TUI_TAB_ITEM_TYPE_NODE
                          && f->as.node.unlocked_channels
                          && f->as.node.focused_channel > 0;
        if (channel) {
            f->as.node.focused_channel -= 1;
            tui_tab_item_draw(f, TUI_TAB_ITEM_DRAW_DECORATIONS);
        } else {
            struct tui_tab_item *next = NULL;

            if (&f->link != tab->items.next) {
                // f is not the first element
                next = CONTAINER_OF(f->link.prev, struct tui_tab_item, link);
            } else if (config.wraparound) {
                // f is first, wrap around to the end
                next = CONTAINER_OF(tab->items.prev, struct tui_tab_item, link);
            } else {
                break;
            }

            if (next->type == TUI_TAB_ITEM_TYPE_NODE && next->as.node.unlocked_channels) {
                next->as.node.focused_channel = next->as.node.n_channels - 1;
            }
            tui_tab_item_focus(next, true, true);
        }
        break;
    }
    }
}

static void redraw_current_tab(void) {
    const struct tui_tab *tab = &tui.tabs[tui.tab_index];

    int bottom = 0;
    LIST_FOREACH(elem, &tab->items) {
        struct tui_tab_item *item = CONTAINER_OF(elem, struct tui_tab_item, link);
        tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_EVERYTHING);
        bottom += item->height;
    }

    if (wmove(tui.pad_win, bottom, 0) != OK) {
        WARN("wmove(tui.pad_win, %d, 0) failed!", bottom);
    } else {
        TRACE("wclrtobot(tui.pad_win) bottom %d", bottom);
        wclrtobot(tui.pad_win);
    }

    if (bottom == 0) {
        /* empty tab */
        static const char empty[] = "Empty";
        wattron(tui.pad_win, A_DIM);
        mvwaddstr(tui.pad_win,
                  (tui.term_height - 1) / 2, (tui.term_width / 2) - (strlen(empty) / 2),
                  empty);
        wattroff(tui.pad_win, A_DIM);
    }
}

static void redraw_status_bar(void) {
    wmove(tui.bar_win, 0, 0);

    FOR_EACH_TAB(tab_index) {
        if (tab_index != tui.tab_index) {
            wattron(tui.bar_win, A_DIM);
        } else {
            wattron(tui.bar_win, A_BOLD);
        }
        waddstr(tui.bar_win, tui_tab_name(tui.tabs[tab_index].type));
        if (tab_index != tui.tab_index) {
            wattroff(tui.bar_win, A_DIM);
        } else {
            wattroff(tui.bar_win, A_BOLD);
        }
        waddstr(tui.bar_win, "   ");
    }

    wclrtoeol(tui.bar_win);
}

void tui_bind_focus_last(union tui_bind_data data) {
    struct tui_tab *const tab = &tui.tabs[tui.tab_index];

    if (list_is_empty(&tab->items)) {
        return;
    }

    struct tui_tab_item *last = CONTAINER_OF(tab->items.prev, struct tui_tab_item, link);
    tui_tab_item_focus(last, true, true);
}

void tui_bind_focus_first(union tui_bind_data data) {
    struct tui_tab *const tab = &tui.tabs[tui.tab_index];

    if (list_is_empty(&tab->items)) {
        return;
    }

    struct tui_tab_item *first = CONTAINER_OF(tab->items.next, struct tui_tab_item, link);
    tui_tab_item_focus(first, true, true);
}

void tui_bind_change_volume(union tui_bind_data data) {
    const enum tui_direction direction = data.direction;
    const struct tui_tab_item *const focused = tui.tabs[tui.tab_index].focused;

    if (focused == NULL || focused->type != TUI_TAB_ITEM_TYPE_NODE || tui.menu_active) {
        return;
    }

    float delta = (direction == UP) ? config.volume_step : -config.volume_step;

    const struct node *node = focused->as.node.node;
    if (focused->as.node.unlocked_channels) {
        node_change_volume(node, false, delta, focused->as.node.focused_channel);
    } else {
        node_change_volume(node, false, delta, ALL_CHANNELS);
    }
}

void tui_bind_set_volume(union tui_bind_data data) {
    const float vol = data.volume;
    const struct tui_tab_item *const focused = tui.tabs[tui.tab_index].focused;

    if (focused == NULL || focused->type != TUI_TAB_ITEM_TYPE_NODE || tui.menu_active) {
        return;
    }

    const struct node *node = focused->as.node.node;
    if (focused->as.node.unlocked_channels) {
        node_change_volume(node, true, vol, focused->as.node.focused_channel);
    } else {
        node_change_volume(node, true, vol, ALL_CHANNELS);
    }
}

void tui_bind_change_mute(union tui_bind_data data) {
    const enum tui_change_mode mode = data.change_mode;
    const struct tui_tab_item *const focused = tui.tabs[tui.tab_index].focused;

    if (focused == NULL || focused->type != TUI_TAB_ITEM_TYPE_NODE || tui.menu_active) {
        return;
    }

    const struct tui_tab_item_node_data *d = &focused->as.node;

    const struct node *node = focused->as.node.node;
    switch (mode) {
    case ENABLE:
        node_set_mute(node, true);
        break;
    case DISABLE:
        node_set_mute(node, false);
        break;
    case TOGGLE:
        node_set_mute(node, !d->muted);
        break;
    }
}

void tui_bind_change_channel_lock(union tui_bind_data data) {
    enum tui_change_mode mode = data.change_mode;
    struct tui_tab_item *const focused = tui.tabs[tui.tab_index].focused;

    if (focused == NULL || focused->type != TUI_TAB_ITEM_TYPE_NODE || tui.menu_active) {
        return;
    }

    bool change = false;
    switch (mode) {
    case ENABLE:
        if (!focused->as.node.unlocked_channels) {
            focused->as.node.unlocked_channels = true;
            change = true;
        }
        break;
    case DISABLE:
        if (focused->as.node.unlocked_channels) {
            focused->as.node.unlocked_channels = false;
            change = true;
        }
        break;
    case TOGGLE:
        focused->as.node.unlocked_channels = !focused->as.node.unlocked_channels;
        change = true;
        break;
    }

    if (change) {
        tui_tab_item_draw(focused, TUI_TAB_ITEM_DRAW_DECORATIONS);
    }
}

static void tui_set_tab_and_redraw(int new_tab_index) {
    if (tui.tab_index == new_tab_index) {
        return;
    } else if (new_tab_index < 0 || new_tab_index > tui.tabs_count - 1) {
        WARN("tui_set_tab_and_redraw: OOB new_tab_index %d", new_tab_index);
        return;
    }

    tui.tab_index = new_tab_index;
    redraw_current_tab();
    redraw_status_bar();

    TRACE("current tab is: index %d (%s)",
          tui.tab_index, tui_tab_name(tui.tabs[tui.tab_index].type));
}

void tui_bind_change_tab(union tui_bind_data data) {
    int new_tab_index;
    switch (data.direction) {
    case UP:
        if (tui.tab_index == tui.tabs_count - 1) {
            new_tab_index = 0;
        } else {
            new_tab_index = tui.tab_index + 1;
        }
        break;
    case DOWN:
        if (tui.tab_index == 0) {
            new_tab_index = tui.tabs_count - 1;
        } else {
            new_tab_index = tui.tab_index - 1;
        }
        break;
    }

    tui_bind_set_tab_index((union tui_bind_data){
        .index = new_tab_index
    });
}

void tui_bind_set_tab(union tui_bind_data data) {
    int tab_index = find_tab(data.tab);
    if (tab_index < 0) {
        return;
    }

    tui_bind_set_tab_index((union tui_bind_data){ .index = tab_index });
}

void tui_bind_set_tab_index(union tui_bind_data data) {
    const int index = data.index;

    if (tui.menu_active) {
        return;
    }

    tui_set_tab_and_redraw(index);
}

void tui_bind_set_default(union tui_bind_data data) {
    struct tui_tab_item *const focused = tui.tabs[tui.tab_index].focused;

    if (focused == NULL || focused->type != TUI_TAB_ITEM_TYPE_NODE || tui.menu_active) {
        return;
    }

    const struct node *const node = focused->as.node.node;
    node_set_default(node);
}

static void on_profile_selection_done(struct tui_menu *menu, struct tui_menu_item *pick) {
    const uint32_t device_id = menu->data.uint;
    const uint32_t profile_id = pick->data.uint;

    TRACE("on_profile_selection_done: device_id %d route_id %d", device_id, profile_id);
    struct device *device = device_lookup(device_id);
    if (device == NULL) {
        WARN("on_profile_selection_done: device with id %d does not exist", device_id);
    }
    device_set_profile(device, profile_id);

    tui_menu_free(menu);
    tui.menu_active = false;

    redraw_current_tab();
}

void tui_bind_select_profile(union tui_bind_data data) {
    struct tui_tab_item *focused = tui.tabs[tui.tab_index].focused;

    if (focused == NULL || focused->type != TUI_TAB_ITEM_TYPE_DEVICE || tui.menu_active) {
        return;
    }

    struct tui_tab_item_device_data *d = &focused->as.device;

    if (d->n_profiles < 2) {
        return;
    }

    tui.menu = tui_menu_create(d->n_profiles);
    tui.menu->callback = on_profile_selection_done;
    tui.menu->data.uint = d->id;

    tui_menu_resize(tui.menu, tui.term_width, tui.term_height);

    wstring_printf(&tui.menu->header, L"Select profile for %ls", d->description.data);

    for (size_t i = 0; i < d->n_profiles; i++) {
        const struct profile_info *p = &d->profiles[i];
        struct tui_menu_item *item = &tui.menu->items[i];

        wstring_printf(&item->wstr, L"%d. %ls (%ls)",
                       p->index, p->description.data, p->name.data);
        item->data.uint = p->index;

        if (p == d->active_profile) {
            tui.menu->selected = i;
        }
    }

    tui.menu_active = true;
}

static void on_route_selection_done(struct tui_menu *menu, struct tui_menu_item *pick) {
    const uint32_t node_id = menu->data.uint;
    const uint32_t route_id = pick->data.uint;

    TRACE("on_route_selection_done: node_id %d route_id %d", node_id, route_id);
    struct node *node = node_lookup(node_id);
    if (node == NULL) {
        WARN("on_route_selection_done: node with id %d does not exist", node_id);
    }
    node_set_route(node, route_id);

    tui_menu_free(menu);
    tui.menu_active = false;

    redraw_current_tab();
}

void tui_bind_select_route(union tui_bind_data data) {
    struct tui_tab_item *const focused = tui.tabs[tui.tab_index].focused;

    if (focused == NULL || focused->type != TUI_TAB_ITEM_TYPE_NODE || tui.menu_active) {
        return;
    }

    struct tui_tab_item_node_data *d = &focused->as.node;

    if (d->n_routes < 2) {
        return;
    }

    tui.menu = tui_menu_create(d->n_routes);
    tui.menu->callback = on_route_selection_done;
    tui.menu->data.uint = d->id;

    tui_menu_resize(tui.menu, tui.term_width, tui.term_height);

    wstring_printf(&tui.menu->header, L"Select route for %ls", d->description.data);

    for (size_t i = 0; i < d->n_routes; i++) {
        const struct route_info *p = &d->routes[i];
        struct tui_menu_item *item = &tui.menu->items[i];

        wstring_printf(&item->wstr, L"%d. %ls (%ls)",
                       p->index, p->description.data, p->name.data);
        item->data.uint = p->index;

        if (p == d->active_route) {
            tui.menu->selected = i;
        }
    }

    tui.menu_active = true;
}

void tui_bind_cancel_selection(union tui_bind_data data) {
    if (!tui.menu_active) {
        return;
    }

    tui_menu_free(tui.menu);
    tui.menu_active = false;

    redraw_current_tab();
}

void tui_bind_confirm_selection(union tui_bind_data data) {
    if (!tui.menu_active) {
        return;
    }

    struct tui_menu *menu = tui.menu;
    menu->callback(menu, &menu->items[menu->selected]);
}

void tui_bind_quit_or_cancel_selection(union tui_bind_data data) {
    if (tui.menu_active) {
        tui_bind_cancel_selection(data);
    } else {
        tui_bind_quit(data);
    }
}

void tui_bind_quit(union tui_bind_data data) {
    pw_main_loop_quit(main_loop);
}

/* Change size (height) of item to (new_height),
 * while also adjusting positions of other items in the same tab as needed.
 * DOES NOT DRAW ANYTHING BY ITSELF */
static bool tui_tab_item_resize(struct tui_tab_item *item, int new_height) {
    const int diff = new_height - item->height;
    if (diff == 0) {
        return false;
    }

    const struct tui_tab *const tab = &tui.tabs[item->tab_index];

    struct tui_tab_item *last = CONTAINER_OF(tab->items.prev, struct tui_tab_item, link);
    tui.pad_win = tui_set_pad_size(tui.pad_win,
                                   AT_LEAST, last->pos + last->height + diff,
                                   AT_LEAST, tui.term_width);

    TRACE("tui_tab_item_resize: resizing item %p from %d to %d",
          (void *)item, item->height, item->height + diff);
    item->height += diff;

    // iterate starting from the first element that should be moved
    // TODO: make this a macro?
    for (struct list *elem = item->link.next; elem != &tab->items; elem = elem->next) {
        struct tui_tab_item *next = CONTAINER_OF(elem, struct tui_tab_item, link);
        TRACE("tui_tab_item_resize: shifting item %p from %d to %d",
              (void *)next, next->pos, next->pos + diff);
        next->pos += diff;
    }

    return true;
}

static void on_device_profiles(struct device *dev,
                               const struct param_profile *profiles, unsigned n_profiles,
                               void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_device_data *d = &item->as.device;

    for (unsigned i = 0; i < d->n_profiles; i++) {
        struct profile_info *oldp = &d->profiles[i];
        wstring_free(&oldp->name);
        wstring_free(&oldp->description);
    }

    d->n_profiles = n_profiles;
    d->profiles = xreallocarray(d->profiles, d->n_profiles, sizeof(d->profiles[0]));
    d->active_profile = NULL;

    for (unsigned i = 0; i < d->n_profiles; i++) {
        struct profile_info *pi = &d->profiles[i];
        const struct param_profile *pp = &profiles[i];

        wstring_init(&pi->name);
        wstring_init(&pi->description);

        pi->index = pp->index;

        wstring_printf(&pi->name, L"%s", pp->name);
        wstring_printf(&pi->description, L"%s", pp->description);

        if (pp->active) {
            d->active_profile = pi;
        }
    }

    tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_PROFILES);
    trigger_update();
}

static void on_device_props(struct device *dev, const struct dict *props, void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_device_data *d = &item->as.device;

    format_render(config.device_format, props, &d->info);

    wstring_clear(&d->description);
    wstring_printf(&d->description, L"%s", dict_get(props, "device.description"));

    tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_DESCRIPTION);
    trigger_update();
}

static void on_device_removed(struct device *dev, void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_device_data *d = &item->as.device;

    TRACE("on_device_removed: id %d", item->as.device.id);

    event_hook_release(item->hook);
    device_unref(&item->as.device.dev);

    tui_tab_item_resize(item, 0);
    tui_tab_item_unfocus(item, false);

    list_remove(&item->link);

    if (item->tab_index == tui.tab_index) {
        redraw_current_tab();
        trigger_update();
    }

    wstring_free(&d->description);
    wstring_free(&d->info);
    for (unsigned i = 0; i < d->n_profiles; i++) {
        wstring_free(&d->profiles[i].name);
        wstring_free(&d->profiles[i].description);
    }
    free(d->profiles);

    free(item);
}

static const struct device_events device_events = {
    .props = on_device_props,
    .profiles = on_device_profiles,
    .removed = on_device_removed,
};

static void on_node_default(struct node *node, bool is_default, void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_node_data *d = &item->as.node;

    d->is_default = is_default;

    tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_DESCRIPTION);
    trigger_update();
}

static void on_node_mute(struct node *node, bool muted, void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_node_data *d = &item->as.node;

    d->muted = muted;

    tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_CHANNELS | TUI_TAB_ITEM_DRAW_DECORATIONS);
    trigger_update();
}

static void on_node_routes(struct node *node,
                           const struct param_route routes[], unsigned routes_count,
                           void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_node_data *d = &item->as.node;

    for (unsigned i = 0; i < d->n_routes; i++) {
        struct route_info *oldp = &d->routes[i];
        wstring_free(&oldp->name);
        wstring_free(&oldp->description);
    }

    const unsigned old_n_routes = d->n_routes;

    d->n_routes = routes_count;
    d->routes = xreallocarray(d->routes, d->n_routes, sizeof(d->routes[0]));
    d->active_route = NULL;

    for (unsigned i = 0; i < d->n_routes; i++) {
        struct route_info *pi = &d->routes[i];
        const struct param_route *pp = &routes[i];

        wstring_init(&pi->name);
        wstring_init(&pi->description);

        pi->index = pp->index;

        wstring_printf(&pi->name, L"%s", pp->name);
        wstring_printf(&pi->description, L"%s", pp->description);

        if (pp->active) {
            d->active_route = pi;
        }
    }

    if ((old_n_routes && !d->n_routes) || (!old_n_routes && d->n_routes)) {
        tui_tab_item_resize(item, d->n_channels + 3 + (bool)d->n_routes);
        if (item->tab_index == tui.tab_index) {
            redraw_current_tab();
        }
    } else {
        tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_ROUTES);
    }

    trigger_update();
}

static void on_node_volume(struct node *node,
                           const float channel_volumes[], unsigned channel_count,
                           void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_node_data *d = &item->as.node;

    for (unsigned i = 0; i < channel_count; i++) {
        d->channels[i].volume = channel_volumes[i];
    }

    tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_CHANNELS);
    trigger_update();
}

static void on_node_channels(struct node *node,
                             const char *channel_names[], unsigned channel_count,
                             void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_node_data *d = &item->as.node;

    d->n_channels = channel_count;
    d->channels = xreallocarray(d->channels, d->n_channels, sizeof(d->channels[0]));
    if (d->focused_channel >= d->n_channels) {
        d->focused_channel = d->n_channels - 1;
    }

    for (unsigned i = 0; i < channel_count; i++) {
        d->channels[i].name = channel_names[i];
    }

    tui_tab_item_resize(item, d->n_channels + 3 + (bool)d->n_routes);

    if (item->tab_index == tui.tab_index) {
        redraw_current_tab();
    }
    trigger_update();
}

static void on_node_props(struct node *node, const struct dict *props, void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_node_data *d = &item->as.node;

    format_render(config.node_format, props, &d->info);

    const char *node_description = dict_get(props, "node.description");
    const char *node_name = dict_get(props, "node.name");

    wstring_clear(&d->description);
    wstring_printf(&d->description, L"%s", node_description ?: node_name);

    tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_DESCRIPTION);
    trigger_update();
}

static void on_node_removed(struct node *node, void *data) {
    struct tui_tab_item *item = data;
    struct tui_tab_item_node_data *d = &item->as.node;

    TRACE("tui_on_node_removed: id %d", d->id);

    event_hook_release(item->hook);
    node_unref(&item->as.node.node);

    tui_tab_item_resize(item, 0);
    tui_tab_item_unfocus(item, false);

    list_remove(&item->link);

    if (item->tab_index == tui.tab_index) {
        redraw_current_tab();
        trigger_update();
    }

    wstring_free(&d->description);
    wstring_free(&d->info);
    for (unsigned i = 0; i < d->n_routes; i++) {
        wstring_free(&d->routes[i].name);
        wstring_free(&d->routes[i].description);
    }
    free(d->routes);
    free(d->channels);

    free(item);
}

static const struct node_events node_events = {
    .removed = on_node_removed,
    .props = on_node_props,
    .channels = on_node_channels,
    .volume = on_node_volume,
    .routes = on_node_routes,
    .mute = on_node_mute,
    .default_ = on_node_default,
};

static void on_pipewire_device(struct device *dev, void *_) {
    TRACE("on_pipewire_device: id %d", device_id(dev));

    int tab_index = find_tab(CARDS);
    if (tab_index < 0) {
        return;
    }

    struct tui_tab_item *new_item = xmalloc(sizeof(*new_item));
    *new_item = (struct tui_tab_item){
        .tab_index = tab_index,
        .type = TUI_TAB_ITEM_TYPE_DEVICE,
        .as.device = {
            .id = device_id(dev),
            .dev = device_ref(dev),
        },
    };

    new_item->hook = device_add_listener(dev, &device_events, new_item);

    const int new_item_height = 4;
    list_insert_after(&tui.tabs[tab_index].items, &new_item->link);
    tui_tab_item_resize(new_item, new_item_height);

    if (tui.tabs[tab_index].focused == NULL || !tui.tabs[tab_index].user_changed_focus) {
        tui_tab_item_focus(new_item, false, false);
    }

    redraw_current_tab();
    trigger_update();
}

static void on_pipewire_node(struct node *node, void *_) {
    TRACE("tui_on_node_added: id %d", node_id(node));

    int tab_index = find_tab(media_class_to_tui_tab(node_media_class(node)));
    if (tab_index < 0) {
        return;
    }

    struct tui_tab_item *new_item = xmalloc(sizeof(*new_item));
    *new_item = (struct tui_tab_item){
        .tab_index = tab_index,
        .type = TUI_TAB_ITEM_TYPE_NODE,
        .as.node = {
            .id = node_id(node),
            .node = node_ref(node),
        }
    };

    new_item->hook = node_add_listener(node, &node_events, new_item);

    int new_item_height = new_item->as.node.n_channels + 3;
    list_insert_after(&tui.tabs[tab_index].items, &new_item->link);
    tui_tab_item_resize(new_item, new_item_height);

    if (tui.tabs[tab_index].focused == NULL || !tui.tabs[tab_index].user_changed_focus) {
        tui_tab_item_focus(new_item, false, false);
    }

    redraw_current_tab();
    trigger_update();
}

static const struct pipewire_events pipewire_events = {
    .node = on_pipewire_node,
    .device = on_pipewire_device,
};

static void on_stdin_ready(void *_, int _, uint32_t _) {
    wint_t ch;
    while (errno = 0, wget_wch(stdscr, &ch) != ERR || errno == EINTR) {
        if (ch == KEY_RESIZE) {
            WARN("KEY_RESIZE %s (%d)", key_name_from_key_code(ch), ch);
        }

        struct tui_bind *bind = map_get(&config.binds, ch);
        if (!bind) {
            DEBUG("unhandled key %s (%d)", key_name_from_key_code(ch), ch);
        } else {
            bind->func(bind->data);
            trigger_update();
        }
    }
}

static void on_resize_triggered(void *_, uint64_t _) {
    tui.resize_triggered = false;

    struct winsize winsize;
    if (ioctl(0 /* stdin */, TIOCGWINSZ, &winsize) < 0) {
        ERROR("failed to get new window size: %s", strerror(errno));
        return;
    }

    resize_term(winsize.ws_row, winsize.ws_col);
    tui.term_height = getmaxy(stdscr);
    tui.term_width = getmaxx(stdscr);
    DEBUG("new window dimensions %d lines %d columns", tui.term_height, tui.term_width);

    tui.pad_win = tui_set_pad_size(tui.pad_win,
                                   AT_LEAST, tui.term_height,
                                   EXACTLY, tui.term_width);
    if (tui.tabs[tui.tab_index].focused != NULL) {
        tui_tab_item_ensure_visible(tui.tabs[tui.tab_index].focused);
    }

    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
    }
    tui.bar_win = newwin(1, tui.term_width, 0, 0);

    redraw_current_tab();
    redraw_status_bar();

    if (tui.menu_active) {
        tui_menu_resize(tui.menu, tui.term_width, tui.term_height);
    }
}

/*
 * Trying to optimize updates is brain damage and I don't wanna deal with it.
 * Instead just update after any event that might or might not cause a draw
 * and let ncurses figure out the rest, it's good at damage tracking
 */
static void on_update_triggered(void *_, uint64_t _) {
    tui.update_triggered = false;

    pnoutrefresh(tui.pad_win,
                 tui.tabs[tui.tab_index].scroll_pos, 0,
                 1, 0,
                 tui.term_height - 1, tui.term_width - 1);

    wnoutrefresh(tui.bar_win);

    if (tui.menu_active) {
        tui_menu_draw(tui.menu);
    }

    doupdate();
}

static void on_sigwinch(int _) {
    trigger_resize();
    trigger_update();
}

bool tui_init(void) {
    /* must set signal handler BEFORE ncurses init */
    sigaction(SIGWINCH, &(struct sigaction){
        .sa_handler = on_sigwinch,
        .sa_flags = SA_RESTART,
    }, NULL);

    initscr();
    refresh(); /* https://stackoverflow.com/a/22121866 */
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE); /* getch() will fail instead of blocking waiting for input */
    keypad(stdscr, TRUE);
    ESCDELAY = 50 /* ms */;

    start_color();
    use_default_colors();
    init_pair(GREEN, COLOR_GREEN, -1);
    init_pair(YELLOW, COLOR_YELLOW, -1);
    init_pair(RED, COLOR_RED, -1);

    tui.tabs_count = config.tabs_count;
    tui.tabs = xcalloc(config.tabs_count, sizeof(tui.tabs[0]));
    FOR_EACH_TAB(i) {
        struct tui_tab *tab = &tui.tabs[i];

        tab->type = config.tabs[i];
        list_init(&tab->items);

        if (tab->type == config.default_tab) {
            tui.tab_index = i;
        }
    }

    tui.stdin_source = pw_loop_add_io(event_loop, 0, POLLIN, false, on_stdin_ready, event_loop);

    tui.resize_source = pw_loop_add_event(event_loop, on_resize_triggered, event_loop);
    tui.resize_triggered = false;

    tui.update_source = pw_loop_add_event(event_loop, on_update_triggered, event_loop);
    tui.update_triggered = false;

    tui.pipewire_hook = pipewire_add_listener(&pipewire_events, &tui);

    /* pick up initial terminal size */
    on_resize_triggered(NULL, 0);

    return true;
}

void tui_cleanup(void) {
    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
    }
    if (tui.pad_win != NULL) {
        delwin(tui.pad_win);
    }

    endwin();
}

