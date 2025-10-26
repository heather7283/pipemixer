#ifndef SRC_PIPEWIRE_NODE_H
#define SRC_PIPEWIRE_NODE_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

#include "pw/common.h"
#include "pw/device.h"

enum node_change_mask {
    NODE_CHANGE_NOTHING = 0,
    NODE_CHANGE_INFO = 1 << 0,
    NODE_CHANGE_MUTE = 1 << 1,
    NODE_CHANGE_VOLUME = 1 << 2,
    NODE_CHANGE_CHANNEL_COUNT = 1 << 3,
    NODE_CHANGE_EVERYTHING = ~0,
};

struct node {
    struct pw_node *pw_node;
    struct spa_hook listener;

    uint32_t id;
    enum media_class media_class;
    char *media_name;
    char *node_name;
    char *node_description;

    bool mute;
    VEC(struct node_channel {
        const char *name;
        float volume;
    }) channels;

    uint32_t device_id;
    int32_t card_profile_device;

    bool new;
    enum node_change_mask changed;

    struct signal_emitter *emitter;
};

struct node *node_lookup(uint32_t id);

void node_create(uint32_t id, enum media_class media_class);
void node_destroy(struct node *node);

void on_node_info(void *data, const struct pw_node_info *info);
void on_node_param(void *data, int seq, uint32_t id, uint32_t index,
                   uint32_t next, const struct spa_pod *param);

void node_set_mute(const struct node *node, bool mute);
void node_change_volume(const struct node *node, bool absolute, float volume, uint32_t channel);
void node_set_route(const struct node *node, uint32_t route_index);

const struct route *node_get_active_route(const struct node *node);
size_t node_get_available_routes(const struct node *node, const struct route *const **proutes);

enum node_events {
    NODE_EVENT_CHANGE = 1 << 0, /* change mask as u64 */
    NODE_EVENT_REMOVE = 1 << 1, /* node id as u64 */
    NODE_EVENT_ANY = ~0,
};

void node_events_subscribe(struct node *node,
                           struct signal_listener *listener, enum node_events events,
                           signal_callback_t callback, void *callback_data);

#endif /* #ifndef SRC_PIPEWIRE_NODE_H */

