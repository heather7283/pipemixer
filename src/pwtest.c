#include <sys/epoll.h>
#include <sys/signalfd.h>

#include "pw.h"
#include "macros.h"

#define EPOLL_MAX_EVENTS 16

int signalfd_init(void) {
    int signal_fd;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        die("failed to block signals: %s", strerror(errno));
    }

    if ((signal_fd = signalfd(-1, &mask, 0)) == -1) {
        die("failed to set up signalfd: %s", strerror(errno));
    }

    return signal_fd;
}

int epoll_init(void) {
    int epoll_fd;

    if ((epoll_fd = epoll_create1(0)) < 0) {
        die("failed to set up epoll: %s", strerror(errno));
    }

    return epoll_fd;
}

void epoll_add_fd(int fd, int epoll_fd) {
    struct epoll_event epoll_event;

    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event) < 0) {
        die("failed to add fd %d to epoll list: %s", fd, strerror(errno));
    }
}

int main(int argc, char **argv) {
    int exit_status = 0;

    setlocale(LC_ALL, "");

    struct pw pw = {0};
    pipewire_init(&pw);

    int signal_fd = signalfd_init();

    int epoll_fd = epoll_init();
    epoll_add_fd(signal_fd, epoll_fd);
    epoll_add_fd(pw.main_loop_loop_fd, epoll_fd);

    int number_fds = -1;
    struct epoll_event events[EPOLL_MAX_EVENTS];
    while (true) {
        /* main event loop */
        do {
            number_fds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR); /* epoll_wait failing with EINTR is normal */

        if (number_fds == -1) {
            die("epoll_wait error: %s", strerror(errno));
        }

        /* handle events */
        for (int n = 0; n < number_fds; n++) {
            if (events[n].data.fd == pw.main_loop_loop_fd) {
                /* pipewire events */
                pw_loop_iterate(pw.main_loop_loop, 0);
            } else if (events[n].data.fd == signal_fd) {
                /* signals */
                struct signalfd_siginfo siginfo;
                ssize_t bytes_read = read(signal_fd, &siginfo, sizeof(siginfo));
                if (bytes_read != sizeof(siginfo)) {
                    die("failed to read signalfd_siginfo from signal_fd");
                }

                uint32_t signo = siginfo.ssi_signo;
                switch (signo) {
                case SIGINT:
                    info("received SIGINT, exiting");
                    goto cleanup;
                case SIGTERM:
                    info("received SIGTERM, exiting");
                    goto cleanup;
                case SIGUSR1:
                    debug("received SIGUSR1");

                    struct node *node;
                    spa_list_for_each(node, &pw.all_nodes, all_nodes_link) {
                        info("node %d ", node->id);
                        info("  mute: %s", node->props.mute ? "yes" : "no");
                        for (uint32_t i = 0; i < node->props.channel_count; i++) {
                            info("  channel %d (%s): %f",
                                 i, node->props.channel_map[i], node->props.channel_volumes[i]);
                        }
                        info("  application.name: %ls", node->application_name);
                        info("  media.name: %ls", node->media_name);
                    }
                    break;
                default:
                    die("caught signal %d but there's no handler for it (BUG)", signo);
                }
            }
        }
    }

cleanup:
    pipewire_cleanup(&pw);

    return exit_status;
}
