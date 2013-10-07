//Copyright Paul Reiche, Fred Ford. 1992-2002

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <assert.h>

#include "build.h"
#include "encount.h"
#include "starmap.h"
#include "libs/file.h"
#include "globdata.h"
#include "options.h"
#include "save.h"
#include "setup.h"
#include "state.h"
#include "grpinfo.h"

#include "libs/tasklib.h"
#include "libs/log.h"
#include "libs/misc.h"

//#define DEBUG_LOAD

ACTIVITY NextActivity;

static inline size_t
read_8 (void *fp, BYTE *v)
{
	BYTE t;
	if (!v) /* read value ignored */
		v = &t;
	return ReadResFile (v, 1, 1, fp);
}

static inline size_t
read_16 (void *fp, UWORD *v)
{
	UWORD t;
	if (!v) /* read value ignored */
		v = &t;
	return ReadResFile (v, 2, 1, fp);
}

static inline size_t
read_16s (void *fp, SWORD *v)
{
	SWORD t;
	if (!v) /* read value ignored */
		v = &t;
	return ReadResFile (v, 2, 1, fp);
}

static inline size_t
read_32 (void *fp, DWORD *v)
{
	DWORD t;
	if (!v) /* read value ignored */
		v = &t;
	return ReadResFile (v, 4, 1, fp);
}

static inline size_t
read_32s (void *fp, SDWORD *v)
{
	DWORD t;
	COUNT ret;
	// value was converted to unsigned when saved
	ret = read_32 (fp, &t);
	// unsigned to signed conversion
	if (v)
		*v = t;
	return ret;
}

static inline size_t
read_a8 (void *fp, BYTE *ar, COUNT count)
{
	assert (ar != NULL);
	return ReadResFile (ar, 1, count, fp) == count;
}

static inline size_t
skip_8 (void *fp, COUNT count)
{
	int i;
	for (i = 0; i < count; ++i)
	{
		if (read_8(fp, NULL) != 1)
			return 0;
	}
	return 1;
}

static inline size_t
read_str (void *fp, char *str, COUNT count)
{
	// no type conversion needed for strings
	return read_a8 (fp, (BYTE *)str, count);
}

static inline size_t
read_a16 (void *fp, UWORD *ar, COUNT count)
{
	assert (ar != NULL);

	for ( ; count > 0; --count, ++ar)
	{
		if (read_16 (fp, ar) != 1)
			return 0;
	}
	return 1;
}

static void
LoadEmptyQueue (void *fh)
{
	COUNT num_links;

	read_16 (fh, &num_links);
	if (num_links)
	{
		log_add (log_Error, "LoadEmptyQueue(): BUG: the queue is not empty!");
#ifdef DEBUG
		explode ();
#endif
	}
}

static void
LoadShipQueue (void *fh, QUEUE *pQueue)
{
	COUNT num_links;

	read_16 (fh, &num_links);

	while (num_links--)
	{
		HSHIPFRAG hStarShip;
		SHIP_FRAGMENT *FragPtr;
		COUNT Index;
		BYTE tmpb;

		read_16 (fh, &Index);

		hStarShip = CloneShipFragment (Index, pQueue, 0);
		FragPtr = LockShipFrag (pQueue, hStarShip);

		// Read SHIP_FRAGMENT elements
		read_8  (fh, &FragPtr->captains_name_index);
		read_8  (fh, &FragPtr->race_id);
		read_8  (fh, &FragPtr->index);
		read_16 (fh, &FragPtr->crew_level);
		read_16 (fh, &FragPtr->max_crew);
		read_8  (fh, &FragPtr->energy_level);
		read_8  (fh, &FragPtr->max_energy);

		UnlockShipFrag (pQueue, hStarShip);
	}
}

