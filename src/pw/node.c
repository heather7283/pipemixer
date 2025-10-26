#include <assert.h>
#include <math.h>

#include <spa/pod/builder.h>
#include <spa/param/props.h>

#include "pw/node.h"
#include "pw/device.h"
#include "pw/roundtrip.h"
#include "pw/events.h"
#include "collections/map.h"
#include "log.h"
#include "xmalloc.h"
#include "macros.h"
#include "config.h"
#include "utils.h"

static MAP(struct node *) nodes = {0};

static enum spa_direction media_class_to_direction(enum media_class class) {
    switch (class) {
    case STREAM_OUTPUT_AUDIO:
    case AUDIO_SINK:
        return SPA_DIRECTION_OUTPUT;
    case STREAM_INPUT_AUDIO:
    case AUDIO_SOURCE:
        return SPA_DIRECTION_INPUT;
    default:
        assert(0 && "Unexpected media_class value passed to media_class_to_direction");
    }
}

struct node *node_lookup(uint32_t id) {
    struct node **node = MAP_GET(&nodes, id);
    if (node == NULL) {
        WARN("node with id %u was not found", id);
        return NULL;
    }
    return *node;
}

static void node_set_props(const struct node *node, const struct spa_pod *props) {
    if (node->device_id == 0) {
        pw_node_set_param(node->pw_node, SPA_PARAM_Props, 0, props);
    } else {
        struct device *dev = device_lookup(node->device_id);
        if (dev == NULL) {
            WARN("tried to set props of node %d with associated device, "
                 "but no device was found", node->id);
            return;
        }

        enum spa_direction direction = media_class_to_direction(node->media_class);
        device_set_props(dev, props, direction, node->card_profile_device);
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

    float cubed_volumes[VEC_SIZE(&node->channels)];
    for (uint32_t i = 0; i < VEC_SIZE(&node->channels); i++) {
        float new_volume;
        float old_volume = VEC_AT(&node->channels, i)->volume;

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
                                                 SIZEOF_ARRAY(cubed_volumes), cubed_volumes));

    node_set_props(node, props);
}

void node_set_route(const struct node *node, uint32_t route_index) {
    if (node->device_id == 0) {
        WARN("Tried to set route on a node that does not have a device");
    } else {
        struct device *dev = device_lookup(node->device_id);
        if (dev == NULL) {
            ERROR("Tried to set route on a node but no device was found");
        } else {
            device_set_route(dev, node->card_profile_device, route_index);
        }
    }
}

size_t node_get_available_routes(const struct node *node, const struct route *const **proutes) {
    static VEC(const struct route *) routes = {0};

    if (node->device_id == 0) {
        return 0;
    }
    const struct device *dev = device_lookup(node->device_id);
    if (dev == NULL) {
        return 0;
    }

    if (dev->active_profile == NULL) {
        ERROR("cannot get available routes for node %d with dev %d: no active profile on node",
              node->id, dev->id);
        return 0;
    }

    const enum spa_direction direction = media_class_to_direction(node->media_class);

    VEC_CLEAR(&routes);
    VEC_FOREACH(&dev->routes, i) {
        const struct route *route = VEC_AT(&dev->routes, i);
        if (route->direction != direction) {
            continue;
        }

        VEC_FOREACH(&route->profiles, j) {
            const int32_t profile = *VEC_AT(&route->profiles, j);
            if (profile == dev->active_profile->index) {
                VEC_APPEND(&routes, &route);
            }
        }
    }

    if (proutes != NULL) {
        *proutes = routes.data;
    }
    return VEC_SIZE(&routes);
}

const struct route *node_get_active_route(const struct node *node) {
    if (node->device_id == 0) {
        return NULL;
    }
    const struct device *dev = device_lookup(node->device_id);
    if (dev == NULL) {
        return NULL;
    }

    const enum spa_direction direction = media_class_to_direction(node->media_class);
    VEC_FOREACH(&dev->active_routes, i) {
        const struct route *route = VEC_AT(&dev->active_routes, i);

        if (route->device != node->card_profile_device || route->direction != direction) {
            continue;
        }

        VEC_FOREACH(&route->profiles, j) {
            const int32_t profile = *VEC_AT(&route->profiles, j);
            if (profile == dev->active_profile->index) {
                return route;
            }
        }
    }

    WARN("did not find active route for node %d", node->id);
    return NULL;
}

static void on_node_peak(float peaks[], unsigned int npeaks, void *data) {
    /* we're in a foreign thread here! TODO: do I need a mutex? */
    struct node *const node = data;

    for (unsigned int i = 0; i < MIN(VEC_SIZE(&node->channels), npeaks); i++) {
        VEC_AT(&node->channels, i)->peak = peaks[i];
    }
}

static void on_node_initial_roundtrip_done(struct node *node) {
    stream_create(&node->listening_stream, node->serial, on_node_peak, node);

        signal_emit_u64(pw.emitter, PIPEWIRE_EVENT_NODE_ADDED, node->id);
}

