/*
 * pollen version 3.1.0
 * latest version is available at: https://github.com/heather7283/pollen
 *
 * This is a single-header library that provides simple event loop abstraction built on epoll.
 * To use this library, do this in one C file:
 *   #define POLLEN_IMPLEMENTATION
 *   #include "pollen.h"
 *
 * COMPILE-TIME TUNABLES:
 *   POLLEN_EPOLL_MAX_EVENTS - Maximum amount of events processed during one loop iteration.
 *     Default: #define POLLEN_EPOLL_MAX_EVENTS 32
 *
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

#if !defined(POLLEN_EPOLL_MAX_EVENTS)
    #define POLLEN_EPOLL_MAX_EVENTS 32
#endif

#if !defined(POLLEN_CALLOC) || !defined(POLLEN_FREE)
    #include <stdlib.h>
#endif
#if !defined(POLLEN_CALLOC)
    #define POLLEN_CALLOC(n, size) calloc(n, size)
#endif
#if !defined(POLLEN_FREE)
    #define POLLEN_FREE(ptr) free(ptr)
#endif

#if !defined(POLLEN_LOG_DEBUG)
    #define POLLEN_LOG_DEBUG(...) (void)(#__VA_ARGS__)
#endif
#if !defined(POLLEN_LOG_INFO)
    #define POLLEN_LOG_INFO(...) (void)(#__VA_ARGS__)
#endif
#if !defined(POLLEN_LOG_WARN)
    #define POLLEN_LOG_WARN(...) (void)(#__VA_ARGS__)
#endif
#if !defined(POLLEN_LOG_ERR)
    #define POLLEN_LOG_ERR(...) (void)(#__VA_ARGS__)
#endif

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>

/* I suck at naming and it took me too long to find a suitable name for this */
struct pollen_event_source;
/* Ugly but ensures backwards compatibility */
#define pollen_callback pollen_event_source

typedef int (*pollen_fd_callback_fn)(struct pollen_event_source *event_source,
                                     int fd, uint32_t events, void *data);
typedef int (*pollen_idle_callback_fn)(struct pollen_event_source *event_source,
                                       void *data);
typedef int (*pollen_signal_callback_fn)(struct pollen_event_source *event_source,
                                         int signum, void *data);
typedef int (*pollen_timer_callback_fn)(struct pollen_event_source *event_source,
                                        void *data);
typedef int (*pollen_efd_callback_fn)(struct pollen_event_source *event_source,
                                      uint64_t val, void *data);

/* Creates a new pollen_loop instance. Returns NULL and sets errno on failure. */
struct pollen_loop *pollen_loop_create(void);
/* Frees all resources associated with the loop. Passing NULL is a harmless no-op. */
void pollen_loop_cleanup(struct pollen_loop *loop);

/*
 * Adds fd to epoll interest list.
 * Argument events directly corresponts to epoll_event.events field, see epoll_ctl(2).
 * If autoclose is true, the fd will be closed when pollen_event_source_remove is called.
 *
 * Returns NULL and sets errno on failure.
 */
struct pollen_event_source *pollen_loop_add_fd(struct pollen_loop *loop,
                                               int fd, uint32_t events, bool autoclose,
                                               pollen_fd_callback_fn callback_fn,
                                               void *data);

/*
 * Modifies fd event source by calling epoll_ctl(2) with EPOLL_CTL_MOD.
 * Argument new_events directly corresponds to epoll_event.events field.
 *
 * Sets errno and returns false on failure, true on success.
 */
bool pollen_fd_modify_events(struct pollen_event_source *event_source, uint32_t new_events);

/*
 * Adds a callback that will run unconditionally on every event loop iteration,
 * after all other callback types were processed.
 * Callbacks with higher priority will run before callbacks with lower priority.
 * If two callbacks have equal priority, the order is undefined.
 *
 * Returns NULL and sets errno on failure.
 */
