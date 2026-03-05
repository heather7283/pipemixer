#include "tui/pad.h"
#include "macros.h"
#include "log.h"

WINDOW *tui_set_pad_size(WINDOW *pad,
                         enum tui_set_pad_size_policy y_policy, int y,
                         enum tui_set_pad_size_policy x_policy, int x) {
    TRACE("tui_set_pad_size: y %s %d x %s %d",
          y_policy == EXACTLY ? "exactly" : "at least", y,
          x_policy == EXACTLY ? "exactly" : "at least", x);

    if (!pad) {
        return newpad(y, x);
    }

    int new_y;
    switch (y_policy) {
    case EXACTLY:
        new_y = y;
        break;
    case AT_LEAST:
        new_y = MAX(y, getmaxy(pad));
        break;
    }

    int new_x;
    switch (x_policy) {
    case EXACTLY:
        new_x = x;
        break;
    case AT_LEAST:
        new_x = MAX(x, getmaxx(pad));
        break;
    }

    /* I don't think this is documented anywhere, but from my testing resizing
     * pads with wresize() seems to work fine, their contents are preserved */
    wresize(pad, new_y, new_x);

    return pad;
}

