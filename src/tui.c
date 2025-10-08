#include <sys/eventfd.h>
#include <sys/ioctl.h>
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
#include "filter.h"
#include "eventloop.h"
#include "pw/events.h"
#include "pw/node.h"

#define FOR_EACH_TAB(var) for (int var = 0; var < TUI_TAB_COUNT; var++)

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
    default: assert(0 && "Invalid media class passed to media_class_to_tui_tab");
    }
}

static const char *tui_tab_name(enum tui_tab_type tab) {
    switch (tab) {
    case PLAYBACK: return "Playback";
    case RECORDING: return "Recording";
    case OUTPUT_DEVICES: return "Output Devices";
    case INPUT_DEVICES: return "Input Devices";
    case CARDS: return "Cards";
    default: assert(0 && "Invalid tab type passed to tui_tab_name");
    }
}

static void trigger_update(void) {
    if (!tui.efd_triggered) {
        if (!pollen_efd_trigger(tui.efd_source)) {
            ERROR("failed to trigger ui update: %m");
        } else {
            tui.efd_triggered = true;
        }
    }
}

static void tui_tab_item_draw_node(const struct tui_tab_item *const item,
                                   enum tui_tab_item_draw_mask mask) {
    #define DRAW(element) if (mask & TUI_TAB_ITEM_DRAW_##element)

    const struct node *node = node_lookup(item->as.node.node_id);

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

    TRACE("tui_draw_node: id %d mask "BYTE_BINARY_FORMAT, node->id, BYTE_BINARY_ARGS(mask));

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
        wchar_t line[usable_width];

        const char *name_str = "NULL";
        if (node->node_description != NULL) {
            name_str = node->node_description;
        } else if (node->node_name != NULL) {
            name_str = node->node_name;
        }

        if (config.display_ids) {
            swprintf(line, SIZEOF_ARRAY(line), L"(%d) %s%s%s%-*s",
                     node->id, name_str,
                     (node->media_name == NULL) ? "" : ": ",
                     (node->media_name == NULL) ? "" : node->media_name,
                     usable_width, "");
        } else {
            swprintf(line, SIZEOF_ARRAY(line), L"%s%s%s%-*s",
                     name_str,
                     (node->media_name == NULL) ? "" : ": ",
                     (node->media_name == NULL) ? "" : node->media_name,
                     usable_width, "");
        }
        wcstrimcols(line, usable_width);
        mvwaddnwstr(win, item->pos + 1, info_area_start, line, usable_width);
    }

    DRAW(CHANNELS) {
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

    DRAW(DECORATIONS) {
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
        char buf[usable_width];
        const int routes_line_pos = item->pos + item->height - 2;

        if (node->device_id != 0) {
            int written = 0, chars;
            chars = snprintf(buf, usable_width - written, "Routes: ");
            mvwaddnstr(win, routes_line_pos, 1 + written, buf, usable_width - written);
            written = (written + chars > usable_width) ? usable_width : (written + chars);

            const struct route *active_route = node_get_active_route(node);
            const struct route *const *routes;
            const size_t nroutes = node_get_available_routes(node, &routes);

            if (active_route != NULL) {
                /* draw active route first */
                chars = snprintf(buf, usable_width - written, "%s",
                                 active_route->description);
                mvwaddnstr(win, routes_line_pos, 1 + written, buf, usable_width - written);
                written = (written + chars > usable_width) ? usable_width : (written + chars);

                wattron(win, A_DIM);
                for (size_t i = 0; i < nroutes; i++) {
                    const struct route *route = routes[i];
                    if (active_route != NULL && route->index == active_route->index) {
                        continue;
                    }

                    chars = snprintf(buf, usable_width - written, "%s%s",
                                     config.routes_separator, route->description);
                    mvwaddnstr(win, routes_line_pos, 1 + written, buf, usable_width - written);
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
                    mvwaddnstr(win, routes_line_pos, 1 + written, buf, usable_width - written);
                    written = (written + chars > usable_width) ? usable_width : (written + chars);
                }
            } else {
                wattron(win, A_DIM);
                chars = snprintf(buf, usable_width - written, "(none)");
                mvwaddnstr(win, routes_line_pos, 1 + written, buf, usable_width - written);
                written = (written + chars > usable_width) ? usable_width : (written + chars);
            }

            if (written >= usable_width) {
                cchar_t cc;
                setcchar(&cc, L"…", 0, DEFAULT, NULL);
                mvwadd_wch(win, routes_line_pos, usable_width, &cc);
            } else {
                mvwhline(win, routes_line_pos, written + 1, ' ', usable_width - written);
            }

            wattroff(win, A_DIM);
        }
    }

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

    const struct device *dev = device_lookup(item->as.device.device_id);

    const int usable_width = tui.term_width - 2; /* account for box borders */

    const bool focused = item->focused;

    TRACE("tui_draw_device: id %d mask "BYTE_BINARY_FORMAT, dev->id, BYTE_BINARY_ARGS(mask));

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
        wchar_t line[usable_width];
        swprintf(line, SIZEOF_ARRAY(line), L"%d. %s", dev->id, dev->description);
        mvwaddnwstr(win, item->pos + 1, 1, line, SIZEOF_ARRAY(line));
        wclrtoeol(win);
    }

    DRAW(PROFILES) {
        /* draw profiles */
        char buf[usable_width];
        const int ports_line_pos = item->pos + item->height - 2;

        int written = 0, chars;
        chars = snprintf(buf, usable_width - written, "Profiles: ");
        mvwaddnstr(win, ports_line_pos, 1 + written, buf, usable_width - written);
        written = (written + chars > usable_width) ? usable_width : (written + chars);

        const struct profile *active_profile = device_get_active_profile(dev);
        const struct profile *profiles;
        const size_t nprofiles = device_get_available_profiles(dev, &profiles);

        if (active_profile != NULL) {
            /* draw active profile first */
            chars = snprintf(buf, usable_width - written, "%s",
                             active_profile->description);
            mvwaddnstr(win, ports_line_pos, 1 + written, buf, usable_width - written);
            written = (written + chars > usable_width) ? usable_width : (written + chars);

            wattron(win, A_DIM);
            for (size_t i = 0; i < nprofiles; i++) {
                const struct profile *const profile = &profiles[i];
                if (active_profile != NULL && profile->index == active_profile->index) {
                    continue;
                }

                chars = snprintf(buf, usable_width - written, "%s%s",
                                 config.profiles_separator, profile->description);
                mvwaddnstr(win, ports_line_pos, 1 + written, buf, usable_width - written);
                written = (written + chars > usable_width) ? usable_width : (written + chars);
            }
        } else if (nprofiles > 0) {
            wattron(win, A_DIM);
            for (size_t i = 0; i < nprofiles; i++) {
                const struct profile *const profile = &profiles[i];
                if (i == 0) {
                    chars = snprintf(buf, usable_width - written, "%s",
                                     profile->description);
                } else {
                    chars = snprintf(buf, usable_width - written, "%s%s",
                                     config.profiles_separator, profile->description);
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
    if (LIST_NEXT(&item->link) != &tab->items) {
        LIST_GET(next, LIST_NEXT(&item->link), link);
    } else if (LIST_PREV(&item->link) != &tab->items) {
        LIST_GET(next, LIST_PREV(&item->link), link);
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
    case DOWN:
        if (f->type == TUI_TAB_ITEM_TYPE_NODE
            && f->as.node.unlocked_channels
            && f->as.node.focused_channel < f->as.node.n_channels - 1) {
            f->as.node.focused_channel += 1;
            tui_tab_item_draw(f, TUI_TAB_ITEM_DRAW_DECORATIONS);
        } else {
            struct tui_tab_item *next = NULL;

            if (!LIST_IS_LAST(&tab->items, &f->link)) {
                LIST_GET(next, LIST_NEXT(&f->link), link);
            } else if (config.wraparound) {
                LIST_GET_FIRST(next, &tab->items, link);
            } else {
                break;
            }

            if (next->type == TUI_TAB_ITEM_TYPE_NODE && next->as.node.unlocked_channels) {
                next->as.node.focused_channel = 0;
            }
            tui_tab_item_focus(next, true, true);
        }
        break;
    case UP:
        if (f->type == TUI_TAB_ITEM_TYPE_NODE
            && f->as.node.unlocked_channels
            && f->as.node.focused_channel > 0) {
            f->as.node.focused_channel -= 1;
            tui_tab_item_draw(f, TUI_TAB_ITEM_DRAW_DECORATIONS);
        } else {
            struct tui_tab_item *next = NULL;

            if (!LIST_IS_FIRST(&tab->items, &f->link)) {
                LIST_GET(next, LIST_PREV(&f->link), link);
            } else if (config.wraparound) {
                LIST_GET_LAST(next, &tab->items, link);
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

static void redraw_current_tab(void) {
    const struct tui_tab *tab = &tui.tabs[tui.tab_index];

    int bottom = 0;
    struct tui_tab_item *item;
    LIST_FOR_EACH(item, &tab->items, link) {
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
        waddstr(tui.bar_win, tui_tab_name(config.tab_map_index_to_enum[tab_index]));
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

    if (LIST_IS_EMPTY(&tab->items)) {
        return;
    }

    struct tui_tab_item *last;
    LIST_GET_LAST(last, &tab->items, link);
    tui_tab_item_focus(last, true, true);
}

void tui_bind_focus_first(union tui_bind_data data) {
    struct tui_tab *const tab = &tui.tabs[tui.tab_index];

    if (LIST_IS_EMPTY(&tab->items)) {
        return;
    }

    struct tui_tab_item *first;
    LIST_GET_FIRST(first, &tab->items, link);
    tui_tab_item_focus(first, true, true);
}

void tui_bind_change_volume(union tui_bind_data data) {
    const enum tui_direction direction = data.direction;
    const struct tui_tab_item *const focused = tui.tabs[tui.tab_index].focused;

    if (focused == NULL || focused->type != TUI_TAB_ITEM_TYPE_NODE || tui.menu_active) {
        return;
    }

    float delta = (direction == UP) ? config.volume_step : -config.volume_step;

    const struct node *const node = node_lookup(focused->as.node.node_id);
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

    const struct node *const node = node_lookup(focused->as.node.node_id);
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

    const struct node *const node = node_lookup(focused->as.node.node_id);
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
    }

    tui.tab_index = new_tab_index;
    redraw_current_tab();
    redraw_status_bar();

    TRACE("current tab is: index %d (enum %d)",
          tui.tab_index, config.tab_map_index_to_enum[tui.tab_index]);
}

void tui_bind_change_tab(union tui_bind_data data) {
    int new_tab_index;
    switch (data.direction) {
    case UP:
        if (tui.tab_index == TUI_TAB_COUNT - 1) {
            new_tab_index = 0;
        } else {
            new_tab_index = tui.tab_index + 1;
        }
        break;
    case DOWN:
        if (tui.tab_index == 0) {
            new_tab_index = TUI_TAB_COUNT - 1;
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
    tui_bind_set_tab_index((union tui_bind_data){
        .index = config.tab_map_enum_to_index[data.tab]
    });
}

void tui_bind_set_tab_index(union tui_bind_data data) {
    const int index = data.index;

    if (tui.menu_active) {
        return;
    }

    tui_set_tab_and_redraw(index);
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
    struct tui_tab_item *const focused = tui.tabs[tui.tab_index].focused;

    if (focused == NULL || focused->type != TUI_TAB_ITEM_TYPE_DEVICE || tui.menu_active) {
        return;
    }

    const struct device *const device = device_lookup(focused->as.device.device_id);
    const struct profile *active_profile = device_get_active_profile(device);
    const struct profile *profiles;
    const size_t nprofiles = device_get_available_profiles(device, &profiles);

    if (nprofiles < 2) {
        return;
    }

    tui.menu = tui_menu_create(nprofiles);
    tui.menu->callback = on_profile_selection_done;
    tui.menu->data.uint = device->id;

    tui_menu_resize(tui.menu, tui.term_width, tui.term_height);

    xasprintf(&tui.menu->header, "Select profile for %s", device->description);

    for (size_t i = 0; i < nprofiles; i++) {
        const struct profile *const profile = &profiles[i];
        struct tui_menu_item *const item = &tui.menu->items[i];
        xasprintf(&item->str, "%d. %s (%s)", profile->index, profile->description, profile->name);
        item->data.uint = profile->index;

        if (active_profile != NULL && profile->index == active_profile->index) {
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

    const struct node *const node = node_lookup(focused->as.node.node_id);
    const struct route *active_route = node_get_active_route(node);
    const struct route *const *routes;
    const size_t nroutes = node_get_available_routes(node, &routes);

    if (nroutes < 2) {
        return;
    }

    tui.menu = tui_menu_create(nroutes);
    tui.menu->callback = on_route_selection_done;
    tui.menu->data.uint = node->id;

    tui_menu_resize(tui.menu, tui.term_width, tui.term_height);

    xasprintf(&tui.menu->header, "Select route for %s", node->node_name);
    for (size_t i = 0; i < nroutes; i++) {
        const struct route *route = routes[i];
        struct tui_menu_item *item = &tui.menu->items[i];
        xasprintf(&item->str, "%d. %s (%s)", route->index, route->description, route->name);
        item->data.uint = route->index;

        if (active_route != NULL && route->index == active_route->index) {
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
 * DOES NOT DRAW ANYTHING BY ITSELF
 */
static void tui_tab_item_resize(struct tui_tab_item *item, int new_height) {
    const int diff = new_height - item->height;
    if (diff == 0) {
        return;
    }

    const struct tui_tab *const tab = &tui.tabs[item->tab_index];

    struct tui_tab_item *last;
    LIST_GET_LAST(last, &tab->items, link);
    tui_set_pad_size(AT_LEAST, last->pos + last->height + diff, AT_LEAST, tui.term_width, true);

    TRACE("tui_tab_item_resize: resizing item %p from %d to %d",
          (void *)item, item->height, item->height + diff);
    item->height += diff;

    struct tui_tab_item *next;
    LIST_FOR_EACH_AFTER(next, &tab->items, &item->link, link) {
        TRACE("tui_tab_item_resize: shifting item %p from %d to %d",
              (void *)next, next->pos, next->pos + diff);
        next->pos += diff;
    }
}

/* EVENTS FOR NODE ITEMS */

static void on_node_remove(struct tui_tab_item *item) {
    TRACE("tui_on_node_removed: id %d", item->type == TUI_TAB_ITEM_TYPE_NODE
                                        ? item->as.node.node_id
                                        : item->as.device.device_id);

    signal_unsubscribe(&item->node_listener);
    signal_unsubscribe(&item->device_listener);

    tui_tab_item_resize(item, 0);
    tui_tab_item_unfocus(item, false);

    LIST_REMOVE(&item->link);

    if (item->tab_index == tui.tab_index) {
        redraw_current_tab();
    }

    free(item);
}

static void on_node_change(struct tui_tab_item *item,
                           const struct node *node, enum node_change_mask change) {
    TRACE("tui_on_node_changed: id %d", node->id);

    if (change & NODE_CHANGE_CHANNEL_COUNT) {
        int new_item_height = VEC_SIZE(&node->channels) + 3;
        if (node->device_id != 0) {
            new_item_height += 1;
        }
        tui_tab_item_resize(item, new_item_height);

        if (item->tab_index == tui.tab_index) {
            redraw_current_tab();
        }
    } else {
        enum tui_tab_item_draw_mask mask = 0;
        if (change & NODE_CHANGE_INFO) {
            mask |= TUI_TAB_ITEM_DRAW_DESCRIPTION;
        }
        if (change & NODE_CHANGE_MUTE) {
            mask |= TUI_TAB_ITEM_DRAW_CHANNELS;
            mask |= TUI_TAB_ITEM_DRAW_DECORATIONS;
        }
        if (change & NODE_CHANGE_VOLUME) {
            mask |= TUI_TAB_ITEM_DRAW_CHANNELS;
        }
        tui_tab_item_draw(item, mask);
    }
}

static void on_device_change_for_node(uint64_t id, uint64_t events,
                                      const struct signal_data *data, void *userdata) {
    struct tui_tab_item *const item = userdata;
    const struct device *const dev = device_lookup(id);
    if (dev != NULL) {
        tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_BORDERS);
    }
}

static void on_node_events(uint64_t id, uint64_t events,
                           const struct signal_data *data, void *userdata) {
    struct tui_tab_item *const item = userdata;

    switch ((enum node_event_types)events) {
    case NODE_EVENT_CHANGE: {
        const uint64_t change = data->as.u64;
        const struct node *const node = node_lookup(id);
        if (node != NULL) {
            on_node_change(item, node, change);
        }
        break;
    }
    case NODE_EVENT_REMOVE: {
        on_node_remove(item);
        break;
    }
    default:
        break;
    }

    trigger_update();
}

static void on_node_added(const struct node *node) {
    TRACE("tui_on_node_added: id %d", node->id);

    if (node_is_filtered(node, VEC_DATA(&config.node_filters), VEC_SIZE(&config.node_filters))) {
        return;
    }

    const int tab_index = config.tab_map_enum_to_index[media_class_to_tui_tab(node->media_class)];

    struct tui_tab_item *new_item = xmalloc(sizeof(*new_item));
    *new_item = (struct tui_tab_item){
        .tab_index = tab_index,
        .type = TUI_TAB_ITEM_TYPE_NODE,
        .as.node = {
            .node_id = node->id,
            .n_channels = VEC_SIZE(&node->channels),
        },
    };

    node_events_subscribe(&new_item->node_listener,
                          node->id, NODE_EVENT_ANY,
                          on_node_events, new_item);
    if (node->device_id != 0) {
        device_events_subscribe(&new_item->device_listener,
                                node->device_id, DEVICE_EVENT_CHANGE,
                                on_device_change_for_node, new_item);
    }

    int new_item_height = new_item->as.node.n_channels + 3;
    if (node->device_id != 0) {
        new_item_height += 1;
    }
    LIST_INSERT(&tui.tabs[tab_index].items, &new_item->link);
    tui_tab_item_resize(new_item, new_item_height);

    if (tui.tabs[tab_index].focused == NULL || !tui.tabs[tab_index].user_changed_focus) {
        tui_tab_item_focus(new_item, false, false);
    }

    redraw_current_tab();
}

/* EVENTS FOR DEVICE ITEMS */

static void on_device_remove(struct tui_tab_item *item) {
    TRACE("tui_on_device_removed: id %d", item->as.device.device_id);

    signal_unsubscribe(&item->device_listener);

    tui_tab_item_resize(item, 0);
    tui_tab_item_unfocus(item, false);

    LIST_REMOVE(&item->link);

    if (item->tab_index == tui.tab_index) {
        redraw_current_tab();
    }

    free(item);
}

static void on_device_events_for_device(uint64_t id, uint64_t events,
                                        const struct signal_data *data, void *userdata) {
    struct tui_tab_item *const item = userdata;

    switch ((enum device_event_types)events) {
    case DEVICE_EVENT_CHANGE: {
        tui_tab_item_draw(item, TUI_TAB_ITEM_DRAW_DESCRIPTION);
        break;
    }
    case DEVICE_EVENT_REMOVE: {
        on_device_remove(item);
        break;
    }
    default:
        break;
    }

    trigger_update();
}

static void on_device_added(const struct device *dev) {
    TRACE("tui_on_device_added: id %d", dev->id);

    const int tab_index = config.tab_map_enum_to_index[CARDS];

    struct tui_tab_item *new_item = xmalloc(sizeof(*new_item));
    *new_item = (struct tui_tab_item){
        .tab_index = tab_index,
        .type = TUI_TAB_ITEM_TYPE_DEVICE,
        .as.device = {
            .device_id = dev->id,
        },
    };

    device_events_subscribe(&new_item->device_listener,
                            dev->id, DEVICE_EVENT_ANY,
                            on_device_events_for_device, new_item);

    const int new_item_height = 4;
    LIST_INSERT(&tui.tabs[tab_index].items, &new_item->link);
    tui_tab_item_resize(new_item, new_item_height);

    if (tui.tabs[tab_index].focused == NULL || !tui.tabs[tab_index].user_changed_focus) {
        tui_tab_item_focus(new_item, false, false);
    }

    redraw_current_tab();
}

static void on_pipewire_object_added(uint64_t id, uint64_t event,
                                     const struct signal_data *data, void *_) {
    switch ((enum pipewire_event_types)event) {
    case PIPEWIRE_EVENT_NODE_ADDED: {
        const struct node *node = node_lookup(data->as.u64);
        if (node != NULL) {
            on_node_added(node);
        }
        break;
    }
    case PIPEWIRE_EVENT_DEVICE_ADDED: {
        const struct device *device = device_lookup(data->as.u64);
        if (device != NULL) {
            on_device_added(device);
        }
        break;
    }
    default:
        break;
    }

    trigger_update();
}

static int on_sigwinch(struct pollen_event_source *_, int _, void *_) {
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

    trigger_update();

    return 0;
}

static int on_stdin_ready(struct pollen_event_source *_, int _, uint32_t _, void *_) {
    wint_t ch;
    while (errno = 0, wget_wch(tui.pad_win, &ch) != ERR || errno == EINTR) {
        struct tui_bind *bind = MAP_GET(&config.binds, ch);
        if (bind == NULL) {
            DEBUG("unhandled key %s (%d)", key_name_from_key_code(ch), ch);
        } else if (bind->func == TUI_BIND_QUIT) {
            pollen_loop_quit(event_loop, 0);
        } else {
            bind->func(bind->data);
            trigger_update();
        }
    }

    return 0;
}

/*
 * Trying to optimize updates is brain damage and I don't wanna deal with it.
 * Instead just update after any event that might or might not cause a draw
 * and let ncurses figure out the rest, it's good at damage tracking
 */
static int on_update_triggered(struct pollen_event_source *_, uint64_t _, void *_) {
    tui.efd_triggered = false;

    pnoutrefresh(tui.pad_win,
                 tui.tabs[tui.tab_index].scroll_pos, 0,
                 1, 0,
                 tui.term_height - 1, tui.term_width - 1);

    wnoutrefresh(tui.bar_win);

    if (tui.menu_active) {
        tui_menu_draw(tui.menu);
    }

    doupdate();

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

    pollen_loop_add_fd(event_loop, 0 /* stdin */, EPOLLIN, false, on_stdin_ready, NULL);
    pollen_loop_add_signal(event_loop, SIGWINCH, on_sigwinch, NULL);

    tui.efd_source = pollen_loop_add_efd(event_loop, on_update_triggered, NULL);
    tui.efd_triggered = false;

    pipewire_events_subscribe(&tui.pipewire_listener,
                              PIPEWIRE_EVENT_ID_CORE, PIPEWIRE_EVENT_ANY,
                              on_pipewire_object_added, NULL);

    tui.tab_index = config.tab_map_enum_to_index[config.default_tab];

    /* send SIGWINCH to self to pick up initial terminal size */
    raise(SIGWINCH);

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

