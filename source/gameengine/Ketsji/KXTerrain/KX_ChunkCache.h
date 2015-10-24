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
 * Contributor(s): Porteries Tristan, Gros Alexis. For the 
 * Uchronia project (2015-16).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __KX_CHUNK_CACHE_H__
#define __KX_CHUNK_CACHE_H__

#include "KX_Chunk.h" // for VERTEX_COUNT
#include "KX_ChunkNode.h" // for Point2D

class VertexZoneInfo;
class KX_Terrain;

class KX_ChunkCache
{
private:
	/*
	 * 5 vertices on x with an internal of 1 on x and 2 on y.
	 * 3 vertices on y with an internal of 2 on y and 2 on x.
	 *
	 * () = optinal, only if a cached chunk is totally
	 * at the bottom and/or to the right.
	 *
	 *         (y0.2)         (y1.2)
	 *           |               |
	 *           |               |
	 * x1.0----x1.1----x1.2----x1.3----(x1.4)
	 *           |               |
	 *           |               |
	 *         y0.1            y1.1
	 *           |               |
	 *           |               |
	 * x0.0----x1.1----x0.2----x0.3----(x0.4)
	 *           |               |
	 *           |               |
	 *         y0.0            y1.0
	 */

	VertexZoneInfo **m_columnsX[2];
	VertexZoneInfo **m_columnsY[2];

	const unsigned short m_level;
	/// Chunk size = delta between vertices.
	const unsigned short m_size;

	KX_ChunkNode::Point2D m_pos;

	/** The numbe of vertices used on X, between
	 * VERTEX_COUNT and (VERTEX_COUNT - 1)
	 */
	const bool m_allVertexesX;

	/** The numbe of vertices used on Y, between
	 * VERTEX_COUNT_INTERN and (VERTEX_COUNT_INTERN - 1)
	 */
	const bool m_allVertexesY;

	unsigned int m_accesCount;

	KX_Terrain *m_terrain;

	/// All 4 cache subchunks.
	KX_ChunkCache **m_subChunkCache;

	void ConstructSubChunkCache();
	void DestructSubChunkCache();

public:
	KX_ChunkCache(unsigned short level, int x, int y, unsigned short size, 
				  bool allvertexesx, bool allvertexesy, KX_Terrain *terrain);
	virtual ~KX_ChunkCache();

	/** Search for a vertex = to current position.
	 * Case :
	 *     - If the vertex is in the chunk:
	 *         - return it (if it exists).
	 *         - create and return it (if it does not exist).
	 *     - If the vertex is outside of the chunk:
	 *         - Subdivide the chunk as 4 subchunks and recall function.
	 */
	VertexZoneInfo *GetVertexZoneInfo(int x, int y);

	void Refresh();
};

class KX_ChunkRootCache
{
private:
	VertexZoneInfo *m_columns[VERTEX_COUNT][VERTEX_COUNT];

	const unsigned short m_size;

	KX_Terrain *m_terrain;

	/// All 4 cache subchunks.
	KX_ChunkCache *m_subChunkCache[4];

public:
	KX_ChunkRootCache(unsigned short size, KX_Terrain *terrain);
	virtual ~KX_ChunkRootCache();

	void Construct();
	void Destruct();

	VertexZoneInfo *GetVertexZoneInfo(int x, int y);

	void Refresh();
};
#endif // __KX_CHUNK_CACHE_H__
