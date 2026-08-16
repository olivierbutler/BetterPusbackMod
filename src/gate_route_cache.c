#include "gate_route_cache.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if IBM
#include <windows.h>
#endif

#include <acfutils/helpers.h>
#include <acfutils/safe_alloc.h>

#include "gate_route_math.h"
#include "xplane.h"

#define GATE_ROUTE_CACHE_HEADER "### BetterPushback gate route slot v2 ###"
#define GATE_ROUTE_CACHE_MAX_SEGS 4096
#define GATE_ROUTE_ANCHOR_COORD_EPSILON 0.00000005
#define GATE_ROUTE_ANCHOR_HEADING_EPSILON 0.01
#define GATE_ROUTE_GEOMETRY_EPSILON 0.01
#define GATE_ROUTE_INITIAL_POSE_EPSILON 0.25
#define GATE_ROUTE_FINAL_HEADING_EPSILON 0.01
#define GATE_ROUTE_COMPONENT_LEN 80

typedef struct {
    bool_t active;
    bool_t valid;
    bool_t have_slot;
    bool_t have_airport;
    bool_t have_ramp;
    bool_t have_anchor;
    bool_t have_aircraft;
    bool_t have_geometry;
    bool_t have_label;
    bool_t have_final;
    bool_t have_count;
    unsigned slot;
    char airport[AIRPORTDB_IDENT_LEN];
    char ramp[32];
    geo_pos2_t anchor_geo;
    double anchor_hdg;
    char aircraft[256];
    double wheelbase;
    double nw_z;
    double main_z;
    char label[16];
    double final_aircraft_hdg;
    double final_tail_hdg;
    unsigned expected_count;
    unsigned actual_count;
    list_t segs;
} parsed_route_t;

static void
segment_list_clear(list_t *segs)
{
    seg_t *seg;

    while ((seg = list_remove_head(segs)) != NULL)
        free(seg);
}

static void
parsed_route_init(parsed_route_t *route)
{
    memset(route, 0, sizeof(*route));
    route->valid = B_TRUE;
    list_create(&route->segs, sizeof(seg_t), offsetof(seg_t, node));
}

static void
parsed_route_fini(parsed_route_t *route)
{
    segment_list_clear(&route->segs);
    list_destroy(&route->segs);
}

