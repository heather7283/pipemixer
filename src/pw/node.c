#include <assert.h>

#include <spa/pod/builder.h>
#include <spa/param/props.h>

#include "pw/node.h"
#include "pw/device.h"
#include "pw/roundtrip.h"
#include "tui.h"
#include "log.h"
#include "macros.h"
#include "config.h"
#include "utils.h"

static void node_set_props(const struct node *node, const struct spa_pod *props) {
    if (!node->has_device) {
        pw_node_set_param(node->pw_node, SPA_PARAM_Props, 0, props);
    } else {
        struct device *device;
        if (HASHMAP_GET(device, &pw.devices, node->device_id, hash)) {
            WARN("tried to set props of node %d with associated device, "
                 "but no device with id %d was found", node->id, node->device_id);
            return;
        }

        device_set_props(device, props, node->card_profile_device);
    }
}

void node_set_mute(const struct node *node, bool mute) {
    uint8_t buffer[1024];
    struct spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    struct spa_pod *props;
    props = spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_Props,
                                       SPA_PARAM_Props, SPA_PROP_mute,
                                       SPA_POD_Bool(mute));

    node_set_props(node, props);
}

void node_change_volume(const struct node *node, bool absolute, float volume, uint32_t channel) {
    uint8_t buffer[4096];
    struct spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    float cubed_volumes[node->props.channel_count];
    for (uint32_t i = 0; i < node->props.channel_count; i++) {
        float new_volume;
        float old_volume = node->props.channel_volumes[i];

        if (channel == ALL_CHANNELS || i == channel) {
            if (absolute) {
                new_volume = volume;
            } else if (volume >= 0 /* positive delta */) {
                if (old_volume + volume > config.volume_max) {
                    new_volume = old_volume;
                } else {
                    new_volume = old_volume + volume;
                }
            }
            else /* volume < 0, negative delta */ {
                if (old_volume + volume < config.volume_min) {
                    new_volume = old_volume;
                } else {
                    new_volume = old_volume + volume;
                }
            }
        } else {
            new_volume = old_volume;
        }

        cubed_volumes[i] = new_volume * new_volume * new_volume;
    }

    struct spa_pod *props =
        spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
                                   SPA_PROP_channelVolumes,
                                   SPA_POD_Array(sizeof(float), SPA_TYPE_Float,
                                                 ARRAY_SIZE(cubed_volumes), cubed_volumes));

    node_set_props(node, props);
}

static void on_node_roundtrip_done(void *data) {
    struct node *node = data;

    if (!HASHMAP_EXISTS(&pw.nodes, node->id)) /* new node */ {
        HASHMAP_INSERT(&pw.nodes, node->id, &node->hash);

        tui_notify_node_new(node);
    } else {
        tui_notify_node_change(node);
    }

    /* reset changes */
    node->changed = NODE_CHANGE_NOTHING;
}

