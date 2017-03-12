/*
	Copyright (C) 2017 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#define __GNU_SOURCE

#include "snis_opcode_def.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>

#include "snis_packet.h"
#include "snis_marshal.h"
#include "stacktrace.h"

#define NSUBCODES 10
#define NOPCODES (256 * NSUBCODES)

static struct opcode_format_descriptor opcode_def[NOPCODES] = { 0 };

static int init_opcode_subcode_def(uint8_t opcode, uint8_t subcode, char *format)
{
	int index = opcode * NSUBCODES + subcode;
	if (subcode > 9) {
		fprintf(stderr, "invalid subcode %d\n", subcode);
		return -1;
	}
	opcode_def[opcode * NSUBCODES + subcode].subcode = subcode;
	if (strcmp(format, "n/a") != 0)
		opcode_def[index].size = calculate_buffer_size(format);
	else
		opcode_def[index].size = 0;
	if (strlen(format) >= sizeof(opcode_def[index].format)) {
		fprintf(stderr, "Opcode format length is too long for opcode %hhu, subcode %hhu\n", opcode, subcode);
		return -1;
	}
	strncpy(opcode_def[index].format, format, sizeof(opcode_def[index].format));
	return 0;
}

static int init_opcode_def(uint8_t opcode, char *format)
{
	return init_opcode_subcode_def(opcode, OPCODE_NO_SUBCODE, format);
}

int snis_opcode_def_init(void)
{
	int rc = 0;

	rc |= init_opcode_def(OPCODE_UPDATE_SHIP, "bwwhSSSRRRwwRRRbbbwbbbbbbbbbbbbbwQQQbbb");
	rc |= init_opcode_def(OPCODE_UPDATE_STARBASE, "bwwSSSQ");
	rc |= init_opcode_def(OPCODE_UPDATE_LASER, "bwwwbSSSQ");
	rc |= init_opcode_def(OPCODE_UPDATE_TORPEDO, "bwwwSSS");
	rc |= init_opcode_def(OPCODE_UPDATE_PLAYER, "n/a");
	rc |= init_opcode_def(OPCODE_ID_CLIENT_SHIP, "bw");
	rc |= init_opcode_def(OPCODE_UPDATE_ASTEROID, "bwwSSSbbbb");
	rc |= init_opcode_def(OPCODE_REQUEST_YAW, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_THRUST, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_GUNYAW, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_TORPEDO, "b");
	rc |= init_opcode_def(OPCODE_DELETE_OBJECT, "bw");
	rc |= init_opcode_def(OPCODE_UPDATE_EXPLOSION, "bwwSSShhhb");
	rc |= init_opcode_def(OPCODE_PLAY_SOUND, "bh");
	rc |= init_opcode_def(OPCODE_REQUEST_SCIYAW, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_SCIBEAMWIDTH, "bb");
	rc |= init_opcode_def(OPCODE_UPDATE_SHIP2, "bwwhSSSRRRwwRRRbbbwbbbbbbbbbbbbbwQQQbbb");
	rc |= init_opcode_def(OPCODE_ECON_UPDATE_SHIP, "bwwhSSSQwbb");
	/* rc |= init_opcode_def(OPCODE_SHIP_SDATA, "bwbbbbbbbr"); */
	rc |= init_opcode_def(OPCODE_LOAD_TORPEDO, "b");
	rc |= init_opcode_def(OPCODE_REQUEST_THROTTLE, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_MANEUVERING_PWR, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_WARP_PWR, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_IMPULSE_PWR, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_SHIELDS_PWR, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_COMMS_PWR, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_SENSORS_PWR, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_PHASERBANKS_PWR, "wb");
	rc |= init_opcode_def(OPCODE_REQUEST_SCIZOOM, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_WARPDRIVE, "bwb");
	rc |= init_opcode_def(OPCODE_ENGAGE_WARP, "bwb");
	rc |= init_opcode_def(OPCODE_ROLE_ONSCREEN, "bb");
	rc |= init_opcode_def(OPCODE_SCI_SELECT_TARGET, "bw");
	rc |= init_opcode_def(OPCODE_UPDATE_DAMAGE, "n/a");
	rc |= init_opcode_def(OPCODE_REQUEST_LASER, "b");
	rc |= init_opcode_def(OPCODE_REQUEST_LASER_WAVELENGTH, "bwb");
	rc |= init_opcode_def(OPCODE_SCI_SELECT_COORDS, "bww");
	rc |= init_opcode_def(OPCODE_REQUEST_SHIELD, "bwb");
	rc |= init_opcode_def(OPCODE_UPDATE_RESPAWN_TIME, "bb");
	rc |= init_opcode_def(OPCODE_UPDATE_NETSTATS, "bqqwwwwwwwww");
	rc |= init_opcode_def(OPCODE_COMMS_TRANSMISSION, "n/a");
	rc |= init_opcode_def(OPCODE_WARP_LIMBO, "bh");
	rc |= init_opcode_def(OPCODE_DEMON_COMMAND, "n/a");
	rc |= init_opcode_def(OPCODE_UPDATE_NEBULA, "bwwSSSSQQSS");
	rc |= init_opcode_def(OPCODE_DAMCON_OBJ_UPDATE, "bwwwSSSRb");
	rc |= init_opcode_def(OPCODE_REQUEST_ROBOT_YAW, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_ROBOT_THRUST, "bb");
	rc |= init_opcode_def(OPCODE_DAMCON_SOCKET_UPDATE, "bwwwSSwbb");
	rc |= init_opcode_def(OPCODE_DAMCON_PART_UPDATE, "bwwwSSRbbb");
	rc |= init_opcode_def(OPCODE_REQUEST_ROBOT_GRIPPER, "b");
	rc |= init_opcode_def(OPCODE_MAINSCREEN_VIEW_MODE, "bRb");
	rc |= init_opcode_def(OPCODE_UPDATE_WORMHOLE, "bwwSSS");
	rc |= init_opcode_def(OPCODE_WORMHOLE_LIMBO, "bh");
	rc |= init_opcode_def(OPCODE_UPDATE_SPACEMONSTER, "bwwSSS");
	rc |= init_opcode_def(OPCODE_REQUEST_WEAPZOOM, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_NAVZOOM, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_REDALERT, "bb");
	rc |= init_opcode_def(OPCODE_UPDATE_POWER_DATA, "n/a");
	rc |= init_opcode_def(OPCODE_UPDATE_PLANET, "bwwSSSSwbbbbhbbbSbhbb");
	rc |= init_opcode_def(OPCODE_CREATE_ITEM, "bSSS");
	rc |= init_opcode_def(OPCODE_DEMON_COMMS_XMIT, "n/a");
	rc |= init_opcode_def(OPCODE_DEMON_FIRE_TORPEDO, "bw");
	rc |= init_opcode_def(OPCODE_DEMON_FIRE_PHASER, "bw");
	rc |= init_opcode_def(OPCODE_DEMON_POSSESS, "bw");
	rc |= init_opcode_def(OPCODE_DEMON_DISPOSSESS, "bw");
	rc |= init_opcode_subcode_def(OPCODE_DEMON_ROT, OPCODE_DEMON_ROT_YAW, "bwb");
	rc |= init_opcode_subcode_def(OPCODE_DEMON_ROT, OPCODE_DEMON_ROT_PITCH, "bwb");
	rc |= init_opcode_subcode_def(OPCODE_DEMON_ROT, OPCODE_DEMON_ROT_ROLL, "bwb");
	rc |= init_opcode_def(OPCODE_DEMON_THRUST, "bwb");
	rc |= init_opcode_def(OPCODE_DEMON_MOVE_OBJECT, "bwSSSQ");
	rc |= init_opcode_def(OPCODE_INITIATE_WARP, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_REVERSE, "bwb");
	rc |= init_opcode_def(OPCODE_UPDATE_LASERBEAM, "bwwww");
	rc |= init_opcode_def(OPCODE_WEAP_SELECT_TARGET , "bw");
	rc |= init_opcode_def(OPCODE_SCI_DETAILS, "bb");
	rc |= init_opcode_def(OPCODE_PROXIMITY_ALERT, "b");
	rc |= init_opcode_def(OPCODE_COLLISION_NOTIFICATION, "b");
	rc |= init_opcode_def(OPCODE_UPDATE_DERELICT, "bwwSSSb");
	rc |= init_opcode_def(OPCODE_DEMON_CLEAR_ALL, "b");
	rc |= init_opcode_def(OPCODE_EXEC_LUA_SCRIPT, "n/a");
	rc |= init_opcode_def(OPCODE_REQUEST_TRACTORBEAM, "bw");
	rc |= init_opcode_def(OPCODE_UPDATE_TRACTORBEAM, "bwwww");
	rc |= init_opcode_def(OPCODE_REQUEST_TRACTOR_PWR, "bwb");
	rc |= init_opcode_def(OPCODE_COMMS_MAINSCREEN, "bb");
	rc |= init_opcode_def(OPCODE_LOAD_SKYBOX, "n/a");
	rc |= init_opcode_def(OPCODE_ROBOT_AUTO_MANUAL, "bb");
	rc |= init_opcode_def(OPCODE_ADD_WARP_EFFECT, "bwSSSSSS");
	rc |= init_opcode_def(OPCODE_REQUEST_MAINZOOM, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_PITCH, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_ROLL, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_SCIBALL_YAW, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_SCIBALL_PITCH, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_SCIBALL_ROLL, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_MANUAL_GUNYAW, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_MANUAL_GUNPITCH, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_MANUAL_LASER, "b");
	rc |= init_opcode_def(OPCODE_REQUEST_WEAPONS_YAW_PITCH, "RR");
	rc |= init_opcode_def(OPCODE_UPDATE_COOLANT_DATA, "n/a");
	rc |= init_opcode_def(OPCODE_REQUEST_MANEUVERING_COOLANT, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_WARP_COOLANT, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_IMPULSE_COOLANT, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_SHIELDS_COOLANT, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_COMMS_COOLANT, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_SENSORS_COOLANT, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_PHASERBANKS_COOLANT, "bwb");
	rc |= init_opcode_def(OPCODE_REQUEST_TRACTOR_COOLANT, "bwb");
	rc |= init_opcode_def(OPCODE_SILENT_UPDATE_DAMAGE, "n/a");
	rc |= init_opcode_def(OPCODE_ECON_UPDATE_SHIP_DEBUG_AI, "bwwhSSSQwbbbbbbbSb");
	rc |= init_opcode_def(OPCODE_TOGGLE_DEMON_AI_DEBUG_MODE, "b");
	rc |= init_opcode_def(OPCODE_TOGGLE_DEMON_SAFE_MODE, "b");
	rc |= init_opcode_def(OPCODE_UPDATE_CARGO_CONTAINER, "bwwSSS");
	rc |= init_opcode_def(OPCODE_CYCLE_MAINSCREEN_POINT_OF_VIEW, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_UNIVERSE_TIMESTAMP, "b");
	rc |= init_opcode_def(OPCODE_UPDATE_UNIVERSE_TIMESTAMP, "bbwS");
	rc |= init_opcode_def(OPCODE_UPDATE_BUILD_INFO, "n/a");
	rc |= init_opcode_def(OPCODE_DETONATE, "bwSSSwU");
	rc |= init_opcode_def(OPCODE_ENSCRIPT, "n/a");
	rc |= init_opcode_def(OPCODE_SCI_ALIGN_TO_SHIP, "b");
	rc |= init_opcode_def(OPCODE_NAV_TRIDENT_MODE, "bwb");
	rc |= init_opcode_def(OPCODE_ATMOSPHERIC_FRICTION, "b");
	rc |= init_opcode_def(OPCODE_UPDATE_DOCKING_PORT, "bwwSSSSQb");
	rc |= init_opcode_def(OPCODE_DOCKING_MAGNETS, "bwb");
	rc |= init_opcode_def(OPCODE_CYCLE_NAV_POINT_OF_VIEW, "bb");
	rc |= init_opcode_def(OPCODE_REQUEST_MINING_BOT, "bw");
	rc |= init_opcode_def(OPCODE_REQUEST_STANDARD_ORBIT, "bwb");
	rc |= init_opcode_def(OPCODE_SWITCH_SERVER, "n/a");
	rc |= init_opcode_def(OPCODE_UPDATE_WARPGATE, "bwwSSSQ");
	rc |= init_opcode_def(OPCODE_ADD_PLAYER_ERROR, "bb");
	rc |= init_opcode_subcode_def(OPCODE_NATURAL_LANGUAGE_REQUEST,
					OPCODE_NL_SUBCOMMAND_TEXT_REQUEST, "n/a");
	rc |= init_opcode_subcode_def(OPCODE_NATURAL_LANGUAGE_REQUEST,
					OPCODE_NL_SUBCOMMAND_TEXT_TO_SPEECH, "n/a");
	rc |= init_opcode_def(OPCODE_SET_SOLARSYSTEM, "n/a");
	rc |= init_opcode_subcode_def(OPCODE_REQUEST_ROBOT_CMD,
					OPCODE_ROBOT_SUBCMD_STG, "bbSS"); /* set short term goal */
	rc |= init_opcode_subcode_def(OPCODE_REQUEST_ROBOT_CMD,
					OPCODE_ROBOT_SUBCMD_LTG, "bbSS"); /* set long term goal */
	rc |= init_opcode_subcode_def(OPCODE_TEXTSCREEN_OP, OPCODE_TEXTSCREEN_CLEAR, "bb");
	rc |= init_opcode_subcode_def(OPCODE_TEXTSCREEN_OP, OPCODE_TEXTSCREEN_TIMEDTEXT, "n/a");
	rc |= init_opcode_subcode_def(OPCODE_TEXTSCREEN_OP, OPCODE_TEXTSCREEN_MENU, "n/a");
	rc |= init_opcode_subcode_def(OPCODE_TEXTSCREEN_OP, OPCODE_TEXTSCREEN_MENU_CHOICE, "bbb");
	rc |= init_opcode_def(OPCODE_UPDATE_BLOCK, "bwwSSSSSSQbb");
	rc |= init_opcode_def(OPCODE_UPDATE_TURRET, "bwwSSSQQb");
	rc |= init_opcode_def(OPCODE_LATENCY_CHECK, "bqq");
	rc |= init_opcode_def(OPCODE_NOOP, "b");
	return rc;
}

