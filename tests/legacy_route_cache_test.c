#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int bool_t;
typedef struct { double x, y, z; } vect3_t;
typedef struct { double lat, lon; } geo_pos2_t;
typedef struct { double lat, lon, alt; } geo_pos3_t;
typedef struct seg_s {
    struct seg_s *next;
    vect3_t start;
    double heading;
    int id, local;
} seg_t;
typedef struct { seg_t *head; } list_t;
typedef struct route_s {
    struct route_s *next;
    vect3_t pos_ecef;
    double hdg;
    list_t segs;
} route_t;
typedef struct { route_t *head; } avl_tree_t;

#define ASSERT(expr) assert(expr)
#define ASSERT3P(a, op, b) assert((a) op (b))
#define GEO_POS3(lat_, lon_, alt_) ((geo_pos3_t){ (lat_), (lon_), (alt_) })
#define AVL_NEXT(table, route) ((void)(table), (route)->next)
static avl_tree_t disk_cache;
static const int wgs84 = 0;
static unsigned writes;

static double vect3_dist(vect3_t a, vect3_t b) {
    return sqrt(pow(a.x-b.x, 2) + pow(a.y-b.y, 2) + pow(a.z-b.z, 2));
}
static double rel_hdg(double a, double b) { return remainder(b-a, 360); }
static vect3_t geo2ecef_mtr(geo_pos3_t pos, const int *datum) {
    (void)datum;
    /* Matching tests use metre coordinates; real geodesy is not under test. */
    return (vect3_t){pos.lat, pos.lon, pos.alt};
}
static void *safe_calloc(size_t count, size_t size) {
    void *result = calloc(count, size);
    assert(result != NULL);
    return result;
}
static seg_t *list_head(const list_t *list) { return list->head; }
static seg_t *list_next(const list_t *list, const seg_t *seg) {
    (void)list;
    return seg->next;
}
static void list_insert_tail(list_t *list, seg_t *seg) {
    seg_t **tail = &list->head;
    while (*tail != NULL)
        tail = &(*tail)->next;
    *tail = seg;
    seg->next = NULL;
}
static void seg_world2local(seg_t *seg) { seg->local = 1; }
static void list_free(list_t *list) {
    while (list->head != NULL) {
        seg_t *seg = list->head;
        list->head = seg->next;
        free(seg);
    }
}
static route_t *avl_first(avl_tree_t *table) { return table->head; }
static void avl_remove(avl_tree_t *table, route_t *route) {
    route_t **node = &table->head;
    while (*node != route) {
        assert(*node != NULL);
        node = &(*node)->next;
    }
    *node = route->next;
}
static void route_free(route_t *route) {
    list_free(&route->segs);
    free(route);
}
static void table_free(avl_tree_t *table) {
    while (table->head != NULL) {
        route_t *route = table->head;
        avl_remove(table, route);
        route_free(route);
    }
}
static list_t clone_segs(const list_t *source) {
    list_t result = {0};
    for (seg_t *seg = source->head; seg != NULL; seg = seg->next) {
        seg_t *copy = safe_calloc(1, sizeof(*copy));
        *copy = *seg;
        list_insert_tail(&result, copy);
    }
    return result;
}
static route_t *route_alloc(avl_tree_t *table, const list_t *segs) {
    route_t *route = safe_calloc(1, sizeof(*route));
    route->pos_ecef = segs->head->start;
    route->hdg = segs->head->heading;
    route->segs = clone_segs(segs);
    route->next = table->head;
    table->head = route;
    return route;
}
static avl_tree_t *routes_load(void) {
    avl_tree_t *table = safe_calloc(1, sizeof(*table));
    for (route_t *r = disk_cache.head; r != NULL; r = r->next)
        route_alloc(table, &r->segs);
    return table;
}
static int routes_store(avl_tree_t *table) {
    ++writes;
    table_free(&disk_cache);
    for (route_t *r = table->head; r != NULL; r = r->next)
        route_alloc(&disk_cache, &r->segs);
    return 1;
}
static void routes_free(avl_tree_t *table) {
    table_free(table);
    free(table);
}

#include "legacy_route_cache.inc"

static void cache_route(double x, double heading, int id) {
    seg_t seg = {.start = {x, 0, 0}, .heading = heading, .id = id};
    list_t list = {.head = &seg};
    route_alloc(&disk_cache, &list);
}

int main(void) {
    route_t anchor = {.pos_ecef = {0, 0, 0}, .hdg = 0};
    route_t candidate = {.pos_ecef = {30, 0, 0}, .hdg = 350};
    assert(legacy_route_matches(&candidate, &anchor));
    candidate.pos_ecef.x = 30.001;
    assert(!legacy_route_matches(&candidate, &anchor));
    candidate.pos_ecef.x = 1;
    candidate.hdg = 349.999;
    assert(!legacy_route_matches(&candidate, &anchor));

    list_t loaded = {0};
    route_load_legacy((geo_pos2_t){0, 0}, 90.04, &loaded);
    assert(loaded.head == NULL && writes == 0);
    cache_route(3, 90.0, 2);
    cache_route(1, 90.0, 1);
    cache_route(0.1, 110, 3);
    route_load_legacy((geo_pos2_t){0, 0}, 90.04, &loaded);
    assert(loaded.head != NULL && loaded.head->id == 1 && loaded.head->local);
    assert(loaded.head->next == NULL && writes == 0);
    list_free(&loaded);
    route_load_legacy((geo_pos2_t){100, 0}, 90.04, &loaded);
    assert(loaded.head == NULL);

    cache_route(100, 90, 4);
    seg_t saved_seg = {.start = {0, 0, 0}, .heading = 90.04, .id = 5};
    list_t saved = {.head = &saved_seg};
    route_save_legacy(&saved);
    assert(writes == 1);
    unsigned count = 0;
    for (route_t *r = disk_cache.head; r != NULL; r = r->next) {
        assert(r->segs.head->id == 3 || r->segs.head->id == 4 || r->segs.head->id == 5);
        ++count;
    }
    assert(count == 3);
    route_load_legacy((geo_pos2_t){1, 0}, 90, &loaded);
    assert(loaded.head != NULL && loaded.head->id == 5);
    list_free(&loaded);
    table_free(&disk_cache);

    /* Exercise multiple segment cloning and order, not just the anchor. */
    seg_t tail = {.start = {50, 0, 0}, .heading = 90, .id = 7};
    saved_seg.next = &tail;
    route_save_legacy(&saved);
    route_load_legacy((geo_pos2_t){1, 0}, 90, &loaded);
    assert(loaded.head->id == 5 && loaded.head->local);
    assert(loaded.head->next->id == 7 && loaded.head->next->local);
    assert(loaded.head->next->next == NULL);
    list_free(&loaded);
    table_free(&disk_cache);
    puts("Legacy route tolerance, recall and replacement tests passed.");
    return 0;
}