static void
strip_line_end(char *line)
{
    size_t len = strlen(line);

    while (len != 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
}

static bool_t
copy_line_value(char *destination, size_t capacity, const char *line,
    const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    size_t value_len;

    if (strncmp(line, prefix, prefix_len) != 0)
        return B_FALSE;
    value_len = strlen(line + prefix_len);
    if (value_len == 0 || value_len >= capacity)
        return B_FALSE;
    strlcpy(destination, line + prefix_len, capacity);
    return B_TRUE;
}

static unsigned
segment_list_count(const list_t *segs)
{
    unsigned count = 0;

    for (const seg_t *seg = list_head(segs); seg != NULL;
         seg = list_next(segs, seg)) {
        count++;
    }
    return count;
}

void
gate_route_context_reset(gate_route_context_t *context)
{
    ASSERT(context != NULL);
    memset(context, 0, sizeof(*context));
    context->anchor_geo = NULL_GEO_POS2;
    context->anchor_hdg = NAN;
    context->frame_hdg = NAN;
    context->wheelbase = NAN;
    context->nw_z = NAN;
    context->main_z = NAN;
}

bool_t
gate_route_find_published_start(airportdb_t *db,
    geo_pos2_t live_nosewheel_geo, vect2_t live_nosewheel_local,
    double live_heading, const char *aircraft, double wheelbase,
    double nw_z, double main_z, gate_route_context_t *context,
    double *match_distance, double *match_heading)
{
    vect3_t live_ecef;
    list_t *airports;
    gate_route_context_t candidate;
    double candidate_distance = NAN, candidate_heading = NAN;
    unsigned matches = 0;

    ASSERT(context != NULL);
    gate_route_context_reset(context);
    if (match_distance != NULL)
        *match_distance = NAN;
    if (match_heading != NULL)
        *match_heading = NAN;
    if (db == NULL || aircraft == NULL || aircraft[0] == '\0' ||
        IS_NULL_GEO_POS(live_nosewheel_geo) ||
        !isfinite(live_heading) || !isfinite(wheelbase) || wheelbase <= 0 ||
        !isfinite(nw_z) || !isfinite(main_z)) {
        return B_FALSE;
    }

    gate_route_context_reset(&candidate);
    live_ecef = geo2ecef_mtr(GEO_POS3(live_nosewheel_geo.lat,
        live_nosewheel_geo.lon, 0), &wgs84);
    load_nearest_airport_tiles(db, live_nosewheel_geo);
    airports = find_nearest_airports(db, live_nosewheel_geo);

    for (airport_t *airport = list_head(airports); airport != NULL;
         airport = list_next(airports, airport)) {
        for (ramp_start_t *ramp = avl_first(&airport->ramp_starts);
             ramp != NULL; ramp = AVL_NEXT(&airport->ramp_starts, ramp)) {
            vect3_t ramp_ecef = geo2ecef_mtr(GEO_POS3(ramp->pos.lat,
                ramp->pos.lon, 0), &wgs84);
            double distance = vect3_dist(live_ecef, ramp_ecef);
            double heading = gate_route_heading_delta(ramp->hdgt,
                live_heading);

            if (!gate_route_start_pose_matches(distance, heading))
                continue;
            matches++;
            candidate.recognized = B_TRUE;
            strlcpy(candidate.airport,
                airport->icao[0] != '\0' ? airport->icao : airport->ident,
                sizeof(candidate.airport));
            strlcpy(candidate.ramp, ramp->name, sizeof(candidate.ramp));
            candidate.anchor_geo = ramp->pos;
            candidate.anchor_hdg = ramp->hdgt;
            candidate.anchor_local = live_nosewheel_local;
            candidate.frame_hdg = live_heading;
            strlcpy(candidate.aircraft, aircraft,
                sizeof(candidate.aircraft));
            candidate.wheelbase = wheelbase;
            candidate.nw_z = nw_z;
            candidate.main_z = main_z;
            candidate_distance = distance;
            candidate_heading = heading;
        }
    }

    free_nearest_airport_list(airports);
    unload_distant_airport_tiles(db, NULL_GEO_POS2);
    if (matches != 1)
        return B_FALSE;

    *context = candidate;
    if (match_distance != NULL)
        *match_distance = candidate_distance;
    if (match_heading != NULL)
        *match_heading = candidate_heading;
    return B_TRUE;
}

static uint32_t
hash_string(uint32_t hash, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0';
         p++) {
        hash ^= *p;
        hash *= 16777619U;
    }
    hash ^= 0xffU;
    return hash * 16777619U;
}

static uint32_t
gate_identity_hash(const gate_route_context_t *context)
{
    char values[160];
    uint32_t hash = 2166136261U;

    hash = hash_string(hash, context->airport);
    hash = hash_string(hash, context->ramp);
    (void)snprintf(values, sizeof(values), "%lld|%lld|%lld",
        (long long)llround(context->anchor_geo.lat * 100000000.0),
        (long long)llround(context->anchor_geo.lon * 100000000.0),
        (long long)llround(context->anchor_hdg * 100.0));
    return hash_string(hash, values);
}

static uint32_t
aircraft_identity_hash(const gate_route_context_t *context)
{
    char values[160];
    uint32_t hash = hash_string(2166136261U, context->aircraft);

    (void)snprintf(values, sizeof(values), "%lld|%lld|%lld",
        (long long)llround(context->wheelbase * 1000.0),
        (long long)llround(context->nw_z * 1000.0),
        (long long)llround(context->main_z * 1000.0));
    return hash_string(hash, values);
}

