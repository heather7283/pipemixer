#include <stdlib.h>

#include "pw/types.h"

void param_route_free_contents(struct param_route *route) {
    if (route) {
        free(route->description);
        free(route->name);
        free(route->devices);
        free(route->profiles);
    }
}

void param_profile_free_contents(struct param_profile *profile) {
    if (profile) {
        free(profile->name);
        free(profile->description);
    }
}

