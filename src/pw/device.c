#include <spa/pod/builder.h>
#include <spa/pod/parser.h>
#include <spa/param/props.h>
#include <spa/param/audio/raw-types.h>

#include "pw/device.h"
#include "collections/vec.h"
#include "log.h"
#include "xmalloc.h"
#include "macros.h"

struct device {
    union {
        struct pw_device *pw_device;
        struct pw_proxy *pw_proxy;
    };
    struct spa_hook listener;
    struct spa_hook proxy_listener;

    uint32_t id;
    struct dict props;

    VEC(struct param_route) routes;
    VEC(struct param_profile) profiles;

    /* needed to atomically update routes and profiles */
    struct {
        VEC(struct param_route) routes;
        VEC(struct param_profile) profiles;
    } staging;

    struct event_emitter *emitter;

    bool has_props;
    bool has_routes;
    bool has_profiles;

    unsigned refcnt;
};

enum device_event_types {
    DEVICE_EVENT_REMOVED,
    DEVICE_EVENT_PROPS,
    DEVICE_EVENT_ROUTES,
    DEVICE_EVENT_PROFILES,
};

static void device_event_dispatcher(uint64_t id, union event_data data,
                                    const void *callbacks, void *callbacks_data,
                                    void *private_data) {
    const struct device_events *table = callbacks;
    struct device *dev = private_data;

    switch ((enum device_event_types)id) {
    case DEVICE_EVENT_REMOVED: {
        EVENT_DISPATCH(table->removed, dev, callbacks_data);
        break;
    }
    case DEVICE_EVENT_PROPS: {
        EVENT_DISPATCH(table->props, dev, &dev->props, callbacks_data);
        break;
    }
    case DEVICE_EVENT_ROUTES: {
        EVENT_DISPATCH(table->routes, dev,
                       dev->routes.data, dev->routes.size,
                       callbacks_data);
        break;
    }
    case DEVICE_EVENT_PROFILES: {
        EVENT_DISPATCH(table->profiles, dev,
                       dev->profiles.data, dev->profiles.size,
                       callbacks_data);
        break;
    }
    default:
        ERROR("unexpected device event id %"PRIu64, id);
    }
}

static void emit_removed(struct device *dev, struct event_hook *hook) {
    event_emit(dev->emitter, hook, DEVICE_EVENT_REMOVED, NULL, '0');
}

static void emit_props(struct device *dev, struct event_hook *hook) {
    event_emit(dev->emitter, hook, DEVICE_EVENT_PROPS, NULL, '0');
}

static void emit_routes(struct device *dev, struct event_hook *hook) {
    event_emit(dev->emitter, hook, DEVICE_EVENT_ROUTES, NULL, '0');
}

static void emit_profiles(struct device *dev, struct event_hook *hook) {
    event_emit(dev->emitter, hook, DEVICE_EVENT_PROFILES, NULL, '0');
}

static void hook_remove(void *private_data) {
    struct device *dev = private_data;
    device_unref(&dev);
}

struct event_hook *device_add_listener(struct device *dev,
                                       const struct device_events *events,
                                       void *data) {
    struct event_hook *hook = event_emitter_add_hook(dev->emitter, events, data,
                                                     hook_remove, device_ref(dev));

    if (dev->has_props) {
        emit_props(dev, hook);
    }
    if (dev->has_profiles) {
        emit_profiles(dev, hook);
    }
    if (dev->has_routes) {
        emit_routes(dev, hook);
    }

    return hook;
}

void device_set_props(const struct device *dev,
                      const struct param_route *route,
                      const struct spa_pod *props) {
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

void device_set_profile(const struct device *dev, int32_t index) {
    uint8_t buffer[1024];
    struct spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    struct spa_pod *profile =
        spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_ParamProfile, SPA_PARAM_Profile,
                                   SPA_PARAM_PROFILE_index, SPA_POD_Int(index),
                                   SPA_PARAM_PROFILE_save, SPA_POD_Bool(true));

    pw_device_set_param(dev->pw_device, SPA_PARAM_Profile, 0, profile);
}

static void on_device_param_route(struct device *dev, const struct spa_pod *param) {
    pw_int_t index, device, profile;

    struct spa_pod_parser p;
    spa_pod_parser_pod(&p, param);
    const int n =
        spa_pod_parser_get_object(&p,
                                  SPA_TYPE_OBJECT_ParamRoute, &(pw_id_t){},
                                  SPA_PARAM_ROUTE_index, SPA_POD_Int(&index),
                                  SPA_PARAM_ROUTE_device, SPA_POD_Int(&device),
                                  SPA_PARAM_ROUTE_profile, SPA_POD_Int(&profile));
    if (n != 3) {
        ERROR("failed to get index, device and profile from Route");
        return;
    }

    struct param_route *active = NULL;
    VEC_FOREACH(&dev->staging.routes, i) {
        struct param_route *route = &dev->staging.routes.data[i];

        if (route->index == index) {
            active = route;
            break;
        }
    }

    if (!active) {
        WARN("Route %d doesn't match any EnumRoute on dev %d", index, dev->id);
    } else {
        active->active = true;
        active->index = index;
        active->device = device;
        DEBUG("dev %d Route: index=%d device=%d", dev->id, active->index, active->device);
    }
}

