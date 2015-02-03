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

KX_Terrain::KX_Terrain(unsigned short maxSubDivisions, unsigned short width, float maxDistance, float chunkSize, float maxheight)
	:m_construct(false),
	m_maxSubDivision(maxSubDivisions),
	m_width(width),
	m_maxDistance(maxDistance * maxDistance),
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
	// le materiau uilisé pour le rendu
	m_bucket = obj->GetMesh(0)->GetMeshMaterial((unsigned int)0)->m_bucket;

	// construction des 4 chunks principaux
	m_chunks[0] = new KX_Chunk(-m_width / 2, -m_width / 2, m_width, 1, this);
	m_chunks[1] = new KX_Chunk(m_width / 2, -m_width / 2, m_width, 1, this);
	m_chunks[2] = new KX_Chunk(-m_width / 2, m_width / 2, m_width, 1, this);
	m_chunks[3] = new KX_Chunk(m_width / 2, m_width / 2, m_width, 1, this);
	m_construct = true;
}

void KX_Terrain::Destruct()
{
	DEBUG("Destruct terrain");
	// destruction des chunks
	delete m_chunks[0];
	delete m_chunks[1];
	delete m_chunks[2];
	delete m_chunks[3];
}

void KX_Terrain::Update(KX_Camera* cam, const MT_Transform& cameratrans, RAS_IRasterizer* rasty)
{
	if (!m_construct)
		Construct();

	// mise à jour du niveau de subdivision
	m_chunks[0]->Update(cam);
	m_chunks[1]->Update(cam);
	m_chunks[2]->Update(cam);
	m_chunks[3]->Update(cam);

	// activation du materiau
	while (m_bucket->ActivateMaterial(cameratrans, rasty)) {}

	// construction du mesh et rendu
	m_chunks[0]->UpdateMesh();
	m_chunks[1]->UpdateMesh();
	m_chunks[2]->UpdateMesh();
	m_chunks[3]->UpdateMesh();

	// rendu
	m_chunks[0]->RenderMesh(rasty);
	m_chunks[1]->RenderMesh(rasty);
	m_chunks[2]->RenderMesh(rasty);
	m_chunks[3]->RenderMesh(rasty);

	// finalisation de la mise à jour
	m_chunks[0]->EndUpdate();
	m_chunks[1]->EndUpdate();
	m_chunks[2]->EndUpdate();
	m_chunks[3]->EndUpdate();
}

unsigned short KX_Terrain::GetSubdivision(float distance)
{
	unsigned int ret = 2;
	for (float i = m_maxSubDivision; i > 0.; --i)
	{
		if (distance > (i / m_maxSubDivision * m_maxDistance) || ret == m_width)
			break;
		ret *= 2;
	}
	return ret;
}

// renvoie le chunk correspondant à cette position, on doit le faire de maniere recursive car on utilise un QuadTree
KX_Chunk* KX_Terrain::GetChunkRelativePosition(int x, int y)
{
	for (unsigned int i = 0; i < 4; ++i)
	{
		KX_Chunk* ret = m_chunks[i]->GetChunkRelativePosition(x, y);
		if (ret)
			return ret;
	}
	return NULL;
};


