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
#include "KX_PythonInit.h"
#include "KX_Scene.h"
#include "KX_GameObject.h"
#include "RAS_MeshObject.h"
#include "RAS_MaterialBucket.h"

#define DEBUG(msg) std::cout << msg << std::endl

KX_Terrain::KX_Terrain(unsigned short maxSubDivisions, unsigned short size, float maxDistance, float chunkSize, float maxheight)
	:m_maxSubDivision(maxSubDivisions),
	m_size(size),
	m_maxDistance(maxDistance*maxDistance),
	m_chunkSize(chunkSize),
	m_maxHeight(maxheight),
	m_bucket(NULL)
{
	DEBUG("Create terrain");
}

KX_Terrain::~KX_Terrain()
{
	Destruct();
}

void KX_Terrain::Construct()
{
	DEBUG("Construct terrain");
	KX_GameObject* obj = (KX_GameObject*)KX_GetActiveScene()->GetObjectList()->FindValue("Cube");
	if (!obj)
	{
		DEBUG("no obj");
		return;
	}
	m_bucket = obj->GetMesh(0)->GetMeshMaterial((unsigned int)0)->m_bucket;

	for (int x = -(m_size / 2); x < m_size / 2; ++x)
	{
		for (int y = -(m_size / 2); y < m_size / 2; ++y)
		{
			vector2DInt pos(x, y);
			m_positionToChunk[pos] = new KX_Chunk(pos, this);
		}
	}
	DEBUG("Create " << m_positionToChunk.size() << " chunks");
}

void KX_Terrain::Destruct()
{
	DEBUG("Destruct terrain");
	for (chunkMapIt it = m_positionToChunk.begin(); it != m_positionToChunk.end(); ++it)
		delete it->second;
}

void KX_Terrain::Update(KX_Camera* cam, const MT_Transform& cameratrans, RAS_IRasterizer* rasty)
{
	if (m_positionToChunk.empty())
		Construct();

	for (chunkMapIt it = m_positionToChunk.begin(); it != m_positionToChunk.end(); ++it)
		it->second->Update(cam);

	while (m_bucket->ActivateMaterial(cameratrans, rasty)) {}

	for (chunkMapIt it = m_positionToChunk.begin(); it != m_positionToChunk.end(); ++it)
		it->second->UpdateDisplayArrayDraw(rasty);

	for (chunkMapIt it = m_positionToChunk.begin(); it != m_positionToChunk.end(); ++it)
		it->second->EndUpdate();
}

unsigned short KX_Terrain::GetSubdivision(float distance)
{
	unsigned int ret = 1;
	for (float i = m_maxSubDivision; i > 0.; --i)
	{
		if (distance > (i / m_maxSubDivision * m_maxDistance))
			break;
		ret += ret;
	}
	return ret;
}

KX_Chunk* KX_Terrain::GetChunk(int x, int y)
{
	const vector2DInt pos(x, y);
	if (m_positionToChunk.count(pos))
		return m_positionToChunk[pos];
	return NULL;
};


