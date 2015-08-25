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
class KX_Terrain;
struct ImBuf;

class VertexZoneInfo
{
public:
	/// Vertex height
	float height;
	/// Vertex 2d coord
	float pos[2];
	/// Vertex color
	float color[3];
	/// count of chunk vertexes which use it.
	unsigned char refcount;

	VertexZoneInfo()
		:height(0.0f),
		refcount(1)
	{
	}

	void AddRef()
	{
		refcount++;
	}
	void Release()
	{
		--refcount;
		if (refcount == 0)
			delete this;
	}
};

class KX_TerrainZoneMesh
{
private:
	/// The terrain used as parent.
	KX_Terrain *m_terrain;
	/// All info on the zone WARNING blender data, no modification on.
	TerrainZone *m_zoneInfo;
	/// The box used to optimize.
	float m_box[4];
	/// The mesh.
	DerivedMesh *m_derivedMesh;
	/// L'image utilisé pour les hauteur (optionelle)
	ImBuf *m_buf;

public:
	KX_TerrainZoneMesh(KX_Terrain *terrain,
					   TerrainZone *zoneInfo,
					   Mesh *mesh);
	~KX_TerrainZoneMesh();

	/// Return the maximum possible height.
	float GetMaxHeight() const;
	/// Return the minimum possible height.
	float GetMinHeight() const;

	/** Compute an height clamped.
	 * \param orgheight The previous height.
	 * \param interp The interpolation of the mesh on this point.
	 * \param x The position on x.
	 * \param y The position on y.
	 * \param v1 The first vertex of the triangle hited.
	 * \param v2 The second vertex of the triangle hited.
	 * \param v3 The firth vertex of the triangle hited.
	 * \return The height clamped
	 */
	float GetClampedHeight(const float orgheight, const float x, const float y,
						   const float *v1, const float *v2, const float *v3) const;

	/** Compute the interpolation on a position.
	 * \param point The 2d point.
	 * \param faceindex The index of the face.
	 * \param v1 The first vertex of the face.
	 * \param v2 The second vertex of the face.
	 * \param v3 The firth vertex of the face.
	 * \return The interpolation on this position.
	 */
	float GetMeshColorInterp(const float *point, const unsigned int faceindex, const MVert &v1, const MVert &v2, const MVert &v3) const;
	float GetNoiseHeight(const float x, const float y) const;
	float GetImageHeight(const float x, const float y) const;

	/** Compute all vertex infos : height and color.
	 * \param x The position on x.
	 * \param y The position on y.
	 * \param r_info All vertex infos.
	 */
	void GetVertexInfo(const float x, const float y, VertexZoneInfo *r_info) const;

	inline TerrainZone *GetTerrainZoneInfo() const {
		return m_zoneInfo;
	}
};

#endif // __KX_TERRAIN_ZONE_H__
