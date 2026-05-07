#pragma once

#include "collections/dict.h"
#include "collections/string.h"
#include "collections/wstring.h"

/*
 * Grammar:
 * escapable = "{" | "}" | "?" | "!" ;
 * literal_char = ? Any printable unicode character except escapable ? | ( "\" escapable ) ;
 * literal = literal_char { literal_char } ;
 *
 * key_char = ? Any lowercase English letter ? | "." ;
 * key = key_char { key_char } ;
 *
 * subst = "{" key [ "?" node ] [ "!" node ] "}" ;
 *
 * node = subst | literal ;
 * format = node { node } ;
 */

enum format_node_type {
    FORMAT_NODE_LITERAL,
    FORMAT_NODE_SUBST,
};

struct format_node {
    enum format_node_type type;
    union {
        struct {
            struct wstring str;
        } literal;
        struct {
            struct string key;
            /* both can be NULL (in which case value for key is substituted) */
            struct format_node *if_true, *if_false;
        } subst;
    } as;
};

struct format {
    unsigned nodes_count;
    struct format_node *nodes[];
};

struct format *format_parse(const char *src, char **error);
void format_render(const struct format *format, const struct dict *dict, struct wstring *result);
void format_free(struct format *format);

