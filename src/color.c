#include <ncurses.h>

#include "color.h"
#include "log.h"

#define CONVERT(color) (color * 1000 / 256)

void tui_init_colors(void) {
    start_color();
    use_default_colors();
    DEBUG("COLORS = %d; COLOR_PAIRS = %d", COLORS, COLOR_PAIRS);
}

int tui_define_color(struct tui_color *fg, struct tui_color *bg) {
    static int last_color = 8; /* skip standard ascii colors? idk what I'm doing honestly */
    static int last_pair = 1;

    int f = -1;
    if (fg != NULL) {
        f = last_color++;
        if (init_extended_color(f, CONVERT(fg->r), CONVERT(fg->g), CONVERT(fg->b)) == ERR) {
            ERROR("init_extended_color(%d, %d, %d, %d)",
                  f, CONVERT(fg->r), CONVERT(fg->g), CONVERT(fg->b));
        }
    }
    int b = -1;
    if (bg != NULL) {
        b = last_color++;
        if (init_extended_color(b, CONVERT(bg->r), CONVERT(bg->g), CONVERT(bg->b)) == ERR) {
            ERROR("init_extended_color(%d, %d, %d, %d)",
                  f, CONVERT(bg->r), CONVERT(bg->g), CONVERT(bg->b));
        }
    }
    const int p = last_pair++;
    init_extended_pair(p, f, b);

    DEBUG("tui_define_color: pair %d ( fg %d bg %d )", p, f, b);

    return p;
}

