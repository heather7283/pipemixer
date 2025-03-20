#include <math.h>
#include <wchar.h>

#include "tui.h"
#include "pw.h"
#include "macros.h"
#include "log.h"
#include "xmalloc.h"
#include "utils.h"
#include "thirdparty/stb_ds.h"

enum color_pair {
    DEFAULT = 0,
    GREEN = 1,
    YELLOW = 2,
    RED = 3,
    GRAY = 4,
};

struct tui tui = {0};

static const char *media_class_name(enum media_class class) {
    switch (class) {
    case STREAM_OUTPUT_AUDIO: return "Playback";
    case STREAM_INPUT_AUDIO: return "Recording";
    case AUDIO_SINK: return "Output Devices";
    case AUDIO_SOURCE: return "Input Devices";
    default: return "Invalid";
    }
}

void tui_draw_node(struct tui_node_display *disp, bool always_draw) {
    struct node *node = disp->node;

    /*
     * On a 80-character-wide term it will look like this:
     * 0                                                                               80
     * ┌──────────────────────────────────────────────────────────────────────────────┐
     * │(35) Equalizer Sink: Equalizer Sink                                           │
     * │                                                    FL 100 ─┌##########-----┐─│
     * │                                                    FR 100 ─└##########-----┘─│
     * └──────────────────────────────────────────────────────────────────────────────┘
     */
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

    WINDOW *win = disp->win;

    if (focused) {
        wattron(disp->win, A_BOLD);
    }
    if (muted) {
        wattron(disp->win, COLOR_PAIR(GRAY));
    }

    wchar_t line[usable_width];

    /* first line displays node name and media name and spans across the entire screen */
    if (focus_changed || change & NODE_CHANGE_INFO || change & NODE_CHANGE_MUTE || always_draw) {
        debug("tui: node %d: drawing top line", node->id);
        swprintf(line, ARRAY_SIZE(line), L"(%d) %ls%s%ls%-*s",
                 node->id,
                 node->node_name,
                 wcsempty(node->media_name) ? "" : ": ",
                 wcsempty(node->media_name) ? L"" : node->media_name,
                 usable_width, "");
        wcstrimcols(line, usable_width);
        mvwaddnwstr(win, 1, info_area_start, line, usable_width);
    }

    if (focus_changed || change & NODE_CHANGE_VOLUME || change & NODE_CHANGE_MUTE || always_draw) {
        /* draw info about each channel */
        debug("tui: node %d: drawing channel info", node->id);
        for (uint32_t i = 0; i < node->props.channel_count; i++) {
            const int pos = i + 2;

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
                setcchar(&cc, (j < thresh) ? L"#" : L"-", 0, pair, NULL);
                mvwadd_wch(win, pos, volume_bar_start + j, &cc);
            }
        }
    }

    /* draw decorations (also focused markers) */
    if (focus_changed || unlocked_channels_changed || change & NODE_CHANGE_MUTE || always_draw) {
        debug("tui: node %d: drawing decorations", node->id);

        for (uint32_t i = 0; i < node->props.channel_count; i++) {
            const int pos = i + 2;

            const wchar_t *wchar_left, *wchar_right;
            cchar_t cchar_left, cchar_right;

            if (node->props.channel_count == 1) {
                wchar_left = L"╶";
                wchar_right = L"╴";
            } else if (i == 0) {
                wchar_left = L"┌";
                wchar_right = L"┐";
            } else if (i == node->props.channel_count - 1) {
                wchar_left = L"└";
                wchar_right = L"┘";
            } else {
                wchar_left = L"├";
                wchar_right = L"┤";
            }
            setcchar(&cchar_left, wchar_left, 0, DEFAULT, NULL);
            setcchar(&cchar_right, wchar_right, 0, DEFAULT, NULL);
            mvwadd_wch(win, pos, volume_bar_start - 1, &cchar_left);
            mvwadd_wch(win, pos, volume_bar_start + volume_bar_width, &cchar_right);

            const wchar_t *wchar_focus;
            if (focused && (!disp->unlocked_channels || disp->focused_channel == i)) {
                wchar_focus = L"─";
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
        box(win, 0, 0);
    }

    wattroff(win, A_BOLD);
    wattroff(win, COLOR_PAIR(GRAY));

    node->changed = 0;
    disp->focus_changed = false;
}

void tui_draw_status_bar(void) {
    debug("tui: drawing status bar");

    wmove(tui.bar_win, 0, 0);
    enum media_class tab = MEDIA_CLASS_START;
    while (++tab != MEDIA_CLASS_END) {
        wattron(tui.bar_win, (tab == tui.active_tab) ? COLOR_PAIR(DEFAULT) : COLOR_PAIR(GRAY));
        wattron(tui.bar_win, (tab == tui.active_tab) ? A_BOLD : 0);
        waddstr(tui.bar_win, media_class_name(tab));
        wattroff(tui.bar_win, (tab == tui.active_tab) ? A_BOLD : 0);
        wattroff(tui.bar_win, (tab == tui.active_tab) ? COLOR_PAIR(DEFAULT) : COLOR_PAIR(GRAY));
        waddstr(tui.bar_win, "   ");
    }
    wclrtoeol(tui.bar_win);
    wnoutrefresh(tui.bar_win);
}

int tui_repaint(bool always_draw) {
    debug("tui: repaint, always_draw %d", always_draw);

    if (tui.need_redo_layout) {
        tui_draw_status_bar();
    }

    struct tui_node_display *node_display;
    spa_list_for_each(node_display, &tui.node_displays, link) {
        tui_draw_node(node_display, always_draw);
    }
    pnoutrefresh(tui.pad_win, tui.pad_pos, 0, 1, 0, tui.term_height - 1, tui.term_width - 1);

    doupdate();

    return 0;
}

int tui_create_layout(void) {
    debug("tui: create_layout");

    uint32_t prev_focused_id;
    if (tui.focused != NULL) {
        prev_focused_id = tui.focused->node->id;
    } else {
        prev_focused_id = 0;
    }
    tui.focused = NULL;

    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
        tui.bar_win = NULL;
    }
    struct tui_node_display *node_display, *node_display_tmp;
    spa_list_for_each_safe(node_display, node_display_tmp, &tui.node_displays, link) {
        delwin(node_display->win);
        spa_list_remove(&node_display->link);
        free(node_display);
    }
    if (tui.pad_win != NULL) {
        delwin(tui.pad_win);
        tui.pad_win = NULL;
    }

    tui.bar_win = newwin(1, tui.term_width, 0, 0);
    tui.pad_win = newpad(stbds_hmlenu(pw.nodes) * (SPA_AUDIO_MAX_CHANNELS + 3), tui.term_width);
    nodelay(tui.pad_win, TRUE); /* getch() will fail instead of blocking waiting for input */
    keypad(tui.pad_win, TRUE);

    bool focused_found = false;
    int pos_y = 0;
    for (size_t i = stbds_hmlenu(pw.nodes); i > 0; i--) {
        struct node *node = pw.nodes[i - 1].value;
        if (node->media_class != tui.active_tab) {
            continue;
        }

        struct tui_node_display *node_display = xcalloc(1, sizeof(*node_display));
        node_display->node = node;
        if (!focused_found && prev_focused_id == node->id) {
            node_display->focused = true;
            tui.focused = node_display;
            focused_found = true;
        }

        int subwin_height = node->props.channel_count + 3;
        node_display->win = subpad(tui.pad_win, subwin_height, tui.term_width, pos_y, 0);
        node_display->pos = pos_y;
        node_display->height = subwin_height;
        pos_y += subwin_height;

        spa_list_insert(&tui.node_displays, &node_display->link);
    }
    if (!focused_found) {
        struct tui_node_display *disp;
        spa_list_for_each_reverse(disp, &tui.node_displays, link) {
            disp->focused = true;
            tui.focused = disp;
            break;
        }
    }

    return 0;
}

