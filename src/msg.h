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

#ifndef    _MSG_H_
#define    _MSG_H_

#include <stdint.h>

#include <acfutils/types.h>

#ifdef    __cplusplus
extern "C" {
#endif

typedef enum {
    LANG_PREF_MATCH_REAL,
    LANG_PREF_NATIVE,
    LANG_PREF_MATCH_ENGLISH
} lang_pref_t;

typedef enum {
    MSG_PLAN_START,
    MSG_PLAN_END,
    MSG_DRIVING_UP,
    MSG_RDY2CONN,
    MSG_RDY2CONN_NOPARK,
    MSG_WINCH,
    MSG_CONNECTED,
    MSG_START_PB,
    MSG_START_TOW,
    MSG_START_PB_NOSTART,
    MSG_START_TOW_NOSTART,
    MSG_OP_COMPLETE,
    MSG_DISCO,
    MSG_DONE_RIGHT,
    MSG_DONE_LEFT,
    MSG_NUM_MSGS
} message_t;

/* What a line spoken through X-Plane's speech (msg_speak) is about. */
typedef enum {
    MSG_SPOKEN_DOORS_GPU,       /* a door open, the GPU or ASU connected */
    MSG_SPOKEN_LIGHTS,          /* landing or taxi lights on during the push */
    MSG_SPOKEN_SYSTEM,          /* a failure or warning about the plugin itself */
    /*
     * The push or tow starts and the crew first removes the chocks: says
     * MSG_START_PB, _TOW, _PB_NOSTART or _TOW_NOSTART, chocks first.
     */
    MSG_SPOKEN_CHOCKS_PB,
    MSG_SPOKEN_CHOCKS_TOW,
    MSG_SPOKEN_CHOCKS_PB_NOSTART,
    MSG_SPOKEN_CHOCKS_TOW_NOSTART
} msg_spoken_t;

typedef struct {
    bool_t active;
    message_t message;
    uint64_t sequence;
    /* The last line was spoken text (msg_speak), not a recording: */
    bool_t spoken;
    msg_spoken_t spoken_kind;
    const char *spoken_text;
} msg_caption_state_t;

bool_t msg_init(const char *my_lang, const char *icao, lang_pref_t lang_pref);

void msg_fini();

void msg_play(message_t msg);

void msg_stop(void);

bool_t mgs_initiated(void);

double msg_dur(message_t msg);

/* The voice pack chosen by msg_init (e.g. "en_GB"), "" when not initialized. */
const char *msg_voice_pack(void);

void msg_get_caption_state(msg_caption_state_t *state);

/*
 * Says `text` through X-Plane's speech and publishes it as a line like the
 * recordings (README-EXTERNAL-API.md, "Crew lines"). Works before msg_init.
 * With the external voice active it is only published, not spoken.
 */
void msg_speak(msg_spoken_t kind, const char *text);

/*
 * External voice (README-EXTERNAL-API.md): another plugin speaks the lines
 * instead of BetterPushback. It writes these through the bp/voice_* datarefs.
 */
extern int msg_ext_voice_mode;        /* 1: the external voice speaks */
extern int msg_ext_voice_done_seq;    /* the last line it finished (msg_seq) */
extern int msg_ext_voice_heartbeat;   /* it counts this up while alive */

/* Notices heartbeats and finished lines; called about ten times a second. */
void msg_ext_voice_poll(void);
/* The external voice is enabled and its heartbeat is current. */
bool_t msg_ext_voice_active(void);
/*
 * How long to treat `msg` as lasting: its recording, or for a line the
 * external voice is speaking, until that voice reports it finished (bounded).
 * The controller waits on this wherever it waits for a line to be spoken.
 */
double msg_dur_effective(message_t msg);

#ifdef    __cplusplus
}
#endif

#endif    /* _MSG_H_ */
