#ifndef SRC_FILTER_H
#define SRC_FILTER_H

#include "pw/node.h"

enum node_filter_type {
    NODE_FILTER_NODE_NAME,
    NODE_FILTER_NODE_DESCRIPTION,
};

struct node_filter {
    enum node_filter_type type;
    char *pattern;
};

bool node_is_filtered(const struct node *node, const struct node_filter *filters, size_t n_filters);

#endif /* #ifndef SRC_FILTER_H */

