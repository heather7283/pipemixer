#pragma once

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <stdbool.h>

typedef void (*stream_peak_callback_t)(float peaks[], unsigned int npeaks, void *data);

struct stream {
    struct pw_stream *pw_stream;
    struct spa_hook listener;

    struct spa_audio_info format;

    stream_peak_callback_t callback;
    void *callback_data;
};

bool stream_create(struct stream *stream, uint64_t node_serial,
                   stream_peak_callback_t callback, void *callback_data);

void stream_destroy(struct stream *stream);

