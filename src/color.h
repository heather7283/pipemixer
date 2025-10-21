#pragma once

struct tui_color {
    unsigned char r, g, b;
};

void tui_init_colors(void);

int tui_define_color(struct tui_color *fg, struct tui_color *bg);

