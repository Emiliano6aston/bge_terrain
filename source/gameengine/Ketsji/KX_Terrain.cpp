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
#include "KX_SG_NodeRelationships.h"
#include "SG_Controller.h"
#include "RAS_MeshObject.h"
#include "RAS_MaterialBucket.h"
#include "BL_BlenderDataConversion.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IGraphicController.h"
#include "PHY_IPhysicsController.h"
#include "KX_MotionState.h"
#include "BKE_object.h"
#include "ListValue.h"

#define DEBUG(msg) std::cout << "Debug : " << msg << std::endl
#define DEBUG_VAR(var) std::cout << #var << " : " << var << std::endl;

typedef std::map<KX_ChunkNode::Point2D, KX_Chunk*>::iterator chunkit;

static STR_String camname = "Camera";

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
	m_chunkList = 
}

KX_Terrain::~KX_Terrain()
{
	Destruct();
}

void KX_Terrain::Construct()
{
	DEBUG("Construct terrain");
	KX_GameObject* obj = (KX_GameObject*)KX_GetActiveScene()->GetInactiveList()->FindValue("Cube");
	if (!obj)
	{
		DEBUG("no obj");
		return;
	}
	DEBUG("org obj stats, physic controller : " << obj->GetPhysicsController());
	// le materiau uilisé pour le rendu
	m_bucket = obj->GetMesh(0)->GetMeshMaterial((unsigned int)0)->m_bucket;

	m_nodeTree = NewNodeList(0, 0, 1);
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

	for (chunkit it = m_chunkList.begin(); it != m_chunkList.end(); ++it)
	{
		if (it->second->Release() > 0)
		{
			DEBUG("Error : chunk ref count on free greater than 0");
		}
	}
}

void KX_Terrain::CalculateVisibleChunks(KX_Camera* culledcam)
{
	KX_Camera* campos = KX_GetActiveScene()->FindCamera(camname);

	if (!m_construct)
		Construct();

// 	double starttime = KX_GetActiveEngine()->GetRealTime();

	m_nodeTree[0]->CalculateVisible(culledcam, campos);
	m_nodeTree[1]->CalculateVisible(culledcam, campos);
	m_nodeTree[2]->CalculateVisible(culledcam, campos);
	m_nodeTree[3]->CalculateVisible(culledcam, campos);

// 	double endtime = KX_GetActiveEngine()->GetRealTime();
// 	DEBUG(__func__ << " spend " << endtime - starttime << " time");
// 	DEBUG(KX_Chunk::m_chunkActive << " active chunk");
// 	DEBUG(m_chunkList.size() << " chunk");
}
void KX_Terrain::UpdateChunksMeshes()
{
// 	double starttime = KX_GetActiveEngine()->GetRealTime();

	for (chunkit it = m_chunkList.begin(); it != m_chunkList.end(); ++it)
	{
		it->second->UpdateMesh();
	}

// 	double endtime = KX_GetActiveEngine()->GetRealTime();
// 	DEBUG(__func__ << " spend " << endtime - starttime << " time");
}

void KX_Terrain::RenderChunksMeshes(const MT_Transform& cameratrans, RAS_IRasterizer* rasty)
{
// 	double starttime = KX_GetActiveEngine()->GetRealTime();

	KX_Camera* cam = KX_GetActiveScene()->FindCamera(camname);
	// rendu du mesh
	for (chunkit it = m_chunkList.begin(); it != m_chunkList.end(); ++it)
	{
		KX_Chunk* chunk = it->second;
		chunk->RenderMesh(rasty, cam);
		chunk->SetCulled(false); // toujours faux
		chunk->UpdateBuckets(false);
	}
	ScheduleEuthanasyChunks();

// 	double endtime = KX_GetActiveEngine()->GetRealTime();
// 	DEBUG(__func__ << " spend " << endtime - starttime << " time");
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
KX_ChunkNode* KX_Terrain::GetNodeRelativePosition(int x, int y)
{

	return NULL;
};

KX_ChunkNode** KX_Terrain::NewNodeList(int x, int y, unsigned short level)
{
	KX_ChunkNode** nodeList = (KX_ChunkNode**)malloc(4 * sizeof(KX_ChunkNode*));

	// la taille relative d'un chunk, = 2 si le noeud et final
	unsigned short relativesize = m_width / level;
	// la largeur du chunk 
	unsigned short width = relativesize / 2;

	nodeList[0] = new KX_ChunkNode(x - width, y - width, relativesize, level * 2, this);
	nodeList[1] = new KX_ChunkNode(x + width, y - width, relativesize, level * 2, this);
	nodeList[2] = new KX_ChunkNode(x - width, y + width, relativesize, level * 2, this);
	nodeList[3] = new KX_ChunkNode(x + width, y + width, relativesize, level * 2, this);
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

	chunk->ActivateGraphicController(true);

	chunk->ReconstructMesh();

	////////////////////////// AJOUT DANS LA LISTE ///////////////////////////
	m_posToChunk[node->GetRelativePos()] = chunk;

	scene->GetRootParentList()->Add(chunk->AddRef());
	return chunk;
}

void KX_Terrain::RemoveChunk(KX_ChunkNode::Point2D pos)
{
	KX_Chunk* chunk = m_posToChunk[pos];
	m_posToChunk.erase(pos);
	chunk->Release();
	m_euthanasyChunkList.push_back(chunk);
}

void KX_Terrain::ScheduleEuthanasyChunks()
{
	for (unsigned int i = 0; i < m_euthanasyChunkList.size(); ++i)
	{
		KX_Chunk* chunk = m_euthanasyChunkList[i];
		if(KX_GetActiveScene()->GetRootParentList()->RemoveValue(chunk))
			chunk->Release();
	}
	m_euthanasyChunkList.clear();
}