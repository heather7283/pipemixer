#pragma once

#include <stdint.h>
#include <stdbool.h>

/* see "POD Types" in https://docs.pipewire.org/page_spa_pod.html */
typedef uint32_t pw_id_t;
typedef int32_t pw_int_t;

struct param_props {
    bool mute;
    unsigned n_channels;
    const char **channel_names;
    float *channel_volumes;
};

void param_props_free_contents(struct param_props *props);

struct param_route {
    pw_int_t index;
    pw_int_t device;
    pw_id_t direction; /* enum spa_direction */

    pw_int_t *devices;
    pw_int_t *profiles;
    unsigned n_devices;
    unsigned n_profiles;

    char *description;
    char *name;

    struct param_props props;

    bool active;
};

void param_route_free_contents(struct param_route *route);

struct param_profile {
    pw_int_t index;

    char *description;
    char *name;

    bool active;
};

void param_profile_free_contents(struct param_profile *profile);

