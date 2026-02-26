#pragma once

#include <pipewire/pipewire.h>

#include "pw/types.h"
#include "events.h"

struct device_props {
    char *description;
};

struct device;

struct device *device_create(struct pw_device *pw_device, uint32_t id);

struct device *device_ref(struct device *dev);
void device_unref(struct device **pdev);

uint32_t device_id(const struct device *dev);

void device_set_props(const struct device *dev,
                      const struct param_route *route,
                      const struct spa_pod *props);
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

struct event_hook *device_add_listener(struct device *dev,
                                       const struct device_events *events,
                                       void *data);

