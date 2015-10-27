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
#include "KX_ChunkCache.h"

#include "KX_Camera.h"
#include "KX_PythonInit.h"
#include "KX_Scene.h"

#include "RAS_IRasterizer.h"

#include "DNA_terrain_types.h"
#include "DNA_material_types.h"

#define DEBUG(msg) // std::cout << "Debug (" << __func__ << ", " << this << ") : " << msg << std::endl;

KX_Terrain::KX_Terrain(void *sgReplicationInfo,
					   SG_Callbacks callbacks,
					   RAS_MaterialBucket *bucket,
					   Material *material,
					   unsigned short maxLevel,
					   unsigned short minPhysicsLevel,
					   unsigned short vertexSubdivision,
					   unsigned short width,
					   float cameraMaxDistance,
					   float objectMaxDistance,
					   float chunkSize,
					   float marginFactor,
					   short debugMode,
					   unsigned short debugTimeFrame,
					   bool useCache,
					   unsigned short cacheRefreshTime)
	:KX_GameObject(sgReplicationInfo, callbacks),
	m_bucket(bucket),
	m_material(material),
	m_maxChunkLevel(maxLevel),
	m_minPhysicsLevel(minPhysicsLevel),
	m_vertexSubdivision(vertexSubdivision),
	m_width(width),
	m_cameraMaxDistance(cameraMaxDistance),
	m_objectMaxDistance(objectMaxDistance),
	m_chunkSize(chunkSize),
	m_marginFactor(marginFactor),
	m_debugMode(debugMode),
	m_debugTimeFrame(debugTimeFrame),
	m_construct(false),
	m_debugFrame(0),
	m_useCache(useCache),
	m_cacheRefreshTime(cacheRefreshTime),
	m_cacheFrame(0)
{
	SetName("Terrain");

	unsigned int realmaxlevel = 0;
	for (unsigned int i = 1; i < m_width; i *= 2) {
		++realmaxlevel;
	}

	if (m_maxChunkLevel > realmaxlevel) {
		std::cout << "Warning: wrong max chunk level, it should be : " << realmaxlevel << std::endl;
		m_maxChunkLevel = realmaxlevel;
	}

	m_chunkRootCache = new KX_ChunkRootCache(m_width * (POLY_COUNT / 2), this);
}

KX_Terrain::~KX_Terrain()
{
	Destruct();

	for (unsigned short i = 0; i < m_zoneMeshList.size(); ++i)
		delete m_zoneMeshList[i];
}

void KX_Terrain::Construct()
{
	DEBUG("Construct terrain");

	m_chunkRootCache->Construct();
	m_nodeTree = new KX_ChunkNode(NULL, 0, 0, m_width, 1, this);
	m_construct = true;
}

void KX_Terrain::Destruct()
{
	DEBUG("Destruct terrain");
	// Main node destruction
	if (m_nodeTree)
		delete m_nodeTree;

	m_chunkRootCache->Destruct();
	delete m_chunkRootCache;

	ScheduleEuthanasyChunks();
}

void KX_Terrain::CalculateVisibleChunks(KX_Camera* culledcam)
{
	if (!m_construct)
		Construct();

	CListValue *objects = KX_GetActiveScene()->GetObjectList();

	m_nodeTree->CalculateVisible(culledcam, objects);

	ScheduleEuthanasyChunks();
}

void KX_Terrain::UpdateChunksMeshes()
{
	for (KX_ChunkList::iterator it = m_chunkList.begin(); it != m_chunkList.end(); ++it) {
		(*it)->UpdateMesh();
	}

	for (KX_ChunkList::iterator it = m_chunkList.begin(); it != m_chunkList.end(); ++it) {
		(*it)->EndUpdateMesh();
	}

	if (m_debugMode & DEBUG_TIME) {
		if (m_debugFrame > m_debugTimeFrame) {
			KX_Chunk::PrintTime();
			KX_Chunk::ResetTime();
			m_debugFrame = 0;
		}
		++m_debugFrame;
	}

	if (m_useCache) {
		if (m_cacheFrame > m_cacheRefreshTime) {
			m_chunkRootCache->Refresh();
			m_cacheFrame = 0;
		}
		++m_cacheFrame;
	}
}

