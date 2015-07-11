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
#include "KX_Terrain.h"
#include "DNA_mesh_types.h"
#include "DNA_terrain_types.h"
#include <iostream>
#include "BLI_math.h"
#include "BLI_noise.h"

KX_TerrainZoneMesh::KX_TerrainZoneMesh(KX_Terrain *terrain, TerrainZone *zoneInfo, Mesh *mesh)
	:m_terrain(terrain),
	m_zoneInfo(zoneInfo)
{
	m_box[0] = 0.0; // min x
	m_box[1] = 0.0; // max x
	m_box[2] = 0.0; // min y
	m_box[3] = 0.0; // max y

	if (mesh) {
		m_derivedMesh = CDDM_from_mesh(mesh);
		DM_ensure_tessface(m_derivedMesh);

		const unsigned int totvert = m_derivedMesh->getNumVerts(m_derivedMesh);
		MVert *mvert = m_derivedMesh->getVertArray(m_derivedMesh);
		for (unsigned int i = 0; i < totvert; ++i) {
			const MVert &vert = mvert[i];

			if (vert.co[0] < m_box[0])
				m_box[0] = vert.co[0];
			if (vert.co[0] > m_box[1])
				m_box[1] = vert.co[0];

			if (vert.co[1] < m_box[2])
				m_box[2] = vert.co[1];
			if (vert.co[1] > m_box[3])
				m_box[3] = vert.co[1];
		}
	}
	else
		m_derivedMesh = NULL;
}

KX_TerrainZoneMesh::~KX_TerrainZoneMesh()
{
	if (m_derivedMesh)
		m_derivedMesh->release(m_derivedMesh);
}

float KX_TerrainZoneMesh::GetMaxHeight() const
{
	float maxheight = 0.0;

	// clampage
	if (m_zoneInfo->flag & TERRAIN_ZONE_CLAMP) {
		maxheight += max_ff(m_zoneInfo->clampstart, m_zoneInfo->clampend);
	}

	// decalge a l'origine
	if (m_zoneInfo->offset > 0.0) {
		maxheight += m_zoneInfo->offset;
	}

	// bruit de perlin
	if (m_zoneInfo->flag & TERRAIN_ZONE_PERLIN_NOISE) {
		if (m_zoneInfo->height > 0.0) {
			maxheight += m_zoneInfo->height;
		}
	}

	return maxheight;
}

float KX_TerrainZoneMesh::GetMinHeight() const
{
	float minheight = 0.0;

	// clampage
	if (m_zoneInfo->flag & TERRAIN_ZONE_CLAMP) {
		minheight += min_ff(m_zoneInfo->clampstart, m_zoneInfo->clampend);
	}

	// decalge a l'origine
	if (m_zoneInfo->offset < 0.0) {
		minheight += m_zoneInfo->offset;
	}

	// bruit de perlin
	if (m_zoneInfo->flag & TERRAIN_ZONE_PERLIN_NOISE) {
		if (m_zoneInfo->height < 0.0) {
			minheight += m_zoneInfo->height;
		}
	}

	return minheight;
}

float KX_TerrainZoneMesh::GetClampedHeight(const float orgheight, const float interp, const float x, const float y, const float *v1, const float *v2, const float *v3) const
{
	float height = orgheight;

	if (m_zoneInfo->flag & TERRAIN_ZONE_CLAMP) {
		/*if (m_zoneInfo->flag & TERRAIN_ZONE_CLAMP_MESH) {
			if (v1 && v2 && v3)) {
				float start[3] = {x, y, 1000.0};
				float normal[3] = {0.0, 0.0, -1000.0};
				float lambda;
				isect_ray_tri_v3(start, normal, v1, v2, v3, &lambda, NULL);
				std::cout << "lambda : " << lambda << std::endl;
			}
		}
		else {*/
			if (orgheight > m_zoneInfo->clampend) {
				height = m_zoneInfo->clampend + (orgheight - m_zoneInfo->clampend) * interp;
			}
			else if (orgheight < m_zoneInfo->clampstart) {
				height = m_zoneInfo->clampstart + (orgheight - m_zoneInfo->clampstart) * interp;
			}
		// }
	}

	return height;
}