struct pollen_event_source *pollen_loop_add_idle(struct pollen_loop *loop, int priority,
                                                 pollen_idle_callback_fn callback,
                                                 void *data);

/*
 * Adds a callback that will run when signal is caught.
 * This function tries to preserve original sigmask if it fails.
 *
 * Returns NULL and sets errno on failure.
 */
struct pollen_event_source *pollen_loop_add_signal(struct pollen_loop *loop, int signal,
                                                   pollen_signal_callback_fn callback,
                                                   void *data);

/*
 * Adds a timerfd-based timer callback.
 * Arm/disarm the timer with pollen_timer_arm/disarm functions.
 * See timerfd_create(2) for description of clockid argument.
 *
 * Returns NULL and sets errno on failure.
 */
struct pollen_event_source *pollen_loop_add_timer(struct pollen_loop *loop, int clockid,
                                                  pollen_timer_callback_fn callback,
                                                  void *data);

/*
 * Arms the timer to expire once after initial timespec,
 * and then repeatedly every periodic timespec.
 * If absolute is true, initial is an absolute value instead of relative.
 *
 * Sets errno and returns false on failre, true on success.
 */
bool pollen_timer_arm(struct pollen_event_source *event_source, bool absolute,
                      struct timespec initial, struct timespec periodic);

/*
 * Arms the timer to expire once after initial_s seconds,
 * and then repeatedly every periodic_s seconds.
 * If absolute is true, initial_s is an absolute value instead of relative.
 *
 * Sets errno and returns false on failre, true on success.
 */
bool pollen_timer_arm_s(struct pollen_event_source *event_source, bool absolute,
                        unsigned long initial_s, unsigned long periodic_s);

/*
 * Arms the timer to expire once after initial_ms milliseconds,
 * and then repeatedly every periodic_ms milliseconds.
 * If absolute is true, initial_ms is an absolute value instead of relative.
 *
 * Sets errno and returns false on failre, true on success.
 */
bool pollen_timer_arm_ms(struct pollen_event_source *event_source, bool absolute,
                         unsigned long initial_ms, unsigned long periodic_ms);

/*
 * Arms the timer to expire once after initial_us microseconds,
 * and then repeatedly every periodic_us microseconds.
 * If absolute is true, initial_us is an absolute value instead of relative.
 *
 * Sets errno and returns false on failre, true on success.
 */
bool pollen_timer_arm_us(struct pollen_event_source *event_source, bool absolute,
                         unsigned long initial_us, unsigned long periodic_us);

/*
 * Arms the timer to expire once after initial_ns nanoseconds,
 * and then repeatedly every periodic_ns nanoseconds.
 * If absolute is true, initial_ns is an absolute value instead of relative.
 *
 * Sets errno and returns false on failre, true on success.
 */
bool pollen_timer_arm_ns(struct pollen_event_source *event_source, bool absolute,
                         unsigned long initial_ns, unsigned long periodic_ns);

/*
 * Disarms the timer.
 *
 * Sets errno and returns false on failre, true on success.
 */
bool pollen_timer_disarm(struct pollen_event_source *event_source);

/*
 * This is a convenience wrapper around eventfd(2).
 * Use pollen_efd_trigger() to increment the efd and cause the callback to run.
 * The efd will be automatically reset before running the callback.
 *
 * Returns NULL and sets errno on failure.
 */
struct pollen_event_source *pollen_loop_add_efd(struct pollen_loop *loop,
                                                pollen_efd_callback_fn callback,
                                                void *data);

/*
 * Increment efd by 1, causing the callback to run on the next loop iteration.
 * Callback must have been created by a call to pollen_loop_add_efd().
 *
 * Returns true on success, false on failure and sets errno.
 */
bool pollen_efd_trigger(struct pollen_event_source *event_source);

/*
 * Increment efd by n, causing the callback to run on the next loop iteration.
 * Callback must have been created by a call to pollen_loop_add_efd().
 *
 * Returns true on success, false on failure and sets errno.
 */
