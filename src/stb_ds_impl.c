#include <stdlib.h>
#include "xmalloc.h"
#define STBDS_REALLOC(context, ptr, size) xrealloc(ptr, size)
#define STBDS_FREE(context, ptr) free(ptr)

#define STBDS_NO_SHORT_NAMES

#define STB_DS_IMPLEMENTATION
#include "lib/stb/stb_ds.h"

