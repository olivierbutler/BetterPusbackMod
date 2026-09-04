#include <assert.h>
#include <stdio.h>

#include "telemetry.h"

int
main(int argc, char **argv)
{
    bp_telemetry_t telemetry = {0};
    bp_telemetry_metadata_t metadata = {0};
    FILE *output;

    assert(argc == 2);
    assert(!BP_ENABLE_RUNTIME_TELEMETRY);
    assert(!bp_telemetry_open(&telemetry, argv[1], &metadata, 10));
    assert(telemetry.fp == NULL);

    output = fopen(argv[1], "r");
    assert(output == NULL);
    return (0);
}
