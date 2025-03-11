/*
 * MIT License
 *
 * Copyright (c) 2025 heather7283
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * event_loop.h version 0.5.1
 * last version is available at: https://github.com/heather7283/event_loop.h
 *
 * This is a single-header library that provides simple event loop abstraction built on epoll.
 * To use this library, do this in one C file:
 *   #define EVENT_LOOP_IMPLEMENTATION
 *   #include "event_loop.h"
 *
 * COMPILE-TIME TUNABLES:
 *   EVENT_LOOP_CALLOC(n, size) - calloc()-like function that will be used to allocate memory.
 *     Default: #define EVENT_LOOP_CALLOC(n, size) calloc(n, size)
 *   EVENT_LOOP_FREE(ptr) - free()-like function that will be used to free memory.
 *     Default: #define EVENT_LOOP_FREE(ptr) free(ptr)
 *
 *   Following macros will, if defined, be used for logging.
 *   They must expand to printf()-like function, for example:
 *   #define EVENT_LOOP_LOG_DEBUG(fmt, ...) fprintf(stderr, "event loop: " fmt "\n", ##__VA_ARGS__)
 *     EVENT_LOOP_LOG_DEBUG(fmt, ...)
 *     EVENT_LOOP_LOG_WARN(fmt, ...)
 *     EVENT_LOOP_LOG_ERR(fmt, ...)
 */

#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#if !defined(EVENT_LOOP_CALLOC) || !defined(EVENT_LOOP_FREE)
    #include <stdlib.h>
#endif
#if !defined(EVENT_LOOP_CALLOC)
    #define EVENT_LOOP_CALLOC(n, size) calloc(n, size)
#endif
#if !defined(EVENT_LOOP_FREE)
    #define EVENT_LOOP_FREE(ptr) free(ptr)
#endif

/*
 * doing this to avoid compiler warning:
 * passing no argument for the '...' parameter of a variadic macro is a C23 extension
 */
#if !defined(EVENT_LOOP_DEBUG) || !defined(EVENT_LOOP_WARN) || !defined(EVENT_LOOP_ERR)
    #include <stdio.h> /* printf() */
#endif
#if !defined(EVENT_LOOP_LOG_DEBUG)
    #define EVENT_LOOP_LOG_DEBUG(fmt, ...) if (0) printf(fmt, ##__VA_ARGS__)
#endif
#if !defined(EVENT_LOOP_LOG_WARN)
    #define EVENT_LOOP_LOG_WARN(fmt, ...) if (0) printf(fmt, ##__VA_ARGS__)
#endif
#if !defined(EVENT_LOOP_LOG_ERR)
    #define EVENT_LOOP_LOG_ERR(fmt, ...) if (0) printf(fmt, ##__VA_ARGS__)
#endif

#include <sys/epoll.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>

struct event_loop_item;
typedef int (*event_loop_callback_pollable)(struct event_loop_item *loop_item, uint32_t events);
typedef int (*event_loop_callback_unconditional)(struct event_loop_item *loop_item);
typedef int (*event_loop_callback_signal)(struct event_loop_item *loop_item, int signal);

/* Creates a new event_loop instance. Returns NULL and sets errno on failure. */
struct event_loop *event_loop_create(void);
/* Frees all resources associated with the loop. Passing NULL is a harmless no-op. */
void event_loop_cleanup(struct event_loop *loop);

/*
 * Adds fd to epoll interest list.
 * Argument events directly corresponts to epoll_event.events field (see man epoll_event).
 * If autoclose is true, the fd will be closed when event_loop_remove_callback is called.
 *
 * Returns NULL and sets errno on failure.
 */
struct event_loop_item *event_loop_add_pollable(struct event_loop *loop,
                                                int fd, uint32_t events, bool autoclose,
                                                event_loop_callback_pollable callback,
                                                void *data);

/*
 * Adds a callback that will run unconditionally on every event loop iteration,
 * after all other callback types were processed.
 * Callbacks with higher priority will run before callbacks with lower priority.
 * If two callbacks have equal priority, the order is undefined.
 *
 * Return NULL and sets errno on failure.
 */
struct event_loop_item *event_loop_add_unconditional(struct event_loop *loop, int priority,
                                                     event_loop_callback_unconditional callback,
                                                     void *data);

/*
 * Adds a callback that will run when signal is caught. Uses signalfd under the hood.
 *
 * Return NULL and sets errno on failure.
 */
struct event_loop_item *event_loop_add_signal(struct event_loop *loop, int signal,
                                              event_loop_callback_signal callback,
                                              void *data);

