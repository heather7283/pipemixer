#include <wchar.h>
#include <spa/param/audio/raw.h>
#include <spa/pod/parser.h>
#include <spa/utils/result.h>
#include <spa/param/props.h>

#include "thirdparty/stb_ds.h"
#include "pw.h"
#include "macros.h"
#include "log.h"
#include "props.h"
#include "param_print.h"
#include "xmalloc.h"

struct pw pw = {0};

static void node_cleanup(struct node *node) {
    pw_proxy_destroy((struct pw_proxy *)node->pw_node);

    free(node);
}

static void on_node_info(void *data, const struct pw_node_info *info) {
    struct node *node = data;

    debug("node info: id %d, op %d/%d%s, ip %d/%d%s, state %d%s, %d params%s,%s change "
          BYTE_BINARY_FORMAT,
          info->id,
          info->n_output_ports, info->max_output_ports,
          info->change_mask & PW_NODE_CHANGE_MASK_OUTPUT_PORTS ? " C" : "",
          info->n_input_ports, info->max_input_ports,
          info->change_mask & PW_NODE_CHANGE_MASK_INPUT_PORTS ? " C" : "",
          info->state,
          info->change_mask & PW_NODE_CHANGE_MASK_STATE ? " C" : "",
          info->n_params,
          info->change_mask & PW_NODE_CHANGE_MASK_PARAMS ? " C" : "",
          info->change_mask & PW_NODE_CHANGE_MASK_PROPS ? " props," : "",
          BYTE_BINARY_ARGS(info->change_mask));

    uint32_t i = 0;
    const struct spa_dict_item *item;
    spa_dict_for_each(item, info->props) {
        const char *k = item->key;
        const char *v = item->value;

        trace("%c---%s: %s", (++i == info->props->n_items ? '\\' : '|'), k, v);

        if (STREQ(k, PW_KEY_MEDIA_NAME)) {
            //size_t conv = mbstowcs(node->media_name, v, ARRAY_SIZE(node->media_name));
            //if (conv == ARRAY_SIZE(node->media_name)) {
            //    /* not enough space for string, add ellipsis and null-terminate */
            //    node->media_name[ARRAY_SIZE(node->media_name) - 1] = L'\0';
            //    node->media_name[ARRAY_SIZE(node->media_name) - 2] = L'…';
            //} else if (conv == (size_t)-1) {
            //    /* invalid sequence was encountered */
            //    err("invalid sequence when converting %s to wide string", v);
            //    swprintf(node->media_name, ARRAY_SIZE(node->media_name), L"INVALID");
            //} /* else, conversion succeeded */
            snprintf(node->media_name, sizeof(node->media_name), "%s", v);
        } else if (STREQ(k, PW_KEY_NODE_NAME)) {
            //size_t conv = mbstowcs(node->application_name, v, ARRAY_SIZE(node->application_name));
            //if (conv == ARRAY_SIZE(node->application_name)) {
            //    /* not enough space for string, add ellipsis and null-terminate */
            //    node->application_name[ARRAY_SIZE(node->application_name) - 1] = L'\0';
            //    node->application_name[ARRAY_SIZE(node->application_name) - 2] = L'…';
            //} else if (conv == (size_t)-1) {
            //    /* invalid sequence was encountered */
            //    err("invalid sequence when converting %s to wide string", v);
            //    swprintf(node->application_name, ARRAY_SIZE(node->application_name), L"INVALID");
            //} /* else, conversion succeeded */
            snprintf(node->application_name, sizeof(node->application_name), "%s", v);
        }
    }

    if (info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) {
        /* DO NOT CHANGE THIS CODE. Stolen from pw-dump.c, I have no idea what it does */
        for (i = 0; i < info->n_params; i++) {
            uint32_t id = info->params[i].id;

            if (info->params[i].user == 0) {
                continue;
            }
            info->params[i].user = 0;

            if (!(info->params[i].flags & SPA_PARAM_INFO_READ)) {
                continue;
            }

            int res = pw_node_enum_params(node->pw_node, ++info->params[i].seq, id, 0, -1, NULL);
            if (SPA_RESULT_IS_ASYNC(res)) {
                info->params[i].seq = res;
            }
        }
    }
}