static void
LoadRaceQueue (void *fh, QUEUE *pQueue)
{
	COUNT num_links;

	read_16 (fh, &num_links);

	while (num_links--)
	{
		HFLEETINFO hStarShip;
		FLEET_INFO *FleetPtr;
		COUNT Index;
		BYTE tmpb;

		read_16 (fh, &Index);

		hStarShip = GetStarShipFromIndex (pQueue, Index);
		FleetPtr = LockFleetInfo (pQueue, hStarShip);

		// Read FLEET_INFO elements
		read_16 (fh, &FleetPtr->allied_state);
		read_8  (fh, &FleetPtr->days_left);
		read_8  (fh, &FleetPtr->growth_fract);
		read_8  (fh, &tmpb);
		FleetPtr->crew_level = tmpb;
		read_8  (fh, &tmpb);
		FleetPtr->max_crew = tmpb;
		read_8  (fh, &FleetPtr->growth);
		read_8  (fh, &FleetPtr->max_energy);
		read_16s(fh, &FleetPtr->loc.x);
		read_16s(fh, &FleetPtr->loc.y);

		read_16 (fh, &FleetPtr->actual_strength);
		read_16 (fh, &FleetPtr->known_strength);
		read_16s(fh, &FleetPtr->known_loc.x);
		read_16s(fh, &FleetPtr->known_loc.y);
		read_8  (fh, &FleetPtr->growth_err_term);
		read_8  (fh, &FleetPtr->func_index);
		read_16s(fh, &FleetPtr->dest_loc.x);
		read_16s(fh, &FleetPtr->dest_loc.y);

		UnlockFleetInfo (pQueue, hStarShip);
	}
}

static void
LoadGroupQueue (void *fh, QUEUE *pQueue)
{
	COUNT num_links;

	read_16 (fh, &num_links);

	while (num_links--)
	{
		HIPGROUP hGroup;
		IP_GROUP *GroupPtr;
		BYTE tmpb;

		hGroup = BuildGroup (pQueue, 0);
		GroupPtr = LockIpGroup (pQueue, hGroup);

		read_16 (fh, &GroupPtr->group_counter);
		read_8  (fh, &GroupPtr->race_id);
		read_8  (fh, &tmpb); /* was var2 */
		GroupPtr->sys_loc = LONIBBLE (tmpb);
		GroupPtr->task = HINIBBLE (tmpb);
		read_8  (fh, &GroupPtr->in_system); /* was crew_level */
		read_8  (fh, &tmpb); /* was energy_level */
		GroupPtr->dest_loc = LONIBBLE (tmpb);
		GroupPtr->orbit_pos = HINIBBLE (tmpb);
		read_8  (fh, &GroupPtr->group_id); /* was max_energy */
		read_16s(fh, &GroupPtr->loc.x);
		read_16s(fh, &GroupPtr->loc.y);

		UnlockIpGroup (pQueue, hGroup);
	}
}

static void
LoadEncounter (ENCOUNTER *EncounterPtr, void *fh)
{
	COUNT i;
	BYTE tmpb;

	EncounterPtr->pred = 0;
	EncounterPtr->succ = 0;
	EncounterPtr->hElement = 0;
	read_16s (fh, &EncounterPtr->transition_state);
	read_16s (fh, &EncounterPtr->origin.x);
	read_16s (fh, &EncounterPtr->origin.y);
	read_16  (fh, &EncounterPtr->radius);
	// former STAR_DESC fields
	read_16s (fh, &EncounterPtr->loc_pt.x);
	read_16s (fh, &EncounterPtr->loc_pt.y);
	read_8   (fh, &EncounterPtr->race_id);
	read_8   (fh, &EncounterPtr->num_ships);
	read_8   (fh, &EncounterPtr->flags);

	// Load each entry in the BRIEF_SHIP_INFO array
	for (i = 0; i < MAX_HYPER_SHIPS; i++)
	{
		BRIEF_SHIP_INFO *ShipInfo = &EncounterPtr->ShipList[i];

		read_8   (fh, &ShipInfo->race_id);
		read_16  (fh, &ShipInfo->crew_level);
		read_16  (fh, &ShipInfo->max_crew);
		read_8   (fh, &ShipInfo->max_energy);
	}

	// Load the stuff after the BRIEF_SHIP_INFO array
	read_32s (fh, &EncounterPtr->log_x);
	read_32s (fh, &EncounterPtr->log_y);
}

static void
LoadEvent (EVENT *EventPtr, void *fh)
{
	EventPtr->pred = 0;
	EventPtr->succ = 0;
	read_8   (fh, &EventPtr->day_index);
	read_8   (fh, &EventPtr->month_index);
	read_16  (fh, &EventPtr->year_index);
	read_8   (fh, &EventPtr->func_index);
}

static void
LoadClockState (CLOCK_STATE *ClockPtr, void *fh)
{
	read_8   (fh, &ClockPtr->day_index);
	read_8   (fh, &ClockPtr->month_index);
	read_16  (fh, &ClockPtr->year_index);
	read_16s (fh, &ClockPtr->tick_count);
	read_16s (fh, &ClockPtr->day_in_ticks);
}

