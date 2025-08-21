/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license in the file COPYING
 * or http://www.opensource.org/licenses/CDDL-1.0.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file COPYING.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2022 Saso Kiselkov. All rights reserved.
 * Copyright 2024 Robert Wellinger. All rights reserved.
 */

#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>

#include <png.h>

#include <XPLMCamera.h>
#include <XPLMGraphics.h>
#include <XPLMNavigation.h>
#include <XPLMScenery.h>
#include <XPLMUtilities.h>
#include <XPLMPlanes.h>
#include <XPLMProcessing.h>
#include <XPStandardWidgets.h>

#include <acfutils/assert.h>
#include <acfutils/dr.h>
#include <acfutils/dr_cmd_reg.h>
#include <acfutils/geom.h>
#include <acfutils/glew.h>
#include <acfutils/intl.h>
#include <acfutils/math.h>
#include <acfutils/list.h>
#include <acfutils/perf.h>
#include <acfutils/safe_alloc.h>
#include <acfutils/time.h>
#include <acfutils/wav.h>
#include <acfutils/widget.h>

#include "bp.h"
#include "bp_cam.h"
#include "cfg.h"
#include "msg.h"
#include "xplane.h"

#define    MIN_XPLANE_VERSION    11550    /* X-Plane 11.55 */
#define    MIN_XPLANE_VERSION_STR    "11.55"    /* X-Plane 11.55 */

#define    MAX_FWD_SPEED        4    /* m/s [~8 knots] */
#define    MAX_SPEED_MED_FRICTION    2    /* m/s */
#define    MAX_SPEED_POOR_FRICTION    1.11    /* m/s */
#define    MAX_REV_SPEED        1.11    /* m/s [4 km/h, "walking speed"] */
#define    NORMAL_ACCEL        0.25    /* m/s^2 */
#define    NORMAL_DECEL        0.17    /* m/s^2 */
#define    BRAKE_PEDAL_THRESH    0.03    /* brake pedal angle, 0..1 */
#define    FORCE_PER_TON        5000    /* max push force per ton, Newtons */
/*
 * X-Plane 10's tire model is a bit less forgiving of slow creeping,
 * so bump the minimum breakaway speed on that version.
 */
#define    BREAKAWAY_THRESH    (bp_xp_ver >= 11000 ? 0.09 : 0.35)
#define    SEG_TURN_MULT        0.9    /* leave 10% for oversteer */
#define    SPEED_COMPLETE_THRESH    0.08    /* m/s */
#define    MIN_STEER_ANGLE        35    /* minimum sensible tire steer angle */
#define    MAX_FWD_ANG_VEL        6    /* degrees per second */
#define    MAX_REV_ANG_VEL        4    /* degrees per second */
#define    MAX_CENTR_ACCEL        0.1    /* m/s^2 */
#define    PB_CRADLE_DELAY        10    /* seconds */
#define    PB_CONN_LIFT_DELAY    13.0    /* seconds */
#define    PB_CONN_LIFT_DURATION    9.0    /* seconds */
#define    PB_START_DELAY        5    /* seconds */
#define    PB_LIFT_TE        0.075    /* fraction */
#define    STATE_TRANS_DELAY    2    /* seconds, state transition delay */
#define    CLEAR_SIGNAL_DELAY    15    /* seconds */
#define    TUG_DRIVE_AWAY_DIST    80    /* meters */
#define    MAX_DRIVING_AWAY_DELAY    30    /* seconds */

#define    TUG_APPCH_LONG_DIST    (6 * bp_ls.tug->veh.wheelbase)
#define    TUG_APPCH_SHORT_DIST    (2 * bp_ls.tug->veh.wheelbase)

#define    MIN_RADIO_VOLUME_THRESH    0.1
#define    MIN_STEP_TIME        0.001    /* minimum simulation step in secs */

#define    MSG_DOORS_GPU "Some doors are still opened or the GPU or the ASU are still connected. I'm waiting for all of them closed and disconnected then I will proceed."
#define	   HINTBAR_HEIGHT	20

/*
 * When stopping the operation, tug and aircraft steering deflections must
 * be below these thresholds before we let the aircraft come to a complete
 * stop. Otherwise we continue pushing/towing at MIN_SPEED_XP10 to let the
 * steering straighten out.
 */
#define    TOW_COMPLETE_TUG_STEER_THRESH    5    /* degrees */
#define    TOW_COMPLETE_ACF_STEER_THRESH    2.5    /* degrees */

/*
 * When we get within this distance of the end of a straight segment that
 * terminates our pushback path, we neutralize steering to be able to stop
 * exactly on the dot.
 */
#define    NEARING_END_THRESHOLD        1    /* meters */

enum {
    RWY_FRICTION_GOOD = 0,
    RWY_FRICTION_MED = 1,
    RWY_FRICTION_POOR = 2
};

typedef struct {
    const char *acf;
    const char *author;
} acf_info_t;

static struct {
    dr_t lbrake, rbrake;
    dr_t pbrake, pbrake_rat;
    bool_t pbrake_is_custom;
    dr_t rot_force_M, rot_force_N;
    dr_t axial_force;
    dr_t override_planepath;
    dr_t local_x, local_y, local_z;
    dr_t local_vx, local_vy, local_vz;
    dr_t lat, lon;
    dr_t pitch, roll, hdg, quaternion;
    dr_t sim_time;
    dr_t acf_mass;
    dr_t mtow;
    dr_t tire_z, tire_x, leg_len, tirrad, tire_rot_spd;
    dr_t nw_steerdeg1, nw_steerdeg2;
    dr_t tire_steer_cmd;
    dr_t override_steer;
    dr_t nw_steer_on;
    dr_t gear_types;
    dr_t gear_steers;
    dr_t gear_on_ground;
    dr_t onground_any;
    dr_t gear_deploy;
    dr_t num_engns;
    dr_t engn_running;
    dr_t acf_livery_path;
    dr_t rwy_friction;

    dr_t landing_lights_on;
    dr_t taxi_light_on;

    dr_t beacon_light;
    dr_t joystick;
    dr_t author;
    dr_t sim_paused;
} drs;

#define MAX_DOOR 20
static struct {
	const char	ICAO[8];
	const char	acf_filename[64];
	const char	studio[64];
    bool_t      info_valid;
    bool_t      info_initialised;
	int		    nb_doors;
	char	    dr[MAX_DOOR][64];
	bool_t	    dr_neg[MAX_DOOR];
} doors_info = {0};

bp_state_t bp = {0};
bp_long_state_t bp_ls = {0};

static bool_t inited = B_FALSE;
static XPLMFlightLoopID bp_floop = NULL;

static bool_t cfg_disco_when_done = B_FALSE;

static bool_t cfg_ignore_park_break = B_FALSE;

bool_t tug_starts_next_plane = B_FALSE;
bool_t tug_auto_start = B_FALSE;
static int previous_beacon;
bool_t tug_pending_mode;

push_manual_t push_manual = {0};

static XPWidgetID bp_hint_status = NULL;
const char *bp_hint_status_str = NULL;
const char *bp_hint_previous_status_str = NULL;

static bool_t read_acf_file_info(void);

static float bp_run(float elapsed, float elapsed2, int counter, void *refcon);

static void bp_complete(void);

static void tug_pos_update(vect2_t my_pos, double my_hdg, bool_t pos_only);

static void disco_intf_hide(void);

static void main_intf_show(void);

void main_intf_hide(void);

static int disco_handler(XPLMCommandRef, XPLMCommandPhase, void *);

static int recon_handler(XPLMCommandRef, XPLMCommandPhase, void *);

static bool_t bp_run_push_manual(void);

static bool_t radio_volume_warn = B_FALSE;

static const acf_info_t incompatible_acf[] = {
        {.acf = NULL, .author = NULL}
};

static XPLMCommandRef disco_cmd = NULL, recon_cmd = NULL;
static button_t disco_buttons[] = {
        {.filename = "disconnect.png", .vk = -1, .tex = 0, .tex_data = NULL},
        {.filename = "reconnect.png", .vk = -1, .tex = 0, .tex_data = NULL},
        {.filename = NULL},
};

static button_t magic_buttons[] = {
        {.filename = "planner.png", .vk = -1, .tex = 0, .tex_data = NULL, .wind_id = NULL},
        {.filename = "conn_first_mb.png", .vk = -1, .tex = 0, .tex_data = NULL, .wind_id = NULL},
        {.filename = "push-back.png", .vk = -1, .tex = 0, .tex_data = NULL, .wind_id = NULL},
        {.filename = "status.png", .vk = -1, .tex = 0, .tex_data = NULL, .wind_id = NULL},
        {.filename = NULL},
};

/*
 * This flag is set by the planner if the user clicked on the "connect first"
 * button. This commands us to start pushback without a plan, but stop just
 * short of actually starting to move the aircraft. This is used when the
 * pushback direction isn't known ahead of time and the tower assigns the
 * direction at the last moment. The user can attach the tug and wait for
 * pushback clearance, then do a quick plan and immediately commence pushing.
 */
bool_t late_plan_requested = B_FALSE;

static double
max_steer_angle(void) {
    switch (dr_geti(&drs.rwy_friction)) {
        case RWY_FRICTION_MED:
            return (50);
        case RWY_FRICTION_POOR:
            return (35);
        default:
            return (bp_xp_ver < 11000 ? 50 : 75);
    }
}

static bool_t
pbrake_is_set(void) {
    bool_t result;
    
    if(slave_mode && pb_set_override) 
        return pb_set_remote;

    if (drs.pbrake_is_custom) {
        result = (dr_getf(&drs.pbrake) != 0);
    } else {
        result = (dr_getf(&drs.pbrake) != 0 || dr_getf(&drs.pbrake_rat) != 0);
    }
    return result;
}

/*
 * Checks if ANY engine of the aircraft is running.
 */
static bool_t
eng_is_running(void) {
    int num_engns = MIN(dr_geti(&drs.num_engns), 100);
    int engn_running[num_engns];

    dr_getvi(&drs.engn_running, engn_running, 0, num_engns);
    for (int i = 0; i < num_engns; i++) {
        if (engn_running[i] != 0) {
            return (B_TRUE);
        }
    }
    return (B_FALSE);
}

/*
 * Returns true if the engines may be started during pushback. Engines may
 * be started IF:
 *	1) there are two or more engines (i.e. they are on the wings and
 *	   won't risk hitting the tug.
 *	2) if there is one engine only, it may be started it if is a jet
 *	   engine. Civillian jet engines generally do not have their intake
 *	   on the nose of the aircraft.
 */
static bool_t
eng_ok2start(void) {
    dr_t eng_type_dr;
    int eng_type;

    if (dr_geti(&drs.num_engns) > 1)
        return (B_TRUE);

    fdr_find(&eng_type_dr, "sim/aircraft/prop/acf_en_type");
    eng_type = dr_geti(&eng_type_dr);

    /*
     * From X-Plane's DataRefs.txt, the engine types are:
     *	0=recip carb		(prop, not OK to start)
     *	1=recip injected	(prop, not OK to start)
     *	2=free turbine		(prop, not OK to start)
     *	3=electric		(prop, not OK to start)
     *	4=lo bypass jet		(jet, OK to start)
     *	5=hi bypass jet		(jet, OK to start)
     *	6=rocket		(don't care, doesn't exist)
     *	7=multi spool jet  (don't care, doesn't exist)
     *	8=fixed turbine		(prop, not OK to start)
     */
    return (eng_type >= 4 && eng_type <= 5);
}

/*
 * Determines if an aircraft is likely to be an airliner.
 */
bool_t
acf_is_airliner(void) {
    /* For our purposes, airliners don't exist in the light category. */
    enum {
        AIRLINE_MIN_MTOW = 7000
    };
    bool_t result = (dr_getf(&drs.mtow) >= AIRLINE_MIN_MTOW &&
                     !bp.acf.model_flags.is_experimental &&
                     !bp.acf.model_flags.is_general_aviation &&
                     !bp.acf.model_flags.is_glider &&
                     !bp.acf.model_flags.is_helicopter &&
                     !bp.acf.model_flags.is_military &&
                     !bp.acf.model_flags.is_sci_fi &&
                     !bp.acf.model_flags.is_ultralight &&
                     !bp.acf.model_flags.is_vtol &&
                     !bp.acf.model_flags.fly_like_a_helo);

    return result;
}

void
read_acf_airline(char airline[1024]) {
    int n;
    char *p;

    (void) dr_gets(&drs.acf_livery_path, airline, 1024);
    n = strlen(airline);
    /* strip the final directory separator */
    if (n > 0) {
        airline[n - 1] = '\0';
        n--;
    }
    /* strip away any leading path components, leave only the last one */
    if ((p = strrchr(airline, '/')) != NULL) {
        int l;
        p++;
        l = n - (p - airline);
        memmove(airline, p, l + 1);
        n -= l;
    }
    if ((p = strrchr(airline, '\\')) != NULL) {
        int l;
        p++;
        l = n - (p - airline);
        memmove(airline, p, l + 1);
        n -= l;
    }
}

/*
 * On single-engine prop aircraft we must rotate the propeller prior to
 * attaching so that the blades are as far away from the ground as possible,
 * so they don't catch on our tug. Any other aircraft type, we leave alone.
 */
static void
prop_single_adjust(void) {
    dr_t eng_type_dr, prop_angle_dr, num_blades_dr;
    int eng_type, num_blades;

    if (dr_geti(&drs.num_engns) > 1)
        return;
    fdr_find(&eng_type_dr, "sim/aircraft/prop/acf_en_type");
    eng_type = dr_geti(&eng_type_dr);
    /* See eng_ok2start for engine type designators */
    if (eng_type > 3 && eng_type < 8)
        return;
    fdr_find(&prop_angle_dr,
             "sim/flightmodel2/engines/prop_rotation_angle_deg");
    fdr_find(&num_blades_dr, "sim/aircraft/prop/acf_num_blades");
    num_blades = dr_geti(&num_blades_dr);
    if (num_blades % 2 == 1) {
        /* odd numbers of blades mean we always go to 0 degrees */
        dr_setf(&prop_angle_dr, 0);
    } else {
        /* even numbers we rotate to put a gap at the bottom */
        dr_setf(&prop_angle_dr, 180 / num_blades);
    }
}

static void
brakes_set(bool_t flag) {
    /*
     * Maximum we can set is 0.9. Any more and we might kick the parking
     * brake off.
     */
    double val = (flag ? 0.9 : 0.0);
    ASSERT(!slave_mode);
    dr_setf(&drs.lbrake, val);
    dr_setf(&drs.rbrake, val);
}

