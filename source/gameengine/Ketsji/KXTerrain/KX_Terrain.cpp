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

#define COLORED_PRINT(msg, color) std::cout << /*"\033[" << color << "m" <<*/ msg << /*"\033[30m" <<*/ std::endl;

#define DEBUG(msg) // std::cout << "Debug (" << __func__ << ", " << this << ") : " << msg << std::endl;
#define WARNING(msg) COLORED_PRINT("Warning : " << msg, 33);
#define INFO(msg) COLORED_PRINT("Info : " << msg, 37);
#define ERROR(msg) COLORED_PRINT("Error : " << msg, 31);
#define SUCCES(msg) COLORED_PRINT("Succes : " << msg, 32);
#define DEBUG_VAR(var) std::cout << #var << " : " << var << std::endl;

#define DEBUGNOENDL(msg) std::cout << msg;

#define DEBUG_VAR(var) std::cout << #var << " : " << var << std::endl;

typedef std::map<KX_ChunkNode::Point2D, KX_Chunk*>::iterator chunkit;

static STR_String camname = "Camera";

KX_Terrain::KX_Terrain(RAS_MaterialBucket *bucket,
					   KX_GameObject *templateObject,
					   unsigned short maxLevel,
					   unsigned short vertexSubdivision,
					   unsigned short width,
					   float maxDistance,
					   float physicsMaxDistance,
					   float chunkSize)
	:m_bucket(bucket),
	m_templateObject(templateObject),
	m_maxChunkLevel(maxLevel),
	m_vertexSubdivision(vertexSubdivision),
	m_width(width),
	m_maxDistance2(maxDistance * maxDistance),
	m_physicsMaxDistance2(physicsMaxDistance * physicsMaxDistance),
	m_chunkSize(chunkSize),
	m_maxHeight(0.0),
	m_minHeight(0.0),
	m_construct(false)
{
	unsigned int realmaxlevel = 1;
	for (unsigned int i = 1; i < m_width; i *= 2) {
		++realmaxlevel;
	}
	--realmaxlevel;
	std::cout << "max level : " << realmaxlevel << std::endl;

	if (m_maxChunkLevel > realmaxlevel)
		m_maxChunkLevel = realmaxlevel;
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
	std::cout << "max height : " << m_maxHeight << ", min height : " << m_minHeight << std::endl;
}

void KX_Terrain::Destruct()
{
	DEBUG("Destruct terrain");
	// destruction des chunks
	delete m_nodeTree[0];
	delete m_nodeTree[1];
	delete m_nodeTree[2];
	delete m_nodeTree[3];

	ScheduleEuthanasyChunks();
}

void KX_Terrain::CalculateVisibleChunks(KX_Camera* culledcam)
{
	culledcam = KX_GetActiveScene()->FindCamera(camname);
	MT_Transform camtrans(culledcam->GetWorldToCamera());
	MT_Matrix4x4 viewmat(camtrans);

	culledcam->SetModelviewMatrix(viewmat);

	if (!m_construct)
		Construct();

	double starttime = KX_GetActiveEngine()->GetRealTime();

	CListValue *objects = KX_GetActiveScene()->GetObjectList();
	m_nodeTree[0]->CalculateVisible(culledcam, objects);
	m_nodeTree[1]->CalculateVisible(culledcam, objects);
	m_nodeTree[2]->CalculateVisible(culledcam, objects);
	m_nodeTree[3]->CalculateVisible(culledcam, objects);

	ScheduleEuthanasyChunks();

	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
// 	std::cout << KX_ChunkNode::m_activeNode << " nodes" << std::endl;
// 	std::cout << m_chunkList.size() << " chunk" << std::endl;
}
void KX_Terrain::UpdateChunksMeshes()
{
	double starttime = KX_GetActiveEngine()->GetRealTime();

	for (unsigned int i = 0; i < m_chunkList.size(); ++i) {
		m_chunkList[i]->UpdateMesh();
	}

	for (unsigned int i = 0; i < m_chunkList.size(); ++i) {
		m_chunkList[i]->EndUpdateMesh();
	}
	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
}

void KX_Terrain::RenderChunksMeshes(const MT_Transform& cameratrans, RAS_IRasterizer* rasty)
{
	double starttime = KX_GetActiveEngine()->GetRealTime();

	KX_Camera* cam = KX_GetActiveScene()->FindCamera(camname);
	// rendu du mesh
	for (unsigned int i = 0; i < m_chunkList.size(); ++i) {
		KX_Chunk* chunk = m_chunkList[i];
		chunk->RenderMesh(rasty, cam);
// 		chunk->SetCulled(false); // toujours faux
		chunk->UpdateBuckets(false);
	}

	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
}

