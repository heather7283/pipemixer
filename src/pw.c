#include <wchar.h>
#include <spa/param/audio/raw.h>
#include <spa/pod/parser.h>
#include <spa/utils/result.h>
#include <spa/param/props.h>

#include "pw.h"
#include "macros.h"
#include "props.h"
#include "param_print.h"

static void node_cleanup(struct node *node) {
    spa_list_remove(&node->all_nodes_link);
    spa_list_remove(&node->link);

    pw_proxy_destroy((struct pw_proxy *)node->pw_node);

    if (node->info != NULL) {
        pw_node_info_free(node->info);
    }

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

        if (streq(k, PW_KEY_MEDIA_NAME)) {
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
        } else if (streq(k, PW_KEY_NODE_NAME)) {
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

    /* DO NOT REMOVE, EVERYTHING WILL BREAK. TODO: figure out what does this actually do */
    info = node->info = pw_node_info_update(node->info, info);
    if (info == NULL) {
        warn("node info: info is NULL after pw_node_info_update() (why?)");
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
            put_pod(node->pw, NULL, param);
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
    struct pw *pw = data;

    debug("registry global: id %d, perms "PW_PERMISSION_FORMAT", ver %d, type %s",
          id, PW_PERMISSION_ARGS(permissions), version, type);
    uint32_t i = 0;
    const struct spa_dict_item *item;
    spa_dict_for_each(item, props) {
        trace("%c---%s: %s", (++i == props->n_items ? '\\' : '|'), item->key, item->value);
    }

    if (streq(type, PW_TYPE_INTERFACE_Node)) {
        struct spa_list *target_list;

        const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        if (media_class == NULL) {
            debug("empty media.class, not binding");
            return;
        } else if (streq(media_class, "Audio/Source")) {
            target_list = &pw->audio_sources;
        } else if (streq(media_class, "Audio/Sink")) {
            target_list = &pw->audio_sinks;
        } else if (streq(media_class, "Stream/Input/Audio")) {
            target_list = &pw->audio_input_streams;
        } else if (streq(media_class, "Stream/Output/Audio")) {
            target_list = &pw->audio_output_streams;
        } else {
            debug("not interested in media.class %s, not binding", media_class);
            return;
        }

        struct node *new_node = calloc(1, sizeof(struct node));
        if (new_node == NULL) {
            die("failed to allocate memory for node struct");
        }

        new_node->id = id;
        new_node->pw = pw;
        new_node->pw_node = pw_registry_bind(pw->registry, id, type, PW_VERSION_NODE, 0);
        pw_node_add_listener(new_node->pw_node, &new_node->listener, &node_events, new_node);

        spa_list_insert(target_list, &new_node->link);
        spa_list_insert(&pw->all_nodes, &new_node->all_nodes_link);
    }
}

static void on_registry_global_remove(void *data, uint32_t id) {
    struct pw *pw = data;

    debug("registry global remove: id %d", id);

    struct node *node = NULL;
    spa_list_for_each(node, &pw->all_nodes, all_nodes_link) {
        if (node->id == id) {
            node_cleanup(node);
            break;
        }
    }
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = on_registry_global,
    .global_remove = on_registry_global_remove,
};

int pipewire_init(struct pw *pw) {
    spa_list_init(&pw->all_nodes);
    spa_list_init(&pw->audio_sinks);
    spa_list_init(&pw->audio_sources);
    spa_list_init(&pw->audio_input_streams);
    spa_list_init(&pw->audio_output_streams);

    pw_init(NULL, NULL);

    pw->main_loop = pw_main_loop_new(NULL /* properties */);
    pw->main_loop_loop = pw_main_loop_get_loop(pw->main_loop);
    pw->main_loop_loop_fd = pw_loop_get_fd(pw->main_loop_loop);

    pw->context = pw_context_new(pw->main_loop_loop, NULL, 0);
    if (pw->context == NULL) {
        err("failed to create pw_context: %s", strerror(errno));
        return -1;
    }

    pw->core = pw_context_connect(pw->context, NULL, 0);
    if (pw->core == NULL) {
        err("failed to connect to pipewire: %s", strerror(errno));
        return -1;
    }

    pw->registry = pw_core_get_registry(pw->core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(pw->registry, &pw->registry_listener, &registry_events, pw);

    return 0;
}

void pipewire_cleanup(struct pw *pw) {
    struct node *node, *tmp_node;
    spa_list_for_each_safe(node, tmp_node, &pw->all_nodes, all_nodes_link) {
        node_cleanup(node);
    }
    pw_proxy_destroy((struct pw_proxy *)pw->registry);
    pw_core_disconnect(pw->core);
    pw_context_destroy(pw->context);
    pw_main_loop_destroy(pw->main_loop);
    pw_deinit();
}

