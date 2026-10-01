#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "interface_mode.h"

typedef bool bool_t;
#define B_TRUE true
#define B_FALSE false
typedef struct { bool present[6]; int value[6]; unsigned writes; } conf_t;
static conf_t config;
static conf_t *bp_conf = &config;
static const char *keys[] = { "classic_mode", BP_INTERFACE_MODE_CONFIG_KEY,
    "disco_when_done", "display_marshaller", "legacy_route_recall",
    "fast_ground_handling" };

static unsigned key_index(const char *key) {
    for (unsigned i = 0; i < 6; ++i)
        if (strcmp(key, keys[i]) == 0) return i;
    assert(false);
    return 0;
}
static bool conf_get_i(conf_t *conf, const char *key, int *value) {
    unsigned i = key_index(key);
    if (!conf->present[i]) return false;
    *value = conf->value[i];
    return true;
}
static bool conf_get_b(conf_t *conf, const char *key, bool_t *value) {
    int stored;
    if (!conf_get_i(conf, key, &stored)) return false;
    *value = stored != 0;
    return true;
}
static void conf_set_i(conf_t *conf, const char *key, int value) {
    unsigned i = key_index(key);
    conf->present[i] = true;
    conf->value[i] = value;
    ++conf->writes;
}
static void conf_set_b(conf_t *conf, const char *key, bool_t value) {
    conf_set_i(conf, key, value);
}

#include "feature_preferences.inc"

int main(void) {
    for (unsigned mask = 0; mask < 32; ++mask) {
        for (unsigned values = 0; values < 32; ++values) {
            memset(&config, 0, sizeof(config));
            config.present[0] = true;
            config.value[0] = true;
            for (unsigned i = 1; i < 6; ++i) {
                config.present[i] = !!(mask & (1u << (i - 1)));
                config.value[i] = !!(values & (1u << (i - 1)));
            }
            conf_t before = config;
            assert(migrate_classic_preferences());
            const int defaults[] = { 0, BP_INTERFACE_MODE_LEGACY_MAGIC_SQUARES,
                true, false, true };
            for (unsigned i = 1; i < 5; ++i) {
                assert(config.present[i]);
                assert(config.value[i] == (before.present[i]
                    ? before.value[i] : defaults[i]));
            }
            assert(!config.value[0]);
            assert(config.present[5] == before.present[5]);
            assert(config.value[5] == before.value[5]);
            unsigned writes = config.writes;
            assert(!migrate_classic_preferences());
            assert(config.writes == writes);
            /* Later explicit choices survive reload; no preset is reapplied. */
            conf_set_i(&config, BP_INTERFACE_MODE_CONFIG_KEY, 0);
            conf_set_b(&config, "legacy_route_recall", false);
            assert(!migrate_classic_preferences());
            assert(config.value[1] == 0 && !bp_legacy_routes());
        }
    }
    for (unsigned classic_present = 0; classic_present < 2; ++classic_present) {
        memset(&config, 0, sizeof(config));
        config.present[0] = classic_present;
        assert(!migrate_classic_preferences() && config.writes == 0);
        assert(!bp_legacy_routes() && !bp_fast_ground_handling());
    }
    for (unsigned mask = 0; mask < 64; ++mask) {
        memset(&config, 0, sizeof(config));
        for (unsigned i = 0; i < 6; ++i) {
            config.present[i] = true;
            config.value[i] = !!(mask & (1u << i));
        }
        assert(bp_legacy_routes() == !!(mask & 16));
        assert(bp_fast_ground_handling() == !!(mask & 32));
    }
    bp_conf = NULL;
    assert(!bp_legacy_routes() && !bp_fast_ground_handling());
    puts("Preference migration and independent settings tests passed.");
    return 0;
}
