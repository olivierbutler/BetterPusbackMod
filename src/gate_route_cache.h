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

#define GATE_ROUTE_CACHE_FILENAME "BetterPushback_gate_routes_v1.dat"

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

void gate_route_context_reset(gate_route_context_t *context);

bool_t gate_route_find_published_start(airportdb_t *db,
    geo_pos2_t live_nosewheel_geo, vect2_t live_nosewheel_local,
    double live_heading, const char *aircraft, double wheelbase,
    double nw_z, double main_z, gate_route_context_t *context,
    double *match_distance, double *match_heading);

bool_t gate_route_cache_load(const gate_route_context_t *context,
    list_t *segs);

bool_t gate_route_cache_save(const gate_route_context_t *context,
    const list_t *segs);

#ifdef __cplusplus
}
#endif

#endif
