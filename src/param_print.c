#include <alloca.h>
#include <stdio.h>
#include <stdarg.h>
#include <spa/utils/result.h>
#include <spa/utils/string.h>
#include <spa/pod/iter.h>
#include <spa/debug/types.h>
#include <spa/utils/json.h>
#include <spa/utils/ansi.h>
#include <spa/utils/string.h>

#include "pw.h"

#define INDENT 2

static bool colors = false;

#define NORMAL	(colors ? SPA_ANSI_RESET : "")
#define LITERAL	(colors ? SPA_ANSI_BRIGHT_MAGENTA : "")
#define NUMBER	(colors ? SPA_ANSI_BRIGHT_CYAN : "")
#define STRING	(colors ? SPA_ANSI_BRIGHT_GREEN : "")
#define KEY	(colors ? SPA_ANSI_BRIGHT_BLUE : "")

#define STATE_KEY	(1<<0)
#define STATE_COMMA	(1<<1)
#define STATE_FIRST	(1<<2)
#define STATE_MASK	0xffff0000
#define STATE_SIMPLE	(1<<16)
static uint32_t state = 0;

static int level = 0;

static void put_key(struct pw *d, const char *key);

static SPA_PRINTF_FUNC(3,4) void put_fmt(struct pw *d, const char *key, const char *fmt, ...)
{
	va_list va;
	if (key)
		put_key(d, key);
	fprintf(stderr, "%s%s%*s",
			state & STATE_COMMA ? "" : "",
			state & (STATE_MASK | STATE_KEY) ? " " : state & STATE_FIRST ? "" : "\n",
			state & (STATE_MASK | STATE_KEY) ? 0 : level, "");
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	state = (state & STATE_MASK) + STATE_COMMA;
}

static void put_key(struct pw *d, const char *key)
{
	int size = (strlen(key) + 1) * 4;
	char *str = alloca(size);
	spa_json_encode_string(str, size, key);
	put_fmt(d, NULL, "%s%s%s:", KEY, str, NORMAL);
	state = (state & STATE_MASK) + STATE_KEY;
}

static void put_begin(struct pw *d, const char *key, const char *type, uint32_t flags)
{
	put_fmt(d, key, "%s", type);
	level += INDENT;
	state = (state & STATE_MASK) + (flags & STATE_SIMPLE);
}

static void put_end(struct pw *d, const char *type, uint32_t flags)
{
	level -= INDENT;
	state = state & STATE_MASK;
	put_fmt(d, NULL, "%s", type);
	state = (state & STATE_MASK) + STATE_COMMA - (flags & STATE_SIMPLE);
}

static void put_encoded_string(struct pw *d, const char *key, const char *val)
{
	put_fmt(d, key, "%s%s%s", STRING, val, NORMAL);
}
static void put_string(struct pw *d, const char *key, const char *val)
{
	int size = (strlen(val) + 1) * 4;
	char *str = alloca(size);
	spa_json_encode_string(str, size, val);
	put_encoded_string(d, key, str);
}

static void put_literal(struct pw *d, const char *key, const char *val)
{
	put_fmt(d, key, "%s%s%s", LITERAL, val, NORMAL);
}

static void put_int(struct pw *d, const char *key, int64_t val)
{
	put_fmt(d, key, "%s%"PRIi64"%s", NUMBER, val, NORMAL);
}

static void put_double(struct pw *d, const char *key, double val)
{
	char buf[128];
	put_fmt(d, key, "%s%s%s", NUMBER,
			spa_json_format_float(buf, sizeof(buf), (float)val), NORMAL);
}

static void put_value(struct pw *d, const char *key, const char *val)
{
	int64_t li;
	float fv;

	if (val == NULL)
		put_literal(d, key, "null");
	else if (spa_streq(val, "true") || spa_streq(val, "false"))
		put_literal(d, key, val);
	else if (spa_atoi64(val, &li, 10))
		put_int(d, key, li);
	else if (spa_json_parse_float(val, strlen(val), &fv))
		put_double(d, key, fv);
	else
		put_string(d, key, val);
}

static void put_dict(struct pw *d, const char *key, struct spa_dict *dict)
{
	const struct spa_dict_item *it;
	spa_dict_qsort(dict);
	put_begin(d, key, "{", 0);
	spa_dict_for_each(it, dict)
		put_value(d, it->key, it->value);
	put_end(d, "}", 0);
}

