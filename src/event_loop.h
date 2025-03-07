#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

struct event_loop;
struct event_loop_item;
struct event_loop_uncond;

typedef int (*event_loop_callback_t)(void *data, struct event_loop_item *loop_item);
typedef int (*event_loop_uncond_callback_t)(void *data, struct event_loop_uncond *uncond);

/* creates a new event_loop instance. returns NULL on failure */
struct event_loop *event_loop_create(void);
/* frees all resources associated with the loop. loop is not valid after this function returns. */
void event_loop_cleanup(struct event_loop *loop);

/*
 * Add new callback to event loop.
 * If fd >= 0, fd is added to epoll interest list.
 * If fd < 0, callback will run unconditionally on every event loop iteration.
 *   Callbacks with higher priority will run before callbacks with lower priority.
 *   If two callbacks have equal priority, the order is undefined.
 */
struct event_loop_item *event_loop_add_item(struct event_loop *loop, int fd,
                                            event_loop_callback_t callback, void *data);
void event_loop_remove_item(struct event_loop_item *item);

struct event_loop *event_loop_item_get_loop(struct event_loop_item *item);
int event_loop_item_get_fd(struct event_loop_item *item);

/* add a callback that will be called on every event loop iteration */
struct event_loop_uncond *event_loop_add_uncond(struct event_loop *loop,
                                                event_loop_uncond_callback_t callback, void *data);
void event_loop_remove_uncond(struct event_loop_uncond *uncond);

int event_loop_run(struct event_loop *loop);
void event_loop_quit(struct event_loop *loop, int retcode);

#endif /* #ifndef EVENT_LOOP_H */

