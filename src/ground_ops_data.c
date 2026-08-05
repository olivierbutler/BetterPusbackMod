/*
 * Pure validation and presentation for optional flight/weather providers.
 * This file has no XPLM, networking, allocation, or draw dependencies.
 */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ground_ops_data.h"

#define MPS_TO_KNOTS 1.9438444924406

static void
copy_text(char *destination, size_t capacity, const char *source)
{
    if (capacity == 0)
        return;
    (void)snprintf(destination, capacity, "%s",
        source != NULL ? source : "");
}

static bool
source_valid(ground_ops_data_source_t source)
{
    return (source > GROUND_OPS_DATA_SOURCE_UNAVAILABLE &&
        source <= GROUND_OPS_DATA_SOURCE_ONLINE_ATIS);
}

static bool
times_valid(double fetched_at_s, double expires_at_s)
{
    return (isfinite(fetched_at_s) && isfinite(expires_at_s) &&
        fetched_at_s >= 0 && expires_at_s >= fetched_at_s);
}

static bool
normalize_token(const void *input, size_t input_len, char *output,
    size_t output_capacity, size_t maximum_length)
{
    const unsigned char *bytes = input;
    size_t begin = 0, end = input_len, count = 0;

    if (output_capacity == 0)
        return (false);
    output[0] = '\0';
    if (input == NULL || input_len == 0)
        return (false);

    while (begin < end && (bytes[begin] == '\0' ||
        isspace(bytes[begin]))) {
        begin++;
    }
    while (end > begin && (bytes[end - 1] == '\0' ||
        isspace(bytes[end - 1]))) {
        end--;
    }
    if (begin == end || end - begin > maximum_length ||
        end - begin >= output_capacity) {
        return (false);
    }
    for (size_t index = begin; index < end; index++) {
        unsigned char value = bytes[index];

        if (!isalnum(value) && value != '-') {
            output[0] = '\0';
            return (false);
        }
        output[count++] = (char)toupper(value);
    }
    output[count] = '\0';
    return (count != 0);
}

static bool
bounded_length(const char *input, size_t limit, size_t *length)
{
    size_t count;

    if (input == NULL || length == NULL)
        return (false);
    for (count = 0; count < limit; count++) {
        if (input[count] == '\0') {
            *length = count;
            return (true);
        }
    }
    return (false);
}

static bool
normalize_text(const char *input, size_t input_len, char *output,
    size_t output_capacity)
{
    size_t begin = 0, end = input_len, count = 0;

    if (output_capacity == 0)
        return (false);
    output[0] = '\0';
    if (input == NULL)
        return (false);
    while (begin < end && isspace((unsigned char)input[begin]))
        begin++;
    while (end > begin && isspace((unsigned char)input[end - 1]))
        end--;
    if (begin == end || end - begin >= output_capacity)
        return (false);
    for (size_t index = begin; index < end; index++) {
        unsigned char value = (unsigned char)input[index];

        if (value < 32 || value > 126) {
            output[0] = '\0';
            return (false);
        }
        output[count++] = (char)value;
    }
    output[count] = '\0';
    return (count != 0);
}

static ground_ops_data_meta_t
make_meta(ground_ops_data_source_t source, double fetched_at_s,
    double expires_at_s)
{
    ground_ops_data_meta_t meta = {source, fetched_at_s, expires_at_s};

    return (meta);
}

static void
mark_changed(ground_ops_data_snapshot_t *snapshot)
{
    snapshot->revision++;
    if (snapshot->revision == 0)
        snapshot->revision = 1;
}

void
ground_ops_data_init(ground_ops_data_snapshot_t *snapshot)
{
    if (snapshot != NULL)
        memset(snapshot, 0, sizeof(*snapshot));
}

void
ground_ops_data_clear(ground_ops_data_snapshot_t *snapshot,
    ground_ops_data_provider_t provider)
{
    bool changed = false;

    if (snapshot == NULL)
        return;
    switch (provider) {
    case GROUND_OPS_DATA_PROVIDER_FLIGHT_IDENTITY:
        changed = snapshot->flight.meta.source !=
            GROUND_OPS_DATA_SOURCE_UNAVAILABLE;
        memset(&snapshot->flight, 0, sizeof(snapshot->flight));
        break;
    case GROUND_OPS_DATA_PROVIDER_SCHEDULE:
        changed = snapshot->schedule.meta.source !=
            GROUND_OPS_DATA_SOURCE_UNAVAILABLE;
        memset(&snapshot->schedule, 0, sizeof(snapshot->schedule));
        break;
    case GROUND_OPS_DATA_PROVIDER_WEATHER:
        changed = snapshot->weather.meta.source !=
            GROUND_OPS_DATA_SOURCE_UNAVAILABLE;
        memset(&snapshot->weather, 0, sizeof(snapshot->weather));
        break;
    case GROUND_OPS_DATA_PROVIDER_METAR:
        changed = snapshot->metar.meta.source !=
            GROUND_OPS_DATA_SOURCE_UNAVAILABLE;
        memset(&snapshot->metar, 0, sizeof(snapshot->metar));
        break;
    case GROUND_OPS_DATA_PROVIDER_ATIS:
        changed = snapshot->atis.meta.source !=
            GROUND_OPS_DATA_SOURCE_UNAVAILABLE;
        memset(&snapshot->atis, 0, sizeof(snapshot->atis));
        break;
    }
    if (changed)
        mark_changed(snapshot);
}

