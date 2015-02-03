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
#include "MT_Transform.h"

class KX_Chunk;
class KX_Camera;

class RAS_IRasterizer;
class RAS_MaterialBucket;

class vector2DInt
{
public:
	int m_x;
	int m_y;
	vector2DInt(const int& x, const int& y)
	{
		m_x = x;
		m_y = y;
	}
};

inline MT_OStream& operator<<(MT_OStream& os, const vector2DInt& v1)
{
	return os << "(x : " << v1.m_x << ", y : " << v1.m_y << ")";
}

inline bool operator==(const vector2DInt& v1, const vector2DInt& v2)
{
	return v1.m_x == v2.m_x && v1.m_y == v2.m_y;
}

inline bool operator!=(const vector2DInt& v1, const vector2DInt& v2)
{
	return v1.m_x != v2.m_x || v1.m_y != v2.m_y;
}

inline bool operator<(const vector2DInt& v1, const vector2DInt& v2)
{
	return v1.m_x < v2.m_x || (!(v2.m_x < v1.m_x) && v1.m_y < v2.m_y);
}

inline bool operator>(const vector2DInt& v1, const vector2DInt& v2)
{
	return v2 < v1;
}

typedef std::map<vector2DInt, KX_Chunk*>::iterator chunkMapIt;

class KX_Terrain
{
private:
	bool m_construct;
	KX_Chunk* m_chunks[4];
	unsigned short m_maxSubDivision;
	unsigned short m_width;
	float m_maxDistance;
	float m_chunkSize;
	float m_maxHeight;

	RAS_MaterialBucket* m_bucket;

public:
	KX_Terrain(unsigned short maxSubDivisions, unsigned short width, float maxDistance, float chunkSize, float maxheight);
	~KX_Terrain();

	void Construct();
	void Destruct();

	void Update(KX_Camera* cam, const MT_Transform& cameratrans, RAS_IRasterizer* rasty);

	// le niveau de subdivision maximal
	inline unsigned short GetMaxSubDivision() { return m_maxSubDivision; };
	// la distance maximal
	inline float GetMaxDistance() { return m_maxDistance; };
	inline float GetChunkSize() { return m_chunkSize; };
	inline float GetMaxHeight() { return m_maxHeight; };
	// le nombre de subdivision par rapport à une distance
	unsigned short GetSubdivision(float distance);
	KX_Chunk* GetChunkRelativePosition(int x, int y);
};

#endif //__KX_TERRAIN_H__