static void
sanitize_component(const char *source, char *destination, size_t capacity)
{
    size_t output = 0;
    bool_t underscore = B_FALSE;

    ASSERT(source != NULL);
    ASSERT(destination != NULL);
    ASSERT(capacity != 0);
    for (const unsigned char *p = (const unsigned char *)source;
         *p != '\0' && output + 1 < capacity; p++) {
        if (isalnum(*p)) {
            destination[output++] = (char)*p;
            underscore = B_FALSE;
        } else if (!underscore && output != 0) {
            destination[output++] = '_';
            underscore = B_TRUE;
        }
    }
    while (output != 0 && destination[output - 1] == '_')
        output--;
    if (output == 0) {
        strlcpy(destination, "Unknown", capacity);
        return;
    }
    destination[output] = '\0';
}

static char *
gate_route_profile_directory(const gate_route_context_t *context)
{
    char airport[GATE_ROUTE_COMPONENT_LEN];
    char ramp[GATE_ROUTE_COMPONENT_LEN];
    char aircraft[GATE_ROUTE_COMPONENT_LEN];
    char gate_directory[GATE_ROUTE_COMPONENT_LEN + 16];
    char aircraft_directory[GATE_ROUTE_COMPONENT_LEN + 16];

    sanitize_component(context->airport, airport, sizeof(airport));
    sanitize_component(context->ramp, ramp, sizeof(ramp));
    sanitize_component(context->aircraft, aircraft, sizeof(aircraft));
    (void)snprintf(gate_directory, sizeof(gate_directory), "%s_%08X", ramp,
        gate_identity_hash(context));
    (void)snprintf(aircraft_directory, sizeof(aircraft_directory), "%s_%08X",
        aircraft, aircraft_identity_hash(context));
    return mkpathname(bp_xpdir, "Output", "caches",
        GATE_ROUTE_CACHE_DIRECTORY, airport, gate_directory,
        aircraft_directory, NULL);
}

static char *
gate_route_slot_path(const gate_route_context_t *context, unsigned slot,
    bool_t create_directory)
{
    char filename[32];
    char *directory, *path;
    bool_t is_directory = B_FALSE;

    if (!context->recognized || slot >= GATE_ROUTE_CACHE_SLOT_COUNT)
        return NULL;
    directory = gate_route_profile_directory(context);
    if (create_directory) {
        if (file_exists(directory, &is_directory)) {
            if (!is_directory) {
                free(directory);
                return NULL;
            }
        } else if (!create_directory_recursive(directory)) {
            free(directory);
            return NULL;
        }
    }
    (void)snprintf(filename, sizeof(filename), "slot_%u.route", slot + 1);
    path = mkpathname(directory, filename, NULL);
    free(directory);
    return path;
}

static bool_t
route_metadata_matches(const parsed_route_t *route,
    const gate_route_context_t *context, unsigned slot)
{
    return route->valid && route->have_slot && route->slot == slot &&
        route->have_airport && route->have_ramp &&
        route->have_anchor && route->have_aircraft && route->have_geometry &&
        route->have_label && route->have_final && route->have_count &&
        route->expected_count == route->actual_count && route->actual_count != 0 &&
        strcmp(route->airport, context->airport) == 0 &&
        strcmp(route->ramp, context->ramp) == 0 &&
        strcmp(route->aircraft, context->aircraft) == 0 &&
        fabs(route->anchor_geo.lat - context->anchor_geo.lat) <=
        GATE_ROUTE_ANCHOR_COORD_EPSILON &&
        fabs(route->anchor_geo.lon - context->anchor_geo.lon) <=
        GATE_ROUTE_ANCHOR_COORD_EPSILON &&
        fabs(gate_route_heading_delta(route->anchor_hdg,
        context->anchor_hdg)) <= GATE_ROUTE_ANCHOR_HEADING_EPSILON &&
        fabs(route->wheelbase - context->wheelbase) <=
        GATE_ROUTE_GEOMETRY_EPSILON &&
        fabs(route->nw_z - context->nw_z) <=
        GATE_ROUTE_GEOMETRY_EPSILON &&
        fabs(route->main_z - context->main_z) <=
        GATE_ROUTE_GEOMETRY_EPSILON;
}

