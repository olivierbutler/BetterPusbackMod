#ifndef GATE_ROUTE_CACHE_H
#define GATE_ROUTE_CACHE_H

#include <acfutils/airportdb.h>
#include <acfutils/geom.h>
#include <acfutils/list.h>
#include <acfutils/types.h>

#include "driving.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GATE_ROUTE_CACHE_DIRECTORY "BetterPushback_Gate_Routes"
#define GATE_ROUTE_CACHE_SLOT_COUNT 2
#define GATE_ROUTE_DIRECTION_LEN 4

typedef struct {
    bool_t recognized;
    char airport[AIRPORTDB_IDENT_LEN];
    char ramp[32];
    geo_pos2_t anchor_geo;
    double anchor_hdg;
    vect2_t anchor_local;
    double frame_hdg;
    char aircraft[256];
    double wheelbase;
    double nw_z;
    double main_z;
} gate_route_context_t;

typedef struct {
    bool_t valid;
    unsigned slot;
    unsigned segment_count;
    double final_aircraft_hdg;
    double final_tail_hdg;
    char tail_direction[GATE_ROUTE_DIRECTION_LEN];
} gate_route_slot_info_t;

void gate_route_context_reset(gate_route_context_t *context);

bool_t gate_route_find_published_start(airportdb_t *db,
    geo_pos2_t live_nosewheel_geo, vect2_t live_nosewheel_local,
    double live_heading, const char *aircraft, double wheelbase,
    double nw_z, double main_z, gate_route_context_t *context,
    double *match_distance, double *match_heading);

unsigned gate_route_cache_list(const gate_route_context_t *context,
    gate_route_slot_info_t slots[GATE_ROUTE_CACHE_SLOT_COUNT]);

bool_t gate_route_cache_load(const gate_route_context_t *context,
    unsigned slot, list_t *segs, gate_route_slot_info_t *info);

bool_t gate_route_cache_save(const gate_route_context_t *context,
    unsigned slot, const list_t *segs, gate_route_slot_info_t *info);

#ifdef __cplusplus
}
#endif

#endif
