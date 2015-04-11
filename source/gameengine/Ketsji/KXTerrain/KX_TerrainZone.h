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

#ifndef __KX_TERRAIN_ZONE_H__
#define __KX_TERRAIN_ZONE_H__

#include "MT_Point2.h"
#include "MT_Point3.h"

extern "C" {
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
}

class Mesh;
class DerivedMesh;
class TerrainZone;

struct VertexZoneInfo
{
	float height;
	float color[3];
};

class KX_TerrainZoneMesh
{
private:
	//La configuration que suit cette zone:
	TerrainZone *m_zoneInfo;
	float m_box[4];
	DerivedMesh *m_derivedMesh;

public:
	KX_TerrainZoneMesh(TerrainZone *zoneInfo,
					   Mesh *mesh);
	~KX_TerrainZoneMesh();

	float GetMaxHeight() const;
	float GetMinHeight() const;

	float GetClampedHeight(const float orgheight, const float interp) const;
	float GetMeshColorInterp(const float *point, const unsigned int faceindex, const MVert &v1, const MVert &v2, const MVert &v3) const;
	float GetHeight(const float x, const float y, const float interp) const;

	///Si ledit point est en contact, on renvoie la modif asociée à sa hauteur
	void GetVertexInfo(const float x, const float y, VertexZoneInfo *info) const;

	inline TerrainZone *GetTerrainZoneInfo() const {
		return m_zoneInfo;
	}
};

#endif // __KX_TERRAIN_ZONE_H__