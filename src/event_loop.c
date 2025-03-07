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
    int priority;
    void *data;
    event_loop_callback_t callback;

    struct spa_list link;
};

struct event_loop {
    bool should_quit;
    int retcode;
    int epoll_fd;

    struct spa_list items;
    struct spa_list unconditional_items;
};

static bool fd_is_valid(int fd) {
    if ((fcntl(fd, F_GETFD) < 0) && (errno == EBADF)) {
        return false;
    }
    return true;
}

struct event_loop *event_loop_create(void) {
    debug("event loop: create");

    struct event_loop *loop = xcalloc(1, sizeof(*loop));

    spa_list_init(&loop->items);
    spa_list_init(&loop->unconditional_items);

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
    spa_list_for_each_safe(item, item_tmp, &loop->unconditional_items, link) {
        event_loop_remove_item(item);
    }

    close(loop->epoll_fd);

    free(loop);
}

struct event_loop_item *event_loop_add_item(struct event_loop *loop, int fd,
                                            event_loop_callback_t callback, void *data) {
    struct event_loop_item *new_item = NULL;

    if (fd >= 0) {
        debug("event loop: adding fd %d to event loop", fd);

        struct epoll_event epoll_event;
        epoll_event.events = EPOLLIN;
        epoll_event.data.fd = fd;
        if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event) < 0) {
            int ret = -errno;
            err("event loop: failed to add fd %d to epoll (%s)", fd, strerror(errno));
            event_loop_quit(loop, ret);
            goto out;
        }

        new_item = xcalloc(1, sizeof(*new_item));
        new_item->loop = loop;
        new_item->fd = fd;
        new_item->priority = -1;
        new_item->callback = callback;
        new_item->data = data;

        spa_list_insert(&loop->items, &new_item->link);
    } else {
        int priority = -fd;
        debug("event loop: adding unconditional callback with prio %d to event loop", priority);

        new_item = xcalloc(1, sizeof(*new_item));
        new_item->loop = loop;
        new_item->fd = -1;
        new_item->priority = priority;
        new_item->callback = callback;
        new_item->data = data;

        if (spa_list_is_empty(&loop->unconditional_items)) {
            spa_list_insert(&loop->unconditional_items, &new_item->link);
        } else {
            struct event_loop_item *elem;
            spa_list_for_each_reverse(elem, &loop->unconditional_items, link) {
                /*         |6|
                 * |9|  |8|\/|4|  |2|
                 * <-----------------
                 * iterate from the end and find the first item with higher prio
                 */
                bool found = false;
                if (elem->priority > priority) {
                    found = true;
                    spa_list_insert(&elem->link, &new_item->link);
                }
                if (!found) {
                    spa_list_insert(&loop->unconditional_items, &new_item->link);
                }
            }
        }
    }

out:
    return new_item;
}

void event_loop_remove_item(struct event_loop_item *item) {
    if (item->fd >= 0) {
        debug("event loop: removing fd %d from event loop", item->fd);

        if (fd_is_valid(item->fd)) {
            if (epoll_ctl(item->loop->epoll_fd, EPOLL_CTL_DEL, item->fd, NULL) < 0) {
                int ret = -errno;
                err("event loop: failed to remove fd %d from epoll (%s)",
                    item->fd, strerror(errno));
                event_loop_quit(item->loop, ret);
            }
            close(item->fd);
        } else {
            warn("event loop: fd %d is not valid, was it closed somewhere else?", item->fd);
        }
    } else {
        debug("event loop: removing unconditional callback with prio %d from event loop",
              item->priority);
    }

    spa_list_remove(&item->link);

    free(item);
}

struct event_loop *event_loop_item_get_loop(struct event_loop_item *item) {
    return item->loop;
}

int event_loop_item_get_fd(struct event_loop_item *item) {
    return item->fd;
}

int event_loop_run(struct event_loop *loop) {
    debug("event loop: run");

    int ret = 0;
    int number_fds = -1;
    struct epoll_event events[EPOLL_MAX_EVENTS];

    loop->should_quit = false;
    while (!loop->should_quit) {
        do {
            number_fds = epoll_wait(loop->epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR); /* epoll_wait failing with EINTR is normal */

        if (number_fds == -1) {
            ret = errno;
            err("event loop: epoll_wait error (%s)", strerror(errno));
            loop->retcode = ret;
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
                    ret = item->callback(item->data, item);
                    if (ret < 0) {
                        err("event loop: callback returned negative, quitting");
                        loop->retcode = ret;
                        goto out;
                    }
                };
            }

            if (!match_found) {
                warn("event loop: no handlers were found for fd %d (BUG)", events[n].data.fd);
            }
        }
    }

out:
    return loop->retcode;
}

void event_loop_quit(struct event_loop *loop, int retcode) {
    debug("event loop: quit");

    loop->should_quit = true;
    loop->retcode = retcode;
}

