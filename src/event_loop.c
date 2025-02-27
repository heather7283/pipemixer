#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <spa/utils/defs.h>
#include <spa/utils/list.h>

#include "event_loop.h"
#include "log.h"
#include "xmalloc.h"

#define EPOLL_MAX_EVENTS 16

struct event_loop_item {
    struct event_loop *loop;

    int fd;
    void *data;
    event_loop_callback_t callback;

    struct spa_list link;
};

struct event_loop_uncond {
    void *data;
    event_loop_uncond_callback_t callback;

    struct spa_list link;
};

struct event_loop {
    bool running;
    int epoll_fd;

    struct spa_list items;
    struct spa_list uncond_callbacks;
};

static bool fd_is_valid(int fd) {
    if ((fcntl(fd, F_GETFD) < 0) && (errno == EBADF)) {
        return false;
    }
    return true;
}

struct event_loop *event_loop_create(void) {
    debug("event loop: create");

    struct event_loop *loop = xmalloc(sizeof(*loop));

    spa_list_init(&loop->items);
    spa_list_init(&loop->uncond_callbacks);

    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) {
        err("event loop: failed to create epoll: %s", strerror(errno));
        goto err;
    }

    return loop;

err:
    err("failed to create event loop");
    free(loop);
    return NULL;
}

void event_loop_cleanup(struct event_loop *loop) {
    debug("event loop: cleanup");

    struct event_loop_item *item, *item_tmp;
    spa_list_for_each_safe(item, item_tmp, &loop->items, link) {
        event_loop_remove_item(item);
    }

    struct event_loop_uncond *callback, *callback_tmp;
    spa_list_for_each_safe(callback, callback_tmp, &loop->uncond_callbacks, link) {
        event_loop_remove_item(item);
    }

    close(loop->epoll_fd);

    free(loop);
}

struct event_loop_item *event_loop_add_item(struct event_loop *loop, int fd,
                                            event_loop_callback_t callback, void *data) {
    debug("event loop: adding fd %d to event loop", fd);

    struct event_loop_item *new_item = xmalloc(sizeof(*new_item));
    new_item->loop = loop;
    new_item->fd = fd;
    new_item->callback = callback;
    new_item->data = data;

    struct epoll_event epoll_event;
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = fd;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event) < 0) {
        err("event loop: failed to add fd %d to epoll (%s)", fd, strerror(errno));
        event_loop_quit(loop);
    }

    spa_list_insert(&loop->items, &new_item->link);

    return new_item;
}

void event_loop_remove_item(struct event_loop_item *item) {
    debug("event loop: removing fd %d from event loop", item->fd);

    spa_list_remove(&item->link);

    if (fd_is_valid(item->fd)) {
        if (epoll_ctl(item->loop->epoll_fd, EPOLL_CTL_DEL, item->fd, NULL) < 0) {
            err("event loop: failed to remove fd %d from epoll (%s)", item->fd, strerror(errno));
            event_loop_quit(item->loop);
        }
        close(item->fd);
    } else {
        warn("event loop: fd %d is not valid, was it closed somewhere else?", item->fd);
    }

    free(item);
}

struct event_loop *event_loop_item_get_loop(struct event_loop_item *item) {
    return item->loop;
}

int event_loop_item_get_fd(struct event_loop_item *item) {
    return item->fd;
}

struct event_loop_uncond *event_loop_add_uncond(struct event_loop *loop,
                                                event_loop_uncond_callback_t callback, void *data) {
    debug("event loop: adding unconditional callback");

    struct event_loop_uncond *new_item = xmalloc(sizeof(*new_item));
    new_item->callback = callback;
    new_item->data = data;

    spa_list_insert(&loop->uncond_callbacks, &new_item->link);

    return new_item;
}

void event_loop_remove_uncond(struct event_loop_uncond *uncond) {
    debug("event loop: removing unconditional callback");

    spa_list_remove(&uncond->link);

    free(uncond);
}

void event_loop_run(struct event_loop *loop) {
    debug("event loop: run");

    int number_fds = -1;
    struct epoll_event events[EPOLL_MAX_EVENTS];

    loop->running = true;
    while (loop->running) {
        do {
            number_fds = epoll_wait(loop->epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR); /* epoll_wait failing with EINTR is normal */

        if (number_fds == -1) {
            err("event loop: epoll_wait error (%s)", strerror(errno));
            loop->running = false;
            goto out;
        }

        debug("event loop: received events on %d fds", number_fds);

        for (int n = 0; n < number_fds; n++) {
            bool match_found = false;
            struct event_loop_item *item, *item_tmp;

            debug("event loop: processing fd %d", events[n].data.fd);

            spa_list_for_each_safe(item, item_tmp, &loop->items, link) {
                if (item->fd == events[n].data.fd) {
                    match_found = true;
                    int ret = item->callback(item->data, item);
                    if (ret < 0) {
                        err("event loop: callback returned non-zero, quitting");
                        loop->running = false;
                        goto out;
                    }
                };
            }

            if (!match_found) {
                warn("event loop: no handlers were found for fd %d (BUG)", events[n].data.fd);
            }
        }

        struct event_loop_uncond *uncond, *uncond_tmp;
        spa_list_for_each_safe(uncond, uncond_tmp, &loop->uncond_callbacks, link) {
            int ret = uncond->callback(uncond->data, uncond);
            if (ret < 0) {
                err("event loop: unconditional callback returned non-zero, quitting");
                loop->running = false;
                goto out;
            }
        }
    }

out:
    return;
}

void event_loop_quit(struct event_loop *loop) {
    debug("event loop: quit");

    loop->running = false;
}

