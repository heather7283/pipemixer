#ifndef SRC_PIPEWIRE_COMMON_H
#define SRC_PIPEWIRE_COMMON_H

#include <pipewire/pipewire.h>

#include "thirdparty/cc/cc.h"

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

    cc_map(uint32_t, struct node *) nodes;
    cc_map(uint32_t, struct device *) devices;

    /* TODO: find a more sensible name for this */
    bool node_list_changed;
};
#include "thirdparty/cc/cc.h"

extern struct pw pw;

int pipewire_init(void);
void pipewire_cleanup(void);

#endif /* #ifndef SRC_PIPEWIRE_COMMON_H */