/* Returns payload size of an opcode (that has no subcode) */
const int snis_opcode_payload_size(uint8_t opcode)
{
	int index = opcode * NSUBCODES;

	assert(opcode < NOPCODES);
	return opcode_def[index].size;
}

/* Returns payload size of an opcode, subcode pair */
const int snis_opcode_subcode_payload_size(uint8_t opcode, uint8_t subcode)
{
	int index = opcode * NSUBCODES + subcode;

	assert(opcode < NOPCODES);
	assert(subcode < NSUBCODES);
	return opcode_def[index].size;
}

/* Returns format of opcode that has no subcode */
const char *snis_opcode_format(uint8_t opcode)
{
	assert(opcode < NOPCODES);
	int index = opcode * NSUBCODES;
	return opcode_def[index].format;
}

/* Returns format of an opcode, subcode pair */
const char *snis_opcode_subcode_format(uint8_t opcode, uint8_t subcode)
{
	int index = opcode * NSUBCODES + subcode;

	assert(opcode < NOPCODES);
	assert(subcode < NSUBCODES);
	return opcode_def[index].format;
}

/* This does exactly what packed_buffer_new() in snis_marshal.c does
 * except it checks that the opcode format is as expected.
 * There are conflicting goals.  It would be nice if the formats
 * for opcodes were in one place.  However, it is also nice to see
 * the formats at the place where the packeds are packed and unpacked
 * which requires duplication of the formats.  It would also be nice
 * to detect if there is a problem with the formats (mismatch)
 * This is the compromise I came up with -- preserving the clarity
 * of having the formats visible at the point packets are created
 * or unpacked, (sacrificing a single point to change such formats)
 * while still checking that they are as expected -- though only at
 * run time.
 */
