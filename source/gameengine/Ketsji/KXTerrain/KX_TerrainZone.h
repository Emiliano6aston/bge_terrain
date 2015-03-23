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

#include "MT_Point2.h"
#include "MT_Point3.h"

class Mesh;
class DerivedMesh;

class KX_TerrainZoneInfo
{
private:
	float m_resolution;
	float m_offset;
	float m_height;

public:
	KX_TerrainZoneInfo(float resolution,
					   float offset,
					   float height);
	~KX_TerrainZoneInfo();

	inline float GetResolution() const {
		return m_resolution;
	}

	inline float GetOffset() const {
		return m_resolution;
	}

	inline float GetHeight() const {
		return m_resolution;
	}
};

class KX_TerrainZoneMesh
{
private:
	//La configuration que suit cette zone:
	KX_TerrainZoneInfo* m_zoneInfo;
	MT_Point2 m_hitBox[4];
	DerivedMesh *m_derivedMesh;

public:
	KX_TerrainZoneMesh(KX_TerrainZoneInfo *zoneInfo,
					   Mesh *mesh);

	///Si ledit point est en contact, on renvoie la modif asociée à sa hauteur
	float GetHeightZ(const MT_Point3 &pos) const;
};


