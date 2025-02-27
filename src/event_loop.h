#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

struct event_loop;
struct event_loop_item;
struct event_loop_uncond;

typedef int (*event_loop_callback_t)(void *data, struct event_loop_item *loop_item);
typedef int (*event_loop_uncond_callback_t)(void *data, struct event_loop_uncond *uncond);

/* returns NULL on failure */
struct event_loop *event_loop_create(void);
void event_loop_cleanup(struct event_loop *loop);

struct event_loop_item *event_loop_add_item(struct event_loop *loop, int fd,
                                            event_loop_callback_t callback, void *data);
void event_loop_remove_item(struct event_loop_item *item);

struct event_loop *event_loop_item_get_loop(struct event_loop_item *item);
int event_loop_item_get_fd(struct event_loop_item *item);

/* add a callback that will be called on every event loop iteration */
struct event_loop_uncond *event_loop_add_uncond(struct event_loop *loop,
                                                event_loop_uncond_callback_t callback, void *data);
void event_loop_remove_uncond(struct event_loop_uncond *uncond);

void event_loop_run(struct event_loop *loop);
void event_loop_quit(struct event_loop *loop);

#endif /* #ifndef EVENT_LOOP_H */

