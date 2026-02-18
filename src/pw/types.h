#pragma once

#include <stdint.h>
#include <stdbool.h>

/* see "POD Types" in https://docs.pipewire.org/page_spa_pod.html */
typedef uint32_t pw_id_t;
typedef int32_t pw_int_t;
typedef int32_t pw_bool_t;

struct route {
    pw_int_t index;
    pw_int_t device;
    pw_id_t direction; /* enum spa_direction */

    pw_int_t *devices;
    pw_int_t *profiles;
    unsigned n_devices;
    unsigned n_profiles;

    char *description;
    char *name;

    bool active;
};

void route_free_contents(struct route *route);

struct profile {
    pw_int_t index;

    char *description;
    char *name;

    bool active;
};

void profile_free_contents(struct profile *profile);

