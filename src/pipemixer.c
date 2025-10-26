#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <ncurses.h>

#include "log.h"
#include "tui.h"
#include "config.h"
#include "utils.h"
#include "eventloop.h"
#include "pw/common.h"

static void crash_handler(int sig) {
    /* restore terminal state before crashing */
    endwin();
    raise(sig);
}

static int sigint_sigterm_handler(struct pollen_event_source *_, int signal, void *_) {
    INFO("caught signal %d, stopping main loop", signal);

    pollen_loop_quit(event_loop, 0);

    return 0;
}

void print_help_and_exit(FILE *stream, int exit_status) {
    const char help_string[] =
        "pipemixer - pipewire volume control\n"
        "\n"
        "usage:\n"
        "    pipemixer [OPTIONS]\n"
        "\n"
        "command line options:\n"
        "    -c, --config     path to configuration file\n"
        "    -l, --loglevel   one of TRACE, DEBUG, INFO, WARN, ERROR, QUIET\n"
        "    -L, --log-fd     write log to this fd (must be open for writing)\n"
        "    -C, --color      force logging with colors\n"
        "    -V, --version    print version information\n"
        "    -h, --help       print this help message and exit\n";

    fputs(help_string, stream);
    exit(exit_status);
}

void print_version_and_exit(FILE *stream, int exit_status) {
    const char version_string[] =
        "pipemixer version "PIPEMIXER_VERSION" "
        "(git tag "PIPEMIXER_GIT_TAG", branch "PIPEMIXER_GIT_BRANCH")\n"
    ;

    fputs(version_string, stream);
    exit(exit_status);
}

int main(int argc, char **argv) {
    int retcode = 0;

    const char *config_path = NULL;
    FILE *log_stream = NULL;
    int log_fd = -1;
    enum log_loglevel loglevel = LOG_DEBUG;
    bool log_force_colors = false;

    static const char shortopts[] = "c:L:l:CVh";
    static const struct option longopts[] = {
        { "config",      required_argument, NULL, 'c' },
        { "log-fd",      required_argument, NULL, 'L' },
        { "loglevel",    required_argument, NULL, 'l' },
        { "color",       no_argument,       NULL, 'C' },
        { "version",     no_argument,       NULL, 'V' },
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
        case 'V':
            print_version_and_exit(stdout, 0);
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

    event_loop = pollen_loop_create();
    if (event_loop == NULL) {
        fprintf(stderr, "pipemixer: failed to create event loop\n");
        retcode = 1;
        goto cleanup;
    }

    signals_global_init();

    if (pipewire_init() < 0) {
        fprintf(stderr, "pipemixer: failed to connect to pipewire\n");
        retcode = 1;
        goto cleanup;
    }

    /* setup crash handler before initialising ncurses */
    struct sigaction sa = {
        .sa_handler = crash_handler,
        .sa_flags = SA_NODEFER | SA_RESETHAND,
    };
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);

    tui_init();

    pollen_loop_add_signal(event_loop, SIGTERM, sigint_sigterm_handler, NULL);
    pollen_loop_add_signal(event_loop, SIGINT, sigint_sigterm_handler, NULL);
    retcode = pollen_loop_run(event_loop);

cleanup:
    pollen_loop_cleanup(event_loop);
    pipewire_cleanup();
    tui_cleanup();

    if (log_stream != NULL) {
        fclose(log_stream);
    }

    /* see https://invisible-island.net/ncurses/man/curs_memleaks.3x.html */
    exit_curses(retcode);
}