bool tui_focus_next(void) {
    struct tui_node_display *disp, *disp_next = NULL;
    spa_list_for_each_reverse(disp, &tui.node_displays, link) {
        if (disp_next != NULL && disp->focused) {
            disp->focused = false;
            disp_next->focused = true;

            disp->focus_changed = true;
            disp_next->focus_changed = true;

            tui.focused = disp_next;

            if (tui.pad_pos > disp_next->pos) {
                tui.pad_pos = disp_next->pos;
            }

            return true;
        }

        disp_next = disp;
    }

    return false;
}

bool tui_focus_prev(void) {
    struct tui_node_display *disp, *disp_prev = NULL;
    spa_list_for_each(disp, &tui.node_displays, link) {
        if (disp_prev != NULL && disp->focused) {
            disp->focused = false;
            disp_prev->focused = true;

            disp->focus_changed = true;
            disp_prev->focus_changed = true;

            tui.focused = disp_prev;

            /*            w           +      x      -        y       <         z */
            if ((tui.term_height - 1) + tui.pad_pos - disp_prev->pos < disp_prev->height) {
                /*    x     =         z         -           w           +       y */
                tui.pad_pos = disp_prev->height - (tui.term_height - 1) + disp_prev->pos;
            }

            return true;
        }

        disp_prev = disp;
    }

    return false;
}

