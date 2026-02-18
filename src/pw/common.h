#pragma once

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

#include "events.h"
#include "pw/node.h"
#include "pw/device.h"
#include "pw/types.h"

enum default_metadata_key {
    DEFAULT_AUDIO_SOURCE,
    DEFAULT_CONFIGURED_AUDIO_SOURCE,
    DEFAULT_AUDIO_SINK,
    DEFAULT_CONFIGURED_AUDIO_SINK,

    DEFAULT_METADATA_KEY_COUNT,
};

bool pipewire_init(void);
void pipewire_cleanup(void);

struct node *node_lookup(pw_id_t id);
struct device *device_lookup(pw_id_t id);

void pipewire_set_default(enum default_metadata_key key, const char *value);

struct pipewire_events {
    void (*node)(struct node *node, void *data);
    void (*device)(struct device *dev, void *data);
    void (*default_)(enum default_metadata_key key, const char *val, void *data);
};

void pipewire_add_listener(struct event_hook *hook, const struct pipewire_events *ev, void *data);

