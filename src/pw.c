#include <math.h>
#include <spa/pod/builder.h>
#include <spa/param/props.h>

#include "pw.h"
#include "macros.h"
#include "utils.h"
#include "log.h"
#include "xmalloc.h"
#include "config.h"
#include "thirdparty/stb_ds.h"

struct pw pw = {0};

void node_set_mute(struct node *node, bool mute) {
    /*
     * I can't just set mute on a Node that has a Device associated with it.
     * I need to find the Route property of a Device that has the same value of
     * device field as card.profile.device field of the Node. Then, I take
     * device and index fields of the Route and use them in set_param request.
     */
    uint8_t buffer[1024];
    struct spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    struct spa_pod *props;
    props = spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_Props,
                                       SPA_PARAM_Props, SPA_PROP_mute,
                                       SPA_POD_Bool(mute));

    if (!node->has_device) {
        pw_node_set_param(node->pw_node, SPA_PARAM_Props, 0, props);
    } else {
        struct device *device = stbds_hmget(pw.devices, node->device_id);
        if (device == NULL) {
            warn("tried to change mute state of node %d with associated device, "
                 "but no device with id %d was found", node->id, node->device_id);
            return;
        }

        bool found = false;
        struct route *route;
        spa_list_for_each(route, &device->routes, link) {
            if (route->device == node->card_profile_device) {
                found = true;
                break;
            }
        }
        if (!found) {
            warn("route with device %d was not found", node->card_profile_device);
            return;
        }

        struct spa_pod* param =
            spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_ParamRoute, SPA_PARAM_Route,
                                       SPA_PARAM_ROUTE_device, SPA_POD_Int(route->device),
                                       SPA_PARAM_ROUTE_index, SPA_POD_Int(route->index),
                                       SPA_PARAM_ROUTE_props, SPA_POD_PodObject(props),
                                       SPA_PARAM_ROUTE_save, SPA_POD_Bool(true));

        pw_device_set_param(device->pw_device, SPA_PARAM_Route, 0, param);
    }
}

void node_change_volume(struct node *node, bool absolute, float volume, uint32_t channel) {
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

    if (!node->has_device) {
        struct spa_pod *pod;
        pod = spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_Props,
                                         SPA_PARAM_Props, SPA_PROP_channelVolumes,
                                         SPA_POD_Array(sizeof(float), SPA_TYPE_Float,
                                         ARRAY_SIZE(cubed_volumes), cubed_volumes));

        pw_node_set_param(node->pw_node, SPA_PARAM_Props, 0, pod);
    } else {
        struct device *device = stbds_hmget(pw.devices, node->device_id);
        if (device == NULL) {
            warn("tried to change volume of node %d with associated device, "
                 "but no device with id %d was found", node->id, node->device_id);
            return;
        }

        bool found = false;
        struct route *route;
        spa_list_for_each(route, &device->routes, link) {
            if (route->device == node->card_profile_device) {
                found = true;
                break;
            }
        }
        if (!found) {
            warn("route with device %d was not found", node->card_profile_device);
            return;
        }

        struct spa_pod* param =
            spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_ParamRoute, SPA_PARAM_Route,
                                       SPA_PARAM_ROUTE_device, SPA_POD_Int(route->device),
                                       SPA_PARAM_ROUTE_index, SPA_POD_Int(route->index),
                                       SPA_PARAM_ROUTE_props, SPA_POD_PodObject(props),
                                       SPA_PARAM_ROUTE_save, SPA_POD_Bool(true));

        pw_device_set_param(device->pw_device, SPA_PARAM_Route, 0, param);
    }
}

static void device_routes_cleanup(struct device *device) {
    struct route *route, *route_tmp;
    spa_list_for_each_safe(route, route_tmp, &device->routes, link) {
        spa_list_remove(&route->link);
        free(route);
    }
}

static void device_cleanup(struct device *device) {
    pw_proxy_destroy((struct pw_proxy *)device->pw_device);

    device_routes_cleanup(device);

    free(device);
}

