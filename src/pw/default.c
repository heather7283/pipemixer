#include <pipewire/extensions/metadata.h>
#include <spa/utils/json.h>

#include "pw/common.h"
#include "pw/default.h"
#include "pw/node.h"
#include "xmalloc.h"
#include "utils.h"
#include "log.h"

static void on_default_changed(const char *value, enum media_class media_class) {
    /* FIXME: this is slow, but default nodes rarely change so it's probably fine */
    struct node **pnode = NULL;
    MAP_FOREACH(&nodes, &pnode) {
        struct node *node = *pnode;

        if (node->media_class != media_class) {
            continue;
        }

        if (node->node_name == NULL) {
            continue;
        }

        /*
         * Yes, the way this is written allows for multiple nodes to be default
         * if they have equal names. This is not a pipemixer bug, this is due
         * to how wireplumber (an amazing piece of software) handles default
         * metadata. It expects it to contain a json { "name": "node_name" },
         * from which it matches node by name and sets it as default. Yes, this
         * is very stupid, but who am I to question the infinite wisdom of fdo?
         *
         * Pavucontrol handles that by just displaying both nodes as default,
         * so let's do the same here to avoid complexities.
         */
        if (streq(node->node_name, value)) {
            if (!node->is_default) {
                INFO("node %d is now default", node->id);
                node->is_default = true;
                signal_emit_bool(node->emitter, NODE_EVENT_DEFAULT, node->is_default);
            }
        } else if (node->is_default) {
            INFO("node %d is now NOT default", node->id);
            node->is_default = false;
            signal_emit_bool(node->emitter, NODE_EVENT_DEFAULT, node->is_default);
        }
    }
}

static bool get_name(const char *json, char **pname) {
    static char buf[4096]; /* thank you pipewire for this amazing json api */

    int res = spa_json_str_object_find(json, strlen(json), "name", buf, sizeof(buf));
    if (res != 1) {
        ERROR("failed to parse json: %s", json);
        return false;
    }

    if (*pname != NULL) {
        free(*pname);
    }
    *pname = xstrdup(buf);
    return true;
}

static int on_metadata_property(void *data, uint32_t idk,
                                const char *key, const char *type, const char *val) {
    struct default_metadata *md = data;

    INFO("metadata property: %d \"%s\" = (%s)%s", idk, key, type, val);

    static const struct {
        const char *const name;
        const unsigned off;
        const enum media_class class;
    } props[] = {
        {
            "default.audio.source",
            offsetof(struct default_metadata, audio_source),
            AUDIO_SOURCE
        },
        {
            "default.configured.audio.source",
            offsetof(struct default_metadata, configured_audio_source),
            AUDIO_SOURCE
        },
        {
            "default.audio.sink",
            offsetof(struct default_metadata, audio_sink),
            AUDIO_SINK
        },
        {
            "default.configured.audio.sink",
            offsetof(struct default_metadata, configured_audio_sink),
            AUDIO_SINK
        }
    };

    for (unsigned i = 0; i < SIZEOF_ARRAY(props); i++) {
        if (streq(key, props[i].name)) {
            char **pname = (char **)((uintptr_t)md + props[i].off);
            if (get_name(val, pname)) {
                on_default_changed(*pname, props[i].class);
            }
        }
    }

    return 0;
}

static const struct pw_metadata_events metadata_events = {
    .version = PW_VERSION_METADATA_EVENTS,
    .property = on_metadata_property,
};

void default_metadata_init(struct default_metadata *md, uint32_t id) {
    md->id = id;
    md->pw_metadata = pw_registry_bind(pw.registry, id,
                                       PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0);
    pw_metadata_add_listener(md->pw_metadata, &md->listener, &metadata_events, md);
}

void default_metadata_cleanup(struct default_metadata *md) {
    if (md->pw_metadata != NULL) {
        pw_proxy_destroy((struct pw_proxy *)md->pw_metadata);
    }
}

bool default_metadata_check_default(struct default_metadata *md,
                                    const char *name, enum media_class media_class) {
    switch (media_class) {
    case AUDIO_SINK:
        return streq(name, md->audio_sink)
            || streq(name, md->configured_audio_sink);
    case AUDIO_SOURCE:
        return streq(name, md->audio_source)
            || streq(name, md->configured_audio_source);
    default:
        return false;
    }
}