bool pollen_efd_inc(struct pollen_event_source *event_source, uint64_t n);

/*
 * Remove an event source from event loop.
 *
 * For fd event sources, this function will close the fd if autoclose=true.
 * For signal event sources, this function will unblock the signal.
 *
 * Passing NULL is a harmless no-op.
 */
void pollen_event_source_remove(struct pollen_event_source *event_source);
/* for backwards compatibility */
#define pollen_loop_remove_callback pollen_event_source_remove

/* Get pollen_loop instance associated with this pollen_event_source. */
struct pollen_loop *pollen_event_source_get_loop(struct pollen_event_source *callback);
/* for backwards compatibility */
#define pollen_callback_get_loop pollen_event_source_get_loop

/*
 * Run the event loop. This function blocks until event loop exits.
 * This function returns 0 if no errors occured.
 * If any of the callbacks return negative value,
 * the event loop with be stopped and this value returned.
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
#include <sys/eventfd.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

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

enum pollen_event_source_type {
    POLLEN_EVENT_SOURCE_TYPE_FD,
    POLLEN_EVENT_SOURCE_TYPE_IDLE,
    POLLEN_EVENT_SOURCE_TYPE_SIGNAL,
    POLLEN_EVENT_SOURCE_TYPE_TIMER,
    POLLEN_EVENT_SOURCE_TYPE_EFD,
};

struct pollen_event_source {
    struct pollen_loop *loop;

    enum pollen_event_source_type type;
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
        struct {
            int efd;
            pollen_efd_callback_fn callback;
        } efd;
    } as;

    void *data;

    struct pollen_ll link;
};

struct pollen_loop {
    bool should_quit;
    int retcode;
    int epoll_fd;

    /* Cannot do signal_sources[SIGRTMAX] here
     * because SIGRTMAX is not a compile-time constant.
     * TODO: this is cringe. Use something like a hashmap? */
    struct pollen_event_source **signal_sources;
    int signal_fd;
    sigset_t sigset;

    struct pollen_ll sources;
    struct pollen_ll idle_sources;
};

