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

#include "KX_TerrainZone.h"
#include "DNA_mesh_types.h"
#include <iostream>
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_math_color.h"
#include "BLI_noise.h"

extern "C" {
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
}


KX_TerrainZoneInfo::KX_TerrainZoneInfo(float resolution,
									   float height,
									   float offset)
	:m_resolution(resolution),
	m_height(height),
	m_offset(offset)
{
}

KX_TerrainZoneInfo::~KX_TerrainZoneInfo()
{
}

KX_TerrainZoneMesh::KX_TerrainZoneMesh(KX_TerrainZoneInfo* zoneInfo, Mesh *mesh)
	:m_zoneInfo(zoneInfo)
{
	m_derivedMesh = CDDM_from_mesh(mesh);
	DM_ensure_tessface(m_derivedMesh);
}

KX_TerrainZoneMesh::~KX_TerrainZoneMesh()
{
	m_derivedMesh->release(m_derivedMesh);
}

///Si ledit point est en contact, on renvoie la modif asociée à sa hauteur
float KX_TerrainZoneMesh::GetHeight(float x, float y) const
{
	const unsigned int totface = m_derivedMesh->getNumTessFaces(m_derivedMesh);
	MVert *mvert = m_derivedMesh->getVertArray(m_derivedMesh);
	MFace *mface = m_derivedMesh->getTessFaceArray(m_derivedMesh);
	MCol *mcol = (MCol *)m_derivedMesh->getTessFaceDataArray(m_derivedMesh, CD_MCOL);

	const float point[2] = {x, y};
	for (unsigned int i = 0; i < totface; ++i) {
		const MVert &v1 = mvert[mface[i].v1];
		const MVert &v2 = mvert[mface[i].v2];
		const MVert &v3 = mvert[mface[i].v3];

		int result = isect_point_tri_v2(point, v1.co, v2.co, v3.co);
		if (result == 1) {
			float c1[3] = {
				(float)mcol[i * 4].r / 255, 
				(float)mcol[i * 4].g / 255,
				(float)mcol[i * 4].b / 255
			};
			float c2[3] = {
				(float)mcol[i * 4 + 1].r / 255,
				(float)mcol[i * 4 + 1].g / 255, 
				(float)mcol[i * 4 + 1].b / 255
			};
			float c3[3] = {
				(float)mcol[i * 4 + 2].r / 255, 
				(float)mcol[i * 4 + 2].g / 255,
				(float)mcol[i * 4 + 2].b / 255
			};
			float weight[3];
			float color[3];

			barycentric_weights_v2(v1.co, v2.co, v3.co, point, weight);
			interp_v3_v3v3v3(color, c1, c2, c3, weight);

			const float noise = BLI_hnoise(m_zoneInfo->GetResolution(), x, y, 0.0);
			return (noise * m_zoneInfo->GetHeightMax() + m_zoneInfo->GetOffset()) * color[0];
		}
	}
	return 0.0;
}