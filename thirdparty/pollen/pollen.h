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
 * pollen version 1.0.3
 * latest version is available at: https://github.com/heather7283/pollen
 *
 * This is a single-header library that provides simple event loop abstraction built on epoll.
 * To use this library, do this in one C file:
 *   #define POLLEN_IMPLEMENTATION
 *   #include "pollen.h"
 *
 * COMPILE-TIME TUNABLES:
 *   POLLEN_CALLOC(n, size) - calloc()-like function that will be used to allocate memory.
 *     Default: #define POLLEN_CALLOC(n, size) calloc(n, size)
 *   POLLEN_FREE(ptr) - free()-like function that will be used to free memory.
 *     Default: #define POLLEN_FREE(ptr) free(ptr)
 *
 *   Following macros will, if defined, be used for logging.
 *   They must expand to printf()-like function, for example:
 *   #define POLLEN_LOG_DEBUG(fmt, ...) fprintf(stderr, "event loop: " fmt "\n", ##__VA_ARGS__)
 *     POLLEN_LOG_DEBUG(fmt, ...)
 *     POLLEN_LOG_INFO(fmt, ...)
 *     POLLEN_LOG_WARN(fmt, ...)
 *     POLLEN_LOG_ERR(fmt, ...)
 */

/* ONLY UNCOMMENT THIS TO GET SYNTAX HIGHLIGHTING, DONT FORGET TO COMMENT IT BACK
#define POLLEN_IMPLEMENTATION
//*/

#ifndef POLLEN_H
#define POLLEN_H

#if !defined(POLLEN_CALLOC) || !defined(POLLEN_FREE)
    #include <stdlib.h>
#endif
#if !defined(POLLEN_CALLOC)
    #define POLLEN_CALLOC(n, size) calloc(n, size)
#endif
#if !defined(POLLEN_FREE)
    #define POLLEN_FREE(ptr) free(ptr)
#endif

/*
 * doing this to avoid compiler warning:
 * passing no argument for the '...' parameter of a variadic macro is a C23 extension
 */
#if !defined(POLLEN_DEBUG)  || !defined(POLLEN_INFO) || \
    !defined(POLLEN_WARN) || !defined(POLLEN_ERR)
    #include <stdio.h> /* printf() */
#endif
#if !defined(POLLEN_LOG_DEBUG)
    #define POLLEN_LOG_DEBUG(fmt, ...) if (0) printf(fmt, ##__VA_ARGS__)
#endif
#if !defined(POLLEN_LOG_INFO)
    #define POLLEN_LOG_INFO(fmt, ...) if (0) printf(fmt, ##__VA_ARGS__)
#endif
#if !defined(POLLEN_LOG_WARN)
    #define POLLEN_LOG_WARN(fmt, ...) if (0) printf(fmt, ##__VA_ARGS__)
#endif
#if !defined(POLLEN_LOG_ERR)
    #define POLLEN_LOG_ERR(fmt, ...) if (0) printf(fmt, ##__VA_ARGS__)
#endif

#include <sys/epoll.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>

struct pollen_callback;
typedef int (*pollen_fd_callback_fn)(struct pollen_callback *callback,
                                     int fd, uint32_t events, void *data);
typedef int (*pollen_idle_callback_fn)(struct pollen_callback *callback,
                                       void *data);
typedef int (*pollen_signal_callback_fn)(struct pollen_callback *callback,
                                         int signum, void *data);
typedef int (*pollen_timer_callback_fn)(struct pollen_callback *callback,
                                        void *data);

/* Creates a new pollen_loop instance. Returns NULL and sets errno on failure. */
struct pollen_loop *pollen_loop_create(void);
/* Frees all resources associated with the loop. Passing NULL is a harmless no-op. */
void pollen_loop_cleanup(struct pollen_loop *loop);

/*
 * Adds fd to epoll interest list.
 * Argument events directly corresponts to epoll_event.events field, see epoll_ctl(2).
 * If autoclose is true, the fd will be closed when pollen_loop_remove_callback is called.
 *
 * Returns NULL and sets errno on failure.
 */
