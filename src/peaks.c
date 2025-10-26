#include <math.h>

#include "peaks.h"

void peak_visualiser_init(struct peak_visualiser *v,
                          float min_db, float max_db,
                          float smoothing_factor) {
    *v = (struct peak_visualiser){
        .min_db = min_db,
        .max_db = max_db,
        .smoothing_factor = smoothing_factor,
    };
}

static float peak_linear_to_db(float linear, float min_db, float max_db) {
    if (linear <= 0) {
        return 0;
    }

    const float db = 20 * log10f(linear);
    const float db_clamped = fmaxf(min_db, fminf(max_db, db));

    const float range_db = max_db - min_db;
    const float normalised_db = (db_clamped - min_db) / range_db;

    return normalised_db;
}

float peak_visualiser_update(struct peak_visualiser *v, float linear_peak) {
    v->smoothed_linear_peak =
        (v->smoothing_factor * linear_peak) + ((1 - v->smoothing_factor) * v->smoothed_linear_peak);

    return peak_linear_to_db(v->smoothed_linear_peak, v->min_db, v->max_db);
}

