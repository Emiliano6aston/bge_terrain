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
 * Contributor(s): Porteries Tristan, Gros Alexis. For the 
 * Uchronia project (2015-16).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KXTerrain/KX_ChunkCache.cpp
 *  \ingroup ketsji
 */

#include "KX_ChunkCache.h"
#include "KX_Terrain.h"
#include "KX_TerrainZone.h"

KX_ChunkCache::KX_ChunkCache(unsigned short level, int x, int y, unsigned short size,
							 bool allvertexesx, bool allvertexesy, KX_Terrain *terrain)
	:m_level(level),
	m_size(size),
	m_pos(KX_ChunkNode::Point2D(x, y)),
	m_allVertexesX(allvertexesx),
	m_allVertexesY(allvertexesy),
	m_accesCount(0),
	m_terrain(terrain),
	m_subChunkCache(NULL)
{
	unsigned short columnXSize = (m_allVertexesX ? VERTEX_COUNT : POLY_COUNT);
	m_columnsX[0] = (VertexZoneInfo **)malloc(sizeof(VertexZoneInfo *) * columnXSize);
	m_columnsX[1] = (VertexZoneInfo **)malloc(sizeof(VertexZoneInfo *) * columnXSize);

	unsigned short columnYSize = (m_allVertexesY ? VERTEX_COUNT_INTERN : POLY_COUNT_INTERN);
	m_columnsY[0] = (VertexZoneInfo **)malloc(sizeof(VertexZoneInfo *) * columnYSize);
	m_columnsY[1] = (VertexZoneInfo **)malloc(sizeof(VertexZoneInfo *) * columnYSize);

	for (unsigned short vertexIndex = 0; vertexIndex < columnXSize; ++vertexIndex) {
		m_columnsX[0][vertexIndex] = NULL;
		m_columnsX[1][vertexIndex] = NULL;
	}
	for (unsigned short vertexIndex = 0; vertexIndex < columnYSize; ++vertexIndex) {
		m_columnsY[0][vertexIndex] = NULL;
		m_columnsY[1][vertexIndex] = NULL;
	}
}

KX_ChunkCache::~KX_ChunkCache()
{
	unsigned short columnXSize = (m_allVertexesX ? VERTEX_COUNT : POLY_COUNT);
	for (unsigned short vertexIndex = 0; vertexIndex < columnXSize; ++vertexIndex) {
		if (m_columnsX[0][vertexIndex]) {
			m_columnsX[0][vertexIndex]->Release();
		}
		if (m_columnsX[1][vertexIndex]) {
			m_columnsX[1][vertexIndex]->Release();
		}
	}

	unsigned short columnYSize = (m_allVertexesY ? VERTEX_COUNT_INTERN : POLY_COUNT_INTERN);
	for (unsigned short vertexIndex = 0; vertexIndex < columnYSize; ++vertexIndex) {
		if (m_columnsY[0][vertexIndex]) {
			m_columnsY[0][vertexIndex]->Release();
		}
		if (m_columnsY[1][vertexIndex]) {
			m_columnsY[1][vertexIndex]->Release();
		}
	}

	free(m_columnsX[0]);
	free(m_columnsX[1]);
	free(m_columnsY[0]);
	free(m_columnsY[1]);

	DestructSubChunkCache();
}

void KX_ChunkCache::ConstructSubChunkCache()
{
	if (!m_subChunkCache) {
		m_subChunkCache = (KX_ChunkCache **)malloc(sizeof(KX_ChunkCache *) * 4);

		unsigned short newsize = m_size / 2;
		const unsigned short chunkinterval = m_size / 4;
		unsigned short newlevel = m_level + 1;

		m_subChunkCache[0] = new KX_ChunkCache(newlevel, m_pos.x - chunkinterval, m_pos.y - chunkinterval,
											   newsize, false, false, m_terrain);
		m_subChunkCache[1] = new KX_ChunkCache(newlevel, m_pos.x + chunkinterval, m_pos.y - chunkinterval,
											   newsize, m_allVertexesX, false, m_terrain);
		m_subChunkCache[2] = new KX_ChunkCache(newlevel, m_pos.x - chunkinterval, m_pos.y + chunkinterval,
											   newsize, false, m_allVertexesY, m_terrain);
		m_subChunkCache[3] = new KX_ChunkCache(newlevel, m_pos.x + chunkinterval, m_pos.y + chunkinterval,
											   newsize, m_allVertexesX, m_allVertexesY, m_terrain);
	}
}

void KX_ChunkCache::DestructSubChunkCache()
{
	if (m_subChunkCache) {
		delete m_subChunkCache[0];
		delete m_subChunkCache[1];
		delete m_subChunkCache[2];
		delete m_subChunkCache[3];
		free(m_subChunkCache);
		m_subChunkCache = NULL;
	}
}

