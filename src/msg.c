/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * CDDL HEADER END
*/
/*
 * Copyright 2017 Saso Kiselkov. All rights reserved.
 */

#include <string.h>
#include <errno.h>

#include <XPLMUtilities.h>

#include <acfutils/assert.h>
#include <acfutils/crc64.h>
#include <acfutils/dr.h>
#include <acfutils/helpers.h>
#include <acfutils/icao2cc.h>
#include <acfutils/intl.h>
#include <acfutils/log.h>
#include <acfutils/time.h>
#include <acfutils/wav.h>

#include "cfg.h"
#include "ext_api_voice.h"
#include "msg.h"
#include "xplane.h"

typedef struct {
    const char *const filename;
    wav_t *wav;
} msg_info_t;

static msg_info_t msgs[MSG_NUM_MSGS] = {
        {.filename = "plan_start.opus", .wav = NULL},
        {.filename = "plan_end.opus", .wav = NULL},
        {.filename = "driving_up.opus", .wav = NULL},
        {.filename = "ready2conn.opus", .wav = NULL},
        {.filename = "ready2conn_nopark.opus", .wav = NULL},
        {.filename = "winch.opus", .wav = NULL},
        {.filename = "connected.opus", .wav = NULL},
        {.filename = "start_pb.opus", .wav = NULL},
        {.filename = "start_tow.opus", .wav = NULL},
        {.filename = "start_pb_nostart.opus", .wav = NULL},
        {.filename = "start_tow_nostart.opus", .wav = NULL},
        {.filename = "op_complete.opus", .wav = NULL},
        {.filename = "disco.opus", .wav = NULL},
        {.filename = "done_right.opus", .wav = NULL},
        {.filename = "done_left.opus", .wav = NULL}
};

static bool_t inited = B_FALSE;
static dr_t sound_on;
static message_t last_msg = 0;
static uint64_t message_sequence = 0;
static uint64_t caption_started_us = 0;
static bool_t caption_issued = B_FALSE;
static alc_t *alc = NULL;
static char voice_pack[64] = "";
static bool_t last_spoken = B_FALSE;        /* the last line was msg_speak's */
static msg_spoken_t spoken_kind = MSG_SPOKEN_SYSTEM;
static char spoken_text[256] = "";

int msg_ext_voice_mode = 0;
int msg_ext_voice_done_seq = 0;
int msg_ext_voice_heartbeat = 0;
static int heartbeat_seen = 0;
static uint64_t heartbeat_us = 0;      /* when the heartbeat last moved */
static bool_t ext_was_active = B_FALSE;
static bool_t line_external = B_FALSE; /* last line handed to the external voice */
static uint64_t line_done_us = 0;      /* when the external voice finished it */
static uint64_t line_seq = 0;          /* its number (message_sequence) */

/*
 * This examines an optional "cc_aliases.cfg" file in our messages directory.
 * This can used to remap a country code to another code to for example
 * utilize the second country's audio set as a common set. One example of
 * this is en_GB, which also applies in British overseas territories.
 */
static void
alias_cc(const char *cc, char aliased_cc[3]) {
    char *path = mkpathname(bp_xpdir, bp_plugindir, "data", "msgs",
                            "cc_aliases.cfg", NULL);
    FILE *fp = fopen(path, "r");
    char cc_in[8], cc_out[8];

    free(path);
    if (fp == NULL)
        goto errout;

    while (!feof(fp)) {
        if (fscanf(fp, "%7s %7s", cc_in, cc_out) != 2)
            break;
        if (*cc_in == '#') {
            int c;
            do {
                c = fgetc(fp);
            } while (c != '\n' && c != '\r' && c != EOF);
            continue;
        }
        if (strcmp(cc, cc_in) == 0) {
            strlcpy(aliased_cc, cc_out, 3);
            fclose(fp);
            return;
        }
    }

    fclose(fp);
    errout:
    strlcpy(aliased_cc, cc, 3);
}

static char *
msg_pack_variant_select(char *base) {
    char **variants = NULL;
    size_t num = 0, pick = 0;
    char *winner;
    char *dname = mkpathname(bp_xpdir, bp_plugindir, "data", "msgs", NULL);
    DIR *dp = opendir(dname);
    int n = strlen(base);

    if (dp == NULL) {
        logMsg(BP_ERROR_LOG "Error opening %s: %s", dname, strerror(errno));
        free(dname);
        return (base);
    }
    for (struct dirent *de = readdir(dp); de != NULL; de = readdir(dp)) {
        if (strcmp(de->d_name, base) == 0 ||
            (strncmp(de->d_name, base, n) == 0 &&
             de->d_name[n] == '(')) {
            variants = realloc(variants,
                               (num + 1) * sizeof(*variants));
            variants[num++] = strdup(de->d_name);
        }
    }
    closedir(dp);

    free(dname);
    free(base);

    ASSERT(num != 0);
    ASSERT(variants);

    pick = crc64_rand() % num;
    winner = variants[pick];
    variants[pick] = NULL;
    for (size_t i = 0; i < num; i++)
        free(variants[i]);
    free(variants);

    return (winner);
}