static void
LoadGameState (GAME_STATE *GSPtr, void *fh)
{
	read_8   (fh, &GSPtr->glob_flags);
	read_8   (fh, &GSPtr->CrewCost);
	read_8   (fh, &GSPtr->FuelCost);
	read_a8  (fh, GSPtr->ModuleCost, NUM_MODULES);
	read_a8  (fh, GSPtr->ElementWorth, NUM_ELEMENT_CATEGORIES);
	read_16  (fh, &GSPtr->CurrentActivity);

	LoadClockState (&GSPtr->GameClock, fh);

	read_16s (fh, &GSPtr->autopilot.x);
	read_16s (fh, &GSPtr->autopilot.y);
	read_16s (fh, &GSPtr->ip_location.x);
	read_16s (fh, &GSPtr->ip_location.y);
	/* STAMP ShipStamp */
	read_16s (fh, &GSPtr->ShipStamp.origin.x);
	read_16s (fh, &GSPtr->ShipStamp.origin.y);
	read_16  (fh, &GSPtr->ShipFacing);
	read_8   (fh, &GSPtr->ip_planet);
	read_8   (fh, &GSPtr->in_orbit);

	/* VELOCITY_DESC velocity */
	read_16  (fh, &GSPtr->velocity.TravelAngle);
	read_16s (fh, &GSPtr->velocity.vector.width);
	read_16s (fh, &GSPtr->velocity.vector.height);
	read_16s (fh, &GSPtr->velocity.fract.width);
	read_16s (fh, &GSPtr->velocity.fract.height);
	read_16s (fh, &GSPtr->velocity.error.width);
	read_16s (fh, &GSPtr->velocity.error.height);
	read_16s (fh, &GSPtr->velocity.incr.width);
	read_16s (fh, &GSPtr->velocity.incr.height);

	read_32  (fh, &GSPtr->BattleGroupRef);

	read_a8  (fh, GSPtr->GameState, sizeof (GSPtr->GameState));
}

static BOOLEAN
LoadSisState (SIS_STATE *SSPtr, void *fp)
{
	if (
			read_32s (fp, &SSPtr->log_x) != 1 ||
			read_32s (fp, &SSPtr->log_y) != 1 ||
			read_32  (fp, &SSPtr->ResUnits) != 1 ||
			read_32  (fp, &SSPtr->FuelOnBoard) != 1 ||
			read_16  (fp, &SSPtr->CrewEnlisted) != 1 ||
			read_16  (fp, &SSPtr->TotalElementMass) != 1 ||
			read_16  (fp, &SSPtr->TotalBioMass) != 1 ||
			read_a8  (fp, SSPtr->ModuleSlots, NUM_MODULE_SLOTS) != 1 ||
			read_a8  (fp, SSPtr->DriveSlots, NUM_DRIVE_SLOTS) != 1 ||
			read_a8  (fp, SSPtr->JetSlots, NUM_JET_SLOTS) != 1 ||
			read_8   (fp, &SSPtr->NumLanders) != 1 ||
			read_a16 (fp, SSPtr->ElementAmounts, NUM_ELEMENT_CATEGORIES) != 1 ||

			read_str (fp, SSPtr->ShipName, SIS_NAME_SIZE) != 1 ||
			read_str (fp, SSPtr->CommanderName, SIS_NAME_SIZE) != 1 ||
			read_str (fp, SSPtr->PlanetName, SIS_NAME_SIZE) != 1
		)
		return FALSE;
	return TRUE;
}