/* a hack to hook signal handling into the loop */
static int pollen_internal_signal_handler(struct pollen_event_source *event_source, int fd,
                                          uint32_t events, void *data) {
    struct pollen_loop *loop = data;

    /* TODO: figure out why does this always only read only one siginfo */
    int ret;
    struct signalfd_siginfo siginfo;
    while ((ret = read(loop->signal_fd, &siginfo, sizeof(siginfo))) == sizeof(siginfo)) {
        int signal = siginfo.ssi_signo;
        POLLEN_LOG_DEBUG("received signal %d via signalfd", signal);

        struct pollen_event_source *signal_callback = loop->signal_sources[signal];
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

static int pollen_internal_setup_signalfd(struct pollen_loop *loop) {
    int save_errno = 0;
    POLLEN_LOG_DEBUG("setting up signalfd");

    loop->signal_sources = POLLEN_CALLOC(SIGRTMAX + 1, sizeof(loop->signal_sources[0]));
    if (loop->signal_sources == NULL) {
        POLLEN_LOG_ERR("failed to allocate memory for signal sources array: %s", strerror(errno));
        goto err;
    }

    sigemptyset(&loop->sigset);
    loop->signal_fd = signalfd(-1, &loop->sigset, SFD_NONBLOCK | SFD_CLOEXEC);
    if (loop->signal_fd < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to create signalfd: %s", strerror(errno));
        goto err;
    }

    if (pollen_loop_add_fd(loop, loop->signal_fd, EPOLLIN, false,
                           pollen_internal_signal_handler, loop) == NULL) {
        save_errno = errno;
        goto err;
    }

    return 0;

err:
    free(loop->signal_sources);
    errno = save_errno;
    return -1;
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

    pollen_ll_init(&loop->sources);
    pollen_ll_init(&loop->idle_sources);

    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to create epoll: %s", strerror(errno));
        goto err;
    }

    /* signalfd will be set up when first signal callback is added */
    loop->signal_fd = -1;

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

    struct pollen_event_source *source, *source_tmp;
    POLLEN_LL_FOR_EACH_SAFE(source, source_tmp, &loop->sources, link) {
        pollen_event_source_remove(source);
    }
    POLLEN_LL_FOR_EACH_SAFE(source, source_tmp, &loop->idle_sources, link) {
        pollen_event_source_remove(source);
    }

    if (loop->signal_fd > 0) {
        close(loop->signal_fd);
    }
    free(loop->signal_sources);

    close(loop->epoll_fd);

    POLLEN_FREE(loop);
}

struct pollen_event_source *pollen_loop_add_fd(struct pollen_loop *loop,
                                               int fd, uint32_t events, bool autoclose,
                                               pollen_fd_callback_fn callback,
                                               void *data) {
    struct pollen_event_source *new_source = NULL;
    int save_errno = 0;

    POLLEN_LOG_INFO("adding fd source to event loop, fd %d, events %X", fd, events);

    new_source = POLLEN_CALLOC(1, sizeof(*new_source));
    if (new_source == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for event source: %s", strerror(errno));
        goto err;
    }
    new_source->loop = loop;
    new_source->type = POLLEN_EVENT_SOURCE_TYPE_FD;
    new_source->as.fd.fd = fd;
    new_source->as.fd.callback = callback;
    new_source->as.fd.autoclose = autoclose;
    new_source->data = data;

    struct epoll_event epoll_event;
    epoll_event.events = events;
    epoll_event.data.ptr = new_source;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to add fd %d to epoll: %s", fd, strerror(errno));
        goto err;
    }

    pollen_ll_insert(&loop->sources, &new_source->link);

    return new_source;

err:
    POLLEN_FREE(new_source);
    errno = save_errno;
    return NULL;
}

bool pollen_fd_modify_events(struct pollen_event_source *source, uint32_t new_events) {
    int save_errno;

    if (source->type != POLLEN_EVENT_SOURCE_TYPE_FD) {
        POLLEN_LOG_ERR("passed non-fd type source to pollen_fd_modify_events");
        save_errno = EINVAL;
        goto err;
    }

    POLLEN_LOG_DEBUG("modifying events for fd %d, new_events: %d",
                     source->as.fd.fd, new_events);

    struct epoll_event ev;
    ev.data.ptr = source;
    ev.events = new_events;

    if (epoll_ctl(source->loop->epoll_fd, EPOLL_CTL_MOD, source->as.fd.fd, &ev) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to modify events for fd %d: %s",
                       source->as.fd.fd, strerror(errno));
        goto err;
    }

    return true;

err:
    errno = save_errno;
    return false;
}

struct pollen_event_source *pollen_loop_add_idle(struct pollen_loop *loop, int priority,
                                                 pollen_idle_callback_fn callback,
                                                 void *data) {
    struct pollen_event_source *new_source = NULL;
    int save_errno = 0;

    POLLEN_LOG_INFO("adding idle source with prio %d to event loop", priority);

    new_source = POLLEN_CALLOC(1, sizeof(*new_source));
    if (new_source == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for event source: %s", strerror(errno));
        goto err;
    }
    new_source->loop = loop;
    new_source->type = POLLEN_EVENT_SOURCE_TYPE_IDLE;
    new_source->as.idle.priority = priority;
    new_source->as.idle.callback = callback;
    new_source->data = data;

    if (pollen_ll_is_empty(&loop->idle_sources)) {
        pollen_ll_insert(&loop->idle_sources, &new_source->link);
    } else {
        struct pollen_event_source *elem;
        bool found = false;
        POLLEN_LL_FOR_EACH_REVERSE(elem, &loop->idle_sources, link) {
            /*         |6|
             * |9|  |8|\/|4|  |2|
             * <-----------------
             * iterate from the end and find the first callback with higher prio
             */
            if (elem->as.idle.priority > priority) {
                found = true;
                pollen_ll_insert(&elem->link, &new_source->link);
                break;
            }
        }
        if (!found) {
            pollen_ll_insert(&loop->idle_sources, &new_source->link);
        }
    }

    return new_source;

err:
    POLLEN_FREE(new_source);
    errno = save_errno;
    return NULL;
}