bool
ground_ops_data_publish_flight_identity(ground_ops_data_snapshot_t *snapshot,
    ground_ops_data_source_t source, const void *flight_number,
    size_t flight_number_len, const char *departure, double fetched_at_s,
    double expires_at_s)
{
    ground_ops_flight_identity_t value = {0};
    bool have_flight, have_departure = false;
    size_t departure_len = 0;

    if (snapshot == NULL || !source_valid(source) ||
        !times_valid(fetched_at_s, expires_at_s)) {
        return (false);
    }
    have_flight = normalize_token(flight_number, flight_number_len,
        value.flight_number, sizeof(value.flight_number), 7);
    if (departure != NULL && bounded_length(departure,
        sizeof(value.departure), &departure_len)) {
        have_departure = normalize_token(departure, departure_len,
            value.departure, sizeof(value.departure), 7);
    }
    if (!have_flight && !have_departure) {
        return (false);
    }
    value.meta = make_meta(source, fetched_at_s, expires_at_s);
    snapshot->flight = value;
    mark_changed(snapshot);
    return (true);
}

bool
ground_ops_data_publish_simulator_flight_identity(
    ground_ops_data_snapshot_t *snapshot, const void *flight_number,
    size_t flight_number_len, const void *aircraft_icao,
    size_t aircraft_icao_len, double fetched_at_s, double expires_at_s)
{
    char normalized_flight[GROUND_OPS_DATA_FLIGHT_NUMBER_LEN] = {0};
    char normalized_icao[GROUND_OPS_DATA_AIRPORT_LEN] = {0};

    if (!normalize_token(flight_number, flight_number_len,
        normalized_flight, sizeof(normalized_flight), 7)) {
        return (false);
    }
    if (normalize_token(aircraft_icao, aircraft_icao_len, normalized_icao,
        sizeof(normalized_icao), 7) &&
        strcmp(normalized_flight, normalized_icao) == 0) {
        return (false);
    }
    return (ground_ops_data_publish_flight_identity(snapshot,
        GROUND_OPS_DATA_SOURCE_SIMULATOR, normalized_flight,
        strlen(normalized_flight), NULL, fetched_at_s, expires_at_s));
}

bool
ground_ops_data_publish_schedule(ground_ops_data_snapshot_t *snapshot,
    ground_ops_data_source_t source, const char *eobt, double fetched_at_s,
    double expires_at_s)
{
    ground_ops_schedule_t value = {0};
    size_t eobt_len = 0;

    if (snapshot == NULL || !source_valid(source) ||
        !times_valid(fetched_at_s, expires_at_s) ||
        !bounded_length(eobt, sizeof(value.eobt), &eobt_len) ||
        !normalize_text(eobt, eobt_len, value.eobt, sizeof(value.eobt))) {
        return (false);
    }
    value.meta = make_meta(source, fetched_at_s, expires_at_s);
    snapshot->schedule = value;
    mark_changed(snapshot);
    return (true);
}

bool
ground_ops_data_publish_weather(ground_ops_data_snapshot_t *snapshot,
    ground_ops_data_source_t source, double wind_direction_deg,
    double wind_speed_mps, double temperature_c, double qnh_pa,
    double fetched_at_s, double expires_at_s)
{
    ground_ops_weather_t value = {0};

    if (snapshot == NULL || !source_valid(source) ||
        !times_valid(fetched_at_s, expires_at_s) ||
        !isfinite(wind_direction_deg) || wind_direction_deg < 0 ||
        wind_direction_deg > 360 || !isfinite(wind_speed_mps) ||
        wind_speed_mps < 0 || wind_speed_mps > 150 ||
        !isfinite(temperature_c) || temperature_c < -100 ||
        temperature_c > 80 || !isfinite(qnh_pa) || qnh_pa < 80000 ||
        qnh_pa > 110000) {
        return (false);
    }
    value.meta = make_meta(source, fetched_at_s, expires_at_s);
    value.wind_direction_deg = wind_direction_deg == 360 ? 0 :
        wind_direction_deg;
    value.wind_speed_mps = wind_speed_mps;
    value.temperature_c = temperature_c;
    value.qnh_pa = qnh_pa;
    snapshot->weather = value;
    mark_changed(snapshot);
    return (true);
}