static BOOLEAN
LoadSummary (SUMMARY_DESC *SummPtr, void *fp)
{
	SDWORD magic;
	DWORD nameSize = 0;
	if (!read_32s (fp, &magic))
		return FALSE;
	if (magic == SAVE_MAGIC)
	{
		if (read_32 (fp, &magic) != 1 || magic != SUMMARY_MAGIC)
			return FALSE;
		if (read_32 (fp, &magic) != 1 || magic < 161)
			return FALSE;
		nameSize = magic - 160;
	}
	else
	{
		return FALSE;
	}

	if (!LoadSisState (&SummPtr->SS, fp))
		return FALSE;

	if (		read_8  (fp, &SummPtr->Activity) != 1 ||
			read_8  (fp, &SummPtr->Flags) != 1 ||
			read_8  (fp, &SummPtr->day_index) != 1 ||
			read_8  (fp, &SummPtr->month_index) != 1 ||
			read_16 (fp, &SummPtr->year_index) != 1 ||
			read_8  (fp, &SummPtr->MCreditLo) != 1 ||
			read_8  (fp, &SummPtr->MCreditHi) != 1 ||
			read_8  (fp, &SummPtr->NumShips) != 1 ||
			read_8  (fp, &SummPtr->NumDevices) != 1 ||
			read_a8 (fp, SummPtr->ShipList, MAX_BUILT_SHIPS) != 1 ||
			read_a8 (fp, SummPtr->DeviceList, MAX_EXCLUSIVE_DEVICES) != 1
		)
		return FALSE;
	
	if (nameSize < SAVE_NAME_SIZE)
	{
		if (read_a8 (fp, SummPtr->SaveName, nameSize) != 1)
			return FALSE;
		SummPtr->SaveName[nameSize] = 0;
	}
	else
	{
		DWORD remaining = nameSize - SAVE_NAME_SIZE + 1;
		if (read_a8 (fp, SummPtr->SaveName, SAVE_NAME_SIZE-1) != 1)
			return FALSE;
		SummPtr->SaveName[SAVE_NAME_SIZE-1] = 0;
		if (skip_8 (fp, remaining) != 1)
			return FALSE;
	}
	return TRUE;
}

static void
LoadStarDesc (STAR_DESC *SDPtr, void *fh)
{
	read_16s(fh, &SDPtr->star_pt.x);
	read_16s(fh, &SDPtr->star_pt.y);
	read_8  (fh, &SDPtr->Type);
	read_8  (fh, &SDPtr->Index);
	read_8  (fh, &SDPtr->Prefix);
	read_8  (fh, &SDPtr->Postfix);
}