struct pollen_callback *pollen_loop_add_fd(struct pollen_loop *loop,
                                           int fd, uint32_t events, bool autoclose,
                                           pollen_fd_callback_fn callback_fn,
                                           void *data);

/*
 * Adds a callback that will run unconditionally on every event loop iteration,
 * after all other callback types were processed.
 * Callbacks with higher priority will run before callbacks with lower priority.
 * If two callbacks have equal priority, the order is undefined.
 *
 * Returns NULL and sets errno on failure.
 */
struct pollen_callback *pollen_loop_add_idle(struct pollen_loop *loop, int priority,
                                             pollen_idle_callback_fn callback,
                                             void *data);

/*
 * Adds a callback that will run when signal is caught.
 * This function tries to preserve original sigmask if it fails.
 *
 * Returns NULL and sets errno on failure.
 */
struct pollen_callback *pollen_loop_add_signal(struct pollen_loop *loop, int signal,
                                               pollen_signal_callback_fn callback,
                                               void *data);

/*
 * Adds a callback that will run periodically every delay_ms milliseconds.
 *
 * Returns NULL and sets errno on failure.
 */
struct pollen_callback *pollen_loop_add_timer(struct pollen_loop *loop, unsigned long delay_ms,
                                              pollen_timer_callback_fn callback,
                                              void *data);

/*
 * Remove a callback from event loop.
 *
 * For fd callbacks, this function will close the fd if autoclose=true.
 * For signal callbacks, this function will unblock the signal.
 */
void pollen_loop_remove_callback(struct pollen_callback *callback);

/* Get pollen_loop instance associated with this pollen_callback. */
struct pollen_loop *pollen_callback_get_loop(struct pollen_callback *callback);

/*
 * Run the event loop. This function blocks until event loop exits.
 * This function returns 0 if no errors occured.
 * If any of the callbacks return negative value, the loop with be stopped and this value returned.
 */
int pollen_loop_run(struct pollen_loop *loop);
/*
 * Quit the event loop.
 * Argument retcode specifies the value that will be returned by pollen_loop_run.
 */
void pollen_loop_quit(struct pollen_loop *loop, int retcode);

#endif /* #ifndef POLLEN_H */

/*
 * ============================================================================
 *                              IMPLEMENTATION
 * ============================================================================
 */
#ifdef POLLEN_IMPLEMENTATION

#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#define POLLEN_EPOLL_MAX_EVENTS 16

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #define POLLEN_TYPEOF(expr) typeof(expr)
#else
    #define POLLEN_TYPEOF(expr) __typeof__(expr)
#endif

#define POLLEN_CONTAINER_OF(ptr, sample, member) \
    (POLLEN_TYPEOF(sample))((char *)(ptr) - offsetof(POLLEN_TYPEOF(*sample), member))

/*
 * Linked list.
 * In the head, next points to the first list elem, prev points to the last.
 * In the list element, next points to the next elem, prev points to the previous elem.
 * In the last element, next points to the head. In the first element, prev points to the head.
 * If the list is empty, next and prev point to the head itself.
 */
struct pollen_ll {
    struct pollen_ll *next;
    struct pollen_ll *prev;
};

static inline void pollen_ll_init(struct pollen_ll *head) {
    head->next = head;
    head->prev = head;
}

static inline bool pollen_ll_is_empty(struct pollen_ll *head) {
    return head->next == head && head->prev == head;
}

/* Inserts new after elem. */
static inline void pollen_ll_insert(struct pollen_ll *elem, struct pollen_ll *new) {
    elem->next->prev = new;
    new->next = elem->next;

    elem->next = new;
    new->prev = elem;
}

static inline void pollen_ll_remove(struct pollen_ll *elem) {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
}

#define POLLEN_LL_FOR_EACH(var, head, member) \
    for (var = POLLEN_CONTAINER_OF((head)->next, var, member); \
         &var->member != (head); \
         var = POLLEN_CONTAINER_OF(var->member.next, var, member))

