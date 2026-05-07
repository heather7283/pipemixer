#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>
#include <wctype.h>
#include <stdio.h>

#include "format.h"
#include "collections/string.h"
#include "xmalloc.h"

/*
 * Grammar:
 * escapable = "{" | "}" | "?" | "!" | "\" ;
 * literal_char = ? Any printable unicode character except escapable ? | ( "\" escapable ) ;
 * literal = literal_char { literal_char } ;
 *
 * key_char = ? Any lowercase English letter ? | "." ;
 * key = key_char { key_char } ;
 *
 * subst = "{" key [ "?" [ format ] ] [ "!" format ] "}" ;
 * node = subst | literal ;
 * format = node { node } ;
 */

enum format_node_type {
    FORMAT_NODE_LITERAL,
    FORMAT_NODE_SUBST,
};

enum format_node_subst_type {
    FORMAT_NODE_SUBST_BASIC, /* {key} */
    FORMAT_NODE_SUBST_IF_EXISTS, /* {key?then} */
    FORMAT_NODE_SUBST_IF_ABSENT, /* {key!else} */
    FORMAT_NODE_SUBST_TERNARY, /* {key?then!else} */
    FORMAT_NODE_SUBST_FALLBACK, /* {key?!else} */
};

struct format_node {
    enum format_node_type type;
    union {
        struct {
            struct wstring str;
        } literal;
        struct format_node_subst {
            struct string key;
            enum format_node_subst_type type;
            struct format *if_true, *if_false;
        } subst;
    } as;
};

struct format {
    unsigned nodes_count;
    struct format_node *nodes[];
};

struct parser {
    const char *src;
    size_t pos, len;
    /* for error reporting */
    unsigned pos_chars;

    wchar_t wchar;
    bool has_wchar;
    mbstate_t mbstate;

    char *error;
    jmp_buf jmp_buf;
};

#define PARSER_ERROR(pp, fmt, ...) do { \
    xasprintf(&(pp)->error, "at %u: " fmt, (pp)->pos_chars, ##__VA_ARGS__); \
    longjmp((pp)->jmp_buf, 67); \
} while (0)

static void parse_format(struct parser *p, struct format **out);

static bool eof(struct parser *p) {
    return p->pos >= p->len && !p->has_wchar;
}

static void get_next_wchar(struct parser *p) {
    /* +1 is needed to include the null terminator and let mbrtowc read it */
    size_t ret = mbrtowc(&p->wchar, &p->src[p->pos], p->len - p->pos + 1, &p->mbstate);
    if (ret == (size_t)-1) {
        PARSER_ERROR(p, "%m");
    } else if (ret == (size_t)-2) {
        PARSER_ERROR(p, "incomplete multibyte sequence");
    } else if (ret != 0) {
        p->pos += ret;
        p->pos_chars += 1;
        p->has_wchar = true;
    }
}

static wchar_t peek(struct parser *p) {
    if (!p->has_wchar) {
        get_next_wchar(p);
    }

    return p->wchar;
}

static wchar_t consume(struct parser *p) {
    if (!p->has_wchar) {
        get_next_wchar(p);
    }

    p->has_wchar = false;
    return p->wchar;
}

static const char *format_wchar(wchar_t wc) {
    static char buf[sizeof("0xFFFFFFFF")];

    if (wc == L'\0') {
        strncpy(buf, "EOF", sizeof(buf));
    } else if (!iswgraph(wc)) {
        snprintf(buf, sizeof(buf), "0x%X", wc);
    } else {
        snprintf(buf, sizeof(buf), "%lc", wc);
    }

    return buf;
}

static void expect(struct parser *p, wchar_t expected) {
    wchar_t got = consume(p);
    if (got != expected) {
        PARSER_ERROR(p, "expected %lc, got %s", expected, format_wchar(got));
    }
}

static void parse_key(struct parser *p, struct string *out) {
    while (!eof(p)) {
        wchar_t c = peek(p);
        if ((c < L'a' || c > L'z') && c != L'.') {
            break;
        }
        string_appendwc(out, consume(p));
    }

    if (!out->len) {
        PARSER_ERROR(p, "expected key, got %s", format_wchar(peek(p)));
    }
}

