#pragma once

#define MAX(a, b) ({ const TYPEOF(a) _a = (a), _b = (b); _a > _b ? _a : _b; })
#define MIN(a, b) ({ const TYPEOF(a) _a = (a), _b = (b); _a < _b ? _a : _b; })

#define TYPEOF(x) __typeof__(x)

#define CONTAINER_OF(member_ptr, container_type, member_name) \
    ((container_type *)((char *)(member_ptr) - offsetof(container_type, member_name)))

#define SIZEOF_ARRAY(arr) (sizeof(arr) / sizeof((arr)[0]))

#define _3(a, b) a##b
#define _2(a, b) _3(a, b)
#define _ _2(_dummy_param_, __COUNTER__)

