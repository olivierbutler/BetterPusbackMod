/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source. A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * CDDL HEADER END
 */

#include <math.h>
#include <string.h>

#include "planner_cache.h"

#define FNV1A64_OFFSET UINT64_C(14695981039346656037)
#define FNV1A64_PRIME UINT64_C(1099511628211)

static void
advance_revision(planner_cache_state_t *cache)
{
    cache->revision++;
    if (cache->revision == 0)
        cache->revision = 1;
}

static double
heading_difference(double a, double b)
{
    double difference = fmod(a - b, 360.0);

    if (difference > 180.0)
        difference -= 360.0;
    else if (difference < -180.0)
        difference += 360.0;
    return (fabs(difference));
}

void
planner_cache_init(planner_cache_state_t *cache)
{
    memset(cache, 0, sizeof(*cache));
}

bool
planner_cache_observe(planner_cache_state_t *cache, uint64_t signature)
{
    if (cache->have_signature &&
        cache->observed_signature == signature)
        return (false);

    cache->have_signature = true;
    cache->observed_signature = signature;
    cache->have_result = false;
    advance_revision(cache);
    return (true);
}

bool
planner_cache_needs_rebuild(const planner_cache_state_t *cache)
{
    return (cache->have_signature && (!cache->have_result ||
        cache->built_revision != cache->revision));
}

void
planner_cache_commit(planner_cache_state_t *cache, bool success,
    size_t point_count, uint64_t build_us, uint64_t terrain_probes)
{
    cache->have_result = true;
    cache->result_success = success;
    cache->built_revision = cache->revision;
    cache->build_count++;
    cache->terrain_probe_count += terrain_probes;
    cache->total_build_us += build_us;
    if (build_us > cache->max_build_us)
        cache->max_build_us = build_us;
    cache->point_count = success ? point_count : 0;
    if (!success)
        cache->failure_count++;
}

void
planner_cache_note_hit(planner_cache_state_t *cache)
{
    cache->cache_hit_count++;
}

void
planner_cache_force_invalidate(planner_cache_state_t *cache)
{
    cache->have_result = false;
    advance_revision(cache);
}

uint64_t
planner_hash_init(void)
{
    return (FNV1A64_OFFSET);
}

uint64_t
planner_hash_bytes(uint64_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= FNV1A64_PRIME;
    }
    return (hash);
}

uint64_t
planner_hash_u64(uint64_t hash, uint64_t value)
{
    return (planner_hash_bytes(hash, &value, sizeof(value)));
}

uint64_t
planner_hash_double(uint64_t hash, double value)
{
    uint64_t bits;

    if (value == 0)
        value = 0; /* canonicalize negative zero */
    memcpy(&bits, &value, sizeof(bits));
    return (planner_hash_u64(hash, bits));
}

void
planner_prediction_key_init(planner_prediction_key_t *key)
{
    memset(key, 0, sizeof(*key));
}

bool
planner_prediction_key_matches(const planner_prediction_key_t *key,
    uint64_t base_signature, double start_x, double start_y,
    double start_hdg, double end_x, double end_y, double end_hdg,
    double position_epsilon, double heading_epsilon)
{
    if (!key->valid || key->base_signature != base_signature)
        return (false);

    return (fabs(key->start_x - start_x) <= position_epsilon &&
        fabs(key->start_y - start_y) <= position_epsilon &&
        heading_difference(key->start_hdg, start_hdg) <= heading_epsilon &&
        fabs(key->end_x - end_x) <= position_epsilon &&
        fabs(key->end_y - end_y) <= position_epsilon &&
        heading_difference(key->end_hdg, end_hdg) <= heading_epsilon);
}

void
planner_prediction_key_set(planner_prediction_key_t *key,
    uint64_t base_signature, double start_x, double start_y,
    double start_hdg, double end_x, double end_y, double end_hdg)
{
    key->valid = true;
    key->base_signature = base_signature;
    key->start_x = start_x;
    key->start_y = start_y;
    key->start_hdg = start_hdg;
    key->end_x = end_x;
    key->end_y = end_y;
    key->end_hdg = end_hdg;
}

void
planner_prediction_key_invalidate(planner_prediction_key_t *key)
{
    key->valid = false;
}