void KX_Terrain::RenderChunksMeshes(KX_Camera *cam, RAS_IRasterizer* rasty)
{
	// Mesh render
	for (KX_ChunkList::iterator it = m_chunkList.begin(); it != m_chunkList.end(); ++it) {
		KX_Chunk *chunk = *it;
		if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_SHADOW) {
			if (chunk->GetNode()->IsCameraVisible(cam) != KX_Camera::OUTSIDE)
				continue;
		}
		chunk->RenderMesh(rasty, cam);
	}
}

void KX_Terrain::DrawDebugNode()
{
	m_nodeTree->DrawDebugInfo(m_debugMode);
}

unsigned short KX_Terrain::GetSubdivision(float distance, bool iscamera) const
{
	// Objects don't need a subdivision as high as the one needed for the camera
	const float maxdistance = iscamera ? m_cameraMaxDistance : m_objectMaxDistance;
	const float interval = maxdistance / m_maxChunkLevel;
	for (unsigned short i = 0; i <= m_maxChunkLevel; ++i) {
		const float leveldistance = i * interval;
		if (distance < (interval * (i + 1))) {
			return m_maxChunkLevel - i;
		}
	}
	return 0;
}

KX_ChunkNode *KX_Terrain::GetNodeRelativePosition(float x, float y)
{
	KX_ChunkNode *node = m_nodeTree->GetNodeRelativePosition(x, y);

	return node;
}


VertexZoneInfo *KX_Terrain::GetVertexInfo(int x, int y) const
{
	VertexZoneInfo *vertexInfo;

	if (m_useCache) {
		vertexInfo = m_chunkRootCache->GetVertexZoneInfo(x, y);
		vertexInfo->AddRef();
	}
	else {
		vertexInfo = NewVertexInfo(x, y);
	}

	return vertexInfo;
}

VertexZoneInfo *KX_Terrain::NewVertexInfo(int x, int y) const
{
	VertexZoneInfo *info = new VertexZoneInfo();

	const float interval = m_chunkSize / POLY_COUNT * 2.0f;
	const float fx = x * interval;
	const float fy = y * interval;

	// set vertex 2d position
	info->pos[0] = fx;
	info->pos[1] = fy;

	info->m_uvs[0].x() = fx;
	info->m_uvs[0].y() = fy;

	for (unsigned short i = 0; i < m_zoneMeshList.size(); ++i)
		m_zoneMeshList[i]->GetVertexInfo(fx, fy, info);

	return info;
}

KX_ChunkNode **KX_Terrain::NewNodeList(KX_ChunkNode *parentNode, int x, int y, unsigned short level)
{
	KX_ChunkNode **nodeList = (KX_ChunkNode **)malloc(4 * sizeof(KX_ChunkNode *));

	// Relative chunk size, = 2 if the node is final
	const unsigned short relativesize = m_width / pow(2, level);
	// Chunk width
	const unsigned short width = relativesize / 2;

	nodeList[0] = new KX_ChunkNode(parentNode, x - width, y - width, relativesize, level + 1, this);
	nodeList[1] = new KX_ChunkNode(parentNode, x + width, y - width, relativesize, level + 1, this);
	nodeList[2] = new KX_ChunkNode(parentNode, x - width, y + width, relativesize, level + 1, this);
	nodeList[3] = new KX_ChunkNode(parentNode, x + width, y + width, relativesize, level + 1, this);

	return nodeList;
}

KX_Chunk* KX_Terrain::AddChunk(KX_ChunkNode* node)
{
	double starttime = KX_GetActiveEngine()->GetRealTime();

	KX_Chunk *chunk = new KX_Chunk(node, m_bucket);

	////////////////////////// ADDING TO LIST ///////////////////////////
	m_chunkList.push_back(chunk);

	double endtime = KX_GetActiveEngine()->GetRealTime();

	KX_Chunk::chunkCreationTime += endtime - starttime;

	return chunk;
}

/// Move chunk from active chunks list to a temporary list for its deletion.
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
}
