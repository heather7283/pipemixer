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

static void node_event_dispatcher(uint64_t id, union event_data data, struct event_hook *hook) {
    const struct node_events *table = hook->callbacks;
    struct node *node = hook->private_data;

    switch ((enum node_event_types)id) {
    case NODE_EVENT_REMOVED:
        EVENT_DISPATCH(table->removed, node, hook->callbacks_data);
        break;
    case NODE_EVENT_ROUTES:
        EVENT_DISPATCH(table->routes, node, node->routes, node->n_routes, hook->callbacks_data);
        break;
    case NODE_EVENT_PROPS:
        EVENT_DISPATCH(table->props, node, &node->props, hook->callbacks_data);
        break;
    case NODE_EVENT_CHANNELS:
        EVENT_DISPATCH(table->channels, node,
                       node->channel_names, node->n_channels,
                       hook->callbacks_data);
        break;
    case NODE_EVENT_VOLUME:
        EVENT_DISPATCH(table->volume, node,
                       node->channel_volumes, node->n_channels,
                       hook->callbacks_data);
        break;
    case NODE_EVENT_MUTE:
        EVENT_DISPATCH(table->mute, node, node->mute, hook->callbacks_data);
        break;
    case NODE_EVENT_DEFAULT:
        EVENT_DISPATCH(table->default_, node, node->is_default, hook->callbacks_data);
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

static void emit_everything(struct node *node, struct event_hook *hook) {
    static void (*const funcs[])(struct node *, struct event_hook *) = {
        emit_props, emit_channels, emit_volume, emit_mute, emit_routes, emit_default,
    };
    for (unsigned i = 0; i < SIZEOF_ARRAY(funcs); i++) {
        funcs[i](node, hook);
    }
}

static void hook_remove(struct event_hook *hook) {
    node_unref((struct node **)&hook->private_data);
}

void node_add_listener(struct node *node, struct event_hook *hook,
                       const struct node_events *ev, void *data) {
    *hook = (struct event_hook){
        .callbacks = ev,
        .callbacks_data = data,
        .private_data = node_ref(node),
        .remove = hook_remove,
    };
    event_emitter_add_hook(node->emitter, hook);

    if (!node->new) {
        emit_everything(node, hook);
    }
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
    /* TODO: check media_class? */
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

    float cubed_volumes[node->n_channels];
    for (uint32_t i = 0; i < node->n_channels; i++) {
        float new_volume;
        const float old_volume = node->channel_volumes[i];

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
    if (is_default != node->is_default) {
        node->is_default = is_default;
        INFO("node %d default: %d", node->id, node->is_default);
        emit_default(node, NULL);
    }
}

static const struct pipewire_events pipewire_events = {
    .default_ = on_default,
};

static int device_cmp(const void *a, const void *b) {
    return (*(int32_t *)a != *(int32_t *)b);
}

static void on_device_routes(struct device *dev,
                             const struct route *routes, unsigned len, void *data) {
    struct node *node = data;

    for (unsigned i = 0; i < node->n_routes; i++) {
        struct route *route = &node->routes[i];
        route_free_contents(route);
    }

    node->routes = xreallocarray(node->routes, len, sizeof(node->routes[0]));
    node->n_routes = 0;

    for (unsigned i = 0; i < len; i++) {
        const struct route *route = &routes[i];

        const bool skip = route->direction != media_class_to_direction(node->media_class)
                       || !lfind(&node->card_profile_device,
                                 route->devices, &(size_t){route->n_devices}, sizeof(int32_t),
                                 device_cmp);
        if (skip) {
            continue;
        }

        DEBUG("node %d route: idx=%d dev=%d dir=%d act=%d",
              node->id, route->index, route->device, route->direction, route->active);

        node->routes[node->n_routes++] = (struct route){
            .index = route->index,
            .device = route->device,
            .direction = route->direction,
            .name = xstrdup(route->name),
            .description = xstrdup(route->description),
            .active = route->active,
        };
    }
}

static const struct device_events device_events = {
    .routes = on_device_routes,
};

void on_node_info(void *data, const struct pw_node_info *info) {
    struct node *node = data;

    DEBUG("node %d info: state=%d n_params=%d change=0x%lx",
          info->id, info->state, info->n_params, info->change_mask);

    if (info->change_mask & PW_NODE_CHANGE_MASK_PROPS) {
        const struct spa_dict *props = info->props;
        for (unsigned i = 0; i < props->n_items; i++) {
            const struct spa_dict_item *item = &props->items[i];
            const char *k = item->key;
            const char *v = item->value;

            bool changed = false;
            if (streq(k, "media.name")) {
                free(node->props.media_name);
                node->props.media_name = xstrdup(v);
                changed = true;
            } else if (streq(k, "node.name")) {
                free(node->props.node_name);
                node->props.node_name = xstrdup(v);
                changed = true;
            } else if (streq(k, "node.description")) {
                free(node->props.node_description);
                node->props.node_description = xstrdup(v);
                changed = true;
            } else if (streq(k, "card.profile.device")) {
                str_to_i32(v, &node->card_profile_device);
            } else if (streq(k, "device.id") && !node->device_id) {
                str_to_u32(v, &node->device_id);
                struct device *dev = device_lookup(node->device_id);
                if (!dev) {
                    WARN("got device.id=%d on node %d but no device with this id was found",
                         node->device_id, node->id);
                } else {
                    node->device = device_ref(dev);
                    device_add_listener(node->device, &node->device_hook, &device_events, node);
                }
            }

            if (changed && !node->new) {
                emit_props(node, NULL);
            }
        }
    }
}

void on_node_param(void *data, int seq, uint32_t id, uint32_t index,
                   uint32_t next, const struct spa_pod *param) {
    struct node *node = data;

    DEBUG("node %d param: id=%d seq=%d index=%d next=%d", node->id, id, seq, index, next);

    struct spa_pod_parser p;
    spa_pod_parser_pod(&p, param);

    bool mute;

    uint32_t map_csize, map_ctype, map_nvals;
    const pw_id_t *map_vals;

    uint32_t vol_csize, vol_ctype, vol_nvals;
    const float *vol_vals;

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

    if (node->n_channels != map_nvals) {
        node->channel_names =
            xreallocarray(node->channel_names, map_nvals, sizeof(node->channel_names[0]));
        node->channel_volumes =
            xreallocarray(node->channel_volumes, map_nvals, sizeof(node->channel_volumes[0]));
        node->n_channels = map_nvals;

        if (!node->new) {
            emit_channels(node, NULL);
        }
    }

    for (unsigned i = 0; i < map_nvals; i++) {
        const enum spa_audio_channel chan = map_vals[i];
        const float volume = vol_vals[i];

        node->channel_names[i] = channel_name_from_enum(chan);
        node->channel_volumes[i] = cbrtf(volume);
    }

    if (!node->new) {
        emit_volume(node, NULL);
    }

    if (mute != node->mute) {
        INFO("node %u mute %d", node->id, mute);
        node->mute = mute;

        if (!node->new) {
            emit_mute(node, NULL);
        }
    }
}

static const struct pw_node_events node_events = {
    .version = PW_VERSION_NODE_EVENTS,
    .info = on_node_info,
    .param = on_node_param,
};

static void on_proxy_roundtrip_done(void *data, int _) {
    struct node *node = data;

    if (node->new) {
        node->new = false;

        /* add this only after we (hopefully) got node.name from the server */
        if (node->media_class == AUDIO_SINK || node->media_class == AUDIO_SOURCE) {
            pipewire_add_listener(&node->default_listener, &pipewire_events, node);
        }

        emit_everything(node, NULL);
    } else {
        WARN("got roundrip_done on but node.new=false");
    }
}

static void on_proxy_removed(void *data) {
    struct node *node = data;

    emit_removed(node, NULL);
}

static const struct pw_proxy_events proxy_events = {
    .version = PW_VERSION_PROXY_EVENTS,
    .done = on_proxy_roundtrip_done,
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
    /* TODO: make sure this sends out an initial event immediately */
    pw_node_subscribe_params(node->pw_node, (uint32_t[]){SPA_PARAM_Props}, 1);
    pw_proxy_add_listener(node->pw_proxy, &node->proxy_listener, &proxy_events, node);
    pw_proxy_sync(node->pw_proxy, 0xB00B1E5);

    TRACE("node_create(%p): id=%u", (void *)node, node->id);

    return node;
}

static void node_destroy(struct node *node) {
    TRACE("node_destroy(%p)", (void *)node);

    pw_proxy_destroy(node->pw_proxy);

    free(node->props.media_name);
    free(node->props.node_name);
    free(node->props.node_description);
    free(node->channel_volumes);
    free(node->channel_names);
    for (unsigned i = 0; i < node->n_routes; i++) {
        struct route *route = &node->routes[i];
        route_free_contents(route);
    }

    event_hook_remove(&node->default_listener);

    event_emitter_release(node->emitter);

    if (node->device) {
        device_unref(&node->device);
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

