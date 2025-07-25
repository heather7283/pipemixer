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
    struct route *route = NULL;
    ARRAY_FOREACH(&dev->active_routes[dev->active_routes_index], i) {
        route = &ARRAY_AT(&dev->active_routes[dev->active_routes_index], i);
        if (route->direction == direction && route->device == card_profile_device) {
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

static void profile_free(struct profile *profile) {
    string_free(&profile->name);
    string_free(&profile->description);
}

static void route_free(struct route *route) {
    string_free(&route->description);
    string_free(&route->name);
    ARRAY_FREE(&route->devices);
    ARRAY_FREE(&route->profiles);
}

void device_free(struct device *device) {
    pw_proxy_destroy((struct pw_proxy *)device->pw_device);

    for (int j = 0; j < 2; j++) {
        ARRAY_FOREACH(&device->all_routes[j], i) {
            struct route *route = &ARRAY_AT(&device->all_routes[j], i);
            route_free(route);
        }
        ARRAY_FREE(&device->all_routes[j]);

        ARRAY_FOREACH(&device->active_routes[j], i) {
            struct route *route = &ARRAY_AT(&device->active_routes[j], i);
            route_free(route);
        }
        ARRAY_FREE(&device->active_routes[j]);

        ARRAY_FOREACH(&device->profiles[j], i) {
            struct profile *profile = &ARRAY_AT(&device->profiles[j], i);
            profile_free(profile);
        }
        ARRAY_FREE(&device->profiles[j]);
    }
    if (device->active_profile != NULL) {
        profile_free(device->active_profile);
        free(device->active_profile);
    }

    free(device);
}

void on_device_roundtrip_done(void *data) {
    struct device *dev = data;

    if (dev->modified_params & ROUTE) {
        ARRAY_FOREACH(&dev->active_routes[dev->active_routes_index], i) {
            struct route *route = &ARRAY_AT(&dev->active_routes[dev->active_routes_index], i);
            route_free(route);
        }
        ARRAY_CLEAR(&dev->active_routes[dev->active_routes_index]);

        /* swap */
        dev->active_routes_index = !dev->active_routes_index;

        dev->modified_params &= ~ROUTE;
    }
    if (dev->modified_params & ENUM_ROUTE) {
        ARRAY_FOREACH(&dev->all_routes[dev->all_routes_index], i) {
            struct route *route = &ARRAY_AT(&dev->all_routes[dev->all_routes_index], i);
            route_free(route);
        }
        ARRAY_CLEAR(&dev->all_routes[dev->all_routes_index]);

        /* swap */
        dev->all_routes_index = !dev->all_routes_index;

        dev->modified_params &= ~ENUM_ROUTE;
    }
    if (dev->modified_params & ENUM_PROFILE) {
        ARRAY_FOREACH(&dev->profiles[dev->profiles_index], i) {
            struct profile *profile = &ARRAY_AT(&dev->profiles[dev->profiles_index], i);
            profile_free(profile);
        }

        /* swap */
        dev->profiles_index = !dev->profiles_index;

        dev->modified_params &= ~ENUM_PROFILE;
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

    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) {
        for (i = 0; i < info->n_params; i++) {
            struct spa_param_info *param = &info->params[i];
            if (param->id == SPA_PARAM_Route && param->flags & SPA_PARAM_INFO_READ) {
                pw_device_enum_params(device->pw_device, 0, param->id, 0, -1, NULL);
                device->modified_params |= ROUTE;
            } else if (param->id == SPA_PARAM_EnumRoute && param->flags & SPA_PARAM_INFO_READ) {
                pw_device_enum_params(device->pw_device, 0, param->id, 0, -1, NULL);
                device->modified_params |= ENUM_ROUTE;
            } else if (param->id == SPA_PARAM_Profile && param->flags & SPA_PARAM_INFO_READ) {
                pw_device_enum_params(device->pw_device, 0, param->id, 0, -1, NULL);
                if (device->active_profile != NULL) {
                    profile_free(device->active_profile);
                    free(device->active_profile);
                    device->active_profile = NULL;
                }
                device->modified_params |= PROFILE;
            } else if (param->id == SPA_PARAM_EnumProfile && param->flags & SPA_PARAM_INFO_READ) {
                pw_device_enum_params(device->pw_device, 0, param->id, 0, -1, NULL);
                device->modified_params |= ENUM_PROFILE;
            }
        }
    }
    if (device->modified_params) {
        roundtrip_async(pw.core, on_device_roundtrip_done, device);
    }
}

static void on_device_param_route(struct device *dev, const struct spa_pod *param) {
    const struct spa_pod_prop *name =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_name);
    const struct spa_pod_prop *index =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_index);
    const struct spa_pod_prop *device =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_device);
    const struct spa_pod_prop *direction =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_direction);
    const struct spa_pod_prop *description =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_description);
    if (index == NULL || device == NULL || direction == NULL
        || description == NULL || name == NULL) {
        WARN("Didn't find all required fields in Route");
        return;
    }

    struct route *new_route = ARRAY_EMPLACE_ZEROED(&dev->active_routes[!dev->active_routes_index]);

    spa_pod_get_int(&index->value, &new_route->index);
    spa_pod_get_int(&device->value, &new_route->device);
    spa_pod_get_id(&direction->value, &new_route->direction);

    const char *description_str = NULL;
    spa_pod_get_string(&description->value, &description_str);
    string_from_pchar(&new_route->description, description_str);

    const char *name_str = NULL;
    spa_pod_get_string(&name->value, &name_str);
    string_from_pchar(&new_route->name, name_str);

    DEBUG("New route (Route) on dev %d: %s device %d index %d dir %d",
          dev->id, new_route->description.data, new_route->device,
          new_route->index, new_route->direction);
}

