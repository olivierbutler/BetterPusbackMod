/*
 * External interface: the ground-crew lines published to other plugins
 * (README-EXTERNAL-API.md, "Crew lines"). Pure mapping from msg.c's messages
 * to stable public numbers, keys and nominal English text; no XPLM or audio
 * dependencies.
 */

#ifndef _EXT_API_MSGS_H_
#define _EXT_API_MSGS_H_

#include "ground_ops_state.h"
#include "msg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BP_EXT_MSG_KEY_LEN 32
#define BP_EXT_MSG_TEXT_LEN 192
#define BP_EXT_MSG_CAPTION_LEN GROUND_OPS_CAPTION_LEN
#define BP_EXT_MSG_VOICE_LEN 64

/*
 * Public line numbers: 0 before any line, then one per crew line. They are
 * part of the interface and never change meaning; new lines get new numbers.
 */
typedef enum {
    BP_EXT_MSG_NONE = 0,
    BP_EXT_MSG_PLAN_START = 1,
    BP_EXT_MSG_PLAN_END = 2,
    BP_EXT_MSG_DRIVING_UP = 3,
    BP_EXT_MSG_READY_TO_CONNECT = 4,
    BP_EXT_MSG_READY_TO_CONNECT_BRAKE_SET = 5,
    BP_EXT_MSG_WINCH = 6,
    BP_EXT_MSG_CONNECTED = 7,
    BP_EXT_MSG_START_PUSHBACK = 8,
    BP_EXT_MSG_START_TOW = 9,
    BP_EXT_MSG_START_PUSHBACK_NO_ENGINE_START = 10,
    BP_EXT_MSG_START_TOW_NO_ENGINE_START = 11,
    BP_EXT_MSG_OPERATION_COMPLETE = 12,
    BP_EXT_MSG_DISCONNECTING = 13,
    BP_EXT_MSG_DONE_RIGHT = 14,
    BP_EXT_MSG_DONE_LEFT = 15,
    /* Spoken through X-Plane's speech (msg_speak), not recorded: */
    BP_EXT_MSG_DOORS_GPU_OPEN = 16,
    BP_EXT_MSG_LIGHTS_WARNING = 17,
    BP_EXT_MSG_SYSTEM = 18,
    /* The push-start lines when the crew removes the chocks first: */
    BP_EXT_MSG_START_PUSHBACK_CHOCKS = 19,
    BP_EXT_MSG_START_TOW_CHOCKS = 20,
    BP_EXT_MSG_START_PUSHBACK_CHOCKS_NO_ENGINE_START = 21,
    BP_EXT_MSG_START_TOW_CHOCKS_NO_ENGINE_START = 22
} bp_ext_msg_t;

int bp_ext_msg_public(message_t msg);
int bp_ext_msg_spoken_public(msg_spoken_t kind);
/* Machine key ("driving_up", the voice file's name; "system"); "" for none. */
const char *bp_ext_msg_key(int public_msg);
/*
 * The nominal English sentence the line says; "" for none and for system
 * messages, whose text is whatever was spoken.
 */
const char *bp_ext_msg_text(int public_msg);
/* The Ground Operations caption for the line. */
ground_ops_caption_t bp_ext_msg_caption(int public_msg);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_MSGS_H_ */