/*
 * Remove a callback from event loop.
 * If a callback has fd associated with it, this function will attempt to close it.
 * If fd was already closed, a warning will be printed.
 */
void event_loop_remove_callback(struct event_loop_item *item);

/* Get event_loop instance associated with this event_loop_item. */
struct event_loop *event_loop_item_get_loop(struct event_loop_item *item);
/* Get pointer to user data saved in this event_loop_item. */
void *event_loop_item_get_data(struct event_loop_item *item);
/*
 * Get file descriptor associated with this event_loop_item.
 * If item is not a pollable callback, -1 is returned.
 */
int event_loop_item_get_fd(struct event_loop_item *item);

/*
 * Run the event loop. This function blocks until event loop exits.
 * This function returns 0 if no errors occured.
 * If any of the callbacks return negative value, the loop with be stopped and this value returned.
 */
int event_loop_run(struct event_loop *loop);
/*
 * Quit the event loop.
 * Argument retcode specifies the value that will be returned by event_loop_run.
 */
void event_loop_quit(struct event_loop *loop, int retcode);

#endif /* #ifndef EVENT_LOOP_H */

/*
 * ============================================================================
 *                              IMPLEMENTATION
 * ============================================================================
 */
#ifdef EVENT_LOOP_IMPLEMENTATION

#include <sys/signalfd.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#define EVENT_LOOP_EPOLL_MAX_EVENTS 16

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #define EVENT_LOOP_TYPEOF(expr) typeof(expr)
#else
    #define EVENT_LOOP_TYPEOF(expr) __typeof__(expr)
#endif

#define EVENT_LOOP_CONTAINER_OF(ptr, sample, member) \
    (EVENT_LOOP_TYPEOF(sample))((char *)(ptr) - offsetof(EVENT_LOOP_TYPEOF(*sample), member))

/*
 * Linked list.
 * In the head, next points to the first list elem, prev points to the last.
 * In the list element, next points to the next elem, prev points to the previous elem.
 * In the last element, next points to the head. In the first element, prev points to the head.
 * If the list is empty, next and prev point to the head itself.
 */
struct event_loop_ll {
    struct event_loop_ll *next;
    struct event_loop_ll *prev;
};

static inline void event_loop_ll_init(struct event_loop_ll *head) {
    head->next = head;
    head->prev = head;
}

static inline bool event_loop_ll_is_empty(struct event_loop_ll *head) {
    return head->next == head && head->prev == head;
}

/* Inserts new after elem. */
static inline void event_loop_ll_insert(struct event_loop_ll *elem, struct event_loop_ll *new) {
    elem->next->prev = new;
    new->next = elem->next;

    elem->next = new;
    new->prev = elem;
}

static inline void event_loop_ll_remove(struct event_loop_ll *elem) {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
}

#define EVENT_LOOP_LL_FOR_EACH(var, head, member) \
    for (var = EVENT_LOOP_CONTAINER_OF((head)->next, var, member); \
         &var->member != (head); \
         var = EVENT_LOOP_CONTAINER_OF(var->member.next, var, member))

#define EVENT_LOOP_LL_FOR_EACH_REVERSE(var, head, member) \
    for (var = EVENT_LOOP_CONTAINER_OF((head)->prev, var, member); \
         &var->member != (head); \
         var = EVENT_LOOP_CONTAINER_OF(var->member.prev, var, member))

#define EVENT_LOOP_LL_FOR_EACH_SAFE(var, tmp, head, member) \
    for (var = EVENT_LOOP_CONTAINER_OF((head)->next, var, member), \
         tmp = EVENT_LOOP_CONTAINER_OF((var)->member.next, tmp, member); \
         &var->member != (head); \
         var = tmp, \
         tmp = EVENT_LOOP_CONTAINER_OF(var->member.next, tmp, member))

enum event_loop_item_type {
    POLLABLE,
    UNCONDITIONAL,
    SIGNAL,
};

struct event_loop_item {
    struct event_loop *loop;

    enum event_loop_item_type type;
    union {
        struct {
            int fd;
            event_loop_callback_pollable callback;
            bool autoclose;
        } pollable;
        struct {
            int priority;
            event_loop_callback_unconditional callback;
        } unconditional;
        struct {
            int sig;
            event_loop_callback_signal callback;
        } signal;
    } as;

    void *data;

    struct event_loop_ll link;
};

struct event_loop {
    bool should_quit;
    int retcode;
    int epoll_fd;

    /* signal(7) says there are 38 standard signals on linux */
    struct event_loop_item *signal_callbacks[38];
    int n_signal_callbacks;
    int signal_fd;
    sigset_t sigset;

