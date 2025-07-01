#include <assert.h>
#include <math.h>
#include <wchar.h>

#include "tui.h"
#include "macros.h"
#include "log.h"
#include "xmalloc.h"
#include "utils.h"
#include "config.h"
#include "thirdparty/stb_ds.h"

#define TUI_ACTIVE_TAB (tui.tabs[tui.tab])
#define FOR_EACH_TAB(var) for (enum tui_tab var = TUI_TAB_FIRST; var <= TUI_TAB_LAST; var++)

enum color_pair {
    DEFAULT = 0,
    GREEN = 1,
    YELLOW = 2,
    RED = 3,
    GRAY = 4,
};

struct tui tui = {0};

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

static void tui_draw_node(struct tui_tab_item *item, bool always_draw) {
    const struct node *node = item->node;

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
    const bool focus_changed = item->focus_changed;
    const bool muted = node->props.mute;
    const bool unlocked_channels_changed = item->unlocked_channels_changed;
    const enum node_change_mask change = node->changed;

    WINDOW *const win = tui.pad_win;

    if (focused) {
        wattron(win, A_BOLD);
    }
    if (muted) {
        wattron(win, COLOR_PAIR(GRAY));
    }

    wchar_t line[usable_width];

    /* first line displays node name and media name and spans across the entire screen */
    if (focus_changed || change & NODE_CHANGE_INFO || change & NODE_CHANGE_MUTE || always_draw) {
        swprintf(line, ARRAY_SIZE(line), L"(%d) %ls%s%ls%-*s",
                 node->id,
                 node->node_name,
                 WCSEMPTY(node->media_name) ? "" : ": ",
                 WCSEMPTY(node->media_name) ? L"" : node->media_name,
                 usable_width, "");
        wcstrimcols(line, usable_width);
        mvwaddnwstr(win, item->pos + 1, info_area_start, line, usable_width);
    }

    if (focus_changed || change & NODE_CHANGE_VOLUME || change & NODE_CHANGE_MUTE || always_draw) {
        /* draw info about each channel */
        for (uint32_t i = 0; i < node->props.channel_count; i++) {
            const int pos = item->pos + i + 2;

            const int vol_int = (int)roundf(node->props.channel_volumes[i] * 100);

            mvwprintw(win, pos, volume_area_start, "%5s %-3d ",
                      node->props.channel_map[i], vol_int);

            /* draw volume bar */
            int pair = DEFAULT;
            const int step = volume_bar_width / 3;
            const int thresh = vol_int * volume_bar_width / 150;
            for (int j = 0; j < volume_bar_width; j++) {
                cchar_t cc;
                if (j % step == 0 && !node->props.mute) {
                    pair += 1;
                }
                setcchar(&cc, (j < thresh) ? config.bar_full_char : config.bar_empty_char,
                         0, pair, NULL);
                mvwadd_wch(win, pos, volume_bar_start + j, &cc);
            }
        }
    }

    /* draw decorations (also focused markers) */
    if (focus_changed || unlocked_channels_changed || change & NODE_CHANGE_MUTE || always_draw) {
        for (uint32_t i = 0; i < node->props.channel_count; i++) {
            const int pos = item->pos + i + 2;

            const wchar_t *wchar_left, *wchar_right;
            cchar_t cchar_left, cchar_right;

            if (node->props.channel_count == 1) {
                wchar_left = config.volume_frame.ml;
                wchar_right = config.volume_frame.mr;
            } else if (i == 0) {
                wchar_left = config.volume_frame.tl;
                wchar_right = config.volume_frame.tr;
            } else if (i == node->props.channel_count - 1) {
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
    }

    if (change & NODE_CHANGE_MUTE || always_draw) {
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
    wattroff(win, COLOR_PAIR(GRAY));

    item->focus_changed = false;
}

static void tui_draw_status_bar(void) {
    wmove(tui.bar_win, 0, 0);

    FOR_EACH_TAB(tab) {
        wattron(tui.bar_win, (tab == tui.tab) ? COLOR_PAIR(DEFAULT) : COLOR_PAIR(GRAY));
        wattron(tui.bar_win, (tab == tui.tab) ? A_BOLD : 0);
        waddstr(tui.bar_win, tui_tab_name(tab));
        wattroff(tui.bar_win, (tab == tui.tab) ? A_BOLD : 0);
        wattroff(tui.bar_win, (tab == tui.tab) ? COLOR_PAIR(DEFAULT) : COLOR_PAIR(GRAY));
        waddstr(tui.bar_win, "   ");
    }

    wclrtoeol(tui.bar_win);
    wnoutrefresh(tui.bar_win);
}

static int tui_repaint(bool always_draw) {
    TRACE("tui_repaint: always_draw %d", always_draw);

    int bottom = 0;
    struct tui_tab_item *tab_item;
    spa_list_for_each(tab_item, &TUI_ACTIVE_TAB.items, link) {
        tui_draw_node(tab_item, always_draw);
        bottom += tab_item->height;
    }

    /* TODO: inefficient? Only clear on tab change/node removal? */
    wmove(tui.pad_win, bottom, 0);
    wclrtobot(tui.pad_win);
    pnoutrefresh(tui.pad_win,
                 TUI_ACTIVE_TAB.scroll_pos, 0,
                 1, 0,
                 tui.term_height - 1, tui.term_width - 1);

    tui_draw_status_bar();

    doupdate();

    return 0;
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

static void tui_tab_item_set_focused(enum tui_tab tab, struct tui_tab_item *item) {
    if (item->focused) {
        return;
    }

    if (tui.tabs[tab].focused != NULL) {
        tui.tabs[tab].focused->focused = false;
        tui.tabs[tab].focused->focus_changed = true;
    }

    tui.tabs[tab].focused = item;
    item->focused = true;
    item->focus_changed = true;

    tui_tab_item_ensure_visible(tab, item);
}

void tui_bind_change_focus(union tui_bind_data data) {
    enum tui_direction direction = data.direction;

    if (TUI_ACTIVE_TAB.focused == NULL) {
        return;
    }
    struct tui_tab_item *f = TUI_ACTIVE_TAB.focused;

    bool change = false;
    switch (direction) {
    case DOWN:
        if (f->unlocked_channels && f->focused_channel < f->node->props.channel_count - 1) {
            f->focused_channel += 1;
            change = true;
        } else {
            struct tui_tab_item *item, *item_next = NULL;
            spa_list_for_each_reverse(item, &TUI_ACTIVE_TAB.items, link) {
                if (item_next != NULL && item->focused) {
                    tui_tab_item_set_focused(tui.tab, item_next);

                    change = true;
                    break;
                }

                item_next = item;
            }
        }
        break;
    case UP:
        if (f->unlocked_channels && f->focused_channel > 0) {
            f->focused_channel -= 1;
            change = true;
        } else {
            struct tui_tab_item *item, *item_prev = NULL;
            spa_list_for_each(item, &TUI_ACTIVE_TAB.items, link) {
                if (item_prev != NULL && item->focused) {
                    tui_tab_item_set_focused(tui.tab, item_prev);

                    change = true;
                    break;
                }

                item_prev = item;
            }
        }
        break;
    }

    if (change) {
        tui_repaint(true /* TODO: granular redraw */);
    }
}

void tui_bind_focus_last(union tui_bind_data data) {
    if (spa_list_is_empty(&TUI_ACTIVE_TAB.items)) {
        return;
    }

    struct tui_tab_item *first = spa_list_last(&TUI_ACTIVE_TAB.items, struct tui_tab_item, link);
    tui_tab_item_set_focused(tui.tab, first);

    tui_repaint(true /* TODO: granular redraw */);
}

void tui_bind_focus_first(union tui_bind_data data) {
    if (spa_list_is_empty(&TUI_ACTIVE_TAB.items)) {
        return;
    }

    struct tui_tab_item *last = spa_list_first(&TUI_ACTIVE_TAB.items, struct tui_tab_item, link);
    tui_tab_item_set_focused(tui.tab, last);

    tui_repaint(true /* TODO: granular redraw */);
}

void tui_bind_change_volume(union tui_bind_data data) {
    enum tui_direction direction = data.direction;

    if (TUI_ACTIVE_TAB.focused == NULL) {
        return;
    }

    float delta = (direction == UP) ? config.volume_step : -config.volume_step;

    if (TUI_ACTIVE_TAB.focused->unlocked_channels) {
        node_change_volume(TUI_ACTIVE_TAB.focused->node, false, delta, TUI_ACTIVE_TAB.focused->focused_channel);
    } else {
        node_change_volume(TUI_ACTIVE_TAB.focused->node, false, delta, ALL_CHANNELS);
    }
}

void tui_bind_set_volume(union tui_bind_data data) {
    float vol = data.volume;

    if (TUI_ACTIVE_TAB.focused == NULL) {
        return;
    }

    if (TUI_ACTIVE_TAB.focused->unlocked_channels) {
        node_change_volume(TUI_ACTIVE_TAB.focused->node,
                           true, vol, TUI_ACTIVE_TAB.focused->focused_channel);
    } else {
        node_change_volume(TUI_ACTIVE_TAB.focused->node,
                           true, vol, ALL_CHANNELS);
    }
}

void tui_bind_change_mute(union tui_bind_data data) {
    enum tui_change_mode mode = data.change_mode;

    if (TUI_ACTIVE_TAB.focused == NULL) {
        return;
    }

    switch (mode) {
    case ENABLE:
        node_set_mute(TUI_ACTIVE_TAB.focused->node, true);
        break;
    case DISABLE:
        node_set_mute(TUI_ACTIVE_TAB.focused->node, false);
        break;
    case TOGGLE:
        node_set_mute(TUI_ACTIVE_TAB.focused->node, !TUI_ACTIVE_TAB.focused->node->props.mute);
        break;
    }
}

void tui_bind_change_channel_lock(union tui_bind_data data) {
    enum tui_change_mode mode = data.change_mode;

    if (TUI_ACTIVE_TAB.focused == NULL) {
        return;
    }

    bool change = false;
    switch (mode) {
    case ENABLE:
        if (!TUI_ACTIVE_TAB.focused->unlocked_channels) {
            TUI_ACTIVE_TAB.focused->unlocked_channels = true;
            TUI_ACTIVE_TAB.focused->unlocked_channels_changed = true;
            change = true;
        }
        break;
    case DISABLE:
        if (TUI_ACTIVE_TAB.focused->unlocked_channels) {
            TUI_ACTIVE_TAB.focused->unlocked_channels = false;
            TUI_ACTIVE_TAB.focused->unlocked_channels_changed = true;
            change = true;
        }
        break;
    case TOGGLE:
        TUI_ACTIVE_TAB.focused->unlocked_channels = !TUI_ACTIVE_TAB.focused->unlocked_channels;
        TUI_ACTIVE_TAB.focused->unlocked_channels_changed = true;
        change = true;
        break;
    }

    if (change) {
        TUI_ACTIVE_TAB.scroll_pos = 0;

        tui_repaint(true /* TODO: granular redraw */);
    }
}

void tui_bind_change_tab(union tui_bind_data data) {
    bool change = false;
    switch (data.direction) {
    case UP:
        if (tui.tab++ == TUI_TAB_LAST) {
            tui.tab = TUI_TAB_FIRST;
        }
        change = true;
        break;
    case DOWN:
        if (tui.tab-- == TUI_TAB_FIRST) {
            tui.tab = TUI_TAB_LAST;
        }
        change = true;
        break;
    default:
        assert(0 && "Invalid tab enum value passed to tui_bind_change_tab");
    }

    if (change) {
        TUI_ACTIVE_TAB.scroll_pos = 0;
    }

    tui_repaint(true /* TODO: granular redraw */);
}

void tui_bind_set_tab(union tui_bind_data data) {
    enum tui_tab tab = data.tab;

    bool change = false;
    switch (tab) {
    case PLAYBACK:
        if (tui.tab != PLAYBACK) {
            tui.tab = PLAYBACK;
            change = true;
        }
        break;
    case RECORDING:
        if (tui.tab != RECORDING) {
            tui.tab = RECORDING;
            change = true;
        }
        break;
    case INPUT_DEVICES:
        if (tui.tab != INPUT_DEVICES) {
            tui.tab = INPUT_DEVICES;
            change = true;
        }
        break;
    case OUTPUT_DEVICES:
        if (tui.tab != OUTPUT_DEVICES) {
            tui.tab = OUTPUT_DEVICES;
            change = true;
        }
        break;
    default:
        assert(0 && "Invalid tab enum value passed to tui_bind_change_tab");
    }

    if (change) {
        TUI_ACTIVE_TAB.scroll_pos = 0;

        tui_repaint(true /* TODO: granular redraw */);
    }
}

static WINDOW *tui_resize_pad(WINDOW *pad, int y, int x, bool keep_contents) {
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
        int new_x, new_y, max_x, max_y;

        switch (y_policy) {
        case EXACTLY:
            new_y = y;
            break;
        case AT_LEAST:
            max_y = getmaxy(tui.pad_win);
            new_y = MAX(max_y, y);
            break;
        }

        switch (x_policy) {
        case EXACTLY:
            new_x = x;
            break;
        case AT_LEAST:
            max_x = getmaxx(tui.pad_win);
            new_x = MAX(max_x, x);
            break;
        }

        tui.pad_win = tui_resize_pad(tui.pad_win, new_y, new_x, keep_contents);
    }
}

int tui_handle_resize(struct event_loop_item *item, int signal) {
    struct winsize winsize;
    if (ioctl(0 /* stdin */, TIOCGWINSZ, &winsize) < 0) {
        err("failed to get new window size: %s", strerror(errno));
        return -1;
    }

    resize_term(winsize.ws_row, winsize.ws_col);
    tui.term_height = getmaxy(stdscr);
    tui.term_width = getmaxx(stdscr);
    debug("new window dimensions %d lines %d columns", tui.term_height, tui.term_width);

    tui_set_pad_size(AT_LEAST, tui.term_height, EXACTLY, tui.term_width, false);
    if (TUI_ACTIVE_TAB.focused != NULL) {
        tui_tab_item_ensure_visible(tui.tab, TUI_ACTIVE_TAB.focused);
    }

    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
    }
    tui.bar_win = newwin(1, tui.term_width, 0, 0);

    tui_repaint(true /* TODO: granular redraw */);

    return 0;
}

int tui_handle_keyboard(struct event_loop_item *item, uint32_t events) {
    wint_t ch;
    while (errno = 0, wget_wch(tui.pad_win, &ch) != ERR || errno == EINTR) {
        struct pipemixer_config_bind *bind = stbds_hmgetp_null(config.binds, ch);
        if (bind == NULL) {
            debug("unhandled key %s (%d)", key_name_from_key_code(ch), ch);
        } else if (bind->value.func == TUI_BIND_QUIT) {
            event_loop_quit(event_loop_item_get_loop(item), 0);
        } else {
            bind->value.func(bind->value.data);
        }
    }

    return 0;
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

    struct tui_tab_item *next;
    spa_list_for_each_next(next, &tui.tabs[tab].items, &item->link, link) {
        TRACE("tui_tab_item_resize: shifting item %p from %d to %d",
              (void *)next, next->pos, next->pos + diff);
        next->pos += diff;
    }

    struct tui_tab_item *last = spa_list_last(&tui.tabs[tab].items, TYPEOF(*last), link);
    tui_set_pad_size(AT_LEAST, last->pos + last->height, AT_LEAST, tui.term_width, true);
}

void tui_notify_node_new(const struct node *node) {
    TRACE("tui_notify_node_new: id %d", node->id);

    enum tui_tab tab = media_class_to_tui_tab(node->media_class);

    struct tui_tab_item *new_item = xcalloc(1, sizeof(*new_item));
    new_item->node = node;
    new_item->pos = 0;

    if (tui.tabs[tab].focused == NULL) {
        new_item->focused = true;
        tui.tabs[tab].focused = new_item;
    }
    spa_list_insert(&tui.tabs[tab].items, &new_item->link);
    tui_tab_item_resize(tab, new_item, node->props.channel_count + 3);

    if (tab == tui.tab) {
        tui_repaint(true /* TODO: granular redraw */);
    }
}

void tui_notify_node_change(const struct node *node) {
    TRACE("tui_notify_node_change: id %d", node->id);

    enum tui_tab tab = media_class_to_tui_tab(node->media_class);

    /* find tui_tab_item associated with this node (FIXME: slow? do I even care?) */
    bool found = false;
    struct tui_tab_item *item;
    spa_list_for_each(item, &tui.tabs[tab].items, link) {
        if (item->node == node) {
            found = true;
            break;
        }
    }
    if (!found) {
        warn("got notify_node_change for node id %d but no tui_tab_item found", node->id);
        return;
    }

    tui_tab_item_resize(tab, item, node->props.channel_count + 3);

    if (tab == tui.tab) {
        tui_repaint(true /* TODO: granular redraw */);
    }
}

void tui_notify_node_remove(const struct node *node) {
    TRACE("tui_notify_node_remove: id %d", node->id);

    enum tui_tab tab = media_class_to_tui_tab(node->media_class);

    /* find tui_tab_item associated with this node (FIXME: slow? do I even care?) */
    bool found = false;
    struct tui_tab_item *item;
    spa_list_for_each(item, &tui.tabs[tab].items, link) {
        if (item->node == node) {
            found = true;
            break;
        }
    }
    if (!found) {
        warn("got notify_node_change for node id %d but no tui_tab_item found", node->id);
        return;
    }

    tui_tab_item_resize(tab, item, 0);
    spa_list_remove(&item->link);

    if (!spa_list_is_empty(&tui.tabs[tab].items) && item->focused) {
        struct tui_tab_item *first = spa_list_first(&tui.tabs[tab].items, TYPEOF(*first), link);
        first->focused = true;
        tui.tabs[tab].focused = first;
    }

    free(item);

    if (tab == tui.tab) {
        tui_repaint(true /* TODO: granular redraw */);
    }
}

int tui_init(void) {
    initscr();
    refresh(); /* https://stackoverflow.com/a/22121866 */
    cbreak();
    noecho();
    curs_set(0);

    start_color();
    use_default_colors();
    init_pair(GREEN, COLOR_GREEN, -1);
    init_pair(YELLOW, COLOR_YELLOW, -1);
    init_pair(RED, COLOR_RED, -1);
    init_pair(GRAY, 8, -1);

    FOR_EACH_TAB(tab) {
        spa_list_init(&tui.tabs[tab].items);
    }

    tui_handle_resize(NULL, 0);

    tui.tab = TUI_TAB_FIRST;
    tui_repaint(true /* TODO: granular redraw */);

    return 0;
}

int tui_cleanup(void) {
    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
    }
    if (tui.pad_win != NULL) {
        delwin(tui.pad_win);
    }
    /* TODO: fix this.
     * It gets called in pipewire_cleanup and shits itself because items are already deleted
     */
    FOR_EACH_TAB(tab) {
        if (spa_list_is_initialized(&tui.tabs[tab].items)) {
            struct tui_tab_item *tab_item, *tab_item_tmp;
            spa_list_for_each_safe(tab_item, tab_item_tmp, &tui.tabs[tab].items, link) {
                spa_list_remove(&tab_item->link);
                free(tab_item);
            }
        }
    }

    endwin();

    return 0;
}

