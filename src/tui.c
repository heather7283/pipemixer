#include <math.h>
#include <wchar.h>

#include "tui.h"
#include "pw.h"
#include "macros.h"
#include "log.h"
#include "xmalloc.h"
#include "thirdparty/stb_ds.h"

struct tui tui = {0};

void tui_draw_node(struct tui_node_display *disp) {
    struct node *node = stbds_hmget(pw.nodes, disp->node_id);

    debug("drawing node %d, disp.focused %d", node->id, disp->focused);

    /*
     * On a 80-character-wide term it will look like this:
     *                                                                                 80 chars
     *                                    volume area start (<= 50%)                   │
     *                                    │                                            │
     * Chromium: Playback                 AUX68 75  ─┌▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮---------------┐─ │
     *                                    AUX69 75  ─└▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮---------------┘─ │
     *                                    │          │                                 │
     *                                    vol.deco (12 + 3 at the end)
     */
    int half_term_width = tui.term_width / 2;
    int volume_area_deco_width_left = 12;
    int volume_area_deco_width_right = 3;
    int volume_area_deco_width = volume_area_deco_width_left + volume_area_deco_width_right;
    int volume_area_width_max_without_deco = half_term_width - volume_area_deco_width;
    int volume_area_width_without_deco = (volume_area_width_max_without_deco / 15) * 15;
    int volume_area_width = volume_area_width_without_deco + volume_area_deco_width;
    int info_area_width = tui.term_width - volume_area_width - 1;

    mvwprintw(disp->win, 1, 1, "(%d) %s: %s", node->id, node->application_name, node->media_name);

    box(disp->win, 0, 0);

    for (uint32_t i = 0; i < node->props.channel_count; i++) {
        wchar_t volume_area[volume_area_width];

        int vol_int = (int)roundf(node->props.channel_volumes[i] * 100);

        swprintf(volume_area, ARRAY_SIZE(volume_area),
                 L"%5s %-3d %lc%lc%*ls%lc%lc",
                 node->props.channel_map[i],
                 vol_int,
                 disp->focused ? L'─' : L' ',
                 (i == 0 ? L'┌' : (i == node->props.channel_count - 1 ? L'└' : L'├')),
                 volume_area_width_without_deco, L"", /* empty space */
                 (i == 0 ? L'┐' : (i == node->props.channel_count - 1 ? L'┘' : L'┤')),
                 disp->focused ? L'─' : L' ');

        int thresh = vol_int * volume_area_width_without_deco / 150;
        for (int j = 0; j < volume_area_width_without_deco; j++) {
            wchar_t *p = volume_area + volume_area_deco_width_left + j;
            *p = j < thresh ? L'#' : L'-';
        }

        mvwprintw(disp->win, i + 2, info_area_width + 1, "%ls", volume_area);
    }
}

int tui_repaint_all(void) {
    debug("tui: repainting and updating everything");

    struct tui_node_display *node_display;
    spa_list_for_each(node_display, &tui.node_displays, link) {
        tui_draw_node(node_display);
    }

    mvwprintw(tui.bar_win, 0, 0, "Status Bar (Terminal: %dx%d)", tui.term_width, tui.term_height);
    wclrtoeol(tui.bar_win);
    wnoutrefresh(tui.bar_win);

    pnoutrefresh(tui.pad_win, tui.pad_pos, 0, 1, 0, tui.term_height - 1, tui.term_width - 1);

    doupdate();

    return 0;
}

int tui_create_layout(void) {
    debug("tui: create_layout");

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
    nodelay(tui.pad_win, TRUE);

    int pos_y = 0;
    for (size_t i = stbds_hmlenu(pw.nodes); i > 0; i--) {
        struct node *node = pw.nodes[i - 1].value;

        struct tui_node_display *node_display = xcalloc(1, sizeof(*node_display));
        node_display->node_id = node->id;
        node_display->focused = i == stbds_hmlenu(pw.nodes);

        int subwin_height = node->props.channel_count + 3;
        node_display->win = subpad(tui.pad_win, subwin_height, tui.term_width, pos_y, 0);
        pos_y += subwin_height;

        spa_list_insert(&tui.node_displays, &node_display->link);
    }

    return 0;
}

bool tui_focus_next(void) {
    struct tui_node_display *disp, *disp_next = NULL;
    spa_list_for_each_reverse(disp, &tui.node_displays, link) {
        if (disp_next != NULL && disp->focused) {
            disp->focused = false;
            disp_next->focused = true;
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
            return true;
        }

        disp_prev = disp;
    }

    return false;
}

int tui_init(void) {
    setlocale(LC_ALL, ""); /* needed for unicode support in ncurses */

    initscr();
    refresh(); /* https://stackoverflow.com/a/22121866 */
    cbreak();
    noecho();
    curs_set(0);

    nodelay(stdscr, TRUE); /* getch() will fail instead of blocking waiting for input */
    keypad(stdscr, TRUE);

    spa_list_init(&tui.node_displays);
    tui.term_width = getmaxx(stdscr);
    tui.term_height = getmaxy(stdscr);

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

