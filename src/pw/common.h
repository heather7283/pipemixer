#ifndef SRC_PIPEWIRE_COMMON_H
#define SRC_PIPEWIRE_COMMON_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

struct props {
    bool mute;
    uint32_t channel_count;
    /* TODO: dynamic array? */
    float channel_volumes[SPA_AUDIO_MAX_CHANNELS];
    const char *channel_map[SPA_AUDIO_MAX_CHANNELS];
};

enum media_class {
    MEDIA_CLASS_START,
    STREAM_OUTPUT_AUDIO,
    STREAM_INPUT_AUDIO,
    AUDIO_SOURCE,
    AUDIO_SINK,
    MEDIA_CLASS_END,
};

struct pw {
    struct pw_main_loop *main_loop;
    struct pw_loop *main_loop_loop;
    int main_loop_loop_fd;

    struct pw_context *context;

    struct pw_core *core;
    struct spa_hook core_listener;

    struct pw_registry *registry;
    struct spa_hook registry_listener;

    /* stb_ds hashmaps */
    struct {
        uint32_t key; /* id */
        struct node *value;
    } *nodes;
    struct {
        uint32_t key; /* id */
        struct device *value;
    } *devices;
};

extern struct pw pw;

int pipewire_init(void);
void pipewire_cleanup(void);

#endif /* #ifndef SRC_PIPEWIRE_COMMON_H */