struct pollen_event_source *pollen_loop_add_signal(struct pollen_loop *loop, int signal,
                                                   pollen_signal_callback_fn callback,
                                                   void *data) {
    struct pollen_event_source *new_source = NULL;
    int save_errno = 0;
    bool sigset_saved = false;
    sigset_t save_global_sigset;
    sigset_t save_loop_sigset = loop->sigset;
    bool need_reset_handler = false;

    POLLEN_LOG_INFO("adding signal source for signal %d", signal);

    if (loop->signal_fd < 0 && pollen_internal_setup_signalfd(loop) < 0) {
        goto err;
    }

    if (sigprocmask(SIG_BLOCK /* ignored */, NULL, &save_global_sigset) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to save original sigmask: %s", strerror(errno));
        goto err;
    }
    sigset_saved = true;

    new_source = POLLEN_CALLOC(1, sizeof(*new_source));
    if (new_source == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for event source: %s", strerror(errno));
        goto err;
    }
    new_source->loop = loop;
    new_source->type = POLLEN_EVENT_SOURCE_TYPE_SIGNAL;
    new_source->as.signal.sig = signal;
    new_source->as.signal.callback = callback;
    new_source->data = data;

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
    if (loop->signal_sources[signal] != NULL) {
        POLLEN_LOG_ERR("source for signal %d already exists", signal);
        save_errno = EEXIST;
        goto err;
    }
    loop->signal_sources[signal] = new_source;
    need_reset_handler = true;

    /* change signalfd mask to report newly added signal */
    int ret = signalfd(loop->signal_fd, &loop->sigset, 0);
    if (ret < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to change signalfd sigmask: %s", strerror(errno));
        goto err;
    }

    pollen_ll_insert(&loop->sources, &new_source->link);

    return new_source;

err:
    /* restore original sigmask on failure. important! */
    if (sigset_saved) {
        if (sigprocmask(SIG_SETMASK, &save_global_sigset, NULL) < 0) {
            POLLEN_LOG_WARN("failed to restore original signal mask! %s", strerror(errno));
        }
    }
    loop->sigset = save_loop_sigset;

    if (need_reset_handler) {
        loop->signal_sources[signal] = NULL;
    }

    POLLEN_FREE(new_source);
    errno = save_errno;
    return NULL;
}

struct pollen_event_source *pollen_loop_add_timer(struct pollen_loop *loop, int clockid,
                                                  pollen_timer_callback_fn callback,
                                                  void *data) {
    struct pollen_event_source *new_source = NULL;
    int save_errno = 0;
    int tfd = -1;

    POLLEN_LOG_INFO("adding timer source to event loop, clockid %d", clockid);

    tfd = timerfd_create(clockid, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to create timerfd: %s", strerror(errno));
        goto err;
    }

    new_source = POLLEN_CALLOC(1, sizeof(*new_source));
    if (new_source == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for event source: %s", strerror(errno));
        goto err;
    }
    new_source->loop = loop;
    new_source->type = POLLEN_EVENT_SOURCE_TYPE_TIMER;
    new_source->as.timer.fd = tfd;
    new_source->as.timer.callback = callback;
    new_source->data = data;

    struct epoll_event epoll_event;
    epoll_event.events = EPOLLIN;
    epoll_event.data.ptr = new_source;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, tfd, &epoll_event) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to add fd %d to epoll: %s", tfd, strerror(errno));
        goto err;
    }

    pollen_ll_insert(&loop->sources, &new_source->link);

    return new_source;

