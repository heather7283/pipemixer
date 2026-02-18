#include <stdlib.h>

#include "pw/types.h"

void route_free_contents(struct route *route) {
    if (route) {
        free(route->description);
        free(route->name);
        free(route->devices);
        free(route->profiles);
    }
}

void profile_free_contents(struct profile *profile) {
    if (profile) {
        free(profile->name);
        free(profile->description);
    }
}

