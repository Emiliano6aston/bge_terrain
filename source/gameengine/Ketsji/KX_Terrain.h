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

class KX_Terrain
{
private:
	bool m_construct;
	KX_ChunkNode** m_nodeTree;
	std::vector<KX_Chunk*> m_posToChunk;
	std::vector<KX_Chunk*> m_euthanasyChunkList;
	unsigned short m_maxSubDivision;
	unsigned short m_width;
	float m_maxDistance2;
	float m_chunkSize;
	float m_maxHeight;

	RAS_MaterialBucket* m_bucket;

public:
	KX_Terrain(unsigned short maxSubDivisions, unsigned short width, float maxDistance, float chunkSize, float maxheight);
	~KX_Terrain();

	void Construct();
	void Destruct();

	void CalculateVisibleChunks(KX_Camera* culledcam);
	void UpdateChunksMeshes();
	void RenderChunksMeshes(const MT_Transform& cameratrans, RAS_IRasterizer* rasty);

	// le niveau de subdivision maximal
	inline unsigned short GetMaxLevel() const { return m_maxSubDivision * m_maxSubDivision * 2; };
	// la distance maximal
	inline float GetMaxDistance2() const { return m_maxDistance2; };
	inline float GetChunkSize() const { return m_chunkSize; };
	inline float GetMaxHeight() const { return m_maxHeight; };
	// le nombre de subdivision par rapport à une distance
	unsigned short GetSubdivision(float distance) const;

	KX_ChunkNode* GetNodeRelativePosition(int x, int y);

	KX_ChunkNode** NewNodeList(int x, int y, unsigned short level);
	KX_Chunk* AddChunk(KX_ChunkNode* node);
	void RemoveChunk(KX_ChunkNode::Point2D pos);
	void ScheduleEuthanasyChunks();
};

#endif //__KX_TERRAIN_H__