float KX_TerrainZoneMesh::GetMeshColorInterp(const float *point, const unsigned int faceindex, const MVert &v1, const MVert &v2, const MVert &v3) const
{
	float interp = 1.0;

	if (m_zoneInfo->flag & TERRAIN_ZONE_MESH_VERTEX_COLOR_INTERP) {
		MCol *mcol = (MCol *)m_derivedMesh->getTessFaceDataArray(m_derivedMesh, CD_MCOL);

		// couleur du remier vertice
		float c1[3] = {
			(float)mcol[faceindex * 4].r / 255, 
			(float)mcol[faceindex * 4].g / 255,
			(float)mcol[faceindex * 4].b / 255
		};
		// couleur du second vertice
		float c2[3] = {
			(float)mcol[faceindex * 4 + 1].r / 255,
			(float)mcol[faceindex * 4 + 1].g / 255, 
			(float)mcol[faceindex * 4 + 1].b / 255
		};
		// couleur du troisieme vertice
		float c3[3] = {
			(float)mcol[faceindex * 4 + 2].r / 255, 
			(float)mcol[faceindex * 4 + 2].g / 255,
			(float)mcol[faceindex * 4 + 2].b / 255
		};

		float weight[3];
		float color[3];

		barycentric_weights_v2(v1.co, v2.co, v3.co, point, weight);
		interp_v3_v3v3v3(color, c1, c2, c3, weight);
		interp = color[0];
	}

	return interp;
}

float KX_TerrainZoneMesh::GetHeight(const float x, const float y, const float interp) const
{
	float height = 0.0;

	if (m_zoneInfo->flag & TERRAIN_ZONE_PERLIN_NOISE) {
		height = (BLI_hnoise(m_zoneInfo->resolution, x, y, 0.0) * m_zoneInfo->height) + m_zoneInfo->offset;
	}
	else {
		height = m_zoneInfo->offset;
	}

	return height * interp;
}

// Si ledit point est en contact, on renvoie la modif asociée à sa hauteur
void KX_TerrainZoneMesh::GetVertexInfo(const float x, const float y, VertexZoneInfo *r_info) const
{
	float height = 0.0;
	bool hit = false;

	// set vertex 2d position
	r_info->pos[0] = x;
	r_info->pos[1] = y;

	// on utilise un mesh comme zone
	if (m_zoneInfo->flag & TERRAIN_ZONE_MESH && m_zoneInfo->mesh) {
		// en premier on verifie que le point est bien compris dans les maximum et minimun du mesh
		if ((m_box[0] < x && x < m_box[1]) && (m_box[2] < y && y < m_box[3])) {
			const unsigned int totface = m_derivedMesh->getNumTessFaces(m_derivedMesh);
			MVert *mvert = m_derivedMesh->getVertArray(m_derivedMesh);
			MFace *mface = m_derivedMesh->getTessFaceArray(m_derivedMesh);
			const float point[2] = {x, y};

			// On parcoure toutes les triangles
			for (unsigned int i = 0; i < totface; ++i) {
				const MVert &v1 = mvert[mface[i].v1];
				const MVert &v2 = mvert[mface[i].v2];
				const MVert &v3 = mvert[mface[i].v3];

				int result = isect_point_tri_v2(point, v1.co, v2.co, v3.co);
				// Si le point est bien dans un des triangles
				if (result == 1) {
					const float interp = GetMeshColorInterp(point, i, v1, v2, v3);
					hit = true;
					// on accéde à la hauteur précedente avec peut être une modification
					height += GetClampedHeight(r_info->height, 1.0 - interp, x, y, v1.co, v2.co, v3.co);
					height += GetHeight(x, y, interp);

					break;
				}
			}
		}
		// si on ne touche rien il faut tout de même garder la valeur précedente
		if (!hit) {
			height += GetClampedHeight(r_info->height, 1.0, x, y, NULL, NULL, NULL);
		}
	}
	else {
		height += GetClampedHeight(r_info->height, 1.0, x, y, NULL, NULL, NULL);
		height += GetHeight(x, y, 1.0);
	}

	r_info->height = height;
	if (hit && m_zoneInfo->flag & TERRAIN_ZONE_VERTEX_COLOR) {
		copy_v3_v3(r_info->color, m_zoneInfo->vertexcolor);
	}
}