    struct event_loop_ll pollable_items;
    struct event_loop_ll unconditional_items;
    struct event_loop_ll signal_items;
};

/* this function itself is a callback that will get called on signalfd events */
int event_loop_signal_handler(struct event_loop_item *item, uint32_t events) {
    struct event_loop *loop = item->loop;

    /* TODO: figure out why does this always only read only one siginfo */
    int ret;
    struct signalfd_siginfo siginfo;
    while ((ret = read(loop->signal_fd, &siginfo, sizeof(siginfo))) == sizeof(siginfo)) {
        int signal = siginfo.ssi_signo;
        EVENT_LOOP_LOG_DEBUG("received signal %d via signalfd", signal);

        struct event_loop_item *signal_callback = loop->signal_callbacks[signal];
        if (signal_callback != NULL) {
            return signal_callback->as.signal.callback(signal_callback, signal);
        } else {
            EVENT_LOOP_LOG_ERR("received signal %d via signalfd has no callbacks installed",
                               signal);
            return -1;
        }
    }

    if (ret >= 0) {
        EVENT_LOOP_LOG_ERR("read incorrect amount of bytes from signalfd");
        return -1;
    } else /* ret < 0 */ {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* no more signalds to handle. exit. */
            EVENT_LOOP_LOG_DEBUG("no more signalds to handle");
            return 0;
        } else {
            EVENT_LOOP_LOG_ERR("failed to read siginfo from signalfd: %s", strerror(errno));
            return -1;
        }
    }

}

struct event_loop *event_loop_create(void) {
    EVENT_LOOP_LOG_DEBUG("create");
    int save_errno = 0;

    struct event_loop *loop = EVENT_LOOP_CALLOC(1, sizeof(*loop));
    if (loop == NULL) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to allocate memory for event loop: %s", strerror(errno));
        goto err;
    }

    event_loop_ll_init(&loop->pollable_items);
    event_loop_ll_init(&loop->unconditional_items);
    event_loop_ll_init(&loop->signal_items);

    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd < 0) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to create epoll: %s", strerror(errno));
        goto err;
    }

    /* TODO: don't create signalfd until the first signal callback is added? */
    sigemptyset(&loop->sigset);
    loop->signal_fd = signalfd(-1, &loop->sigset, SFD_NONBLOCK);
    if (loop->signal_fd < 0) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to create signalfd: %s", strerror(errno));
        goto err;
    }
    if (event_loop_add_pollable(loop, loop->signal_fd, EPOLLIN, false,
                                event_loop_signal_handler, NULL) == NULL) {
        /* bruh moment */
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to add signal handler callback: %s", strerror(errno));
        goto err;
    }

    return loop;

err:
    EVENT_LOOP_FREE(loop);
    errno = save_errno;
    return NULL;
}

void event_loop_cleanup(struct event_loop *loop) {
    if (loop == NULL) {
        return;
    }

    EVENT_LOOP_LOG_DEBUG("cleanup");

    struct event_loop_item *item, *item_tmp;
    EVENT_LOOP_LL_FOR_EACH_SAFE(item, item_tmp, &loop->unconditional_items, link) {
        event_loop_remove_callback(item);
    }
    /* make sure signal are deleted before pollable bc signal handler is itself pollable */
    EVENT_LOOP_LL_FOR_EACH_SAFE(item, item_tmp, &loop->signal_items, link) {
        event_loop_remove_callback(item);
    }
    EVENT_LOOP_LL_FOR_EACH_SAFE(item, item_tmp, &loop->pollable_items, link) {
        event_loop_remove_callback(item);
    }

    close(loop->signal_fd);
    close(loop->epoll_fd);

    EVENT_LOOP_FREE(loop);
}

struct event_loop_item *event_loop_add_pollable(struct event_loop *loop,
                                                int fd, uint32_t events, bool autoclose,
                                                event_loop_callback_pollable callback,
                                                void *data) {
    struct event_loop_item *new_item = NULL;
    int save_errno = 0;

    EVENT_LOOP_LOG_DEBUG("adding pollable callback to event loop, fd %d, events %X", fd, events);

    new_item = EVENT_LOOP_CALLOC(1, sizeof(*new_item));
    if (new_item == NULL) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to allocate memory for callback: %s", strerror(errno));
        goto err;
    }
    new_item->loop = loop;
    new_item->type = POLLABLE;
    new_item->as.pollable.fd = fd;
    new_item->as.pollable.callback = callback;
    new_item->as.pollable.autoclose = autoclose;
    new_item->data = data;

    struct epoll_event epoll_event;
    epoll_event.events = events;
    epoll_event.data.ptr = new_item;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event) < 0) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to add fd %d to epoll: %s", fd, strerror(errno));
        goto err;
    }

    event_loop_ll_insert(&loop->pollable_items, &new_item->link);

    return new_item;

