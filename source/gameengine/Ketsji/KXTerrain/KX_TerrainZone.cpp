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
#include "DNA_group_types.h"

#include "BLI_noise.h"
#include "BLI_utildefines.h"

#include "BKE_image.h"

#include "IMB_imbuf_types.h"

extern "C" {
	#include "IMB_imbuf.h"
	#include "BLI_math.h"
}

#include <iostream>

KX_TerrainZoneMesh::KX_TerrainZoneMesh(KX_Terrain *terrain, TerrainZone *zoneInfo, Mesh *mesh)
	:m_terrain(terrain),
	m_zoneInfo(zoneInfo)
{
	if (mesh) {
		m_derivedMesh = CDDM_from_mesh(mesh);
		DM_ensure_tessface(m_derivedMesh);

		const unsigned int totvert = m_derivedMesh->getNumVerts(m_derivedMesh);
		MVert *mvert = m_derivedMesh->getVertArray(m_derivedMesh);
		for (unsigned int i = 0; i < totvert; ++i) {
			const MVert &vert = mvert[i];

			if (i == 0) {
				m_box[0] = vert.co[0];
				m_box[1] = vert.co[0];
				m_box[2] = vert.co[1];
				m_box[3] = vert.co[1];
				continue;
			}

			m_box[0] = min_ff(m_box[0], vert.co[0]);
			m_box[1] = max_ff(m_box[1], vert.co[0]);
			m_box[2] = min_ff(m_box[2], vert.co[1]);
			m_box[3] = max_ff(m_box[3], vert.co[1]);
		}
	}
	else
		m_derivedMesh = NULL;

	m_buf = m_zoneInfo->image ? BKE_image_acquire_ibuf(m_zoneInfo->image, NULL, NULL) : NULL;
	if (m_buf) {
		// On genère l'image avec une precision de 32 bits.
		IMB_float_from_rect(m_buf);
	}
}

KX_TerrainZoneMesh::~KX_TerrainZoneMesh()
{
	if (m_derivedMesh)
		m_derivedMesh->release(m_derivedMesh);
	if (m_buf)
		BKE_image_release_ibuf(m_zoneInfo->image, m_buf, NULL);
}

float KX_TerrainZoneMesh::GetMaxHeight(float origmaxheight) const
{
	if (!(m_zoneInfo->flag & TERRAIN_ZONE_ACTIVE)) {
		return origmaxheight;
	}

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
		maxheight += max_ff(m_zoneInfo->noiseheight, 0.0f);
	}
	if (m_zoneInfo->flag & TERRAIN_ZONE_IMAGE) {
		maxheight += max_ff(m_zoneInfo->imageheight, 0.0f);
	}

	return max_ff(origmaxheight, maxheight);
}

float KX_TerrainZoneMesh::GetMinHeight(float origminheight) const
{
	if (!(m_zoneInfo->flag & TERRAIN_ZONE_ACTIVE)) {
		return origminheight;
	}

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
		minheight += min_ff(m_zoneInfo->noiseheight, 0.0f);
	}
	if (m_zoneInfo->flag & TERRAIN_ZONE_IMAGE) {
		minheight += min_ff(m_zoneInfo->imageheight, 0.0f);
	}

	return min_ff(origminheight, minheight);
}

float KX_TerrainZoneMesh::GetClampedHeight(const float orgheight, const float x, const float y, const float *v1, const float *v2, const float *v3) const
{
	float height = 0.0f;

	if (m_zoneInfo->flag & TERRAIN_ZONE_CLAMP) {
		if (m_zoneInfo->flag & TERRAIN_ZONE_CLAMP_MESH) {
			if (v1 && v2 && v3) {
				const float rayheight = 5000.0f; // TODO exposer à l'utilisateur
				float start[3] = {x, y, rayheight};
				float normal[3] = {0.0f, 0.0f, -1.0f};
				float uv[2];
				float lambda = 0.0f;
				if (isect_ray_tri_v3(start, normal, v1, v2, v3, &lambda, uv)) {
					height = rayheight - lambda - orgheight;
				}
			}
		}
		else {
			float origheight2 = orgheight;
			CLAMP(origheight2, m_zoneInfo->clampstart, m_zoneInfo->clampend);
			height = origheight2 - orgheight;
		}
	}

	return height;
}

