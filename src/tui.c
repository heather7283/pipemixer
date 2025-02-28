#include <math.h>

#include "tui.h"
#include "macros.h"
#include "log.h"

void tui_draw_node(struct tui *tui, struct node *node, int *pad_pos) {
    /*
     * On a 80-character-wide term it will look like this:
     *                                                                                 80 chars
     *                                          volume area start (<= 50%)             │
     *                                          │                                      │
     * Chromium: Playback                       75  ─┌▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮---------------┐─ │
     *                                          75  ─└▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮---------------┘─ │
     *                                          │    │                                 │
     *                                         vol.deco (6 + 3 at the end)
     */
    int half_term_width = tui->term_width / 2;
    int volume_area_deco_width_left = 6;
    int volume_area_deco_width_right = 3;
    int volume_area_deco_width = volume_area_deco_width_left + volume_area_deco_width_right;
    int volume_area_width_max_without_deco = half_term_width - volume_area_deco_width;
    int volume_area_width_without_deco = (volume_area_width_max_without_deco / 15) * 15;
    int volume_area_width = volume_area_width_without_deco + volume_area_deco_width;
    int info_area_width = tui->term_width - volume_area_width - 1;

    for (uint32_t i = 0; i < node->props.channel_count; i++) {
        wchar_t volume_area[volume_area_width];

        bool focused = false;
        int vol_int = (int)roundf(node->props.channel_volumes[i] * 100);

        swprintf(volume_area, ARRAY_SIZE(volume_area),
                 L"%-3d %lc%lc%*ls%lc%lc",
                 vol_int,
                 focused ? L'─' : L' ',
                 (i == 0 ? L'┌' : (i == node->props.channel_count - 1 ? L'└' : L'│')),
                 volume_area_width_without_deco, L"", /* empty space */
                 (i == 0 ? L'┐' : (i == node->props.channel_count - 1 ? L'┘' : L'│')),
                 focused ? L'─' : L' ');

        int thresh = vol_int * volume_area_width_without_deco / 150;
        for (int j = 0; j < volume_area_width_without_deco; j++) {
            wchar_t *p = volume_area + volume_area_deco_width_left + j;
            *p = j < thresh ? L'#' : L'-';
        }

        debug("%ls", volume_area);

        mvwprintw(tui->pad_win, (*pad_pos)++, info_area_width + 1, "%ls", volume_area);
    }
}

void tui_repaint_all(struct tui *tui, struct spa_list *node_list) {
    debug("tui: repainting and updating everything");

    WINDOW *pad = tui->pad_win;
    int pad_pos = 0;

    struct node *node;
    spa_list_for_each(node, node_list, link) {
        tui_draw_node(tui, node, &pad_pos);
        //mvwprintw(pad, pad_pos++, 0, "(%d) %s: %s",
        //          node->id, node->application_name, node->media_name);
        //for (uint32_t i = 0; i < node->props.channel_count; i++) {
        //    mvwprintw(pad, pad_pos++, 0, "    %s: %f",
        //              node->props.channel_map[i], node->props.channel_volumes[i]);
        //}
        //mvwprintw(pad, pad_pos++, 0, "%*s", MAX_SCREEN_WIDTH, ""); /* fill with spaces */
    }

    pnoutrefresh(tui->pad_win, tui->pad_pos, 0, 1, 0, tui->term_height - 1, tui->term_width - 1);
    doupdate();
}