static void parse_subst(struct parser *p, struct format_node **out) {
    struct format_node *n = *out = xzalloc(sizeof(*n));
    n->type = FORMAT_NODE_SUBST;
    struct format_node_subst *s = &n->as.subst;
    s->type = FORMAT_NODE_SUBST_BASIC;

    expect(p, L'{');

    parse_key(p, &s->key);

    if (peek(p) == L'?') {
        consume(p);
        if (peek(p) == L'!') {
            s->type = FORMAT_NODE_SUBST_FALLBACK;
        } else {
            s->type = FORMAT_NODE_SUBST_IF_EXISTS;
            parse_format(p, &s->if_true);
        }
    }

    if (peek(p) == L'!') {
        if (s->type == FORMAT_NODE_SUBST_BASIC) {
            s->type = FORMAT_NODE_SUBST_IF_ABSENT;
        } else if (s->type == FORMAT_NODE_SUBST_IF_EXISTS) {
            s->type = FORMAT_NODE_SUBST_TERNARY;
        }
        consume(p);
        parse_format(p, &s->if_false);
    }

    expect(p, L'}');
}

static bool is_escapable(wchar_t c) {
    return ((c == L'{') || (c == L'}') || (c == L'?') || (c == L'!') || (c == L'\\'));
}

static void parse_literal(struct parser *p, struct format_node **out) {
    struct format_node *n = *out = xzalloc(sizeof(*n));
    n->type = FORMAT_NODE_LITERAL;

    struct wstring *s = &n->as.literal.str;

    while (!eof(p)) {
        wchar_t c = peek(p);
        if (c == L'\\') {
            consume(p);
            c = peek(p);
            if (!is_escapable(c)) {
                PARSER_ERROR(p, "invalid escaped character: %s", format_wchar(c));
            }
        } else if (is_escapable(c)) {
            break;
        }

        wstring_appendwc(s, consume(p));
    }

    if (!s->len) {
        PARSER_ERROR(p, "expected literal, got %s", format_wchar(peek(p)));
    }
}

static void parse_node(struct parser *p, struct format_node **out) {
    if (peek(p) == L'{') {
        parse_subst(p, out);
    } else {
        parse_literal(p, out);
    }
}

static void parse_format(struct parser *p, struct format **out) {
    struct format *f = *out = xzalloc(sizeof(*f));

    while (!eof(p)) {
        wchar_t c = peek(p);
        if (c == L'!' || c == L'}') {
            break;
        }

        f = *out = xrealloc(f, sizeof(*f) + sizeof(f->nodes[0]) * ++f->nodes_count);
        parse_node(p, &f->nodes[f->nodes_count - 1]);
    }

    if (!f->nodes_count) {
        PARSER_ERROR(p, "unexpected character: %s", format_wchar(peek(p)));
    }
}

struct format *format_parse(const char *src, char **error) {
    struct parser p = {
        .src = src,
        .len = strlen(src),
    };

    struct format *f = NULL;

    if (setjmp(p.jmp_buf)) {
        /* abnormal return */
        format_free(f);
        if (error) {
            *error = p.error;
        }
        return NULL;
    }

    parse_format(&p, &f);
    if (!eof(&p)) {
        PARSER_ERROR(&p, "unexpected character: %s", format_wchar(peek(&p)));
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
        wstring_appendwsn(res, node->as.literal.str.data, node->as.literal.str.len);
        break;
    case FORMAT_NODE_SUBST:;
        const char *val = dict_get(dict, node->as.subst.key.data);
        const struct format *if_true = node->as.subst.if_true;
        const struct format *if_false = node->as.subst.if_false;
        switch (node->as.subst.type) {
        case FORMAT_NODE_SUBST_BASIC:
            if (val) {
                wstring_appendsz(res, val);
            }
            break;
        case FORMAT_NODE_SUBST_IF_EXISTS:
            if (val) {
                format_render(if_true, dict, res);
            }
            break;
        case FORMAT_NODE_SUBST_IF_ABSENT:
            if (!val) {
                format_render(if_false, dict, res);
            }
            break;
        case FORMAT_NODE_SUBST_TERNARY:
            if (val) {
                format_render(if_true, dict, res);
            } else {
                format_render(if_false, dict, res);
            }
            break;
        case FORMAT_NODE_SUBST_FALLBACK:
            if (val) {
                wstring_appendsz(res, val);
            } else {
                format_render(if_false, dict, res);
            }
            break;
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

static void format_node_free(struct format_node *node) {
    if (!node) {
        return;
    }

    switch (node->type) {
    case FORMAT_NODE_LITERAL:
        wstring_free(&node->as.literal.str);
        break;
    case FORMAT_NODE_SUBST:
        string_free(&node->as.subst.key);
        format_free(node->as.subst.if_true);
        format_free(node->as.subst.if_false);
        break;
    }

    free(node);
}

void format_free(struct format *fmt) {
    if (!fmt) {
        return;
    }

    for (unsigned i = 0; i < fmt->nodes_count; i++) {
        format_node_free(fmt->nodes[i]);
    }
    free(fmt);
}