static bool_t
route_final_metadata_matches(const parsed_route_t *route,
    const gate_route_context_t *context)
{
    const seg_t *last = list_tail(&route->segs);
    char direction[GATE_ROUTE_DIRECTION_LEN];
    char expected_label[16];
    double aircraft_heading, tail_heading;

    if (last == NULL)
        return B_FALSE;
    aircraft_heading = gate_route_heading_from_delta(context->anchor_hdg,
        last->end_hdg);
    tail_heading = gate_route_heading_from_delta(aircraft_heading, 180.0);
    gate_route_tail_direction(aircraft_heading, direction, sizeof(direction));
    (void)snprintf(expected_label, sizeof(expected_label), "Tail %s",
        direction);
    return fabs(gate_route_heading_delta(aircraft_heading,
        route->final_aircraft_hdg)) <= GATE_ROUTE_FINAL_HEADING_EPSILON &&
        fabs(gate_route_heading_delta(tail_heading,
        route->final_tail_hdg)) <= GATE_ROUTE_FINAL_HEADING_EPSILON &&
        strcmp(route->label, expected_label) == 0;
}

static void
copy_route_to_local(const parsed_route_t *source,
    const gate_route_context_t *context, list_t *destination)
{
    segment_list_clear(destination);
    for (const seg_t *relative = list_head(&source->segs); relative != NULL;
         relative = list_next(&source->segs, relative)) {
        seg_t *local = safe_calloc(1, sizeof(*local));

        *local = *relative;
        memset(&local->node, 0, sizeof(local->node));
        gate_route_point_from_relative(relative->start_pos.x,
            relative->start_pos.y, context->anchor_local.x,
            context->anchor_local.y, context->frame_hdg,
            &local->start_pos.x, &local->start_pos.y);
        gate_route_point_from_relative(relative->end_pos.x,
            relative->end_pos.y, context->anchor_local.x,
            context->anchor_local.y, context->frame_hdg,
            &local->end_pos.x, &local->end_pos.y);
        local->start_hdg = gate_route_heading_from_delta(
            context->frame_hdg, relative->start_hdg);
        local->end_hdg = gate_route_heading_from_delta(
            context->frame_hdg, relative->end_hdg);
        if (local->type == SEG_TYPE_STRAIGHT)
            local->len = vect2_dist(local->start_pos, local->end_pos);
        local->have_local_coords = B_TRUE;
        local->have_world_coords = B_FALSE;
        list_insert_tail(destination, local);
    }
}

static bool_t
route_initial_pose_matches(const gate_route_context_t *context,
    const list_t *segs)
{
    const seg_t *first = list_head(segs);
    double expected_x, expected_y;

    if (first == NULL)
        return B_FALSE;
    gate_route_point_from_relative(0, -context->wheelbase,
        context->anchor_local.x, context->anchor_local.y,
        context->frame_hdg, &expected_x, &expected_y);
    return hypot(first->start_pos.x - expected_x,
        first->start_pos.y - expected_y) <= GATE_ROUTE_INITIAL_POSE_EPSILON &&
        fabs(gate_route_heading_delta(context->frame_hdg,
        first->start_hdg)) <= GATE_ROUTE_START_HEADING_TOLERANCE_DEGREES;
}

static bool_t
parse_segment_line(const char *line, parsed_route_t *route)
{
    unsigned type, backward, user_placed, right;
    double start_x, start_y, start_hdg, end_x, end_y, end_hdg, value;
    seg_t *seg;

    if (sscanf(line, "seg %u %lf %lf %lf %lf %lf %lf %u %u %lf %u",
        &type, &start_x, &start_y, &start_hdg, &end_x, &end_y, &end_hdg,
        &backward, &user_placed, &value, &right) != 11 ||
        type > SEG_TYPE_TURN || backward > 1 || user_placed > 1 || right > 1 ||
        !isfinite(start_x) || !isfinite(start_y) || !isfinite(start_hdg) ||
        !isfinite(end_x) || !isfinite(end_y) || !isfinite(end_hdg) ||
        !isfinite(value) || route->actual_count >= GATE_ROUTE_CACHE_MAX_SEGS) {
        return B_FALSE;
    }
    if (type == SEG_TYPE_TURN && value <= 0)
        return B_FALSE;

    seg = safe_calloc(1, sizeof(*seg));
    seg->type = type;
    seg->start_pos = VECT2(start_x, start_y);
    seg->start_hdg = start_hdg;
    seg->end_pos = VECT2(end_x, end_y);
    seg->end_hdg = end_hdg;
    seg->backward = backward;
    seg->user_placed = user_placed;
    if (seg->type == SEG_TYPE_STRAIGHT)
        seg->len = value;
    else {
        seg->turn.r = value;
        seg->turn.right = right;
    }
    list_insert_tail(&route->segs, seg);
    route->actual_count++;
    return B_TRUE;
}

