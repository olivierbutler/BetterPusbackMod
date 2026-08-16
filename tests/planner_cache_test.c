#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "planner_cache.h"

static void
test_cache_lifecycle(void)
{
    planner_cache_state_t cache;

    planner_cache_init(&cache);
    assert(!planner_cache_needs_rebuild(&cache));

    assert(planner_cache_observe(&cache, UINT64_C(100)));
    assert(cache.revision == 1);
    assert(planner_cache_needs_rebuild(&cache));

    planner_cache_commit(&cache, true, 42, 1500, 42);
    assert(!planner_cache_needs_rebuild(&cache));
    assert(cache.result_success);
    assert(cache.point_count == 42);
    assert(cache.build_count == 1);
    assert(cache.terrain_probe_count == 42);

    assert(!planner_cache_observe(&cache, UINT64_C(100)));
    planner_cache_note_hit(&cache);
    assert(cache.cache_hit_count == 1);
    assert(!planner_cache_needs_rebuild(&cache));

    assert(planner_cache_observe(&cache, UINT64_C(101)));
    assert(cache.revision == 2);
    assert(planner_cache_needs_rebuild(&cache));
    planner_cache_commit(&cache, false, 99, 2000, 0);
    assert(!planner_cache_needs_rebuild(&cache));
    assert(!cache.result_success);
    assert(cache.point_count == 0);
    assert(cache.failure_count == 1);
    assert(cache.total_build_us == 3500);
    assert(cache.max_build_us == 2000);

    assert(!planner_cache_observe(&cache, UINT64_C(101)));
    assert(!planner_cache_needs_rebuild(&cache));
    planner_cache_force_invalidate(&cache);
    assert(cache.revision == 3);
    assert(planner_cache_needs_rebuild(&cache));
}

static void
test_hashing(void)
{
    uint64_t first = planner_hash_init();
    uint64_t second = planner_hash_init();
    uint64_t changed = planner_hash_init();

    first = planner_hash_u64(first, UINT64_C(7));
    first = planner_hash_double(first, 12.5);
    second = planner_hash_u64(second, UINT64_C(7));
    second = planner_hash_double(second, 12.5);
    changed = planner_hash_u64(changed, UINT64_C(8));
    changed = planner_hash_double(changed, 12.5);

    assert(first == second);
    assert(first != changed);
    assert(planner_hash_double(planner_hash_init(), 0.0) ==
        planner_hash_double(planner_hash_init(), -0.0));
}

static void
test_prediction_key(void)
{
    planner_prediction_key_t key;

    planner_prediction_key_init(&key);
    assert(!planner_prediction_key_matches(&key, UINT64_C(9), 1, 2,
        359.999, 3, 4, 0.001, 0.01, 0.01));

    planner_prediction_key_set(&key, UINT64_C(9), 1, 2, 359.999,
        3, 4, 0.001);
    assert(planner_prediction_key_matches(&key, UINT64_C(9), 1.005,
        1.995, 0.001, 3.005, 3.995, 359.999, 0.01, 0.01));
    assert(!planner_prediction_key_matches(&key, UINT64_C(10), 1, 2,
        359.999, 3, 4, 0.001, 0.01, 0.01));
    assert(!planner_prediction_key_matches(&key, UINT64_C(9), 1.02, 2,
        359.999, 3, 4, 0.001, 0.01, 0.01));
    assert(!planner_prediction_key_matches(&key, UINT64_C(9), 1, 2,
        359.9, 3, 4, 0.001, 0.01, 0.01));

    planner_prediction_key_invalidate(&key);
    assert(!planner_prediction_key_matches(&key, UINT64_C(9), 1, 2,
        359.999, 3, 4, 0.001, 0.01, 0.01));
}

int
main(void)
{
    test_cache_lifecycle();
    test_hashing();
    test_prediction_key();
    puts("planner cache tests passed");
    return (0);
}
