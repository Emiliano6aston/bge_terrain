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

extern "C" {
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
}


KX_TerrainZoneInfo::KX_TerrainZoneInfo(float resolution,
									   float offset,
									   float height)
	:m_resolution(resolution),
	m_offset(offset),
	m_height(height)
{
}

KX_TerrainZoneInfo::~KX_TerrainZoneInfo()
{
}

KX_TerrainZoneMesh::KX_TerrainZoneMesh(KX_TerrainZoneInfo* zoneInfo, Mesh *mesh)
	:m_zoneInfo(zoneInfo)
{
	m_derivedMesh = CDDM_from_mesh(mesh);
}

///Si ledit point est en contact, on renvoie la modif asociée à sa hauteur
float KX_TerrainZoneMesh::GetHeightZ(const MT_Point3 &pos) const
{
	return 0.0;
}



