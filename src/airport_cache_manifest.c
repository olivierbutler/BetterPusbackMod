#include "airport_cache_manifest.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MANIFEST_FORMAT_VERSION 1U
#define MANIFEST_PATH_CAPACITY 4096U
#define FNV1A_OFFSET UINT64_C(14695981039346656037)
#define FNV1A_PRIME UINT64_C(1099511628211)

static uint64_t
hash_bytes(uint64_t hash, const void *data, size_t length)
{
    const unsigned char *bytes = data;

    for (size_t index = 0; index < length; index++) {
        hash ^= bytes[index];
        hash *= FNV1A_PRIME;
    }
    return hash;
}

static uint64_t
hash_string(uint64_t hash, const char *text)
{
    hash = hash_bytes(hash, text, strlen(text));
    return hash_bytes(hash, "\0", 1);
}

static bool
path_join(char *destination, size_t capacity, const char *first,
    const char *second)
{
    size_t first_length, second_offset = 0;
    int written;

    if (destination == NULL || capacity == 0 || first == NULL ||
        second == NULL) {
        return false;
    }
    first_length = strlen(first);
    while (first_length != 0 &&
        (first[first_length - 1] == '/' || first[first_length - 1] == '\\')) {
        first_length--;
    }
    while (second[second_offset] == '/' || second[second_offset] == '\\')
        second_offset++;
    written = snprintf(destination, capacity, "%.*s/%s", (int)first_length,
        first, second + second_offset);
    return written >= 0 && (size_t)written < capacity;
}

static void
normalize_path(char *path)
{
    for (char *cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '\\')
            *cursor = '/';
    }
}

static bool
path_is_absolute(const char *path)
{
    return path != NULL && (path[0] == '/' || path[0] == '\\' ||
        (isalpha((unsigned char)path[0]) && path[1] == ':'));
}

static char *
trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while (end != text && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return text;
}

static bool
file_metadata(const char *path, uint64_t *size, int64_t *modified)
{
#if defined(_WIN32)
    struct _stat64 status;

    if (_stat64(path, &status) != 0 || (status.st_mode & _S_IFREG) == 0)
        return false;
#else
    struct stat status;

    if (stat(path, &status) != 0 || !S_ISREG(status.st_mode))
        return false;
#endif
    *size = (uint64_t)status.st_size;
    *modified = (int64_t)status.st_mtime;
    return true;
}

static void
hash_input(airport_cache_manifest_t *manifest, const char *path)
{
    uint64_t size = 0;
    int64_t modified = 0;
    bool exists = file_metadata(path, &size, &modified);

    manifest->fingerprint = hash_string(manifest->fingerprint, path);
    manifest->fingerprint = hash_bytes(manifest->fingerprint, &exists,
        sizeof(exists));
    manifest->fingerprint = hash_bytes(manifest->fingerprint, &size,
        sizeof(size));
    manifest->fingerprint = hash_bytes(manifest->fingerprint, &modified,
        sizeof(modified));
    manifest->input_count++;
}

static bool
hash_scenery_pack(airport_cache_manifest_t *manifest,
    const char *xplane_directory, const char *pack)
{
    char pack_path[MANIFEST_PATH_CAPACITY];
    char apt_path[MANIFEST_PATH_CAPACITY];

    if (path_is_absolute(pack)) {
        if (snprintf(pack_path, sizeof(pack_path), "%s", pack) < 0 ||
            strlen(pack) >= sizeof(pack_path)) {
            return false;
        }
    } else if (!path_join(pack_path, sizeof(pack_path), xplane_directory,
        pack)) {
        return false;
    }
    if (!path_join(apt_path, sizeof(apt_path), pack_path,
        "Earth nav data/apt.dat")) {
        return false;
    }
    normalize_path(apt_path);
    hash_input(manifest, apt_path);
    return true;
}

