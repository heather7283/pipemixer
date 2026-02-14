#pragma once

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

#include "events.h"
#include "pw/node.h"
#include "pw/device.h"

/* for use is (node|device)_set_(volume|mute) functions, see node.h, device.h */
#define ALL_CHANNELS ((uint32_t)-1)

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
        uint32_t id;
        struct pw_metadata *pw_metadata;
        struct spa_hook listener;

        char *configured_audio_sink;
        char *audio_sink;
        char *configured_audio_source;
        char *audio_source;
    } default_metadata;

    struct event_emitter emitter;
};

extern struct pipewire pw;

int pipewire_init(void);
void pipewire_cleanup(void);

struct pipewire_events {
    void (*node)(struct node *node, void *data);
    void (*device)(struct device *dev, void *data);
};

void pipewire_add_listener(struct event_hook *hook, const struct pipewire_events *ev, void *data);

