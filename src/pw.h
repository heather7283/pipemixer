#ifndef PW_H
#define PW_H

#include <pipewire/pipewire.h>

#include "props.h"

#define MAX_STRING_LENGTH 64
struct node {
    struct pw *pw;

    struct pw_node *pw_node;
    struct spa_hook listener;

    uint32_t id;
    char media_name[MAX_STRING_LENGTH];
    char application_name[MAX_STRING_LENGTH];
    struct node_props props;

    /* don't remove */
    struct pw_node_info *info;

    struct spa_list all_nodes_link;
    struct spa_list link;
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

    struct spa_list all_nodes;
    struct spa_list audio_sources;
    struct spa_list audio_sinks;
    struct spa_list audio_output_streams;
    struct spa_list audio_input_streams;
};

int pipewire_init(struct pw *pw);
void pipewire_cleanup(struct pw *pw);

#endif /* #ifndef PW_H */