static void on_device_info(void *data, const struct pw_device_info *info) {
    struct device *device = data;

    debug("device info: id %d, %d params%s,%s change "
          BYTE_BINARY_FORMAT,
          info->id,
          info->n_params,
          info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS ? " C" : "",
          info->change_mask & PW_DEVICE_CHANGE_MASK_PROPS ? " props," : "",
          BYTE_BINARY_ARGS(info->change_mask));

    uint32_t i = 0;
    const struct spa_dict_item *item;
    spa_dict_for_each(item, info->props) {
        const char *k = item->key;
        const char *v = item->value;

        TRACE("%c---%s: %s", (++i == info->props->n_items ? '\\' : '|'), k, v);
    }

    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) {
        for (i = 0; i < info->n_params; i++) {
            struct spa_param_info *param = &info->params[i];
            if (param->id == SPA_PARAM_Route && param->flags & SPA_PARAM_INFO_READ) {
                device_routes_cleanup(device);
                pw_device_enum_params(device->pw_device, 0, param->id, 0, -1, NULL);
            }
        }
    }
}

static void on_device_param(void *data, int seq, uint32_t id, uint32_t index,
                            uint32_t next, const struct spa_pod *param) {
    struct device *device = data;

    debug("device %d param: id %d seq %d index %d next %d param %p",
          device->id, id, seq, index, next, (void *)param);

    const struct spa_pod_prop *index_prop = spa_pod_find_prop(param, NULL,
                                                              SPA_PARAM_ROUTE_index);
    const struct spa_pod_prop *device_prop = spa_pod_find_prop(param, NULL,
                                                               SPA_PARAM_ROUTE_device);
    if (index_prop == NULL || device_prop == NULL) {
        warn("didn't find index and device in route object");
        return;
    }

    /* don't even ask, this is pipewire */
    const struct spa_pod_prop *prop_ = spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_props);
    if (prop_ == NULL) {
        warn("didn't find props in route object");
        return;
    }

    const struct spa_pod *prop = &prop_->value;
    const struct spa_pod_prop *volumes_prop = spa_pod_find_prop(prop, NULL,
                                                                SPA_PROP_channelVolumes);
    const struct spa_pod_prop *channels_prop = spa_pod_find_prop(prop, NULL,
                                                                 SPA_PROP_channelMap);
    const struct spa_pod_prop *mute_prop = spa_pod_find_prop(prop, NULL,
                                                             SPA_PROP_mute);
    if (volumes_prop == NULL || channels_prop == NULL || mute_prop == NULL) {
        warn("didn't find volumes, channels or mute in route object's props");
        return;
    }

    struct route *new_route = xcalloc(1, sizeof(*new_route));

    spa_pod_get_int(&index_prop->value, &new_route->index);
    spa_pod_get_int(&device_prop->value, &new_route->device);

    struct route_props *props = &new_route->props;
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
    props->channel_count = i;
    spa_pod_get_bool(&mute_prop->value, &props->mute);

    spa_list_insert(&device->routes, &new_route->link);
}

static const struct pw_device_events device_events = {
    .version = PW_VERSION_DEVICE_EVENTS,
    .info = on_device_info,
    .param = on_device_param,
};

static void node_cleanup(struct node *node) {
    pw_proxy_destroy((struct pw_proxy *)node->pw_node);

    free(node);
}

