#ifndef PARAM_PRINT_H
#define PARAM_PRINT_H

#include <stdint.h>

#include "pw.h"

void put_pod(struct pw *d, const char *key, const struct spa_pod *pod);
void put_params(struct pw *pw, const char *key, struct spa_param_info *params,
                uint32_t n_params, struct spa_list *list);

#endif /* #ifndef PARAM_PRINT_H */