err:
    if (tfd > 0) {
        close(tfd);
    }
    POLLEN_FREE(new_source);
    errno = save_errno;
    return NULL;
}

bool pollen_timer_arm(struct pollen_event_source *source, bool absolute,
                      struct timespec initial, struct timespec periodic) {
    int save_errno = 0;

    if (source->type != POLLEN_EVENT_SOURCE_TYPE_TIMER) {
        POLLEN_LOG_ERR("passed non-timer type source to pollen_timer_arm");
        save_errno = EINVAL;
        goto err;
    }

    POLLEN_LOG_DEBUG("arming timerfd %d for (%li s %li ns) initial, (%li s %li ns) periodic",
                     source->as.timer.fd,
                     initial.tv_sec, initial.tv_nsec,
                     periodic.tv_sec, periodic.tv_nsec);

    const struct itimerspec itimerspec = {
        .it_value = initial,
        .it_interval = periodic,
    };
    const int flags = absolute ? TFD_TIMER_ABSTIME : 0;
    if (timerfd_settime(source->as.timer.fd, flags, &itimerspec, NULL) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to arm timer: %s", strerror(errno));
        goto err;
    }

    return true;

err:
    errno = save_errno;
    return false;
}

bool pollen_timer_arm_s(struct pollen_event_source *source, bool absolute,
                        unsigned long initial_s, unsigned long periodic_s) {
    const struct timespec initial = {
        .tv_sec = initial_s
    };
    const struct timespec periodic = {
        .tv_sec = periodic_s
    };
    return pollen_timer_arm(source, absolute, initial, periodic);
}

bool pollen_timer_arm_ms(struct pollen_event_source *source, bool absolute,
                         unsigned long initial_ms, unsigned long periodic_ms) {
    const struct timespec initial = {
        .tv_sec = initial_ms / 1000,
        .tv_nsec = (initial_ms % 1000) * 1000000,
    };
    const struct timespec periodic = {
        .tv_sec = periodic_ms / 1000,
        .tv_nsec = (periodic_ms % 1000) * 1000000,
    };
    return pollen_timer_arm(source, absolute, initial, periodic);
}

bool pollen_timer_arm_us(struct pollen_event_source *source, bool absolute,
                         unsigned long initial_us, unsigned long periodic_us) {
    const struct timespec initial = {
        .tv_sec = initial_us / 1000000,
        .tv_nsec = (initial_us % 1000000) * 1000,
    };
    const struct timespec periodic = {
        .tv_sec = periodic_us / 1000000,
        .tv_nsec = (periodic_us % 1000000) * 1000,
    };
    return pollen_timer_arm(source, absolute, initial, periodic);
}

bool pollen_timer_arm_ns(struct pollen_event_source *source, bool absolute,
                         unsigned long initial_ns, unsigned long periodic_ns) {
    const struct timespec initial = {
        .tv_sec = initial_ns / 1000000000,
        .tv_nsec = initial_ns % 1000000000,
    };
    const struct timespec periodic = {
        .tv_sec = periodic_ns / 1000000000,
        .tv_nsec = periodic_ns % 1000000000,
    };
    return pollen_timer_arm(source, absolute, initial, periodic);
}

bool pollen_timer_disarm(struct pollen_event_source *source) {
    int save_errno = 0;

    if (source->type != POLLEN_EVENT_SOURCE_TYPE_TIMER) {
        POLLEN_LOG_ERR("passed non-timer type source to pollen_timer_disarm");
        save_errno = EINVAL;
        goto err;
    }

    POLLEN_LOG_DEBUG("disarming timerfd %d", source->as.timer.fd);

    struct itimerspec itimerspec;
    itimerspec.it_value.tv_sec = 0;
    itimerspec.it_value.tv_nsec = 0;
    itimerspec.it_interval.tv_sec = 0;
    itimerspec.it_interval.tv_nsec = 0;

    if (timerfd_settime(source->as.timer.fd, 0, &itimerspec, NULL) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to disarm timer: %s", strerror(errno));
        goto err;
    }

    return true;

err:
    errno = save_errno;
    return false;
}