static void on_node_param(void *data, int seq, uint32_t id, uint32_t index,
                          uint32_t next, const struct spa_pod *param) {
    struct node *node = data;

    debug("node %d param: id %d seq %d index %d next %d param %p",
          node->id, id, seq, index, next, (void *)param);

    /* Parsing will fail sometimes. Ignore it. */
    if (param->type == SPA_TYPE_Object
        && ((struct spa_pod_object *)param)->body.type == SPA_TYPE_OBJECT_Props) {

        if (parse_node_props(param, &node->props) < 0) {
            //put_pod(&pw, NULL, param);
        }
    }
}

static const struct pw_node_events node_events = {
    PW_VERSION_NODE_EVENTS,
    .info = on_node_info,
    .param = on_node_param,
};

static void on_registry_global(void *data, uint32_t id, uint32_t permissions,
                               const char *type, uint32_t version,
                               const struct spa_dict *props) {
    debug("registry global: id %d, perms "PW_PERMISSION_FORMAT", ver %d, type %s",
          id, PW_PERMISSION_ARGS(permissions), version, type);
    uint32_t i = 0;
    const struct spa_dict_item *item;
    spa_dict_for_each(item, props) {
        trace("%c---%s: %s", (++i == props->n_items ? '\\' : '|'), item->key, item->value);
    }

    if (STREQ(type, PW_TYPE_INTERFACE_Node)) {
        const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        enum media_class media_class_value;
        if (media_class == NULL) {
            debug("empty media.class, not binding");
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
            debug("not interested in media.class %s, not binding", media_class);
            return;
        }

        struct node *new_node = xcalloc(1, sizeof(struct node));

        new_node->id = id;
        new_node->media_class = media_class_value;
        new_node->pw_node = pw_registry_bind(pw.registry, id, type, PW_VERSION_NODE, 0);
        pw_node_add_listener(new_node->pw_node, &new_node->listener, &node_events, new_node);

        stbds_hmput(pw.nodes, new_node->id, new_node);
    }
}

static void on_registry_global_remove(void *data, uint32_t id) {
    debug("registry global remove: id %d", id);

    struct node *node;
    if ((node = stbds_hmget(pw.nodes, id)) != NULL) {
        stbds_hmdel(pw.nodes, id);
        node_cleanup(node);
    }
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = on_registry_global,
    .global_remove = on_registry_global_remove,
};

int pipewire_init(void) {
    pw_init(NULL, NULL);

    pw.main_loop = pw_main_loop_new(NULL /* properties */);
    pw.main_loop_loop = pw_main_loop_get_loop(pw.main_loop);
    pw.main_loop_loop_fd = pw_loop_get_fd(pw.main_loop_loop);

    pw.context = pw_context_new(pw.main_loop_loop, NULL, 0);
    if (pw.context == NULL) {
        err("failed to create pw_context: %s", strerror(errno));
        return -1;
    }

    pw.core = pw_context_connect(pw.context, NULL, 0);
    if (pw.core == NULL) {
        err("failed to connect to pipewire: %s", strerror(errno));
        return -1;
    }

    pw.registry = pw_core_get_registry(pw.core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(pw.registry, &pw.registry_listener, &registry_events, NULL);

    return 0;
}

void pipewire_cleanup(void) {
    struct node *node;
    size_t i;
    while ((i = stbds_hmlenu(pw.nodes)) > 0) {
        node = pw.nodes[i - 1].value;
        stbds_hmdel(pw.nodes, node->id);
        node_cleanup(node);
    }
    stbds_hmfree(pw.nodes);

    pw_proxy_destroy((struct pw_proxy *)pw.registry);
    pw_core_disconnect(pw.core);
    pw_context_destroy(pw.context);
    pw_main_loop_destroy(pw.main_loop);
    pw_deinit();
}

