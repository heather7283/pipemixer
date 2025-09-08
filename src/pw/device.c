#include <spa/pod/builder.h>
#include <spa/param/props.h>

#include "pw/device.h"
#include "pw/roundtrip.h"
#include "collections/map.h"
#include "tui.h"
#include "log.h"
#include "xmalloc.h"
#include "macros.h"

static MAP(struct device) devices = {0};

struct device *device_lookup(uint32_t id) {
    struct device *dev = MAP_GET(&devices, id);
    if (dev == NULL) {
        WARN("device with id %u was not found", id);
    }
    return dev;
}

void device_set_props(const struct device *dev, const struct spa_pod *props,
                      enum spa_direction direction, int32_t card_profile_device) {
    bool found = false;
    struct route *route = NULL;
    VEC_FOREACH(&dev->active_routes, i) {
        route = VEC_AT(&dev->active_routes, i);
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

static void profile_free_contents(struct profile *profile) {
    if (profile != NULL) {
        free(profile->name);
        free(profile->description);
    }
}

static void route_free_contents(struct route *route) {
    if (route != NULL) {
        free(route->description);
        free(route->name);
        VEC_FREE(&route->devices);
        VEC_FREE(&route->profiles);
    }
}

const struct pw_device_events device_events = {
    .version = PW_VERSION_DEVICE_EVENTS,
    .info = on_device_info,
    .param = on_device_param,
};

void device_create(uint32_t id) {
    struct device *dev = MAP_EMPLACE_ZEROED(&devices, id);
    dev->id = id;
    dev->pw_device = pw_registry_bind(pw.registry, id,
                                      PW_TYPE_INTERFACE_Device, PW_VERSION_DEVICE, 0);
    pw_device_add_listener(dev->pw_device, &dev->listener, &device_events, dev);
}

void device_destroy(struct device *device) {
    pw_proxy_destroy((struct pw_proxy *)device->pw_device);

    VEC_FOREACH(&device->routes, i) {
        struct route *route = VEC_AT(&device->routes, i);
        route_free_contents(route);
    }
    VEC_FREE(&device->routes);

    VEC_FOREACH(&device->active_routes, i) {
        struct route *route = VEC_AT(&device->active_routes, i);
        route_free_contents(route);
    }
    VEC_FREE(&device->active_routes);

    VEC_FOREACH(&device->profiles, i) {
        struct profile *profile = VEC_AT(&device->profiles, i);
        profile_free_contents(profile);
    }
    VEC_FREE(&device->profiles);

    if (device->active_profile != NULL) {
        profile_free_contents(device->active_profile);
        free(device->active_profile);
    }

    /* staging */
    VEC_FOREACH(&device->staging.routes, i) {
        struct route *route = VEC_AT(&device->staging.routes, i);
        route_free_contents(route);
    }
    VEC_FREE(&device->staging.routes);

    VEC_FOREACH(&device->staging.active_routes, i) {
        struct route *route = VEC_AT(&device->staging.active_routes, i);
        route_free_contents(route);
    }
    VEC_FREE(&device->staging.active_routes);

    VEC_FOREACH(&device->staging.profiles, i) {
        struct profile *profile = VEC_AT(&device->staging.profiles, i);
        profile_free_contents(profile);
    }
    VEC_FREE(&device->staging.profiles);

    if (device->staging.active_profile != NULL) {
        profile_free_contents(device->staging.active_profile);
        free(device->staging.active_profile);
    }

    MAP_REMOVE(&devices, device->id);
}

void on_device_roundtrip_done(void *data) {
    struct device *dev = data;

    if (dev->modified_params & ROUTE) {
        VEC_FOREACH(&dev->active_routes, i) {
            struct route *route = VEC_AT(&dev->active_routes, i);
            route_free_contents(route);
        }
        VEC_CLEAR(&dev->active_routes);

        VEC_EXCHANGE(&dev->active_routes, &dev->staging.active_routes);

        dev->modified_params &= ~ROUTE;
    }
    if (dev->modified_params & ENUM_ROUTE) {
        VEC_FOREACH(&dev->routes, i) {
            struct route *route = VEC_AT(&dev->routes, i);
            route_free_contents(route);
        }
        VEC_CLEAR(&dev->routes);

        VEC_EXCHANGE(&dev->routes, &dev->staging.routes);

        dev->modified_params &= ~ENUM_ROUTE;
    }
    if (dev->modified_params & ENUM_PROFILE) {
        VEC_FOREACH(&dev->profiles, i) {
            struct profile *profile = VEC_AT(&dev->profiles, i);
            profile_free_contents(profile);
        }
        VEC_CLEAR(&dev->profiles);

        VEC_EXCHANGE(&dev->profiles, &dev->staging.profiles);

        dev->modified_params &= ~ENUM_PROFILE;
    }
    if (dev->modified_params & PROFILE) {
        profile_free_contents(dev->active_profile);
        free(dev->active_profile);
        dev->active_profile = NULL;

        SWAP(dev->active_profile, dev->staging.active_profile);

        dev->modified_params &= ~PROFILE;
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
    const struct spa_pod_prop *profiles =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_profiles);
    const struct spa_pod_prop *direction =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_direction);
    const struct spa_pod_prop *description =
        spa_pod_find_prop(param, NULL, SPA_PARAM_ROUTE_description);
    if (index == NULL || device == NULL || direction == NULL
        || description == NULL || name == NULL) {
        WARN("Didn't find all required fields in Route");
        return;
    }

    struct route *new_route = VEC_EMPLACE_BACK_ZEROED(&dev->staging.active_routes);

    spa_pod_get_int(&index->value, &new_route->index);
    spa_pod_get_int(&device->value, &new_route->device);
    spa_pod_get_id(&direction->value, &new_route->direction);

    const char *description_str = NULL;
    spa_pod_get_string(&description->value, &description_str);
    new_route->description = xstrdup(description_str);

    const char *name_str = NULL;
    spa_pod_get_string(&name->value, &name_str);
    new_route->name = xstrdup(name_str);

    struct spa_pod *iter;
    SPA_POD_ARRAY_FOREACH((const struct spa_pod_array *)&profiles->value, iter) {
        VEC_APPEND(&new_route->profiles, (int32_t *)iter);
    }

    DEBUG("New route (Route) on dev %d: %s device %d index %d dir %d",
          dev->id, new_route->description, new_route->device,
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

    struct route *new_route = VEC_EMPLACE_BACK_ZEROED(&dev->staging.routes);

    spa_pod_get_int(&index->value, &new_route->index);
    spa_pod_get_id(&direction->value, &new_route->direction);

    const char *description_str = NULL;
    spa_pod_get_string(&description->value, &description_str);
    new_route->description = xstrdup(description_str);

    const char *name_str = NULL;
    spa_pod_get_string(&name->value, &name_str);
    new_route->name = xstrdup(name_str);

    struct spa_pod *iter;
    SPA_POD_ARRAY_FOREACH((const struct spa_pod_array *)&devices->value, iter) {
        VEC_APPEND(&new_route->devices, (int32_t *)iter);
    }
    SPA_POD_ARRAY_FOREACH((const struct spa_pod_array *)&profiles->value, iter) {
        VEC_APPEND(&new_route->profiles, (int32_t *)iter);
    }

    DEBUG("New route (EnumRoute) on dev %d: %s index %d dir %d",
          dev->id, new_route->description, new_route->index, new_route->direction);
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

    struct profile *new_profile = VEC_EMPLACE_BACK_ZEROED(&dev->staging.profiles);

    spa_pod_get_int(&index->value, &new_profile->index);

    const char *description_str = NULL;
    spa_pod_get_string(&description->value, &description_str);
    new_profile->description = xstrdup(description_str);

    const char *name_str = NULL;
    spa_pod_get_string(&name->value, &name_str);
    new_profile->name = xstrdup(name_str);

    DEBUG("New profile (EnumProfile) on dev %d: %s (%s) index %d",
          dev->id, new_profile->description, new_profile->name, new_profile->index);
}

static void on_device_param_profile(struct device *dev, const struct spa_pod *param) {
    if (dev->staging.active_profile != NULL) {
        ERROR("Got Profile for dev %d, but active profile is already set to %d (%s), "
              "PLEASE REPORT THIS AS A BUG!!!",
              dev->id, dev->active_profile->index, dev->active_profile->name);
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
    new_profile->description = xstrdup(description_str);

    const char *name_str = NULL;
    spa_pod_get_string(&name->value, &name_str);
    new_profile->name = xstrdup(name_str);

    dev->staging.active_profile = new_profile;
    DEBUG("New profile (Profile) on dev %d: %s (%s) index %d",
          dev->id, new_profile->description, new_profile->name, new_profile->index);
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

