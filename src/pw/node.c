#include <search.h>
#include <assert.h>
#include <math.h>

#include <spa/pod/builder.h>
#include <spa/pod/parser.h>
#include <spa/param/props.h>

#include "pw/node.h"
#include "pw/device.h"
#include "pw/common.h"
#include "log.h"
#include "xmalloc.h"
#include "macros.h"
#include "config.h"
#include "utils.h"

enum node_event_types {
    NODE_EVENT_REMOVED,
    NODE_EVENT_ROUTES,
    NODE_EVENT_PROPS,
    NODE_EVENT_CHANNELS,
    NODE_EVENT_VOLUME,
    NODE_EVENT_MUTE,
    NODE_EVENT_DEFAULT,
};

static void node_event_dispatcher(uint64_t id, union event_data data,
                                  const void *callbacks, void *callbacks_data,
                                  void *private_data) {
    const struct node_events *table = callbacks;
    struct node *node = private_data;

    switch ((enum node_event_types)id) {
    case NODE_EVENT_REMOVED:
        EVENT_DISPATCH(table->removed, node, callbacks_data);
        break;
    case NODE_EVENT_ROUTES:
        EVENT_DISPATCH(table->routes, node, node->routes, node->n_routes, callbacks_data);
        break;
    case NODE_EVENT_PROPS:
        EVENT_DISPATCH(table->props, node, &node->props, callbacks_data);
        break;
    case NODE_EVENT_CHANNELS:
        EVENT_DISPATCH(table->channels, node,
                       node->param_props.channel_names, node->param_props.n_channels,
                       callbacks_data);
        break;
    case NODE_EVENT_VOLUME:
        EVENT_DISPATCH(table->volume, node,
                       node->param_props.channel_volumes, node->param_props.n_channels,
                       callbacks_data);
        break;
    case NODE_EVENT_MUTE:
        EVENT_DISPATCH(table->mute, node, node->param_props.mute, callbacks_data);
        break;
    case NODE_EVENT_DEFAULT:
        EVENT_DISPATCH(table->default_, node, node->is_default, callbacks_data);
        break;
    default:
        ERROR("unexpected node event id %"PRIu64, id);
    }
}

static void emit_removed(struct node *node, struct event_hook *hook) {
    TRACE("node emit_removed(%p)", node);
    event_emit(node->emitter, hook, NODE_EVENT_REMOVED, '0');
}

static void emit_routes(struct node *node, struct event_hook *hook) {
    event_emit(node->emitter, hook, NODE_EVENT_ROUTES, '0');
}

static void emit_props(struct node *node, struct event_hook *hook) {
    event_emit(node->emitter, hook, NODE_EVENT_PROPS, '0');
}

static void emit_channels(struct node *node, struct event_hook *hook) {
    event_emit(node->emitter, hook, NODE_EVENT_CHANNELS, '0');
}

static void emit_volume(struct node *node, struct event_hook *hook) {
    event_emit(node->emitter, hook, NODE_EVENT_VOLUME, '0');
}

static void emit_mute(struct node *node, struct event_hook *hook) {
    event_emit(node->emitter, hook, NODE_EVENT_MUTE, '0');
}

static void emit_default(struct node *node, struct event_hook *hook) {
    event_emit(node->emitter, hook, NODE_EVENT_DEFAULT, '0');
}

static void hook_remove(void *private_data) {
    struct node *node = private_data;
    node_unref(&node);
}

struct event_hook *node_add_listener(struct node *node, const struct node_events *ev, void *data) {
    struct event_hook *hook = event_emitter_add_hook(node->emitter, ev, data,
                                                     hook_remove, node_ref(node));
    if (node->has_props) {
        emit_props(node, hook);
    }
    if (node->has_routes) {
        emit_routes(node, hook);
    }
    if (node->has_param_props) {
        emit_channels(node, hook);
        emit_volume(node, hook);
        emit_mute(node, hook);
    }
    if (node->has_default) {
        emit_default(node, hook);
    }

    return hook;
}

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