#define POLLEN_LL_FOR_EACH_REVERSE(var, head, member) \
    for (var = POLLEN_CONTAINER_OF((head)->prev, var, member); \
         &var->member != (head); \
         var = POLLEN_CONTAINER_OF(var->member.prev, var, member))

#define POLLEN_LL_FOR_EACH_SAFE(var, tmp, head, member) \
    for (var = POLLEN_CONTAINER_OF((head)->next, var, member), \
         tmp = POLLEN_CONTAINER_OF((var)->member.next, tmp, member); \
         &var->member != (head); \
         var = tmp, \
         tmp = POLLEN_CONTAINER_OF(var->member.next, tmp, member))

enum pollen_callback_type {
    POLLEN_CALLBACK_TYPE_FD,
    POLLEN_CALLBACK_TYPE_IDLE,
    POLLEN_CALLBACK_TYPE_SIGNAL,
    POLLEN_CALLBACK_TYPE_TIMER,
};

struct pollen_callback {
    struct pollen_loop *loop;

    enum pollen_callback_type type;
    union {
        struct {
            int fd;
            pollen_fd_callback_fn callback;
            bool autoclose;
        } fd;
        struct {
            int priority;
            pollen_idle_callback_fn callback;
        } idle;
        struct {
            int sig;
            pollen_signal_callback_fn callback;
        } signal;
        struct {
            int fd;
            pollen_timer_callback_fn callback;
        } timer;
    } as;

    void *data;

    struct pollen_ll link;
};

struct pollen_loop {
    bool should_quit;
    int retcode;
    int epoll_fd;

    /* signal(7) says there are 38 standard signals on linux */
    struct pollen_callback *signal_callbacks[38];
    int n_signal_callbacks;
    int signal_fd;
    sigset_t sigset;
    struct pollen_callback signalfd_callback;

    struct pollen_ll fd_callbacks_list;
    struct pollen_ll idle_callbacks_list;
    struct pollen_ll signal_callbacks_list;
    struct pollen_ll timer_callbacks_list;
};

/* not an actual real callback, more like a hack to hook signal handling into the loop */
static int pollen_internal_signal_handler(struct pollen_callback *callback, int _, void *__) {
    struct pollen_loop *loop = callback->loop;

    /* TODO: figure out why does this always only read only one siginfo */
    int ret;
    struct signalfd_siginfo siginfo;
    while ((ret = read(loop->signal_fd, &siginfo, sizeof(siginfo))) == sizeof(siginfo)) {
        int signal = siginfo.ssi_signo;
        POLLEN_LOG_DEBUG("received signal %d via signalfd", signal);

        struct pollen_callback *signal_callback = loop->signal_callbacks[signal];
        if (signal_callback != NULL) {
            return signal_callback->as.signal.callback(signal_callback, signal,
                                                       signal_callback->data);
        } else {
            POLLEN_LOG_ERR("signal %d received via signalfd has no callbacks installed", signal);
            return -1;
        }
    }

    if (ret >= 0) {
        POLLEN_LOG_ERR("read incorrect amount of bytes from signalfd");
        return -1;
    } else /* ret < 0 */ {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* no more signalds to handle. exit. */
            POLLEN_LOG_DEBUG("no more signals to handle");
            return 0;
        } else {
            POLLEN_LOG_ERR("failed to read siginfo from signalfd: %s", strerror(errno));
            return -1;
        }
    }
}

struct pollen_loop *pollen_loop_create(void) {
    POLLEN_LOG_INFO("creating event loop");
    int save_errno = 0;

    struct pollen_loop *loop = POLLEN_CALLOC(1, sizeof(*loop));
    if (loop == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for event loop: %s", strerror(errno));
        goto err;
    }

