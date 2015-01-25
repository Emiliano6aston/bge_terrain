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
 
#include "KX_Terrain.h"
#include "KX_Chunk.h"

#include "KX_Camera.h"

KX_Terrain::KX_Terrain(unsigned short maxSubDivisions, unsigned short size, float maxDistance, float chunkSize, float maxheight)
	:m_maxSubDivision(maxSubDivisions),
	m_size(size),
	m_maxDistance(maxDistance),
	m_chunkSize(chunkSize),
	m_maxHeight(maxheight)
{
}

KX_Terrain::~KX_Terrain()
{
}

void KX_Terrain::Construct()
{
	for (int x = -(m_size / 2); x < m_size / 2; ++x)
	{
		for (int y = -(m_size / 2); y < m_size / 2; ++y)
		{
			vector2DInt pos(x, y);
			m_positionToChunk[pos] = new KX_Chunk(pos, this);
		}
	}
}

void KX_Terrain::Destruct()
{
	for (chunkMapIt it = m_positionToChunk.begin(); it != m_positionToChunk.end(); ++it)
		delete (*it).second;
}

void KX_Terrain::Update(KX_Camera* cam)
{
	for (chunkMapIt it = m_positionToChunk.begin(); it != m_positionToChunk.end(); ++it)
		(*it).second->Update(cam);

	for (chunkMapIt it = m_positionToChunk.begin(); it != m_positionToChunk.end(); ++it)
		(*it).second->UpdateDisplayArray((*it).first);

	for (chunkMapIt it = m_positionToChunk.begin(); it != m_positionToChunk.end(); ++it)
		(*it).second->EndUpdate();
}

unsigned short KX_Terrain::GetSubdivision(float distance)
{
	if (!distance)
		return m_maxSubDivision;

	return (unsigned short)(distance / m_maxDistance) * m_maxSubDivision;
}