static void node_set_props(const struct node *node, const struct spa_pod *props) {
    if (!node->device_id) {
        pw_node_set_param(node->pw_node, SPA_PARAM_Props, 0, props);
    } else if (!node->device) {
        WARN("tried to set props of node %d with device, but no device was found", node->id);
    } else {
        enum spa_direction direction = media_class_to_direction(node->media_class);
        device_set_props(node->device, props, direction, node->card_profile_device);
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

    float cubed_volumes[node->param_props.n_channels];
    for (uint32_t i = 0; i < node->param_props.n_channels; i++) {
        float new_volume;
        const float old_volume = node->param_props.channel_volumes[i];

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
    if (!node->device) {
        WARN("Tried to set route on a node that does not have a device");
    } else {
        device_set_route(node->device, node->card_profile_device, route_index);
    }
}

void node_set_default(const struct node *node) {
    enum default_metadata_key key;
    switch (node->media_class) {
    case AUDIO_SINK:
        key = DEFAULT_CONFIGURED_AUDIO_SINK;
        break;
    case AUDIO_SOURCE:
        key = DEFAULT_CONFIGURED_AUDIO_SOURCE;
        break;
    default:
        WARN("node_set_default called on a node that's neither a sink nor a source");
        return;
    }

    pipewire_set_default(key, node->props.node_name);
}

static void on_default(enum default_metadata_key key, const char *val, void *data) {
    struct node *node = data;

    /* on_default should only fire for sources and sinks */
    ASSERT(node->media_class == AUDIO_SOURCE || node->media_class == AUDIO_SINK);

    /* there are 2 types of default metadata values, those that have "configured"
     * in their key and those that do not. I have no idea what the difference is,
     * it is of course not documented anywhere. I'm just going to ignore those
     * with "configured" and hope for the best. */
    if (key == DEFAULT_CONFIGURED_AUDIO_SINK || key == DEFAULT_CONFIGURED_AUDIO_SOURCE) {
        return;
    }

    const bool match = (key == DEFAULT_AUDIO_SINK && node->media_class == AUDIO_SINK)
                    || (key == DEFAULT_AUDIO_SOURCE && node->media_class == AUDIO_SOURCE);
    if (!match) {
        return;
    }

    const bool is_default = streq(node->props.node_name, val);
    if (is_default != node->is_default || !node->has_default) {
        node->is_default = is_default;
        INFO("node %d default: %d", node->id, node->is_default);
        emit_default(node, NULL);
        node->has_default = true;
    }
}

static const struct pipewire_events pipewire_events = {
    .default_ = on_default,
};

static void process_param_props(struct node *node, bool mute, unsigned n_channels,
                                const char *names[], const float volumes[]) {
    struct param_props *props = &node->param_props;

    if (props->n_channels != n_channels || !node->has_param_props) {
        props->channel_names = xreallocarray(props->channel_names,
                                             n_channels,
                                             sizeof(props->channel_names[0]));
        props->channel_volumes = xreallocarray(props->channel_volumes,
                                               n_channels,
                                               sizeof(props->channel_volumes[0]));
        props->n_channels = n_channels;

        INFO("node %u n_channels %d", node->id, n_channels);
        emit_channels(node, NULL);
    }

    memcpy(props->channel_names, names, sizeof(names[0]) * n_channels);
    memcpy(props->channel_volumes, volumes, sizeof(volumes[0]) * n_channels);

    emit_volume(node, NULL);

    if (mute != props->mute || !node->has_param_props) {
        props->mute = mute;
        INFO("node %u mute %d", node->id, mute);
        emit_mute(node, NULL);
    }

    node->has_param_props = true;
}

static int int32_cmp(const void *a, const void *b) {
    return (*(int32_t *)a != *(int32_t *)b);
}

static void on_device_routes(struct device *dev,
                             const struct param_route *routes, unsigned len, void *data) {
    struct node *node = data;

    for (unsigned i = 0; i < node->n_routes; i++) {
        struct param_route *route = &node->routes[i];
        param_route_free_contents(route);
    }

    node->routes = xreallocarray(node->routes, len, sizeof(node->routes[0]));
    node->n_routes = 0;

    /* I wish I could explain what is happening here, but I don't even fully
     * understand it myself. Pipewire's surreal and incomprehensible nature
     * simply cannot be put into words. */
    const struct param_route *active_candidate = NULL;
    for (unsigned i = 0; i < len; i++) {
        const struct param_route *route = &routes[i];

        const bool skip = route->direction != media_class_to_direction(node->media_class)
                       || !lfind(&node->card_profile_device, route->devices,
                                 &(size_t){route->n_devices}, sizeof(int32_t), int32_cmp);
        if (skip) {
            continue;
        }

        if (route->active && !active_candidate) {
            active_candidate = route;
        }

        const bool has_profile = lfind(&node->device_profile, route->profiles,
                                       &(size_t){route->n_profiles}, sizeof(int32_t), int32_cmp);
        if (!has_profile) {
            continue;
        }

        DEBUG("node %d route: idx=%d dev=%d dir=%d act=%d",
              node->id, route->index, route->device, route->direction, route->active);

        struct param_route *new_route = &node->routes[node->n_routes++];
        *new_route = (struct param_route){
            .index = route->index,
            .device = route->device,
            .direction = route->direction,
            .name = xstrdup(route->name),
            .description = xstrdup(route->description),
            .active = route->active,
        };
    }
    if (active_candidate) {
        const struct param_props *props = &active_candidate->props;
        process_param_props(node, props->mute, props->n_channels,
                            props->channel_names, props->channel_volumes);
    }

    emit_routes(node, NULL);
    node->has_routes = true;
}

static void on_device_profiles(struct device *dev,
                               const struct param_profile profiles[], unsigned profiles_count,
                               void *data) {
    struct node *node = data;

    node->device_profile = -1;
    for (unsigned i = 0; i < profiles_count; i++) {
        const struct param_profile *profile = &profiles[i];
        if (profile->active) {
            node->device_profile = profile->index;
            return;
        }
    }

    ERROR("didn't find an active profile on device %u for node %u", dev->id, node->id);
}

static const struct device_events device_events = {
    .routes = on_device_routes,
    .profiles = on_device_profiles,
};

static void on_node_info(void *data, const struct pw_node_info *info) {
    struct node *node = data;

    DEBUG("node %d info: state=%d n_params=%d change=0x%lx",
          info->id, info->state, info->n_params, info->change_mask);

    if (info->change_mask & PW_NODE_CHANGE_MASK_PROPS) {
        const bool first_props = !node->has_props;
        const struct spa_dict *props = info->props;
        for (unsigned i = 0; i < props->n_items; i++) {
            const struct spa_dict_item *item = &props->items[i];
            const char *k = item->key;
            const char *v = item->value;

            if (streq(k, "media.name")) {
                free(node->props.media_name);
                node->props.media_name = xstrdup(v);
            } else if (streq(k, "node.name")) {
                free(node->props.node_name);
                node->props.node_name = xstrdup(v);

                /* now we can start checking for default */
                if (node->media_class == AUDIO_SINK || node->media_class == AUDIO_SOURCE) {
                    node->default_hook = pipewire_add_listener(&pipewire_events, node);
                }
            } else if (streq(k, "node.description")) {
                free(node->props.node_description);
                node->props.node_description = xstrdup(v);
            } else if (streq(k, "card.profile.device")) {
                str_to_i32(v, &node->card_profile_device);
            } else if (streq(k, "device.id") && !node->device) {
                str_to_u32(v, &node->device_id);
            }
        }

        if (first_props && !node->device_id) {
            pw_node_subscribe_params(node->pw_node, (uint32_t[]){SPA_PARAM_Props}, 1);
        } else if (node->device_id && !node->device) {
            struct device *dev = device_lookup(node->device_id);
            if (!dev) {
                WARN("got device.id=%d on node %d but no device with this id was found",
                     node->device_id, node->id);
            } else {
                node->device = device_ref(dev);
                node->device_hook = device_add_listener(node->device, &device_events, node);
            }
        }

        emit_props(node, NULL);
        node->has_props = true;
    }
}

static void on_node_param(void *data, int seq, uint32_t id, uint32_t index,
                          uint32_t next, const struct spa_pod *param) {
    struct node *node = data;

    DEBUG("node %d param: id=%d seq=%d index=%d next=%d", node->id, id, seq, index, next);

    bool mute;

    uint32_t map_csize, map_ctype, map_nvals;
    const pw_id_t *map_vals;

    uint32_t vol_csize, vol_ctype, vol_nvals;
    const float *vol_vals;

    struct spa_pod_parser p;
    spa_pod_parser_pod(&p, param);
    const int n =
        spa_pod_parser_get_object(&p,
                                  SPA_TYPE_OBJECT_Props, &(pw_id_t){},
                                  SPA_PROP_mute, SPA_POD_Bool(&mute),
                                  SPA_PROP_channelMap, SPA_POD_Array(&map_csize, &map_ctype,
                                                                     &map_nvals, &map_vals),
                                  SPA_PROP_channelVolumes, SPA_POD_Array(&vol_csize, &vol_ctype,
                                                                         &vol_nvals, &vol_vals));
    if (n != 3) {
        ERROR("failed to parse node Props");
        return;
    } else if (vol_ctype != SPA_TYPE_Float || map_ctype != SPA_TYPE_Id) {
        ERROR("unexpected array member type in pod");
        return;
    } else if (map_nvals != vol_nvals) {
        ERROR("channelMap size != channelVolumes size (wtf)");
        return;
    }

    static const char *names[SPA_AUDIO_MAX_CHANNELS];
    static float volumes[SPA_AUDIO_MAX_CHANNELS];
    for (unsigned i = 0; i < map_nvals; i++) {
        const enum spa_audio_channel chan = map_vals[i];
        const float volume = vol_vals[i];

        names[i] = channel_name_from_enum(chan);
        volumes[i] = cbrtf(volume);
    }

    process_param_props(node, mute, map_nvals, names, volumes);
}

static const struct pw_node_events node_events = {
    .version = PW_VERSION_NODE_EVENTS,
    .info = on_node_info,
    .param = on_node_param,
};

static void on_proxy_removed(void *data) {
    struct node *node = data;

    emit_removed(node, NULL);
}

static const struct pw_proxy_events proxy_events = {
    .version = PW_VERSION_PROXY_EVENTS,
    .removed = on_proxy_removed,
};

struct node *node_create(struct pw_node *pw_node, uint32_t id, enum media_class media_class) {
    struct node *node = xmalloc(sizeof(*node));

    *node = (struct node){
        .id = id,
        .pw_node = pw_node,
        .media_class = media_class,
        .new = true,
        .refcnt = 1,
    };

    node->emitter = event_emitter_create(node_event_dispatcher);

    pw_node_add_listener(node->pw_node, &node->listener, &node_events, node);
    pw_proxy_add_listener(node->pw_proxy, &node->proxy_listener, &proxy_events, node);

    TRACE("node_create(%p): id=%u", (void *)node, node->id);

    return node;
}

static void node_destroy(struct node *node) {
    TRACE("node_destroy(%p)", (void *)node);

    pw_proxy_destroy(node->pw_proxy);

    free(node->props.media_name);
    free(node->props.node_name);
    free(node->props.node_description);
    param_props_free_contents(&node->param_props);

    for (unsigned i = 0; i < node->n_routes; i++) {
        struct param_route *route = &node->routes[i];
        param_route_free_contents(route);
    }
    free(node->routes);

    event_hook_release(node->default_hook);

    event_emitter_release(node->emitter);

    if (node->device) {
        device_unref(&node->device);
        event_hook_release(node->device_hook);
    }

    free(node);
}

struct node *node_ref(struct node *node) {
    ASSERT(node->refcnt++ > 0);

    TRACE("node_ref(%p): %u -> %u", (void *)node, node->refcnt - 1, node->refcnt);

    return node;
}

void node_unref(struct node **pnode) {
    struct node *node = *pnode;
    ASSERT(node->refcnt > 0);

    TRACE("node_unref(%p): %u -> %u", (void *)node, node->refcnt, node->refcnt - 1);

    if (--node->refcnt == 0) {
        node_destroy(node);
    }
    *pnode = NULL;
}

