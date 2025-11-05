#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "pw/common.h"

void default_metadata_init(struct default_metadata *md, uint32_t id);
void default_metadata_cleanup(struct default_metadata *md);

bool default_metadata_check_default(struct default_metadata *md,
                                    const char *name, enum media_class media_class);

