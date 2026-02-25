#include <pipewire/extensions/metadata.h>
#include <spa/utils/json.h>

#include "pw/common.h"
#include "pw/node.h"
#include "pw/device.h"
#include "collections/map.h"
#include "eventloop.h"
#include "xmalloc.h"
#include "macros.h"
#include "utils.h"
#include "log.h"

struct pipewire {
    struct pw_loop *main_loop;
    int main_loop_fd;

    struct pw_context *context;

    struct pw_core *core;
    struct spa_hook core_listener;

    struct pw_registry *registry;
    struct spa_hook registry_listener;

    struct default_metadata {
        union {
            struct pw_metadata *pw_metadata;
            struct pw_proxy *pw_proxy;
        };
        struct spa_hook listener, proxy_listener;
        char *properties[DEFAULT_METADATA_KEY_COUNT];
        bool roundtrip;
    } default_metadata;

    struct map nodes, devices;

    struct event_emitter *emitter;
} pw = {0};

enum pipewire_event_types {
    PIPEWIRE_EVENT_NODE,
    PIPEWIRE_EVENT_DEVICE,
    PIPEWIRE_EVENT_DEFAULT,
};

static void pipewire_event_dispatcher(uint64_t id, union event_data data,
                                      const void *callbacks, void *callbacks_data,
                                      void *_) {
    const struct pipewire_events *table = callbacks;

    switch ((enum pipewire_event_types)id) {
    case PIPEWIRE_EVENT_NODE: {
        struct node *node = node_lookup(data.u);
        if (node) {
            EVENT_DISPATCH(table->node, node, callbacks_data);
        }
        break;
    }
    case PIPEWIRE_EVENT_DEVICE: {
        struct device *dev = device_lookup(data.u);
        if (dev) {
            EVENT_DISPATCH(table->device, dev, callbacks_data);
        }
        break;
    }
    case PIPEWIRE_EVENT_DEFAULT: {
        enum default_metadata_key key = data.u;
        struct default_metadata *md = &pw.default_metadata;
        EVENT_DISPATCH(table->default_, key, md->properties[key], callbacks_data);
        break;
    }
    default:
        ERROR("unexpected pipewire event %"PRIu64, id);
    }
}

static void emit_node(struct node *node, struct event_hook *hook) {
    event_emit(pw.emitter, hook, PIPEWIRE_EVENT_NODE, 'u', node->id);
}

static void emit_device(struct device *dev, struct event_hook *hook) {
    event_emit(pw.emitter, hook, PIPEWIRE_EVENT_DEVICE, 'u', dev->id);
}

static void emit_default(enum default_metadata_key key, struct event_hook *hook) {
    event_emit(pw.emitter, hook, PIPEWIRE_EVENT_DEFAULT, 'u', key);
}

struct event_hook *pipewire_add_listener(const struct pipewire_events *events, void *data) {
    struct event_hook *hook = event_emitter_add_hook(pw.emitter, events, data, NULL, NULL);

    struct node *node;
    MAP_FOREACH(&pw.nodes, &node) {
        emit_node(node, hook);
    }

    struct device *device;
    MAP_FOREACH(&pw.devices, &device) {
        emit_device(device, hook);
    }

    if (pw.default_metadata.pw_metadata) {
        for (unsigned i = 0; i < DEFAULT_METADATA_KEY_COUNT; i++) {
            emit_default(i, hook);
        }
    }

    return hook;
}

struct node *node_lookup(uint32_t id) {
    struct node *node = map_get(&pw.nodes, id);
    if (!node) {
        WARN("node with id %u was not found", id);
    }
    return node;
}

struct device *device_lookup(uint32_t id) {
    struct device *device = map_get(&pw.devices, id);
    if (!device) {
        WARN("device with id %u was not found", id);
    }
    return device;
}

static const char *default_metadata_key_str(enum default_metadata_key key) {
    static const char *const keys[] = {
        [DEFAULT_AUDIO_SINK] = "default.audio.sink",
        [DEFAULT_CONFIGURED_AUDIO_SINK] = "default.configured.audio.sink",
        [DEFAULT_AUDIO_SOURCE] = "default.audio.source",
        [DEFAULT_CONFIGURED_AUDIO_SOURCE] = "default.configured.audio.source",
    };

    return keys[key];
}

void pipewire_set_default(enum default_metadata_key key, const char *value) {
    /* TODO: proper escaping? */
    char *json;
    xasprintf(&json, "{ \"name\": \"%s\" }", value);

    pw_metadata_set_property(pw.default_metadata.pw_metadata, 0,
                             default_metadata_key_str(key), "Spa:String:JSON", json);
    free(json);
}

static int on_default_metadata_property(void *data, uint32_t id, const char *key,
                                        const char *type, const char *val) {
    struct default_metadata *md = data;

    INFO("default metadata property id=%u key=%s type=%s val=%s", id, key, type, val);

    if (!streq(type, "Spa:String:JSON")) {
        WARN("unexpected metadata property type %s", type);
        return 0; /* what am I even expected to return here? */
    }

    const char *name = NULL;
    int name_len = 0;
    struct spa_json iter;
    if (spa_json_begin_object(&iter, val, strlen(val)) < 0) {
        ERROR("could not parse metdata property json");
        return 0;
    } else if ((name_len = spa_json_object_find(&iter, "name", &name)) < 0) {
        ERROR("did not find \"name\" in metadata property json");
        return 0;
    } else if (name[0] != '"' || name[name_len - 1] != '"') {
        ERROR("value of \"name\" in metadata property is not a string");
        return 0;
    }

    /* thank you pipewire for this amazing json api that
     * returns strings WITH QUOTES FOR WHATEVER REASON??? */
    name += 1;
    name_len -= 2;

    for (unsigned i = 0; i < DEFAULT_METADATA_KEY_COUNT; i++) {
        if (streq(key, default_metadata_key_str(i))) {
            free(md->properties[i]);
            xasprintf(&md->properties[i], "%.*s", name_len, name);
            emit_default(i, NULL);
            break;
        }
    }

    return 0;
}

