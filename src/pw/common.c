#include "pw/common.h"
#include "pw/node.h"
#include "pw/device.h"
#include "macros.h"
#include "log.h"
#include "xmalloc.h"
#include "thirdparty/stb_ds.h"

struct pw pw = {0};

const struct pw_device_events device_events = {
    .version = PW_VERSION_DEVICE_EVENTS,
    .info = on_device_info,
    .param = on_device_param,
};

const struct pw_node_events node_events = {
    .version = PW_VERSION_NODE_EVENTS,
    .info = on_node_info,
    .param = on_node_param,
};

static void on_registry_global(void *data, uint32_t id, uint32_t permissions,
                               const char *type, uint32_t version,
                               const struct spa_dict *props) {
    DEBUG("registry global: id %d, perms "PW_PERMISSION_FORMAT", ver %d, type %s",
          id, PW_PERMISSION_ARGS(permissions), version, type);
    uint32_t i = 0;
    const struct spa_dict_item *item;
    spa_dict_for_each(item, props) {
        TRACE("%c---%s: %s", (++i == props->n_items ? '\\' : '|'), item->key, item->value);
    }

    if (STREQ(type, PW_TYPE_INTERFACE_Node)) {
        const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
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

        struct node *new_node = xcalloc(1, sizeof(*new_node));
        new_node->id = id;
        new_node->media_class = media_class_value;
        new_node->pw_node = pw_registry_bind(pw.registry, id, type, PW_VERSION_NODE, 0);
        pw_node_add_listener(new_node->pw_node, &new_node->listener, &node_events, new_node);
    } else if (STREQ(type, PW_TYPE_INTERFACE_Device)) {
        const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        if (media_class == NULL) {
            DEBUG("empty media.class, not binding");
            return;
        } else if (STREQ(media_class, "Audio/Device")) {
            /* no-op */
        } else {
            DEBUG("not interested in media.class %s, not binding", media_class);
            return;
        }

        struct device *new_device = xcalloc(1, sizeof(*new_device));
        spa_list_init(&new_device->routes);

        new_device->id = id;
        new_device->pw_device = pw_registry_bind(pw.registry, id, type, PW_VERSION_DEVICE, 0);
        pw_device_add_listener(new_device->pw_device, &new_device->listener,
                               &device_events, new_device);

        stbds_hmput(pw.devices, new_device->id, new_device);
    }
}

static void on_registry_global_remove(void *data, uint32_t id) {
    DEBUG("registry global remove: id %d", id);

    struct node *node;
    if ((node = stbds_hmget(pw.nodes, id)) != NULL) {
        on_node_remove(node);
    }
    struct device *device;
    if ((device = stbds_hmget(pw.devices, id)) != NULL) {
        stbds_hmdel(pw.devices, id);
        device_free(device);
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

int pipewire_init(void) {
    pw_init(NULL, NULL);

    pw.main_loop = pw_main_loop_new(NULL /* properties */);
    pw.main_loop_loop = pw_main_loop_get_loop(pw.main_loop);
    pw.main_loop_loop_fd = pw_loop_get_fd(pw.main_loop_loop);

    pw.context = pw_context_new(pw.main_loop_loop, NULL, 0);
    if (pw.context == NULL) {
        ERROR("failed to create pw_context: %s", strerror(errno));
        return -1;
    }

    pw.core = pw_context_connect(pw.context, NULL, 0);
    if (pw.core == NULL) {
        ERROR("failed to connect to pipewire: %s", strerror(errno));
        return -1;
    }
    pw_core_add_listener(pw.core, &pw.core_listener, &core_events, NULL);

    pw.registry = pw_core_get_registry(pw.core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(pw.registry, &pw.registry_listener, &registry_events, NULL);

    return 0;
}

void pipewire_cleanup(void) {
    for (int i = stbds_hmlen(pw.nodes) - 1; i >= 0; i--) {
        node_free(pw.nodes[i].value);
    }
    stbds_hmfree(pw.nodes);

    for (int i = stbds_hmlen(pw.devices) - 1; i >= 0; i--) {
        device_free(pw.devices[i].value);
    }
    stbds_hmfree(pw.devices);

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
        pw_main_loop_destroy(pw.main_loop);
    }
    pw_deinit();
}

