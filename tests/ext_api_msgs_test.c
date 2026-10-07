#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ext_api_msgs.h"
#include "intl_test_stub.h"

static void
test_every_line_has_a_unique_number_key_text_and_caption(void)
{
    bool seen[MSG_NUM_MSGS + 1] = { false };

    for (int m = 0; m < MSG_NUM_MSGS; m++) {
        int pub = bp_ext_msg_public((message_t)m);

        assert(pub >= 1 && pub <= MSG_NUM_MSGS);
        assert(!seen[pub]);
        seen[pub] = true;
        assert(strlen(bp_ext_msg_key(pub)) > 0);
        assert(strlen(bp_ext_msg_key(pub)) < BP_EXT_MSG_KEY_LEN);
        assert(strlen(bp_ext_msg_text(pub)) > 0);
        assert(strlen(bp_ext_msg_text(pub)) < BP_EXT_MSG_TEXT_LEN);
        assert(bp_ext_msg_caption(pub) != GROUND_OPS_CAPTION_NONE);
        assert(strlen(ground_ops_caption_text(bp_ext_msg_caption(pub))) > 0);
        for (int other = 1; other < pub; other++)
            assert(strcmp(bp_ext_msg_key(other), bp_ext_msg_key(pub)) != 0);
    }
}

static void
test_published_numbers_and_keys_never_change(void)
{
    assert(bp_ext_msg_public(MSG_PLAN_START) == 1);
    assert(bp_ext_msg_public(MSG_DRIVING_UP) == 3);
    assert(bp_ext_msg_public(MSG_CONNECTED) == 7);
    assert(bp_ext_msg_public(MSG_START_PB) == 8);
    assert(bp_ext_msg_public(MSG_OP_COMPLETE) == 12);
    assert(bp_ext_msg_public(MSG_DONE_LEFT) == 15);
    assert(strcmp(bp_ext_msg_key(3), "driving_up") == 0);
    assert(strcmp(bp_ext_msg_key(7), "connected") == 0);
    assert(strcmp(bp_ext_msg_text(7),
        "Tow connected and bypass pin inserted. Release parking brake.") == 0);
}

static void
test_variants_drop_the_part_that_does_not_apply(void)
{
    /* Brake already set: no "Set parking brake". */
    assert(strstr(bp_ext_msg_text(BP_EXT_MSG_READY_TO_CONNECT),
        "Set parking brake") != NULL);
    assert(strstr(bp_ext_msg_text(BP_EXT_MSG_READY_TO_CONNECT_BRAKE_SET),
        "parking brake") == NULL);
    /* Engines cannot be started now: no "you may start engines". */
    assert(strstr(bp_ext_msg_text(BP_EXT_MSG_START_PUSHBACK),
        "start engines") != NULL);
    assert(strstr(bp_ext_msg_text(BP_EXT_MSG_START_PUSHBACK_NO_ENGINE_START),
        "start engines") == NULL);
}

static void
test_no_line_is_empty(void)
{
    assert(bp_ext_msg_public((message_t)MSG_NUM_MSGS) == BP_EXT_MSG_NONE);
    assert(strcmp(bp_ext_msg_key(BP_EXT_MSG_NONE), "") == 0);
    assert(strcmp(bp_ext_msg_text(BP_EXT_MSG_NONE), "") == 0);
    assert(bp_ext_msg_caption(BP_EXT_MSG_NONE) == GROUND_OPS_CAPTION_NONE);
    assert(strcmp(ground_ops_caption_text(GROUND_OPS_CAPTION_NONE), "") == 0);
}

static void
test_spoken_lines_follow_the_recordings(void)
{
    /* X-Plane speech lines get the numbers after the recordings. */
    assert(bp_ext_msg_spoken_public(MSG_SPOKEN_DOORS_GPU) == 16);
    assert(bp_ext_msg_spoken_public(MSG_SPOKEN_LIGHTS) == 17);
    assert(bp_ext_msg_spoken_public(MSG_SPOKEN_SYSTEM) == 18);
    assert(strcmp(bp_ext_msg_key(16), "doors_gpu_open") == 0);
    assert(strcmp(bp_ext_msg_key(17), "lights_warning") == 0);
    assert(strcmp(bp_ext_msg_key(18), "system") == 0);
    assert(strstr(bp_ext_msg_text(16), "GPU") != NULL);
    /* System messages vary: their text is whatever was spoken. */
    assert(strcmp(bp_ext_msg_text(18), "") == 0);
    for (int pub = 16; pub <= 18; pub++) {
        for (int m = 0; m < MSG_NUM_MSGS; m++)
            assert(strcmp(bp_ext_msg_key(pub),
                bp_ext_msg_key(bp_ext_msg_public((message_t)m))) != 0);
    }
    assert(strcmp(bp_ext_msg_key(23), "") == 0);
}

static void
test_chocks_lines_are_the_start_lines_chocks_first(void)
{
    /* Each push-start variant has its chocks line, after the speech lines. */
    static const struct {
        msg_spoken_t kind;
        int pub;
        message_t start;
        const char *key;
    } chocks[] = {
        { MSG_SPOKEN_CHOCKS_PB, 19, MSG_START_PB, "start_pb_chocks" },
        { MSG_SPOKEN_CHOCKS_TOW, 20, MSG_START_TOW, "start_tow_chocks" },
        { MSG_SPOKEN_CHOCKS_PB_NOSTART, 21, MSG_START_PB_NOSTART,
            "start_pb_chocks_nostart" },
        { MSG_SPOKEN_CHOCKS_TOW_NOSTART, 22, MSG_START_TOW_NOSTART,
            "start_tow_chocks_nostart" }
    };

    for (int i = 0; i < 4; i++) {
        const char *text = bp_ext_msg_text(chocks[i].pub);
        const char *start = bp_ext_msg_text(
            bp_ext_msg_public(chocks[i].start));

        assert(bp_ext_msg_spoken_public(chocks[i].kind) == chocks[i].pub);
        assert(strcmp(bp_ext_msg_key(chocks[i].pub), chocks[i].key) == 0);
        assert(strncmp(text, "Removing chocks, and beginning ", 31) == 0);
        assert(strlen(text) < BP_EXT_MSG_TEXT_LEN);
        /* Engines may be started exactly when the start line says so. */
        assert((strstr(text, "start engines") != NULL) ==
            (strstr(start, "start engines") != NULL));
        assert((strstr(text, "pushback") != NULL) ==
            (strstr(start, "pushback") != NULL));
        assert(bp_ext_msg_caption(chocks[i].pub) == GROUND_OPS_CAPTION_NONE);
    }
}

int
main(void)
{
    test_every_line_has_a_unique_number_key_text_and_caption();
    test_published_numbers_and_keys_never_change();
    test_variants_drop_the_part_that_does_not_apply();
    test_no_line_is_empty();
    test_spoken_lines_follow_the_recordings();
    test_chocks_lines_are_the_start_lines_chocks_first();
    printf("ext_api_msgs tests passed\n");
    return (0);
}