VertexZoneInfo *KX_ChunkCache::GetVertexZoneInfo(int x, int y)
{
	const unsigned short interval = m_size / POLY_COUNT;
	const unsigned short halfsize = m_size / 2;
	const short bottomx = m_pos.x - halfsize;
	const short bottomy = m_pos.y - halfsize;
	const float relx = ((float)(x - bottomx)) / interval;
	const float rely = ((float)(y - bottomy)) / interval;

	VertexZoneInfo *vertexInfo = NULL;

	// Helps to know if the node is frequently used.
	++m_accesCount;

	// The vertex is outside of chunk.
	if (relx < 0.0f || rely < 0.0f ||
		relx > 4.0f || rely > 4.0f ||
		(!m_allVertexesX && relx == 4.0f) ||
		(!m_allVertexesY && rely == 4.0f))
	{
		return NULL;
	}

	const bool alignedX = fmod(relx, 1.0f) == 0.0f;
	const bool alignedY = fmod(rely, 1.0f) == 0.0f;
	const bool pairX = fmod(relx, 2.0f) == 0.0f;
	const bool pairY = fmod(rely, 2.0f) == 0.0f;

	// The vertex is between two columns, we then have to subdivide the node.
	if (!alignedX || !alignedY) {
		ConstructSubChunkCache();
		for (unsigned short i = 0; i < 4; ++i) {
			vertexInfo = m_subChunkCache[i]->GetVertexZoneInfo(x, y);
			if (vertexInfo) {
				return vertexInfo;
			}
		}
		// Really impossible.
		return NULL;
	}

	/* The point is aligned to the columns by its X axis, then rely = 1 or rely = 3.
	 * It is only needed to find the correct column in X
	 * and to access the vertex with index as relx.
	 */
	if (!pairY) {
		// rely / 2 gives 0 or 1 since rely = 1 or = 3.
		const unsigned short columnIndex = (int)(rely / 2.0f);
		const unsigned short vertexIndex = (int)(relx);

		vertexInfo = m_columnsX[columnIndex][vertexIndex];
		// The point is not yet created.
		if (!vertexInfo) {
			m_columnsX[columnIndex][vertexIndex] = vertexInfo = m_terrain->NewVertexInfo(x, y);
		}
	}
	/* The point is only aligned to the columns by its X axis, then
	 * relx = 1 or relx = 3 but rely != 1 or rely != 3.
	 * We only need need to pick the correct vertex in
	 * the y column with an index of rely / 2.
	 */
	else if (!pairX) {
		const unsigned short columnIndex = (int)(relx / 2.0f);
		const unsigned short vertexIndex = (int)(rely / 2.0f);

		vertexInfo = m_columnsY[columnIndex][vertexIndex];
		// The point is not yet created.
		if (!vertexInfo) {
			m_columnsY[columnIndex][vertexIndex] = vertexInfo = m_terrain->NewVertexInfo(x, y);
		}
	}

	return vertexInfo;
}

void KX_ChunkCache::Refresh()
{
	if (m_accesCount == 0) {
		DestructSubChunkCache();
	}

	m_accesCount = 0;
}

KX_ChunkRootCache::KX_ChunkRootCache(unsigned short size, KX_Terrain *terrain)
	:m_size(size),
	m_terrain(terrain)
{
}

KX_ChunkRootCache::~KX_ChunkRootCache()
{
}

void KX_ChunkRootCache::Construct()
{
	const unsigned short interval = m_size / POLY_COUNT;
	const unsigned short halfsize = m_size / 2;

	// The tree cache automatically generates the points.
	for (unsigned short columnIndex = 0; columnIndex < VERTEX_COUNT; ++columnIndex) {
		for (unsigned short vertexIndex = 0; vertexIndex < VERTEX_COUNT; ++vertexIndex) {
			const int x = columnIndex * interval - halfsize;
			const int y = vertexIndex * interval - halfsize;
			m_columns[columnIndex][vertexIndex] = m_terrain->NewVertexInfo(x, y);
		}
	}

	const unsigned short newsize = m_size / 2;
	const unsigned short newlevel = 1;
	const unsigned short chunkinterval = m_size / 4;

	m_subChunkCache[0] = new KX_ChunkCache(newlevel, -chunkinterval, -chunkinterval, newsize, false, false, m_terrain);
	m_subChunkCache[1] = new KX_ChunkCache(newlevel,  chunkinterval, -chunkinterval, newsize, true, false, m_terrain);
	m_subChunkCache[2] = new KX_ChunkCache(newlevel, -chunkinterval,  chunkinterval, newsize, false, true, m_terrain);
	m_subChunkCache[3] = new KX_ChunkCache(newlevel,  chunkinterval,  chunkinterval, newsize, true, true, m_terrain);
}

void KX_ChunkRootCache::Destruct()
{
	for (unsigned short columnIndex = 0; columnIndex < VERTEX_COUNT; ++columnIndex) {
		for (unsigned short vertexIndex = 0; vertexIndex < VERTEX_COUNT; ++vertexIndex) {
			if (m_columns[columnIndex][vertexIndex]) {
				m_columns[columnIndex][vertexIndex]->Release();
			}
		}
	}

	for (unsigned short i = 0; i < 4; ++i) {
		delete m_subChunkCache[i];
	}
}

VertexZoneInfo *KX_ChunkRootCache::GetVertexZoneInfo(int x, int y)
{
	VertexZoneInfo *vertexInfo = NULL;

	const unsigned short interval = m_size / POLY_COUNT;
	const unsigned short halfsize = m_size / 2;
	/* If the vertex is on the grid, relx and rely will be:
	 * 0.0, 1.0, 2.0, 3.0
	 * Else the vertex is in a subnode grid.
	 */
	const float relx = ((float)(x + halfsize)) / interval;
	const float rely = ((float)(y + halfsize)) / interval;

	const bool alignedX = fmod(relx, 1.0f) == 0.0f;
	const bool alignedY = fmod(rely, 1.0f) == 0.0f;

	/* If vertex is in between the columns on that chunk,
	 * we then need to subdivide the node.
	 */
	if (!alignedX || !alignedY) {
		for (unsigned short i = 0; i < 4; ++i) {
			vertexInfo = m_subChunkCache[i]->GetVertexZoneInfo(x, y);
			if (vertexInfo) {
				return vertexInfo;
			}
		}
		// Really impossible.
		return NULL;
	}

	vertexInfo = m_columns[(int)relx][(int)rely];

	return vertexInfo;
}

void KX_ChunkRootCache::Refresh()
{
	for (unsigned short i = 0; i < 4; ++i) {
		m_subChunkCache[i]->Refresh();
	}
}