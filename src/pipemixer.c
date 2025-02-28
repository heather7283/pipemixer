#include <sys/signalfd.h>
#include <signal.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <ncurses.h>

#include "pipemixer.h"
#include "log.h"
#include "tui.h"
#include "macros.h"
#include "event_loop.h"
#include "pw.h"

static void handle_resize(struct tui *tui) {
    // Get new dimensions
    endwin();
    refresh();
    getmaxyx(stdscr, tui->term_height, tui->term_width);

    // Resize top window
    wresize(tui->bar_win, 1, tui->term_width);
    mvwin(tui->bar_win, 0, 0);

    // Clear and redraw
    wclear(tui->bar_win);
    mvwprintw(tui->bar_win, 0, 0, "Top Window - Status Bar (Terminal: %dx%d)",
              tui->term_width, tui->term_height);

    // Make sure pad_pos is within valid range after resize
    if (tui->pad_pos > PAD_SIZE - tui->term_height) {
        tui->pad_pos = PAD_SIZE - tui->term_height;
    }
    if (tui->pad_pos < 0) {
        tui->pad_pos = 0;
    }

    // Refresh everything
    refresh();
    wrefresh(tui->bar_win);
    pnoutrefresh(tui->pad_win, tui->pad_pos, 0, 1, 0, tui->term_height - 1, tui->term_width - 1);
    doupdate();
}

static int keyboard_handler(void *data, struct event_loop_item *item) {
    //struct tui *tui = data;
    struct pipemixer *pipemixer = data;

    int ch;
    while (true) {
        errno = 0;
        if ((ch = getch()) == ERR) {
            if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        }

        getmaxyx(stdscr, pipemixer->tui->term_height, pipemixer->tui->term_width);

        switch (ch) {
            case KEY_RESIZE:
                /* idk why am I getting those even though I installed my own SIGWINCH handler */
                break;
            case 'k':
                if (pipemixer->tui->pad_pos > 0) {
                    pipemixer->tui->pad_pos--;
                    pnoutrefresh(pipemixer->tui->pad_win, pipemixer->tui->pad_pos, 0, 1, 0,
                                 pipemixer->tui->term_height - 1, pipemixer->tui->term_width - 1);
                    doupdate();
                }
                break;
            case 'j':
                if (pipemixer->tui->pad_pos < PAD_SIZE - pipemixer->tui->term_height) {
                    pipemixer->tui->pad_pos++;
                    pnoutrefresh(pipemixer->tui->pad_win, pipemixer->tui->pad_pos, 0, 1, 0,
                                 pipemixer->tui->term_height - 1, pipemixer->tui->term_width - 1);
                    doupdate();
                }
                break;
            case 'r':
                tui_repaint_all(pipemixer);
                break;
            case 'q':
                event_loop_quit(event_loop_item_get_loop(item));
                break;
        }
    }

    return 0;
}

static int signals_handler(void *data, struct event_loop_item *item) {
    struct tui *tui = data; /* I pass tui here so I can handle SIGWINCH (term resize) */

    debug("processing signals");
    struct signalfd_siginfo siginfo;
    if (read(event_loop_item_get_fd(item), &siginfo, sizeof(siginfo)) != sizeof(siginfo)) {
        err("failed to read signalfd_siginfo from signalfd: %s", strerror(errno));
        return -1;
    }

    switch (siginfo.ssi_signo) {
    case SIGINT:
        info("caught SIGINT, stopping main loop");
        event_loop_quit(event_loop_item_get_loop(item));
        break;
    case SIGTERM:
        info("caught SIGTERM, stopping main loop");
        event_loop_quit(event_loop_item_get_loop(item));
        break;
    case SIGWINCH:
        debug("caught SIGWINCH, calling resize handler");
        //ungetch(KEY_RESIZE);
        handle_resize(tui);
        break;
    default:
        err("(BUG) unhandled signal received through signalfd: %d", siginfo.ssi_signo);
        return -1;
    }

    return 0;
}

static int pipewire_handler(void *data, struct event_loop_item *item) {
    struct pipemixer *pipemixer = data;

    pw_loop_iterate(pipemixer->pw->main_loop_loop, 0);

    tui_repaint_all(pipemixer);

    return 0;
}

