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

#ifndef _PLANNER_CACHE_H_
#define _PLANNER_CACHE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool have_signature;
    bool have_result;
    bool result_success;
    uint64_t observed_signature;
    uint64_t revision;
    uint64_t built_revision;
    uint64_t build_count;
    uint64_t cache_hit_count;
    uint64_t failure_count;
    uint64_t terrain_probe_count;
    uint64_t total_build_us;
    uint64_t max_build_us;
    size_t point_count;
} planner_cache_state_t;

typedef struct {
    bool valid;
    uint64_t base_signature;
    double start_x;
    double start_y;
    double start_hdg;
    double end_x;
    double end_y;
    double end_hdg;
} planner_prediction_key_t;

void planner_cache_init(planner_cache_state_t *cache);
bool planner_cache_observe(planner_cache_state_t *cache,
    uint64_t signature);
bool planner_cache_needs_rebuild(const planner_cache_state_t *cache);
void planner_cache_commit(planner_cache_state_t *cache, bool success,
    size_t point_count, uint64_t build_us, uint64_t terrain_probes);
void planner_cache_note_hit(planner_cache_state_t *cache);
void planner_cache_force_invalidate(planner_cache_state_t *cache);

uint64_t planner_hash_init(void);
uint64_t planner_hash_bytes(uint64_t hash, const void *data, size_t size);
uint64_t planner_hash_u64(uint64_t hash, uint64_t value);
uint64_t planner_hash_double(uint64_t hash, double value);

void planner_prediction_key_init(planner_prediction_key_t *key);
bool planner_prediction_key_matches(const planner_prediction_key_t *key,
    uint64_t base_signature, double start_x, double start_y,
    double start_hdg, double end_x, double end_y, double end_hdg,
    double position_epsilon, double heading_epsilon);
void planner_prediction_key_set(planner_prediction_key_t *key,
    uint64_t base_signature, double start_x, double start_y,
    double start_hdg, double end_x, double end_y, double end_hdg);
void planner_prediction_key_invalidate(planner_prediction_key_t *key);

#ifdef __cplusplus
}
#endif

#endif /* _PLANNER_CACHE_H_ */
