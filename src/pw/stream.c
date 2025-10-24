#include <spa/param/audio/format-utils.h>

#include "stream.h"
#include "pw/common.h"
#include "log.h"

/* our data processing function is in general:
 *
 *  struct pw_buffer *b;
 *  b = pw_stream_dequeue_buffer(stream);
 *
 *  .. consume stuff in the buffer ...
 *
 *  pw_stream_queue_buffer(stream, b);
 */
static void on_stream_process(void *userdata) {
    struct stream *const stream = userdata;

    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *samples, max;
    uint32_t c, n, n_channels, n_samples, peak;

    if ((b = pw_stream_dequeue_buffer(stream->pw_stream)) == NULL) {
        WARN("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    if ((samples = buf->datas[0].data) == NULL)
        return;

    n_channels = stream->format.info.raw.channels;
    n_samples = buf->datas[0].chunk->size / sizeof(float);

    //TRACE("captured %d samples", n_samples / n_channels);

    float peaks[n_channels];

    for (c = 0; c < n_channels; c++) {
        max = 0.0f;
        for (n = c; n < n_samples; n += n_channels) {
            max = fmaxf(max, fabsf(samples[n]));
        }

        peaks[c] = max;

        //peak = (uint32_t)SPA_CLAMPF(max * 30, 0.f, 39.f);

        //TRACE("channel %d: |%*s%*s| peak:%f", c, peak+1, "*", 40 - peak, "", max);
    }

    pw_stream_queue_buffer(stream->pw_stream, b);

    stream->callback(peaks, n_channels, stream->callback_data);
}

/* Be notified when the stream param changes. We're only looking at the
 * format changes.
 */
static void on_stream_param_changed(void *userdata, uint32_t id, const struct spa_pod *param) {
    struct stream *const stream = userdata;

    /* NULL means to clear the format */
    if (param == NULL || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(param, &stream->format.media_type, &stream->format.media_subtype) < 0)
        return;

    /* only accept raw audio */
    if (stream->format.media_type != SPA_MEDIA_TYPE_audio
        || stream->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    /* call a helper function to parse the format for us. */
    spa_format_audio_raw_parse(param, &stream->format.info.raw);

    INFO("capturing rate:%d channels:%d",
         stream->format.info.raw.rate, stream->format.info.raw.channels);

}
static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_stream_process,
};

bool stream_create(struct stream *stream, uint64_t node_serial,
                   stream_peak_callback_t callback, void *callback_data) {
    static char node_id_str[21];

    snprintf(node_id_str, sizeof(node_id_str), "%"PRIu64, node_serial);
    struct pw_properties *stream_props = pw_properties_new(PW_KEY_APP_NAME, "pipemixer",
                                                           PW_KEY_TARGET_OBJECT, node_id_str,
                                                           NULL);

    INFO("creating stream on node serial %"PRIu64, node_serial);
    stream->pw_stream = pw_stream_new(pw.core, "pipemixer", stream_props);
    pw_stream_add_listener(stream->pw_stream, &stream->listener, &stream_events, stream);

    static uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &SPA_AUDIO_INFO_RAW_INIT(
        .format = SPA_AUDIO_FORMAT_F32
    ));

    const enum pw_stream_flags flags = PW_STREAM_FLAG_AUTOCONNECT
                                     | PW_STREAM_FLAG_DONT_RECONNECT
                                     | PW_STREAM_FLAG_MAP_BUFFERS
                                     | PW_STREAM_FLAG_RT_PROCESS;
    pw_stream_connect(stream->pw_stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                      flags, params, SIZEOF_ARRAY(params));

    stream->callback = callback;
    stream->callback_data = callback_data;

    return true;
}

void stream_destroy(struct stream *stream) {
    pw_stream_destroy(stream->pw_stream);
}