bool
ground_ops_data_publish_metar(ground_ops_data_snapshot_t *snapshot,
    const char *metar, double fetched_at_s, double expires_at_s)
{
    ground_ops_metar_t value = {0};
    size_t metar_len = 0;

    if (snapshot == NULL || !times_valid(fetched_at_s, expires_at_s) ||
        !bounded_length(metar, sizeof(value.text), &metar_len) ||
        !normalize_text(metar, metar_len, value.text,
        sizeof(value.text))) {
        return (false);
    }
    value.meta = make_meta(GROUND_OPS_DATA_SOURCE_CACHED_METAR,
        fetched_at_s, expires_at_s);
    snapshot->metar = value;
    mark_changed(snapshot);
    return (true);
}

bool
ground_ops_data_publish_atis(ground_ops_data_snapshot_t *snapshot,
    const char *identifier, const char *runway, double fetched_at_s,
    double expires_at_s)
{
    ground_ops_atis_t value = {0};
    bool have_identifier = false, have_runway = false;
    size_t identifier_len = 0, runway_len = 0;

    if (snapshot == NULL || !times_valid(fetched_at_s, expires_at_s))
        return (false);
    if (identifier != NULL && bounded_length(identifier,
        sizeof(value.identifier), &identifier_len)) {
        have_identifier = normalize_token(identifier, identifier_len,
            value.identifier, sizeof(value.identifier), 7);
    }
    if (runway != NULL && bounded_length(runway, sizeof(value.runway),
        &runway_len)) {
        have_runway = normalize_token(runway, runway_len, value.runway,
            sizeof(value.runway), 7);
    }
    if (!have_identifier && !have_runway) {
        return (false);
    }
    value.meta = make_meta(GROUND_OPS_DATA_SOURCE_ONLINE_ATIS,
        fetched_at_s, expires_at_s);
    snapshot->atis = value;
    mark_changed(snapshot);
    return (true);
}

ground_ops_data_freshness_t
ground_ops_data_freshness(const ground_ops_data_meta_t *meta, double now_s)
{
    if (meta == NULL || meta->source == GROUND_OPS_DATA_SOURCE_UNAVAILABLE ||
        !isfinite(now_s)) {
        return (GROUND_OPS_DATA_FRESHNESS_UNAVAILABLE);
    }
    if (now_s > meta->expires_at_s)
        return (GROUND_OPS_DATA_FRESHNESS_STALE);
    return (GROUND_OPS_DATA_FRESHNESS_CURRENT);
}

const char *
ground_ops_data_source_name(ground_ops_data_source_t source)
{
    switch (source) {
    case GROUND_OPS_DATA_SOURCE_SIMULATOR:
        return ("Simulator");
    case GROUND_OPS_DATA_SOURCE_CACHED_METAR:
        return ("Cached METAR");
    case GROUND_OPS_DATA_SOURCE_ONLINE_ATIS:
        return ("Online ATIS");
    case GROUND_OPS_DATA_SOURCE_UNAVAILABLE:
    default:
        return ("Unavailable");
    }
}

static void
note_source(ground_ops_data_source_t source,
    ground_ops_data_freshness_t freshness, ground_ops_data_source_t *selected,
    bool *mixed, bool *stale)
{
    if (freshness == GROUND_OPS_DATA_FRESHNESS_UNAVAILABLE)
        return;
    if (*selected == GROUND_OPS_DATA_SOURCE_UNAVAILABLE)
        *selected = source;
    else if (*selected != source)
        *mixed = true;
    if (freshness == GROUND_OPS_DATA_FRESHNESS_STALE)
        *stale = true;
}

