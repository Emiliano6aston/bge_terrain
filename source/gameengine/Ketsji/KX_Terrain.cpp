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

#define DEBUG(msg) std::cout << "Debug : " << msg << std::endl

KX_Terrain::KX_Terrain(unsigned short maxSubDivisions, unsigned short width, float maxDistance, float chunkSize, float maxheight)
	:m_construct(false),
	m_maxSubDivision(maxSubDivisions),
	m_width(width),
	m_maxDistance2(maxDistance * maxDistance),
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

void KX_Terrain::CalculateVisibleChunks(KX_Camera* cam)
{
	if (!m_construct)
		Construct();
	double starttime = KX_GetActiveEngine()->GetRealTime();
	m_chunks[0]->CalculateVisible(cam);
	m_chunks[1]->CalculateVisible(cam);
	m_chunks[2]->CalculateVisible(cam);
	m_chunks[3]->CalculateVisible(cam);
	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
	DEBUG(KX_Chunk::m_chunkActive << " active chunk");
	DEBUG(KX_ChunkNode::m_finalNode << " final chunk");
}
void KX_Terrain::UpdateChunksMeshes()
{
	double starttime = KX_GetActiveEngine()->GetRealTime();
	for (unsigned short i = 0; i < 4; ++i)
	{
		if (!m_chunks[i]->IsCulled())
			m_chunks[i]->UpdateMesh();
	}
	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
}

void KX_Terrain::RenderChunksMeshes(const MT_Transform& cameratrans, RAS_IRasterizer* rasty)
{
	// activation du materiau
	while (m_bucket->ActivateMaterial(cameratrans, rasty)) {}

	double starttime = KX_GetActiveEngine()->GetRealTime();
	// rendu du mesh
	for (unsigned short i = 0; i < 4; ++i)
	{
		if (!m_chunks[i]->IsCulled())
			m_chunks[i]->RenderMesh(rasty);
	}
	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
}

unsigned short KX_Terrain::GetSubdivision(float distance) const
{
	unsigned int ret = 2;
	for (float i = m_maxSubDivision; i > 0.; --i)
	{
		if (distance > (i / m_maxSubDivision * m_maxDistance2) || ret == m_width)
			break;
		ret *= 2;
	}
	return ret;
}

// renvoie le chunk correspondant à cette position, on doit le faire de maniere recursive car on utilise un QuadTree
KX_ChunkNode* KX_Terrain::GetChunkRelativePosition(short x, short y)
{
	for (unsigned short i = 0; i < 4; ++i)
	{
		KX_ChunkNode* ret = m_chunks[i]->GetChunkRelativePosition(x, y);
		if (ret)
			return ret;
	}
	return NULL;
};

KX_ChunkNode** KX_Terrain::NewChunkNodeList(short x, short y, unsigned short level)
{
	KX_ChunkNode **nodeList = (KX_ChunkNode**)malloc(4 * sizeof(KX_ChunkNode*));

	// la taille relative d'un chunk, = 2 si le noeud et final
	unsigned short relativesize = m_width / level;
	// la largeur du chunk 
	unsigned short width = relativesize / 2;

	nodeList[0] = new KX_Chunk(x - width, y - width, relativesize, level * 2, this);
	nodeList[1] = new KX_Chunk(x + width, y - width, relativesize, level * 2, this);
	nodeList[2] = new KX_Chunk(x - width, y + width, relativesize, level * 2, this);
	nodeList[3] = new KX_Chunk(x + width, y + width, relativesize, level * 2, this);
	return nodeList;
}

