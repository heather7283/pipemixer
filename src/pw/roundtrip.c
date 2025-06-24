#include "pw/roundtrip.h"
#include "log.h"
#include "xmalloc.h"

struct roundtrip_async_data {
    roundtrip_async_callback_func_t callback;
    void *data;
    int seq;

    struct spa_list link;
};

/* LIFO */
static struct spa_list callbacks = SPA_LIST_INIT(&callbacks);

static void on_core_done(void *data, uint32_t id, int seq) {
    struct roundtrip_async_data *d = spa_list_last(&callbacks, struct roundtrip_async_data, link);
    if (d->seq != seq) {
        err("LIFO error: expected seq %d got %d", d->seq, seq);
    } else {
        TRACE("roundtrip finished with seq %d", seq);

        if (d->callback != NULL) {
            d->callback(d->data);
        }
        spa_list_remove(&d->link);
        free(d);
    }
}

static const struct pw_core_events core_events = {
    .done = on_core_done,
};

void roundtrip_async(struct pw_core *core, roundtrip_async_callback_func_t callback, void *data) {
    static struct spa_hook listener;
    static bool initial_setup_done = false;

    if (!initial_setup_done) {
        initial_setup_done = true;
        pw_core_add_listener(core, &listener, &core_events, NULL);
    }

    struct roundtrip_async_data *d = xmalloc(sizeof(*d));
    d->callback = callback;
    d->data = data;
    d->seq = pw_core_sync(core, PW_ID_CORE, 0);
    TRACE("roundtrip started with seq %d", d->seq);

    spa_list_insert(&callbacks, &d->link);
}

