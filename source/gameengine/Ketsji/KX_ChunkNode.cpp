#include "KX_ChunkNode.h"
#include "KX_Chunk.h"
#include "KX_Terrain.h"

#include "KX_Camera.h"
#include "KX_PythonInit.h"

#include "RAS_IRasterizer.h"

#include <stdio.h>

#define COLORED_PRINT(msg, color) std::cout << /*"\033[" << color << "m" <<*/ msg << /*"\033[30m" <<*/ std::endl;

#define DEBUG(msg) COLORED_PRINT("Debug : " << msg, 30);
#define WARNING(msg) COLORED_PRINT("Warning : " << msg, 33);
#define INFO(msg) COLORED_PRINT("Info : " << msg, 37);
#define ERROR(msg) COLORED_PRINT("Error : " << msg, 31);
#define SUCCES(msg) COLORED_PRINT("Succes : " << msg, 32);

#define DEBUGNOENDL(msg) std::cout << msg;

#define NEWVERT RAS_TexVert(MT_Point3(x, y, BLI_hnoise(60., x, y, 0.) * maxheight), \
												uvs_buffer, \
												MT_Vector4(1., 1., 1., 1.), \
												0, \
												MT_Vector3(0, 0, 1.), \
												true, 0) \

unsigned int KX_ChunkNode::m_finalNode = 0;

KX_ChunkNode::KX_ChunkNode(short x, short y, unsigned short relativesize, unsigned short level, KX_Terrain *terrain)
	:m_relativePosX(x),
	m_relativePosY(y),
	m_relativeSize(relativesize),
	m_culled(false),
	m_level(level),
	m_nodeList(NULL),
	m_chunk(NULL),
	m_terrain(terrain)
{
	// la taille et sa moitié du chunk
	const float size = m_terrain->GetChunkSize();
	const float width = size / 2 * relativesize;

	// la hauteur maximal du chunk
	const float maxheight = m_terrain->GetMaxHeight();

	// le rayon du chunk
	m_radius2 = (width * width) * 2;

	DEBUG("create new chunk node, pos : " << x << " " << y << ", level : " << level << ", size : " << relativesize);

	// la coordonnée reel du chunk
	const float realX = x * size;
	const float realY = y * size;
	m_realPos = MT_Point2(realX, realY);

	// creation de la boite utilisé pour le frustum culling
	m_box[0] = MT_Point3(realX - width, realY - width, 0.);
	m_box[1] = MT_Point3(realX + width, realY - width, 0.);
	m_box[2] = MT_Point3(realX + width, realY + width, 0.);
	m_box[3] = MT_Point3(realX - width, realY + width, 0.);
	m_box[4] = MT_Point3(realX - width, realY - width, maxheight);
	m_box[5] = MT_Point3(realX + width, realY - width, maxheight);
	m_box[6] = MT_Point3(realX + width, realY + width, maxheight);
	m_box[7] = MT_Point3(realX - width, realY + width, maxheight);
}

KX_ChunkNode::~KX_ChunkNode()
{
	DestructAllNodes();
	DestructChunk();
}

void KX_ChunkNode::ConstructAllNodes()
{
	if (!m_nodeList)
		m_nodeList = m_terrain->NewNodeList(m_relativePosX, m_relativePosY, m_level);
}

void KX_ChunkNode::DestructAllNodes()
{
	if (m_nodeList)
	{
		delete m_nodeList[0];
		delete m_nodeList[1];
		delete m_nodeList[2];
		delete m_nodeList[3];

		delete m_nodeList;
		m_nodeList = NULL;
	}
}

void KX_ChunkNode::ConstructChunk()
{
	if (!m_chunk)
	{
		m_chunk = m_terrain->AddChunk(this);
		m_chunk->AddRef();
	}
}

void KX_ChunkNode::DestructChunk()
{
	if (m_chunk)
	{
		m_terrain->RemoveChunk(m_chunk);
		m_chunk->Release();
		m_chunk = NULL;
	}
}

bool KX_ChunkNode::NeedCreateSubChunks(KX_Camera* cam) const
{
	const float distance2 = MT_Point3(m_realPos.x(), m_realPos.y(), 0.).distance2(cam->NodeGetWorldPosition()) - m_radius2;
	const unsigned short subdivision = m_terrain->GetSubdivision(distance2);
	return subdivision >= (m_level*2);
}

void KX_ChunkNode::MarkCulled(KX_Camera* cam)
{
	m_culled = cam->BoxInsideFrustum(m_box) == KX_Camera::OUTSIDE;
}

void KX_ChunkNode::CalculateVisible(KX_Camera* cam)
{
	MarkCulled(cam); // on test si le chunk est visible
	if (!m_culled) // si oui on essai de créer des chunks et des les mettres à jour
	{
		if (NeedCreateSubChunks(cam))
		{
			// si on a besoin des sous chunks et qu'ils n'existent pas déjà
			ConstructAllNodes();
			DestructChunk();

			m_nodeList[0]->CalculateVisible(cam);
			m_nodeList[1]->CalculateVisible(cam);
			m_nodeList[2]->CalculateVisible(cam);
			m_nodeList[3]->CalculateVisible(cam);
		}
		else // si on en a pas besoin et qu'ils existent
		{
			DestructAllNodes();
			ConstructChunk();
		}
	}
	else // si le chunk est invisible
	{
		DestructAllNodes();
		DestructChunk();
	}
}

KX_ChunkNode* KX_ChunkNode::GetNodeRelativePosition(short x, short y)
{
	if((m_relativePosX - m_relativeSize) <= x && x <= (m_relativePosX + m_relativeSize) &&
		(m_relativePosY - m_relativeSize) <= y && y <= (m_relativePosY + m_relativeSize))
	{
		if (m_nodeList)
		{
			for (unsigned short i = 0; i < 4; ++i)
			{
				KX_ChunkNode* ret = m_nodeList[i]->GetNodeRelativePosition(x, y);
				if (ret)
					return ret;
			}
		}
		else
			return this;
	}
	return NULL;
}