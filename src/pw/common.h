#ifndef SRC_PIPEWIRE_COMMON_H
#define SRC_PIPEWIRE_COMMON_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

#include "signals.h"

/* for use is (node|device)_set_(volume|mute) functions, see node.h, device.h */
#define ALL_CHANNELS ((uint32_t)-1)

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

    struct signal_emitter *emitter;
};

extern struct pw pw;

int pipewire_init(void);
void pipewire_cleanup(void);

#endif /* #ifndef SRC_PIPEWIRE_COMMON_H */