static bool_t
read_route_file(const char *filename, parsed_route_t *route)
{
    char *line = NULL;
    size_t line_capacity = 0;
    FILE *fp;
    bool_t header_ok = B_FALSE, ended = B_FALSE;

    parsed_route_init(route);
    route->active = B_TRUE;
    fp = fopen(filename, "r");
    if (fp == NULL)
        return B_FALSE;
    while (getline(&line, &line_capacity, fp) > 0) {
        strip_line_end(line);
        if (!header_ok) {
            header_ok = (strcmp(line, GATE_ROUTE_CACHE_HEADER) == 0);
            if (!header_ok)
                break;
            continue;
        }
        if (line[0] == '\0' || line[0] == '#')
            continue;
        if (ended) {
            route->valid = B_FALSE;
        } else if (strncmp(line, "slot ", 5) == 0) {
            unsigned stored_slot;

            route->have_slot = (sscanf(line, "slot %u", &stored_slot) == 1 &&
                stored_slot >= 1 && stored_slot <= GATE_ROUTE_CACHE_SLOT_COUNT);
            if (route->have_slot)
                route->slot = stored_slot - 1;
            route->valid = route->valid && route->have_slot;
        } else if (strncmp(line, "airport ", 8) == 0) {
            route->have_airport = copy_line_value(route->airport,
                sizeof(route->airport), line, "airport ");
            route->valid = route->valid && route->have_airport;
        } else if (strncmp(line, "ramp ", 5) == 0) {
            route->have_ramp = copy_line_value(route->ramp,
                sizeof(route->ramp), line, "ramp ");
            route->valid = route->valid && route->have_ramp;
        } else if (strncmp(line, "anchor ", 7) == 0) {
            route->have_anchor = (sscanf(line, "anchor %lf %lf %lf",
                &route->anchor_geo.lat, &route->anchor_geo.lon,
                &route->anchor_hdg) == 3 &&
                is_valid_lat(route->anchor_geo.lat) &&
                is_valid_lon(route->anchor_geo.lon) &&
                isfinite(route->anchor_hdg));
            route->valid = route->valid && route->have_anchor;
        } else if (strncmp(line, "aircraft ", 9) == 0) {
            route->have_aircraft = copy_line_value(route->aircraft,
                sizeof(route->aircraft), line, "aircraft ");
            route->valid = route->valid && route->have_aircraft;
        } else if (strncmp(line, "geometry ", 9) == 0) {
            route->have_geometry = (sscanf(line, "geometry %lf %lf %lf",
                &route->wheelbase, &route->nw_z, &route->main_z) == 3 &&
                isfinite(route->wheelbase) && route->wheelbase > 0 &&
                isfinite(route->nw_z) && isfinite(route->main_z));
            route->valid = route->valid && route->have_geometry;
        } else if (strncmp(line, "label ", 6) == 0) {
            route->have_label = copy_line_value(route->label,
                sizeof(route->label), line, "label ");
            route->valid = route->valid && route->have_label;
        } else if (strncmp(line, "final ", 6) == 0) {
            route->have_final = (sscanf(line, "final %lf %lf",
                &route->final_aircraft_hdg, &route->final_tail_hdg) == 2 &&
                isfinite(route->final_aircraft_hdg) &&
                isfinite(route->final_tail_hdg));
            route->valid = route->valid && route->have_final;
        } else if (strncmp(line, "segments ", 9) == 0) {
            route->have_count = (sscanf(line, "segments %u",
                &route->expected_count) == 1 && route->expected_count > 0 &&
                route->expected_count <= GATE_ROUTE_CACHE_MAX_SEGS);
            route->valid = route->valid && route->have_count;
        } else if (strncmp(line, "seg ", 4) == 0) {
            route->valid = route->valid && parse_segment_line(line, route);
        } else if (strcmp(line, "endroute") == 0) {
            ended = B_TRUE;
            route->active = B_FALSE;
        } else {
            route->valid = B_FALSE;
        }
    }
    free(line);
    if (fclose(fp) != 0)
        route->valid = B_FALSE;
    return (header_ok && ended && route->valid);
}

