#include <assert.h>
#include <setjmp.h>

#include "format.h"
#include "xmalloc.h"

struct parser {
    const char *src, *pos;
    char *error;
    jmp_buf jmp_buf;
};

#define PARSER_ERROR(pp, fmt, ...) do { \
    xasprintf(&(pp)->error, "at %u: " fmt, (pp)->pos - (pp)->src + 1, ##__VA_ARGS__); \
    longjmp((pp)->jmp_buf, 67); \
} while (0)

static struct format_node *parse_node(struct parser *p);
static void format_node_free_contents(struct format_node *node);

static char *parse_key(struct parser *p) {
    size_t len = 0;
    size_t bufsz = 32;
    char *buf = xmalloc(bufsz);

    for (char c = *p->pos; (c >= 'a' && c <= 'z') || c == '.'; c = *(++p->pos)) {
        if (len + 2 > bufsz) {
            bufsz *= 2;
            buf = xrealloc(buf, bufsz);
        }
        buf[len++] = c;
    }

    if (!len) {
        free(buf);
        if (!*p->pos) {
            PARSER_ERROR(p, "unexpected EOF, expected key", *p->pos);
        } else {
            PARSER_ERROR(p, "expected key, got %c", *p->pos);
        }
    }

    buf[len] = '\0';
    return buf;
}

static struct format_node *parse_subst(struct parser *p) {
    assert(*p->pos++ == '{');

    char *key = parse_key(p);

    struct format_node *if_true = NULL;
    if (*p->pos == '?') {
        p->pos += 1;
        if_true = parse_node(p);
    }

    struct format_node *if_false = NULL;
    if (*p->pos == '!') {
        p->pos += 1;
        if_false = parse_node(p);
    }

    if (*p->pos != '}') {
        format_node_free_contents(if_true);
        free(if_true);
        format_node_free_contents(if_false);
        free(if_false);
        PARSER_ERROR(p, "expectdd }, got %c", *p->pos);
    }
    p->pos += 1;

    struct format_node *n = xmalloc(sizeof(*n));
    *n = (struct format_node){
        .type = FORMAT_NODE_SUBST,
        .as.subst = {
            .key = key,
            .if_true = if_true,
            .if_false = if_false,
        },
    };

    return n;
}

static bool is_escapable(char c) {
    return ((c == '{') || (c == '}') || (c == '?') || (c == '!') || (c == '\\'));
}

static struct format_node *parse_literal(struct parser *p) {
    size_t len = 0;
    size_t bufsz = 32;
    char *buf = xmalloc(bufsz);

    while (*p->pos) {
        char c = *p->pos;
        if (c == '\\') {
            c = *(++p->pos);
            if (!c) {
                PARSER_ERROR(p, "trailing backslash");
            } else if (!is_escapable(c)) {
                PARSER_ERROR(p, "invalid escaped character");
            }
        } else if (is_escapable(c)) {
            break;
        }

        if (len + 2 > bufsz) {
            bufsz *= 2;
            buf = xrealloc(buf, bufsz);
        }

        buf[len++] = c;
        p->pos += 1;
    }

    if (!len) {
        free(buf);
        if (!*p->pos) {
            PARSER_ERROR(p, "unexpected EOF, expected literal", *p->pos);
        } else {
            PARSER_ERROR(p, "expected literal, got %c", *p->pos);
        }
    }

    buf[len] = '\0';

    struct format_node *n = xmalloc(sizeof(*n));
    *n = (struct format_node){
        .type = FORMAT_NODE_LITERAL,
        .as.literal = {
            .str = buf,
        },
    };

    return n;
}

static struct format_node *parse_node(struct parser *p) {
    if (*p->pos == '{') {
        return parse_subst(p);
    } else {
        return parse_literal(p);
    }
}

static void format_node_free_contents(struct format_node *node) {
    if (!node) {
        return;
    }

    switch (node->type) {
    case FORMAT_NODE_LITERAL:
        free(node->as.literal.str);
        break;
    case FORMAT_NODE_SUBST:
        free(node->as.subst.key);
        struct format_node *a = node->as.subst.if_true;
        struct format_node *b = node->as.subst.if_false;
        format_node_free_contents(a);
        format_node_free_contents(b);
        free(a);
        free(b);
        break;
    }
}

struct format *format_parse(const char *src, char **error) {
    struct parser p = {
        .src = src,
        .pos = src,
    };

    struct format *f = xzalloc(sizeof(*f));

    if (setjmp(p.jmp_buf)) {
        /* abnormal return */
        format_free(f);
        if (error) {
            *error = p.error;
        }
        return NULL;
    }

    while (*p.pos != '\0') {
        struct format_node *node = parse_node(&p);
        f = xrealloc(f, sizeof(*f) + sizeof(f->nodes[0]) * ++f->nodes_count);
        f->nodes[f->nodes_count - 1] = node;
    }

    if (error) {
        *error = NULL;
    }
    return f;
}

static void format_node_render(const struct format_node *node,
                               const struct dict *dict, struct wstring *res) {
    switch (node->type) {
    case FORMAT_NODE_LITERAL:
        wstring_printf(res, L"%s", node->as.literal.str);
        break;
    case FORMAT_NODE_SUBST:;
        const char *val = dict_get(dict, node->as.subst.key);
        const struct format_node *if_true = node->as.subst.if_true;
        const struct format_node *if_false = node->as.subst.if_false;
        if (!if_true && !if_false) {
            if (val) {
                wstring_printf(res, L"%s", val);
            }
        } else if (if_true) {
            if (val) {
                format_node_render(if_true, dict, res);
            }
        } else if (if_false) {
            if (!val) {
                format_node_render(if_false, dict, res);
            }
        } else {
            if (val) {
                format_node_render(if_true, dict, res);
            } else {
                format_node_render(if_false, dict, res);
            }
        }
        break;
    }
}

void format_render(const struct format *fmt, const struct dict *dict, struct wstring *res) {
    if (!fmt) {
        return;
    }

    for (unsigned i = 0; i < fmt->nodes_count; i++) {
        const struct format_node *node = fmt->nodes[i];
        format_node_render(node, dict, res);
    }
}

void format_free(struct format *fmt) {
    if (!fmt) {
        return;
    }

    for (unsigned i = 0; i < fmt->nodes_count; i++) {
        struct format_node *node = fmt->nodes[i];
        format_node_free_contents(node);
    }
    free(fmt);
}