void tui_next_tab(void) {
    if (++tui.active_tab == MEDIA_CLASS_END) {
        tui.active_tab = MEDIA_CLASS_START + 1;
    }

    tui.need_redo_layout = true;
}

int tui_handle_resize(struct event_loop_item *item, int signal) {
    debug("window resized");

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
    int ch;
    while (errno = 0, (ch = wgetch(tui.pad_win)) != ERR || errno == EINTR) {
        switch (ch) {
        case KEY_RESIZE:
            warn("WHY AM I GETTING KEY_RESIZE ???");
            break;
        case 'j':
        case KEY_DOWN:
            if (tui.focused->unlocked_channels) {
                if (tui.focused->focused_channel == tui.focused->node->props.channel_count - 1) {
                    tui_focus_prev();
                } else {
                    tui.focused->focused_channel += 1;
                }
            } else {
                tui_focus_prev();
            }
            break;
        case 'k':
        case KEY_UP:
            if (tui.focused->unlocked_channels) {
                if (tui.focused->focused_channel == 0) {
                    tui_focus_next();
                } else {
                    tui.focused->focused_channel -= 1;
                }
            } else {
                tui_focus_next();
            }
            break;
        case 't':
        case '\t':
            tui_next_tab();
            break;
        case 'm':
            node_toggle_mute(tui.focused->node);
            break;
        case 'l':
        case KEY_RIGHT:
            if (tui.focused->unlocked_channels) {
                node_change_volume(tui.focused->node, 0.01, tui.focused->focused_channel);
            } else {
                node_change_volume(tui.focused->node, 0.01, ALL_CHANNELS);
            }
            break;
        case 'h':
        case KEY_LEFT:
            if (tui.focused->unlocked_channels) {
                node_change_volume(tui.focused->node, -0.01, tui.focused->focused_channel);
            } else {
                node_change_volume(tui.focused->node, -0.01, ALL_CHANNELS);
            }
            break;
        case ' ':
            tui.focused->unlocked_channels =
                    !tui.focused->unlocked_channels;
            tui.focused->unlocked_channels_changed = true;
            break;
        case 'q':
            event_loop_quit(event_loop_item_get_loop(item), 0);
            break;
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
    setlocale(LC_ALL, ""); /* needed for unicode support in ncurses */

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

    spa_list_init(&tui.node_displays);
    tui.term_width = getmaxx(stdscr);
    tui.term_height = getmaxy(stdscr);

    tui.active_tab = MEDIA_CLASS_START + 1;
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
    struct tui_node_display *node_display, *node_display_tmp;
    spa_list_for_each_safe(node_display, node_display_tmp, &tui.node_displays, link) {
        delwin(node_display->win);
        free(node_display);
    }

    endwin();

    return 0;
}