static void
slot_info_from_route(const parsed_route_t *route, gate_route_slot_info_t *info)
{
    ASSERT(route != NULL);
    ASSERT(info != NULL);
    memset(info, 0, sizeof(*info));
    info->valid = B_TRUE;
    info->slot = route->slot;
    info->segment_count = route->actual_count;
    info->final_aircraft_hdg = route->final_aircraft_hdg;
    info->final_tail_hdg = route->final_tail_hdg;
    gate_route_tail_direction(info->final_aircraft_hdg,
        info->tail_direction, sizeof(info->tail_direction));
}

static bool_t
read_valid_slot(const gate_route_context_t *context, unsigned slot,
    parsed_route_t *route, list_t *local)
{
    char *filename;
    list_t temporary;
    list_t *validation = local;
    bool_t result;

    filename = gate_route_slot_path(context, slot, B_FALSE);
    if (filename == NULL) {
        parsed_route_init(route);
        return B_FALSE;
    }
    result = read_route_file(filename, route);
    free(filename);
    if (!result || !route_metadata_matches(route, context, slot) ||
        !route_final_metadata_matches(route, context)) {
        return B_FALSE;
    }
    if (validation == NULL) {
        list_create(&temporary, sizeof(seg_t), offsetof(seg_t, node));
        validation = &temporary;
    }
    copy_route_to_local(route, context, validation);
    result = route_initial_pose_matches(context, validation);
    if (!result || local == NULL)
        segment_list_clear(validation);
    if (local == NULL)
        list_destroy(&temporary);
    return result;
}

unsigned
gate_route_cache_list(const gate_route_context_t *context,
    gate_route_slot_info_t slots[GATE_ROUTE_CACHE_SLOT_COUNT])
{
    unsigned count = 0;

    ASSERT(context != NULL);
    ASSERT(slots != NULL);
    memset(slots, 0, sizeof(*slots) * GATE_ROUTE_CACHE_SLOT_COUNT);
    if (!context->recognized)
        return 0;
    for (unsigned slot = 0; slot < GATE_ROUTE_CACHE_SLOT_COUNT; slot++) {
        parsed_route_t route;

        if (read_valid_slot(context, slot, &route, NULL)) {
            slot_info_from_route(&route, &slots[slot]);
            count++;
        }
        parsed_route_fini(&route);
    }
    return count;
}

bool_t
gate_route_cache_load(const gate_route_context_t *context, unsigned slot,
    list_t *segs, gate_route_slot_info_t *info)
{
    parsed_route_t route;
    bool_t result;

    ASSERT(context != NULL);
    ASSERT(segs != NULL);
    ASSERT(list_head(segs) == NULL);
    if (!context->recognized || slot >= GATE_ROUTE_CACHE_SLOT_COUNT)
        return B_FALSE;
    result = read_valid_slot(context, slot, &route, segs);
    if (result && info != NULL)
        slot_info_from_route(&route, info);
    parsed_route_fini(&route);
    return result;
}

static bool_t
replace_route_file(const char *filename, const char *temporary)
{
    bool_t result;

#if IBM
    if (file_exists(filename, NULL)) {
        result = ReplaceFileA(filename, temporary, NULL,
            REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS,
            NULL, NULL) != 0;
    } else {
        result = (rename(temporary, filename) == 0);
    }
#else
    result = (rename(temporary, filename) == 0);
#endif
    if (!result)
        (void)remove(temporary);
    return result;
}