struct pollen_event_source *pollen_loop_add_efd(struct pollen_loop *loop,
                                                pollen_efd_callback_fn callback,
                                                void *data) {
    struct pollen_event_source *new_source = NULL;
    int save_errno = 0;

    POLLEN_LOG_INFO("adding efd source to event loop");

    int efd = eventfd(0, EFD_CLOEXEC);
    if (efd < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to create eventfd: %s", strerror(errno));
        goto err;
    }

    new_source = POLLEN_CALLOC(1, sizeof(*new_source));
    if (new_source == NULL) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to allocate memory for event source: %s", strerror(errno));
        goto err;
    }
    new_source->loop = loop;
    new_source->type = POLLEN_EVENT_SOURCE_TYPE_EFD;
    new_source->as.efd.efd = efd;
    new_source->as.efd.callback = callback;
    new_source->data = data;

    struct epoll_event epoll_event;
    epoll_event.events = EPOLLIN;
    epoll_event.data.ptr = new_source;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, efd, &epoll_event) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to add efd %d to epoll: %s", efd, strerror(errno));
        goto err;
    }

    pollen_ll_insert(&loop->sources, &new_source->link);

    return new_source;

err:
    POLLEN_FREE(new_source);
    errno = save_errno;
    return NULL;
}

bool pollen_efd_inc(struct pollen_event_source *source, uint64_t n) {
    int save_errno;

    if (source->type != POLLEN_EVENT_SOURCE_TYPE_EFD) {
        POLLEN_LOG_ERR("passed non-efd type source to pollen_efd_trigger");
        save_errno = EINVAL;
        goto err;
    }

    if (write(source->as.efd.efd, &n, sizeof(n)) < 0) {
        save_errno = errno;
        POLLEN_LOG_ERR("failed to write %lu to efd %d: %s",
                       n, source->as.efd.efd, strerror(errno));
        goto err;
    }

    return true;

err:
    errno = save_errno;
    return false;
}

bool pollen_efd_trigger(struct pollen_event_source *source) {
    return pollen_efd_inc(source, 1);
}

