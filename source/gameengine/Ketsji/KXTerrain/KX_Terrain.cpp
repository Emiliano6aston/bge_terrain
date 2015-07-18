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
 
#include "KX_Terrain.h"
#include "KX_Chunk.h"
#include "KX_ChunkMotionState.h"

#include "KX_Camera.h"
#include "KX_PythonInit.h"
#include "KX_Scene.h"
#include "KX_SG_NodeRelationships.h"
#include "SG_Controller.h"
#include "RAS_MeshObject.h"
#include "RAS_MaterialBucket.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IGraphicController.h"
#include "PHY_IPhysicsController.h"
#include "KX_MotionState.h"
#include "ListValue.h"
#include "DNA_terrain_types.h"

#define DEBUG(msg) // std::cout << "Debug (" << __func__ << ", " << this << ") : " << msg << std::endl;

static STR_String camname = "Camera";

KX_Terrain::KX_Terrain(void *sgReplicationInfo,
					   SG_Callbacks callbacks,
					   RAS_MaterialBucket *bucket,
					   KX_GameObject *templateObject,
					   unsigned short maxLevel,
					   unsigned short vertexSubdivision,
					   unsigned short width,
					   float maxDistance,
					   float physicsMaxDistance,
					   float chunkSize)
	:KX_GameObject(sgReplicationInfo, callbacks),
	m_bucket(bucket),
	m_templateObject(templateObject),
	m_maxChunkLevel(maxLevel),
	m_vertexSubdivision(vertexSubdivision),
	m_width(width),
	m_maxDistance2(maxDistance * maxDistance),
	m_physicsMaxDistance2(physicsMaxDistance * physicsMaxDistance),
	m_chunkSize(chunkSize),
	m_maxHeight(0.0f),
	m_minHeight(0.0f),
	m_construct(false),
	m_frame(0)
{
	unsigned int realmaxlevel = 0;
	for (unsigned int i = 1; i < m_width; i *= 2) {
		++realmaxlevel;
	}

	if (m_maxChunkLevel > realmaxlevel) {
		std::cout << "Warning: wrong max chunk level, it should be : " << realmaxlevel << std::endl;
		m_maxChunkLevel = realmaxlevel;
	}
}

KX_Terrain::~KX_Terrain()
{
	Destruct();

	for (unsigned short i = 0; i < m_zoneMeshList.size(); ++i) {
		delete m_zoneMeshList[i];
	}
}

void KX_Terrain::Construct()
{
	DEBUG("Construct terrain");

	m_nodeTree = NewNodeList(NULL, 0, 0, 2);
	m_construct = true;
}

void KX_Terrain::Destruct()
{
	DEBUG("Destruct terrain");
	// destruction des chunks
	delete m_nodeTree[0];
	delete m_nodeTree[1];
	delete m_nodeTree[2];
	delete m_nodeTree[3];

	free(m_nodeTree);

	ScheduleEuthanasyChunks();
}

void KX_Terrain::CalculateVisibleChunks(KX_Camera* culledcam)
{
	if (!m_construct)
		Construct();

	CListValue *objects = KX_GetActiveScene()->GetObjectList();
	m_nodeTree[0]->CalculateVisible(culledcam, objects);
	m_nodeTree[1]->CalculateVisible(culledcam, objects);
	m_nodeTree[2]->CalculateVisible(culledcam, objects);
	m_nodeTree[3]->CalculateVisible(culledcam, objects);

	ScheduleEuthanasyChunks();

// 	std::cout << KX_ChunkNode::m_activeNode << " nodes" << std::endl;
// 	std::cout << m_chunkList.size() << " chunk" << std::endl;
}
void KX_Terrain::UpdateChunksMeshes()
{
	for (KX_ChunkList::iterator it = m_chunkList.begin(); it != m_chunkList.end(); ++it) {
		(*it)->UpdateMesh();
	}

	for (KX_ChunkList::iterator it = m_chunkList.begin(); it != m_chunkList.end(); ++it) {
		(*it)->EndUpdateMesh();
	}

#ifdef STATS
	++m_frame;

	if (m_frame > 60) {
		KX_Chunk::PrintTime();
		KX_Chunk::ResetTime();
		m_frame = 0;
	}
#endif
}

void KX_Terrain::RenderChunksMeshes(const MT_Transform& cameratrans, RAS_IRasterizer* rasty)
{
	KX_Camera* cam = KX_GetActiveScene()->FindCamera(camname);
	// rendu du mesh
	for (KX_ChunkList::iterator it = m_chunkList.begin(); it != m_chunkList.end(); ++it) {
		KX_Chunk *chunk = *it;
		chunk->RenderMesh(rasty, cam);
// 		chunk->SetCulled(false); // toujours faux
// 		chunk->UpdateBuckets(false);
	}
}