bool_t
gate_route_cache_save(const gate_route_context_t *context, unsigned slot,
    const list_t *segs, gate_route_slot_info_t *info)
{
    char *filename, *temporary;
    FILE *fp;
    bool_t result = B_TRUE;
    unsigned count;
    const seg_t *last;
    gate_route_slot_info_t saved;

    ASSERT(context != NULL);
    ASSERT(segs != NULL);
    if (!context->recognized || slot >= GATE_ROUTE_CACHE_SLOT_COUNT ||
        list_head(segs) == NULL ||
        !route_initial_pose_matches(context, segs)) {
        return B_FALSE;
    }

    filename = gate_route_slot_path(context, slot, B_TRUE);
    if (filename == NULL)
        return B_FALSE;
    temporary = sprintf_alloc("%s.tmp", filename);
    fp = fopen(temporary, "w");
    if (fp == NULL) {
        free(temporary);
        free(filename);
        return B_FALSE;
    }

    count = segment_list_count(segs);
    last = list_tail(segs);
    memset(&saved, 0, sizeof(saved));
    saved.valid = B_TRUE;
    saved.slot = slot;
    saved.segment_count = count;
    saved.final_aircraft_hdg = gate_route_heading_from_delta(
        context->anchor_hdg, gate_route_heading_delta(context->frame_hdg,
        last->end_hdg));
    saved.final_tail_hdg = gate_route_heading_from_delta(
        saved.final_aircraft_hdg, 180.0);
    gate_route_tail_direction(saved.final_aircraft_hdg,
        saved.tail_direction, sizeof(saved.tail_direction));

    fprintf(fp, "%s\n", GATE_ROUTE_CACHE_HEADER);
    fprintf(fp, "slot %u\n", slot + 1);
    fprintf(fp, "airport %s\n", context->airport);
    fprintf(fp, "ramp %s\n", context->ramp);
    fprintf(fp, "anchor %.17f %.17f %.9f\n", context->anchor_geo.lat,
        context->anchor_geo.lon, context->anchor_hdg);
    fprintf(fp, "aircraft %s\n", context->aircraft);
    fprintf(fp, "geometry %.9f %.9f %.9f\n", context->wheelbase,
        context->nw_z, context->main_z);
    fprintf(fp, "label Tail %s\n", saved.tail_direction);
    fprintf(fp, "final %.9f %.9f\n", saved.final_aircraft_hdg,
        saved.final_tail_hdg);
    fprintf(fp, "segments %u\n", count);
    for (const seg_t *seg = list_head(segs); seg != NULL;
         seg = list_next(segs, seg)) {
        double start_x, start_y, end_x, end_y;
        double start_hdg = gate_route_heading_delta(context->frame_hdg,
            seg->start_hdg);
        double end_hdg = gate_route_heading_delta(context->frame_hdg,
            seg->end_hdg);
        double value = (seg->type == SEG_TYPE_STRAIGHT ? seg->len :
            seg->turn.r);
        unsigned right = (seg->type == SEG_TYPE_TURN ? seg->turn.right : 0);

        gate_route_point_to_relative(seg->start_pos.x, seg->start_pos.y,
            context->anchor_local.x, context->anchor_local.y,
            context->frame_hdg, &start_x, &start_y);
        gate_route_point_to_relative(seg->end_pos.x, seg->end_pos.y,
            context->anchor_local.x, context->anchor_local.y,
            context->frame_hdg, &end_x, &end_y);
        fprintf(fp, "seg %u %.17f %.17f %.9f %.17f %.17f %.9f %u %u "
            "%.9f %u\n", seg->type, start_x, start_y, start_hdg,
            end_x, end_y, end_hdg, seg->backward, seg->user_placed,
            value, right);
    }
    fprintf(fp, "endroute\n");
    if (fflush(fp) != 0 || ferror(fp))
        result = B_FALSE;
    if (fclose(fp) != 0)
        result = B_FALSE;
    if (result)
        result = replace_route_file(filename, temporary);
    else
        (void)remove(temporary);
    if (result && info != NULL)
        *info = saved;
    free(temporary);
    free(filename);
    return result;
}
