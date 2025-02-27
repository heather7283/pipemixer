#include <math.h>
#include <spa/param/audio/raw-types.h>

#include "props.h"
#include "log.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
/* TODO: make this modular. Also this is the worst code I've ever written, fix it */
int parse_node_props(const struct spa_pod *pod, struct node_props *out) {
    static struct node_props props;
    bool pod_good = true;

    /*
     * I really tried using normal functions to parse this, put doing it
     * the cursed way really is just easier. I hate pipewire.
     */
    uint32_t *p = (uint32_t *)pod;
    { /* object header */
        uint32_t size = *p++;
        uint32_t id = *p++; /* must be 15 */
        uint32_t object_type = *p++;
        uint32_t object_id = *p++;
        if (id != SPA_TYPE_Object) {
            pod_good = false;
            warn("while parsing param: expected type Object (%d), got %d", SPA_TYPE_Object, id);
            goto param_end;
        }
    }
    { /* object property 1 (volume, Float) */
        uint32_t key = *p++;
        uint32_t flags = *p++;
        uint32_t size = *p++; /* must be 4 */
        uint32_t id = *p++; /* must be 6 */
        float value = *(float *)(p++);
        uint32_t padding = *p++;
        if (size != 4) {
            pod_good = false;
            warn("while parsing param: expected float size 4, got %d", size);
            goto param_end;
        }
        if (id != SPA_TYPE_Float) {
            pod_good = false;
            warn("while parsing param: expected type Float (%d), got %d", SPA_TYPE_Float, id);
            goto param_end;
        }
    }
    { /* object property 2 (mute, Bool) */
        uint32_t key = *p++;
        uint32_t flags = *p++;
        uint32_t size = *p++; /* must be 4 */
        uint32_t id = *p++; /* must be 2 */
        uint32_t value = *p++;
        uint32_t padding = *p++;
        if (size != 4) {
            pod_good = false;
            warn("while parsing param: expected bool size 4, got %d", size);
            goto param_end;
        }
        if (id != SPA_TYPE_Bool) {
            pod_good = false;
            warn("while parsing param: expected type Bool (%d), got %d", SPA_TYPE_Bool, id);
            goto param_end;
        }

        props.mute = value ? true : false;
    }
    { /* object property 3 (Array of Float, channel_volumes) */
        uint32_t key = *p++;
        uint32_t flags = *p++;
        uint32_t size = *p++;
        uint32_t id = *p++; /* must be 13 */
        uint32_t child_size = *p++;
        uint32_t child_type = *p++;
        if (id != SPA_TYPE_Array) {
            pod_good = false;
            warn("while parsing param: expected type Array (%d), got %d", SPA_TYPE_Array, id);
            goto param_end;
        }
        if (child_type != SPA_TYPE_Float) {
            pod_good = false;
            warn("while parsing param: expected array members of type Float (%d), got %d",
                 SPA_TYPE_Float, child_type);
            goto param_end;
        }
        if (child_size != 4) {
            pod_good = false;
            warn("while parsing param: expected array members of size 4 (Float), got %d",
                 child_size);
            goto param_end;
        }

        uint32_t children_size = size - (4 * 2); /* exclude child_size and child_type */
        if (children_size % child_size != 0) {
            pod_good = false;
            warn("while parsing param: children size %d is not a multiple of child size %d",
                 children_size, child_size);
            goto param_end;
        }

        uint32_t children_count = children_size / child_size;
        props.channel_count = children_count;

        for (uint32_t i = 0; i < children_count; i++) {
            float value = *(float *)(p++);

            /* values I get here are cubed, TODO: figure out why */
            value = cbrtf(value);

            debug("channel_volumes[%d] = %f", i, value);
            props.channel_volumes[i] = value;
        }
        if (children_count % 2 != 0) {
            /* padding */
            p++;
        }
    }
    { /* object property 4 (Array of Id, channel_map) */
        uint32_t key = *p++;
        uint32_t flags = *p++;
        uint32_t size = *p++;
        uint32_t id = *p++; /* must be 13 */
        uint32_t child_size = *p++;
        uint32_t child_type = *p++;
        if (id != SPA_TYPE_Array) {
            pod_good = false;
            warn("while parsing param: expected type Array (%d), got %d", SPA_TYPE_Array, id);
            goto param_end;
        }
        if (child_type != SPA_TYPE_Id) {
            pod_good = false;
            warn("while parsing param: expected array members of type Id (%d), got %d",
                 SPA_TYPE_Id, child_type);
            goto param_end;
        }
        if (child_size != 4) {
            pod_good = false;
            warn("while parsing param: expected array members of size 4 (Id), got %d",
                 child_size);
            goto param_end;
        }

        uint32_t children_size = size - (4 * 2); /* exclude child_size and child_type */
        if (children_size % child_size != 0) {
            pod_good = false;
            warn("while parsing param: children size %d is not a multiple of child size %d",
                 children_size, child_size);
            goto param_end;
        }

        uint32_t children_count = children_size / child_size;
        props.channel_count = children_count;

        for (uint32_t i = 0; i < children_count; i++) {
            uint32_t value = *p++;

            strlcpy(props.channel_map[i], spa_type_audio_channel[value].name,
                    sizeof(props.channel_map[0]));

            debug("channel_map[%d] = %s (%d)", i, props.channel_map[i], value);
        }
        if (children_count % 2 != 0) {
            /* padding */
            p++;
        }
    }

param_end:
    if (pod_good) {
        debug("successfully parsed param!");
        /* TODO: probably inefficient af */
        memcpy(out, &props, sizeof(props));
        return 0;
    } else {
        warn("error while parsing param!");
        return -1;
    }
}
#pragma GCC diagnostic pop

