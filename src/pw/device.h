#ifndef SRC_PIPEWIRE_DEVICE_H
#define SRC_PIPEWIRE_DEVICE_H

#include <assert.h>

#include <pipewire/pipewire.h>

#include "collections/vec.h"

struct route {
    int32_t index;
    int32_t device;
    uint32_t direction; /* enum spa_direction */

    VEC(int32_t) devices;
    VEC(int32_t) profiles;

    char *description;
    char *name;
};

struct profile {
    int32_t index;

    char *description;
    char *name;
};

enum device_modified_params {
    ROUTE = 1 << 0,
    ENUM_ROUTE = 1 << 1,
    PROFILE = 1 << 2,
    ENUM_PROFILE = 1 << 3,
};

struct device {
    struct pw_device *pw_device;
    struct spa_hook listener;

    uint32_t id;

    enum device_modified_params modified_params;

    VEC(struct route) routes;
    VEC(struct route) active_routes;
    VEC(struct profile) profiles;
    /* FIXME: relies on the assumption that only one profile can be active at a time. */
    struct profile *active_profile;

    /* needed to atomically update routes and profiles */
    struct {
        VEC(struct route) routes;
        VEC(struct route) active_routes;
        VEC(struct profile) profiles;
        struct profile *active_profile;
    } staging;
};

struct device *device_lookup(uint32_t id);

void device_create(uint32_t id);
void device_destroy(struct device *device);

void on_device_info(void *data, const struct pw_device_info *info);
void on_device_param(void *data, int seq, uint32_t id, uint32_t index,
                     uint32_t next, const struct spa_pod *param);

void device_set_props(const struct device *dev, const struct spa_pod *props,
                      enum spa_direction direction, int32_t card_profile_device);
void device_set_route(const struct device *dev, int32_t card_profile_device, int32_t index);

#endif /* #ifndef SRC_PIPEWIRE_DEVICE_H */

