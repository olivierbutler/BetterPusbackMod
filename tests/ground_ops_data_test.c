#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ground_ops_data.h"

static void
test_unavailable_defaults_are_explicit(void)
{
    ground_ops_data_snapshot_t snapshot;
    ground_ops_data_presentation_t presentation;

    ground_ops_data_init(&snapshot);
    ground_ops_data_format(&snapshot, 10, &presentation);
    assert(strcmp(presentation.flight, "Flight --") == 0);
    assert(strcmp(presentation.schedule, "EOBT --") == 0);
    assert(strcmp(presentation.weather, "SIM WX unavailable") == 0);
    assert(strcmp(presentation.pressure, "QNH unavailable") == 0);
    assert(strcmp(presentation.advisory, "METAR -- | ATIS --") == 0);
    assert(strcmp(presentation.source, "Local plugin | Offline") == 0);
}

static void
test_aircraft_type_is_the_flight_identity_fallback(void)
{
    ground_ops_data_snapshot_t snapshot;
    ground_ops_data_presentation_t presentation;

    ground_ops_data_init(&snapshot);
    assert(ground_ops_data_publish_simulator_flight_identity(&snapshot,
        NULL, 0, "b737", 4, 10, 12.5));
    ground_ops_data_format(&snapshot, 11, &presentation);
    assert(strcmp(presentation.flight, "Flight B737") == 0);

    assert(ground_ops_data_publish_simulator_flight_identity(&snapshot,
        "B737", 4, "b737", 4, 10, 12.5));
    ground_ops_data_format(&snapshot, 11, &presentation);
    assert(strcmp(presentation.flight, "Flight B737") == 0);

    assert(ground_ops_data_publish_simulator_flight_identity(&snapshot,
        "KLM511", 6, "B737", 4, 10, 12.5));
    ground_ops_data_format(&snapshot, 11, &presentation);
    assert(strcmp(presentation.flight, "Flight KLM511") == 0);
}

static void
test_simulator_flight_id_is_bounded_and_normalized(void)
{
    ground_ops_data_snapshot_t snapshot;
    ground_ops_data_presentation_t presentation;
    const char padded[8] = {' ', 'k', 'l', 'm', '5', '1', '1', ' '};
    const char invalid[8] = {'K', 'L', 'M', '/', '5', '1', '1', '\0'};

    ground_ops_data_init(&snapshot);
    assert(ground_ops_data_publish_flight_identity(&snapshot,
        GROUND_OPS_DATA_SOURCE_SIMULATOR, padded, sizeof(padded), NULL, 10,
        12.5));
    ground_ops_data_format(&snapshot, 11, &presentation);
    assert(strcmp(snapshot.flight.flight_number, "KLM511") == 0);
    assert(strcmp(presentation.flight, "Flight KLM511") == 0);
    assert(strcmp(presentation.source, "Simulator | Current") == 0);

    assert(!ground_ops_data_publish_flight_identity(&snapshot,
        GROUND_OPS_DATA_SOURCE_SIMULATOR, invalid, sizeof(invalid), NULL, 12,
        14.5));
    ground_ops_data_format(&snapshot, 12, &presentation);
    assert(strcmp(presentation.flight, "Flight KLM511") == 0);
    ground_ops_data_clear(&snapshot,
        GROUND_OPS_DATA_PROVIDER_FLIGHT_IDENTITY);
    ground_ops_data_format(&snapshot, 12, &presentation);
    assert(strcmp(presentation.flight, "Flight --") == 0);
}

static void
test_simulator_weather_format_and_staleness(void)
{
    ground_ops_data_snapshot_t snapshot;
    ground_ops_data_presentation_t presentation;

    ground_ops_data_init(&snapshot);
    assert(ground_ops_data_publish_weather(&snapshot,
        GROUND_OPS_DATA_SOURCE_SIMULATOR, 274.6, 5.14444, 17.6, 101325,
        20, 22.5));
    ground_ops_data_format(&snapshot, 21, &presentation);
    assert(strcmp(presentation.weather, "SIM 275/10KT 18C") == 0);
    assert(strcmp(presentation.pressure,
        "QNH 1013 hPa / 29.92 inHg") == 0);
    assert(strcmp(presentation.source, "Simulator | Current") == 0);

    ground_ops_data_format(&snapshot, 23, &presentation);
    assert(strcmp(presentation.weather,
        "SIM 275/10KT 18C STALE") == 0);
    assert(strcmp(presentation.source, "Simulator | Stale") == 0);
}

