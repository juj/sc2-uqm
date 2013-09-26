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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _SAVE_H
#define _SAVE_H

#include "sis.h" // SUMMARY_DESC includes SIS_STATE in it
#include "globdata.h"
#include "libs/compiler.h"

#if defined(__cplusplus)
extern "C" {
#endif

// XXX: Theoretically, a player can have 17 devices on board without
//   cheating. We only provide
//   room for 16 below, which is not really a problem since this
//   is only used for displaying savegame summaries. There is also
//   room for only 16 devices on screen.
#define MAX_EXCLUSIVE_DEVICES 16
#define SAVE_MAGIC 0x01534d55
#define SAVE_NAME_SIZE 24

typedef struct
{
	SIS_STATE SS;
	BYTE Activity;
	BYTE Flags;
	BYTE day_index, month_index;
	COUNT year_index;
	BYTE MCreditLo, MCreditHi;
	BYTE NumShips, NumDevices;
	BYTE ShipList[MAX_BUILT_SHIPS];
	BYTE DeviceList[MAX_EXCLUSIVE_DEVICES];
	UNICODE SaveName[SAVE_NAME_SIZE];
} SUMMARY_DESC;

extern ACTIVITY NextActivity;

extern BOOLEAN LoadGame (COUNT which_game, SUMMARY_DESC *summary_desc);

extern void SaveProblem (void);
extern BOOLEAN SaveGame (COUNT which_game, SUMMARY_DESC *summary_desc, const char *name);

#if defined(__cplusplus)
}
#endif

#endif  /* _SAVE_H */

