#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef bool bool_t;
#define B_FALSE false
typedef struct { bool present[3], value[3]; } conf_t;
static conf_t config;
static conf_t *bp_conf = &config;
static const char *aircraft = "B738.acf";
static const char *keys[] = {
    "per_aircraft_is_global", "disco_when_done", "disco_when_done_B738_acf"
};

static unsigned key_index(const char *key) {
    for (unsigned i = 0; i < 3; ++i)
        if (strcmp(key, keys[i]) == 0) return i;
    assert(false);
    return 0;
}
static bool conf_get_b(conf_t *conf, const char *key, bool_t *value) {
    unsigned i = key_index(key);
    if (!conf->present[i]) return false;
    *value = conf->value[i];
    return true;
}
static void conf_set_b(conf_t *conf, const char *key, bool_t value) {
    unsigned i = key_index(key);
    conf->present[i] = true;
    conf->value[i] = value;
}
static void *safe_malloc(size_t size) {
    void *result = malloc(size);
    assert(result != NULL);
    return result;
}
static void XPLMGetNthAircraftModel(int index, char *name, char *path) {
    assert(index == 0);
    strcpy(name, aircraft);
    strcpy(path, "");
}

#include "per_aircraft_preferences.inc"

int main(void) {
    /* Absent and explicit false overrides must not be conflated. */
    for (unsigned global_present = 0; global_present < 2; ++global_present)
        for (unsigned global_value = 0; global_value < 2; ++global_value)
            for (unsigned acf_present = 0; acf_present < 2; ++acf_present)
                for (unsigned acf_value = 0; acf_value < 2; ++acf_value) {
                    memset(&config, 0, sizeof(config));
                    config.present[1] = global_present;
                    config.value[1] = global_value;
                    config.present[2] = acf_present;
                    config.value[2] = acf_value;
                    bool value = false;
                    bool found = conf_get_b_per_acf("disco_when_done", &value);
                    assert(found == !!(acf_present || global_present));
                    assert(value == (acf_present ? !!acf_value :
                        global_present && global_value));
                    conf_set_b_per_acf("disco_when_done", !value);
                    assert(config.present[2] && config.value[2] == !value);
                    assert(config.present[1] == !!global_present);
                    assert(config.value[1] == !!global_value);
                    bool saved = value;
                    assert(conf_get_b_per_acf("disco_when_done", &saved));
                    assert(saved == !value);
                    aircraft = "";
                    saved = false;
                    assert(conf_get_b_per_acf("disco_when_done", &saved)
                        == !!global_present);
                    assert(saved == !!(global_present && global_value));
                    aircraft = "B738.acf";
                    conf_set_b(&config, "per_aircraft_is_global", true);
                    saved = false;
                    assert(conf_get_b_per_acf("disco_when_done", &saved)
                        == !!global_present);
                    assert(saved == !!(global_present && global_value));
                    conf_set_b_per_acf("disco_when_done", false);
                    assert(config.present[1] && !config.value[1]);
                    assert(config.value[2] == !value);
                }
    puts("Per-aircraft completion, saved overrides and global fallback passed.");
    return 0;
}