BOOLEAN
LoadGame (COUNT which_game, SUMMARY_DESC *SummPtr)
{
	uio_Stream *in_fp;
	char file[PATH_MAX];
	char buf[256];
	SUMMARY_DESC loc_sd;
	GAME_STATE_FILE *fp;
	COUNT num_links;
	STAR_DESC SD;
	ACTIVITY Activity;
	DWORD chunk, chunkSize;

	sprintf (file, "uqmsave.%02u", which_game);
	in_fp = res_OpenResFile (saveDir, file, "rb");
	if (!in_fp)
		return LoadLegacyGame (which_game, SummPtr);

	if (!LoadSummary (&loc_sd, in_fp))
	{
		res_CloseResFile (in_fp);
		return LoadLegacyGame (which_game, SummPtr);
	}

	if (!SummPtr)
	{
		SummPtr = &loc_sd;
	}
	else
	{	// only need summary for displaying to user
		memcpy (SummPtr, &loc_sd, sizeof (*SummPtr));
		res_CloseResFile (in_fp);
		return TRUE;
	}

	GlobData.SIS_state = SummPtr->SS;

	chunk = 0;
	while (chunk != OMNIBUS_MAGIC)
	{
		if (read_32(in_fp, &chunk) != 1)
		{
			res_CloseResFile (in_fp);
			return FALSE;
		}
		if (read_32(in_fp, &chunkSize) != 1)
		{
			res_CloseResFile (in_fp);
			return FALSE;
		}
		if (chunk == OMNIBUS_MAGIC)
			break;

		log_add (log_Debug, "Skipping chunk of tag %08X (size %u)", chunk, chunkSize);
		if (skip_8(in_fp, chunkSize) != 1)
			return FALSE;
	}

	ReinitQueue (&GLOBAL (GameClock.event_q));
	ReinitQueue (&GLOBAL (encounter_q));
	ReinitQueue (&GLOBAL (ip_group_q));
	ReinitQueue (&GLOBAL (npc_built_ship_q));
	ReinitQueue (&GLOBAL (built_ship_q));

	memset (&GLOBAL (GameState[0]), 0, sizeof (GLOBAL (GameState)));
	Activity = GLOBAL (CurrentActivity);
	LoadGameState (&GlobData.Game_state, in_fp);
	NextActivity = GLOBAL (CurrentActivity);
	GLOBAL (CurrentActivity) = Activity;

	LoadRaceQueue (in_fp, &GLOBAL (avail_race_q));
	// START_INTERPLANETARY is only set when saving from Homeworld
	//   encounter screen. When the game is loaded, the
	//   GenerateOrbitalFunction for the current star system will
	//   create the encounter anew and populate the npc queue.
	if (!(NextActivity & START_INTERPLANETARY))
	{
		if (NextActivity & START_ENCOUNTER)
			LoadShipQueue (in_fp, &GLOBAL (npc_built_ship_q));
		else if (LOBYTE (NextActivity) == IN_INTERPLANETARY)
			// XXX: Technically, this queue does not need to be
			//   saved/loaded at all. IP groups will be reloaded
			//   from group state files. But the original code did,
			//   and so will we until we can prove we do not need to.
			LoadGroupQueue (in_fp, &GLOBAL (ip_group_q));
		else
			// XXX: The empty queue read is only needed to maintain
			//   the savegame compatibility
			LoadEmptyQueue (in_fp);
	}
	LoadShipQueue (in_fp, &GLOBAL (built_ship_q));

	// Load the game events (compressed)
	read_16 (in_fp, &num_links);
	{
#ifdef DEBUG_LOAD
		log_add (log_Debug, "EVENTS:");
#endif /* DEBUG_LOAD */
		while (num_links--)
		{
			HEVENT hEvent;
			EVENT *EventPtr;

			hEvent = AllocEvent ();
			LockEvent (hEvent, &EventPtr);

			LoadEvent (EventPtr, in_fp);

#ifdef DEBUG_LOAD
		log_add (log_Debug, "\t%u/%u/%u -- %u",
				EventPtr->month_index,
				EventPtr->day_index,
				EventPtr->year_index,
				EventPtr->func_index);
#endif /* DEBUG_LOAD */
			UnlockEvent (hEvent);
			PutEvent (hEvent);
		}
	}

	// Load the encounters (black globes in HS/QS (compressed))
	read_16 (in_fp, &num_links);
	{
		while (num_links--)
		{
			HENCOUNTER hEncounter;
			ENCOUNTER *EncounterPtr;

			hEncounter = AllocEncounter ();
			LockEncounter (hEncounter, &EncounterPtr);

			LoadEncounter (EncounterPtr, in_fp);

			UnlockEncounter (hEncounter);
			PutEncounter (hEncounter);
		}
	}

	// Copy the star info file from the compressed stream
	fp = OpenStateFile (STARINFO_FILE, "wb");
	if (fp)
	{
		DWORD flen;

		read_32 (in_fp, &flen);
		while (flen)
		{
			COUNT num_bytes;

			num_bytes = flen >= sizeof (buf) ? sizeof (buf) : (COUNT)flen;
			read_a8 (in_fp, buf, num_bytes);
			WriteStateFile (buf, num_bytes, 1, fp);

			flen -= num_bytes;
		}
		CloseStateFile (fp);
	}

	// Copy the defined groupinfo file from the compressed stream
	fp = OpenStateFile (DEFGRPINFO_FILE, "wb");
	if (fp)
	{
		DWORD flen;

		read_32 (in_fp, &flen);
		while (flen)
		{
			COUNT num_bytes;

			num_bytes = flen >= sizeof (buf) ? sizeof (buf) : (COUNT)flen;
			read_a8 (in_fp, buf, num_bytes);
			WriteStateFile (buf, num_bytes, 1, fp);

			flen -= num_bytes;
		}
		CloseStateFile (fp);
	}

	// Copy the random groupinfo file from the compressed stream
	fp = OpenStateFile (RANDGRPINFO_FILE, "wb");
	if (fp)
	{
		DWORD flen;

		read_32 (in_fp, &flen);
		while (flen)
		{
			COUNT num_bytes;

			num_bytes = flen >= sizeof (buf) ? sizeof (buf) : (COUNT)flen;
			read_a8 (in_fp, buf, num_bytes);
			WriteStateFile (buf, num_bytes, 1, fp);

			flen -= num_bytes;
		}
		CloseStateFile (fp);
	}

	LoadStarDesc (&SD, in_fp);

	res_CloseResFile (in_fp);

	EncounterGroup = 0;
	EncounterRace = -1;

	ReinitQueue (&race_q[0]);
	ReinitQueue (&race_q[1]);
	CurStarDescPtr = FindStar (NULL, &SD.star_pt, 0, 0);
	if (!(NextActivity & START_ENCOUNTER)
			&& LOBYTE (NextActivity) == IN_INTERPLANETARY)
		NextActivity |= START_INTERPLANETARY;

	return TRUE;
}
