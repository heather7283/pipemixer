#include <ncurses.h>
#include <stdlib.h>
#include <spa/utils/list.h>

#define PAD_HEIGHT 100
#define SUBWIN_HEIGHT 3
#define SUBWIN_COUNT  (PAD_HEIGHT / SUBWIN_HEIGHT)

struct tui_subwin {
    WINDOW *win;

    struct spa_list link;
};

struct tui {
    WINDOW *pad;
    WINDOW *bar;
    WINDOW *subwins[SUBWIN_COUNT];

    int pad_top;
};

void tui_init(struct tui *tui) {
    initscr();            // Initialize ncurses
    refresh();            // https://stackoverflow.com/a/22121866
    noecho();             // Disable echoing of typed characters
    cbreak();             // Disable line buffering
    curs_set(0);          // Hide the cursor
    keypad(stdscr, TRUE); // Enable special keys (e.g., arrow keys)
}

void tui_repaint_all(struct tui *tui) {
    // Populate the pad with subwindows
    for (int i = 0; i < SUBWIN_COUNT; ++i) {
        // Create a border around the subwindow
        box(tui->subwins[i], 0, 0);

        // Display the window number in the center of the subwindow
        mvwprintw(tui->subwins[i], 1, 1, "Window Number %d", i + 1);
    }

    mvwprintw(tui->bar, 0, 0, "SUSSY AMOGUS IMPOSTER pad_top = %d COLS %d LINES %d",
              tui->pad_top, COLS, LINES);
    wclrtoeol(tui->bar);
    wnoutrefresh(tui->bar);

    pnoutrefresh(tui->pad, tui->pad_top, 0, 1, 0, LINES - 1, COLS - 1);

    doupdate();
}

void tui_create_layout(struct tui *tui) {
    if (tui->bar != NULL) {
        delwin(tui->bar);
        tui->bar = NULL;
    }
    for (int i = 0; i < SUBWIN_COUNT; i++) {
        if (tui->subwins[i] != NULL) {
            delwin(tui->subwins[i]);
            tui->subwins[i] = NULL;
        }
    }
    if (tui->pad != NULL) {
        delwin(tui->pad);
        tui->pad = NULL;
    }

    tui->bar = newwin(1, COLS, 0, 0);

    // Create a new pad
    tui->pad = newpad(PAD_HEIGHT, COLS);
    if (tui->pad == NULL) {
        endwin();
        fprintf(stderr, "Error creating pad.\n");
        exit(EXIT_FAILURE);
    }
    nodelay(tui->pad, TRUE);

    // Populate the pad with subwindows
    for (int i = 0; i < SUBWIN_COUNT; ++i) {
        int y = i * SUBWIN_HEIGHT;

        // Create a subwindow within the pad
        WINDOW *sub = subpad(tui->pad, SUBWIN_HEIGHT, COLS, y, 0);
        if (sub == NULL) {
            endwin();
            fprintf(stderr, "Error creating subwindow %d.\n", i + 1);
            exit(EXIT_FAILURE);
        }
        tui->subwins[i] = sub;
    }
}

int main(void) {
    struct tui tui = {0};

    tui_init(&tui);
    tui_create_layout(&tui);
    tui_repaint_all(&tui);

    // Main loop to handle user input
    int ch;
    while ((ch = wgetch(tui.pad)) != 'q') {
        switch (ch) {
            case 'j':
                if (tui.pad_top + LINES < PAD_HEIGHT) {
                    tui.pad_top += SUBWIN_HEIGHT;
                }
                tui_repaint_all(&tui);
                break;
            case 'k':
                if (tui.pad_top > 0) {
                    tui.pad_top -= SUBWIN_HEIGHT;
                }
                tui_repaint_all(&tui);
                break;
            case KEY_RESIZE:
                tui_create_layout(&tui);
                tui_repaint_all(&tui);
                break;
            default:
                break;
        }
    }

    endwin();

    return 0;
}