err:
    EVENT_LOOP_FREE(new_item);
    errno = save_errno;
    return NULL;
}

struct event_loop_item *event_loop_add_unconditional(struct event_loop *loop, int priority,
                                                     event_loop_callback_unconditional callback,
                                                     void *data) {
    struct event_loop_item *new_item = NULL;
    int save_errno = 0;

    EVENT_LOOP_LOG_DEBUG("adding unconditional callback with prio %d to event loop", priority);

    new_item = EVENT_LOOP_CALLOC(1, sizeof(*new_item));
    if (new_item == NULL) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to allocate memory for callback: %s", strerror(errno));
        goto err;
    }
    new_item->loop = loop;
    new_item->type = UNCONDITIONAL;
    new_item->as.unconditional.priority = priority;
    new_item->as.unconditional.callback = callback;
    new_item->data = data;

    if (event_loop_ll_is_empty(&loop->unconditional_items)) {
        event_loop_ll_insert(&loop->unconditional_items, &new_item->link);
    } else {
        struct event_loop_item *elem;
        bool found = false;
        EVENT_LOOP_LL_FOR_EACH_REVERSE(elem, &loop->unconditional_items, link) {
            /*         |6|
             * |9|  |8|\/|4|  |2|
             * <-----------------
             * iterate from the end and find the first item with higher prio
             */
            if (elem->as.unconditional.priority > priority) {
                found = true;
                event_loop_ll_insert(&elem->link, &new_item->link);
                break;
            }
        }
        if (!found) {
            event_loop_ll_insert(&loop->unconditional_items, &new_item->link);
        }
    }

    return new_item;

err:
    EVENT_LOOP_FREE(new_item);
    errno = save_errno;
    return NULL;
}

struct event_loop_item *event_loop_add_signal(struct event_loop *loop, int signal,
                                              event_loop_callback_signal callback,
                                              void *data) {
    struct event_loop_item *new_item = NULL;
    int save_errno = 0;
    bool sigset_saved = false;
    sigset_t save_global_sigset;
    sigset_t save_loop_sigset = loop->sigset;
    bool need_reset_handler = false;

    EVENT_LOOP_LOG_DEBUG("adding signal callback for signal %d", signal);

    if (sigprocmask(SIG_BLOCK /* ignored */, NULL, &save_global_sigset) < 0) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to save original sigmask: %s", strerror(errno));
        goto err;
    }
    sigset_saved = true;

    new_item = EVENT_LOOP_CALLOC(1, sizeof(*new_item));
    if (new_item == NULL) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to allocate memory for callback: %s", strerror(errno));
        goto err;
    }
    new_item->loop = loop;
    new_item->type = SIGNAL;
    new_item->as.signal.sig = signal;
    new_item->as.signal.callback = callback;
    new_item->data = data;

    /* first, create empty sigset and add our desired signal there. */
    sigset_t set;
    sigemptyset(&set);
    if (sigaddset(&set, signal) < 0) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to add signal %d to sigset: %s", signal, strerror(errno));
        goto err;
    }

    /* block the desired signal globally. */
    if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to block signal %d: %s", signal, strerror(errno));
        goto err;
    }

    /* on success, add the same signal to loop's sigset. */
    if (sigaddset(&loop->sigset, signal) < 0) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to add signal %d to loop sigset: %s", signal, strerror(errno));
        goto err;
    }

    /* check if handler for this signal already exists */
    if (loop->signal_callbacks[signal] != NULL) {
        EVENT_LOOP_LOG_ERR("callback for signal %d already exists", signal);
        save_errno = EEXIST;
        goto err;
    }
    loop->signal_callbacks[signal] = new_item;
    loop->n_signal_callbacks += 1;
    need_reset_handler = true;

    /* change signalfd mask to report newly added signal */
    int ret = signalfd(loop->signal_fd, &loop->sigset, 0);
    if (ret < 0) {
        save_errno = errno;
        EVENT_LOOP_LOG_ERR("failed to change signalfd sigmask: %s", strerror(errno));
        goto err;
    }

    event_loop_ll_insert(&loop->signal_items, &new_item->link);

    return new_item;

err:
    /* restore original sigmask on failure. important! */
    if (sigset_saved) {
        if (sigprocmask(SIG_SETMASK, &save_global_sigset, NULL) < 0) {
            EVENT_LOOP_LOG_WARN("failed to restore original signal mask! %s", strerror(errno));
        }
    }
    loop->sigset = save_loop_sigset;

    if (need_reset_handler) {
        loop->signal_callbacks[signal] = NULL;
    }

    EVENT_LOOP_FREE(new_item);
    errno = save_errno;
    return NULL;
}

