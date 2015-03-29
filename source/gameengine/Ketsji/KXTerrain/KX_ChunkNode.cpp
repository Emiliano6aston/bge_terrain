#include "KX_Terrain.h"
#include "KX_Chunk.h"

#include "KX_Camera.h"
#include "KX_PythonInit.h"

#include "RAS_IRasterizer.h"

#include <stdio.h>

#define COLORED_PRINT(msg, color) std::cout << /*"\033[" << color << "m" <<*/ msg << /*"\033[30m" <<*/ std::endl;

#define DEBUG(msg) std::cout << "Debug (" << __func__ << ", " << this << ") : " << msg << std::endl
#define WARNING(msg) COLORED_PRINT("Warning : " << msg, 33);
#define INFO(msg) COLORED_PRINT("Info : " << msg, 37);
#define ERROR(msg) COLORED_PRINT("Error : " << msg, 31);
#define SUCCES(msg) COLORED_PRINT("Succes : " << msg, 32);
#define DEBUG_VAR(var) std::cout << #var << " : " << var << std::endl;

#define DEBUGNOENDL(msg) std::cout << msg;

unsigned int KX_ChunkNode::m_finalNode = 0;

KX_ChunkNode::KX_ChunkNode(int x, int y, unsigned short relativesize, unsigned short level, KX_Terrain *terrain)
	:m_relativePos(Point2D(x, y)),
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
	float gap = size * relativesize * 2;
	m_radius2 = (width * width * 2) + (gap * gap);

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
		m_nodeList = m_terrain->NewNodeList(m_relativePos.x, m_relativePos.y, m_level);
}

void KX_ChunkNode::DestructAllNodes()
{
	if (m_nodeList)
	{
		delete m_nodeList[0];
		delete m_nodeList[1];
		delete m_nodeList[2];
		delete m_nodeList[3];

		free(m_nodeList);
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

bool KX_ChunkNode::NeedCreateSubChunks(KX_Camera* campos) const
{
	const float distance2 = MT_Point3(m_realPos.x(), m_realPos.y(), 0.).distance2(campos->NodeGetWorldPosition()) - m_radius2;
	const unsigned short subdivision = m_terrain->GetSubdivision(distance2);
	return subdivision >= (m_level*2);
}

void KX_ChunkNode::MarkCulled(KX_Camera* culledcam)
{
	m_culled = culledcam->BoxInsideFrustum(m_box) == KX_Camera::OUTSIDE;
}

void KX_ChunkNode::CalculateVisible(KX_Camera *culledcam, KX_Camera* campos)
{
	MarkCulled(culledcam); // on test si le chunk est visible
	if (!m_culled) // si oui on essai de créer des chunks et des les mettres à jour
	{
		if (NeedCreateSubChunks(campos))
		{
			// si on a besoin des sous chunks et qu'ils n'existent pas déjà
			ConstructAllNodes();
			DestructChunk();

			m_nodeList[0]->CalculateVisible(culledcam, campos);
			m_nodeList[1]->CalculateVisible(culledcam, campos);
			m_nodeList[2]->CalculateVisible(culledcam, campos);
			m_nodeList[3]->CalculateVisible(culledcam, campos);
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

KX_ChunkNode *KX_ChunkNode::GetNodeRelativePosition(const Point2D& pos)
{
	unsigned short relativewidth = m_relativeSize / 2;

	if(!m_culled &&
	   (m_relativePos.x - relativewidth) < pos.x && pos.x < (m_relativePos.x + relativewidth) &&
	   (m_relativePos.y - relativewidth) < pos.y && pos.y < (m_relativePos.y + relativewidth))
	{
		if (m_nodeList) {
			for (unsigned short i = 0; i < 4; ++i) {
				KX_ChunkNode *ret = m_nodeList[i]->GetNodeRelativePosition(pos);
				if (ret)
					return ret;
			}
		}
		else
			return this;
	}
	return NULL;
}

bool operator<(const KX_ChunkNode::Point2D& pos1, const KX_ChunkNode::Point2D& pos2)
{
	return pos1.x < pos2.x || (!(pos2.x < pos1.x) && pos1.y < pos2.y);
}