static int signalfd_init(void) {
    int fd;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGWINCH);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        err("failed to block signals: %s", strerror(errno));
        return -1;
    }

    if ((fd = signalfd(-1, &mask, SFD_CLOEXEC)) < 0) {
        err("failed to set up signalfd: %s", strerror(errno));
        return -1;
    }

    return fd;
}

void print_help_and_exit(FILE *stream, int exit_status) {
    const char* help_string =
        "pipemixer - pipewire volume control\n"
        "\n"
        "usage:\n"
        "    pipemixer [OPTIONS]\n"
        "\n"
        "command line options:\n"
        "    -l, --loglevel   one of DEBUG, INFO, WARN, ERROR, QUIET\n"
        "    -L, --log-fd     write log to this fd (must be open for writing)\n"
        "    -h, --help       print this help message and exit\n";

    fputs(help_string, stream);
    exit(exit_status);
}

int main(int argc, char** argv) {
    int retcode = 0;
    int signal_fd = -1;

    FILE *log_stream = NULL;
    int log_fd = -1;
    enum log_loglevel loglevel = LOG_DEBUG;

    static const char shortopts[] = "L:l:h";
    static const struct option longopts[] = {
        { "log-fd",      required_argument, NULL, 'L' },
        { "loglevel",    required_argument, NULL, 'l' },
        { "help",        no_argument,       NULL, 'h' },
        { 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) > 0) {
        switch (c) {
        case 'L':
            errno = 0;
            char *endptr;
            log_fd = strtol(optarg, &endptr, 10); /* casting long to int, don't care */
            if (errno != 0 || *endptr != '\0') {
                fprintf(stderr, "failed to convert %s to integer: %s\n",
                        optarg, strerror(errno != 0 ? errno : EINVAL));
                exit(1);
            }
            break;
        case 'l':
            loglevel = log_str_to_loglevel(optarg);
            if (loglevel == LOG_INVALID) {
                fprintf(stderr, "%s is not a valid loglevel\n", optarg);
                exit(1);
            }
            break;
        case 'h':
            print_help_and_exit(stdout, 0);
            break;
        default:
            print_help_and_exit(stderr, 1);
            break;
        }
    }

    if (log_fd != -1) {
        log_stream = fdopen(log_fd, "w");
        if (log_stream == NULL) {
            fprintf(stderr, "failed to fdopen() fd %d: %s\n", log_fd, strerror(errno));
            exit(1);
        }

        log_init(log_stream, loglevel);
    }

    struct tui tui = {0};
    struct pw pw = {0};
    struct pipemixer pipemixer = {
        .pw = &pw,
        .tui = &tui,
    };

    struct event_loop *loop = event_loop_create();
    if (loop == NULL) {
        retcode = 1;
        goto cleanup;
    }

    if ((signal_fd = signalfd_init()) < 0) {
        retcode = 1;
        goto cleanup;
    }

    pipewire_init(&pw);

    setlocale(LC_ALL, ""); /* needed for unicode support in ncurses */

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE); /* getch() will fail instead of blocking waiting for input */
    keypad(stdscr, TRUE);

    getmaxyx(stdscr, tui.term_height, tui.term_width);

    tui.bar_win = newwin(1, tui.term_width, 0, 0);
    wrefresh(tui.bar_win);

    tui.pad_win = newpad(PAD_SIZE, MAX_SCREEN_WIDTH);

    /* initial draw */
    handle_resize(&tui);

    event_loop_add_item(loop, 0, keyboard_handler, &pipemixer); /* stdin */
    event_loop_add_item(loop, signal_fd, signals_handler, &tui); /* passing tui for SIGWINCH */
    event_loop_add_item(loop, pw.main_loop_loop_fd, pipewire_handler, &pipemixer);
    event_loop_run(loop);

cleanup:
    if (loop != NULL) {
        event_loop_cleanup(loop);
    }

    if (tui.bar_win != NULL) {
        delwin(tui.bar_win);
    }
    if (tui.pad_win != NULL) {
        delwin(tui.pad_win);
    }
    endwin();

    pipewire_cleanup(&pw);

    if (log_stream != NULL) {
        fclose(log_stream);
    }

    /* see https://invisible-island.net/ncurses/man/curs_memleaks.3x.html */
    exit_curses(retcode);
}