    pollen_ll_init(&loop->fd_callbacks_list);
    pollen_ll_init(&loop->idle_callbacks_list);
    pollen_ll_init(&loop->signal_callbacks_list);
    pollen_ll_init(&loop->timer_callbacks_list);

    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to create epoll: %s", strerror(errno));
        goto err;
    }

    /*
     * Setup signalfd now, there's a dummy callback in the event loop struct
     * that lets us insert signal handling in the loop more easily.
     *
     * TODO: don't create signalfd until the first signal callback is added?
     */
    sigemptyset(&loop->sigset);
    loop->signal_fd = signalfd(-1, &loop->sigset, SFD_NONBLOCK | SFD_CLOEXEC);
    if (loop->signal_fd < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to create signalfd: %s", strerror(errno));
        goto err;
    }

    loop->signalfd_callback.loop = loop;
    loop->signalfd_callback.type = POLLEN_CALLBACK_TYPE_SIGNAL;
    loop->signalfd_callback.as.signal.sig = 0xDEAD;
    loop->signalfd_callback.as.signal.callback = pollen_internal_signal_handler;
    loop->signalfd_callback.data = NULL;

    struct epoll_event signalfd_epoll_event;
    signalfd_epoll_event.events = EPOLLIN;
    signalfd_epoll_event.data.ptr = &loop->signalfd_callback;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, loop->signal_fd, &signalfd_epoll_event) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to add fd %d to epoll: %s", loop->signal_fd, strerror(errno));
        goto err;
    }

    return loop;

err:
    POLLEN_FREE(loop);
    errno = save_errno;
    return NULL;
}

void pollen_loop_cleanup(struct pollen_loop *loop) {
    if (loop == NULL) {
        return;
    }

    POLLEN_LOG_INFO("cleaning up event loop");

    struct pollen_callback *callback, *callback_tmp;
    POLLEN_LL_FOR_EACH_SAFE(callback, callback_tmp, &loop->idle_callbacks_list, link) {
        pollen_loop_remove_callback(callback);
    }
    /* make sure signal are deleted before pollable bc signal handler is itself pollable */
    POLLEN_LL_FOR_EACH_SAFE(callback, callback_tmp, &loop->signal_callbacks_list, link) {
        pollen_loop_remove_callback(callback);
    }
    POLLEN_LL_FOR_EACH_SAFE(callback, callback_tmp, &loop->fd_callbacks_list, link) {
        pollen_loop_remove_callback(callback);
    }
    POLLEN_LL_FOR_EACH_SAFE(callback, callback_tmp, &loop->timer_callbacks_list, link) {
        pollen_loop_remove_callback(callback);
    }

    close(loop->signal_fd);
    close(loop->epoll_fd);

    POLLEN_FREE(loop);
}

struct pollen_callback *pollen_loop_add_fd(struct pollen_loop *loop,
                                           int fd, uint32_t events, bool autoclose,
                                           pollen_fd_callback_fn callback,
                                           void *data) {
    struct pollen_callback *new_callback = NULL;
    int save_errno = 0;

    POLLEN_LOG_INFO("adding pollable callback to event loop, fd %d, events %X", fd, events);

    new_callback = POLLEN_CALLOC(1, sizeof(*new_callback));
    if (new_callback == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for callback: %s", strerror(errno));
        goto err;
    }
    new_callback->loop = loop;
    new_callback->type = POLLEN_CALLBACK_TYPE_FD;
    new_callback->as.fd.fd = fd;
    new_callback->as.fd.callback = callback;
    new_callback->as.fd.autoclose = autoclose;
    new_callback->data = data;

    struct epoll_event epoll_event;
    epoll_event.events = events;
    epoll_event.data.ptr = new_callback;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to add fd %d to epoll: %s", fd, strerror(errno));
        goto err;
    }

    pollen_ll_insert(&loop->fd_callbacks_list, &new_callback->link);

    return new_callback;

err:
    POLLEN_FREE(new_callback);
    errno = save_errno;
    return NULL;
}

