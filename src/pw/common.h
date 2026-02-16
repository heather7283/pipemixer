#pragma once

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

#include "events.h"
#include "pw/node.h"
#include "pw/device.h"

/* for use is (node|device)_set_(volume|mute) functions, see node.h, device.h */
#define ALL_CHANNELS ((uint32_t)-1)

enum default_metadata_key {
    DEFAULT_AUDIO_SOURCE,
    DEFAULT_CONFIGURED_AUDIO_SOURCE,
    DEFAULT_AUDIO_SINK,
    DEFAULT_CONFIGURED_AUDIO_SINK,

    DEFAULT_METADATA_KEY_COUNT,
};

struct pipewire {
    struct pw_main_loop *main_loop;
    struct pw_loop *main_loop_loop;
    int main_loop_loop_fd;

    struct pw_context *context;

    struct pw_core *core;
    struct spa_hook core_listener;

    struct pw_registry *registry;
    struct spa_hook registry_listener;

    struct default_metadata {
        union {
            struct pw_metadata *pw_metadata;
            struct pw_proxy *pw_proxy;
        };
        struct spa_hook listener, proxy_listener;
        char *properties[DEFAULT_METADATA_KEY_COUNT];
        bool roundtrip;
    } default_metadata;

    struct event_emitter emitter;
};

extern struct pipewire pw;

int pipewire_init(void);
void pipewire_cleanup(void);

void pipewire_set_default(enum default_metadata_key key, const char *value);

struct pipewire_events {
    void (*node)(struct node *node, void *data);
    void (*device)(struct device *dev, void *data);
    void (*default_)(enum default_metadata_key key, const char *val, void *data);
};

void pipewire_add_listener(struct event_hook *hook, const struct pipewire_events *ev, void *data);