float KX_TerrainZoneMesh::GetMeshColorInterp(const float x, const float y, const int faceindex, const float *v1, const float *v2, const float *v3) const
{
	float interp = 1.0f;

	if ((m_zoneInfo->flag & TERRAIN_ZONE_MESH_VERTEX_COLOR_INTERP && m_derivedMesh) || faceindex != -1) {
		MCol *mcol = (MCol *)m_derivedMesh->getTessFaceDataArray(m_derivedMesh, CD_MCOL);

		// couleur du remier vertice
		float c1[3] = {
			(float)mcol[faceindex * 4].r / 255.0f, 
			(float)mcol[faceindex * 4].g / 255.0f,
			(float)mcol[faceindex * 4].b / 255.0f
		};
		// couleur du second vertice
		float c2[3] = {
			(float)mcol[faceindex * 4 + 1].r / 255.0f,
			(float)mcol[faceindex * 4 + 1].g / 255.0f,
			(float)mcol[faceindex * 4 + 1].b / 255.0f
		};
		// couleur du troisieme vertice
		float c3[3] = {
			(float)mcol[faceindex * 4 + 2].r / 255.0f,
			(float)mcol[faceindex * 4 + 2].g / 255.0f,
			(float)mcol[faceindex * 4 + 2].b / 255.0f
		};

		float weight[3];
		float color[3];

		const float point[2] = {x, y};
		barycentric_weights_v2(v1, v2, v3, point, weight);
		interp_v3_v3v3v3(color, c1, c2, c3, weight);
		interp = color[0];
	}
	else if (m_zoneInfo->flag & TERRAIN_ZONE_USE_OBJECT && m_zoneInfo->groupobject) {
		Group *group = m_zoneInfo->groupobject;
		interp = 0.0f;
		unsigned short objindex = 1;
		for (GroupObject *groupobj = (GroupObject *)group->gobject.first;
			 groupobj; groupobj = groupobj->next, ++objindex)
		{
			Object *blendobj = groupobj->ob;
			const float *position = blendobj->loc;
			const float *scale = blendobj->size;
			const float influence = m_zoneInfo->objectinfluence;

			const float point[2] = {fabs(x - position[0]), fabs(y - position[1])};
			const float radius[2] = {scale[0] * influence, scale[1] * influence};

			if (point[0] < radius[0] && point[1] < radius[1]) {
				const float pointlength = len_v2(point);
				static const float xaxis[2] = {1.0f, 0.0f};
				const float angle = angle_v2v2(xaxis, point);
				const float max[2] = {cos(angle) * radius[0], sin(angle) * radius[1]};
				const float maxlength = len_v2(max);

				float objinterp = (maxlength - pointlength) / maxlength;
				CLAMP(objinterp, 0.0f, 1.0f);

				interp = max_ff(interp, objinterp);
			}
		}
	}

	return interp;
}

float KX_TerrainZoneMesh::GetNoiseHeight(const float x, const float y) const
{
	float height = 0.0f;

	if (m_zoneInfo->flag & TERRAIN_ZONE_PERLIN_NOISE) {
		height = (BLI_hnoise(m_zoneInfo->resolution, x, y, 0.0f) * m_zoneInfo->noiseheight);
	}

	return height;
}

float KX_TerrainZoneMesh::GetImageHeight(const float x, const float y) const
{
	float height = 0.0f;

	if (m_zoneInfo->flag & TERRAIN_ZONE_IMAGE && m_buf) {
		const float terrainsize = m_terrain->GetWidth() * m_terrain->GetChunkSize();
		const float halfterrainsize = terrainsize / 2.0f;

		float color[4];

		float u = (halfterrainsize + x) / terrainsize * m_buf->x;
		float v = (halfterrainsize + y) / terrainsize * m_buf->y;
		CLAMP(u, 1, m_buf->x - 1);
		CLAMP(v, 1, m_buf->y - 1);

		BLI_bicubic_interpolation_fl(m_buf->rect_float, color, m_buf->x, m_buf->y, 4, u, v);

		height = color[0] * m_zoneInfo->imageheight;
	}

	return height;
}

// Si ledit point est en contact, on renvoie la modif asociée à sa hauteur
void KX_TerrainZoneMesh::GetVertexInfo(const float x, const float y, VertexZoneInfo *r_info) const
{
	if (!(m_zoneInfo->flag & TERRAIN_ZONE_ACTIVE)) {
		return;
	}

	float deltaheight = 0.0f;
	float interp = 1.0f;
	bool hit = false;

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

				const int result = isect_point_tri_v2(point, v1.co, v2.co, v3.co);
				// Si le point est bien dans un des triangles
				if (result == 1) {
					interp = GetMeshColorInterp(x, y, i, v1.co, v2.co, v3.co);
					hit = true;
					// La difference entre la hauteur precedente et une hauteur clampée.
					deltaheight += GetClampedHeight(r_info->height, x, y, v1.co, v2.co, v3.co);
					// La hauteur par default.
					deltaheight += m_zoneInfo->offset;
					// La hauteur calculé avec un bruit de perlin.
					deltaheight += GetNoiseHeight(x, y);
					// La hauteur dedui par une image.
					deltaheight += GetImageHeight(x, y);
					// On fais l'interpolation de cette difference de hauteur.
					deltaheight *= interp;

					break;
				}
			}
		}
		if (!hit) {
			return;
		}
	}
	else {
		interp = GetMeshColorInterp(x, y, -1, NULL, NULL, NULL);
		deltaheight += GetClampedHeight(r_info->height, x, y, NULL, NULL, NULL);
		deltaheight += m_zoneInfo->offset;
		deltaheight += GetNoiseHeight(x, y);
		deltaheight += GetImageHeight(x, y);
		deltaheight *= interp;
	}

	r_info->height += deltaheight;

	if (m_zoneInfo->flag & TERRAIN_ZONE_USE_UV_TEXTURE_COLOR) {
		if (deltaheight != 0.0f) {
			const unsigned short channel = m_zoneInfo->uvchannel / 2;
			const unsigned short axis = m_zoneInfo->uvchannel % 2;
			float value;
			if (m_zoneInfo->flag & TERRAIN_ZONE_USE_HEIGHT_COLOR) {
				value = fabs(deltaheight);
				if (m_zoneInfo->flag & TERRAIN_ZONE_DIVIDE_COLOR) {
					value /= m_zoneInfo->colordividor;
				}
				value *= m_zoneInfo->color;
			}
			else {
				value = m_zoneInfo->color;
			}
			r_info->m_uvs[channel][axis] = value * interp;
		}
	}
}
