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
#ifndef __BKE_TERRAIN_H__
#define __BKE_TERRAIN_H__

/** \file BKE_terrain.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

struct Main;
struct Terrain;

void BKE_terrain_free(struct Terrain *sc);
void BKE_terrain_free_ex(struct Terrain *sc, bool do_id_user);
struct Terrain *add_terrain(struct Main *bmian, const char *name);
struct Terrain *BKE_terrain_copy(struct Terrain *terrain);
void BKE_terrain_make_local(struct Terrain *terrain);
bool BKE_terrain_zone_remove(struct Terrain *terrain);
void BKE_terrain_zone_add(struct Terrain *terrain);
void BKE_terrain_zone_move_up(struct Terrain *terrain);
void BKE_terrain_zone_move_down(struct Terrain *terrain);

#endif