struct packed_buffer *snis_opcode_pkt(const char *format, ...)
{
	va_list ap, apcopy;
	struct packed_buffer *pb;
	int size = calculate_buffer_size(format);
	uint8_t opcode;

	if (size < 0)
		return NULL;
	pb = packed_buffer_allocate(size);
	if (!pb)
		return NULL;

	/* Check that this looks like an opcode format */
	if (format[0] != 'b') {
		fprintf(stderr, "Bad opcode format '%s'\n", format);
		va_end(ap);
		packed_buffer_free(pb);
		stacktrace("Bad opcode format");
		return NULL;
	}

	/* Check that the opcode format seems correct */
	va_start(ap, format);
	va_copy(apcopy, ap);
	opcode = (uint8_t) va_arg(apcopy, int);
	va_end(apcopy);
	const char *expected_format = snis_opcode_format(opcode);
	if (strcmp(expected_format, format) != 0) {
		packed_buffer_free(pb);
		fprintf(stderr, "Bad format '%s' for opcode %hhu, expected format '%s'\n",
			format, opcode, expected_format);
		stacktrace("Unexpected opcode format");
		va_end(ap);
		return NULL;
	}

	if (packed_buffer_append_va(pb, format, ap)) {
		packed_buffer_free(pb);
		va_end(ap);
		return NULL;
	}
	va_end(ap);
	return pb;
}