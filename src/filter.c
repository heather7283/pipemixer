#include <fnmatch.h>

#include "filter.h"
#include "log.h"

bool node_is_filtered(const struct node *node, const struct node_filter *filters, size_t n_filters) {
    for (size_t i = 0; i < n_filters; i++) {
        const struct node_filter *const f = &filters[i];
        switch (f->type) {
        case NODE_FILTER_NODE_NAME:
            if (node->node_name && fnmatch(f->pattern, node->node_name, 0) == 0) {
                INFO("node %d node.name matched against filter %s", node->id, f->pattern);
                return true;
            }
            break;
        case NODE_FILTER_NODE_DESCRIPTION:
            if (node->node_description && fnmatch(f->pattern, node->node_description, 0) == 0) {
                INFO("node %d node.description matched against filter %s", node->id, f->pattern);
                return true;
            }
            break;
        }
    }

    return false;
}

