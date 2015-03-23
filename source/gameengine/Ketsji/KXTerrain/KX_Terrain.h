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

#ifndef __KX_TERRAIN_H__
#define __KX_TERRAIN_H__

#include <map>
#include <vector>
#include "MT_Transform.h"
#include "KX_ChunkNode.h" // for Point2D

class RAS_IRasterizer;
class RAS_MaterialBucket;
class CListValue;
class KX_GameObject;
class KX_TerrainZoneInfo;
class KX_TerrainZoneMesh;

class KX_Terrain
{
private:
	RAS_MaterialBucket *m_bucket;
	KX_GameObject	   *m_templateObject;
	unsigned short		m_maxChunkLevel;
	unsigned short		m_vertexSubdivision;
	unsigned short		m_width;
	float				m_maxDistance2;
	float				m_chunkSize;
	float				m_maxHeight;
	float				m_noiseSize;

	

	bool					m_construct;
	KX_ChunkNode		  **m_nodeTree;
	std::vector<KX_Chunk *>	m_chunkList;
	std::vector<KX_Chunk *>	m_euthanasyChunkList;

	std::vector<KX_TerrainZoneInfo *>	m_zoneInfoList;
	std::vector<KX_TerrainZoneMesh *>	m_zoneMeshList;

public:
	KX_Terrain(RAS_MaterialBucket *bucket,
			   KX_GameObject *templateObject,
			   unsigned short maxLevel,
			   unsigned short vertexSubdivision,
			   unsigned short width,
			   float maxDistance,
			   float chunkSize,
			   float maxheight,
			   float noiseSize);
	~KX_Terrain();

	void Construct();
	void Destruct();

	void CalculateVisibleChunks(KX_Camera *culledcam);
	void UpdateChunksMeshes();
	void RenderChunksMeshes(const MT_Transform &cameratrans, RAS_IRasterizer *rasty);

	/// Le niveau de subdivision maximal
	inline unsigned short GetMaxLevel() const {
		return (m_maxChunkLevel * m_maxChunkLevel * 2);
	}
	/// La distance maximal pour avoir un niveau de subdivision superieur à 1
	inline float GetMaxDistance2() const {
		return m_maxDistance2;
	}
	/// La taille de tous les chunks
	inline float GetChunkSize() const {
		return m_chunkSize;
	}
	/// La hauteur maximale du mesh d'un chunk
	inline float GetMaxHeight() const { 
		return m_maxHeight;
	}
	/// La hauteur maximale du mesh d'un chunk
	inline float GetNoiseSize() const { 
		return m_noiseSize;
	}
	// le nombre de subdivision par rapport à une distance
	unsigned short GetSubdivision(float distance) const;

	KX_ChunkNode *GetNodeRelativePosition(const KX_ChunkNode::Point2D &pos);

	KX_ChunkNode **NewNodeList(int x, int y, unsigned short level);
	KX_Chunk *AddChunk(KX_ChunkNode *node);
	void RemoveChunk(KX_Chunk *chunk);
	void ScheduleEuthanasyChunks();

	void AddTerrainZoneInfo(KX_TerrainZoneInfo *zoneInfo);
	void AddTerrainZoneMesh(KX_TerrainZoneMesh *zoneMesh);
};

#endif //__KX_TERRAIN_H__