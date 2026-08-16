/*
 * Fixed-size, source-aware Ground Operations context. Provider code builds a
 * validated snapshot away from draw code; the UI only consumes presentation
 * strings copied from that snapshot.
 */

#ifndef _GROUND_OPS_DATA_H_
#define _GROUND_OPS_DATA_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GROUND_OPS_DATA_FLIGHT_NUMBER_LEN 12
#define GROUND_OPS_DATA_AIRPORT_LEN 8
#define GROUND_OPS_DATA_EOBT_LEN 12
#define GROUND_OPS_DATA_METAR_LEN 96
#define GROUND_OPS_DATA_ATIS_LEN 8
#define GROUND_OPS_DATA_RUNWAY_LEN 8
#define GROUND_OPS_DATA_FLIGHT_DISPLAY_LEN 24
#define GROUND_OPS_DATA_SCHEDULE_DISPLAY_LEN 20
#define GROUND_OPS_DATA_WEATHER_DISPLAY_LEN 48
#define GROUND_OPS_DATA_PRESSURE_DISPLAY_LEN 40
#define GROUND_OPS_DATA_ADVISORY_DISPLAY_LEN 48
#define GROUND_OPS_DATA_SOURCE_DISPLAY_LEN 32

typedef enum {
    GROUND_OPS_DATA_SOURCE_UNAVAILABLE,
    GROUND_OPS_DATA_SOURCE_SIMULATOR,
    GROUND_OPS_DATA_SOURCE_CACHED_METAR,
    GROUND_OPS_DATA_SOURCE_ONLINE_ATIS
} ground_ops_data_source_t;

typedef enum {
    GROUND_OPS_DATA_FRESHNESS_UNAVAILABLE,
    GROUND_OPS_DATA_FRESHNESS_CURRENT,
    GROUND_OPS_DATA_FRESHNESS_STALE
} ground_ops_data_freshness_t;

typedef enum {
    GROUND_OPS_DATA_PROVIDER_FLIGHT_IDENTITY,
    GROUND_OPS_DATA_PROVIDER_SCHEDULE,
    GROUND_OPS_DATA_PROVIDER_WEATHER,
    GROUND_OPS_DATA_PROVIDER_METAR,
    GROUND_OPS_DATA_PROVIDER_ATIS
} ground_ops_data_provider_t;

typedef struct {
    ground_ops_data_source_t source;
    double fetched_at_s;
    double expires_at_s;
} ground_ops_data_meta_t;

typedef struct {
    ground_ops_data_meta_t meta;
    char flight_number[GROUND_OPS_DATA_FLIGHT_NUMBER_LEN];
    char departure[GROUND_OPS_DATA_AIRPORT_LEN];
} ground_ops_flight_identity_t;

typedef struct {
    ground_ops_data_meta_t meta;
    char eobt[GROUND_OPS_DATA_EOBT_LEN];
} ground_ops_schedule_t;

typedef struct {
    ground_ops_data_meta_t meta;
    double wind_direction_deg;
    double wind_speed_mps;
    double temperature_c;
    double qnh_pa;
} ground_ops_weather_t;

typedef struct {
    ground_ops_data_meta_t meta;
    char text[GROUND_OPS_DATA_METAR_LEN];
} ground_ops_metar_t;

typedef struct {
    ground_ops_data_meta_t meta;
    char identifier[GROUND_OPS_DATA_ATIS_LEN];
    char runway[GROUND_OPS_DATA_RUNWAY_LEN];
} ground_ops_atis_t;

typedef struct {
    uint64_t revision;
    ground_ops_flight_identity_t flight;
    ground_ops_schedule_t schedule;
    ground_ops_weather_t weather;
    ground_ops_metar_t metar;
    ground_ops_atis_t atis;
} ground_ops_data_snapshot_t;

typedef struct {
    char flight[GROUND_OPS_DATA_FLIGHT_DISPLAY_LEN];
    char schedule[GROUND_OPS_DATA_SCHEDULE_DISPLAY_LEN];
    char weather[GROUND_OPS_DATA_WEATHER_DISPLAY_LEN];
    char pressure[GROUND_OPS_DATA_PRESSURE_DISPLAY_LEN];
    char advisory[GROUND_OPS_DATA_ADVISORY_DISPLAY_LEN];
    char source[GROUND_OPS_DATA_SOURCE_DISPLAY_LEN];
} ground_ops_data_presentation_t;

void ground_ops_data_init(ground_ops_data_snapshot_t *snapshot);
void ground_ops_data_clear(ground_ops_data_snapshot_t *snapshot,
    ground_ops_data_provider_t provider);

bool ground_ops_data_publish_flight_identity(
    ground_ops_data_snapshot_t *snapshot, ground_ops_data_source_t source,
    const void *flight_number, size_t flight_number_len,
    const char *departure, double fetched_at_s, double expires_at_s);
bool ground_ops_data_publish_simulator_flight_identity(
    ground_ops_data_snapshot_t *snapshot, const void *flight_number,
    size_t flight_number_len, const void *aircraft_icao,
    size_t aircraft_icao_len, double fetched_at_s, double expires_at_s);
bool ground_ops_data_publish_schedule(ground_ops_data_snapshot_t *snapshot,
    ground_ops_data_source_t source, const char *eobt, double fetched_at_s,
    double expires_at_s);
bool ground_ops_data_publish_weather(ground_ops_data_snapshot_t *snapshot,
    ground_ops_data_source_t source, double wind_direction_deg,
    double wind_speed_mps, double temperature_c, double qnh_pa,
    double fetched_at_s, double expires_at_s);
bool ground_ops_data_publish_metar(ground_ops_data_snapshot_t *snapshot,
    const char *metar, double fetched_at_s, double expires_at_s);
bool ground_ops_data_publish_atis(ground_ops_data_snapshot_t *snapshot,
    const char *identifier, const char *runway, double fetched_at_s,
    double expires_at_s);

ground_ops_data_freshness_t ground_ops_data_freshness(
    const ground_ops_data_meta_t *meta, double now_s);
const char *ground_ops_data_source_name(ground_ops_data_source_t source);
void ground_ops_data_format(const ground_ops_data_snapshot_t *snapshot,
    double now_s, ground_ops_data_presentation_t *presentation);

#ifdef __cplusplus
}
#endif

#endif /* _GROUND_OPS_DATA_H_ */