void event_loop_remove_callback(struct event_loop_item *item) {
    switch (item->type) {
    case POLLABLE: {
        int fd = item->as.pollable.fd;

        EVENT_LOOP_LOG_DEBUG("removing pollable callback for fd %d from event loop", fd);

        if (epoll_ctl(item->loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
            EVENT_LOOP_LOG_WARN("failed to remove fd %d from epoll: %s", fd, strerror(errno));
        }

        if (item->as.pollable.autoclose) {
            EVENT_LOOP_LOG_DEBUG("closing fd %d", fd);
            if (close(fd) < 0) {
                EVENT_LOOP_LOG_WARN("closing fd %d failed: %s (was it closed somewhere else?)",
                                    fd, strerror(errno));
            };
        }
        break;
    }
    case UNCONDITIONAL: {
        EVENT_LOOP_LOG_DEBUG("removing unconditional callback with prio %d from event loop",
                             item->as.unconditional.priority);
        break;
    }
    case SIGNAL: {
        int signal = item->as.signal.sig;
        struct event_loop *loop = item->loop;

        EVENT_LOOP_LOG_DEBUG("removing signal callback for signal %d from event loop", signal);

        sigdelset(&loop->sigset, signal);
        int ret = signalfd(loop->signal_fd, &loop->sigset, 0);
        if (ret < 0) {
            EVENT_LOOP_LOG_WARN("failed to remove signal %d from signalfd: %s (THIS IS VERY BAD)",
                                signal, strerror(errno));
        }

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, signal);
        if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0) {
            EVENT_LOOP_LOG_WARN("failed to unblock signal %d: %s (program might misbehave)",
                                signal, strerror(errno));
        };

        loop->signal_callbacks[signal] = NULL;
        loop->n_signal_callbacks -= 1;
        break;
    }
    }

    event_loop_ll_remove(&item->link);

    EVENT_LOOP_FREE(item);
}

struct event_loop *event_loop_item_get_loop(struct event_loop_item *item) {
    return item->loop;
}

void *event_loop_item_get_data(struct event_loop_item *item) {
    return item->data;
}

int event_loop_item_get_fd(struct event_loop_item *item) {
    if (item->type != POLLABLE) {
        return -1;
    } else {
        return item->as.pollable.fd;
    }
}

int event_loop_run(struct event_loop *loop) {
    EVENT_LOOP_LOG_DEBUG("run");

    int ret = 0;
    int number_fds = -1;
    struct epoll_event events[EVENT_LOOP_EPOLL_MAX_EVENTS];

    loop->should_quit = false;
    while (!loop->should_quit) {
        do {
            number_fds = epoll_wait(loop->epoll_fd, events, EVENT_LOOP_EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR); /* epoll_wait failing with EINTR is normal */

        if (number_fds == -1) {
            ret = errno;
            EVENT_LOOP_LOG_ERR("epoll_wait error (%s)", strerror(errno));
            loop->retcode = -ret;
            goto out;
        }

        EVENT_LOOP_LOG_DEBUG("received events on %d fds", number_fds);

        for (int n = 0; n < number_fds; n++) {
            struct event_loop_item *item = events[n].data.ptr;

            EVENT_LOOP_LOG_DEBUG("running callback for fd %d", item->as.pollable.fd);

            ret = item->as.pollable.callback(item, events[n].events);
            if (ret < 0) {
                EVENT_LOOP_LOG_ERR("callback returned %d, quitting", ret);
                loop->retcode = ret;
                goto out;
            }
        }

        /* process unconditional callbacks */
        struct event_loop_item *item, *item_tmp;
        EVENT_LOOP_LL_FOR_EACH_SAFE(item, item_tmp, &loop->unconditional_items, link) {
            EVENT_LOOP_LOG_DEBUG("running unconditional callback with prio %d",
                                 item->as.unconditional.priority);

            ret = item->as.unconditional.callback(item);
            if (ret < 0) {
                EVENT_LOOP_LOG_ERR("callback returned %d, quitting", ret);
                loop->retcode = ret;
                goto out;
            }
        }
    }

out:
    return loop->retcode;
}

void event_loop_quit(struct event_loop *loop, int retcode) {
    EVENT_LOOP_LOG_DEBUG("quit");

    loop->should_quit = true;
    loop->retcode = retcode;
}

#endif /* #ifndef EVENT_LOOP_IMPLEMENTATION */

