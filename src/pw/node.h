#pragma once

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

#include "events.h"
#include "pw/types.h"
#include "collections/dict.h"

enum media_class {
    MEDIA_CLASS_START,
    STREAM_OUTPUT_AUDIO,
    STREAM_INPUT_AUDIO,
    AUDIO_SOURCE,
    AUDIO_SINK,
    MEDIA_CLASS_END,
};

struct node;

struct node *node_create(struct pw_node *pw_node, uint32_t id, enum media_class media_class);

struct node *node_ref(struct node *node);
void node_unref(struct node **pnode);

uint32_t node_id(const struct node *node);
enum media_class node_media_class(const struct node *node);

#define ALL_CHANNELS ((uint32_t)-1)

void node_set_mute(const struct node *node, bool mute);
void node_change_volume(const struct node *node, bool absolute, float volume, uint32_t channel);
void node_set_route(const struct node *node, uint32_t route_index);
void node_set_default(const struct node *node);

struct node_events {
    void (*removed)(struct node *node, void *data);
    void (*routes)(struct node *node,
                   const struct param_route routes[], unsigned routes_count,
                   void *data);
    void (*props)(struct node *node, const struct dict *props, void *data);
    void (*channels)(struct node *node,
                     const char *channel_names[], unsigned channel_count,
                     void *data);
    void (*volume)(struct node *node,
                   const float channel_volumes[], unsigned channel_count,
                   void *data);
    void (*mute)(struct node *node, bool mute, void *data);
    void (*default_)(struct node *node, bool is_default, void *data);
};

struct event_hook *node_add_listener(struct node *node,
                                     const struct node_events *event,
                                     void *data);