struct pollen_callback *pollen_loop_add_idle(struct pollen_loop *loop, int priority,
                                             pollen_idle_callback_fn callback,
                                             void *data) {
    struct pollen_callback *new_callback = NULL;
    int save_errno = 0;

    POLLEN_LOG_INFO("adding unconditional callback with prio %d to event loop", priority);

    new_callback = POLLEN_CALLOC(1, sizeof(*new_callback));
    if (new_callback == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for callback: %s", strerror(errno));
        goto err;
    }
    new_callback->loop = loop;
    new_callback->type = POLLEN_CALLBACK_TYPE_IDLE;
    new_callback->as.idle.priority = priority;
    new_callback->as.idle.callback = callback;
    new_callback->data = data;

    if (pollen_ll_is_empty(&loop->idle_callbacks_list)) {
        pollen_ll_insert(&loop->idle_callbacks_list, &new_callback->link);
    } else {
        struct pollen_callback *elem;
        bool found = false;
        POLLEN_LL_FOR_EACH_REVERSE(elem, &loop->idle_callbacks_list, link) {
            /*         |6|
             * |9|  |8|\/|4|  |2|
             * <-----------------
             * iterate from the end and find the first callback with higher prio
             */
            if (elem->as.idle.priority > priority) {
                found = true;
                pollen_ll_insert(&elem->link, &new_callback->link);
                break;
            }
        }
        if (!found) {
            pollen_ll_insert(&loop->idle_callbacks_list, &new_callback->link);
        }
    }

    return new_callback;

err:
    POLLEN_FREE(new_callback);
    errno = save_errno;
    return NULL;
}

struct pollen_callback *pollen_loop_add_signal(struct pollen_loop *loop, int signal,
                                               pollen_signal_callback_fn callback,
                                               void *data) {
    struct pollen_callback *new_callback = NULL;
    int save_errno = 0;
    bool sigset_saved = false;
    sigset_t save_global_sigset;
    sigset_t save_loop_sigset = loop->sigset;
    bool need_reset_handler = false;

    POLLEN_LOG_INFO("adding signal callback for signal %d", signal);

    if (sigprocmask(SIG_BLOCK /* ignored */, NULL, &save_global_sigset) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to save original sigmask: %s", strerror(errno));
        goto err;
    }
    sigset_saved = true;

    new_callback = POLLEN_CALLOC(1, sizeof(*new_callback));
    if (new_callback == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for callback: %s", strerror(errno));
        goto err;
    }
    new_callback->loop = loop;
    new_callback->type = POLLEN_CALLBACK_TYPE_SIGNAL;
    new_callback->as.signal.sig = signal;
    new_callback->as.signal.callback = callback;
    new_callback->data = data;

    /* first, create empty sigset and add our desired signal there. */
    sigset_t set;
    sigemptyset(&set);
    if (sigaddset(&set, signal) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to add signal %d to sigset: %s", signal, strerror(errno));
        goto err;
    }

    /* block the desired signal globally. */
    if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to block signal %d: %s", signal, strerror(errno));
        goto err;
    }

    /* on success, add the same signal to loop's sigset. */
    if (sigaddset(&loop->sigset, signal) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to add signal %d to loop sigset: %s", signal, strerror(errno));
        goto err;
    }

    /* check if handler for this signal already exists */
    if (loop->signal_callbacks[signal] != NULL) {
        POLLEN_LOG_ERR("callback for signal %d already exists", signal);
        save_errno = EEXIST;
        goto err;
    }
    loop->signal_callbacks[signal] = new_callback;
    loop->n_signal_callbacks += 1;
    need_reset_handler = true;

    /* change signalfd mask to report newly added signal */
    int ret = signalfd(loop->signal_fd, &loop->sigset, 0);
    if (ret < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to change signalfd sigmask: %s", strerror(errno));
        goto err;
    }

    pollen_ll_insert(&loop->signal_callbacks_list, &new_callback->link);

    return new_callback;

err:
    /* restore original sigmask on failure. important! */
    if (sigset_saved) {
        if (sigprocmask(SIG_SETMASK, &save_global_sigset, NULL) < 0) {
            POLLEN_LOG_WARN("failed to restore original signal mask! %s", strerror(errno));
        }
    }
    loop->sigset = save_loop_sigset;

    if (need_reset_handler) {
        loop->signal_callbacks[signal] = NULL;
    }

    POLLEN_FREE(new_callback);
    errno = save_errno;
    return NULL;
}