static void on_device_param_enum_route(struct device *dev, const struct spa_pod *param) {
    const struct spa_pod_prop *name =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_name);
    const struct spa_pod_prop *index =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_index);
    const struct spa_pod_prop *devices =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_devices);
    const struct spa_pod_prop *profiles =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_profiles);
    const struct spa_pod_prop *direction =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_direction);
    const struct spa_pod_prop *description =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_description);
    if (index == NULL || direction == NULL || description == NULL
        || profiles == NULL || devices == NULL || name == NULL) {
        WARN("Didn't find all required fields in Route");
        return;
    }

    struct route *new_route = ARRAY_EMPLACE_ZEROED(&dev->all_routes[!dev->all_routes_index]);

    spa_pod_get_int(&index->value, &new_route->index);
    spa_pod_get_id(&direction->value, &new_route->direction);

    const char *description_str = NULL;
    spa_pod_get_string(&description->value, &description_str);
    string_from_pchar(&new_route->description, description_str);

    const char *name_str = NULL;
    spa_pod_get_string(&name->value, &name_str);
    string_from_pchar(&new_route->name, name_str);

    struct spa_pod *iter;
    SPA_POD_ARRAY_FOREACH((const struct spa_pod_array *)&devices->value, iter) {
        ARRAY_APPEND(&new_route->devices, (int32_t *)iter);
    }
    SPA_POD_ARRAY_FOREACH((const struct spa_pod_array *)&profiles->value, iter) {
        ARRAY_APPEND(&new_route->profiles, (int32_t *)iter);
    }

    DEBUG("New route (EnumRoute) on dev %d: %s index %d dir %d",
          dev->id, new_route->description.data, new_route->index, new_route->direction);
}

static void on_device_param_enum_profile(struct device *dev, const struct spa_pod *param) {
    const struct spa_pod_prop *index =
        spa_pod_find_prop(param, NULL, SPA_PARAM_PROFILE_index);
    const struct spa_pod_prop *description =
        spa_pod_find_prop(param, NULL, SPA_PARAM_PROFILE_description);
    const struct spa_pod_prop *name =
        spa_pod_find_prop(param, NULL, SPA_PARAM_PROFILE_name);
    if (index == NULL || description == NULL || name == NULL) {
        WARN("Didn't find index or name or description in Profile");
        return;
    }

    struct profile *new_profile = ARRAY_EMPLACE_ZEROED(&dev->profiles[!dev->profiles_index]);

    spa_pod_get_int(&index->value, &new_profile->index);

    const char *description_str = NULL;
    spa_pod_get_string(&description->value, &description_str);
    string_from_pchar(&new_profile->description, description_str);

    const char *name_str = NULL;
    spa_pod_get_string(&name->value, &name_str);
    string_from_pchar(&new_profile->name, name_str);

    DEBUG("New profile (EnumProfile) on dev %d: %s (%s) index %d",
          dev->id, new_profile->description.data, new_profile->name.data, new_profile->index);
}

static void on_device_param_profile(struct device *dev, const struct spa_pod *param) {
    if (dev->active_profile != NULL) {
        ERROR("Got Profile for dev %d, but active profile is already set to %d (%s), "
              "PLEASE REPORT THIS AS A BUG!!!",
              dev->id, dev->active_profile->index, dev->active_profile->name.data);
        return;
    }

    const struct spa_pod_prop *index =
        spa_pod_find_prop(param, NULL, SPA_PARAM_PROFILE_index);
    const struct spa_pod_prop *description =
        spa_pod_find_prop(param, NULL, SPA_PARAM_PROFILE_description);
    const struct spa_pod_prop *name =
        spa_pod_find_prop(param, NULL, SPA_PARAM_PROFILE_name);
    if (index == NULL || description == NULL || name == NULL) {
        WARN("Didn't find index or name or description in Profile");
        return;
    }

    struct profile *new_profile = xcalloc(1, sizeof(*new_profile));

    spa_pod_get_int(&index->value, &new_profile->index);

    const char *description_str = NULL;
    spa_pod_get_string(&description->value, &description_str);
    string_from_pchar(&new_profile->description, description_str);

    const char *name_str = NULL;
    spa_pod_get_string(&name->value, &name_str);
    string_from_pchar(&new_profile->name, name_str);

    dev->active_profile = new_profile;
    DEBUG("New profile (Profile) on dev %d: %s (%s) index %d",
          dev->id, new_profile->description.data, new_profile->name.data, new_profile->index);
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
    case SPA_PARAM_Profile:
        on_device_param_profile(device, param);
        break;
    case SPA_PARAM_EnumProfile:
        on_device_param_enum_profile(device, param);
        break;
    }
}