static void put_pod_value(struct pw *d, const char *key, const struct spa_type_info *info,
		uint32_t type, void *body, uint32_t size)
{
	if (key)
		put_key(d, key);
	switch (type) {
	case SPA_TYPE_Bool:
		put_value(d, NULL, *(int32_t*)body ? "true" : "false");
		break;
	case SPA_TYPE_Id:
	{
		const char *str;
		char fallback[32];
		uint32_t id = *(uint32_t*)body;
		str = spa_debug_type_find_short_name(info, *(uint32_t*)body);
		if (str == NULL) {
			snprintf(fallback, sizeof(fallback), "id-%08x", id);
			str = fallback;
		}
		put_value(d, NULL, str);
		break;
	}
	case SPA_TYPE_Int:
		put_int(d, NULL, *(int32_t*)body);
		break;
	case SPA_TYPE_Fd:
	case SPA_TYPE_Long:
		put_int(d, NULL, *(int64_t*)body);
		break;
	case SPA_TYPE_Float:
		put_double(d, NULL, *(float*)body);
		break;
	case SPA_TYPE_Double:
		put_double(d, NULL, *(double*)body);
		break;
	case SPA_TYPE_String:
		put_string(d, NULL, (const char*)body);
		break;
	case SPA_TYPE_Rectangle:
	{
		struct spa_rectangle *r = (struct spa_rectangle *)body;
		put_begin(d, NULL, "{", STATE_SIMPLE);
		put_int(d, "width", r->width);
		put_int(d, "height", r->height);
		put_end(d, "}", STATE_SIMPLE);
		break;
	}
	case SPA_TYPE_Fraction:
	{
		struct spa_fraction *f = (struct spa_fraction *)body;
		put_begin(d, NULL, "{", STATE_SIMPLE);
		put_int(d, "num", f->num);
		put_int(d, "denom", f->denom);
		put_end(d, "}", STATE_SIMPLE);
		break;
	}
	case SPA_TYPE_Array:
	{
		struct spa_pod_array_body *b = (struct spa_pod_array_body *)body;
		void *p;
		info = info && info->values ? info->values: info;
		put_begin(d, NULL, "[", STATE_SIMPLE);
		SPA_POD_ARRAY_BODY_FOREACH(b, size, p)
			put_pod_value(d, NULL, info, b->child.type, p, b->child.size);
		put_end(d, "]", STATE_SIMPLE);
		break;
	}
	case SPA_TYPE_Choice:
	{
		struct spa_pod_choice_body *b = (struct spa_pod_choice_body *)body;
		int index = 0;

		if (b->type == SPA_CHOICE_None) {
			put_pod_value(d, NULL, info, b->child.type,
					SPA_POD_CONTENTS(struct spa_pod, &b->child),
					b->child.size);
		} else {
			static const char * const range_labels[] = { "default", "min", "max", NULL };
			static const char * const step_labels[] = { "default", "min", "max", "step", NULL };
			static const char * const enum_labels[] = { "default", "alt%u" };
			static const char * const flags_labels[] = { "default", "flag%u" };

			const char * const *labels;
			const char *label;
			char buffer[64];
			int max_labels, flags = 0;
			void *p;

			switch (b->type) {
			case SPA_CHOICE_Range:
				labels = range_labels;
				max_labels = 3;
				flags |= STATE_SIMPLE;
				break;
			case SPA_CHOICE_Step:
				labels = step_labels;
				max_labels = 4;
				flags |= STATE_SIMPLE;
				break;
			case SPA_CHOICE_Enum:
				labels = enum_labels;
				max_labels = 1;
				break;
			case SPA_CHOICE_Flags:
				labels = flags_labels;
				max_labels = 1;
				break;
			default:
				labels = NULL;
				break;
			}
			if (labels == NULL)
				break;

			put_begin(d, NULL, "{", flags);
			SPA_POD_CHOICE_BODY_FOREACH(b, size, p) {
				if ((label = labels[SPA_CLAMP(index, 0, max_labels)]) == NULL)
					break;
				snprintf(buffer, sizeof(buffer), label, index);
				put_pod_value(d, buffer, info, b->child.type, p, b->child.size);
				index++;
			}
			put_end(d, "}", flags);
		}
		break;
	}
	case SPA_TYPE_Object:
	{
		put_begin(d, NULL, "{", 0);
		struct spa_pod_object_body *b = (struct spa_pod_object_body *)body;
		struct spa_pod_prop *p;
		const struct spa_type_info *ti, *ii;

		ti = spa_debug_type_find(info, b->type);
		ii = ti ? spa_debug_type_find(ti->values, 0) : NULL;
		ii = ii ? spa_debug_type_find(ii->values, b->id) : NULL;

		info = ti ? ti->values : info;

		SPA_POD_OBJECT_BODY_FOREACH(b, size, p) {
			char fallback[32];
			const char *name;

			ii = spa_debug_type_find(info, p->key);
			name = ii ? spa_debug_type_short_name(ii->name) : NULL;
			if (name == NULL) {
				snprintf(fallback, sizeof(fallback), "id-%08x", p->key);
				name = fallback;
			}
			put_pod_value(d, name,
					ii ? ii->values : NULL,
					p->value.type,
					SPA_POD_CONTENTS(struct spa_pod_prop, p),
					p->value.size);
		}
		put_end(d, "}", 0);
		break;
	}
	case SPA_TYPE_Struct:
	{
		struct spa_pod *b = (struct spa_pod *)body, *p;
		put_begin(d, NULL, "[", 0);
		SPA_POD_FOREACH(b, size, p)
			put_pod_value(d, NULL, info, p->type, SPA_POD_BODY(p), p->size);
		put_end(d, "]", 0);
		break;
	}
	case SPA_TYPE_None:
		put_value(d, NULL, NULL);
		break;
	}
}

void put_pod(struct pw *d, const char *key, const struct spa_pod *pod)
{
	if (pod == NULL) {
		put_value(d, key, NULL);
	} else {
		put_pod_value(d, key, SPA_TYPE_ROOT,
				SPA_POD_TYPE(pod),
				SPA_POD_BODY(pod),
				SPA_POD_BODY_SIZE(pod));
	}
    fprintf(stderr, "\n");
}