static void
test_invalid_weather_is_not_published(void)
{
    ground_ops_data_snapshot_t snapshot;
    ground_ops_data_presentation_t presentation;

    ground_ops_data_init(&snapshot);
    assert(!ground_ops_data_publish_weather(&snapshot,
        GROUND_OPS_DATA_SOURCE_SIMULATOR, 90, -1, 15, 101300, 10, 12));
    assert(!ground_ops_data_publish_weather(&snapshot,
        GROUND_OPS_DATA_SOURCE_SIMULATOR, 90, 3, NAN, 101300, 10, 12));
    assert(!ground_ops_data_publish_weather(&snapshot,
        GROUND_OPS_DATA_SOURCE_SIMULATOR, 90, 3, 15, 50000, 10, 12));
    ground_ops_data_format(&snapshot, 10, &presentation);
    assert(strcmp(presentation.weather, "SIM WX unavailable") == 0);
}

static void
test_optional_provider_metadata_and_mixed_sources(void)
{
    ground_ops_data_snapshot_t snapshot;
    ground_ops_data_presentation_t presentation;

    ground_ops_data_init(&snapshot);
    assert(ground_ops_data_publish_flight_identity(&snapshot,
        GROUND_OPS_DATA_SOURCE_SIMULATOR, "BAW117", 6, "KDEN", 100, 700));
    assert(ground_ops_data_publish_schedule(&snapshot,
        GROUND_OPS_DATA_SOURCE_SIMULATOR, "18:45Z", 100, 700));
    assert(ground_ops_data_publish_metar(&snapshot,
        "KDEN 041753Z 18008KT 10SM FEW080 22/08 A3001", 100, 400));
    assert(ground_ops_data_publish_atis(&snapshot, "B", "17L", 100, 160));
    ground_ops_data_format(&snapshot, 120, &presentation);
    assert(strcmp(presentation.flight, "Flight BAW117") == 0);
    assert(strcmp(presentation.schedule, "EOBT 18:45Z") == 0);
    assert(strcmp(presentation.advisory, "ATIS B RWY 17L") == 0);
    assert(strcmp(presentation.source, "Mixed sources | Current") == 0);

    ground_ops_data_format(&snapshot, 170, &presentation);
    assert(strcmp(presentation.advisory, "ATIS B RWY 17L STALE") == 0);
    assert(strcmp(presentation.source, "Mixed sources | Stale") == 0);
}

static void
test_oversized_or_malformed_text_is_rejected(void)
{
    ground_ops_data_snapshot_t snapshot;
    ground_ops_data_presentation_t presentation;
    char oversized[GROUND_OPS_DATA_METAR_LEN + 1];
    char malformed[] = "KDEN\nINJECTED";

    ground_ops_data_init(&snapshot);
    memset(oversized, 'A', sizeof(oversized) - 1);
    oversized[sizeof(oversized) - 1] = '\0';
    assert(ground_ops_data_publish_metar(&snapshot,
        "KDEN 041753Z 18008KT 10SM FEW080", 1, 20));
    assert(!ground_ops_data_publish_metar(&snapshot, oversized, 1, 2));
    assert(!ground_ops_data_publish_metar(&snapshot, malformed, 1, 2));
    assert(snapshot.metar.meta.source == GROUND_OPS_DATA_SOURCE_CACHED_METAR);
    ground_ops_data_format(&snapshot, 10, &presentation);
    assert(strcmp(presentation.advisory, "METAR current") == 0);
}

int
main(void)
{
    test_unavailable_defaults_are_explicit();
    test_simulator_flight_id_is_bounded_and_normalized();
    test_aircraft_type_is_the_flight_identity_fallback();
    test_simulator_weather_format_and_staleness();
    test_invalid_weather_is_not_published();
    test_optional_provider_metadata_and_mixed_sources();
    test_oversized_or_malformed_text_is_rejected();
    assert(sizeof(ground_ops_data_snapshot_t) < 512);
    assert(sizeof(ground_ops_data_presentation_t) < 256);
    puts("ground operations data tests passed");
    return (0);
}