bool_t
mgs_initiated(void) {
    return inited;
}

bool_t
msg_init(const char *my_lang, const char *icao, lang_pref_t lang_pref) {
    const char *arpt_cc = icao2cc(icao);
    const char *arpt_lang = icao2lang(icao);
    enum {
        MAX_MATCHES = 4
    };
    char match_set[MAX_MATCHES][8] = {{0},
                                      {0},
                                      {0},
                                      {0}};
    char *msg_dir_name = NULL;
    char cc[3];
    const char *radio_dev = NULL;
    bool_t shared_ctx = B_FALSE;

    (void) conf_get_str(bp_conf, "radio_device", &radio_dev);
    (void) conf_get_b(bp_conf, "shared_ctx", &shared_ctx);

    ASSERT(!inited);

    alc = openal_init(radio_dev, shared_ctx);
    if (alc == NULL)
        goto errout;

    if (arpt_cc != NULL)
        alias_cc(arpt_cc, cc);
    else
        strcpy(cc, "XX");

    switch (lang_pref) {
        case LANG_PREF_MATCH_REAL:
            /*
             * For match real our preference order is:
             * 1) try "my_lang_CC" for country-local accent of my language
             * 2) if arpt_lang == my_lang, try "my_lang" for generic
             *	language fallback
             * 3) try "en_CC" for country-local English accent
             * 4) try "en-arpt_lang" for generic English accent of local
             *	language
             */
            snprintf(match_set[0], sizeof(*match_set), "%s_%s",
                     my_lang, cc);
            if (strcmp(arpt_lang, my_lang) == 0)
                strlcpy(match_set[1], my_lang, sizeof(*match_set));
            snprintf(match_set[2], sizeof(*match_set), "en_%s", cc);
            snprintf(match_set[3], sizeof(*match_set), "en-%s", arpt_lang);
            CTASSERT(MAX_MATCHES > 3);
            break;
        case LANG_PREF_NATIVE:
            /*
             * For native, our preference order is:
             * 1) try "my_lang", ignoring any local accent
             * 2) try "my_lang_CC" for country-local accent
             */
            strlcpy(match_set[0], my_lang, sizeof(*match_set));
            snprintf(match_set[1], sizeof(*match_set), "%s_%s",
                     my_lang, cc);
            CTASSERT(MAX_MATCHES > 1);
            break;
        default:
            /*
             * For match-English, preference order is:
             * 1) try "en_CC" for country-local English accent
             * 2) try "en-arpt_lang" for generic English accent of local
             *	language
             */
            VERIFY3U(lang_pref, ==, LANG_PREF_MATCH_ENGLISH);
            snprintf(match_set[0], sizeof(*match_set), "en_%s", cc);
            snprintf(match_set[1], sizeof(*match_set), "en-%s", arpt_lang);
            CTASSERT(MAX_MATCHES > 1);
            break;
    };

    for (int i = 0; i < MAX_MATCHES; i++) {
        char *path;
        bool_t isdir;

        if (*match_set[i] == 0)
            continue;
        path = mkpathname(bp_xpdir, bp_plugindir, "data", "msgs",
                          match_set[i], NULL);
        if (file_exists(path, &isdir) && isdir) {
            free(path);
            msg_dir_name = strdup(match_set[i]);
            break;
        }
        free(path);
    }

    if (msg_dir_name == NULL) {
        /* final fallback for everything: generic English */
        msg_dir_name = strdup("en");
    }

    /*
     * To support multiple sound pack variants, we look through data/msgs
     * again to look for variations of 'msg_dir_name(XYZ)', where
     * msg_dir_name is the message directory we selected above. If there
     * are multiple matching ones, we randomly select one.
     */
    msg_dir_name = msg_pack_variant_select(msg_dir_name);

    for (message_t msg = 0; msg < MSG_NUM_MSGS; msg++) {
        char *path = mkpathname(bp_xpdir, bp_plugindir, "data",
                                "msgs", msg_dir_name, msgs[msg].filename, NULL);
        msgs[msg].wav = wav_load(path, msgs[msg].filename, alc);
        if (msgs[msg].wav == NULL) {
            logMsg(BP_ERROR_LOG "initialization error, unable "
                                "to load sound file %s (prefdir: %s)", path,
                   msg_dir_name);
            free(path);
            goto errout;
        }
        free(path);
    }

    strlcpy(voice_pack, msg_dir_name, sizeof (voice_pack));
    free(msg_dir_name);
    fdr_find(&sound_on, "sim/operation/sound/sound_on");

    inited = B_TRUE;

    return (B_TRUE);
    errout:
    free(msg_dir_name);
    for (message_t msg = 0; msg < MSG_NUM_MSGS; msg++) {
        if (msgs[msg].wav != NULL) {
            wav_free(msgs[msg].wav);
            msgs[msg].wav = NULL;
        }
    }
    if (alc != NULL) {
        openal_fini(alc);
        alc = NULL;
    }
    return (B_FALSE);
}