/*
 * Adds a callback that will run periodically every delay_ms milliseconds.
 *
 * Return NULL and sets errno on failure.
 */
struct pollen_callback *pollen_loop_add_timer(struct pollen_loop *loop, unsigned long delay_ms,
                                              pollen_timer_callback_fn callback,
                                              void *data) {
    struct pollen_callback *new_callback = NULL;
    int save_errno = 0;
    int tfd = -1;

    POLLEN_LOG_INFO("adding timer callback to event loop, delay %lu ms", delay_ms);

    struct itimerspec itimerspec;
    itimerspec.it_interval.tv_sec = delay_ms / 1000;
    itimerspec.it_interval.tv_nsec = (delay_ms % 1000) * 1000000L;
    itimerspec.it_value.tv_sec = delay_ms / 1000;
    itimerspec.it_value.tv_nsec = (delay_ms % 1000) * 1000000L;

    tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to create timerfd: %s", strerror(errno));
        goto err;
    }

    if (timerfd_settime(tfd, 0, &itimerspec, NULL) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to arm timer: %s", strerror(errno));
        goto err;
    }

    new_callback = POLLEN_CALLOC(1, sizeof(*new_callback));
    if (new_callback == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for callback: %s", strerror(errno));
        goto err;
    }
    new_callback->loop = loop;
    new_callback->type = POLLEN_CALLBACK_TYPE_TIMER;
    new_callback->as.timer.fd = tfd;
    new_callback->as.timer.callback = callback;
    new_callback->data = data;

    struct epoll_event epoll_event;
    epoll_event.events = EPOLLIN;
    epoll_event.data.ptr = new_callback;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, tfd, &epoll_event) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to add fd %d to epoll: %s", tfd, strerror(errno));
        goto err;
    }

    pollen_ll_insert(&loop->timer_callbacks_list, &new_callback->link);

    return new_callback;

err:
    if (tfd > 0) {
        close(tfd);
    }
    POLLEN_FREE(new_callback);
    errno = save_errno;
    return NULL;
}

void pollen_loop_remove_callback(struct pollen_callback *callback) {
    switch (callback->type) {
    case POLLEN_CALLBACK_TYPE_FD: {
        int fd = callback->as.fd.fd;

        POLLEN_LOG_INFO("removing pollable callback for fd %d from event loop", fd);

        if (epoll_ctl(callback->loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
            POLLEN_LOG_WARN("failed to remove fd %d from epoll: %s", fd, strerror(errno));
        }

        if (callback->as.fd.autoclose) {
            POLLEN_LOG_INFO("closing fd %d", fd);
            if (close(fd) < 0) {
                POLLEN_LOG_WARN("closing fd %d failed: %s (was it closed somewhere else?)",
                                    fd, strerror(errno));
            };
        }
        break;
    }
    case POLLEN_CALLBACK_TYPE_IDLE: {
        POLLEN_LOG_INFO("removing unconditional callback with prio %d from event loop",
                             callback->as.idle.priority);
        break;
    }
    case POLLEN_CALLBACK_TYPE_SIGNAL: {
        int signal = callback->as.signal.sig;
        struct pollen_loop *loop = callback->loop;

        POLLEN_LOG_INFO("removing signal callback for signal %d from event loop", signal);

        sigdelset(&loop->sigset, signal);
        int ret = signalfd(loop->signal_fd, &loop->sigset, 0);
        if (ret < 0) {
            POLLEN_LOG_WARN("failed to remove signal %d from signalfd: %s (THIS IS VERY BAD)",
                                signal, strerror(errno));
        }

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, signal);
        if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0) {
            POLLEN_LOG_WARN("failed to unblock signal %d: %s (program might misbehave)",
                                signal, strerror(errno));
        };

        loop->signal_callbacks[signal] = NULL;
        loop->n_signal_callbacks -= 1;
        break;
    }
    case POLLEN_CALLBACK_TYPE_TIMER: {
        int tfd = callback->as.timer.fd;

        POLLEN_LOG_INFO("removing timer callback with tfd %d for from event loop", tfd);

        if (epoll_ctl(callback->loop->epoll_fd, EPOLL_CTL_DEL, tfd, NULL) < 0) {
            POLLEN_LOG_WARN("failed to remove tfd %d from epoll: %s", tfd, strerror(errno));
        }

        if (close(tfd) < 0) {
            POLLEN_LOG_WARN("closing tfd %d failed: %s", tfd, strerror(errno));
        };
        break;
    }
    }

    pollen_ll_remove(&callback->link);

    POLLEN_FREE(callback);
}

