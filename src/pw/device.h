#ifndef SRC_PIPEWIRE_DEVICE_H
#define SRC_PIPEWIRE_DEVICE_H

#include <assert.h>

#include <pipewire/pipewire.h>

#include "strutils.h"
#include "collections.h"

struct route {
    int32_t device, index;
    struct string description;
    uint32_t direction; /* enum spa_direction */

    LIST_ENTRY link;
};

struct device {
    struct pw_device *pw_device;
    struct spa_hook listener;

    uint32_t id;

    struct {
        LIST_HEAD all;
        LIST_HEAD active;
    } routes[2];
    struct {
        LIST_HEAD all;
        LIST_HEAD active;
    } new_routes[2]; /* needed to atomically update routes. TODO: better way to do it? */
    static_assert(SPA_DIRECTION_INPUT == 0 && SPA_DIRECTION_OUTPUT == 1,
                  "Unexpected values of spa_direction enums");

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

