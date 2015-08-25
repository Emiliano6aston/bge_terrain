/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/terrain.c
 *  \ingroup bke
 */


#include <string.h>
#include <math.h>
#include "MEM_guardedalloc.h"

#include "DNA_terrain_types.h"
#include "DNA_scene_types.h"
#include "DNA_material_types.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_blenlib.h"
#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_terrain.h"

void BKE_terrain_free_ex(Terrain *terrain, bool do_id_user)
{
	if (terrain->material) {
		if (do_id_user)
			terrain->material->id.us--;
	}
	BLI_freelistN(&terrain->zones);
}

void BKE_terrain_free(Terrain *terrain)
{
	BKE_terrain_free_ex(terrain, true);
}

Terrain *add_terrain(Main *bmain, const char *name)
{
	Terrain *terrain;

	terrain = BKE_libblock_alloc(bmain, ID_TER, name);
	terrain->material = NULL;
	terrain->maxlevel = 6;
	terrain->width = 64;
	terrain->vertexsubdivision = 4;
	terrain->cameradistance = 500.0;
	terrain->objectdistance = 0.0;
	terrain->chunksize = 30.0;
	terrain->minphysicslevel = 0;
	terrain->active_zoneindex = 0;

	return terrain;
}

Terrain *BKE_terrain_copy(Terrain *terrain)
{
	Terrain *terrain_new;
	terrain_new = BKE_libblock_copy(&terrain->id);
	terrain_new->material = terrain->material;

	if (terrain->material) {
		id_us_plus((ID *)terrain_new->material);
	}

	if (terrain->id.lib) {
		BKE_id_lib_local_paths(G.main, terrain->id.lib, &terrain_new->id);
	}

	return terrain_new;
}

void BKE_terrain_make_local(Terrain *terrain)
{
	Main *bmain = G.main;
	Scene *sce;
	bool is_local = false, is_lib = false;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	if (terrain->id.lib == NULL) {
		return;
	}
	if (terrain->id.us == 1) {
		id_clear_lib_data(bmain, &terrain->id);
		return;
	}

	for (sce = bmain->scene.first; sce && ELEM(false, is_lib, is_local); sce = sce->id.next) {
		if (sce->terrain == terrain) {
			if (sce->id.lib) is_lib = true;
			else is_local = true;
		}
	}

	if (is_local && is_lib == false) {
		id_clear_lib_data(bmain, &terrain->id);
	}
	else if (is_local && is_lib) {
		Terrain *terrain_new = BKE_terrain_copy(terrain);
		terrain_new->id.us = 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, terrain->id.lib, &terrain_new->id);

		for (sce = bmain->scene.first; sce; sce = sce->id.next) {
			if (sce->terrain == terrain) {
				if (sce->id.lib == NULL) {
					sce->terrain = terrain_new;
					terrain_new->id.us++;
					terrain->id.us--;
				}
			}
		}
	}
}

bool BKE_terrain_zone_remove(Terrain *terrain)
{
	TerrainZone *zone;

	// no zones
	if (terrain->active_zoneindex == -1)
		return false;

	zone = BLI_findlink(&terrain->zones, terrain->active_zoneindex);

	BLI_remlink(&terrain->zones, zone);
	MEM_freeN(zone);

	terrain->active_zoneindex = BLI_listbase_count(&terrain->zones) - 1;
	return true;
}

void BKE_terrain_zone_add(Terrain *terrain)
{
	TerrainZone *zone = MEM_callocN(sizeof(TerrainZone), "Terrain Zone");

	BLI_strncpy(zone->name, DATA_("Terrain Zone"), sizeof(zone->name));


	zone->mesh = NULL;
	zone->noiseheight = 10.0;
	zone->offset = 0.0;
	zone->resolution = 100.0;
	zone->clampstart = 0.0;
	zone->clampend = 0.0;
	zone->image = NULL;
	zone->imageheight = 0.0f;
	zone->uvchannel = 2;

	BLI_addtail(&terrain->zones, zone);

	BLI_uniquename(&terrain->zones, zone, DATA_("Terrain Zone"), '.', offsetof(TerrainZone, name), sizeof(zone->name));
}

void BKE_terrain_zone_move_up(Terrain *terrain)
{
	TerrainZone *zone;
	TerrainZone *prevzone;

	zone = BLI_findlink(&terrain->zones, terrain->active_zoneindex);
	prevzone = zone->prev;

	if (zone && prevzone) {
		BLI_listbase_swaplinks(&terrain->zones, zone, prevzone);
		--terrain->active_zoneindex;
	}
}

void BKE_terrain_zone_move_down(Terrain *terrain)
{
	TerrainZone *zone;
	TerrainZone *nextzone;

	zone = BLI_findlink(&terrain->zones, terrain->active_zoneindex);
	nextzone = zone->next;

	if (zone && nextzone) {
		BLI_listbase_swaplinks(&terrain->zones, zone, nextzone);
		++terrain->active_zoneindex;
	}
}
