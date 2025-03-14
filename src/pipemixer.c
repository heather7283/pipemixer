#include <sys/ioctl.h>
#include <sys/epoll.h>
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
#include "pw.h"
#include "thirdparty/event_loop.h"

static int keyboard_handler(struct event_loop_item *item, uint32_t events) {
    int ch;
    while (errno = 0, (ch = wgetch(tui.pad_win)) != ERR || errno == EINTR) {
        debug("keyboard: char %c", ch);

        switch (ch) {
        case KEY_RESIZE:
            warn("WHY AM I GETTING KEY_RESIZE ???");
            break;
        case 'q':
            event_loop_quit(event_loop_item_get_loop(item), 0);
            break;
        }
    }

    return 0;
}

static int siging_sigterm_handler(struct event_loop_item *item, int signal) {
    info("caught signal %d, stopping main loop", signal);

    event_loop_quit(event_loop_item_get_loop(item), 0);

    return 0;
}

static int sigwinch_handler(struct event_loop_item *item, int signal) {
    debug("caught SIGWINCH, calling resize handler");

    struct winsize winsize;
    if (ioctl(0 /* stdin */, TIOCGWINSZ, &winsize) < 0) {
        err("failed to get new window size: %s", strerror(errno));
        return -1;
    }

    resize_term(winsize.ws_row, winsize.ws_col);
    tui.term_height = getmaxy(stdscr);
    tui.term_width = getmaxx(stdscr);
    debug("new window dimensions %d lines %d columns", tui.term_height, tui.term_width);

    tui_create_layout();
    tui_repaint_all();

    return 0;
}

static int pipewire_handler(struct event_loop_item *item, uint32_t events) {
    pw_loop_iterate(pw.main_loop_loop, 0);

    tui_create_layout();
    tui_repaint_all();

    return 0;
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
        "    -C, --color      force logging with colors\n"
        "    -h, --help       print this help message and exit\n";

    fputs(help_string, stream);
    exit(exit_status);
}

int main(int argc, char** argv) {
    int retcode = 0;

    FILE *log_stream = NULL;
    int log_fd = -1;
    enum log_loglevel loglevel = LOG_DEBUG;
    bool log_force_colors = false;

    static const char shortopts[] = "L:l:Ch";
    static const struct option longopts[] = {
        { "log-fd",      required_argument, NULL, 'L' },
        { "loglevel",    required_argument, NULL, 'l' },
        { "color",       no_argument,       NULL, 'C' },
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
        case 'C':
            log_force_colors = true;
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

        log_init(log_stream, loglevel, log_force_colors);
    }

    struct event_loop *loop = event_loop_create();
    if (loop == NULL) {
        retcode = 1;
        goto cleanup;
    }

    pipewire_init();

    tui_init();
    tui_create_layout();
    tui_repaint_all();

    event_loop_add_pollable(loop, 0 /* stdin */, EPOLLIN, false, keyboard_handler, NULL);
    event_loop_add_pollable(loop, pw.main_loop_loop_fd, EPOLLIN, false, pipewire_handler, NULL);
    event_loop_add_signal(loop, SIGTERM, siging_sigterm_handler, NULL);
    event_loop_add_signal(loop, SIGINT, siging_sigterm_handler, NULL);
    event_loop_add_signal(loop, SIGWINCH, sigwinch_handler, NULL);
    retcode = event_loop_run(loop);

cleanup:
    event_loop_cleanup(loop);

    tui_cleanup();

    pipewire_cleanup();

    if (log_stream != NULL) {
        fclose(log_stream);
    }

    /* see https://invisible-island.net/ncurses/man/curs_memleaks.3x.html */
    exit_curses(retcode);
}