void KX_Terrain::DrawDebugNode()
{
#ifdef DRAW_DEBUG
	for (unsigned int i = 0; i < 4; ++i)
		m_nodeTree[i]->DrawDebugInfo(KX_ChunkNode::DEBUG_BOX);
#endif // DRAW_DEBUG
}

unsigned short KX_Terrain::GetSubdivision(float distance, bool iscamera) const
{
	unsigned int ret = 1;
	// les objets non pas besoin d'une aussi grande subdivision que la camera
	const float maxdistance = iscamera ? m_maxDistance2 : m_physicsMaxDistance2;
	for (float i = m_maxChunkLevel; i > 0.; --i) {
		if (distance > (i / m_maxChunkLevel * maxdistance) || ret == m_maxChunkLevel)
			break;
		++ret;
	}
	return ret;
}

KX_ChunkNode *KX_Terrain::GetNodeRelativePosition(const KX_ChunkNode::Point2D& pos)
{
	for (unsigned int i = 0; i < 4; ++i) {
		KX_ChunkNode *node = m_nodeTree[i]->GetNodeRelativePosition(pos);
		if (node)
			return node;
	}
	return NULL;
}

VertexZoneInfo *KX_Terrain::GetVertexInfo(float x, float y) const
{
	VertexZoneInfo *info = new VertexZoneInfo();

	for (unsigned short i = 0; i < m_zoneMeshList.size(); ++i)
		m_zoneMeshList[i]->GetVertexInfo(x, y, info);

	return info;
}

KX_ChunkNode** KX_Terrain::NewNodeList(KX_ChunkNode *parentNode, int x, int y, unsigned short level)
{
	KX_ChunkNode** nodeList = (KX_ChunkNode**)malloc(4 * sizeof(KX_ChunkNode*));

	// la taille relative d'un chunk, = 2 si le noeud et final
	const unsigned short relativesize = m_width / pow(2, level);
	// la largeur du chunk 
	const unsigned short width = relativesize / 2;

	nodeList[0] = new KX_ChunkNode(parentNode, x - width, y - width, relativesize, level + 1, this);
	nodeList[1] = new KX_ChunkNode(parentNode, x + width, y - width, relativesize, level + 1, this);
	nodeList[2] = new KX_ChunkNode(parentNode, x - width, y + width, relativesize, level + 1, this);
	nodeList[3] = new KX_ChunkNode(parentNode, x + width, y + width, relativesize, level + 1, this);

	return nodeList;
}

KX_Chunk* KX_Terrain::AddChunk(KX_ChunkNode* node)
{
#ifdef STATS
	double starttime;
	double endtime;
#endif

#ifdef STATS
	starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	KX_Scene* scene = KX_GetActiveScene();
	KX_GameObject* orgobj = (KX_GameObject*)scene->GetInactiveList()->FindValue("Cube");

	PHY_IPhysicsController *phyCtrl = NULL;

	// replicate physics controller
	if (orgobj->GetPhysicsController()) {
		PHY_IMotionState *motionstate = new KX_ChunkMotionState(node);
		phyCtrl = orgobj->GetPhysicsController()->GetReplica();

		phyCtrl->SetNewClientInfo(m_pClient_info);
		phyCtrl->PostProcessReplica(motionstate, NULL);
	}

	KX_Chunk *chunk = new KX_Chunk(node, m_bucket, phyCtrl);

	////////////////////////// AJOUT DANS LA LISTE ///////////////////////////
	m_chunkList.push_back(chunk);

#ifdef STATS
	endtime = KX_GetActiveEngine()->GetRealTime();
	KX_Chunk::chunkCreationTime += endtime - starttime;
#endif

	return chunk;
}

/** Supprime le chunk de la liste des chunks actifs et l'ajoute dans une 
 * liste temporaire pour sa suppression.
 */
void KX_Terrain::RemoveChunk(KX_Chunk *chunk)
{
	m_chunkList.remove(chunk);
	m_euthanasyChunkList.push_back(chunk);
}

void KX_Terrain::ScheduleEuthanasyChunks()
{
	for (KX_ChunkList::iterator it = m_euthanasyChunkList.begin(); it != m_euthanasyChunkList.end(); ++it) {
		KX_Chunk *chunk = *it;

		delete chunk;
	}
	m_euthanasyChunkList.clear();
}

void KX_Terrain::AddTerrainZoneMesh(KX_TerrainZoneMesh *zoneMesh)
{
	m_zoneMeshList.push_back(zoneMesh);

	const float maxheight = zoneMesh->GetMaxHeight();
	const float minheight = zoneMesh->GetMinHeight();

	m_maxHeight += maxheight;
	m_minHeight += minheight;

	if (maxheight < minheight) {
		std::cout << "Warning : min height greater than max height !" << std::endl;
	}
}
