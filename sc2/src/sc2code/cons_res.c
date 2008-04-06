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

#include "element.h"
#include "resinst.h"
#include "nameref.h"
#include "setup.h"

FRAME planet[NUM_VIEWS];

void
load_gravity_well (BYTE selector)
{
	COUNT i;
	RES_TYPE rt;
	RES_INSTANCE ri;
	RES_PACKAGE rp;

	if (selector == NUMBER_OF_PLANET_TYPES)
	{
		planet[0] = CaptureDrawable (
				LoadGraphic (SAMATRA_BIG_MASK_PMAP_ANIM)
				);
		planet[1] = planet[2] = 0;
	}
	else
	{
		if (selector & PLANET_SHIELDED)
		{
			rt = GET_TYPE (SHIELDED_BIG_MASK_PMAP_ANIM);
			ri = GET_INSTANCE (SHIELDED_BIG_MASK_PMAP_ANIM);
			rp = GET_PACKAGE (SHIELDED_BIG_MASK_PMAP_ANIM);
		}
		else
		{
			rt = GET_TYPE (PLANET00_BIG_MASK_PMAP_ANIM);
			ri = GET_INSTANCE (PLANET00_BIG_MASK_PMAP_ANIM)
					+ (selector * NUM_VIEWS);
			rp = GET_PACKAGE (PLANET00_BIG_MASK_PMAP_ANIM)
					+ selector;
		}

		for (i = 0; i < NUM_VIEWS; ++i, ++ri)
		{
			planet[i] = CaptureDrawable (
					LoadGraphic (MAKE_RESOURCE (rp, rt, ri))
					);
		}
	}

}

void
free_gravity_well (void)
{
	COUNT i;

	for (i = 0; i < NUM_VIEWS; ++i)
	{
		DestroyDrawable (ReleaseDrawable (planet[i]));
		planet[i] = 0;
	}
}

FRAME
load_life_form (BYTE selector)
{
	return CaptureDrawable (LoadGraphic (MAKE_RESOURCE (
			GET_PACKAGE (LIFE00_MASK_PMAP_ANIM) + selector,
			GFXRES,
			GET_INSTANCE (LIFE00_MASK_PMAP_ANIM) + selector)));
}

MUSIC_REF
load_orbit_theme (BYTE selector)
{
	RES_PACKAGE rp = GET_PACKAGE (ORBIT1_MUSIC);
	RES_INSTANCE ri = GET_INSTANCE (ORBIT1_MUSIC) + selector;
	RES_TYPE rt = GET_TYPE (ORBIT1_MUSIC);
	
	return LoadMusic (MAKE_RESOURCE (rp, rt, ri));
}