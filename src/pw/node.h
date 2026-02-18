#pragma once

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

#include "pw/device.h"
#include "collections/map.h"
#include "events.h"

enum media_class {
    MEDIA_CLASS_START,
    STREAM_OUTPUT_AUDIO,
    STREAM_INPUT_AUDIO,
    AUDIO_SOURCE,
    AUDIO_SINK,
    MEDIA_CLASS_END,
};

enum node_change_mask {
    NODE_CHANGE_NOTHING = 0,
    NODE_CHANGE_INFO = 1 << 0,
    NODE_CHANGE_MUTE = 1 << 1,
    NODE_CHANGE_VOLUME = 1 << 2,
    NODE_CHANGE_CHANNEL_COUNT = 1 << 3,
    NODE_CHANGE_DEFAULT = 1 << 4,
    NODE_CHANGE_EVERYTHING = ~0,
};

struct node_props {
    char *media_name;
    char *node_name;
    char *node_description;
};

struct node {
    union {
        struct pw_node *pw_node;
        struct pw_proxy *pw_proxy;
    };
    struct spa_hook listener;
    struct spa_hook proxy_listener;

    uint32_t id;
    enum media_class media_class;
    struct node_props props;

    bool mute;
    unsigned n_channels;
    const char **channel_names;
    float *channel_volumes;

    bool is_default;
    struct event_hook default_listener;

    uint32_t device_id;
    struct device *device;
    struct event_hook device_hook;

    int32_t card_profile_device;
    struct route *routes;
    unsigned n_routes;

    struct event_emitter emitter;

    bool new;
    unsigned refcnt;
};

extern MAP(struct node *) nodes;

struct node *node_lookup(uint32_t id);

void node_create(struct pw_node *pw_node, uint32_t id, enum media_class media_class);

struct node *node_ref(struct node *node);
void node_unref(struct node **pnode);

#define ALL_CHANNELS ((uint32_t)-1)

void node_set_mute(const struct node *node, bool mute);
void node_change_volume(const struct node *node, bool absolute, float volume, uint32_t channel);
void node_set_route(const struct node *node, uint32_t route_index);
void node_set_default(const struct node *node);

const struct route *node_get_active_route(const struct node *node);
size_t node_get_available_routes(const struct node *node, const struct route *const **proutes);

struct node_events {
    void (*removed)(struct node *node, void *data);
    void (*routes)(struct node *node, const struct route routes[], unsigned count, void *data);
    void (*props)(struct node *node, const struct node_props *props, void *data);
    void (*channels)(struct node *node, const char *channels[], unsigned count, void *data);
    void (*volume)(struct node *node, const float channels[], unsigned count, void *data);
    void (*mute)(struct node *node, bool mute, void *data);
    void (*default_)(struct node *node, bool is_default, void *data);
};

void node_add_listener(struct node *node, struct event_hook *hook,
                       const struct node_events *events, void *data);

