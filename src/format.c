#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>
#include <wctype.h>

#include "format.h"
#include "xmalloc.h"

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

static struct format_node *parse_node(struct parser *p);
static void format_node_free_contents(struct format_node *node);

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

static void expect(struct parser *p, wchar_t expected) {
    wchar_t got = consume(p);
    if (got != expected) {
        if (eof(p)) {
            PARSER_ERROR(p, "expected %lc, got EOF", expected);
        } else if (!iswgraph(got)) {
            PARSER_ERROR(p, "expected %lc, got 0x%X", expected, got);
        } else {
            PARSER_ERROR(p, "expected %lc, got %lc", expected, got);
        }
    }
}

static struct string parse_key(struct parser *p) {
    struct string s = {0};

    wchar_t c;
    while (!eof(p)) {
        c = peek(p);
        if (!(c >= L'a' && c <= L'z') || c == L'.') {
            break;
        }
        string_append(&s, consume(p));
    }

    if (!s.len) {
        string_free(&s);
        if (eof(p)) {
            PARSER_ERROR(p, "unexpected EOF, expected key");
        } else {
            PARSER_ERROR(p, "expected key, got %lc", c);
        }
    }

    return s;
}

static struct format_node *parse_subst(struct parser *p) {
    expect(p, L'{');

    struct string key = parse_key(p);

    struct format_node *if_true = NULL;
    if (peek(p) == L'?') {
        consume(p);
        if_true = parse_node(p);
    }

    struct format_node *if_false = NULL;
    if (peek(p) == L'!') {
        consume(p);
        if_false = parse_node(p);
    }

    // TODO: this leaks on failure
    expect(p, L'}');

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

static bool is_escapable(wchar_t c) {
    return ((c == L'{') || (c == L'}') || (c == L'?') || (c == L'!') || (c == L'\\'));
}

static struct format_node *parse_literal(struct parser *p) {
    struct wstring s = {0};

    wchar_t c;
    while (!eof(p)) {
        c = peek(p);
        if (c == L'\\') {
            consume(p);
            c = peek(p);
            if (!c) {
                PARSER_ERROR(p, "trailing backslash");
            } else if (!is_escapable(c)) {
                PARSER_ERROR(p, "invalid escaped character");
            }
        } else if (is_escapable(c)) {
            break;
        }

        wstring_append(&s, consume(p));
    }

    if (!s.len) {
        wstring_free(&s);
        if (eof(p)) {
            PARSER_ERROR(p, "unexpected EOF, expected literal");
        } else {
            PARSER_ERROR(p, "expected literal, got %lc", c);
        }
    }

    struct format_node *n = xmalloc(sizeof(*n));
    *n = (struct format_node){
        .type = FORMAT_NODE_LITERAL,
        .as.literal = {
            .str = s,
        },
    };

    return n;
}

static struct format_node *parse_node(struct parser *p) {
    if (peek(p) == L'{') {
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
        wstring_free(&node->as.literal.str);
        break;
    case FORMAT_NODE_SUBST:
        string_free(&node->as.subst.key);
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
        .len = strlen(src),
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

    while (!eof(&p)) {
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
        const char *val = dict_get(dict, node->as.subst.key.data);
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