unsigned short KX_Terrain::GetSubdivision(float distance, bool iscamera) const
{
	unsigned int ret = 1;
	// les objets non pas besoin d'une aussi grande subdivision que la camera
	float maxdistance = iscamera ? m_maxDistance2 : m_physicsMaxDistance2;
	for (float i = m_maxChunkLevel; i > 0.; --i)
	{
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
	info->height = 0.0;

	for (unsigned short i = 0; i < m_zoneMeshList.size(); ++i)
		m_zoneMeshList[i]->GetVertexInfo(x, y, info);

	return info;
}

KX_ChunkNode** KX_Terrain::NewNodeList(KX_ChunkNode *parentNode, int x, int y, unsigned short level)
{
	KX_ChunkNode** nodeList = (KX_ChunkNode**)malloc(4 * sizeof(KX_ChunkNode*));

	// la taille relative d'un chunk, = 2 si le noeud et final
	unsigned short relativesize = m_width / pow(2, level);
	// la largeur du chunk 
	unsigned short width = relativesize / 2;

	nodeList[0] = new KX_ChunkNode(parentNode, x - width, y - width, relativesize, level + 1, this);
	nodeList[1] = new KX_ChunkNode(parentNode, x + width, y - width, relativesize, level + 1, this);
	nodeList[2] = new KX_ChunkNode(parentNode, x - width, y + width, relativesize, level + 1, this);
	nodeList[3] = new KX_ChunkNode(parentNode, x + width, y + width, relativesize, level + 1, this);

	return nodeList;
}

KX_Chunk* KX_Terrain::AddChunk(KX_ChunkNode* node)
{
	KX_Scene* scene = KX_GetActiveScene();
	KX_GameObject* orgobj = (KX_GameObject*)KX_GetActiveScene()->GetInactiveList()->FindValue("Cube");
	SG_Node* orgnode = orgobj->GetSGNode();
	KX_Chunk* chunk = new KX_Chunk(scene, KX_Scene::m_callbacks, node, m_bucket);
	SG_Node* rootnode = new SG_Node(chunk, scene, KX_Scene::m_callbacks);

	// define the relationship between this node and it's parent.
	KX_NormalParentRelation * parent_relation = 
		KX_NormalParentRelation::New();
	rootnode->SetParentRelation(parent_relation);

	// set node
	chunk->SetSGNode(rootnode);

	SGControllerList scenegraphcontrollers = orgnode->GetSGControllerList();
	rootnode->RemoveAllControllers();
	SGControllerList::iterator cit;
	
	for (cit = scenegraphcontrollers.begin();!(cit==scenegraphcontrollers.end());++cit)
	{
		// controller replication is quite complicated
		// only replicate ipo controller for now

		SG_Controller* replicacontroller = (*cit)->GetReplica((SG_Node*)rootnode);
		if (replicacontroller)
		{
			replicacontroller->SetObject(rootnode);
			rootnode->AddSGController(replicacontroller);
		}
	}

	// replicate graphic controller
	if (orgobj->GetGraphicController())
	{
		PHY_IMotionState* motionstate = new KX_MotionState(chunk->GetSGNode());
		PHY_IGraphicController* newctrl = orgobj->GetGraphicController()->GetReplica(motionstate);
		newctrl->SetNewClientInfo(chunk->getClientInfo());
		chunk->SetGraphicController(newctrl);
	}

	// replicate physics controller
	if (orgobj->GetPhysicsController())
	{
		PHY_IMotionState* motionstate = new KX_MotionState(chunk->GetSGNode());
		PHY_IPhysicsController* newctrl = orgobj->GetPhysicsController()->GetReplica();

		KX_GameObject *parent = chunk->GetParent();
		PHY_IPhysicsController* parentctrl = (parent) ? parent->GetPhysicsController() : NULL;

		newctrl->SetNewClientInfo(chunk->getClientInfo());
		chunk->SetPhysicsController(newctrl, chunk->IsDynamic());
		newctrl->PostProcessReplica(motionstate, parentctrl);
	}

	Object* blenderobject = orgobj->GetBlenderObject();
	chunk->SetUserCollisionGroup(blenderobject->col_group);
	chunk->SetUserCollisionMask(blenderobject->col_mask);

	rootnode->SetLocalScale(MT_Vector3(1., 1., 1.));
	MT_Point2 pos2d = node->GetRealPos();

	rootnode->SetLocalPosition(MT_Point3(pos2d.x(), pos2d.y(), 0.));
	rootnode->SetLocalOrientation(MT_Matrix3x3(1., 0., 0.,
											   0., 1., 0.,
											   0., 0., 1.));

	rootnode->UpdateWorldData(0);
	rootnode->SetBBox(orgnode->BBox());
	rootnode->SetRadius(orgnode->Radius());
	chunk->NodeUpdateGS(0.0);

	chunk->ActivateGraphicController(true);
	////////////////////////// AJOUT DANS LA LISTE ///////////////////////////
	m_chunkList.push_back((KX_Chunk *)chunk->AddRef());

	scene->GetRootParentList()->Add(chunk->AddRef());
	chunk->Release();

	chunk->RemoveMeshes();

	chunk->SetCulled(false);
	chunk->UpdateBuckets(false);

	return chunk;
}

void KX_Terrain::RemoveChunk(KX_Chunk *chunk)
{
	for (unsigned int i = 0; i < m_chunkList.size(); ++i) {
		if (m_chunkList[i] == chunk) {
			m_chunkList.erase(m_chunkList.begin() + i);
			chunk->Release();
			break;
		}
	}

	m_euthanasyChunkList.push_back((KX_Chunk*)chunk->AddRef());
}

void KX_Terrain::ScheduleEuthanasyChunks()
{
	for (unsigned int i = 0; i < m_euthanasyChunkList.size(); ++i) {
		KX_Chunk *chunk = m_euthanasyChunkList[i];
		chunk->Release();
		if(KX_GetActiveScene()->GetRootParentList()->RemoveValue(chunk))
			chunk->Release();
	}
	m_euthanasyChunkList.clear();
}

void KX_Terrain::AddTerrainZoneMesh(KX_TerrainZoneMesh *zoneMesh)
{
	m_zoneMeshList.push_back(zoneMesh);

	const float maxheight = zoneMesh->GetMaxHeight();
	const float minheight = zoneMesh->GetMinHeight();

	if (m_maxHeight < maxheight)
		m_maxHeight = maxheight;
	if (m_minHeight > minheight)
		m_minHeight = minheight;

	if (maxheight < minheight) {
		std::cout << "Warning : min height greater than max height !" << std::endl;
	}
}