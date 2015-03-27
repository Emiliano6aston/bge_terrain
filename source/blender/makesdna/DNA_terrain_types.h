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

/** \file DNA_terrain_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_TERRAIN_TYPES_H__
#define __DNA_TERRAIN_TYPES_H__

#include "DNA_ID.h"
#include "DNA_listBase.h"

typedef struct TerrainZone {
	struct TerrainZone *next, *prev;
	char name[64];

	struct Mesh *mesh;
	float height;
	float offset;
	float resolution;
	int pad;
} TerrainZone;

typedef struct Terrain {
	ID id;
	struct Material *material;

	int maxlevel;
	int vertexsubdivision;
	int width;
	float distance;
	float chunksize;
	float height;
	float noisesize;
	int active_zoneindex;
	ListBase zones;
} Terrain;

#endif

