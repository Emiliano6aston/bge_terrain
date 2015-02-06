#include "KX_ChunkNode.h"
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
	m_level(level),
	m_terrain(terrain),
	m_culled(false),
	m_subChunks(NULL)
{
}

KX_ChunkNode::~KX_ChunkNode()
{
	if (m_subChunks)
		DestructSubChunks();
}

void KX_ChunkNode::ConstructSubChunks()
{
	m_finalNode--;
	m_finalNode += 4;
	m_subChunks = m_terrain->NewChunkNodeList(m_relativePosX, m_relativePosY, m_level);
}

void KX_ChunkNode::DestructSubChunks()
{
	m_finalNode -= 4;
	m_finalNode++;
	delete m_subChunks[0];
	delete m_subChunks[1];
	delete m_subChunks[2];
	delete m_subChunks[3];

	delete m_subChunks;
	m_subChunks = NULL;
}

void KX_ChunkNode::CalculateVisible(KX_Camera* cam)
{
	MarkCulled(cam); // on test si le chunk est visible
	if (!m_culled) // si oui on essai de créer des chunks et des les mettres à jour
	{
		if (NeedCreateSubChunks(cam)) // si on a besoin des sous chunks et qu'ils n'existent pas déjà
		{
			if (!m_subChunks)
				ConstructSubChunks();

			m_subChunks[0]->CalculateVisible(cam);
			m_subChunks[1]->CalculateVisible(cam);
			m_subChunks[2]->CalculateVisible(cam);
			m_subChunks[3]->CalculateVisible(cam);
		}
		else if (m_subChunks) // si on en a pas besoin et qu'ils existent
			DestructSubChunks();
	}
	else if (m_subChunks) // si le chunk est invisible
		DestructSubChunks();
}

void KX_ChunkNode::UpdateMesh()
{
	for (unsigned short i = 0; i < 4; ++i)
	{
		if (!m_subChunks[i]->IsCulled())
			m_subChunks[i]->UpdateMesh();
	}
}

void KX_ChunkNode::RenderMesh(RAS_IRasterizer* rasty)
{
	for (unsigned short i = 0; i < 4; ++i)
	{
		if (!m_subChunks[i]->IsCulled())
			m_subChunks[i]->RenderMesh(rasty);
	}
}

KX_ChunkNode* KX_ChunkNode::GetChunkRelativePosition(short x, short y)
{
	if((m_relativePosX - m_relativeSize) <= x && x <= (m_relativePosX + m_relativeSize) &&
		(m_relativePosY - m_relativeSize) <= y && y <= (m_relativePosY + m_relativeSize))
	{
		if (m_subChunks)
		{
			for (unsigned short i = 0; i < 4; ++i)
			{
				KX_ChunkNode* ret = m_subChunks[i]->GetChunkRelativePosition(x, y);
				if (ret)
					return ret;
			}
		}
		else
			return this;
	}
	return NULL;
}