struct pollen_loop *pollen_callback_get_loop(struct pollen_callback *callback) {
    return callback->loop;
}

int pollen_loop_run(struct pollen_loop *loop) {
    POLLEN_LOG_INFO("running event loop");

    int ret = 0;
    int number_fds = -1;
    struct epoll_event events[POLLEN_EPOLL_MAX_EVENTS];

    loop->should_quit = false;
    while (!loop->should_quit) {
        do {
            number_fds = epoll_wait(loop->epoll_fd, events, POLLEN_EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR); /* epoll_wait failing with EINTR is normal */

        if (number_fds == -1) {
            ret = errno;
            POLLEN_LOG_ERR("epoll_wait error (%s)", strerror(errno));
            loop->retcode = -ret;
            goto out;
        }

        POLLEN_LOG_DEBUG("received events on %d fds", number_fds);

        for (int n = 0; n < number_fds; n++) {
            struct pollen_callback *callback = events[n].data.ptr;

            switch (callback->type) {
            case POLLEN_CALLBACK_TYPE_FD:
                POLLEN_LOG_DEBUG("running callback for fd %d", callback->as.fd.fd);
                ret = callback->as.fd.callback(callback, callback->as.fd.fd,
                                               events[n].events, callback->data);
                break;
            case POLLEN_CALLBACK_TYPE_TIMER:
                POLLEN_LOG_DEBUG("running callback for timer on tfd %d", callback->as.timer.fd);

                /* drain the timer fd */
                uint64_t dummy;
                while ((ret = read(callback->as.timer.fd, &dummy, sizeof(dummy))) > 0) {
                    /* no-op */
                }
                if (ret < 0 && errno != EAGAIN) {
                    POLLEN_LOG_ERR("failed to read from timerfd %d: %s",
                                   callback->as.timer.fd, strerror(errno));
                    loop->retcode = ret;
                    goto out;
                }

                ret = callback->as.timer.callback(callback, callback->data);
                break;
            case POLLEN_CALLBACK_TYPE_SIGNAL:
                POLLEN_LOG_DEBUG("running internal signals handler");

                ret = callback->as.signal.callback(callback, 0xDEAD, NULL);
                break;
            default:
                POLLEN_LOG_ERR("got invalid callback type from epoll");
                loop->retcode = -1;
                goto out;
            }

            if (ret < 0) {
                POLLEN_LOG_ERR("callback returned %d, quitting", ret);
                loop->retcode = ret;
                goto out;
            }
        }

        /* process unconditional callbacks */
        struct pollen_callback *callback, *callback_tmp;
        POLLEN_LL_FOR_EACH_SAFE(callback, callback_tmp, &loop->idle_callbacks_list, link) {
            POLLEN_LOG_DEBUG("running unconditional callback with prio %d",
                             callback->as.idle.priority);

            ret = callback->as.idle.callback(callback, callback->data);
            if (ret < 0) {
                POLLEN_LOG_ERR("callback returned %d, quitting", ret);
                loop->retcode = ret;
                goto out;
            }
        }
    }

out:
    return loop->retcode;
}

void pollen_loop_quit(struct pollen_loop *loop, int retcode) {
    POLLEN_LOG_INFO("quitting pollen loop");

    loop->should_quit = true;
    loop->retcode = retcode;
}

#endif /* #ifndef POLLEN_IMPLEMENTATION */