void pollen_event_source_remove(struct pollen_event_source *source) {
    if (source == NULL) {
        return;
    }

    switch (source->type) {
    case POLLEN_EVENT_SOURCE_TYPE_FD: {
        int fd = source->as.fd.fd;

        POLLEN_LOG_INFO("removing fd source for fd %d from event loop", fd);

        if (epoll_ctl(source->loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
            POLLEN_LOG_WARN("failed to remove fd %d from epoll: %s", fd, strerror(errno));
        }

        if (source->as.fd.autoclose) {
            POLLEN_LOG_INFO("closing fd %d", fd);
            if (close(fd) < 0) {
                POLLEN_LOG_WARN("closing fd %d failed: %s (was it closed somewhere else?)",
                                fd, strerror(errno));
            };
        }
        break;
    }
    case POLLEN_EVENT_SOURCE_TYPE_IDLE: {
        POLLEN_LOG_INFO("removing idle source with prio %d from event loop",
                        source->as.idle.priority);
        break;
    }
    case POLLEN_EVENT_SOURCE_TYPE_SIGNAL: {
        int signal = source->as.signal.sig;
        struct pollen_loop *loop = source->loop;

        POLLEN_LOG_INFO("removing signal source for signal %d from event loop", signal);

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

        loop->signal_sources[signal] = NULL;
        break;
    }
    case POLLEN_EVENT_SOURCE_TYPE_TIMER: {
        int tfd = source->as.timer.fd;

        POLLEN_LOG_INFO("removing timer source with tfd %d for from event loop", tfd);

        if (epoll_ctl(source->loop->epoll_fd, EPOLL_CTL_DEL, tfd, NULL) < 0) {
            POLLEN_LOG_WARN("failed to remove tfd %d from epoll: %s", tfd, strerror(errno));
        }

        if (close(tfd) < 0) {
            POLLEN_LOG_WARN("closing tfd %d failed: %s", tfd, strerror(errno));
        };
        break;
    }
    case POLLEN_EVENT_SOURCE_TYPE_EFD: {
        int efd = source->as.efd.efd;

        POLLEN_LOG_INFO("removing efd source for efd %d from event loop", efd);

        if (epoll_ctl(source->loop->epoll_fd, EPOLL_CTL_DEL, efd, NULL) < 0) {
            POLLEN_LOG_WARN("failed to remove efd %d from epoll: %s", efd, strerror(errno));
        }

        if (close(efd) < 0) {
            POLLEN_LOG_WARN("closing efd %d failed: %s", efd, strerror(errno));
        };
        break;
    }
    }

    pollen_ll_remove(&source->link);

    POLLEN_FREE(source);
}

struct pollen_loop *pollen_event_source_get_loop(struct pollen_event_source *source) {
    return source->loop;
}

int pollen_loop_run(struct pollen_loop *loop) {
    POLLEN_LOG_INFO("running event loop");

    int ret = 0;
    int number_fds = -1;
    static struct epoll_event events[POLLEN_EPOLL_MAX_EVENTS];

    loop->should_quit = false;
    while (!loop->should_quit) {
        do {
            number_fds = epoll_wait(loop->epoll_fd, events, POLLEN_EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR);

        if (number_fds == -1) {
            ret = errno;
            POLLEN_LOG_ERR("epoll_wait error (%s)", strerror(errno));
            loop->retcode = -ret;
            goto out;
        }

        POLLEN_LOG_DEBUG("received events on %d fds", number_fds);

        for (int n = 0; n < number_fds; n++) {
            struct pollen_event_source *source = events[n].data.ptr;

            switch (source->type) {
            case POLLEN_EVENT_SOURCE_TYPE_FD:
                POLLEN_LOG_DEBUG("running callback for fd %d", source->as.fd.fd);
                ret = source->as.fd.callback(source, source->as.fd.fd,
                                               events[n].events, source->data);
                break;
            case POLLEN_EVENT_SOURCE_TYPE_TIMER:
                POLLEN_LOG_DEBUG("running callback for timer on tfd %d", source->as.timer.fd);

                /* drain the timer fd */
                uint64_t dummy;
                while ((ret = read(source->as.timer.fd, &dummy, sizeof(dummy))) > 0) {
                    /* no-op */
                }
                if (ret < 0 && errno != EAGAIN) {
                    POLLEN_LOG_ERR("failed to read from timerfd %d: %s",
                                   source->as.timer.fd, strerror(errno));
                    loop->retcode = ret;
                    goto out;
                }

                ret = source->as.timer.callback(source, source->data);
                break;
            case POLLEN_EVENT_SOURCE_TYPE_EFD:
                POLLEN_LOG_DEBUG("running callback for efd %d", source->as.efd.efd);

                uint64_t efd_val;
                if (read(source->as.efd.efd, &efd_val, sizeof(efd_val)) < 0) {
                    POLLEN_LOG_ERR("failed to read from efd %d: %s",
                                   source->as.efd.efd, strerror(errno));
                    loop->retcode = -1;
                    goto out;
                }

                ret = source->as.efd.callback(source, efd_val, source->data);
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

        /* dispatch idle callbacks */
        struct pollen_event_source *source, *source_tmp;
        POLLEN_LL_FOR_EACH_SAFE(source, source_tmp, &loop->idle_sources, link) {
            POLLEN_LOG_DEBUG("running idle callback with prio %d", source->as.idle.priority);

            ret = source->as.idle.callback(source, source->data);
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

/*
 * pollen is licensed under the standard MIT license:
 *
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
 */
