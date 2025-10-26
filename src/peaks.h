#pragma once

struct peak_visualiser {
    float min_db, max_db;

    /* simple exponential smoother in linear domain */
    float smoothing_factor;
    float smoothed_linear_peak;
};

void peak_visualiser_init(struct peak_visualiser *visualiser,
                          float min_db, float max_db,
                          float smoothing_factor);

float peak_visualiser_update(struct peak_visualiser *visualiser, float linear_peak);

