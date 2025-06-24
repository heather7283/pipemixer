#ifndef SRC_PIPEWIRE_DEVICE_H
#define SRC_PIPEWIRE_DEVICE_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

struct route_props {
    bool mute;
    uint32_t channel_count;
    float channel_volumes[SPA_AUDIO_MAX_CHANNELS];
    const char *channel_map[SPA_AUDIO_MAX_CHANNELS];
};

struct route {
    int32_t device, index;
    struct route_props props;

    struct spa_list link;
};

struct device {
    struct pw_device *pw_device;
    struct spa_hook listener;

    uint32_t id;

    struct spa_list routes;
};

void device_free(struct device *device);

#endif /* #ifndef SRC_PIPEWIRE_DEVICE_H */