bool
airport_cache_manifest_compute(const char *xplane_directory,
    airport_cache_manifest_t *manifest)
{
    char scenery_ini[MANIFEST_PATH_CAPACITY];
    char default_apt[MANIFEST_PATH_CAPACITY];
    char line[MANIFEST_PATH_CAPACITY];
    FILE *file;

    if (xplane_directory == NULL || manifest == NULL ||
        !path_join(scenery_ini, sizeof(scenery_ini), xplane_directory,
        "Custom Scenery/scenery_packs.ini")) {
        return false;
    }
    *manifest = (airport_cache_manifest_t){
        .fingerprint = FNV1A_OFFSET,
        .input_count = 0
    };
    manifest->fingerprint = hash_bytes(manifest->fingerprint,
        &((const unsigned){MANIFEST_FORMAT_VERSION}), sizeof(unsigned));

    file = fopen(scenery_ini, "rb");
    if (file != NULL) {
        while (fgets(line, sizeof(line), file) != NULL) {
            static const char prefix[] = "SCENERY_PACK ";
            char *entry = trim(line);

            if (strncmp(entry, prefix, sizeof(prefix) - 1) != 0)
                continue;
            entry = trim(entry + sizeof(prefix) - 1);
            if (entry[0] != '\0' && !hash_scenery_pack(manifest,
                xplane_directory, entry)) {
                fclose(file);
                return false;
            }
        }
        if (ferror(file)) {
            fclose(file);
            return false;
        }
        fclose(file);
    } else if (errno != ENOENT) {
        return false;
    }

    if (!path_join(default_apt, sizeof(default_apt), xplane_directory,
        "Resources/default scenery/default apt dat/Earth nav data/apt.dat")) {
        return false;
    }
    if (!file_metadata(default_apt, &(uint64_t){0}, &(int64_t){0}) &&
        !path_join(default_apt, sizeof(default_apt), xplane_directory,
        "Global Scenery/Global Airports/Earth nav data/apt.dat")) {
        return false;
    }
    normalize_path(default_apt);
    hash_input(manifest, default_apt);
    return true;
}

static bool
manifest_path(char *destination, size_t capacity, const char *cache_directory,
    const char *filename)
{
    return path_join(destination, capacity, cache_directory, filename);
}

bool
airport_cache_manifest_matches(const char *cache_directory,
    const airport_cache_manifest_t *manifest)
{
    char path[MANIFEST_PATH_CAPACITY];
    unsigned version = 0;
    uint64_t input_count = 0;
    uint64_t fingerprint = 0;
    FILE *file;

    if (cache_directory == NULL || manifest == NULL ||
        !manifest_path(path, sizeof(path), cache_directory,
        AIRPORT_CACHE_MANIFEST_FILENAME)) {
        return false;
    }
    file = fopen(path, "rb");
    if (file == NULL)
        return false;
    bool parsed = fscanf(file, "%u %" SCNu64 " %" SCNx64, &version,
        &input_count,
        &fingerprint) == 3;
    fclose(file);
    return parsed && version == MANIFEST_FORMAT_VERSION &&
        input_count == (uint64_t)manifest->input_count &&
        fingerprint == manifest->fingerprint;
}

bool
airport_cache_manifest_write(const char *cache_directory,
    const airport_cache_manifest_t *manifest)
{
    char path[MANIFEST_PATH_CAPACITY];
    FILE *file;

    if (cache_directory == NULL || manifest == NULL ||
        !manifest_path(path, sizeof(path), cache_directory,
        AIRPORT_CACHE_MANIFEST_FILENAME)) {
        return false;
    }
    file = fopen(path, "wb");
    if (file == NULL)
        return false;
    bool written = fprintf(file, "%u %" PRIu64 " %016" PRIx64 "\n",
        MANIFEST_FORMAT_VERSION, (uint64_t)manifest->input_count,
        manifest->fingerprint) > 0;
    if (fclose(file) != 0)
        written = false;
    return written;
}

bool
airport_cache_manifest_invalidate(const char *cache_directory)
{
    char cache_version[MANIFEST_PATH_CAPACITY];
    struct stat status;
    FILE *file;

    if (cache_directory == NULL ||
        !manifest_path(cache_version, sizeof(cache_version), cache_directory,
        "version")) {
        return false;
    }
    if (stat(cache_directory, &status) != 0)
        return errno == ENOENT;
    file = fopen(cache_version, "wb");
    if (file == NULL)
        return false;
    bool written = fputs("-1\n", file) >= 0;
    if (fclose(file) != 0)
        written = false;
    return written;
}
