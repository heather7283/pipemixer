#ifndef SRC_PIPEWIRE_DEVICE_H
#define SRC_PIPEWIRE_DEVICE_H

#include <assert.h>

#include <pipewire/pipewire.h>

#include "collections/string.h"
#include "collections/list.h"
#include "collections/hashmap.h"
#include "collections/array.h"

struct route {
    int32_t index;
    int32_t device;
    uint32_t direction; /* enum spa_direction */

    ARRAY(int32_t) devices;
    ARRAY(int32_t) profiles;

    struct string description;

    /* for use in node_get_available_routes() (kinda cringe but eh it works) */
    LIST_ENTRY link;
};

struct profile {
    int32_t index;

    struct string description;
    struct string name;
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

    /* needed to atomically update route list. TODO: is there a better way to do it? */
    int all_routes_index;
    ARRAY(struct route) all_routes[2];
    int active_routes_index;
    ARRAY(struct route) active_routes[2];

    int profiles_index;
    ARRAY(struct profile) profiles[2];
    /* FIXME: relies on the assumption that only one profile can be active at a time. */
    struct profile *active_profile;

    HASHMAP_ENTRY hash;
};

void device_free(struct device *device);

void on_device_info(void *data, const struct pw_device_info *info);
void on_device_param(void *data, int seq, uint32_t id, uint32_t index,
                     uint32_t next, const struct spa_pod *param);

void device_set_props(const struct device *dev, const struct spa_pod *props,
                      enum spa_direction direction, int32_t card_profile_device);
void device_set_route(const struct device *dev, int32_t card_profile_device, int32_t index);

#endif /* #ifndef SRC_PIPEWIRE_DEVICE_H */

