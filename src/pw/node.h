#ifndef SRC_PIPEWIRE_NODE_H
#define SRC_PIPEWIRE_NODE_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-types.h>

#include "pw/common.h"
#include "pw/device.h"
#include "strutils.h"

enum node_change_mask {
    NODE_CHANGE_NOTHING = 0,
    NODE_CHANGE_INFO = 1 << 0,
    NODE_CHANGE_MUTE = 1 << 1,
    NODE_CHANGE_VOLUME = 1 << 2,
    NODE_CHANGE_CHANNEL_COUNT = 1 << 3,
    NODE_CHANGE_EVERYTHING = ~0,
};

struct node {
    struct pw_node *pw_node;
    struct spa_hook listener;

    uint32_t id;
    enum media_class media_class;
    struct wstring media_name;
    struct wstring node_name;
    struct props props;

    bool has_device;
    struct device *device;
    int32_t card_profile_device;

    enum node_change_mask changed;

    HASHMAP_ENTRY hash;
};

void node_free(struct node *node);

void on_node_remove(struct node *node);
void on_node_info(void *data, const struct pw_node_info *info);
void on_node_param(void *data, int seq, uint32_t id, uint32_t index,
                   uint32_t next, const struct spa_pod *param);

void node_set_mute(const struct node *node, bool mute);
void node_change_volume(const struct node *node, bool absolute, float volume, uint32_t channel);

const char *node_get_current_port_name(const struct node *node);

#endif /* #ifndef SRC_PIPEWIRE_NODE_H */

