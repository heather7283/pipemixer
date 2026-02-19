#pragma once

#include <pipewire/pipewire.h>

#include "pw/types.h"
#include "collections/vec.h"
#include "events.h"

struct device_props {
    char *description;
};

struct device {
    union {
        struct pw_device *pw_device;
        struct pw_proxy *pw_proxy;
    };
    struct spa_hook listener;
    struct spa_hook proxy_listener;

    uint32_t id;
    struct device_props props;

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

    bool new;
    unsigned refcnt;
};

struct device *device_create(struct pw_device *pw_device, uint32_t id);

struct device *device_ref(struct device *dev);
void device_unref(struct device **pdev);

uint32_t device_get_id(const struct device *dev);

void device_set_props(const struct device *dev, const struct spa_pod *props,
                      enum spa_direction direction, int32_t card_profile_device);
void device_set_route(const struct device *dev, int32_t card_profile_device, int32_t index);

void device_set_profile(const struct device *dev, int32_t index);

struct device_events {
    void (*removed)(struct device *dev, void *data);
    void (*props)(struct device *dev, const struct device_props *props, void *data);
    void (*routes)(struct device *dev,
                   const struct param_route routes[], unsigned routes_count,
                   void *data);
    void (*profiles)(struct device *dev,
                     const struct param_profile profiles[], unsigned profiles_count,
                     void *data);
};

void device_add_listener(struct device *dev, struct event_hook *hook,
                         const struct device_events *ev, void *data);