/*
 * Initializes the doors dataref list
 *
 * This function attempts to match the currently loaded aircraft with our
 * door datarefs in  BetterPushback_doors.cfg. The file is
 * parsed here. It consists of a set of whitespace-separated keywords with
 * optional arguments. String arguments allow for "%XY" escape sequences.
 * See unescape_percent in helpers.c.
 * A typical config file will consists from one or more blocks like this:
 *	icao	ABCD
 *	studio	Foo%20Bar%20Studios
 *	author	Bob%20The%20Aircraft%20Builder
 *	acf	WrightFlyer3000.acf
 *	door	737u/doors/L1
 *	door	737u/doors/L2
 *  door	@737u/doors/cargos     <-- @ indicate here that this dataref returns an array
 *  door!	laminar/B738/gpu_available     <-- ! indicates here that a value below 0.1 is considered as open or active

 * These keywords have the following meanings:
 *	icao (required): Denotes the start of an aircraft block and must be
 *		followed by a 4-letter ICAO aircraft type identifier (e.g.
 *		"B752"). This must be matched by the ICAO identifier of the
 *		currently loaded aircraft.
 *	studio (optional): When specified, checks if the currently loaded
 *		aircraft's studio (as defined in Plane Maker) matches the
 *		string argument.
 *	author (optional): When specified, checks if the currently loaded
 *		aircraft's author (as defined in Plane Maker) matches the
 *		string argument.
 *	acf	(optional): When specified, checks if the currently loaded
 *		aircraft's ACF filename matches the string argument.
 *	door or door! (required): the dataref of 1 door. you may provide multiple 
 *	    door block as necessary ( maximum 20 per aircraft )
 *		format can be 737u/doors/L1 or @737u/doorsarray in case of an array
 *      with door tag,  if the value of the door dataref is below 0.1, the door/GPU/ASU is considered as closed or inactive
 *      with door! tag, if the value of the door/GPU/ASU dataref is below 0.1, the door/GPU/ASU is considered as open or active
*/
static void
doors_refs_init(void)
{
	char		buf[128] = { 0 };
	FILE		*fp;
	char		*filename;
	char		my_icao[8] = { 0 }, my_author[256] = { 0 };
	char		my_studio[256] = { 0 }, my_acf[256] = { 0 };
	char		acf_path[512] = { 0 };
	char		*line = NULL;
	size_t		line_len = 0;
	dr_t		icao_dr, auth_dr;
	bool_t		skip = B_FALSE;

    memset(&doors_info, 0, sizeof(doors_info));
    // we flag here that doors_refs_init was executed
    doors_info.info_initialised = B_TRUE;
	fdr_find(&icao_dr, "sim/aircraft/view/acf_ICAO");
	fdr_find(&auth_dr, "sim/aircraft/view/acf_author");

	XPLMGetNthAircraftModel(0, my_acf, acf_path);
	dr_gets(&icao_dr, my_icao, sizeof (my_icao));
	dr_gets(&auth_dr, my_author, sizeof (my_author));

	/*
	 * Unfortunately the studio isn't available via datarefs, so parse
	 * our acf file instead.
	 */
	fp = fopen(acf_path, "r");
	if (fp == NULL)
		return;
	while (getline(&line, &line_len, fp) > 0) {
		if (strstr(line, "P acf/_studio ") == line) {
			strip_space(line);
			strlcpy(my_studio, &line[14], sizeof (my_studio));
			break;
		}
	}
	fclose(fp);


	filename = mkpathname(bp_xpdir, "Output", "preferences",  "BetterPushback_doors.cfg", NULL);
	fp = fopen(filename, "r");
	free(filename);
	if (fp == NULL) {
		filename = mkpathname(bp_xpdir, bp_plugindir, "BetterPushback_doors.cfg", NULL); 
		fp = fopen(filename, "r");
		free(filename);
		if (fp == NULL) {
			return;
		}
		logMsg("found : BetterPushback_doors.cfg in plugins folder");	
	} else {
	logMsg("found : BetterPushback_doors.cfg in Output/preferences folder");
	}

#define	FILTER_PARAM(param) \
	do { \
		char param[256]; \
		int res; \
		if (!doors_info.info_valid) \
			continue; \
		if (fscanf(fp, "%255s", param) != 1) { \
			logMsg("Error parsing BetterPushback_doors.cfg: expected " \
			    "string following \"" #param "\"."); \
			goto errout; \
		} \
		unescape_percent(param); \
		res = strcmp(param, my_ ## param); \
		if (res != 0) { \
			doors_info.info_valid = B_FALSE; \
			skip = B_TRUE; \
		} \
	} while (0)


	while (!feof(fp) && fscanf(fp, "%127s", buf) == 1) {
		if (buf[0] == '#') {
			while (fgetc(fp) != '\n' && !feof(fp))
				;
			continue;
		}
		if (strcmp(buf, "icao") == 0) {
			char icao[8];
			int res;
			if (doors_info.info_valid) {
				/* We're done parsing the entry we wanted */
				break;
			}
			if (fscanf(fp, "%7s", icao) != 1) {
				logMsg("Error parsing BetterPushback_doors.cfg: "
				    "expected string following \"icao\".");
				goto errout;
			}
			unescape_percent(icao);
			res = strcmp(icao, my_icao);

			if (res == 0) {
                doors_info.info_valid = B_TRUE;
                skip = B_FALSE;
			} else {
				skip = B_TRUE;
			}
		} else if (strcmp(buf, "studio") == 0) {
			FILTER_PARAM(studio);
		} else if (strcmp(buf, "acf") == 0) {
			FILTER_PARAM(acf);
		} else if (strcmp(buf, "author") == 0) {
			FILTER_PARAM(author);
		} else if ( (strcmp(buf, "door") == 0) || (strcmp(buf, "door!") == 0) ) {
			if ((!doors_info.info_valid) || (doors_info.nb_doors >= MAX_DOOR -1) )
				continue;
    		if (fscanf(fp, "%64s", doors_info.dr[doors_info.nb_doors]) != 1) { 
	    		logMsg("Error parsing BetterPushback_doors.cfg: expected " 
		    	    "string following \"door\"."); 
			    goto errout; 
		        } else {
                    doors_info.dr_neg[doors_info.nb_doors] = (strcmp(buf, "door!") == 0);
                    doors_info.nb_doors++;
                }
		}  else if (!skip) {
			logMsg("Error parsing BetterPushback_doors.cfg: "
			    "unknown keyword \"%s\".", buf);
			goto errout;
		}
	}
#undef	FILTER_PARAM

	fclose(fp);
	return;

errout:
    doors_info.nb_doors = 0;
    doors_info.info_valid = B_FALSE;
    logMsg("Fail reading doors info :%d", doors_info.nb_doors);
	fclose(fp);
}

/* check the  door status 
* return true if the door is closed OR if the dataref is not found 
* this avoid to block the process 
*/
bool_t
dr_door_check(char *dr) {
    dr_t door;
    if (dr_find(&door, "%s", dr)) {
        if (dr_getf(&door) > 0.1) {
            return B_FALSE;
        }
    }
    return B_TRUE;
}

bool_t
dr_door_check_vf32(char *dr) {
    dr_t door;
    int vf_size;
    float door_pos;
    if (dr_find(&door, "%s", dr)) {
        vf_size = dr_getvf32(&door, NULL , 0, 0);
        for ( int i=0; i < vf_size; i++) {
            dr_getvf32(&door, &door_pos , i, 1);
            if (door_pos > 0.1) {
                return B_FALSE;
            }
        }  
    }
    return B_TRUE;
}

bool_t
acf_doors_closed(bool_t with_cfg_flag) {
    bool_t result = B_TRUE;

    if  (!doors_info.info_initialised) {
        doors_refs_init();
    }

    if (with_cfg_flag) {
        int doors_check;
        conf_get_i_per_acf((char *)"doors_check", &doors_check);
        if (doors_check == Ignore) {
            return result;
        }
    }

    
    for (int i = 0 ; i< doors_info.nb_doors ; i++) {
        if (doors_info.dr[i][0] == '@') {
            result = dr_door_check_vf32(doors_info.dr[i]+1);
        } else {
            result = dr_door_check(doors_info.dr[i]);
        }
        result = doors_info.dr_neg[i] ? !result : result;
        if (!result) 
            break;
    }

    return result;
}

bool_t
acf_is_compatible(void) {
    char my_acf[512], my_path[512];
    char my_author[512];

    XPLMGetNthAircraftModel(0, my_acf, my_path);
    dr_gets(&drs.author, my_author, sizeof(my_author));

    for (int i = 0; incompatible_acf[i].acf != NULL; i++) {
        if (strcmp(incompatible_acf[i].acf, my_acf) == 0 &&
            (incompatible_acf[i].author == NULL ||
             strcmp(incompatible_acf[i].author, my_author) == 0))
            return (B_FALSE);
    }

    return (B_TRUE);
}

/*
 * Locates the airport nearest to our current location, but which is also
 * within 10km (MAX_ARPT_DIST). If a suitable airport is found, its ICAO
 * code is placed in the return argument `icao' and the function returns
 * B_TRUE. Otherwise the variable is left untouched and B_FALSE is returned.
 */
bool_t
find_nearest_airport(char icao[8]) {
    geo_pos2_t my_pos = GEO_POS2(dr_getf(&drs.lat), dr_getf(&drs.lon));
    vect3_t my_pos_ecef = geo2ecef_mtr(GEO_POS3(my_pos.lat, my_pos.lon, 0),
                                       &wgs84);
    list_t *list;
    airport_t *arpt;
    double min_dist = 1e10;

    *icao = 0;

    load_nearest_airport_tiles(airportdb, my_pos);
    list = find_nearest_airports(airportdb, my_pos);

    for (arpt = list_head(list); arpt != NULL;
         arpt = list_next(list, arpt)) {
        double dist = vect3_dist(arpt->ecef, my_pos_ecef);
        if (dist < min_dist) {
            strlcpy(icao, arpt->icao, sizeof(arpt->icao));
            min_dist = dist;
        }
    }
    free_nearest_airport_list(list);
    unload_distant_airport_tiles(airportdb, NULL_GEO_POS2);

    return (*icao != 0);
}

static void
bp_gather(void) {
    /*
     * CAREFUL!
     * X-Plane's north-south axis (Z) is flipped to our understanding, so
     * whenever we access 'local_z' or 'vz', we need to flip it.
     */
    bp.cur_pos.pos = VECT2(dr_getf(&drs.local_x),
                           -dr_getf(&drs.local_z));
    bp.cur_pos.hdg = normalize_hdg(dr_getf(&drs.hdg));
    bp.cur_pos.spd = vect2_dotprod(hdg2dir(bp.cur_pos.hdg),
                                   VECT2(dr_getf(&drs.local_vx), -dr_getf(&drs.local_vz)));
    bp.cur_t = dr_getf(&drs.sim_time);
}

static void
reorient_aircraft(double d_roll, double d_pitch, double d_hdg) {
    double phi = dr_getf(&drs.roll) + d_roll;
    double phi_mod = DEG2RAD(phi) / 2;
    double sin_phi_mod = sin(phi_mod), cos_phi_mod = cos(phi_mod);
    double theta = dr_getf(&drs.pitch) + d_pitch;
    double theta_mod = DEG2RAD(theta) / 2;
    double sin_theta_mod = sin(theta_mod), cos_theta_mod = cos(theta_mod);
    double psi = dr_getf(&drs.hdg) + d_hdg;
    double psi_mod = DEG2RAD(psi) / 2;
    double sin_psi_mod = sin(psi_mod), cos_psi_mod = cos(psi_mod);
    double q[4];

    q[0] = cos_psi_mod * cos_theta_mod * cos_phi_mod +
           sin_psi_mod * sin_theta_mod * sin_phi_mod;
    q[1] = cos_psi_mod * cos_theta_mod * sin_phi_mod -
           sin_psi_mod * sin_theta_mod * cos_phi_mod;
    q[2] = cos_psi_mod * sin_theta_mod * cos_phi_mod +
           sin_psi_mod * cos_theta_mod * sin_phi_mod;
    q[3] = -cos_psi_mod * sin_theta_mod * sin_phi_mod +
           sin_psi_mod * cos_theta_mod * cos_phi_mod;

    dr_setvf(&drs.quaternion, q, 0, 4);
}

/*
 * Computes the distance from the tug's fixed steering (rear) axle
 * to the aircraft's nosewheel.
 */
static double
tug_rear2acf_nw(void) {
    double nlg_tug_z_off;
    switch (bp_ls.tug->info->lift_wall_loc) {
        case LIFT_WALL_FRONT:
            nlg_tug_z_off = bp_ls.tug->info->lift_wall_z - bp.acf.tirrad;
            break;
        case LIFT_WALL_CENTER:
            nlg_tug_z_off = bp_ls.tug->info->lift_wall_z;
            break;
        default:
            ASSERT3U(bp_ls.tug->info->lift_wall_loc, ==, LIFT_WALL_BACK);
            nlg_tug_z_off = bp_ls.tug->info->lift_wall_z + bp.acf.tirrad;
            break;
    }
    return (nlg_tug_z_off - bp_ls.tug->veh.fixed_z_off);
}

static void
turn_nosewheel(double req_steer) {
    int dir_mult = (bp_ls.tug->pos.spd >= 0 ? 1 : -1);
    double cur_nw_steer, tug_turn_r, tug_turn_rate, rel_tug_turn_rate;
    double d_steer, nlg_tug_rear_off, d_hdg, turn_inc;
    vect2_t off_v;

    cur_nw_steer = rel_hdg(bp.cur_pos.hdg, bp_ls.tug->pos.hdg);

    /* limit the steering request to what we can actually do */
    req_steer = MIN(req_steer, bp.veh.max_steer);
    req_steer = MAX(req_steer, -bp.veh.max_steer);

    if (ABS(bp_ls.tug->cur_steer) > 0.01) {
        tug_turn_r = (1 / tan(DEG2RAD(bp_ls.tug->cur_steer))) *
                     bp_ls.tug->veh.wheelbase;
    } else {
        tug_turn_r = 1e10;
    }
    tug_turn_rate = (bp_ls.tug->pos.spd / (2 * M_PI * tug_turn_r)) * 360;
    rel_tug_turn_rate = tug_turn_rate - bp.d_pos.hdg / bp.d_t;

    cur_nw_steer += rel_tug_turn_rate * bp.d_t;
    cur_nw_steer = MIN(cur_nw_steer, 85);
    cur_nw_steer = MAX(cur_nw_steer, -85);
    d_steer = req_steer - cur_nw_steer;

    if (ABS(bp_ls.tug->pos.spd) > 0.01) {
        /*
         * Limit steering of the tug at high speeds to prevent the
         * tug swinging like crazy around.
         */
        double tug_steer = dir_mult * 3 * d_steer;
        double speed;

        tug_steer = MIN(MAX(tug_steer, -bp_ls.tug->veh.max_steer),
                        bp_ls.tug->veh.max_steer);
        speed = ang_vel_speed_limit(&bp_ls.tug->veh, tug_steer,
                                    bp_ls.tug->pos.spd);
        if (speed < bp_ls.tug->pos.spd)
            tug_steer *= speed / bp_ls.tug->pos.spd;
        tug_set_steering(bp_ls.tug, tug_steer, bp.d_t);
    }

    dr_setvf(&drs.tire_steer_cmd, &cur_nw_steer, bp.acf.nw_i, 1);

    /*
     * Since the nosewheel always isn't exactly over the tug's fixed
     * steering axle, we need to manually shift the aircraft's heading,
     * so as appear as if it steering around the tug's fixed steering
     * axle. We do so by calculating an incremental lateral displacement
     * from the aircraft's point of view.
     * nlg_tug_z_off: is the long offset along the tug's axis of the
     *	centerpoint of the aircraft's nose landing gear.
     * nlg_tug_rear_off: is the long offset along the tug's axis of
     *	the center of the aircraft's nose landing gear relative to
     *	where the fixed steering axle is located. This forms a
     *	similar triangle to the triangle being formed when the tug's
     *	steering turns.
     * We compute the lateral steering displacement of the tug, apply
     * a sin() function to reduce it based on how far the nosewheel is
     * deflected (obviously we don't want any deflection at near 90
     * degrees) and scale the similar triangles. The result is an
     * absolute lateral displacement that the nosewheel should
     * experience from the aircraft's point of view. We then translate
     * that into a heading change and write that to the orientation
     * quaternion, overriding the aircraft's heading.
     */
    nlg_tug_rear_off = tug_rear2acf_nw();
    turn_inc = rel_tug_turn_rate * bp.d_t;

    /*
     * We compute the lateral & longitudinal displacement in the
     * tug's coordinates. We then rotate this vector to the aircraft's
     * vector and apply the x component to the aircraft's heading.
     */
    off_v.x = sin(DEG2RAD(turn_inc)) * (nlg_tug_rear_off /
                                        bp_ls.tug->veh.wheelbase);
    off_v.y = (cos(DEG2RAD(turn_inc)) - 1) * (nlg_tug_rear_off /
                                              bp_ls.tug->veh.wheelbase);
    off_v = vect2_rot(off_v, cur_nw_steer);
    d_hdg = RAD2DEG(asin(off_v.x / bp.veh.wheelbase));
    /*
     * For some inexplicable reason, we have to amplify the heading change
     * by around 10x to get it to show properly in the sim. Probably
     * something to do with ground stickiness or heading change
     * granularity/float rounding errors. Definitely file under "WTF".
     */
    reorient_aircraft(0, 0, 10 * d_hdg);
}

static double
tug_speed(void) {
    vect2_t v = VECT2(DEG2RAD(bp.d_pos.hdg / bp.d_t) * bp.veh.wheelbase,
                      bp.cur_pos.spd);
    vect2_t u = hdg2dir(dr_getf(&drs.tire_steer_cmd));
    return (vect2_dotprod(u, v));
}

static void
push_at_speed(double targ_speed, double max_accel, bool_t allow_snd_ctl,
              bool_t decelerating) {
    double force_lim, force_incr, force, accel_now, d_v, Fx, Fz, steer;
    double cur_spd, nose_down_moment;

    VERIFY3S(dr_getvf(&drs.tire_steer_cmd, &steer, bp.acf.nw_i, 1), ==, 1);

    /*
     * Limit our speed hard when on slippery surfaces.
     */
    switch (dr_geti(&drs.rwy_friction)) {
        case RWY_FRICTION_MED:
            targ_speed = MIN(MAX(targ_speed, -MAX_SPEED_MED_FRICTION),
                             MAX_SPEED_MED_FRICTION);
            break;
        case RWY_FRICTION_POOR:
            targ_speed = MIN(MAX(targ_speed, -MAX_SPEED_POOR_FRICTION),
                             MAX_SPEED_POOR_FRICTION);
            break;
    }

    /*
     * Multiply force limit by weight in tons - that's at most how
     * hard we'll try to push the aircraft. This prevents us from
     * flinging the aircraft across the tarmac in case some external
     * factor is blocking us (like chocks).
     */
    force_lim = FORCE_PER_TON * (dr_getf(&drs.acf_mass) / 1000);

    /*
     * Scale the maximum force increment by frame time. This means it'll
     * take up to 1s for us to apply full pushback force.
     */
    force_incr = force_lim * bp.d_t;

    /*
     * We actually control ground speed to be the speed of the tug rather
     * than the longitudinal speed of the aircraft. So scale the
     * longitudinal speed based on nosewheel steering angle.
     */
    if (bp_xp_ver >= 11000) {
        cur_spd = tug_speed();
        accel_now = (bp.d_pos.spd / cos(DEG2RAD(fabs(steer)))) / bp.d_t;
    } else {
        /*
         * XP10's buggy sticky tire model prevents us from reducing
         * longitudinal speed below MIN_SPEED_XP10, so make sure we
         * keep the speed up above that value.
         */
        cur_spd = bp.cur_pos.spd;
        accel_now = bp.d_pos.spd / bp.d_t;
    }

    force = bp.last_force;
    d_v = targ_speed - cur_spd;

    /*
     * This is some fudge needed to get some high-thrust aircraft
     * going, otherwise we'll just jitter in-place due to thinking
     * we're overdoing acceleration.
     */
    if (ABS(cur_spd) < BREAKAWAY_THRESH)
        max_accel *= 100;

    if (d_v > 0) {        /* speed up */
        /*
         * Modulate the acceleration to reach our target speed smoothly,
         * unless we're trying to decelerate or we've not yet broken
         * away (to prevent jumpiness on XP10's sticky tires).
         */
        if (d_v < max_accel && !decelerating &&
            ABS(bp.cur_pos.spd) >= BREAKAWAY_THRESH)
            max_accel = d_v;
        if (accel_now > max_accel)
            force -= force_incr;
        else if (accel_now < max_accel)
            force += force_incr;
    } else if (d_v < 0) {    /* slow down */
        max_accel *= -1;
        if (d_v > max_accel && !decelerating &&
            ABS(bp.cur_pos.spd) >= BREAKAWAY_THRESH)
            max_accel = d_v;
        if (accel_now < max_accel)
            force += force_incr;
        else if (accel_now > max_accel)
            force -= force_incr;
    }

    /*
     * Calculate the vector components of our force on the aircraft
     * to correctly apply angular momentum forces below.
     * N.B. we only push in the horizontal plane, hence no Fy component.
     */
    Fx = force * sin(DEG2RAD(steer));
    Fz = force * cos(DEG2RAD(steer));

    dr_setf(&drs.axial_force, dr_getf(&drs.axial_force) - Fz);
    dr_setf(&drs.rot_force_N, dr_getf(&drs.rot_force_N) +
                              Fx * (-bp.acf.nw_z));

    /*
     * The nose-down force moment is composed of two parts:
     * 1) Us pushing or pulling on the aircraft. This will tend
     *	apply a nose-down moment when pushing (because we're
     *	pushing below the CG), and a nose-up moment when towing.
     * 2) As a safety, if for whatever reason the aircraft's nose gear
     *	wants to lift off the ground, we will simulate that it's
     *	trying to lift our tug up. So as soon as ground contact is
     *	lost on that wheel, we start incrementing the
     *	tug_weight_force until the nosewheel is again in contact
     *	with the ground (at which point we will reset it to 0 again).
     *	This should prevent any possibility of the aircraft's nose
     *	lifting off the ground in case of a sudden application of
     *	brakes, or some landing gear friction bug. In that case we
     *	also start reducing the force pushing or pulling on the NLG.
     */
    nose_down_moment = dr_getf(&drs.rot_force_M) + Fz * bp.acf.nw_len;
    if (bp_xp_ver >= 11000) {
        int on_gnd;
        VERIFY3S(dr_getvi(&drs.gear_on_ground, &on_gnd, bp.acf.nw_i,
                          1), ==, 1);
        if (on_gnd != 1) {
            bp.tug_weight_force +=
                    MASS2GFORCE(bp_ls.tug->info->mass) * bp.d_t;
            bp.tug_weight_force = MIN(bp.tug_weight_force,
                                      MASS2GFORCE(bp_ls.tug->info->mass));
            nose_down_moment += bp.tug_weight_force * bp.acf.nw_z;
            /*
             * Start neutralizing push force to get rid of
             * the problem.
             */
            if (force < 0)
                force += 2 * force_incr;
            else
                force -= 2 * force_incr;
        } else {
            bp.tug_weight_force = 0;
        }
    }
    dr_setf(&drs.rot_force_M, nose_down_moment);

    /* Don't overstep the force limits for this aircraft */
    force = MIN(force_lim, force);
    force = MAX(-force_lim, force);

    bp.last_force = force;

    if (allow_snd_ctl) {
        tug_set_TE_override(bp_ls.tug, B_TRUE);
        if ((bp.cur_pos.spd > 0 && force > 0) ||
            (bp.cur_pos.spd < 0 && force < 0)) {
            double spd_fract = (ABS(bp.cur_pos.spd) /
                                bp_ls.tug->info->max_fwd_speed);
            double force_fract = fabs(force /
                                      bp_ls.tug->info->max_TE);
            tug_set_TE_snd(bp_ls.tug, AVG(force_fract, spd_fract),
                           bp.d_t);
        } else {
            tug_set_TE_snd(bp_ls.tug, 0, bp.d_t);
        }
    }
}

static bool_t
read_gear_info(void) {
    double tire_z[10];
    int gear_steers[10], gear_types[10], gear_on_ground[10];
    int gear_deploy[10];

    dr_getvi(&drs.gear_deploy, gear_deploy, 0, 10);
    if (bp_xp_ver >= 11000)
        dr_getvi(&drs.gear_on_ground, gear_on_ground, 0, 10);
    else
        memset(gear_on_ground, 0xff, sizeof(gear_on_ground));

    /* First determine where the gears are */
    for (int i = 0, n = dr_getvi(&drs.gear_types, gear_types, 0, 10);
         i < n; i++) {
        /*
         * Gear types are:
         * 0) Nothing.
         * 1) Skid.
         * 2+) Wheel based gear in various arrangements. A tug can
         *	provide a filter for this.
         *
         * Also make sure to ONLY select gears which are deployed and
         * are touching the ground. Some aircraft models have weird
         * gears which are, for whatever reason, hovering in mid air
         * (huh?).
         */
        if (gear_types[i] >= 2 && gear_on_ground[i] != 0 &&
            gear_deploy[i] != 0)
            bp.acf.gear_is[bp.acf.n_gear++] = i;
    }

    /* Read nosegear long axis deflections */
    VERIFY3S(dr_getvf(&drs.tire_z, tire_z, 0, 10), >=, bp.acf.n_gear);
    bp.acf.nw_i = -1;
    bp.acf.nw_z = 1e10;

    /* Next determine which gear steers. Pick the one most forward. */
    VERIFY3S(dr_getvi(&drs.gear_steers, gear_steers, 0, 10), >=,
             bp.acf.n_gear);
    for (int i = 0; i < bp.acf.n_gear; i++) {
        if (gear_steers[bp.acf.gear_is[i]] == 1 &&
            tire_z[bp.acf.gear_is[i]] < bp.acf.nw_z) {
            bp.acf.nw_i = bp.acf.gear_is[i];
            bp.acf.nw_z = tire_z[bp.acf.gear_is[i]];
        }
    }

    /*
     * Aircraft appears to not have any steerable gears.
     * Hope same fix as on the tu154 helps here...
     */
    if (bp.acf.nw_i == -1) {
        bp.acf.nw_i = bp.acf.gear_is[0];
        bp.acf.nw_z = tire_z[bp.acf.gear_is[0]];
    }

    /* Nose gear strut length and tire radius */
    VERIFY3S(dr_getvf(&drs.leg_len, &bp.acf.nw_len, bp.acf.nw_i, 1), ==, 1);
    VERIFY3S(dr_getvf(&drs.tirrad, &bp.acf.tirrad, bp.acf.nw_i, 1), ==, 1);

    /* Read nosewheel type */
    bp.acf.nw_type = gear_types[bp.acf.nw_i];

    /* Compute main gear Z deflection as mean of all main gears */
    for (int i = 0; i < bp.acf.n_gear; i++) {
        if (bp.acf.gear_is[i] != bp.acf.nw_i)
            bp.acf.main_z += tire_z[bp.acf.gear_is[i]];
    }
    bp.acf.main_z /= bp.acf.n_gear - 1;

    return (B_TRUE);
}

static bool_t
bp_state_init(void) {
    memset(&bp, 0, sizeof(bp));
    list_create(&bp.segs, sizeof(seg_t), offsetof(seg_t, node));

    if (bp_xp_ver < MIN_XPLANE_VERSION) {
        char msg[256];
        snprintf(msg, sizeof(msg), _("Pushback failure: X-Plane "
                                     "version too old. This plugin requires at least X-Plane "
                                     "%s to operate."), MIN_XPLANE_VERSION_STR);
        XPLMSpeakString(msg);
        logMsg(BP_FATAL_LOG "x-plane version %d to old. Minimal version supported is X-Plane %s", bp_xp_ver,
               MIN_XPLANE_VERSION_STR);
        return (B_FALSE);
    }

    if (!read_acf_file_info()) {
        XPLMSpeakString(_("Pushback failure: error reading aircraft "
                          "files from disk."));
        logMsg(BP_ERROR_LOG "Error reading aircraft files from disk.");
        return (B_FALSE);
    }
    if (bp.acf.model_flags.is_helicopter ||
        bp.acf.model_flags.fly_like_a_helo) {
        //XPLMSpeakString(_("Pushback failure: Are you seriously "
        //                  "trying to call pushback for a helicopter?"));
        // no need to speak up here
        logMsg("User is starting flight with an helicopter: BpB idle for now");
        return (B_FALSE);
    }

    if (!read_gear_info()) {
        logMsg(BP_WARN_LOG "Not able to read gear information");
        return (B_FALSE);
    }

    bp.veh.wheelbase = bp.acf.main_z - bp.acf.nw_z;
    bp.veh.fixed_z_off = -bp.acf.main_z;    /* X-Plane's Z is negative */
    if (bp.veh.wheelbase <= 0) {
        //XPLMSpeakString(_("Pushback failure: aircraft has non-positive "
        //                  "wheelbase. Sorry, tail daggers aren't supported."));
        // No need to speak up here
        logMsg("aircraft has still non-positive wheelbase. (wheelbase = %f): BpB idle for now", bp.veh.wheelbase);
        return (B_FALSE);
    }

    bp.veh.max_steer = MIN(MAX(dr_getf(&drs.nw_steerdeg1), dr_getf(&drs.nw_steerdeg2)), max_steer_angle());
    /*
     * Some aircraft have a broken declaration here and only declare the
     * high-speed rudder steering angle. For those, ignore what they say
     * and use our max_steer_angle().
     */
    if (bp.veh.max_steer < MIN_STEER_ANGLE)
        bp.veh.max_steer = (max_steer_angle() + MIN_STEER_ANGLE) / 2;
    bp.veh.max_fwd_spd = MAX_FWD_SPEED;
    bp.veh.max_rev_spd = MAX_REV_SPEED;
    bp.veh.max_fwd_ang_vel = MAX_FWD_ANG_VEL;
    bp.veh.max_rev_ang_vel = MAX_REV_ANG_VEL;
    bp.veh.max_centr_accel = MAX_CENTR_ACCEL;
    bp.veh.max_accel = NORMAL_ACCEL;
    bp.veh.max_decel = NORMAL_DECEL;
    /*
     * To achieve more accurate pushback results, we use our rear axle
     * position to actually direct the pushback, not our aircraft's
     * origin point.
     */
    bp.veh.use_rear_pos = B_TRUE;

    bp.step = PB_STEP_OFF;
    bp.step_start_t = 0;

    return (B_TRUE);
}

bool_t
audio_sys_init(void) {
    lang_pref_t lang_pref = LANG_PREF_MATCH_REAL;
    char icao[8];
    logMsg(BP_INFO_LOG "Initialising audio");
    find_nearest_airport(icao);
    (void) conf_get_i(bp_conf, "lang_pref", (int *) &lang_pref);
    if (!msg_init(bp_get_lang(), icao, lang_pref)) {
        XPLMSpeakString(_("Pushback failure: error initialising audio "
                          "messages. Please reinstall BetterPushback."));
        logMsg(BP_FATAL_LOG "Error initialising audio");
        return (B_FALSE);
    }

    return (B_TRUE);
}

static bool_t
acf_on_gnd_stopped(const char **reason) {
    if (dr_geti(&drs.onground_any) != 1) {
        if (reason != NULL) {
            *reason = _("Pushback failure: aircraft not on ground.");
            logMsg(BP_WARN_LOG "Aircraft not on the ground.");
        }
        return (B_FALSE);
    }
    if (vect3_abs(VECT3(dr_getf(&drs.local_vx), dr_getf(&drs.local_vy),
                        dr_getf(&drs.local_vz))) >= 1) {
        if (reason != NULL) {
            *reason = _("Pushback failure: aircraft not stationary.");
            logMsg(BP_WARN_LOG "Aircraft not stationary.");
        }
        return (B_FALSE);
    }
    if (dr_getf(&drs.gear_deploy) != 1) {
        if (reason != NULL) {
            *reason = _("Pushback failure: gear not extended.");
            logMsg(BP_WARN_LOG "Gear not extended.");
        }
        return (B_FALSE);
    }
    return (B_TRUE);
}

/*
 * Normally, we delay calling bp_init and bp_fini until the plugin is actually
 * needed. This can mess with 3rd party plugin integration which might need to
 * look for things such as commands we create much earlier. To solve this, we
 * have bp_boot_init and bp_shut_fini, which are called from XPluginStart and
 * XPluginStop.
 */
void
bp_boot_init(void) {
    disco_cmd = XPLMCreateCommand("BetterPushback/disconnect",
                                  _("Disconnect tow + headset and switch to hand signals."));
    recon_cmd = XPLMCreateCommand("BetterPushback/reconnect",
                                  _("Reconnect tow and await further instructions."));

    DCR_CREATE_F(NULL, &bp.anim.nosewheel_rot_spd, false, "bp/anim/nosewheel_rotation_speed_rad_sec");
}

void
bp_shut_fini(void) {
}

/*
 * Reads the aircraft's .acf file and grabs the info we want from it.
 */
static bool_t
read_acf_file_info(void) {
    char my_acf[512], my_path[512];
    FILE *fp;
    char *line = NULL;
    size_t cap = 0;
    bool_t parsing_props = B_FALSE;

    XPLMGetNthAircraftModel(0, my_acf, my_path);
    fp = fopen(my_path, "rb");
    if (fp == NULL) {
        logMsg(BP_ERROR_LOG "error reading %s: %s", my_acf, strerror(errno));
        return (B_FALSE);
    }

#define    PARSE_FLAG_PARAM(flag) \
    do { \
        size_t n; \
        char **comps = strsplit(line, " ", B_TRUE, &n); \
        if (n != 3) { \
            free_strlist(comps, n); \
            continue; \
        } \
        sscanf(comps[2], "%d", &bp.acf.model_flags.flag); \
        free_strlist(comps, n); \
    } while (0)

    while (getline(&line, &cap, fp) > 0) {
        strip_space(line);
        if (!parsing_props) {
            if (strcmp(line, "PROPERTIES_BEGIN") == 0)
                parsing_props = B_TRUE;
            continue;
        }
        if (strcmp(line, "PROPERTIES_END") == 0)
            break;
        if (strstr(line, "acf/_is_airliner") != NULL)
            PARSE_FLAG_PARAM(is_airliner);
        else if (strstr(line, "acf/_is_experimental") != NULL)
            PARSE_FLAG_PARAM(is_experimental);
        else if (strstr(line, "acf/_is_general_aviation") != NULL)
            PARSE_FLAG_PARAM(is_general_aviation);
        else if (strstr(line, "acf/_is_glider") != NULL)
            PARSE_FLAG_PARAM(is_glider);
        else if (strstr(line, "acf/_is_helicopter") != NULL)
            PARSE_FLAG_PARAM(is_helicopter);
        else if (strstr(line, "acf/_is_military") != NULL)
            PARSE_FLAG_PARAM(is_military);
        else if (strstr(line, "acf/_is_sci_fi") != NULL)
            PARSE_FLAG_PARAM(is_sci_fi);
        else if (strstr(line, "acf/_is_seaplane") != NULL)
            PARSE_FLAG_PARAM(is_seaplane);
        else if (strstr(line, "acf/_is_ultralight") != NULL)
            PARSE_FLAG_PARAM(is_ultralight);
        else if (strstr(line, "acf/_is_vtol") != NULL)
            PARSE_FLAG_PARAM(is_vtol);
        else if (strstr(line, "acf/_fly_like_a_helo") != NULL)
            PARSE_FLAG_PARAM(fly_like_a_helo);
    }

#undef    PARSE_FLAG_PARAM

    fclose(fp);

    return (B_TRUE);
}

bool_t
bp_init(void) {
    const char *reason;
    dr_t radio_vol, sound_on;
    char my_acf[512], my_path[512];
    char *acf_override_file;

    /*
     * Due to numerous spurious bug reports of missing ground crew audio,
     * check that the user hasn't turned down the radio volume and just
     * forgotten about it. Warn the user if the volume is very low.
     */
    fdr_find(&sound_on, "sim/operation/sound/sound_on");
    fdr_find(&radio_vol, "sim/operation/sound/radio_volume_ratio");
    if (dr_getf(&radio_vol) < MIN_RADIO_VOLUME_THRESH &&
        dr_geti(&sound_on) == 1 && !radio_volume_warn) {
        XPLMSpeakString(_("Pushback advisory: you have your radio "
                          "volume turned very low and may not be able to hear "
                          "ground crew. Please increase your radio volume in "
                          "the X-Plane sound preferences."));
        radio_volume_warn = B_TRUE;
    }

    if (inited)
        return (B_TRUE);

    memset(&drs, 0, sizeof(drs));

    fdr_find(&drs.lbrake, "sim/cockpit2/controls/left_brake_ratio");
    fdr_find(&drs.rbrake, "sim/cockpit2/controls/right_brake_ratio");
    if (/* FlightFactor A320 */
            !dr_find(&drs.pbrake, "model/controls/park_break") &&
            /* Felis Tu-154M */
            !dr_find(&drs.pbrake, "sim/custom/controll/parking_brake")) {
        fdr_find(&drs.pbrake, "sim/flightmodel/controls/parkbrake");
        drs.pbrake_is_custom = B_FALSE;
    } else {
        drs.pbrake_is_custom = B_TRUE;
    }
    if (bp_xp_ver >= 12200) {
        fdr_find(&drs.pbrake_rat, "sim/cockpit2/controls/wheel_brake_ratio");
    } else {
        fdr_find(&drs.pbrake_rat, "sim/cockpit2/controls/parking_brake_ratio");
    }
    fdr_find(&drs.rot_force_M, "sim/flightmodel/forces/M_plug_acf");
    fdr_find(&drs.rot_force_N, "sim/flightmodel/forces/N_plug_acf");
    fdr_find(&drs.axial_force, "sim/flightmodel/forces/faxil_plug_acf");
    fdr_find(&drs.override_planepath,
             "sim/operation/override/override_planepath");
    fdr_find(&drs.local_x, "sim/flightmodel/position/local_x");
    fdr_find(&drs.local_y, "sim/flightmodel/position/local_y");
    fdr_find(&drs.local_z, "sim/flightmodel/position/local_z");
    fdr_find(&drs.lat, "sim/flightmodel/position/latitude");
    fdr_find(&drs.lon, "sim/flightmodel/position/longitude");
    fdr_find(&drs.roll, "sim/flightmodel/position/phi");
    fdr_find(&drs.pitch, "sim/flightmodel/position/theta");
    fdr_find(&drs.hdg, "sim/flightmodel/position/psi");
    fdr_find(&drs.quaternion, "sim/flightmodel/position/q");
    fdr_find(&drs.local_vx, "sim/flightmodel/position/local_vx");
    fdr_find(&drs.local_vy, "sim/flightmodel/position/local_vy");
    fdr_find(&drs.local_vz, "sim/flightmodel/position/local_vz");
    fdr_find(&drs.sim_time, "sim/time/total_running_time_sec");
    fdr_find(&drs.acf_mass, "sim/flightmodel/weight/m_total");
    fdr_find(&drs.tire_z, "sim/flightmodel/parts/tire_z_no_deflection");
    fdr_find(&drs.tire_x, "sim/flightmodel/parts/tire_x_no_deflection");
    fdr_find(&drs.tire_rot_spd,
             "sim/flightmodel2/gear/tire_rotation_speed_rad_sec");
    fdr_find(&drs.mtow, "sim/aircraft/weight/acf_m_max");
    fdr_find(&drs.leg_len, "sim/aircraft/parts/acf_gear_leglen");
    if (bp_xp_ver >= 12100) {
        fdr_find(&drs.tirrad, "sim/flightmodel2/gear/tire_radius_mtrs");
    } else {
        fdr_find(&drs.tirrad, "sim/aircraft/parts/acf_gear_tirrad");
    }
    fdr_find(&drs.nw_steerdeg1, "sim/aircraft/gear/acf_nw_steerdeg1");
    fdr_find(&drs.nw_steerdeg2, "sim/aircraft/gear/acf_nw_steerdeg2");
    fdr_find(&drs.tire_steer_cmd,
             "sim/flightmodel/parts/tire_steer_cmd");
    fdr_find(&drs.override_steer,
             "sim/operation/override/override_wheel_steer");
    fdr_find(&drs.nw_steer_on, "sim/cockpit2/controls/nosewheel_steer_on");
    fdr_find(&drs.gear_types, "sim/aircraft/parts/acf_gear_type");
    if (bp_xp_ver >= 11000) {
        fdr_find(&drs.gear_on_ground,
                 "sim/flightmodel2/gear/on_ground");
    }
    fdr_find(&drs.onground_any, "sim/flightmodel/failures/onground_any");
    fdr_find(&drs.gear_steers, "sim/aircraft/overflow/acf_gear_steers");
    fdr_find(&drs.gear_deploy, "sim/aircraft/parts/acf_gear_deploy");
    fdr_find(&drs.num_engns, "sim/aircraft/engine/acf_num_engines");
    fdr_find(&drs.engn_running, "sim/flightmodel/engine/ENGN_running");
    fdr_find(&drs.acf_livery_path, "sim/aircraft/view/acf_livery_path");

    // runway_friction has changed on XP12
    if (bp_xp_ver >= 12000) {
        fdr_find(&drs.rwy_friction, "sim/weather/region/runway_friction");
    } else {
        fdr_find(&drs.rwy_friction, "sim/weather/runway_friction");
    }

    fdr_find(&drs.landing_lights_on,
             "sim/cockpit/electrical/landing_lights_on");
    fdr_find(&drs.taxi_light_on, "sim/cockpit/electrical/taxi_light_on");

    fdr_find(&drs.author, "sim/aircraft/view/acf_author");
    fdr_find(&drs.sim_paused, "sim/time/paused");

    fdr_find(&drs.beacon_light, "sim/cockpit2/switches/beacon_on");

    fdr_find(&drs.joystick, "sim/joystick/joy_mapped_axis_value");

    XPLMRegisterCommandHandler(disco_cmd, disco_handler, 1, NULL);
    XPLMRegisterCommandHandler(recon_cmd, recon_handler, 1, NULL);

    /*
     * We do this check before attempting to read gear info, because
     * in-flight the gear info check will fail with "non-steerable"
     * gears, which is a little cryptic to understand to the user.
     */
    if (!acf_on_gnd_stopped(&reason))
        goto errout;

    if (!bp_state_init())
        goto errout;
    if (!audio_sys_init() || !load_buttons() ||
        !load_icon(&disco_buttons[0]) || !load_icon(&disco_buttons[1]))
        goto errout;

    XPLMGetNthAircraftModel(0, my_acf, my_path);

    cfg_disco_when_done = B_FALSE;
    conf_get_b_per_acf("disco_when_done", &cfg_disco_when_done);


    cfg_ignore_park_break = B_FALSE;
    conf_get_b_per_acf("ignore_park_brake", &cfg_ignore_park_break);



    previous_beacon = dr_geti(&drs.beacon_light);

    doors_refs_init();
    
    acf_override_file  = mkpathname(bp_xpdir, bp_plugindir, "objects", "override", my_acf, NULL);
    if (file_exists(acf_override_file, NULL)) {
        logMsg(BP_INFO_LOG "acf override file found in %s : using it  ", acf_override_file);
        bp_ls.outline = acf_outline_read(acf_override_file);
    } else {
        bp_ls.outline = acf_outline_read(my_path);
    }
    free(acf_override_file);

    if (bp_ls.outline == NULL)
        goto errout;

    inited = B_TRUE;

    return (B_TRUE);
    errout:
    XPLMUnregisterCommandHandler(disco_cmd, disco_handler, 1, NULL);
    XPLMUnregisterCommandHandler(recon_cmd, recon_handler, 1, NULL);
    msg_fini();
    unload_buttons();
    unload_icon(&disco_buttons[0]);
    unload_icon(&disco_buttons[1]);
    if (bp_ls.outline != NULL) {
        acf_outline_free(bp_ls.outline);
        bp_ls.outline = NULL;
    }
    return (B_FALSE);
}

static void
draw_tugs(void) {
    if (bp_ls.tug == NULL) {
        /*
         * If we have no tug loaded, we must either be in the
         * tug-selection phase, or be slaved to a master instance
         * which has not yet notified us which tug to use.
         */
        ASSERT(bp.step <= PB_STEP_TUG_LOAD || slave_mode);
        return;
    }

    if (list_head(&bp_ls.tug->segs) == NULL &&
        bp.step >= PB_STEP_GRABBING &&
        bp.step <= PB_STEP_UNGRABBING) {
        vect2_t my_pos = VECT2(dr_getf(&drs.local_x),
                               -dr_getf(&drs.local_z));
        double my_hdg = dr_getf(&drs.hdg);
        tug_pos_update(my_pos, my_hdg, B_TRUE);
    }

    tug_draw(bp_ls.tug, bp.cur_t);
}

bool_t
bp_can_start(const char **reason) {
    seg_t *seg;
    if (!acf_is_compatible()) {
        if (reason != NULL)
            *reason = _("Pushback failure: aircraft is not "
                        "compatible with BetterPushback.");
        return (B_FALSE);
    }

    if (!acf_on_gnd_stopped(reason))
        return (B_FALSE);

    if (!eng_ok2start() && eng_is_running()) {
        if (reason != NULL) {
            *reason = _("Pushback failure: cannot push this "
                        "aircraft with engines running. Shutdown "
                        "engines first.");
        }
        return (B_FALSE);
    }


    if (!push_manual.active) {
        seg = list_head(&bp.segs);
        if (seg == NULL && !late_plan_requested && !slave_mode) {
            if (reason != NULL) {
                *reason = _("Pushback failure: please first plan your "
                            "pushback to tell me where you want to go.");
            }
            return (B_FALSE);
        }
    } else {
        logMsg("Manual push: Just started, not checking the pre-plan");
    }

    return (B_TRUE);
}

void
bp_delete_all_segs(void) {
    seg_t *seg;
    while ((seg = list_remove_head(&bp.segs)) != NULL)
        free(seg);
}

bool_t
bp_start(void) {
    const char *reason;
    XPLMCreateFlightLoop_t floop = {
            .structSize = sizeof(XPLMCreateFlightLoop_t),
            .phase = xplm_FlightLoop_Phase_BeforeFlightModel,
            .callbackFunc = bp_run,
            .refcon = NULL
    };

    if (bp_started)
        return (B_TRUE);
    if (!bp_can_start(&reason)) {
        XPLMSpeakString(reason);
        return (B_FALSE);
    }

    bp_gather();
    bp.last_pos = bp.cur_pos;
    bp.last_t = bp.cur_t;

    bp.step = 1;
    bp.step_start_t = bp.cur_t;

    /*
     * Memorize where we were at the start. We will use this to determine
     * which way to turn when disconnecting and where to attempt to go
     * once we're done.
     */
    bp.start_pos = bp.cur_pos.pos;
    bp.start_hdg = bp.cur_pos.hdg;

    if (bp_floop == NULL)
        bp_floop = XPLMCreateFlightLoop(&floop);
    XPLMScheduleFlightLoop(bp_floop, -1, 1);

    if (!slave_mode && !late_plan_requested && !push_manual.active)
        route_save(&bp.segs);

    bp_started = B_TRUE;
    bp_conf_set_save_enabled(!bp_started);

    /*
     * Some aircraft (like the MD-80) do not have a taxi light switch,
     * so if the previously loaded aircraft had taxi lights on, the
     * dataref could left set to '1' with the pilot having no way of
     * switching the lights off. So we manually make sure the lights
     * are off here. This way we can be sure that if we see the light
     * on during pushback, it was the pilot who turned it on.
     */
    dr_seti(&drs.landing_lights_on, 0);
    dr_seti(&drs.taxi_light_on, 0);

    return (B_TRUE);
}

bool_t
bp_stop(void) {
    seg_t *seg;

    if (!bp_started)
        return (B_FALSE);

    /* prevent trying to reach segment end hdg and apply correct back */
    bp.last_hdg = NAN;
    if ((seg = list_tail(&bp.segs)) != NULL)
        bp.last_seg_is_back = seg->backward;
    bp_delete_all_segs();
    late_plan_requested = B_FALSE;
    tug_pending_mode = B_FALSE;

    return (B_TRUE);
}

void
bp_fini(void) {
    if (!inited)
        return;

    if (bp_ls.outline != NULL) {
        acf_outline_free(bp_ls.outline);
        bp_ls.outline = NULL;
    }

    if (bp_floop != NULL) {
        XPLMDestroyFlightLoop(bp_floop);
        bp_floop = NULL;
    }

    XPLMUnregisterCommandHandler(disco_cmd, disco_handler, 1, NULL);
    XPLMUnregisterCommandHandler(recon_cmd, recon_handler, 1, NULL);

    msg_fini();
    bp_complete();

    /* segs have been released in bp_complete */
    list_destroy(&bp.segs);

    unload_icon(&disco_buttons[0]);
    unload_icon(&disco_buttons[1]);
    unload_buttons();

    radio_volume_warn = B_FALSE;

    inited = B_FALSE;
}

static bool_t
nearing_end(void) {
    double long_displ;
    seg_t *seg = list_head(&bp.segs);
    vect2_t end_dir, end2acf;

    if (seg->type != SEG_TYPE_STRAIGHT || seg != list_tail(&bp.segs))
        return (B_FALSE);

    end_dir = hdg2dir(seg->end_hdg);
    if (seg->backward)
        end_dir = vect2_neg(end_dir);
    end2acf = vect2_sub(bp.cur_pos.pos, seg->end_pos);
    long_displ = vect2_dotprod(end_dir, end2acf);
    return (long_displ > -NEARING_END_THRESHOLD);
}

/*
 * We need to compute a fake position for drive_segs. This is because when
 * steering, we don't actually perform simple steering around our nosewheel.
 * Instead, the nosewheel swings by being articulated with the tug service
 * as the platform. So instead of simply passing our true position to
 * drive_segs, we pretend that our centerline actually passes through the
 * tug's fixed steering axle.
 */
static vehicle_pos_t
corr_acf_pos(void) {
    vect2_t dir = hdg2dir(bp.cur_pos.hdg);
    vect2_t main_pos = vect2_add(bp.cur_pos.pos,
                                 vect2_scmul(dir, -bp.acf.main_z));
    vect2_t nw_pos = vect2_add(bp.cur_pos.pos,
                               vect2_scmul(dir, -bp.acf.nw_z));
    double tug_rear2acf_nw_l = tug_rear2acf_nw();
    double steer, corr_hdg;
    vect2_t tug_rear_pos, corr_dir, corr_pos;

    VERIFY3S(dr_getvf(&drs.tire_steer_cmd, &steer, bp.acf.nw_i, 1), ==, 1);
    tug_rear_pos = vect2_add(nw_pos, vect2_scmul(hdg2dir(normalize_hdg(
            bp.cur_pos.hdg + steer + 180)), tug_rear2acf_nw_l));
    corr_dir = vect2_sub(tug_rear_pos, main_pos);
    corr_pos = vect2_add(main_pos, vect2_set_abs(corr_dir, bp.acf.main_z));
    corr_hdg = dir2hdg(corr_dir);

    return ((vehicle_pos_t) {corr_pos, corr_hdg, bp.cur_pos.spd});
}

static bool_t
bp_run_push(void) {

    if ( push_manual.active) {
        return bp_run_push_manual();
    }

    seg_t *seg = list_head(&bp.segs);
    /*
     * We memorize the direction of this segment in case we flip segments
     * and the next one goes in the opposite direction.
     */
    bool_t last_backward = (seg != NULL ? seg->backward : B_FALSE);

    while (seg != NULL) {
        double steer, speed;
        bool_t decel;
        vehicle_pos_t corr_pos;

        /* Pilot pressed brake pedals or set parking brake, stop */
        if (dr_getf(&drs.lbrake) >= BRAKE_PEDAL_THRESH ||
            dr_getf(&drs.rbrake) >= BRAKE_PEDAL_THRESH ||
            pbrake_is_set()) {
            tug_set_TE_snd(bp_ls.tug, 0, bp.d_t);
            dr_setf(&drs.axial_force, 0);
            dr_setf(&drs.rot_force_N, 0);
            bp.last_force = 0;
            break;
        }
        /*
         * If we have reversed direction, wait a little to simulate
         * the driver changing gear and flipping around.
         */
        if (bp.reverse_t != 0.0) {
            if (bp.cur_t - bp.reverse_t < 2 * STATE_TRANS_DELAY) {
                push_at_speed(0, bp.veh.max_accel, B_TRUE,
                              B_FALSE);
                break;
            }
            bp.reverse_t = 0.0;
        }
        corr_pos = corr_acf_pos();
        if (drive_segs(&corr_pos, &bp.veh, &bp.segs,
                       &bp.last_mis_hdg, bp.d_t, &steer, &speed, &decel)) {
            double nw_defl;
            if (!nearing_end()) {
                turn_nosewheel(steer);
            } else {
                /*
                 * When nearing the end of the route, we want
                 * to start neutralizing steering early to not
                 * overshoot too far.
                 */
                turn_nosewheel(0);
            }
            /*
             * Since the drive_segs function returns a longitudinal
             * speed, but push_at_speed controls speed based on the
             * tug's angle, so we need to correct for that.
             */
            nw_defl = rel_hdg(bp.cur_pos.hdg, bp_ls.tug->pos.hdg);
            speed /= MAX(cos(DEG2RAD(nw_defl)), 0.1);
            push_at_speed(speed, bp.veh.max_accel, B_TRUE, decel);
            break;
        }
        seg = list_head(&bp.segs);
        if (seg != NULL && seg->backward != last_backward) {
            bp.reverse_t = bp.cur_t;
            last_backward = seg->backward;
        }
    }

    return (seg != NULL);
}

static bool_t
bp_run_push_manual(void) {
    double speed = 0;
    float angle = 0;

    /* Pilot pressed brake pedals or set parking brake or manual pause, stop */
    if (dr_getf(&drs.lbrake) >= BRAKE_PEDAL_THRESH ||
        dr_getf(&drs.rbrake) >= BRAKE_PEDAL_THRESH ||
        pbrake_is_set()) {
        tug_set_TE_snd(bp_ls.tug, 0, bp.d_t);
        dr_setf(&drs.axial_force, 0);
        dr_setf(&drs.rot_force_N, 0);
        bp.last_force = 0;
        return (push_manual.active);
    }
    /*
        * If we have reversed direction, wait a little to simulate
        * the driver changing gear and flipping around.
        */
    if (bp.reverse_t != 0.0) {
        if (bp.cur_t - bp.reverse_t < 2 * STATE_TRANS_DELAY) {
            push_at_speed(0, bp.veh.max_accel, B_TRUE,
                            B_FALSE);
            return (push_manual.active);                
        }
        bp.reverse_t = 0.0;
    }


    if (push_manual.with_yoke) { 
        dr_getvf32(&drs.joystick, &angle, 2, 1);
    } else {
        angle = push_manual.angle/100.0;
    }
    angle *= bp.veh.max_steer;


    if (push_manual.with_yoke) {
        float speed_;
        dr_getvf32(&drs.joystick, &speed_, 1, 1);
        // pushing the yoke forward as accelerator
        // dr is negative when pushin forward
        speed_ = -speed_ ;
        if (speed_ < 0) {
            speed_ = 0;
        }
        speed = bp.veh.max_fwd_spd * (double)speed_;
    } else {
        //without yoke, always at "full" speed
        speed = bp.veh.max_fwd_spd;
    }

    if (!push_manual.forward_direction){
        speed = -speed; 
    }


    // if in reverse (by default the max speed is the forward speed) , limiting also at the max reverse speed
    if ( speed < -bp.veh.max_rev_spd) {
        speed = -bp.veh.max_rev_spd;
    }
    // for high angle the forward speed is limit to the max rev speed value 
    if ( speed > bp.veh.max_rev_spd) {
        if ( fabs(angle)> MIN_STEER_ANGLE) {
        speed = bp.veh.max_rev_spd;
        }   
    }

    turn_nosewheel((double) angle);
   
    // reducing the speed using the angle of the tug or set to 0 if paused
    speed *= push_manual.pause ? 0 : MAX(cos(DEG2RAD(fabs(angle))), 0.1);
    push_at_speed(speed, bp.veh.max_accel, B_TRUE, false);

    return (push_manual.active);
}


void manual_bp_start() {
    push_manual.active = true;
    push_manual.requested = false;
    push_manual.pause = false;
    push_manual.forward_direction = false;
    push_manual.angle = 0;
    logMsg("Manual push:  Starting %s yoke support", push_manual.with_yoke ? "with" : "without");
}

void manual_bp_request(bool_t with_yoke) {
    push_manual.active = false;
    push_manual.requested = true;
    push_manual.with_yoke = with_yoke;
}

void manual_bp_stop(void) {
    push_manual.active = false;
    push_manual.requested = false;
}


/*
 * Tears down a pushback session. This resets all state variables, unloads the
 * tug model and prepares us for another start.
 */
static void
bp_complete(void) {
    /*
     * Needs to go before the bp_started check in case the planner has
     * placed segments, but user has not yet started pushback.
     */
    bp_delete_all_segs();

    if (!bp_started)
        return;

    bp_started = B_FALSE;
    bp_connected = B_FALSE;
    bp_conf_set_save_enabled(!bp_started);
    late_plan_requested = B_FALSE;
    plan_complete = B_FALSE;

    if (bp_ls.tug != NULL) {
        tug_free(bp_ls.tug);
        bp_ls.tug = NULL;
    }

    disco_intf_hide();

    if (!slave_mode) {
        dr_seti(&drs.override_steer, 0);
        brakes_set(B_FALSE);
        dr_setvf(&drs.leg_len, &bp.acf.nw_len, bp.acf.nw_i, 1);
    }

    bp_done_notify();
    /*
     * Reinitialize our state so we're starting with a clean slate
     * next time.
     */
    bp_state_init();
}

/*
 * Returns B_TRUE when the late plan phase can be exited. This occurs when:
 * 1) if the machine is a master, the user must have completed the plan
 *	AND exited the pushback camera.
 * 2) if the machine is a slave, the plan_completed flag is synced from the
 *	master machine.
 */
static bool_t
late_plan_end_cond(void) {
    return ((!slave_mode && list_head(&bp.segs) != NULL &&
             !bp_cam_is_running()) || (slave_mode && plan_complete));
}

static bool_t
pb_step_tug_load(void) {
    bool_t tug_starts_next_plane = B_FALSE;
    (void) conf_get_b(bp_conf,"tug_starts_next_plane", &tug_starts_next_plane);

    if (!slave_mode) {
        char icao[8] = {0};
        char airline[1024] = {0};

        (void) find_nearest_airport(icao);
        if (acf_is_airliner())
            read_acf_airline(airline);

        bp_ls.tug = tug_alloc_auto(dr_getf(&drs.mtow),
                                   dr_getf(&drs.leg_len), bp.acf.tirrad,
                                   bp.acf.nw_type, strcmp(icao, "") != 0 ? icao : NULL,
                                   airline);
        if (bp_ls.tug == NULL) {
            /* tug_alloc_auto already spoke the error */
            bp_complete();
            return (B_FALSE);
        }
        strlcpy(bp_tug_name, bp_ls.tug->info->tug_name,
                sizeof(bp_tug_name));
    } else {
        char tug_name[sizeof(bp_tug_name)];
        char *ext;
        char icao[8] = {0};
        char airline[1024] = {0};

        /* make sure the tug name is properly terminated */
        memcpy(tug_name, bp_tug_name, sizeof(tug_name));
        tug_name[sizeof(tug_name) - 1] = '\0';

        /* wait until the tug name has been synced */
        if (strcmp(tug_name, "") == 0)
            return (B_TRUE);

        /* security check - must not contain a dir separator */
        if (strchr(tug_name, '/') != NULL ||
            strchr(tug_name, '\\') != NULL)
            return (B_TRUE);

        /* sanity check - must end in '.tug' */
        ext = strrchr(tug_name, '.');
        if (ext == NULL || strcmp(&ext[1], "tug") != 0)
            return (B_TRUE);

        (void) find_nearest_airport(icao);

        if (acf_is_airliner())
            read_acf_airline(airline);
        bp_ls.tug = tug_alloc_man(tug_name, bp.acf.tirrad, icao,
                                  airline);
        if (bp_ls.tug == NULL) {
            char msg[256];
            snprintf(msg, sizeof(msg), _("ERROR: "
                                         "master requested tug \"%s\", which we don't have "
                                         "in our in our library. Please sync your tug "
                                         "libraries before trying again."), tug_name);
            logMsg(BP_ERROR_LOG "%s", msg);
            XPLMSpeakString(msg);
            bp_complete();
            return (B_FALSE);
        }
    }
    if (!bp_ls.tug->info->drive_debug) {
        vect2_t p_start, dir;
        dir = hdg2dir(bp.cur_pos.hdg);
        if (tug_starts_next_plane) {
            p_start = vect2_add(bp.cur_pos.pos, vect2_scmul(dir,
                                                            -bp.acf.nw_z + TUG_APPCH_SHORT_DIST));
            tug_set_pos(bp_ls.tug, p_start, normalize_hdg(bp.cur_pos.hdg), 0);
        } else {
            p_start = vect2_add(bp.cur_pos.pos, vect2_scmul(dir,
                                                            -bp.acf.nw_z + TUG_APPCH_LONG_DIST));
            p_start = vect2_add(p_start, vect2_scmul(vect2_norm(dir,
                                                                B_TRUE), 10 * bp_ls.tug->veh.wheelbase));
            tug_set_pos(bp_ls.tug, p_start, normalize_hdg(bp.cur_pos.hdg -
                                                    90), bp_ls.tug->veh.max_fwd_spd);
        }                                               
    } else {
        tug_set_pos(bp_ls.tug, bp.cur_pos.pos, bp.cur_pos.hdg, 0);
    }
    bp.step++;
    bp.step_start_t = bp.cur_t;

    return (B_TRUE);
}

static void
pb_step_start(void) {
    if (!bp_ls.tug->info->drive_debug) {
        vect2_t left_off, p_end, dir;
        bool_t tug_starts_next_plane = B_FALSE;
        (void) conf_get_b(bp_conf,"tug_starts_next_plane", &tug_starts_next_plane);

        dir = hdg2dir(bp.cur_pos.hdg);

        if (tug_starts_next_plane) {
            left_off = vect2_add(bp.cur_pos.pos, vect2_scmul(dir,
                                                            -bp.acf.nw_z + TUG_APPCH_SHORT_DIST));
            tug_set_pos(bp_ls.tug, left_off, normalize_hdg(bp.cur_pos.hdg), 0.1 * bp_ls.tug->veh.max_fwd_spd);                                                
            p_end = vect2_add(bp.cur_pos.pos, vect2_scmul(dir,
                                                        (-bp.acf.nw_z) + bp_ls.tug->info->apch_dist));
             VERIFY(tug_drive2point(bp_ls.tug, p_end, bp.cur_pos.hdg));
        } else {
            left_off = vect2_add(bp.cur_pos.pos, vect2_scmul(dir,
                                                            -bp.acf.nw_z + TUG_APPCH_LONG_DIST));
            left_off = vect2_add(left_off, vect2_scmul(
                    vect2_norm(dir, B_FALSE), 2 * bp_ls.tug->veh.wheelbase));
            p_end = vect2_add(bp.cur_pos.pos, vect2_scmul(dir,
                                                        (-bp.acf.nw_z) + bp_ls.tug->info->apch_dist));

            VERIFY(tug_drive2point(bp_ls.tug, left_off,
                                normalize_hdg(bp.cur_pos.hdg - 90)));
            VERIFY(tug_drive2point(bp_ls.tug, p_end, bp.cur_pos.hdg));
        }
    } else {
        for (seg_t *seg = list_head(&bp.segs); seg != NULL;
             seg = list_next(&bp.segs, seg)) {
            seg_t *seg2 = safe_calloc(1, sizeof(*seg2));
            memcpy(seg2, seg, sizeof(*seg2));
            list_insert_tail(&bp_ls.tug->segs, seg2);
        }
    }

    msg_play(MSG_DRIVING_UP);
    bp.step++;
    bp.step_start_t = bp.cur_t;
    bp.last_voice_t = bp.cur_t;
}

static void
pb_step_driving_up_close(void) {
    if (!tug_is_stopped(bp_ls.tug)) {
        /*
         * Keep resetting the start time to enforce the state
         * transition delay once the tug stops.
         */
        bp.step_start_t = bp.cur_t;
    } else if (bp.cur_t - bp.step_start_t >= STATE_TRANS_DELAY) {
        tug_set_cradle_beeper_on(bp_ls.tug, B_TRUE);
        tug_set_cradle_lights_on(B_TRUE);
        tug_set_hazard_lights_on(B_TRUE);
        bp.step++;
        bp.step_start_t = bp.cur_t;
    }
}

static void
pb_step_waiting_for_pbrake(void) {
    vect2_t p_end, dir;
    dr_t zibo_chocks;

    if (!pbrake_is_set() ||
        /* wait until the rdy2conn message has stopped playing */
        bp.cur_t - bp.last_voice_t < msg_dur(MSG_RDY2CONN)) {
        /* keep resetting the start time to enforce a delay */
        bp.step_start_t = bp.cur_t;
        return;
    }
    /*
     * After the parking brake is set and the message has finished
     * playing, wait a short moment until starting to move again.
     */
    if (bp.cur_t - bp.step_start_t < STATE_TRANS_DELAY)
        return;

    /* Workaround for Zibo 737 chocks being set - remove them. */
    if (dr_find(&zibo_chocks, "laminar/B738/fms/chock_status") &&
        dr_geti(&zibo_chocks) != 0) {
        if (zibo_chocks.writable) {
            dr_seti(&zibo_chocks, 0);
        } else {
            XPLMSpeakString(_("Pushback warning: unable to remove "
                              "your chocks. Remove them yourself, or else I "
                              "won't be able to push your aircraft."));
            logMsg(BP_WARN_LOG "unable to remove your chocks.");
        }
    }

    dir = hdg2dir(bp_ls.tug->pos.hdg);
    if (bp_ls.tug->info->lift_type == LIFT_GRAB) {
        p_end = vect2_add(bp_ls.tug->pos.pos, vect2_scmul(dir,
                                                          -(bp_ls.tug->info->apch_dist +
                                                            bp_ls.tug->info->lift_wall_z -
                                                            tug_lift_wall_off(bp_ls.tug))));
    } else {
        p_end = vect2_add(bp_ls.tug->pos.pos, vect2_scmul(dir,
                                                          -(bp_ls.tug->info->apch_dist + bp_ls.tug->info->plat_z)));
    }
    VERIFY(tug_drive2point(bp_ls.tug, p_end, bp.cur_pos.hdg));
    bp.step++;
    bp.step_start_t = bp.cur_t;
}

static void
pb_step_driving_up_connect(void) {
    if (!slave_mode && !cfg_ignore_park_break)
        brakes_set(B_TRUE);
    if (!tug_is_stopped(bp_ls.tug)) {
        /*
         * Keep resetting the start time to enforce a state
         * transition delay once the tug stops.
         */
        bp.step_start_t = bp.cur_t;
    } else if (bp.cur_t - bp.step_start_t >= STATE_TRANS_DELAY) {
        bp.winching.start_acf_pos = bp.cur_pos.pos;
        bp.step++;
        bp.step_start_t = bp.cur_t;
    }
}

static void
pb_step_connect_grab(void) {
    double d_t = bp.cur_t - bp.step_start_t;
    double cradle_closed_fract = d_t / PB_CONN_LIFT_DELAY;

    cradle_closed_fract = MAX(MIN(cradle_closed_fract, 1), 0);
    tug_set_lift_arm_pos(bp_ls.tug, 1 - cradle_closed_fract, B_TRUE);

    if (!slave_mode) {
        /* When grabbing, keep the aircraft firmly in place */
        if (!cfg_ignore_park_break)
            brakes_set(B_TRUE);
    }

    if (cradle_closed_fract >= 1) {
        bp.step++;
        bp.step_start_t = bp.cur_t;
    }
}

static void
pb_step_connect_winch(void) {
    double d_t = bp.cur_t - bp.step_start_t;
    const tug_info_t *ti = bp_ls.tug->info;
    double winch_total, winched_dist;

    /* spend some time putting the winching strap in place */
    if (!bp.winching.complete && d_t < STATE_TRANS_DELAY)
        return;

    tug_set_lift_pos(0);
    tug_set_winch_on(bp_ls.tug, B_TRUE);

    /* after installing the strap, wait some more to make the pbrake call */
    if (!bp.winching.complete && d_t < 2 * STATE_TRANS_DELAY) {
        tug_set_lift_arm_pos(bp_ls.tug, 1.0, B_TRUE);
        return;
    }

    if (!bp.winching.complete && pbrake_is_set()) {
        if (!bp.winching.pbrk_rele_called) {
            msg_play(MSG_WINCH);
            bp.last_voice_t = bp.cur_t;
            bp.winching.pbrk_rele_called = B_TRUE;
        }
        return;
    }

    if (!slave_mode) {
        brakes_set(B_FALSE);
    }

    winch_total = ti->lift_wall_z - ti->plat_z -
                  tug_lift_wall_off(bp_ls.tug);
    winched_dist = vect2_dist(bp.winching.start_acf_pos, bp.cur_pos.pos);
    if (winched_dist < winch_total && !bp.winching.complete) {
        /*
         * While 'winch_total' tells us how far we need to winch,
         * the animation values are as a proportion of the maximum
         * possible winching distance (i.e. at the smallest tirrad).
         */
        double x = winched_dist / (ti->lift_wall_z - ti->plat_z);
        if (!slave_mode) {
            double lift = ti->plat_h * x + bp.acf.nw_len;
            push_at_speed(0.05, 0.05, B_FALSE, B_FALSE);
            dr_setvf(&drs.leg_len, &lift, bp.acf.nw_i, 1);
        }
        tug_set_lift_arm_pos(bp_ls.tug, 1 - x, B_TRUE);
        tug_set_TE_override(bp_ls.tug, B_TRUE);
        tug_set_TE_snd(bp_ls.tug, PB_LIFT_TE, bp.d_t);
        /*
         * While winching, we can simply look at the normal nose gear
         * animation speed to determine the gear rotation speed,
         * since our tug is standing still and it's the aircraft
         * which is moving.
         */
        dr_getvf32(&drs.tire_rot_spd, &bp.anim.nosewheel_rot_spd,
                   bp.acf.nw_i, 1);
    } else {
        bp.winching.complete = B_TRUE;
        /*
         * Stop nosewheel animation when we're done winching.
         */
        bp.anim.nosewheel_rot_spd = 0;
    }

    if (bp.winching.complete) {
        bp.step++;
        bp.step_start_t = bp.cur_t;
    }
}

static void
pb_step_grab(void) {
    if (!slave_mode) {
        double steer = 0;
        dr_setvf(&drs.tire_steer_cmd, &steer, bp.acf.nw_i, 1);
    }
    tug_set_cradle_beeper_on(bp_ls.tug, B_TRUE);
    tug_set_lift_in_transit(B_TRUE);
    if (bp_ls.tug->info->lift_type == LIFT_GRAB)
        pb_step_connect_grab();
    else
        pb_step_connect_winch();
}

static void
pb_step_lift(void) {
    double d_t = bp.cur_t - bp.step_start_t;
    double lift;
    double lift_fract = d_t / PB_CONN_LIFT_DURATION;

    lift_fract = MAX(MIN(lift_fract, 1), 0);
    tug_set_lift_pos(lift_fract);

    /* Iterate the lift */
    lift = (bp_ls.tug->info->lift_height * lift_fract) + bp.acf.nw_len +
           tug_plat_h(bp_ls.tug);
    if (!slave_mode && !cfg_ignore_park_break) {
        brakes_set(B_TRUE);
        dr_setvf(&drs.leg_len, &lift, bp.acf.nw_i, 1);
    }

    /*
     * While lifting, we simulate a ramp-up and ramp-down of the
     * tug's Tractive Effort to simulate that the engine is
     * being used to pressurize a hydraulic lift system.
     */
    if (d_t < PB_CONN_LIFT_DURATION) {
        tug_set_TE_override(bp_ls.tug, B_TRUE);
        tug_set_TE_snd(bp_ls.tug, PB_LIFT_TE, bp.d_t);
    }
    if (d_t >= PB_CONN_LIFT_DURATION) {
        tug_set_TE_override(bp_ls.tug, B_TRUE);
        tug_set_TE_snd(bp_ls.tug, 0, bp.d_t);
        tug_set_cradle_beeper_on(bp_ls.tug, B_FALSE);
        tug_set_lift_in_transit(B_FALSE);
        tug_set_TE_override(bp_ls.tug, B_FALSE);
    }

    if (d_t >= PB_CONN_LIFT_DURATION + STATE_TRANS_DELAY) {
        bp_connected = B_TRUE;
        if (late_plan_requested) {
            /*
             * The user requested a late plan, so this is as
             * far as we can go without segments. Also wait
             * for the camera to stop.
             */
            if (!late_plan_end_cond()) {
                bp_hint_status_str = _("Connected to the aircraft, waiting for clearance");
                return;
            }
            late_plan_requested = B_FALSE;
            /*
             * We normally save the route during bp_start,
             * but since the user requested late planning,
             * we need to save it now.
             */
            if (!slave_mode) {
                plan_complete = B_TRUE;
                route_save(&bp.segs);
            }
        }

        if (bp_ls.tug->info->lift_type != LIFT_WINCH) {
            msg_play(MSG_CONNECTED);
            bp.last_voice_t = bp.cur_t;
        }
        bp.step++;
        bp.step_start_t = bp.cur_t;
    }
}

static void
pb_step_connected(void) {
    if (pbrake_is_set() ||
        bp.cur_t - bp.last_voice_t < msg_dur(MSG_CONNECTED)) {
        /*
         * Keep resetting the start time to enforce the state delay
         * after the message is done and the parking brake is released.
         */
        bp.step_start_t = bp.cur_t;
        bp_hint_status_str = _("Waiting for the parking brakes release");
    } else if (bp.cur_t - bp.step_start_t >= STATE_TRANS_DELAY) {
        if (!slave_mode) {
            bool_t backward = true; 
            if (!push_manual.active) {
                seg_t *seg = list_head(&bp.segs);
                ASSERT(seg != NULL);
                backward = seg->backward;
            }
            if (dr_geti(&drs.num_engns) == 0 ||
                eng_is_running() || !eng_ok2start()) {
                msg_play(backward ? MSG_START_PB_NOSTART :
                         MSG_START_TOW_NOSTART);
            } else {
                msg_play(backward ? MSG_START_PB :
                         MSG_START_TOW);
            }
        } else {
            /*
             * Since we don't know the segs, we'll just
             * assume it's going to be backward (as that's
             * the most likely direction anyhow).
             */
            msg_play(MSG_START_PB);
        }

        bp.step++;
        bp.step_start_t = bp.cur_t;
        bp.last_voice_t = bp.cur_t;
    }
}


static void
pb_step_waiting_for_doors(void) {
    if (!acf_doors_closed(B_TRUE)) {
        int doors_check;
        conf_get_i_per_acf((char *)"doors_check", &doors_check);
        if (doors_check == ActiveWithMessage) {
            XPLMSpeakString(_(MSG_DOORS_GPU));
        }
    } 
    bp.step++;
    bp.step_start_t = bp.cur_t;
}

static void
pb_step_pushing(void) {
    if (dr_geti(&drs.landing_lights_on) != 0 ||
        dr_geti(&drs.taxi_light_on) != 0) {
        if (!slave_mode)
            push_at_speed(0, bp.veh.max_accel, B_TRUE, B_TRUE);
        if (!bp.light_warn) {
            if (dr_geti(&drs.landing_lights_on) != 0) {
                XPLMSpeakString(_("Hey! Quit blinding me with "
                                  "your landing lights! Turn them off!"));
            } else {
                XPLMSpeakString(_("Hey! Quit blinding me with "
                                  "your taxi light! Turn it off!"));
            }
        }
        bp.light_warn = B_TRUE;
        return;
    } else if (bp.light_warn) {
        bp.light_warn = B_FALSE;
    }

    if (!slave_mode) {
        dr_seti(&drs.override_steer, 1);
        if (!bp_run_push()) {
            bp.step++;
            bp.step_start_t = bp.cur_t;
            op_complete = B_TRUE;
            manual_bp_stop();
        }
    } else {
        /*
         * Since in slave mode we don't actually know our
         * tractive effort, just simulate it by following
         * the aircraft's speed of motion.
         */
        tug_set_TE_override(bp_ls.tug, B_FALSE);
    }
}

static void
pb_step_stopping(void) {
    bool_t done = B_TRUE;

    tug_set_TE_override(bp_ls.tug, B_FALSE);
    if (!slave_mode) {
        vehicle_pos_t corr_pos;
        double steer, rhdg;

        VERIFY3S(dr_getvf(&drs.tire_steer_cmd, &steer, bp.acf.nw_i,
                          1), ==, 1);
        corr_pos = corr_acf_pos();
        if (!isnan(bp.last_hdg) &&
            fabs(rhdg = rel_hdg(corr_pos.hdg, bp.last_hdg)) > 1) {
            double amp = fx_lin(bp.veh.wheelbase /
                                bp_ls.tug->veh.wheelbase, 1, 3, 5, 10);
            double nsteer = (bp.last_seg_is_back ? -1 : 1) * rhdg *
                            MAX(MIN(amp, 10), 2);
            turn_nosewheel(nsteer);
            push_at_speed(bp.last_seg_is_back ? -MIN_SPEED_XP10 :
                          MIN_SPEED_XP10, bp.veh.max_accel, B_FALSE, B_FALSE);
            done = B_FALSE;
        } else if (ABS(bp_ls.tug->cur_steer) >
                   TOW_COMPLETE_TUG_STEER_THRESH ||
                   ABS(steer) > TOW_COMPLETE_ACF_STEER_THRESH) {
            /* Keep pushing until steering is neutralized */
            turn_nosewheel(0);
            push_at_speed(bp.last_seg_is_back ? -MIN_SPEED_XP10 :
                          MIN_SPEED_XP10, bp.veh.max_accel, B_FALSE, B_FALSE);
            done = B_FALSE;
        } else {
            turn_nosewheel(0);
            push_at_speed(0, bp.veh.max_accel, B_FALSE, B_TRUE);
        }
    }
    if (ABS(bp.cur_pos.spd) >= SPEED_COMPLETE_THRESH || !done) {
        /*
         * Keep resetting the start time to enforce a delay
         * once stopped.
         */
        bp.step_start_t = bp.cur_t;
    } else {
        if (!slave_mode && !cfg_ignore_park_break)
            brakes_set(B_TRUE);
        if (bp.cur_t - bp.step_start_t >= STATE_TRANS_DELAY) {
            msg_play(MSG_OP_COMPLETE);
            bp.step++;
            bp.step_start_t = bp.cur_t;
            bp.last_voice_t = bp.cur_t;
        }
    }
}

static void
pb_step_stopped(void) {
    if (!slave_mode) {
        turn_nosewheel(0);
        push_at_speed(0, bp.veh.max_accel, B_FALSE, B_FALSE);
        if (!cfg_ignore_park_break)
            brakes_set(B_TRUE);
    }
    if (!pbrake_is_set() && !cfg_ignore_park_break) {
        /*
         * Ignoring Brake status if ignore_park_break is set
         * Keep resetting the start time to enforce a delay
         * when the parking brake is set.
         */
        bp.step_start_t = bp.cur_t;
        bp_hint_status_str = _("Waiting for the parking brakes set");
    } else if (bp.cur_t - bp.step_start_t >= STATE_TRANS_DELAY &&
               bp.cur_t - bp.last_voice_t >= msg_dur(MSG_OP_COMPLETE) +
                                             STATE_TRANS_DELAY) {
        msg_play(MSG_DISCO);
        bp.step++;
        bp.step_start_t = bp.cur_t;
        bp.last_voice_t = bp.cur_t;
    }
}

static void
pb_step_lowering(void) {
    double d_t = bp.cur_t - bp.step_start_t;
    double lift_fract = 1 - ((d_t - STATE_TRANS_DELAY) /
                             PB_CONN_LIFT_DURATION);
    double lift;

    if (!slave_mode) {
        turn_nosewheel(0);
        if (!cfg_ignore_park_break)
            brakes_set(B_TRUE);
    }

    if (bp.cur_t - bp.last_voice_t < msg_dur(MSG_OP_COMPLETE)) {
        /*
         * Keep resetting step_start_t to properly calculate
         * lift_fract relative to our step_start_t.
         */
        bp.step_start_t = bp.cur_t;
        return;
    }

    tug_set_lift_in_transit(B_TRUE);

    /* Slight delay after the parking brake ann was made */
    if (d_t <= STATE_TRANS_DELAY)
        return;

    lift_fract = MAX(MIN(lift_fract, 1), 0);

    /* Iterate the lift */
    lift = (bp_ls.tug->info->lift_height * lift_fract) + bp.acf.nw_len +
           tug_plat_h(bp_ls.tug);
    if (!slave_mode)
        dr_setvf(&drs.leg_len, &lift, bp.acf.nw_i, 1);

    tug_set_lift_pos(lift_fract);
    tug_set_cradle_air_on(bp_ls.tug, B_TRUE, bp.cur_t);
    tug_set_cradle_beeper_on(bp_ls.tug, B_TRUE);

    if (lift_fract == 0) {
        tug_set_cradle_air_on(bp_ls.tug, B_FALSE, bp.cur_t);
        bp.step++;
        bp.step_start_t = bp.cur_t;
    }
}

static bool_t
pb_step_ungrabbing_grab(void) {
    double d_t = bp.cur_t - bp.step_start_t;
    double cradle_fract = d_t / PB_CRADLE_DELAY;

    cradle_fract = MAX(MIN(cradle_fract, 1), 0);
    tug_set_lift_arm_pos(bp_ls.tug, cradle_fract, B_TRUE);

    if (cradle_fract >= 1.0)
        tug_set_cradle_beeper_on(bp_ls.tug, B_FALSE);

    return (d_t >= PB_CRADLE_DELAY + STATE_TRANS_DELAY);
}

static bool_t
pb_step_ungrabbing_winch(void) {
    double d_t = bp.cur_t - bp.step_start_t;

    /*
     * enforce some delays between removing the winch strap and
     * driving away
     */
    if (d_t < STATE_TRANS_DELAY)
        return (B_FALSE);

    tug_set_winch_on(bp_ls.tug, B_FALSE);

    if (d_t < 2 * STATE_TRANS_DELAY)
        return (B_FALSE);

    return (B_TRUE);
}

static void
pb_step_ungrabbing(void) {
    bool_t complete;

    if (bp_ls.tug->info->lift_type == LIFT_GRAB)
        complete = pb_step_ungrabbing_grab();
    else
        complete = pb_step_ungrabbing_winch();

    if (complete) {
        if (!slave_mode)
            brakes_set(B_FALSE);

        tug_set_lift_in_transit(B_FALSE);
        tug_set_TE_override(bp_ls.tug, B_FALSE);

        /* reset the state for the disconnection phase */
        bp.reconnect = B_FALSE;
        bp.ok2disco = B_FALSE;

        bp.step++;
        bp.step_start_t = bp.cur_t;
    }
}

/*
 * This determines whether we perform a right or left turn. The direction of
 * the turn depends on whether our original starting position is to the left
 * or to the right of the aircraft.
 */
static bool_t
tug_clear_is_right(void) {
    if (VECT2_EQ(bp.start_pos, bp.cur_pos.pos)) {
        return (B_TRUE);
    } else {
        return (rel_hdg(bp.cur_pos.hdg, dir2hdg(vect2_sub(bp.start_pos,
                                                          bp.cur_pos.pos))) >= 0);
    }
}

static void
pb_step_closing_cradle(void) {
    double d_t = bp.cur_t - bp.step_start_t;

    tug_set_lift_in_transit(B_TRUE);
    tug_set_tire_sense_pos(bp_ls.tug, 1 - d_t / PB_CRADLE_DELAY);
    tug_set_lift_pos(d_t / PB_CRADLE_DELAY);

    if (d_t >= PB_CRADLE_DELAY) {
        tug_set_cradle_beeper_on(bp_ls.tug, B_FALSE);
        tug_set_lift_in_transit(B_FALSE);
    }

    if (d_t >= PB_CRADLE_DELAY + STATE_TRANS_DELAY) {
        /* determine which direction we'll drive away */
        bool_t right = tug_clear_is_right();
        msg_play(right ? MSG_DONE_RIGHT : MSG_DONE_LEFT);
        tug_set_cradle_lights_on(B_FALSE);

        tug_set_hazard_lights_on(B_FALSE);

        bp.step++;
        bp.step_start_t = bp.cur_t;
        bp.last_voice_t = bp.cur_t;
    }
}

static void
disco_win_draw(XPLMWindowID inWindowID, void *inRefcon) {
    int w, h, mx, my;

    UNUSED(inRefcon);
    h = monitor_def.h;
    w = monitor_def.w;
    XPLMGetMouseLocationGlobal(&mx, &my);

    XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
    if (inWindowID == bp_ls.disco_win) {
        bool_t is_lit = (mx >= monitor_def.x_origin + w / 2 - 1.5 * disco_buttons[0].w &&
                         mx <= monitor_def.x_origin + w / 2 - 0.5 * disco_buttons[0].w &&
                         my >= monitor_def.y_origin + h - 1.5 * disco_buttons[0].h &&
                         my <= monitor_def.y_origin + h - 0.5 * disco_buttons[0].h);
        draw_icon(&disco_buttons[0], monitor_def.x_origin + w / 2 - 1.5 * disco_buttons[0].w,
                  monitor_def.y_origin + h - 1.5 * disco_buttons[0].h, 1.0,
                  B_FALSE, is_lit);
    } else {
        bool_t is_lit = (mx >= monitor_def.x_origin + w / 2 + 0.5 * disco_buttons[1].w &&
                         mx <= monitor_def.x_origin + w / 2 + 1.5 * disco_buttons[1].w &&
                         my >= monitor_def.y_origin + h - 1.5 * disco_buttons[1].h &&
                         my <= monitor_def.y_origin + h - 0.5 * disco_buttons[1].h);
        ASSERT(inWindowID == bp_ls.recon_win);
        draw_icon(&disco_buttons[1], monitor_def.x_origin + w / 2 + 0.5 * disco_buttons[1].w,
                  monitor_def.y_origin + h - 1.5 * disco_buttons[1].h, 1.0,
                  B_FALSE, is_lit);
    }
}

static int
disco_handler(XPLMCommandRef cmd, XPLMCommandPhase phase, void *refcon) {
    UNUSED(cmd);
    UNUSED(phase);
    UNUSED(refcon);

    if (bp.step != PB_STEP_WAITING4OK2DISCO)
        return (0);
    bp.ok2disco = B_TRUE;

    return (1);
}

static int
recon_handler(XPLMCommandRef cmd, XPLMCommandPhase phase, void *refcon) {
    UNUSED(cmd);
    UNUSED(phase);
    UNUSED(refcon);

    if (bp.step != PB_STEP_WAITING4OK2DISCO)
        return (0);

    /*
     * Reconnection works as follows:
     * 1) We shift state back to the grabbing step, so the tug starts
     *    the reattachment and lift process.
     * 2) We notify the GUI portion that a reconnection has taken place.
     */
    op_complete = B_FALSE;
    bp.reconnect = B_TRUE;
    bp.step = PB_STEP_GRABBING;
    bp.step_start_t = bp.cur_t;
    bp_reconnect_notify();
    return (1);
}

static int
disco_win_click(XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus inMouse,
                void *inRefcon) {
    UNUSED(x);
    UNUSED(y);
    UNUSED(inRefcon);

    if (inMouse != xplm_MouseUp)
        return (1);
    if (inWindowID == bp_ls.disco_win) {
        XPLMCommandOnce(disco_cmd);
    } else if (inWindowID == bp_ls.recon_win)
        XPLMCommandOnce(recon_cmd);

    return (1);
}

static XPLMCursorStatus
nil_win_cursor(XPLMWindowID inWindowID, int x, int y, void *inRefcon) {
    UNUSED(inWindowID);
    UNUSED(x);
    UNUSED(y);
    UNUSED(inRefcon);
    return (xplm_CursorDefault);
}

static int
nil_win_wheel(XPLMWindowID inWindowID, int x, int y, int wheel, int clicks,
              void *inRefcon) {
    UNUSED(inWindowID);
    UNUSED(x);
    UNUSED(y);
    UNUSED(wheel);
    UNUSED(clicks);
    UNUSED(inRefcon);
    return (1);
}

static void
disco_intf_show(void) {
    XPLMCreateWindow_t disco_ops = {
            .structSize = sizeof(XPLMCreateWindow_t),
            .left = 0, .top = 0, .right = 0, .bottom = 0, .visible = 1,
            .drawWindowFunc = disco_win_draw,
            .handleMouseClickFunc = disco_win_click,
            .handleKeyFunc = nil_win_key,
            .handleCursorFunc = nil_win_cursor,
            .handleMouseWheelFunc = nil_win_wheel,
            .refcon = NULL
    };
    int w, h;

    initMonitorOrigin();
    h = monitor_def.h;
    w = monitor_def.w;

    disco_ops.left = monitor_def.x_origin + w / 2 - 1.5 * disco_buttons[0].w;
    disco_ops.right = monitor_def.x_origin + w / 2 - 0.5 * disco_buttons[0].w;
    disco_ops.top = monitor_def.y_origin + h - 0.5 * disco_buttons[0].h;
    disco_ops.bottom = monitor_def.y_origin + h - 1.5 * disco_buttons[0].h;
    bp_ls.disco_win = XPLMCreateWindowEx(&disco_ops);
    ASSERT(bp_ls.disco_win != NULL);
    XPLMBringWindowToFront(bp_ls.disco_win);

    disco_ops.left = monitor_def.x_origin + w / 2 + 0.5 * disco_buttons[1].w;
    disco_ops.right = monitor_def.x_origin + w / 2 + 1.5 * disco_buttons[1].w;
    disco_ops.top = monitor_def.y_origin + h - 0.5 * disco_buttons[1].h;
    disco_ops.bottom = monitor_def.y_origin + h - 1.5 * disco_buttons[1].h;
    bp_ls.recon_win = XPLMCreateWindowEx(&disco_ops);
    ASSERT(bp_ls.recon_win != NULL);
    XPLMBringWindowToFront(bp_ls.recon_win);
}

static void
disco_intf_hide(void) {
    if (bp_ls.disco_win != NULL) {
        XPLMDestroyWindow(bp_ls.disco_win);
        bp_ls.disco_win = NULL;
    }
    if (bp_ls.recon_win != NULL) {
        XPLMDestroyWindow(bp_ls.recon_win);
        bp_ls.recon_win = NULL;
    }
}

static int
magic_buttons_hit_check(int mx, int my) {
    bool_t is_hit;
    int max_x = 0;

    if (!bp_cam_is_running()) {
        for (int i = 0; magic_buttons[i].filename != NULL; i++) {
            max_x = MAX(max_x, magic_buttons[i].w);
        }    
        // pre-check only on x axis
        is_hit = (mx >= monitor_def.x_origin && mx <= monitor_def.x_origin + max_x);

        if (is_hit) {
            for (int i = 0; magic_buttons[i].filename != NULL; i++) {
                if (magic_buttons[i].wind_id != NULL) {
                    is_hit = (mx >= monitor_def.x_origin && mx <= monitor_def.x_origin + magic_buttons[i].w &&
                                    my >= monitor_def.y_origin + monitor_def.magic_squares_height - i * 1.5 * magic_buttons[i].h - magic_buttons[i].h &&
                                    my <= monitor_def.y_origin + monitor_def.magic_squares_height - i * 1.5 * magic_buttons[i].h);
                    if (is_hit) {
                        return i;
                    }
                }    
            }
        }
    }
    return -1;
}

static int
main_win_click(XPLMWindowID inWindowID, int mx, int my, XPLMMouseStatus inMouse,
                void *inRefcon) {
    int button_hit = magic_buttons_hit_check( mx,  my);

    UNUSED(inWindowID);
    UNUSED(inRefcon);

    if (inMouse != xplm_MouseUp)
        return (1);

    if (button_hit == 0 ) {
        XPLMCommandOnce(start_cam);
        return (1);
    }
    
    if (button_hit == 1) {
        XPLMCommandOnce(conn_first);
        return (1);
    }    
    
    if (button_hit == 2) {
        XPLMCommandOnce(start_pb);
        return (1);
    }    

    return (1);
}

static void
hide_bp_status(void) {
    	if (bp_hint_status != NULL) {
		XPDestroyWidget(bp_hint_status, 1);
		bp_hint_status = NULL;
	}
}


static void
show_bp_status(int mx, int my) {
    if ( bp_hint_previous_status_str != bp_hint_status_str) {
        hide_bp_status();
    }
    if ((bp_hint_status == NULL) && (bp_hint_status_str != NULL)) {
		int w = XPLMMeasureString(xplmFont_Proportional,
		    bp_hint_status_str, strlen(bp_hint_status_str));
		XPWidgetID caption;

		bp_hint_status = create_widget_rel(mx,
		     my, B_TRUE, w + 20,
		    HINTBAR_HEIGHT, 0, "", 1, NULL, xpWidgetClass_MainWindow);
		XPSetWidgetProperty(bp_hint_status, xpProperty_MainWindowType,
		    xpMainWindowStyle_Translucent);

		caption = create_widget_rel(5, 0, B_FALSE, w, HINTBAR_HEIGHT,
		    1, bp_hint_status_str, 0, bp_hint_status, xpWidgetClass_Caption);
		XPSetWidgetProperty(caption, xpProperty_CaptionLit, 1);

		XPShowWidget(bp_hint_status);
        bp_hint_previous_status_str = bp_hint_status_str;
	}
}


static void
main_win_draw(XPLMWindowID inWindowID, void *inRefcon) {
    int mx, my;
    int button_hit;

    UNUSED(inWindowID);
    UNUSED(inRefcon);


    XPLMGetMouseLocationGlobal(&mx, &my);
    button_hit = magic_buttons_hit_check( mx,  my);

    XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
    if (!bp_cam_is_running()) {
        if (magic_buttons[0].wind_id != NULL)  {
        draw_icon(&magic_buttons[0], monitor_def.x_origin,
                    monitor_def.y_origin + monitor_def.magic_squares_height - magic_buttons[0].h, 1.0,
                    B_FALSE, button_hit == 0);
        }
        if (magic_buttons[1].wind_id  != NULL)  {
        draw_icon(&magic_buttons[1], monitor_def.x_origin,
                    monitor_def.y_origin + monitor_def.magic_squares_height - 1.5 * magic_buttons[0].h - magic_buttons[0].h, 1.0,
                    B_FALSE, button_hit == 1);
        }            
        if (magic_buttons[2].wind_id  != NULL)  {
             draw_icon(&magic_buttons[2], monitor_def.x_origin,
                        monitor_def.y_origin + monitor_def.magic_squares_height - 3 * magic_buttons[0].h - magic_buttons[0].h, 1.0,
                        B_FALSE, button_hit == 2);
        }

        int pos_x = monitor_def.x_origin;
        int pos_y = monitor_def.y_origin + monitor_def.magic_squares_height - 4.5 * magic_buttons[3].h - magic_buttons[3].h;
        if (magic_buttons[3].wind_id  != NULL)  {
            draw_icon(&magic_buttons[3], pos_x,pos_y, 1.0,
                        B_FALSE, button_hit == 3);
            if (button_hit == 3) {
                show_bp_status(pos_x,pos_y);
            } else {
                hide_bp_status();
            }
        }
    }
}

static void
main_intf_show(void) {
    bool_t always_connect_tug_first = B_FALSE;
    (void) conf_get_b(bp_conf,"always_connect_tug_first", &always_connect_tug_first);

    if ((start_pb_enable) && (tug_auto_start && tug_starts_next_plane) && acf_doors_closed(B_TRUE)) {
        int beacon_light = dr_geti(&drs.beacon_light);
        if ( (previous_beacon == 0) && (beacon_light) ) {
            previous_beacon = beacon_light;
            tug_pending_mode = B_TRUE;
            XPLMCommandOnce(conn_first);
        }
        previous_beacon = beacon_light; 
    }

    if ((bp_ls.planner_win == NULL) && (bp_ls.start_pb_win == NULL) && (bp_ls.conn_tug_first == NULL) && (bp_ls.pb_status_win == NULL) ) {
        initMonitorOrigin();
    }
    if ((bp_ls.planner_win == NULL) || (bp_ls.start_pb_win == NULL) || (bp_ls.conn_tug_first == NULL) || (bp_ls.pb_status_win == NULL) ) {
        XPLMCreateWindow_t magic_ops = {
                .structSize = sizeof(XPLMCreateWindow_t),
                .left = 0, .top = 0, .right = 0, .bottom = 0, .visible = 1,
                .drawWindowFunc = main_win_draw,
                .handleMouseClickFunc = main_win_click,
                .handleKeyFunc = nil_win_key,
                .handleCursorFunc = nil_win_cursor,
                .handleMouseWheelFunc = nil_win_wheel,
                .refcon = NULL
        };

        if (bp_ls.planner_win == NULL)  {
            load_icon(&magic_buttons[0]);
            magic_ops.left = monitor_def.x_origin ;
            magic_ops.right = magic_ops.left + magic_buttons[0].w;
            magic_ops.top = monitor_def.y_origin + monitor_def.magic_squares_height ;
            magic_ops.bottom = magic_ops.top - magic_buttons[0].h;
            bp_ls.planner_win = XPLMCreateWindowEx(&magic_ops);
            ASSERT(bp_ls.planner_win != NULL);
            XPLMBringWindowToFront(bp_ls.planner_win);
        }

        if (bp_ls.conn_tug_first == NULL) {
            load_icon(&magic_buttons[1]);
            magic_ops.left = monitor_def.x_origin ;
            magic_ops.right = magic_ops.left + magic_buttons[1].w;
            magic_ops.top = monitor_def.y_origin + monitor_def.magic_squares_height - 1.5 * magic_buttons[1].h;
            magic_ops.bottom =  magic_ops.top - magic_buttons[1].h;
            bp_ls.conn_tug_first = XPLMCreateWindowEx(&magic_ops);
            ASSERT(bp_ls.conn_tug_first != NULL);
            XPLMBringWindowToFront(bp_ls.conn_tug_first);
        }


        if  (bp_ls.start_pb_win == NULL) {
            load_icon(&magic_buttons[2]);
            magic_ops.left = monitor_def.x_origin ;
            magic_ops.right = magic_ops.left + magic_buttons[2].w;
            magic_ops.top = monitor_def.y_origin + monitor_def.magic_squares_height - 3 * magic_buttons[2].h;
            magic_ops.bottom =  magic_ops.top - magic_buttons[2].h;
            bp_ls.start_pb_win = XPLMCreateWindowEx(&magic_ops);
            ASSERT(bp_ls.start_pb_win != NULL);
            XPLMBringWindowToFront(bp_ls.start_pb_win);
        }

        if (bp_ls.pb_status_win == NULL) {
            load_icon(&magic_buttons[3]);
            magic_ops.left = monitor_def.x_origin ;
            magic_ops.right = magic_ops.left + magic_buttons[3].w;
            magic_ops.top = monitor_def.y_origin + monitor_def.magic_squares_height - 4.5 * magic_buttons[3].h;
            magic_ops.bottom =  magic_ops.top - magic_buttons[3].h;
            bp_ls.pb_status_win = XPLMCreateWindowEx(&magic_ops);
            ASSERT(bp_ls.pb_status_win != NULL);
            XPLMBringWindowToFront(bp_ls.pb_status_win);
        }
    }
    if (tug_starts_next_plane && tug_auto_start) {
    magic_buttons[0].wind_id =  NULL;
    magic_buttons[1].wind_id =  NULL;
    magic_buttons[2].wind_id = tug_pending_mode || ( ( bp.step == PB_STEP_LIFTING) && late_plan_requested) ? bp_ls.start_pb_win : NULL;
    } else {
    magic_buttons[0].wind_id = (!bp_started && !always_connect_tug_first) ? bp_ls.planner_win : NULL;
    magic_buttons[1].wind_id = (!bp_started && !always_connect_tug_first) ? bp_ls.conn_tug_first : NULL;
    magic_buttons[2].wind_id = !bp_started  || ( ( bp.step == PB_STEP_LIFTING) && late_plan_requested) ? bp_ls.start_pb_win : NULL;
    }
    magic_buttons[3].wind_id = bp_started ? bp_ls.pb_status_win : NULL;
}

void
main_intf_hide(void) {
    if (bp_ls.planner_win != NULL) {
        XPLMDestroyWindow(bp_ls.planner_win);
        unload_icon(&magic_buttons[0]);
        magic_buttons[0].wind_id = NULL;
        bp_ls.planner_win = NULL;
    }
    if (bp_ls.start_pb_win != NULL) {
        XPLMDestroyWindow(bp_ls.start_pb_win);
        unload_icon(&magic_buttons[2]);
        magic_buttons[2].wind_id = NULL;
        bp_ls.start_pb_win = NULL;
    }
    if (bp_ls.pb_status_win != NULL) {
        XPLMDestroyWindow(bp_ls.pb_status_win);
        unload_icon(&magic_buttons[3]);
        magic_buttons[3].wind_id = NULL;
        bp_ls.pb_status_win = NULL;
    }
    if (bp_ls.conn_tug_first != NULL) {
        XPLMDestroyWindow(bp_ls.conn_tug_first);
        unload_icon(&magic_buttons[1]);
        magic_buttons[1].wind_id = NULL;
        bp_ls.conn_tug_first = NULL;
    }
}

void
main_intf(bool_t force_hide) {
    if (get_pref_widget_status() // show also the magic button while in the pref window
     || (( bp_started || (acf_is_airliner() && acf_on_gnd_stopped(NULL))) && !force_hide) ) {
        main_intf_show();
    } else {
        main_intf_hide();
        hide_bp_status();
    }
}

static void
pb_step_waiting4ok2disco(void) {
    if (!bp.ok2disco) {
        if (bp_ls.disco_win == NULL && !slave_mode) {
            if (cfg_disco_when_done) {
                /*
                 * Don't actually show the interface, just
                 * fire the disconnection command.
                 */
                XPLMCommandOnce(disco_cmd);
                return;
            }
            disco_intf_show();
        }

        /* Keep resetting the start time to enforce the delay */
        bp.step_start_t = bp.cur_t;
        return;
    }

    /* Once the user clicked disconnect, hide the buttons immediately */
    disco_intf_hide();

    if (bp.cur_t - bp.step_start_t >= STATE_TRANS_DELAY) {
        vect2_t dir, p;

        dir = hdg2dir(bp.cur_pos.hdg);
        p = vect2_add(bp.cur_pos.pos, vect2_scmul(dir,
                                                  -bp.acf.nw_z + bp_ls.tug->info->apch_dist));
        (void) tug_drive2point(bp_ls.tug, p, bp.cur_pos.hdg);

        bp.step++;
        bp.step_start_t = bp.cur_t;
    }
}

static void
pb_step_starting2clear(void) {
    bool_t right;
    vect2_t turn_p, abeam_p, dir, norm_dir;
    double turn_hdg, back_hdg, square_side;

    /* Let the message play out before starting to move */
    if (bp.cur_t - bp.step_start_t < MAX(msg_dur(MSG_DONE_RIGHT),
                                         msg_dur(MSG_DONE_LEFT)) + STATE_TRANS_DELAY)
        return;

    right = tug_clear_is_right();
    square_side = MAX(4 * bp_ls.tug->veh.wheelbase, 1.5 * bp.veh.wheelbase);

    dir = hdg2dir(bp.cur_pos.hdg);
    norm_dir = vect2_norm(dir, right);

    /*
     * turn_p is offset 3x tug wheelbase forward and
     * half square_side to the direction of the turn.
     */
    turn_p = vect2_add(bp_ls.tug->pos.pos, vect2_scmul(dir,
                                                       3 * bp_ls.tug->veh.wheelbase));
    turn_p = vect2_add(turn_p, vect2_scmul(norm_dir,
                                           square_side / 2));
    turn_hdg = normalize_hdg(bp.cur_pos.hdg + (right ? 90 : -90));

    /*
     * abeam point is displaced from turn_p back 2x tug wheelbase,
     * 4x tug wheelbase in the direction of the turn and going the
     * opposite way to the aircraft at a 45 degree angle.
     */
    abeam_p = vect2_add(turn_p, vect2_scmul(vect2_neg(dir),
                                            2 * bp_ls.tug->veh.wheelbase));
    abeam_p = vect2_add(abeam_p, vect2_scmul(norm_dir,
                                             4 * bp_ls.tug->veh.wheelbase));
    back_hdg = normalize_hdg(turn_hdg + (right ? 45 : -45));

    VERIFY(tug_drive2point(bp_ls.tug, turn_p, turn_hdg));
    VERIFY(tug_drive2point(bp_ls.tug, abeam_p, back_hdg));

    bp.step++;
    bp.step_start_t = bp.cur_t;
}

static void
drive_away_fallback(void) {
    /*
     * If all else fails, reset the tug's position to get rid of an
     * intermediate turn segment and just send the tug straight for
     * a fixed distance.
     */
    vect2_t end_p = vect2_add(bp_ls.tug->pos.pos,
                              vect2_scmul(hdg2dir(bp_ls.tug->pos.hdg), TUG_DRIVE_AWAY_DIST));

    tug_set_pos(bp_ls.tug, bp_ls.tug->pos.pos, bp_ls.tug->pos.hdg, 0);
    VERIFY(tug_drive2point(bp_ls.tug, end_p, bp_ls.tug->pos.hdg));
}

static void
pb_step_clear_signal(void) {
    double acf2start_lat_displ, acf2start_long_displ;
    vect2_t acf2start, acfdir;

    tug_set_clear_signal(B_TRUE, tug_clear_is_right());

    if (bp.cur_t - bp.step_start_t < CLEAR_SIGNAL_DELAY)
        return;

    /*
     * In order to determine if we should be even attempting to reach
     * our starting point, we make sure that start_pos isn't within a
     * box as follows:
     *                 -4 x wheelbase
     *                   |<----->|
     *                   |       |
     *           ------- +-------+------------------>>> (to infinity)
     *  1.5x     ^       |
     * wheelbase |       |       |
     *           v______ |   |___|__
     *                   |   |   |
     *                   |       |
     *                   |
     *                   +-------------------------->>>
     */
    acf2start = vect2_sub(bp.start_pos, bp.cur_pos.pos);
    acfdir = hdg2dir(bp.cur_pos.hdg);
    acf2start_lat_displ = fabs(vect2_dotprod(vect2_norm(acfdir, B_TRUE),
                                             acf2start));
    acf2start_long_displ = vect2_dotprod(acfdir, acf2start);

    if (acf2start_lat_displ < 1.5 * bp.veh.wheelbase &&
        acf2start_long_displ > -4 * bp.veh.wheelbase) {
        drive_away_fallback();
    } else {
        double rhdg = fabs(rel_hdg(bp_ls.tug->pos.hdg,
                                   dir2hdg(vect2_sub(bp.start_pos, bp_ls.tug->pos.pos))));
        /*
         * start_pos seems far enough away from the aircraft that
         * it won't be a problem if we drive to it. Just make sure
         * we're not trying to back into it.
         */
        if (rhdg >= 90 || !tug_drive2point(bp_ls.tug, bp.start_pos,
                                           bp.start_hdg)) {
            /*
             * It's possible the start_pos is beyond a 90 degree
             * turn, so we'd attempt to back into it. Try to stick
             * in an intermediate 90-degree turn in its direction.
             */
            bool_t right = (rel_hdg(bp_ls.tug->pos.hdg, dir2hdg(
                    vect2_sub(bp.start_pos, bp_ls.tug->pos.pos))) >= 0);
            vect2_t dir = hdg2dir(bp_ls.tug->pos.hdg);
            vect2_t turn_p = vect2_add(bp_ls.tug->pos.pos,
                                       vect2_scmul(dir, 2 * bp_ls.tug->veh.wheelbase));
            turn_p = vect2_add(turn_p, vect2_scmul(vect2_norm(dir,
                                                              right), 2 * bp_ls.tug->veh.wheelbase));
            if (!tug_drive2point(bp_ls.tug, turn_p, normalize_hdg(
                    bp_ls.tug->pos.hdg + (right ? 90 : -90))) ||
                !tug_drive2point(bp_ls.tug, bp.start_pos,
                                 bp.start_hdg)) {
                drive_away_fallback();
            }
        }
    }
    tug_set_clear_signal(B_FALSE, B_FALSE);
    bp.step++;
    bp.step_start_t = bp.cur_t;
}

/*
 * Updates the tug's position with respect to where we are and its orientation
 * based on the tug's current steering input. When `pos_only' is true, only
 * the tug's position is update to match our nose gear position, but we leave
 * its heading untouched. This is because this can be called from the draw
 * function as well, which might update more frequently than the flight loop,
 * so we want to keep the tug firmly attached to our nosewheel, but not
 * actually change any params that might affect our steering.
 */
static void
tug_pos_update(vect2_t my_pos, double my_hdg, bool_t pos_only) {
    double tug_hdg, tug_spd, steer, radius;
    vect2_t dir, tug_pos;

    dr_getvf(&drs.tire_steer_cmd, &steer, bp.acf.nw_i, 1);

    tug_spd = tug_speed();

    radius = tan(DEG2RAD(90 - bp_ls.tug->cur_steer)) *
             bp_ls.tug->veh.wheelbase;
    if (pos_only) {
        tug_hdg = bp_ls.tug->pos.hdg;
    } else if (slave_mode) {
        /*
         * In slave mode, the tug tracks our nosewheel and doesn't
         * actually do any steering of its own.
         */
        tug_hdg = normalize_hdg(my_hdg + steer);
    } else if (fabs(radius) < 1e3) {
        double d_hdg = RAD2DEG(tug_spd / radius) * bp.d_t;
        double r_hdg;

        tug_hdg = normalize_hdg(bp_ls.tug->pos.hdg + d_hdg);
        r_hdg = rel_hdg(my_hdg, tug_hdg);
        /* check if we hit the hard steering stop */
        if (r_hdg > bp.veh.max_steer)
            tug_hdg = normalize_hdg(my_hdg + bp.veh.max_steer);
        else if (r_hdg < -bp.veh.max_steer)
            tug_hdg = normalize_hdg(my_hdg - bp.veh.max_steer);
    } else {
        tug_hdg = bp_ls.tug->pos.hdg;
    }

    dir = hdg2dir(my_hdg);
    if (bp.step == PB_STEP_GRABBING &&
        bp_ls.tug->info->lift_type == LIFT_WINCH) {
        /*
         * When winching the aircraft forward, we keep the tug in a
         * fixed position relative to where the aircraft was when the
         * winching operation started.
         */
        tug_set_pos(bp_ls.tug, vect2_add(bp.winching.start_acf_pos,
                                         vect2_scmul(dir, (-bp.acf.nw_z) +
                                                          (-bp_ls.tug->info->plat_z))), my_hdg, 0);
    } else {
        vect2_t off_v = vect2_scmul(hdg2dir(tug_hdg),
                                    (-bp_ls.tug->info->lift_wall_z) +
                                    tug_lift_wall_off(bp_ls.tug));
        tug_pos = vect2_add(vect2_add(my_pos, vect2_scmul(dir,
                                                          -bp.acf.nw_z)), off_v);
        tug_set_pos(bp_ls.tug, tug_pos, tug_hdg, tug_spd);
    }
}

static float
bp_run(float elapsed, float elapsed2, int counter, void *refcon) {
    UNUSED(elapsed);
    UNUSED(elapsed2);
    UNUSED(counter);
    UNUSED(refcon);

    bp_gather();
    /*
     * This used to draw the tug from a drawing phase, but since
     * we've switched to the XPLMInstance API, this instead updates
     * the tug's position.
     */
    draw_tugs();

    if (bp.cur_t - bp.last_t < MIN_STEP_TIME)
        return (-1);

    bp.d_pos.pos = vect2_sub(bp.cur_pos.pos, bp.last_pos.pos);
    bp.d_pos.hdg = rel_hdg(bp.last_pos.hdg, bp.cur_pos.hdg);
    bp.d_pos.spd = bp.cur_pos.spd - bp.last_pos.spd;
    bp.d_t = bp.cur_t - bp.last_t;

    ASSERT(bp_ls.tug != NULL || bp.step <= PB_STEP_TUG_LOAD);
    if (bp_ls.tug != NULL) {
        /* drive slowly while approaching & moving away from acf */
        tug_run(bp_ls.tug, bp.d_t,
                bp.step == PB_STEP_DRIVING_UP_CONNECT ||
                bp.step == PB_STEP_MOVING_AWAY);
        tug_anim(bp_ls.tug, bp.d_t, bp.cur_t);

        if (list_head(&bp_ls.tug->segs) == NULL &&
            bp.step >= PB_STEP_GRABBING &&
            bp.step <= PB_STEP_UNGRABBING)
            tug_pos_update(bp.cur_pos.pos, bp.cur_pos.hdg, B_FALSE);
    }

    if (!slave_mode) {
        /*
         * We persistently try to enable nosewheel steering. If by
         * reaching PB_STEP_START nosewheel steering is still disabled,
         * that means something else is resetting the variable to '0'.
         * Stop the operation, somebody is trying to mess with us.
         */
        if (bp.step > PB_STEP_START && dr_geti(&drs.nw_steer_on) != 1) {
            XPLMSpeakString(_("Pushback failure: your flight "
                              "controls are preventing me from steering the "
                              "aircraft. Unbind any buttons you have set to "
                              "\"toggle nosewheel steering\"."));
            msg_stop();
            bp_complete();
            return (0);
        }
        dr_seti(&drs.nw_steer_on, 1);
        if (bp.step >= PB_STEP_DRIVING_UP_CONNECT &&
            bp.step <= PB_STEP_MOVING_AWAY)
            dr_seti(&drs.override_steer, 1);
        else
            dr_seti(&drs.override_steer, 0);
    }

    // that's the default, may be fine tuned in pb_step_lift
    bp_connected = (bp.step >= PB_STEP_CONNECTED &&
                    bp.step <= PB_STEP_MOVING_AWAY);

    /*
     * If we have no segs, means the user stopped the operation.
     * Jump to the appropriate state. If we haven't connected yet,
     * just disappear. If we have, jump to the stopping state.
     */
    if (!late_plan_requested &&
        ((!slave_mode && ((list_head(&bp.segs) == NULL) && !push_manual.active )) ||
         (slave_mode && op_complete))) {
        if (bp.step < PB_STEP_GRABBING) {
            bp_complete();
            return (0);
        }
        if (bp.step < PB_STEP_STOPPING) {
            /*
             * If we're effectively stopped, skip the stopping
             * step to avoid playing MSG_OP_COMPLETE.
             */
            if (ABS(bp.cur_pos.spd) < SPEED_COMPLETE_THRESH &&
                pbrake_is_set()) {
                bp.step = PB_STEP_STOPPED;
            } else {
                bp.step = PB_STEP_STOPPING;
            }
        }
    }

    /*
     * When performing quick debugging, skip the whole driving-up phase.
     */
    if (!slave_mode && bp_ls.tug != NULL && bp_ls.tug->info->quick_debug) {
        if (bp.step < PB_STEP_CONNECTED) {
            double lift = bp_ls.tug->info->lift_height +
                          bp.acf.nw_len;
            bp.step = PB_STEP_CONNECTED;
            tug_set_lift_pos(1);
            tug_set_lift_arm_pos(bp_ls.tug, 0, B_TRUE);
            dr_setvf(&drs.leg_len, &lift, bp.acf.nw_i, 1);
            /*
             * Just a quick'n'dirty way of removing all tug driving
             * segs. The actual tug position will be updated in
             * draw_tugs.
             */
            tug_set_pos(bp_ls.tug, ZERO_VECT2, bp.cur_pos.hdg, 0);
        } else if (bp.step == PB_STEP_UNGRABBING) {
            dr_setvf(&drs.leg_len, &bp.acf.nw_len, bp.acf.nw_i, 1);
            bp_complete();
            return (0);
        }
    }

    if (bp.step != PB_STEP_WAITING4OK2DISCO) {
        /*
         * If the user requests reconnection, we cannot destroy the
         * window from within the mouse handler, so we destroy it
         * here instead.
         */
        disco_intf_hide();
    }

    bp_hint_status_str = NULL ;

    switch (bp.step) {
        case PB_STEP_OFF:
            VERIFY_FAIL();
        case PB_STEP_TUG_LOAD:
            ASSERT3P(bp_ls.tug, ==, NULL);
            if (!pb_step_tug_load())
                return (0);
            tug_pending_mode = (tug_auto_start && tug_starts_next_plane);
            break;
        case PB_STEP_START:
            if (tug_pending_mode) {
                bp_hint_status_str = _("Push-back waiting to be called");
            }
          if (!tug_pending_mode || !tug_auto_start || !tug_starts_next_plane) {
            bp_hint_status_str = _("Push-back called");
            pb_step_start();
          }
            break;
        case PB_STEP_DRIVING_UP_CLOSE:
            bp_hint_status_str = _("Driving to the aircraft");
            pb_step_driving_up_close();
            break;
        case PB_STEP_WAITING_FOR_DOORS:
            bp_hint_status_str = _("Waiting for doors/GPU/ASU closed/disconnected");
            pb_step_waiting_for_doors();
            break;
        case PB_STEP_OPENING_CRADLE: {
            bp_hint_status_str = _("Waiting for doors/GPU/ASU closed/disconnected");
            if (acf_doors_closed(B_TRUE)) {
                bp_hint_status_str = _("Opening the cradle");
                double d_t = bp.cur_t - bp.step_start_t;

                tug_set_lift_in_transit(B_TRUE);
                tug_set_lift_pos(1 - d_t / PB_CRADLE_DELAY);
                tug_set_tire_sense_pos(bp_ls.tug, d_t / PB_CRADLE_DELAY);
                if (d_t >= PB_CRADLE_DELAY) {
                    tug_set_lift_in_transit(B_FALSE);
                    tug_set_cradle_beeper_on(bp_ls.tug, B_FALSE);
                    prop_single_adjust();
                }
                if (d_t >= PB_CRADLE_DELAY + STATE_TRANS_DELAY) {
                    if (!bp.reconnect) {
                        if (pbrake_is_set())
                            msg_play(MSG_RDY2CONN_NOPARK);
                        else
                            msg_play(MSG_RDY2CONN);
                        bp.last_voice_t = bp.cur_t;
                    }
                    bp.step++;
                    bp.step_start_t = bp.cur_t;
                }
            }
            break;
        }
        case PB_STEP_WAITING_FOR_PBRAKE:
            bp_hint_status_str = _("Waiting for the parking brakes set");
            pb_step_waiting_for_pbrake();
            break;
        case PB_STEP_DRIVING_UP_CONNECT:
            bp_hint_status_str = _("Connecting to the aircraft");
            pb_step_driving_up_connect();
            break;
        case PB_STEP_GRABBING:
            bp_hint_status_str = _("Grabbing the aircraft");
            pb_step_grab();
            break;
        case PB_STEP_LIFTING:
            bp_hint_status_str = _("Lifting the aircraft");
            pb_step_lift();
            break;
        case PB_STEP_CONNECTED:
            bp_hint_status_str = _("Connected to the aircraft");
            pb_step_connected();
            break;
        case PB_STEP_STARTING:
            bp_hint_status_str = _("Push-back started");
            if (!slave_mode) {
                dr_seti(&drs.override_steer, 1);
                brakes_set(B_FALSE);
            }
            if (bp.cur_t - bp.step_start_t >= PB_START_DELAY) {
                bp.step++;
                bp.step_start_t = bp.cur_t;
            } else if (!slave_mode) {
                if (!push_manual.active) {
                    seg_t *seg = list_tail(&bp.segs);
                    ASSERT(seg != NULL);
                    bp.last_seg_is_back = seg->backward;
                    /*
                    * Try to straighten out if we don't end
                    * in a straight segment.
                    */
                    if (seg->type == SEG_TYPE_TURN)
                        bp.last_hdg = seg->end_hdg;
                    else
                        bp.last_hdg = NAN;
                } else {
                    push_manual.angle = 0;
                    push_manual.pause = false;
                }
                turn_nosewheel(0);
                push_at_speed(0, bp.veh.max_accel, B_FALSE, B_FALSE);
            }
            break;
        case PB_STEP_PUSHING:
            bp_hint_status_str = _("Push-back in progress");
            pb_step_pushing();
            break;
        case PB_STEP_STOPPING:
            bp_hint_status_str = _("Push-back stopping");
            pb_step_stopping();
            break;
        case PB_STEP_STOPPED:
            bp_hint_status_str = _("Push-back stopped");
            pb_step_stopped();
            break;
        case PB_STEP_LOWERING:
            bp_hint_status_str = _("Lowering the nose");
            pb_step_lowering();
            break;
        case PB_STEP_UNGRABBING:
            bp_hint_status_str = _("Ungrabbing the nose");
            pb_step_ungrabbing();
            break;
        case PB_STEP_WAITING4OK2DISCO:
            bp_hint_status_str = _("Waiting the OK to disconnect");
            pb_step_waiting4ok2disco();
            break;
        case PB_STEP_MOVING_AWAY:
            bp_hint_status_str = _("Disconnecting the tug away from the aircraft");
            if (bp_ls.tug->info->lift_type == LIFT_WINCH && !slave_mode) {
                /*
                 * When moving the tug away from the aircraft, the
                 * aircraft will have been positioned on the platform.
                 * Slowly lower the nosewheel the rest of the way.
                 */
                double dist = vect2_dist(bp.cur_pos.pos,
                                         bp_ls.tug->pos.pos);
                const tug_info_t *ti = bp_ls.tug->info;
                double plat_len = ti->lift_wall_z - ti->plat_z;
                double x, lift, tirrad;

                dist -= (-bp.acf.nw_z);
                dist -= (-ti->lift_wall_z);
                x = 1 - (dist / plat_len);
                x = MIN(MAX(x, 0), 1);
                lift = ti->plat_h * x + bp.acf.nw_len;
                dr_setvf(&drs.leg_len, &lift, bp.acf.nw_i, 1);
                /*
                 * Roll the nosewheel slowly backwards to symbolize
                 * that the tug is slipping out from underneath.
                 */
                dr_getvf(&drs.tirrad, &tirrad, bp.acf.nw_i, 1);
                ASSERT(bp_ls.tug != NULL);
                if (dist / plat_len < 1) {
                    bp.anim.nosewheel_rot_spd =
                            -bp_ls.tug->veh_slow.max_fwd_spd /
                            MAX(tirrad, 1e-3);
                } else {
                    bp.anim.nosewheel_rot_spd = 0;
                }
            }
            if (tug_is_stopped(bp_ls.tug)) {
                tug_set_cradle_beeper_on(bp_ls.tug, B_TRUE);
                bp.step++;
                bp.step_start_t = bp.cur_t;
                bp.anim.nosewheel_rot_spd = 0;
            }
            break;
        case PB_STEP_CLOSING_CRADLE:
            bp_hint_status_str = _("Closing the cradle");
            pb_step_closing_cradle();
            break;
        case PB_STEP_STARTING2CLEAR:
            bp_hint_status_str = _("Moving to the side of the aircraft");
            pb_step_starting2clear();
            break;
        case PB_STEP_MOVING2CLEAR:
            bp_hint_status_str = _("Moving to the side of the aircraft");
            if (tug_is_stopped(bp_ls.tug)) {
                bp.step++;
                bp.step_start_t = bp.cur_t;
            }
            break;
        case PB_STEP_CLEAR_SIGNAL:
            bp_hint_status_str = _("Showing the pin and the clear signal");
            pb_step_clear_signal();
            break;
        case PB_STEP_DRIVING_AWAY:
            bp_hint_status_str = _("Driving the tug away back to his station");
            if (tug_is_stopped(bp_ls.tug) ||
                bp.cur_t - bp.step_start_t > MAX_DRIVING_AWAY_DELAY) {
                bp_complete();
                bp_hint_status_str = NULL;

                /*
                 * Can't unregister floop from within, so just tell
                 * X-Plane to not call us anymore. bp_fini will take
                 * care of the rest.
                 */
                return (0);
            }
            break;
    }

    bp.last_pos = bp.cur_pos;
    bp.last_t = bp.cur_t;
    dr_getvf(&drs.tire_steer_cmd, &bp.last_steer, bp.acf.nw_i, 1);

    return (-1);
}

unsigned
bp_num_segs(void) {
    if (!bp_init())
        return (0);
    return (list_count(&bp.segs));
}
