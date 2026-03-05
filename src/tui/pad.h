#pragma once

#include <ncurses.h>

enum tui_set_pad_size_policy {
    EXACTLY,
    AT_LEAST
};

WINDOW *tui_set_pad_size(WINDOW *pad,
                         enum tui_set_pad_size_policy y_policy, int y,
                         enum tui_set_pad_size_policy x_policy, int x);

