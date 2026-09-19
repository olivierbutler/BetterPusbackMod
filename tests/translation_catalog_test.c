#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <acfutils/intl.h>

#include "ground_ops_state.h"

typedef struct {
    const char *language;
    const char *active;
    const char *parking_brake;
} translation_expectation_t;

static const translation_expectation_t expectations[] = {
    {"de", "AKTIV", "Parkbremse setzen"},
    {"es", "ACTIVO", "Aplique el freno de estacionamiento"},
    {"fr", "ACTIF", "Serrez le frein de parking"},
    {"it", "ATTIVO", "Inserire il freno di stazionamento"},
    {"pt", "ATIVO", "Acione o travão de estacionamento"},
    {"pt_BR", "ATIVO", "Acione o freio de estacionamento"},
    {"ru", "АКТИВНО", "Установите стояночный тормоз"},
    {"zh", "进行中", "设置停机刹车"}
};

static const char *const eyebrow_msgids[] = {
    "REVIEW", "COMPLETE", "PILOT ACTION", "ACTIVE", "WAITING",
    "GROUND CREW", "ACTION", "HOLD", "CONFIRMED", "READY", "CLEAR",
    "STANDBY"
};

void
XPLMDebugString(const char *message)
{
    (void)message;
}

void
log_impl(const char *filename, int line, const char *format, ...)
{
    va_list args;

    (void)fprintf(stderr, "%s:%d: ", filename, line);
    va_start(args, format);
    (void)vfprintf(stderr, format, args);
    va_end(args);
    (void)fputc('\n', stderr);
}

static bool
check_translation(const char *msgid, const char *expected)
{
    const char *actual = acfutils_xlate(msgid);

    if (strcmp(actual, expected) == 0)
        return (true);
    (void)fprintf(stderr, "translation mismatch for '%s': '%s' != '%s'\n",
        msgid, actual, expected);
    return (false);
}

int
main(int argc, char **argv)
{
    char path[1024];

    if (argc != 2) {
        (void)fprintf(stderr, "usage: %s REPOSITORY_ROOT\n", argv[0]);
        return (2);
    }

    for (size_t index = 0;
        index < sizeof (expectations) / sizeof (expectations[0]); index++) {
        const translation_expectation_t *expectation = &expectations[index];

        (void)snprintf(path, sizeof (path), "%s/data/po/%s/strings.po",
            argv[1], expectation->language);
        if (!acfutils_xlate_init(path)) {
            (void)fprintf(stderr, "failed to load %s\n", path);
            return (1);
        }
        if (!check_translation("ACTIVE", expectation->active) ||
            !check_translation("Set the parking brake",
                expectation->parking_brake) ||
            !check_translation("METAR -- | ATIS --",
                "METAR -- | ATIS --")) {
            acfutils_xlate_fini();
            return (1);
        }
        for (size_t msg_index = 0; msg_index < sizeof (eyebrow_msgids) /
            sizeof (eyebrow_msgids[0]); msg_index++) {
            const char *translated = acfutils_xlate(
                eyebrow_msgids[msg_index]);

            if (strlen(translated) >= GROUND_OPS_EYEBROW_LEN) {
                (void)fprintf(stderr,
                    "%s eyebrow translation is too long: %s\n",
                    expectation->language, translated);
                acfutils_xlate_fini();
                return (1);
            }
        }
        acfutils_xlate_fini();
    }

    puts("translation catalog tests passed");
    return (0);
}
