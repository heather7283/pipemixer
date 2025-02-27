#ifndef POD_H
#define POD_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw.h>

struct node_props {
    bool mute;
    uint32_t channel_count;
    /* so much wasted ram... TODO: can I optimize mem usage? */
    float channel_volumes[SPA_AUDIO_MAX_CHANNELS];
    /* longest name is AUX63 + Spa:Enum:AudioChannel: + null < 32 */
    char channel_map[SPA_AUDIO_MAX_CHANNELS][32];
};

int parse_node_props(const struct spa_pod *pod, struct node_props *out);

void pod_print(const struct spa_pod *pod);

#endif /* #ifndef POD_H */

