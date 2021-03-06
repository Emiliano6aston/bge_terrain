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

	float offset;


	int noisebasis;
	int musgravetype;

	float noiseheight;
	float resolution;
	int octaves;
	float lacunarity;
	float gain;
	float musgraveoffset;
	float H;

	int pad;

	float clampstart;
	float clampend;

	int pad1;

	struct Image *image;
	float imageheight;

	int pad2;

	struct Group *groupobject;
	float objectinfluence;

	int uvchannel;
	float color;
	float colordividor;

	int pad3;

	int flag;
} TerrainZone;

typedef struct Terrain {
	ID id;
	struct Material *material;

	int maxlevel;
	int width;
	int vertexsubdivision;

	float cameradistance;
	float objectdistance;
	float chunksize;
	float marginfactor;

	int pad;

	int debugmode;
	int debugtimeframe;

	int flag;
	int cacherefreshtime;

	int minphysicslevel;
	int active_zoneindex;

	ListBase zones;
} Terrain;

#define TERRAIN_USE_CACHE	(1 << 0)

#define TERRAIN_ZONE_MESH						(1 << 0)
#define TERRAIN_ZONE_PERLIN_NOISE				(1 << 1)
#define TERRAIN_ZONE_IMAGE						(1 << 2)
#define TERRAIN_ZONE_CLAMP						(1 << 3)
#define TERRAIN_ZONE_CLAMP_MESH					(1 << 4)
#define TERRAIN_ZONE_MESH_VERTEX_COLOR_INTERP	(1 << 5)
#define TERRAIN_ZONE_USE_UV_TEXTURE_COLOR		(1 << 6)
#define TERRAIN_ZONE_USE_HEIGHT_COLOR			(1 << 7)
#define TERRAIN_ZONE_DIVIDE_COLOR				(1 << 8) /* Used only with TERRAIN_ZONE_USE_HEIGHT_COLOR */
#define TERRAIN_ZONE_ACTIVE						(1 << 9)
#define TERRAIN_ZONE_USE_OBJECT					(1 << 10)
#define TERRAIN_ZONE_CLAMP_OBJECT				(1 << 11)

#define DEBUG_DRAW_BOXES	(1 << 0)
#define DEBUG_DRAW_LINES	(1 << 1)
#define DEBUG_DRAW_CENTERS	(1 << 2)
#define DEBUG_WARNINGS		(1 << 3)
#define DEBUG_ERRORS		(1 << 4)
#define DEBUG_TIME			(1 << 5)

#endif /*__DNA_TERRAIN_TYPES_H__*/