void
msg_fini(void) {
    if (!inited)
        return;
    for (message_t msg = 0; msg < MSG_NUM_MSGS; msg++) {
        if (msgs[msg].wav != NULL) {
            wav_free(msgs[msg].wav);
            msgs[msg].wav = NULL;
        }
    }
    if (alc != NULL) {
        openal_fini(alc);
        alc = NULL;
    }
    inited = B_FALSE;
    caption_issued = B_FALSE;
    voice_pack[0] = '\0';
    line_external = B_FALSE;
    line_done_us = 0;
}

const char *
msg_voice_pack(void)
{
    return (inited ? voice_pack : "");
}

void
msg_play(message_t msg) {
    VERIFY3U(msg, <, MSG_NUM_MSGS);
    ASSERT(inited);
    last_msg = msg;
    last_spoken = B_FALSE;
    message_sequence++;
    caption_started_us = microclock();
    caption_issued = B_TRUE;
    line_external = msg_ext_voice_active();
    line_seq = message_sequence;
    line_done_us = 0;
    if (line_external)
        return;         /* the external voice speaks it (bp/msg_seq moved) */
    if (dr_geti(&sound_on) == 0)
        return;
    // log convertion, we are controling here audio volume
    wav_set_gain(msgs[msg].wav, (double) (bp_ground_crew_audio_volume * bp_ground_crew_audio_volume));
    wav_play(msgs[msg].wav);
}

void
msg_stop(void) {
    ASSERT(inited);
    wav_stop(msgs[last_msg].wav);
    caption_issued = B_FALSE;
}

double
msg_dur(message_t msg) {
    VERIFY3U(msg, <, MSG_NUM_MSGS);
    ASSERT(inited);
    return (msgs[msg].wav->duration);
}

void
msg_get_caption_state(msg_caption_state_t *state)
{
    uint64_t duration_us;

    if (state == NULL)
        return;
    state->active = B_FALSE;
    state->message = last_msg;
    state->sequence = message_sequence;
    state->spoken = last_spoken;
    state->spoken_kind = spoken_kind;
    state->spoken_text = spoken_text;
    if (!inited || !caption_issued)
        return;

    duration_us = (uint64_t)(msg_dur_effective(last_msg) * 1000000.0);
    if (microclock() - caption_started_us <= duration_us)
        state->active = B_TRUE;
    else
        caption_issued = B_FALSE;
}

void
msg_speak(msg_spoken_t kind, const char *text)
{
    if (text == NULL)
        return;
    strlcpy(spoken_text, text, sizeof (spoken_text));
    spoken_kind = kind;
    last_spoken = B_TRUE;
    message_sequence++;
    /* The panel's caption shows recordings only; it must not repeat the last. */
    caption_issued = B_FALSE;
    if (!msg_ext_voice_active())
        XPLMSpeakString(text);   /* else the external voice says it */
}

void
msg_ext_voice_poll(void)
{
    bool_t active;

    if (msg_ext_voice_heartbeat != heartbeat_seen) {
        heartbeat_seen = msg_ext_voice_heartbeat;
        heartbeat_us = microclock();
    }
    /* Done once the external voice has finished the recorded line handed to
     * it (spoken lines that came after it do not hold the operation). */
    if (line_external && line_done_us == 0 && msg_ext_voice_done_seq > 0 &&
        (uint64_t)msg_ext_voice_done_seq >= line_seq)
        line_done_us = microclock();
    active = msg_ext_voice_active();
    if (active != ext_was_active) {
        logMsg(BP_INFO_LOG "External voice %s", active ?
            "active: ground crew lines are spoken by another plugin" :
            "inactive: BetterPushback speaks the ground crew lines");
        ext_was_active = active;
    }
}

bool_t
msg_ext_voice_active(void)
{
    return (msg_ext_voice_mode == 1 && heartbeat_us != 0 &&
        microclock() - heartbeat_us <
        (uint64_t)(BP_EXT_VOICE_ALIVE_S * 1000000.0));
}

/*
 * Every caller waits on the line it has just played (or on one of two
 * variants of it), so a line handed to the external voice is timed as the
 * last line played, whichever variant `msg` names.
 */
double
msg_dur_effective(message_t msg)
{
    double so_far, done_after;

    if (!line_external)
        return (msg_dur(msg));
    so_far = (microclock() - caption_started_us) / 1000000.0;
    done_after = line_done_us != 0 ?
        (line_done_us - caption_started_us) / 1000000.0 : 0;
    return (bp_ext_voice_effective_dur(msg_dur(last_msg), B_TRUE,
        msg_ext_voice_active(), line_done_us != 0, done_after, so_far));
}
