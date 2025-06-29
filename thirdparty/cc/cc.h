#ifndef THIRDPARTY_CC_CC_H
#define THIRDPARTY_CC_CC_H

#define CC_REALLOC xrealloc
#define CC_FREE free
#define CC_NO_SHORT_NAMES

#include "thirdparty/cc/real_cc.h"

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
    #define typeof(expr) __typeof__(expr)
#endif

/* useful wrappers around cc functions */
#define cc_is_empty(cntr) (cc_size(cntr) == 0)

#define cc_getv(pvar, cntr, key) \
    ({ \
        typeof(pvar) tmp = cc_get(cntr, key); \
        (tmp == (void *)0) ? (false) : ((*(pvar)) = (*tmp), true); \
    })

#define cc_for_each_v(cntr, pvar) \
    for (typeof(pvar) tmp = cc_first(cntr); \
         (tmp != cc_end(cntr)) && (((*(pvar)) = (*tmp)), 1); \
         tmp = cc_next(cntr, tmp))

#define cc_r_for_each_v(cntr, pvar) \
    for (typeof(pvar) tmp = cc_last(cntr); \
         (tmp != cc_r_end(cntr)) && (((*(pvar)) = (*tmp)), 1); \
         tmp = cc_prev(cntr, tmp))

#endif /* #ifndef THIRDPARTY_CC_CC_H */