void
ground_ops_data_format(const ground_ops_data_snapshot_t *snapshot,
    double now_s, ground_ops_data_presentation_t *presentation)
{
    ground_ops_data_freshness_t flight_freshness, schedule_freshness;
    ground_ops_data_freshness_t weather_freshness, metar_freshness;
    ground_ops_data_freshness_t atis_freshness;
    ground_ops_data_source_t selected = GROUND_OPS_DATA_SOURCE_UNAVAILABLE;
    bool mixed = false, stale = false;

    if (presentation == NULL)
        return;
    memset(presentation, 0, sizeof(*presentation));
    if (snapshot == NULL) {
        copy_text(presentation->flight, sizeof(presentation->flight),
            "Flight --");
        copy_text(presentation->schedule, sizeof(presentation->schedule),
            "EOBT --");
        copy_text(presentation->weather, sizeof(presentation->weather),
            "SIM WX unavailable");
        copy_text(presentation->pressure, sizeof(presentation->pressure),
            "QNH unavailable");
        copy_text(presentation->advisory, sizeof(presentation->advisory),
            "METAR -- | ATIS --");
        copy_text(presentation->source, sizeof(presentation->source),
            "Local plugin | Offline");
        return;
    }

    flight_freshness = ground_ops_data_freshness(&snapshot->flight.meta,
        now_s);
    schedule_freshness = ground_ops_data_freshness(&snapshot->schedule.meta,
        now_s);
    weather_freshness = ground_ops_data_freshness(&snapshot->weather.meta,
        now_s);
    metar_freshness = ground_ops_data_freshness(&snapshot->metar.meta, now_s);
    atis_freshness = ground_ops_data_freshness(&snapshot->atis.meta, now_s);

    if (flight_freshness != GROUND_OPS_DATA_FRESHNESS_UNAVAILABLE &&
        snapshot->flight.flight_number[0] != '\0') {
        (void)snprintf(presentation->flight, sizeof(presentation->flight),
            "Flight %s", snapshot->flight.flight_number);
    } else {
        copy_text(presentation->flight, sizeof(presentation->flight),
            "Flight --");
    }
    if (schedule_freshness != GROUND_OPS_DATA_FRESHNESS_UNAVAILABLE &&
        snapshot->schedule.eobt[0] != '\0') {
        (void)snprintf(presentation->schedule, sizeof(presentation->schedule),
            "EOBT %s", snapshot->schedule.eobt);
    } else {
        copy_text(presentation->schedule, sizeof(presentation->schedule),
            "EOBT --");
    }
    if (weather_freshness != GROUND_OPS_DATA_FRESHNESS_UNAVAILABLE) {
        int direction = (int)lround(snapshot->weather.wind_direction_deg);
        int knots = (int)lround(snapshot->weather.wind_speed_mps *
            MPS_TO_KNOTS);
        int temperature = (int)lround(snapshot->weather.temperature_c);
        int qnh_hpa = (int)lround(snapshot->weather.qnh_pa / 100.0);
        double qnh_inhg = snapshot->weather.qnh_pa / 3386.389;

        if (direction == 360)
            direction = 0;
        (void)snprintf(presentation->weather,
            sizeof(presentation->weather), "SIM %03d/%02dKT %dC%s",
            direction, knots, temperature,
            weather_freshness == GROUND_OPS_DATA_FRESHNESS_STALE ?
            " STALE" : "");
        (void)snprintf(presentation->pressure,
            sizeof(presentation->pressure),
            "QNH %d hPa / %.2f inHg", qnh_hpa, qnh_inhg);
    } else {
        copy_text(presentation->weather, sizeof(presentation->weather),
            "SIM WX unavailable");
        copy_text(presentation->pressure, sizeof(presentation->pressure),
            "QNH unavailable");
    }
    if (metar_freshness == GROUND_OPS_DATA_FRESHNESS_UNAVAILABLE &&
        atis_freshness == GROUND_OPS_DATA_FRESHNESS_UNAVAILABLE) {
        copy_text(presentation->advisory, sizeof(presentation->advisory),
            "METAR -- | ATIS --");
    } else if (atis_freshness != GROUND_OPS_DATA_FRESHNESS_UNAVAILABLE) {
        (void)snprintf(presentation->advisory,
            sizeof(presentation->advisory), "ATIS %s%s%s%s",
            snapshot->atis.identifier[0] != '\0' ?
            snapshot->atis.identifier : "--",
            snapshot->atis.runway[0] != '\0' ? " RWY " : "",
            snapshot->atis.runway,
            atis_freshness == GROUND_OPS_DATA_FRESHNESS_STALE ?
            " STALE" : "");
    } else {
        (void)snprintf(presentation->advisory,
            sizeof(presentation->advisory), "METAR %s",
            metar_freshness == GROUND_OPS_DATA_FRESHNESS_STALE ?
            "STALE" : "current");
    }

    note_source(snapshot->flight.meta.source, flight_freshness, &selected,
        &mixed, &stale);
    note_source(snapshot->schedule.meta.source, schedule_freshness, &selected,
        &mixed, &stale);
    note_source(snapshot->weather.meta.source, weather_freshness, &selected,
        &mixed, &stale);
    note_source(snapshot->metar.meta.source, metar_freshness, &selected,
        &mixed, &stale);
    note_source(snapshot->atis.meta.source, atis_freshness, &selected,
        &mixed, &stale);
    if (selected == GROUND_OPS_DATA_SOURCE_UNAVAILABLE) {
        copy_text(presentation->source, sizeof(presentation->source),
            "Local plugin | Offline");
    } else {
        (void)snprintf(presentation->source, sizeof(presentation->source),
            "%s | %s", mixed ? "Mixed sources" :
            ground_ops_data_source_name(selected), stale ? "Stale" :
            "Current");
    }
}