static const struct pw_metadata_events default_metadata_events = {
    .version = PW_VERSION_METADATA_EVENTS,
    .property = on_default_metadata_property,
};

static void on_registry_global(void *data, uint32_t id, uint32_t permissions,
                               const char *type, uint32_t version,
                               const struct spa_dict *props) {
    DEBUG("registry global: id=%d, perms=0o%o, type=%s, ver=%d", id, permissions, type, version);

    if (streq(type, PW_TYPE_INTERFACE_Node)) {
        const char *media_class = spa_dict_lookup(props, "media.class");
        enum media_class media_class_value;
        if (media_class == NULL) {
            DEBUG("empty media.class, not binding");
            return;
        } else if (STREQ(media_class, "Audio/Source")) {
            media_class_value = AUDIO_SOURCE;
        } else if (STREQ(media_class, "Audio/Sink")) {
            media_class_value = AUDIO_SINK;
        } else if (STREQ(media_class, "Stream/Input/Audio")) {
            media_class_value = STREAM_INPUT_AUDIO;
        } else if (STREQ(media_class, "Stream/Output/Audio")) {
            media_class_value = STREAM_OUTPUT_AUDIO;
        } else {
            DEBUG("not interested in media.class %s, not binding", media_class);
            return;
        }

        struct pw_node *pw_node = pw_registry_bind(pw.registry, id, type, PW_VERSION_NODE, 0);
        struct node *node = node_create(pw_node, id, media_class_value);
        map_insert(&pw.nodes, id, node);
        emit_node(node, NULL);
    } else if (streq(type, PW_TYPE_INTERFACE_Device)) {
        const char *media_class = spa_dict_lookup(props, "media.class");
        if (media_class == NULL) {
            DEBUG("empty media.class, not binding");
            return;
        } else if (STREQ(media_class, "Audio/Device")) {
            /* no-op */
        } else {
            DEBUG("not interested in media.class %s, not binding", media_class);
            return;
        }

        struct pw_device *pw_device = pw_registry_bind(pw.registry, id, type, PW_VERSION_DEVICE, 0);
        struct device *device = device_create(pw_device, id);
        map_insert(&pw.devices, id, device);
        emit_device(device, NULL);
    } else if (streq(type, PW_TYPE_INTERFACE_Metadata)) {
        if (!streq(spa_dict_lookup(props, "metadata.name"), "default")) {
            return;
        }

        struct default_metadata *md = &pw.default_metadata;
        if (md->pw_metadata) {
            WARN("got another instance of default metadata (id %d)", id);
            return;
        }

        INFO("got default metadata, id %d", id);
        md->pw_metadata = pw_registry_bind(pw.registry, id, type, PW_VERSION_METADATA, 0);
        pw_metadata_add_listener(md->pw_metadata, &md->listener,
                                 &default_metadata_events, md);
    }
}

static void on_registry_global_remove(void *data, uint32_t id) {
    struct node *node = map_remove(&pw.nodes, id);
    if (node) {
        TRACE("registry global_remove: found node %u", id);
        node_unref(&node);
        return;
    }

    struct device *device = map_remove(&pw.devices, id);
    if (device) {
        TRACE("registry global_remove: found device %u", id);
        device_unref(&device);
        return;
    }
}

static const struct pw_registry_events registry_events = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = on_registry_global,
    .global_remove = on_registry_global_remove,
};

static void on_core_error(void *data, uint32_t id, int seq, int res, const char *message) {
    ERROR("core error %d on object %d: %d (%s)", seq, id, res, message);
}

static const struct pw_core_events core_events = {
    .version = PW_VERSION_CORE_EVENTS,
    .error = on_core_error,
};

bool pipewire_init(void) {
    pw.context = pw_context_new(event_loop, NULL, 0);
    if (pw.context == NULL) {
        ERROR("failed to create pw_context: %s", strerror(errno));
        return false;
    }

    pw.core = pw_context_connect(pw.context, NULL, 0);
    if (pw.core == NULL) {
        ERROR("failed to connect to pipewire: %s", strerror(errno));
        return false;
    }
    pw_core_add_listener(pw.core, &pw.core_listener, &core_events, NULL);

    pw.registry = pw_core_get_registry(pw.core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(pw.registry, &pw.registry_listener, &registry_events, NULL);

    pw.emitter = event_emitter_create(pipewire_event_dispatcher);

    return true;
}

void pipewire_cleanup(void) {
    if (pw.registry != NULL) {
        pw_proxy_destroy((struct pw_proxy *)pw.registry);
    }
    if (pw.core != NULL) {
        pw_core_disconnect(pw.core);
    }
    if (pw.context != NULL) {
        pw_context_destroy(pw.context);
    }
    if (pw.main_loop != NULL) {
        pw_loop_destroy(pw.main_loop);
    }
    pw_deinit();
}