static void on_node_info(void *data, const struct pw_node_info *info) {
    struct node *node = data;

    debug("node info: id %d, op %d/%d%s, ip %d/%d%s, state %d%s, %d params%s,%s change "
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
            size_t ret = mbsrtowcs(node->media_name, &v, ARRAY_SIZE(node->media_name), NULL);
            node->media_name[ARRAY_SIZE(node->media_name) - 1] = L'\0';
            if (ret == (size_t)-1) {
                wcsncpy(node->media_name, L"INVALID", ARRAY_SIZE(node->media_name));
            }
            node->changed = NODE_CHANGE_INFO;
        } else if (STREQ(k, PW_KEY_NODE_NAME) && WCSEMPTY(node->node_name)) {
            size_t ret = mbsrtowcs(node->node_name, &v, ARRAY_SIZE(node->node_name), NULL);
            node->node_name[ARRAY_SIZE(node->node_name) - 1] = L'\0';
            if (ret == (size_t)-1) {
                wcsncpy(node->node_name, L"INVALID", ARRAY_SIZE(node->node_name));
            }
            node->changed = NODE_CHANGE_INFO;
        } else if (STREQ(k, PW_KEY_NODE_DESCRIPTION)) {
            /* node.description is better than node.name so overwrite it */
            size_t ret = mbsrtowcs(node->node_name, &v, ARRAY_SIZE(node->node_name), NULL);
            node->node_name[ARRAY_SIZE(node->node_name) - 1] = L'\0';
            if (ret == (size_t)-1) {
                wcsncpy(node->node_name, L"INVALID", ARRAY_SIZE(node->node_name));
            }
            node->changed = NODE_CHANGE_INFO;
        } else if (STREQ(k, PW_KEY_DEVICE_ID)) {
            node->has_device = true;

            errno = 0;
            uint32_t device_id = strtoul(v, NULL, 10);
            if (errno != 0) {
                warn("failed to convert device.id %s to integer", v);
                node->device_id = 0xDEADBEEF;
            } else {
                node->device_id = device_id;
            }
        } else if (STREQ(k, "card.profile.device")) {
            errno = 0;
            uint32_t card_profile_device = strtoul(v, NULL, 10);
            if (errno != 0) {
                warn("failed to convert card.profile.device %s to integer", v);
                node->card_profile_device = 0xDEADBEEF;
            } else {
                node->card_profile_device = card_profile_device;
            }
        }
    }

    if (info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) {
        for (i = 0; i < info->n_params; i++) {
            struct spa_param_info *param = &info->params[i];
            if (param->id == SPA_PARAM_Props && param->flags & SPA_PARAM_INFO_READ) {
                pw_node_enum_params(node->pw_node, 0, param->id, 0, -1, NULL);
            }
        }
    }
}

static void on_node_param(void *data, int seq, uint32_t id, uint32_t index,
                          uint32_t next, const struct spa_pod *param) {
    struct node *node = data;

    debug("node %d param: id %d seq %d index %d next %d param %p",
          node->id, id, seq, index, next, (void *)param);

    const struct spa_pod_prop *volumes_prop = spa_pod_find_prop(param, NULL,
                                                                SPA_PROP_channelVolumes);
    const struct spa_pod_prop *channels_prop = spa_pod_find_prop(param, NULL,
                                                                 SPA_PROP_channelMap);
    const struct spa_pod_prop *mute_prop = spa_pod_find_prop(param, NULL,
                                                             SPA_PROP_mute);
    if (volumes_prop == NULL || channels_prop == NULL || mute_prop == NULL) {
        return;
    }

    struct node_props *props = &node->props;

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
        pw.node_list_changed = true;
    }
}

static const struct pw_node_events node_events = {
    .version = PW_VERSION_NODE_EVENTS,
    .info = on_node_info,
    .param = on_node_param,
};

