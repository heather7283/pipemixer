#include <unistd.h>
#include <getopt.h>

#include <ncurses.h>
#include <spa/utils/string.h>

#include "log.h"
#include "config.h"
#include "macros.h"
#include "eventloop.h"
#include "tui/tui.h"
#include "pw/common.h"

struct pw_main_loop *main_loop = NULL;
struct pw_loop *event_loop = NULL;

static void events_fd_handler(void *_, int _, uint32_t _) {
    events_dispatch();
}

static void bad_signal_handler(int sig) {
    /* restore terminal state before crashing */
    endwin();
    raise(sig);
}

static void exit_signal_handler(int sig) {
    INFO("caught signal %d, stopping main loop", sig);
    pw_main_loop_quit(main_loop);
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
        "    -v, --validate   validate config file and exit 1 on errors\n"
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
        "pipemixer version " PIPEMIXER_VERSION
    #ifdef PIPEMIXER_GIT_TAG
        ", git tag " PIPEMIXER_GIT_TAG
    #endif
    #ifdef PIPEMIXER_GIT_BRANCH
        ", git branch " PIPEMIXER_GIT_BRANCH
    #endif
        "\n"
    ;

    fputs(version_string, stream);
    exit(exit_status);
}

int main(int argc, char **argv) {
    int retcode = 0;

    const char *config_path = NULL;
    bool validate_config = false;
    FILE *log_stream = NULL;
    int log_fd = -1;
    enum log_loglevel loglevel = LOG_DEBUG;
    bool log_force_colors = false;

    static const char shortopts[] = "c:L:l:vCVh";
    static const struct option longopts[] = {
        { "config",      required_argument, NULL, 'c' },
        { "log-fd",      required_argument, NULL, 'L' },
        { "loglevel",    required_argument, NULL, 'l' },
        { "validate",    no_argument,       NULL, 'v' },
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
        case 'v':
            validate_config = true;
            break;
        case 'L':
            if (!spa_atoi32(optarg, &log_fd, 10)) {
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

    if (log_fd > 0) {
        log_stream = fdopen(log_fd, "w");
        if (log_stream == NULL) {
            fprintf(stderr, "failed to fdopen() fd %d: %s\n", log_fd, strerror(errno));
            exit(1);
        }

        log_init(log_stream, loglevel, log_force_colors);
    }

    /* needed for unicode support in ncurses and correct unicode handling in config */
    setlocale(LC_ALL, "");

    bool config_valid = load_config(config_path);
    if (validate_config) {
        return !config_valid;
    }

    pw_init(NULL, NULL);

    main_loop = pw_main_loop_new(NULL);
    if (!main_loop) {
        fprintf(stderr, "failed to create event loop\n");
        retcode = 1;
        goto cleanup;
    }
    event_loop = pw_main_loop_get_loop(main_loop);

    int events_fd = events_global_init();
    pw_loop_add_io(event_loop, events_fd, POLL_IN, false, events_fd_handler, NULL);

    /* naming is unfortunate */
    if (!pipewire_init()) {
        fprintf(stderr, "pipemixer: failed to connect to pipewire\n");
        retcode = 1;
        goto cleanup;
    }

    /* setup crash handler before initialising ncurses */
    static const int bad_signals[] = {
        SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS,
    };
    for (unsigned i = 0; i < SIZEOF_ARRAY(bad_signals); i++) {
        sigaction(bad_signals[i], &(struct sigaction){
            .sa_handler = bad_signal_handler,
            .sa_flags = SA_NODEFER | SA_RESETHAND | SA_RESTART,
        }, NULL);
    }

    static const int exit_signals[] = {
        SIGTERM, SIGINT,
    };
    for (unsigned i = 0; i < SIZEOF_ARRAY(exit_signals); i++) {
        sigaction(exit_signals[i], &(struct sigaction){
            .sa_handler = exit_signal_handler,
            .sa_flags = SA_RESTART,
        }, NULL);
    }

    tui_init();

    TRACE("entering main loop");
    pw_main_loop_run(main_loop);
    TRACE("leaving main loop");

cleanup:
    pipewire_cleanup();
    tui_cleanup();

    /* see https://invisible-island.net/ncurses/man/curs_memleaks.3x.html */
    exit_curses(retcode);
}

