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

static void tui_draw_node(struct tui_node_display *disp, bool always_draw, int offset) {
    struct node *node = disp->node;

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

    const bool focused = disp->focused;
    const bool focus_changed = disp->focus_changed;
    const bool muted = node->props.mute;
    const bool unlocked_channels_changed = disp->unlocked_channels_changed;
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
        mvwaddnwstr(win, offset + 1, info_area_start, line, usable_width);
    }

    if (focus_changed || change & NODE_CHANGE_VOLUME || change & NODE_CHANGE_MUTE || always_draw) {
        /* draw info about each channel */
        for (uint32_t i = 0; i < node->props.channel_count; i++) {
            const int pos = offset + i + 2;

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
            const int pos = offset + i + 2;

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
            if (focused && (!disp->unlocked_channels || disp->focused_channel == i)) {
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
        wmove(win, offset, 0);
        waddwstr(win, config.borders.tl);
        for (int x = 1; x < tui.term_width - 1; x++) {
            waddwstr(win, config.borders.ts);
        }
        waddwstr(win, config.borders.tr);

        wmove(win, offset + disp->height - 1, 0);
        waddwstr(win, config.borders.bl);
        for (int x = 1; x < tui.term_width - 1; x++) {
            waddwstr(win, config.borders.bs);
        }
        waddwstr(win, config.borders.br);

        for (int y = 1; y < disp->height - 1; y++) {
            wmove(win, offset + y, 0);
            waddwstr(win, config.borders.ls);
        }

        for (int y = 1; y < disp->height - 1; y++) {
            wmove(win, offset + y, tui.term_width - 1);
            waddwstr(win, config.borders.ls);
        }
    }

    wattroff(win, A_BOLD);
    wattroff(win, COLOR_PAIR(GRAY));

    node->changed = 0;
    disp->focus_changed = false;
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
    if (tui.need_redo_layout) {
        tui_draw_status_bar();
    }

    struct tui_node_display *node_display;
    spa_list_for_each(node_display, &TUI_ACTIVE_TAB.node_displays, link) {
        tui_draw_node(node_display, always_draw, node_display->pos);
    }
    pnoutrefresh(tui.pad_win,
                 TUI_ACTIVE_TAB.scroll_pos, 0,
                 1, 0,
                 tui.term_height - 1, tui.term_width - 1);

    doupdate();

    return 0;
}

static WINDOW *tui_resize_pad(WINDOW *pad, int y, int x) {
    WINDOW *new_pad = newpad(y, x);

    if (pad != NULL) {
        copywin(pad, new_pad, 0, 0, 0, 0, y, x, 0);
        delwin(pad);
    }

    return new_pad;
}

static int tui_create_layout(void) {
    debug("tui: create_layout");

    struct {
        uint32_t id;
        bool found;
    } prev_focused[TUI_TAB_COUNT] = {0};

    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
        tui.bar_win = NULL;
    }
    FOR_EACH_TAB(tab) {
        tui.tabs[tab].focused = NULL;
        tui.tabs[tab].scroll_pos = 0;

        struct tui_node_display *node_display, *node_display_tmp;
        spa_list_for_each_safe(node_display, node_display_tmp,
                               &tui.tabs[tab].node_displays, link) {
            if (node_display->focused) {
                prev_focused[tab].id = node_display->node->id;
            }
            spa_list_remove(&node_display->link);
            free(node_display);
        }
    }

    FOR_EACH_TAB(tab) {
        err("prev_focused[%i]: id %d found %d", tab, prev_focused[tab].id, prev_focused[tab].found);
    }

    tui.bar_win = newwin(1, tui.term_width, 0, 0);
    const int padsize = stbds_hmlenu(pw.nodes) * (SPA_AUDIO_MAX_CHANNELS + 3);
    tui.pad_win = tui_resize_pad(tui.pad_win, padsize, tui.term_width);
    nodelay(tui.pad_win, TRUE); /* getch() will fail instead of blocking waiting for input */
    keypad(tui.pad_win, TRUE);

    for (size_t i = stbds_hmlenu(pw.nodes); i > 0; i--) {
        struct node *node = pw.nodes[i - 1].value;
        enum tui_tab tab = media_class_to_tui_tab(node->media_class);

        struct tui_node_display *node_display = xcalloc(1, sizeof(*node_display));
        node_display->node = node;
        if (!prev_focused[tab].found && prev_focused[tab].id == node->id) {
            node_display->focused = true;
            tui.tabs[tab].focused = node_display;
            prev_focused[tab].found = true;
        }

        node_display->height = node->props.channel_count + 3;
        if (spa_list_is_empty(&tui.tabs[tab].node_displays)) {
            node_display->pos = 0;
        } else {
            struct tui_node_display *first = spa_list_first(&tui.tabs[tab].node_displays,
                                                            struct tui_node_display, link);
            node_display->pos = first->pos + first->height;
        }

        spa_list_insert(&tui.tabs[tab].node_displays, &node_display->link);
    }
    FOR_EACH_TAB(tab) {
        if (!prev_focused[tab].found && !spa_list_is_empty(&tui.tabs[tab].node_displays)) {
            struct tui_node_display *last = spa_list_last(&tui.tabs[tab].node_displays,
                                                          struct tui_node_display, link);
            last->focused = true;
            tui.tabs[tab].focused = last;
        }
    }

    return 0;
}

