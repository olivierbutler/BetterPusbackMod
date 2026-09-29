#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "airport_cache_manifest.h"

#define TEST_PATH_CAPACITY 4096

static void
path_join(char *destination, const char *root, const char *relative)
{
    int written = snprintf(destination, TEST_PATH_CAPACITY, "%s/%s", root,
        relative);
    assert(written >= 0 && written < TEST_PATH_CAPACITY);
}

static void
make_directory(const char *root, const char *relative)
{
    char path[TEST_PATH_CAPACITY];

    path_join(path, root, relative);
    assert(mkdir(path, 0700) == 0);
}

static void
write_file(const char *root, const char *relative, const char *contents)
{
    char path[TEST_PATH_CAPACITY];
    FILE *file;

    path_join(path, root, relative);
    file = fopen(path, "wb");
    assert(file != NULL);
    assert(fputs(contents, file) >= 0);
    assert(fclose(file) == 0);
}

static void
remove_file(const char *root, const char *relative)
{
    char path[TEST_PATH_CAPACITY];

    path_join(path, root, relative);
    assert(remove(path) == 0);
}

static void
remove_directory(const char *root, const char *relative)
{
    char path[TEST_PATH_CAPACITY];

    path_join(path, root, relative);
    assert(rmdir(path) == 0);
}

int
main(void)
{
    char temporary[] = "/tmp/betterpushback-airport-cache-XXXXXX";
    char cache[TEST_PATH_CAPACITY];
    char version[TEST_PATH_CAPACITY];
    airport_cache_manifest_t original, changed, disabled_change;
    FILE *file;
    char value[16] = {0};

    assert(mkdtemp(temporary) != NULL);
    make_directory(temporary, "Custom Scenery");
    make_directory(temporary, "Custom Scenery/A");
    make_directory(temporary, "Custom Scenery/A/Earth nav data");
    make_directory(temporary, "Custom Scenery/B");
    make_directory(temporary, "Custom Scenery/B/Earth nav data");
    make_directory(temporary, "Global Scenery");
    make_directory(temporary, "Global Scenery/Global Airports");
    make_directory(temporary, "Global Scenery/Global Airports/Earth nav data");
    make_directory(temporary, "cache");
    path_join(cache, temporary, "cache");

    write_file(temporary, "Custom Scenery/scenery_packs.ini",
        "I\n1000 Version\nSCENERY\n\n"
        "SCENERY_PACK Custom Scenery/A/\n"
        "SCENERY_PACK_DISABLED Custom Scenery/B/\n");
    write_file(temporary, "Custom Scenery/A/Earth nav data/apt.dat",
        "A airport data\n");
    write_file(temporary, "Custom Scenery/B/Earth nav data/apt.dat",
        "B airport data\n");
    write_file(temporary,
        "Global Scenery/Global Airports/Earth nav data/apt.dat",
        "global airport data\n");

    assert(airport_cache_manifest_compute(temporary, &original));
    assert(original.input_count == 2);
    assert(!airport_cache_manifest_matches(cache, &original));
    assert(airport_cache_manifest_write(cache, &original));
    assert(airport_cache_manifest_matches(cache, &original));

    write_file(temporary, "Custom Scenery/B/Earth nav data/apt.dat",
        "disabled B airport data changed\n");
    assert(airport_cache_manifest_compute(temporary, &disabled_change));
    assert(disabled_change.fingerprint == original.fingerprint);
    assert(disabled_change.input_count == original.input_count);

    write_file(temporary, "Custom Scenery/A/Earth nav data/apt.dat",
        "active A airport data changed and grew\n");
    assert(airport_cache_manifest_compute(temporary, &changed));
    assert(changed.fingerprint != original.fingerprint);
    assert(!airport_cache_manifest_matches(cache, &changed));
    assert(airport_cache_manifest_write(cache, &changed));
    assert(airport_cache_manifest_matches(cache, &changed));

    write_file(temporary, "Custom Scenery/scenery_packs.ini",
        "I\n1000 Version\nSCENERY\n\n"
        "SCENERY_PACK Custom Scenery/B/\n"
        "SCENERY_PACK Custom Scenery/A/\n");
    assert(airport_cache_manifest_compute(temporary, &original));
    assert(original.input_count == 3);
    assert(original.fingerprint != changed.fingerprint);

    write_file(temporary, "cache/version", "21");
    assert(airport_cache_manifest_invalidate(cache));
    path_join(version, temporary, "cache/version");
    file = fopen(version, "rb");
    assert(file != NULL);
    assert(fgets(value, sizeof(value), file) != NULL);
    assert(fclose(file) == 0);
    assert(strcmp(value, "-1\n") == 0);

    remove_file(temporary, "cache/version");
    remove_file(temporary, "cache/scenery_inputs_v1");
    remove_file(temporary, "Custom Scenery/scenery_packs.ini");
    remove_file(temporary, "Custom Scenery/A/Earth nav data/apt.dat");
    remove_file(temporary, "Custom Scenery/B/Earth nav data/apt.dat");
    remove_file(temporary,
        "Global Scenery/Global Airports/Earth nav data/apt.dat");
    remove_directory(temporary, "cache");
    remove_directory(temporary, "Custom Scenery/A/Earth nav data");
    remove_directory(temporary, "Custom Scenery/A");
    remove_directory(temporary, "Custom Scenery/B/Earth nav data");
    remove_directory(temporary, "Custom Scenery/B");
    remove_directory(temporary, "Custom Scenery");
    remove_directory(temporary, "Global Scenery/Global Airports/Earth nav data");
    remove_directory(temporary, "Global Scenery/Global Airports");
    remove_directory(temporary, "Global Scenery");
    assert(rmdir(temporary) == 0);

    puts("airport cache manifest tests passed");
    return 0;
}
