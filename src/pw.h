#ifndef PW_H
#define PW_H

#include <pipewire/pipewire.h>

#include "props.h"

enum media_class {
    AUDIO_SOURCE,
    AUDIO_SINK,
    STREAM_INPUT_AUDIO,
    STREAM_OUTPUT_AUDIO,
};

#define MAX_STRING_LENGTH 64
struct node {
    struct pw *pw;

    struct pw_node *pw_node;
    struct spa_hook listener;

    uint32_t id;
    enum media_class media_class;
    char media_name[MAX_STRING_LENGTH];
    char application_name[MAX_STRING_LENGTH];
    struct node_props props;

    /* don't remove */
    struct pw_node_info *info;
};
#undef MAX_STRING_LENGTH

struct pw {
    struct pw_main_loop *main_loop;
    struct pw_loop *main_loop_loop;
    int main_loop_loop_fd;

    struct pw_context *context;

    struct pw_core *core;
    struct spa_hook core_listener;

    struct pw_registry *registry;
    struct spa_hook registry_listener;

    /* stb_ds hashmap */
    struct {
        uint32_t key; /* id */
        struct node *value;
    } *nodes;
};

int pipewire_init(struct pw *pw);
void pipewire_cleanup(struct pw *pw);

#endif /* #ifndef PW_H */

