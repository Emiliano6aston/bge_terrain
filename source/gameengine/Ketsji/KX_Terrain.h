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

class KX_Chunk;
class KX_Camera;

typedef std::pair<int, int> vector2DInt;
typedef std::map<vector2DInt, KX_Chunk*>::iterator chunkMapIt;

class KX_Terrain
{
private:
	std::map<vector2DInt, KX_Chunk*> m_positionToChunk;
	unsigned short m_maxSubDivision;
	unsigned short m_size;
	float m_maxDistance;
	float m_chunkSize;
	float m_maxHeight;

public:
	KX_Terrain(unsigned short maxSubDivisions, unsigned short size, float maxDistance, float chunkSize, float maxheight);
	~KX_Terrain();

	void Construct();
	void Destruct();

	void Update(KX_Camera* cam);

	// le niveau de subdivision maximal
	inline unsigned short GetMaxSubDivision() { return m_maxSubDivision; };
	// la distance maximal
	inline float GetMaxDistance() { return m_maxDistance; };
	inline float GetChunkSize() { return m_chunkSize; };
	inline float GetMaxHeight() { return m_maxHeight; };
	// le nombre de subdivision par rapport à une distance
	unsigned short GetSubdivision(float distance);
	inline KX_Chunk* GetChunk(vector2DInt position) { return m_positionToChunk[position]; };
};

#endif //__KX_TERRAIN_H__