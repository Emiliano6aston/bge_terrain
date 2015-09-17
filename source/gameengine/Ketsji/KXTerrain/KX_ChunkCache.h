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
	 * 5 vertices en x a un intervale de 1 en x et 2 en y.
	 * 3 vertices en y a un intervale de 2 en y et 2 en x.
	 * 
	 * () = optionelle, seulement dans le cas d'un chunk de cache complètement
	 * en bas et/ou a droite.
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
	/// La taille du chunk = décalage entre les vertices.
	const unsigned short m_size;

	KX_ChunkNode::Point2D m_pos;

	/** Le nombre de vertices utilisés en X, entre
	 * VERTEX_COUNT et (VERTEX_COUNT - 1)
	 */
	const bool m_allVertexesX;

	/** Le nombre de vertices utilisés en Y, entre
	 * VERTEX_COUNT_INTERN et (VERTEX_COUNT_INTERN - 1)
	 */
	const bool m_allVertexesY;

	unsigned int m_accesCount;

	KX_Terrain *m_terrain;

	/// Les 4 sous chunks de cache.
	KX_ChunkCache **m_subChunkCache;

	void ConstructSubChunkCache();
	void DestructSubChunkCache();

public:
	KX_ChunkCache(unsigned short level, int x, int y, unsigned short size, 
				  bool allvertexesx, bool allvertexesy, KX_Terrain *terrain);
	virtual ~KX_ChunkCache();

	/** Cherche un vertice coorespondant a cette position.
	 * Cas :
	 *     - Si le vertice est dans ce chunk :
	 *         - Si le vertice existe on le renvoie.
	 *         - Si le vertice n'existe pas on le crée et on le renvoie.
	 *     - Si le vertice n'est pas compris dans ce chunk :
	 *         - On subdivise le chunk en 4 sous chunks et on rappelle cette fonction.
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

	/// Les 4 sous chunks de cache.
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