static void on_device_param_enum_route(struct device *dev, const struct spa_pod *param) {
    /* EnumRoute does not have device and profile, only devices and profiles */
    struct spa_pod_parser p;
    spa_pod_parser_pod(&p, param);

    pw_int_t index;
    pw_id_t direction;
    const char *name, *description;

    uint32_t dev_csize, dev_ctype, dev_nvals;
    const pw_int_t *dev_vals;

    uint32_t prof_csize, prof_ctype, prof_nvals;
    const pw_int_t *prof_vals;

    const int n =
        spa_pod_parser_get_object(&p,
                                  SPA_TYPE_OBJECT_ParamRoute, &(pw_id_t){},
                                  SPA_PARAM_ROUTE_name, SPA_POD_String(&name),
                                  SPA_PARAM_ROUTE_description, SPA_POD_String(&description),
                                  SPA_PARAM_ROUTE_index, SPA_POD_Int(&index),
                                  SPA_PARAM_ROUTE_direction, SPA_POD_Id(&direction),
                                  SPA_PARAM_ROUTE_devices, SPA_POD_Array(&dev_csize, &dev_ctype,
                                                                         &dev_nvals, &dev_vals),
                                  SPA_PARAM_ROUTE_profiles, SPA_POD_Array(&prof_csize, &prof_ctype,
                                                                          &prof_nvals, &prof_vals));
    if (n != 6) {
        ERROR("failed to parse EnumRoute");
        return;
    } else if (dev_ctype != SPA_TYPE_Int || prof_ctype != SPA_TYPE_Int) {
        ERROR("unexpected array member type in pod");
        return;
    }

    struct param_route *new_route = VEC_APPEND(&dev->staging.routes);
    *new_route = (struct param_route){
        .index = index,
        .direction = direction,
        .name = xstrdup(name),
        .description = xstrdup(description),
        .n_devices = dev_nvals,
        .devices = xmemduparray(dev_vals, dev_nvals, dev_csize),
        .n_profiles = prof_nvals,
        .profiles = xmemduparray(prof_vals, prof_nvals, prof_csize),
    };

    DEBUG("dev %d EnumRoute: index=%d dir=%d name=%s desc=%s",
          dev->id, new_route->index, new_route->direction, new_route->name, new_route->description);
}

static void on_device_param_enum_profile(struct device *dev, const struct spa_pod *param) {
    struct spa_pod_parser p;
    spa_pod_parser_pod(&p, param);

    const char *description, *name;
    pw_int_t index;

    const int n =
        spa_pod_parser_get_object(&p,
                                  SPA_TYPE_OBJECT_ParamProfile, &(pw_id_t){},
                                  SPA_PARAM_PROFILE_name, SPA_POD_String(&name),
                                  SPA_PARAM_PROFILE_description, SPA_POD_String(&description),
                                  SPA_PARAM_PROFILE_index, SPA_POD_Int(&index));
    if (n != 3) {
        ERROR("failed to parse Profile");
        return;
    }

    struct param_profile *new_profile = VEC_APPEND(&dev->staging.profiles);
    *new_profile = (struct param_profile){
        .index = index,
        .description = xstrdup(description),
        .name = xstrdup(name),
    };

    DEBUG("dev %d EnumProfile: index=%d name=%s desc=%s",
          dev->id, new_profile->index, new_profile->name, new_profile->description);
}

static void on_device_param_profile(struct device *dev, const struct spa_pod *param) {
    struct spa_pod_parser p;
    spa_pod_parser_pod(&p, param);

    pw_int_t index;

    const int n =
        spa_pod_parser_get_object(&p,
                                  SPA_TYPE_OBJECT_ParamProfile, &(pw_id_t){},
                                  SPA_PARAM_PROFILE_index, SPA_POD_Int(&index));
    if (n != 1) {
        ERROR("failed to get index from Profile");
        return;
    }

    struct param_profile *active = NULL;
    VEC_FOREACH(&dev->staging.profiles, i) {
        struct param_profile *prof = &dev->staging.profiles.data[i];

        if (prof->active) {
            WARN("BUG: got a Profile (%d) after active Profile has already been found (%d)!",
                 index, prof->index);
        } else if (prof->index == index) {
            active = prof;
            /* don't break here to check all others too */
        }
    }

    if (!active) {
        WARN("Profile %d doesn't match any EnumProfile on dev %d", index, dev->id);
    } else {
        active->active = true;
        DEBUG("dev %d Profile: index=%d", dev->id, active->index);
    }
}

static void on_device_param(void *data, int seq, uint32_t id, uint32_t index,
                            uint32_t next, const struct spa_pod *param) {
    struct device *device = data;

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
    default:
        ERROR("unexpected device param");
    }
}

