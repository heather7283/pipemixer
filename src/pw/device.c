#include <spa/pod/builder.h>
#include <spa/param/props.h>

#include "pw/device.h"
#include "pw/roundtrip.h"
#include "tui.h"
#include "log.h"
#include "xmalloc.h"
#include "macros.h"

void device_set_props(const struct device *dev, const struct spa_pod *props,
                      enum spa_direction direction, int32_t card_profile_device) {
    bool found = false;
    struct route *route;
    LIST_FOR_EACH(route, &dev->routes[direction].active, link) {
        if (route->device == card_profile_device) {
            found = true;
            break;
        }
    }
    if (!found) {
        ERROR("could not set props on dev %d: route with device %d was not found",
              dev->id, card_profile_device);
        return;
    }

    uint8_t buffer[4096];
    struct spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));
    struct spa_pod* param =
        spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_ParamRoute, SPA_PARAM_Route,
                                   SPA_PARAM_ROUTE_device, SPA_POD_Int(route->device),
                                   SPA_PARAM_ROUTE_index, SPA_POD_Int(route->index),
                                   SPA_PARAM_ROUTE_props, SPA_POD_PodObject(props),
                                   SPA_PARAM_ROUTE_save, SPA_POD_Bool(true));

    pw_device_set_param(dev->pw_device, SPA_PARAM_Route, 0, param);
}

void device_set_route(const struct device *dev, int32_t card_profile_device, int32_t index) {
    uint8_t buffer[1024];
    struct spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    struct spa_pod *route =
        spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_ParamRoute, SPA_PARAM_Route,
                                   SPA_PARAM_ROUTE_device, SPA_POD_Int(card_profile_device),
                                   SPA_PARAM_ROUTE_index, SPA_POD_Int(index),
                                   SPA_PARAM_ROUTE_save, SPA_POD_Bool(true));

    pw_device_set_param(dev->pw_device, SPA_PARAM_Route, 0, route);
}

static void device_routes_free(const LIST_HEAD *list) {
    struct route *route;
    LIST_FOR_EACH(route, list, link) {
        LIST_REMOVE(&route->link);
        string_free(&route->description);
        free(route);
    }
}

void device_free(struct device *device) {
    pw_proxy_destroy((struct pw_proxy *)device->pw_device);

    for (unsigned int i = 0; i < SIZEOF_ARRAY(device->routes); i++) {
        device_routes_free(&device->routes[i].all);
        device_routes_free(&device->routes[i].active);
    }

    free(device);
}

void on_device_roundtrip_done(void *data) {
    struct device *dev = data;

    for (unsigned int i = 0; i < SIZEOF_ARRAY(dev->routes); i++) {
        device_routes_free(&dev->routes[i].all);
        device_routes_free(&dev->routes[i].active);

        LIST_SWAP_HEADS(&dev->routes[i].active, &dev->new_routes[i].active);
        LIST_SWAP_HEADS(&dev->routes[i].all, &dev->new_routes[i].all);
    }

    tui_notify_device_change(dev);
}

void on_device_info(void *data, const struct pw_device_info *info) {
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

    bool needs_roundtrip = false;
    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) {
        for (i = 0; i < info->n_params; i++) {
            struct spa_param_info *param = &info->params[i];
            if (param->id == SPA_PARAM_Route && param->flags & SPA_PARAM_INFO_READ) {
                pw_device_enum_params(device->pw_device, 0, param->id, 0, -1, NULL);
                needs_roundtrip = true;
            } else if (param->id == SPA_PARAM_EnumRoute && param->flags & SPA_PARAM_INFO_READ) {
                pw_device_enum_params(device->pw_device, 0, param->id, 0, -1, NULL);
                needs_roundtrip = true;
            }
        }
    }
    if (needs_roundtrip) {
        roundtrip_async(pw.core, on_device_roundtrip_done, device);
    }
}

static void on_device_param_route(struct device *dev, const struct spa_pod *param) {
    const struct spa_pod_prop *index = spa_pod_find_prop(param,
                                                         NULL, SPA_PARAM_ROUTE_index);
    const struct spa_pod_prop *device = spa_pod_find_prop(param,
                                                          NULL, SPA_PARAM_ROUTE_device);
    const struct spa_pod_prop *direction = spa_pod_find_prop(param,
                                                             NULL, SPA_PARAM_ROUTE_direction);
    const struct spa_pod_prop *description = spa_pod_find_prop(param,
                                                               NULL, SPA_PARAM_ROUTE_description);
    if (index == NULL || device == NULL || direction == NULL || description == NULL) {
        WARN("Didn't find index or device or direction or description in Route");
        return;
    }

    struct route *new_route = xcalloc(1, sizeof(*new_route));

    spa_pod_get_int(&index->value, &new_route->index);
    spa_pod_get_int(&device->value, &new_route->device);
    spa_pod_get_id(&direction->value, &new_route->direction);

    const char *description_str = NULL;
    spa_pod_get_string(&description->value, &description_str);
    string_from_pchar(&new_route->description, description_str);

    DEBUG("New route (Route) on dev %d: %s device %d index %d dir %d",
          dev->id, new_route->description.data, new_route->device,
          new_route->index, new_route->direction);

    LIST_INSERT(&dev->new_routes[new_route->direction].active, &new_route->link);
}

static void on_device_param_enum_route(struct device *dev, const struct spa_pod *param) {
    const struct spa_pod_prop *index = spa_pod_find_prop(param,
                                                         NULL, SPA_PARAM_ROUTE_index);
    const struct spa_pod_prop *direction = spa_pod_find_prop(param,
                                                             NULL, SPA_PARAM_ROUTE_direction);
    const struct spa_pod_prop *description = spa_pod_find_prop(param,
                                                               NULL, SPA_PARAM_ROUTE_description);
    if (index == NULL || direction == NULL || description == NULL) {
        WARN("Didn't find index or direction or description in Route (EnumRoute)");
        return;
    }

    struct route *new_route = xcalloc(1, sizeof(*new_route));

    spa_pod_get_int(&index->value, &new_route->index);
    spa_pod_get_id(&direction->value, &new_route->direction);

    const char *description_str = NULL;
    spa_pod_get_string(&description->value, &description_str);
    string_from_pchar(&new_route->description, description_str);

    DEBUG("New route (EnumRoute) on dev %d: %s index %d dir %d",
          dev->id, new_route->description.data, new_route->index, new_route->direction);

    LIST_INSERT(&dev->new_routes[new_route->direction].all, &new_route->link);
}

void on_device_param(void *data, int seq, uint32_t id, uint32_t index,
                     uint32_t next, const struct spa_pod *param) {
    struct device *device = data;

    DEBUG("device %d param: id %d seq %d index %d next %d", device->id, id, seq, index, next);

    switch (id) {
    case SPA_PARAM_Route:
        on_device_param_route(device, param);
        break;
    case SPA_PARAM_EnumRoute:
        on_device_param_enum_route(device, param);
        break;
    }
}