static void on_node_roundtrip_done(void *data) {
    /* node might get removed before roundtrip finishes,
     * so instead of passing node by ptr here pass its id
     * and look it up in the hashmap when roundtrip finishes */
    struct node *node = node_lookup((uintptr_t)data);
    if (node == NULL) {
        WARN("roundtrip finished for node that does not exist!");
        WARN("was it removed after roundtrip started?");
        return;
    }

    if (node->new) {
        node->new = false;
        on_node_initial_roundtrip_done(node);
    } else {
        signal_emit_u64(node->emitter, NODE_EVENT_CHANGE, node->changed);
    }
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

    /* reset changes */
    node->changed = NODE_CHANGE_NOTHING;

    uint32_t i = 0;
    const struct spa_dict_item *item;
    spa_dict_for_each(item, info->props) {
        const char *k = item->key;
        const char *v = item->value;

        TRACE("%c---%s: %s", (++i == info->props->n_items ? '\\' : '|'), k, v);

        if (STREQ(k, PW_KEY_MEDIA_NAME)) {
            free(node->media_name);
            node->media_name = xstrdup(v);
            node->changed = NODE_CHANGE_INFO;
        } else if (STREQ(k, PW_KEY_NODE_NAME)) {
            free(node->node_name);
            node->node_name = xstrdup(v);
            node->changed = NODE_CHANGE_INFO;
        } else if (STREQ(k, PW_KEY_NODE_DESCRIPTION)) {
            free(node->node_description);
            node->node_description = xstrdup(v);
            node->changed = NODE_CHANGE_INFO;
        } else if (STREQ(k, PW_KEY_DEVICE_ID)) {
            str_to_u32(v, &node->device_id);
        } else if (STREQ(k, PW_KEY_OBJECT_SERIAL)) {
            spa_atou64(v, &node->serial, 10);
        } else if (STREQ(k, "card.profile.device")) {
            str_to_i32(v, &node->card_profile_device);
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
        roundtrip_async(pw.core, on_node_roundtrip_done, (void *)(uintptr_t)node->id);
    } else {
        on_node_roundtrip_done((void *)(uintptr_t)node->id);
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

    const uint32_t old_channel_count = VEC_SIZE(&node->channels);
    VEC_CLEAR(&node->channels);

    const struct spa_pod_array *volumes_arr = (const struct spa_pod_array *)&volumes_prop->value;
    const struct spa_pod_array *channels_arr = (const struct spa_pod_array *)&channels_prop->value;
    const uint32_t volumes_child_size = volumes_arr->body.child.size;
    const uint32_t channels_child_size = channels_arr->body.child.size;
    const uint32_t n_channels = (volumes_arr->pod.size - 8) / volumes_child_size;

    /* cursed af */
    for (uint32_t i = 0; i < n_channels; i++) {
        const float *volume =
            (void *)((uintptr_t)&volumes_arr->body + 8 + (volumes_child_size * i));
        const enum spa_audio_channel *channel_enum =
            (void *)((uintptr_t)&channels_arr->body + 8 + (channels_child_size * i));

        const struct node_channel c = {
            .volume = cbrtf(*volume),
            .name = channel_name_from_enum(*channel_enum),
        };
        VEC_APPEND(&node->channels, &c);
    }

    node->changed |= NODE_CHANGE_VOLUME;

    const bool old_mute = node->mute;
    spa_pod_get_bool(&mute_prop->value, &node->mute);
    if (old_mute != node->mute) {
        node->changed |= NODE_CHANGE_MUTE;
    }

    if (old_channel_count != VEC_SIZE(&node->channels)) {
        node->changed |= NODE_CHANGE_CHANNEL_COUNT;
    }
}

static const struct pw_node_events node_events = {
    .version = PW_VERSION_NODE_EVENTS,
    .info = on_node_info,
    .param = on_node_param,
};

void node_create(uint32_t id, enum media_class media_class) {
    struct node *node = xmalloc(sizeof(*node));

    *node = (struct node){
        .id = id,
        .new = true,
        .media_class = media_class,
        .pw_node = pw_registry_bind(pw.registry, id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0),
        .emitter = signal_emitter_create(),
    };

    pw_node_add_listener(node->pw_node, &node->listener, &node_events, node);

    MAP_INSERT(&nodes, id, &node);
}

void node_destroy(struct node *node) {
    signal_emit_u64(node->emitter, NODE_EVENT_REMOVE, node->id);

    stream_destroy(&node->listening_stream);

    pw_proxy_destroy((struct pw_proxy *)node->pw_node);
    free(node->media_name);
    free(node->node_name);
    free(node->node_description);
    VEC_FREE(&node->channels);

    MAP_REMOVE(&nodes, node->id);

    free(node);
}

void node_events_subscribe(struct node *node,
                           struct signal_listener *listener, enum node_events events,
                           signal_callback_t callback, void *callback_data) {
    signal_listener_subscribe(listener, node->emitter, events, callback, callback_data);
}