static void on_device_info(void *data, const struct pw_device_info *info) {
    struct device *dev = data;

    DEBUG("dev %d info: n_params=%d change=0x%lx", info->id, info->n_params, info->change_mask);

    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PROPS) {
        const struct spa_dict *props = info->props;

        dict_clear(&dev->props);
        dict_reserve(&dev->props, props->n_items);
        for (unsigned i = 0; i < props->n_items; i++) {
            const struct spa_dict_item *item = &props->items[i];
            dict_insert(&dev->props, item->key, item->value);
        }

        emit_props(dev, NULL);
        dev->has_props = true;
    }

    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) {
        bool changed = false;

        for (unsigned i = 0; i < info->n_params; i++) {
            struct spa_param_info *param = &info->params[i];

            /* TODO: is this needed? what does this even do? */
            if (!(param->flags & SPA_PARAM_INFO_READ)) {
                continue;
            }

            const bool matches = (param->id == SPA_PARAM_Profile)
                              || (param->id == SPA_PARAM_EnumProfile)
                              || (param->id == SPA_PARAM_Route)
                              || (param->id == SPA_PARAM_EnumRoute);
            if (matches) {
                changed = true;
                break;
            }
        }

        if (changed) {
            /* IN THIS EXACT ORDER! */
            static const int ids[] = {
                SPA_PARAM_EnumProfile, SPA_PARAM_Profile, SPA_PARAM_EnumRoute, SPA_PARAM_Route
            };
            for (unsigned i = 0; i < SIZEOF_ARRAY(ids); i++) {
                /* TODO: would be great to figure out what the last parameter ("filter") does */
                pw_device_enum_params(dev->pw_device, 0, ids[i], 0, -1, NULL);
            }

            pw_proxy_sync(dev->pw_proxy, 1337);
        }
    }
}

const struct pw_device_events device_events = {
    .version = PW_VERSION_DEVICE_EVENTS,
    .info = on_device_info,
    .param = on_device_param,
};

static void on_proxy_roundtrip_done(void *data, int _) {
    struct device *dev = data;

    VEC_FOREACH(&dev->routes, i) {
        struct param_route *route = &dev->routes.data[i];
        param_route_free_contents(route);
    }
    VEC_CLEAR(&dev->routes);
    VEC_EXCHANGE(&dev->routes, &dev->staging.routes);

    VEC_FOREACH(&dev->profiles, i) {
        struct param_profile *profile = &dev->profiles.data[i];
        param_profile_free_contents(profile);
    }
    VEC_CLEAR(&dev->profiles);
    VEC_EXCHANGE(&dev->profiles, &dev->staging.profiles);

    emit_profiles(dev, NULL);
    dev->has_profiles = true;
    emit_routes(dev, NULL);
    dev->has_routes = true;
}

static void on_proxy_removed(void *data) {
    struct device *dev = data;

    emit_removed(dev, NULL);
}

static const struct pw_proxy_events proxy_events = {
    .version = PW_VERSION_PROXY_EVENTS,
    .done = on_proxy_roundtrip_done,
    .removed = on_proxy_removed,
};

struct device *device_create(struct pw_device *pw_device, uint32_t id) {
    struct device *dev = xmalloc(sizeof(*dev));

    *dev = (struct device){
        .id = id,
        .pw_device = pw_device,
        .refcnt = 1,
    };

    dev->emitter = event_emitter_create(device_event_dispatcher);

    pw_device_add_listener(dev->pw_device, &dev->listener, &device_events, dev);
    pw_proxy_add_listener(dev->pw_proxy, &dev->proxy_listener, &proxy_events, dev);

    return dev;
}

static void device_destroy(struct device *device) {
    pw_proxy_destroy(device->pw_proxy);

    dict_free(&device->props);

    VEC_FOREACH(&device->routes, i) {
        struct param_route *route = &device->routes.data[i];
        param_route_free_contents(route);
    }
    VEC_FREE(&device->routes);

    VEC_FOREACH(&device->profiles, i) {
        struct param_profile *profile = &device->profiles.data[i];
        param_profile_free_contents(profile);
    }
    VEC_FREE(&device->profiles);

    /* staging */
    VEC_FOREACH(&device->staging.routes, i) {
        struct param_route *route = &device->staging.routes.data[i];
        param_route_free_contents(route);
    }
    VEC_FREE(&device->staging.routes);

    VEC_FOREACH(&device->staging.profiles, i) {
        struct param_profile *profile = &device->staging.profiles.data[i];
        param_profile_free_contents(profile);
    }
    VEC_FREE(&device->staging.profiles);

    event_emitter_release(device->emitter);

    free(device);
}

struct device *device_ref(struct device *dev) {
    ASSERT(dev->refcnt++ > 0);
    return dev;
}

void device_unref(struct device **pdev) {
    struct device *dev = *pdev;
    ASSERT(dev->refcnt > 0);
    if (--dev->refcnt == 0) {
        device_destroy(dev);
    }
    *pdev = NULL;
}

uint32_t device_id(const struct device *dev) {
    return dev->id;
}

