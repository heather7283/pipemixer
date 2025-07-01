#include <spa/pod/builder.h>
#include <spa/param/props.h>

#include "pw/device.h"
#include "log.h"
#include "xmalloc.h"
#include "utils.h"
#include "macros.h"

static void device_routes_free(struct device *device) {
    struct route *route, *route_tmp;
    spa_list_for_each_safe(route, route_tmp, &device->routes, link) {
        spa_list_remove(&route->link);
        free(route);
    }
}

void device_free(struct device *device) {
    pw_proxy_destroy((struct pw_proxy *)device->pw_device);

    device_routes_free(device);

    free(device);
}

static void on_device_info(void *data, const struct pw_device_info *info) {
    struct device *device = data;

    DEBUG("device info: id %d, %d params%s,%s change "
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
                device_routes_free(device);
                pw_device_enum_params(device->pw_device, 0, param->id, 0, -1, NULL);
            }
        }
    }
}

static void on_device_param(void *data, int seq, uint32_t id, uint32_t index,
                            uint32_t next, const struct spa_pod *param) {
    struct device *device = data;

    DEBUG("device %d param: id %d seq %d index %d next %d param %p",
          device->id, id, seq, index, next, (void *)param);

    const struct spa_pod_prop *index_prop = spa_pod_find_prop(param, NULL,
                                                              SPA_PARAM_ROUTE_index);
    const struct spa_pod_prop *device_prop = spa_pod_find_prop(param, NULL,
                                                               SPA_PARAM_ROUTE_device);
    if (index_prop == NULL || device_prop == NULL) {
        WARN("didn't find index and device in route object");
        return;
    }

    /* don't even ask, this is pipewire */
    const struct spa_pod_prop *prop_ = spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_props);
    if (prop_ == NULL) {
        WARN("didn't find props in route object");
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
        WARN("didn't find volumes, channels or mute in route object's props");
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

const struct pw_device_events device_events = {
    .version = PW_VERSION_DEVICE_EVENTS,
    .info = on_device_info,
    .param = on_device_param,
};

