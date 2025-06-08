#ifndef PW_H
#define PW_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

enum media_class {
    MEDIA_CLASS_START,
    STREAM_OUTPUT_AUDIO,
    STREAM_INPUT_AUDIO,
    AUDIO_SOURCE,
    AUDIO_SINK,
    MEDIA_CLASS_END,
};

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

struct node_props {
    bool mute;
    uint32_t channel_count;
    /* so much wasted ram... TODO: can I optimize mem usage? */
    float channel_volumes[SPA_AUDIO_MAX_CHANNELS];
    const char *channel_map[SPA_AUDIO_MAX_CHANNELS];
};

enum node_change_mask {
    NODE_CHANGE_INFO   = 1 << 0,
    NODE_CHANGE_MUTE   = 1 << 1,
    NODE_CHANGE_VOLUME = 1 << 2,
};

struct node {
    struct pw_node *pw_node;
    struct spa_hook listener;

    uint32_t id;
    enum media_class media_class;
    wchar_t media_name[128];
    wchar_t node_name[128];
    struct node_props props;

    bool has_device;
    uint32_t device_id;
    int32_t card_profile_device;

    enum node_change_mask changed;
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

    /* TODO: find a more sensible name for this */
    bool node_list_changed;
};

extern struct pw pw;

int pipewire_init(void);
void pipewire_cleanup(void);

void node_set_mute(struct node *node, bool mute);
/* (uint32_t)-1 to change all channels */
#define ALL_CHANNELS ((uint32_t)-1)
void node_change_volume(struct node *node, float delta, uint32_t channel);

#endif /* #ifndef PW_H */

