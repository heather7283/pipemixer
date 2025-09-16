#include "pw/common.h"
#include "pw/node.h"
#include "pw/device.h"
#include "eventloop.h"
#include "macros.h"
#include "log.h"

struct pw pw = {0};

struct signal_emitter core_emitter = {0};

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

        node_create(id, media_class_value);
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

        device_create(id);
    }
}

static void on_registry_global_remove(void *data, uint32_t id) {
    DEBUG("registry global remove: id %d", id);

    struct node *node = node_lookup(id);
    if (node != NULL) {
        node_destroy(node);
        return;
    }
    struct device *device = device_lookup(id);
    if (device != NULL) {
        device_destroy(device);
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

static int pipewire_fd_handler(struct pollen_callback *callback,
                               int fd, uint32_t events, void *data) {
    int res = pw_loop_iterate(pw.main_loop_loop, 0);
    if (res < 0 && res != -EINTR) {
        return res;
    } else {
        return 0;
    }
}

int pipewire_init(void) {
    pw_init(NULL, NULL);

    pw.main_loop = pw_main_loop_new(NULL /* properties */);
    pw.main_loop_loop = pw_main_loop_get_loop(pw.main_loop);
    pw.main_loop_loop_fd = pw_loop_get_fd(pw.main_loop_loop);

    pollen_loop_add_fd(event_loop, pw.main_loop_loop_fd, EPOLLIN, false,
                       pipewire_fd_handler, NULL);

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

    signal_emitter_init(&pw.core_emitter);
    signal_emitter_init(&pw.node_emitter);
    signal_emitter_init(&pw.device_emitter);

    return 0;
}

void pipewire_cleanup(void) {
    //struct node *node;
    //HASHMAP_FOR_EACH(node, &pw.nodes, hash) {
    //    node_destroy(node);
    //}

    //struct device *device;
    //HASHMAP_FOR_EACH(device, &pw.devices, hash) {
    //    device_destroy(device);
    //}

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