void on_node_info(void *data, const struct pw_node_info *info) {
    struct node *node = data;

    DEBUG("node info: id %d, op %d/%d%s, ip %d/%d%s, state %d%s, %d params%s,%s change "
          BYTE_BINARY_FORMAT,
          info->id,
          info->n_output_ports, info->max_output_ports,
          info->change_mask & PW_NODE_CHANGE_MASK_OUTPUT_PORTS ? " C" : "",
          info->n_input_ports, info->max_input_ports,
          info->change_mask & PW_NODE_CHANGE_MASK_INPUT_PORTS ? " C" : "",
          info->state,
          info->change_mask & PW_NODE_CHANGE_MASK_STATE ? " C" : "",
          info->n_params,
          info->change_mask & PW_NODE_CHANGE_MASK_PARAMS ? " C" : "",
          info->change_mask & PW_NODE_CHANGE_MASK_PROPS ? " props," : "",
          BYTE_BINARY_ARGS(info->change_mask));

    uint32_t i = 0;
    const struct spa_dict_item *item;
    spa_dict_for_each(item, info->props) {
        const char *k = item->key;
        const char *v = item->value;

        TRACE("%c---%s: %s", (++i == info->props->n_items ? '\\' : '|'), k, v);

        if (STREQ(k, PW_KEY_MEDIA_NAME)) {
            wstring_from_pchar(&node->media_name, v);
            node->changed = NODE_CHANGE_INFO;
        } else if (STREQ(k, PW_KEY_NODE_NAME) && wstring_is_empty(&node->node_name)) {
            wstring_from_pchar(&node->node_name, v);
            node->changed = NODE_CHANGE_INFO;
        } else if (STREQ(k, PW_KEY_NODE_DESCRIPTION)) {
            wstring_from_pchar(&node->node_name, v);
            node->changed = NODE_CHANGE_INFO;
        } else if (STREQ(k, PW_KEY_DEVICE_ID)) {
            node->has_device = true;
            assert(str_to_u32(v, &node->device_id));
        } else if (STREQ(k, "card.profile.device")) {
            assert(str_to_i32(v, &node->card_profile_device));
        }
    }

    bool needs_roundtrip = false;
    if (info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) {
        for (i = 0; i < info->n_params; i++) {
            struct spa_param_info *param = &info->params[i];
            if (param->id == SPA_PARAM_Props && param->flags & SPA_PARAM_INFO_READ) {
                pw_node_enum_params(node->pw_node, 0, param->id, 0, -1, NULL);
                needs_roundtrip = true;
            }
        }
    }
    if (needs_roundtrip) {
        roundtrip_async(pw.core, on_node_roundtrip_done, node);
    } else {
        on_node_roundtrip_done(node);
    }
}

void on_node_param(void *data, int seq, uint32_t id, uint32_t index,
                          uint32_t next, const struct spa_pod *param) {
    struct node *node = data;

    DEBUG("node %d param: id %d seq %d index %d next %d", node->id, id, seq, index, next);

    const struct spa_pod_prop *volumes_prop = spa_pod_find_prop(param, NULL,
                                                                SPA_PROP_channelVolumes);
    const struct spa_pod_prop *channels_prop = spa_pod_find_prop(param, NULL,
                                                                 SPA_PROP_channelMap);
    const struct spa_pod_prop *mute_prop = spa_pod_find_prop(param, NULL,
                                                             SPA_PROP_mute);
    if (volumes_prop == NULL || channels_prop == NULL || mute_prop == NULL) {
        return;
    }

    struct props *props = &node->props;

    struct spa_pod *iter;
    int i = 0;
    SPA_POD_ARRAY_FOREACH((const struct spa_pod_array *)&channels_prop->value, iter) {
        props->channel_map[i++] = channel_name_from_enum(*(enum spa_audio_channel *)iter);
    }
    i = 0;
    SPA_POD_ARRAY_FOREACH((const struct spa_pod_array *)&volumes_prop->value, iter) {
        float vol_cubed = *(float *)iter;
        float vol = cbrtf(vol_cubed);
        props->channel_volumes[i++] = vol;
    }
    const uint32_t old_channel_count = node->props.channel_count;
    props->channel_count = i;
    node->changed |= NODE_CHANGE_VOLUME;

    const bool old_mute = props->mute;
    spa_pod_get_bool(&mute_prop->value, &props->mute);
    if (old_mute != props->mute) {
        node->changed |= NODE_CHANGE_MUTE;
    }

    if (old_channel_count != props->channel_count) {
        node->changed |= NODE_CHANGE_CHANNEL_COUNT;
    }
}

void on_node_remove(struct node *node) {
    tui_notify_node_remove(node);

    HASHMAP_DELETE(&pw.nodes, node->id);
    node_free(node);
}

void node_free(struct node *node) {
    pw_proxy_destroy((struct pw_proxy *)node->pw_node);
    wstring_free(&node->media_name);
    wstring_free(&node->node_name);
    free(node);
}

