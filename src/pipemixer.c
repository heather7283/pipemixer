#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <ncurses.h>

#include "log.h"
#include "tui.h"
#include "config.h"
#include "utils.h"
#include "pw/common.h"
#include "lib/pollen/pollen.h"

static int sigint_sigterm_handler(struct pollen_callback *callback, int signal, void *data) {
    INFO("caught signal %d, stopping main loop", signal);

    pollen_loop_quit(pollen_callback_get_loop(callback), 0);

    return 0;
}

static int pipewire_handler(struct pollen_callback *callback, int fd, uint32_t events, void *data) {
    int res = pw_loop_iterate(pw.main_loop_loop, 0);
    if (res < 0 && res != -EINTR) {
        return res;
    } else {
        return 0;
    }
}

void print_help_and_exit(FILE *stream, int exit_status) {
    const char *help_string =
        "pipemixer - pipewire volume control\n"
        "\n"
        "usage:\n"
        "    pipemixer [OPTIONS]\n"
        "\n"
        "command line options:\n"
        "    -c, --config     path to configuration file\n"
        "    -l, --loglevel   one of DEBUG, INFO, WARN, ERROR, QUIET\n"
        "    -L, --log-fd     write log to this fd (must be open for writing)\n"
        "    -C, --color      force logging with colors\n"
        "    -h, --help       print this help message and exit\n";

    fputs(help_string, stream);
    exit(exit_status);
}

int main(int argc, char **argv) {
    int retcode = 0;

    const char *config_path = NULL;
    FILE *log_stream = NULL;
    int log_fd = -1;
    enum log_loglevel loglevel = LOG_DEBUG;
    bool log_force_colors = false;

    static const char shortopts[] = "c:L:l:Ch";
    static const struct option longopts[] = {
        { "config",      required_argument, NULL, 'c' },
        { "log-fd",      required_argument, NULL, 'L' },
        { "loglevel",    required_argument, NULL, 'l' },
        { "color",       no_argument,       NULL, 'C' },
        { "help",        no_argument,       NULL, 'h' },
        { 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) > 0) {
        switch (c) {
        case 'c':
            config_path = optarg;
            break;
        case 'L':
            if (!str_to_i32(optarg, &log_fd)) {
                fprintf(stderr, "failed to convert %s to integer\n", optarg);
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

    /* needed for unicode support in ncurses and correct unicode handling in config */
    setlocale(LC_ALL, "");
    load_config(config_path);

    struct pollen_loop *loop = pollen_loop_create();
    if (loop == NULL) {
        fprintf(stderr, "pipemixer: failed to create event loop\n");
        retcode = 1;
        goto cleanup;
    }

    if (pipewire_init() < 0) {
        fprintf(stderr, "pipemixer: failed to connect to pipewire\n");
        retcode = 1;
        goto cleanup;
    }

    tui_init();

    pollen_loop_add_fd(loop, 0 /* stdin */, EPOLLIN, false, tui_handle_keyboard, NULL);
    pollen_loop_add_fd(loop, pw.main_loop_loop_fd, EPOLLIN, false, pipewire_handler, NULL);
    pollen_loop_add_signal(loop, SIGTERM, sigint_sigterm_handler, NULL);
    pollen_loop_add_signal(loop, SIGINT, sigint_sigterm_handler, NULL);
    pollen_loop_add_signal(loop, SIGWINCH, tui_handle_resize, NULL);
    retcode = pollen_loop_run(loop);

cleanup:
    pollen_loop_cleanup(loop);
    pipewire_cleanup();
    tui_cleanup();
    config_cleanup();

    if (log_stream != NULL) {
        fclose(log_stream);
    }

    /* see https://invisible-island.net/ncurses/man/curs_memleaks.3x.html */
    exit_curses(retcode);
}
