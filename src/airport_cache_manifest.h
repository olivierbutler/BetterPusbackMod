#ifndef _AIRPORT_CACHE_MANIFEST_H_
#define _AIRPORT_CACHE_MANIFEST_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AIRPORT_CACHE_MANIFEST_FILENAME "scenery_inputs_v1"

typedef struct {
    uint64_t fingerprint;
    size_t input_count;
} airport_cache_manifest_t;

bool airport_cache_manifest_compute(const char *xplane_directory,
    airport_cache_manifest_t *manifest);
bool airport_cache_manifest_matches(const char *cache_directory,
    const airport_cache_manifest_t *manifest);
bool airport_cache_manifest_write(const char *cache_directory,
    const airport_cache_manifest_t *manifest);
bool airport_cache_manifest_invalidate(const char *cache_directory);

#ifdef __cplusplus
}
#endif

#endif