void tui_bind_change_focus(union tui_bind_data data) {
    enum tui_direction direction = data.direction;

    if (TUI_ACTIVE_TAB.focused == NULL) {
        return;
    }
    struct tui_node_display *f = TUI_ACTIVE_TAB.focused;

    switch (direction) {
    case UP:
        if (f->unlocked_channels && f->focused_channel > 0) {
            f->focused_channel -= 1;
        } else {
            struct tui_node_display *disp, *disp_next = NULL;
            spa_list_for_each_reverse(disp, &TUI_ACTIVE_TAB.node_displays, link) {
                if (disp_next != NULL && disp->focused) {
                    disp->focused = false;
                    disp_next->focused = true;

                    disp->focus_changed = true;
                    disp_next->focus_changed = true;

                    TUI_ACTIVE_TAB.focused = disp_next;

                    if (TUI_ACTIVE_TAB.scroll_pos > disp_next->pos) {
                        TUI_ACTIVE_TAB.scroll_pos = disp_next->pos;
                    }

                    break;
                }

                disp_next = disp;
            }
        }
        break;
    case DOWN:
        if (f->unlocked_channels && f->focused_channel < f->node->props.channel_count - 1) {
            f->focused_channel += 1;
        } else {
            struct tui_node_display *disp, *disp_prev = NULL;
            spa_list_for_each(disp, &TUI_ACTIVE_TAB.node_displays, link) {
                if (disp_prev != NULL && disp->focused) {
                    disp->focused = false;
                    disp_prev->focused = true;

                    disp->focus_changed = true;
                    disp_prev->focus_changed = true;

                    TUI_ACTIVE_TAB.focused = disp_prev;

                    /*            w           +      x      -        y       <         z */
                    if ((tui.term_height - 1) + TUI_ACTIVE_TAB.scroll_pos - disp_prev->pos < disp_prev->height) {
                        /*    x     =         z         -           w           +       y */
                        TUI_ACTIVE_TAB.scroll_pos = disp_prev->height - (tui.term_height - 1) + disp_prev->pos;
                    }

                    break;
                }

                disp_prev = disp;
            }
        }
        break;
    }
}

void tui_bind_focus_first(union tui_bind_data data) {
    if (spa_list_is_empty(&TUI_ACTIVE_TAB.node_displays) || TUI_ACTIVE_TAB.focused == NULL) {
        return;
    }

    struct tui_node_display *first = spa_list_last(&TUI_ACTIVE_TAB.node_displays,
                                                   struct tui_node_display, link);
    if (first == TUI_ACTIVE_TAB.focused) {
        return;
    }

    first->focused = true;
    first->focus_changed = true;

    TUI_ACTIVE_TAB.focused->focused = false;
    TUI_ACTIVE_TAB.focused->focus_changed = true;

    TUI_ACTIVE_TAB.scroll_pos = 0;

    TUI_ACTIVE_TAB.focused = first;
}

void tui_bind_focus_last(union tui_bind_data data) {
    if (spa_list_is_empty(&TUI_ACTIVE_TAB.node_displays) || TUI_ACTIVE_TAB.focused == NULL) {
        return;
    }

    struct tui_node_display *last = spa_list_first(&TUI_ACTIVE_TAB.node_displays,
                                                   struct tui_node_display, link);
    if (last == TUI_ACTIVE_TAB.focused) {
        return;
    }

    last->focused = true;
    last->focus_changed = true;

    TUI_ACTIVE_TAB.focused->focused = false;
    TUI_ACTIVE_TAB.focused->focus_changed = true;

    TUI_ACTIVE_TAB.scroll_pos = last->height - (tui.term_height - 1) + last->pos;

    TUI_ACTIVE_TAB.focused = last;
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
        node_change_volume(TUI_ACTIVE_TAB.focused->node, true, vol, TUI_ACTIVE_TAB.focused->focused_channel);
    } else {
        node_change_volume(TUI_ACTIVE_TAB.focused->node, true, vol, ALL_CHANNELS);
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

    switch (mode) {
    case ENABLE:
        if (!TUI_ACTIVE_TAB.focused->unlocked_channels) {
            TUI_ACTIVE_TAB.focused->unlocked_channels = true;
            TUI_ACTIVE_TAB.focused->unlocked_channels_changed = true;
        }
        break;
    case DISABLE:
        if (TUI_ACTIVE_TAB.focused->unlocked_channels) {
            TUI_ACTIVE_TAB.focused->unlocked_channels = false;
            TUI_ACTIVE_TAB.focused->unlocked_channels_changed = true;
        }
        break;
    case TOGGLE:
        TUI_ACTIVE_TAB.focused->unlocked_channels = !TUI_ACTIVE_TAB.focused->unlocked_channels;
        TUI_ACTIVE_TAB.focused->unlocked_channels_changed = true;
        break;
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
        tui.need_redo_layout = true;
    }
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
        tui.need_redo_layout = true;
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

    tui.need_redo_layout = true;

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

int tui_update(struct event_loop_item *loop_item) {
    if (tui.need_redo_layout || pw.node_list_changed) {
        tui_create_layout();
        tui_repaint(true);

        tui.need_redo_layout = false;
        pw.node_list_changed = false;
    } else {
        tui_repaint(false);
    }

    return 0;
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
        spa_list_init(&tui.tabs[tab].node_displays);
    }

    tui_handle_resize(NULL, 0);

    tui.tab = TUI_TAB_FIRST;
    tui.need_redo_layout = true;

    return 0;
}

int tui_cleanup(void) {
    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
    }
    if (tui.pad_win != NULL) {
        delwin(tui.pad_win);
    }
    FOR_EACH_TAB(tab) {
        if (spa_list_is_initialized(&tui.tabs[tab].node_displays)) {
            struct tui_node_display *node_display, *node_display_tmp;
            spa_list_for_each_safe(node_display, node_display_tmp,
                                   &tui.tabs[tab].node_displays, link) {
                free(node_display);
            }
        }
    }

    endwin();

    return 0;
}

