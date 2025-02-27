#include "tui.h"
#include "log.h"

void tui_repaint_all(struct tui *tui, struct spa_list *node_list) {
    debug("tui: repainting and updating everything");

    WINDOW *pad = tui->pad_win;
    int pad_pos = 0;

    struct node *node;
    spa_list_for_each(node, node_list, link) {
        mvwprintw(pad, pad_pos++, 0, "(%d) %s: %s",
                  node->id, node->application_name, node->media_name);
        for (uint32_t i = 0; i < node->props.channel_count; i++) {
            mvwprintw(pad, pad_pos++, 0, "    %s: %f",
                      node->props.channel_map[i], node->props.channel_volumes[i]);
        }
        mvwprintw(pad, pad_pos++, 0, "%*s", MAX_SCREEN_WIDTH, ""); /* fill with spaces */
    }

    pnoutrefresh(tui->pad_win, tui->pad_pos, 0, 1, 0, tui->term_height - 1, tui->term_width - 1);
    doupdate();
}

