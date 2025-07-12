#ifndef SRC_PIPEWIRE_DEVICE_H
#define SRC_PIPEWIRE_DEVICE_H

#include "strutils.h"
#include "pw/common.h"

struct route {
    int32_t device, index;
    struct string description;
    uint32_t direction; /* enum spa_direction */

    struct props props;

    struct spa_list link;
};

struct device {
    struct pw_device *pw_device;
    struct spa_hook listener;

    uint32_t id;

    struct spa_list active_routes;
    struct spa_list all_routes;

    HASHMAP_ENTRY hash;
};

void device_free(struct device *device);

void on_device_info(void *data, const struct pw_device_info *info);
void on_device_param(void *data, int seq, uint32_t id, uint32_t index,
                     uint32_t next, const struct spa_pod *param);

void device_set_props(const struct device *dev,
                      const struct spa_pod *props, int32_t card_profile_device);

#endif /* #ifndef SRC_PIPEWIRE_DEVICE_H */