static void on_registry_global(void *data, uint32_t id, uint32_t permissions,
                               const char *type, uint32_t version,
                               const struct spa_dict *props) {
    debug("registry global: id %d, perms "PW_PERMISSION_FORMAT", ver %d, type %s",
          id, PW_PERMISSION_ARGS(permissions), version, type);
    uint32_t i = 0;
    const struct spa_dict_item *item;
    spa_dict_for_each(item, props) {
        TRACE("%c---%s: %s", (++i == props->n_items ? '\\' : '|'), item->key, item->value);
    }

    if (STREQ(type, PW_TYPE_INTERFACE_Node)) {
        const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        enum media_class media_class_value;
        if (media_class == NULL) {
            debug("empty media.class, not binding");
            return;
        } else if (STREQ(media_class, "Audio/Source")) {
            media_class_value = AUDIO_SOURCE;
        } else if (STREQ(media_class, "Audio/Sink")) {
            media_class_value = AUDIO_SINK;
        } else if (STREQ(media_class, "Stream/Input/Audio")) {
            media_class_value = STREAM_INPUT_AUDIO;
        } else if (STREQ(media_class, "Stream/Output/Audio")) {
            media_class_value = STREAM_OUTPUT_AUDIO;
        } else {
            debug("not interested in media.class %s, not binding", media_class);
            return;
        }

        struct node *new_node = xcalloc(1, sizeof(struct node));
        new_node->id = id;
        new_node->media_class = media_class_value;
        new_node->pw_node = pw_registry_bind(pw.registry, id, type, PW_VERSION_NODE, 0);
        pw_node_add_listener(new_node->pw_node, &new_node->listener, &node_events, new_node);

        stbds_hmput(pw.nodes, new_node->id, new_node);

        pw.node_list_changed = true;
    } else if (STREQ(type, PW_TYPE_INTERFACE_Device)) {
        const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        if (media_class == NULL) {
            debug("empty media.class, not binding");
            return;
        } else if (STREQ(media_class, "Audio/Device")) {
            /* no-op */
        } else {
            debug("not interested in media.class %s, not binding", media_class);
            return;
        }

        struct device *new_device = xcalloc(1, sizeof(*new_device));
        spa_list_init(&new_device->routes);

        new_device->id = id;
        new_device->pw_device = pw_registry_bind(pw.registry, id, type, PW_VERSION_DEVICE, 0);
        pw_device_add_listener(new_device->pw_device, &new_device->listener,
                               &device_events, new_device);

        stbds_hmput(pw.devices, new_device->id, new_device);
    }
}

static void on_registry_global_remove(void *data, uint32_t id) {
    debug("registry global remove: id %d", id);

    struct node *node;
    if ((node = stbds_hmget(pw.nodes, id)) != NULL) {
        stbds_hmdel(pw.nodes, id);
        node_cleanup(node);

        pw.node_list_changed = true;
    }
    struct device *device;
    if ((device = stbds_hmget(pw.devices, id)) != NULL) {
        stbds_hmdel(pw.devices, id);
        device_cleanup(device);
    }
}

static const struct pw_registry_events registry_events = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = on_registry_global,
    .global_remove = on_registry_global_remove,
};

static void on_core_done(void *data, uint32_t id, int seq) {
    info("core done id %d seq %d", id, seq);
}

static void on_core_error(void *data, uint32_t id, int seq, int res, const char *message) {
    err("core error %d on object %d: %d (%s)", seq, id, res, message);
}

static const struct pw_core_events core_events = {
    .version = PW_VERSION_CORE_EVENTS,
    .done = on_core_done,
    .error = on_core_error,
};

int pipewire_init(void) {
    pw_init(NULL, NULL);

    pw.main_loop = pw_main_loop_new(NULL /* properties */);
    pw.main_loop_loop = pw_main_loop_get_loop(pw.main_loop);
    pw.main_loop_loop_fd = pw_loop_get_fd(pw.main_loop_loop);

    pw.context = pw_context_new(pw.main_loop_loop, NULL, 0);
    if (pw.context == NULL) {
        err("failed to create pw_context: %s", strerror(errno));
        return -1;
    }

    pw.core = pw_context_connect(pw.context, NULL, 0);
    if (pw.core == NULL) {
        err("failed to connect to pipewire: %s", strerror(errno));
        return -1;
    }
    pw_core_add_listener(pw.core, &pw.core_listener, &core_events, NULL);

    pw.registry = pw_core_get_registry(pw.core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(pw.registry, &pw.registry_listener, &registry_events, NULL);

    return 0;
}

void pipewire_cleanup(void) {
    for (int i = 0; i < stbds_hmlen(pw.nodes); i++) {
        node_cleanup(pw.nodes[i].value);
    }
    stbds_hmfree(pw.nodes);

    for (int i = 0; i < stbds_hmlen(pw.devices); i++) {
        device_cleanup(pw.devices[i].value);
    }
    stbds_hmfree(pw.devices);

    if (pw.registry != NULL) {
        pw_proxy_destroy((struct pw_proxy *)pw.registry);
    }
    if (pw.core != NULL) {
        pw_core_disconnect(pw.core);
    }
    if (pw.context != NULL) {
        pw_context_destroy(pw.context);
    }
    if (pw.main_loop != NULL) {
        pw_main_loop_destroy(pw.main_loop);
    }
    pw_deinit();
}

