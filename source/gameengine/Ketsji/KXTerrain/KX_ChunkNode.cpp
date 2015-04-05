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

unsigned int KX_ChunkNode::m_activeNode = 0;

KX_ChunkNode::KX_ChunkNode(int x, int y, 
						   unsigned short relativesize, 
						   unsigned short level,
						   KX_Terrain *terrain)
	:m_relativePos(Point2D(x, y)),
	m_relativeSize(relativesize),
	m_culled(false),
	m_level(level),
	m_nodeList(NULL),
	m_chunk(NULL),
	m_terrain(terrain)
{
	++m_activeNode;
	// la taille et sa moitié du chunk
	const float size = m_terrain->GetChunkSize();
	const float width = size / 2 * relativesize;

	// la taille maximale et minimale en hauteur de la boite de frustum culling
	const float maxHeight = m_terrain->GetMaxHeight();
	const float minHeight = m_terrain->GetMinHeight();

	// le rayon du chunk
	m_radius2NoGap = (width * width * 2);
	m_radius2Object = m_radius2NoGap + (width * width * 2);
	float gap = size * relativesize * 2;
	m_radius2Camera = m_radius2NoGap + (gap * gap);

	// la coordonnée reel du chunk
	const float realX = x * size;
	const float realY = y * size;
	m_realPos = MT_Point2(realX, realY);

	/* creation d'une boite temporaire maximale pour la creation
	 * recursive des noeuds. Celle ci sera redimensionner plus tard pour
	 * une meilleur optimization
	 */
	m_box[0] = MT_Point3(realX - width, realY - width, minHeight);
	m_box[1] = MT_Point3(realX + width, realY - width, minHeight);
	m_box[2] = MT_Point3(realX + width, realY + width, minHeight);
	m_box[3] = MT_Point3(realX - width, realY + width, minHeight);
	m_box[4] = MT_Point3(realX - width, realY - width, maxHeight);
	m_box[5] = MT_Point3(realX + width, realY - width, maxHeight);
	m_box[6] = MT_Point3(realX + width, realY + width, maxHeight);
	m_box[7] = MT_Point3(realX - width, realY + width, maxHeight);
}

KX_ChunkNode::~KX_ChunkNode()
{
	DestructNodes();
	DestructChunk();
	--m_activeNode;
}

void KX_ChunkNode::ConstructNodes()
{
	if (!m_nodeList) {
		m_nodeList = m_terrain->NewNodeList(m_relativePos.x, m_relativePos.y, m_level);
	}
}

void KX_ChunkNode::DestructNodes()
{
	if (m_nodeList) {
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
	if (!m_chunk) {
		m_chunk = m_terrain->AddChunk(this);
		m_chunk->AddRef();
	}
	else {
		m_chunk->SetVisible(true, false);
	}
}

void KX_ChunkNode::DestructChunk()
{
	if (m_chunk) {
		m_terrain->RemoveChunk(m_chunk);
		m_chunk->Release();
		m_chunk = NULL;
	}
}

void KX_ChunkNode::DisableChunkVisibility()
{
	if (m_chunk) {
		m_chunk->SetVisible(false, false);
	}
}

bool KX_ChunkNode::NeedCreateNodes(CListValue *objects, KX_Camera *cam) const
{
	bool needcreatenode = false;

	for (unsigned i = 0; i < objects->GetCount(); ++i) {
		KX_GameObject *object = (KX_GameObject *)objects->GetValue(i);

		bool iscamera = (object->GetGameObjectType() == SCA_IObject::OBJ_CAMERA);
		if ((!object->GetVisible() || 
			!object->GetPhysicsController()) && 
			!iscamera)
		{
			continue;
		}

		float distance2 = MT_Point3(m_realPos.x(), m_realPos.y(), 0.).distance2(object->NodeGetWorldPosition());
		distance2 -= iscamera ? m_radius2Camera : m_radius2Object;

		needcreatenode = (m_terrain->GetSubdivision(distance2, iscamera) > m_level);
		if (needcreatenode)
			break;
	}

	return needcreatenode;
}

bool KX_ChunkNode::InNode(CListValue *objects) const
{
	bool innode = false;

	for (unsigned i = 0; i < objects->GetCount(); ++i) {
		KX_GameObject *object = (KX_GameObject *)objects->GetValue(i);

		if ((!object->GetVisible() ||
			!object->GetPhysicsController()))
		{
			continue;
		}

		const float objdistance2 = MT_Point3(m_realPos.x(), m_realPos.y(), 0.).distance2(object->NodeGetWorldPosition()) - m_radius2NoGap;
		innode = (objdistance2 < 0.0);
		if (innode)
			break;
	}

	return innode;
}

void KX_ChunkNode::MarkCulled(KX_Camera* culledcam)
{
	m_culled = culledcam->BoxInsideFrustum(m_box) == KX_Camera::OUTSIDE;
}

void KX_ChunkNode::CalculateVisible(KX_Camera *culledcam, CListValue *objects)
{
	MarkCulled(culledcam); // on test si le chunk est visible

	// si le noeud est visible
	if (!m_culled) {
		// le noeud est a une distance suffisante d'un des objets dans la liste requise pour une subdivision
		if (NeedCreateNodes(objects, culledcam)) {
			// donc on subdivise les noeuds
			ConstructNodes();
			// et supprimons le chunk
			DestructChunk();

			// puis on fais la même chose avec nos nouveaux noeuds
			m_nodeList[0]->CalculateVisible(culledcam, objects);
			m_nodeList[1]->CalculateVisible(culledcam, objects);
			m_nodeList[2]->CalculateVisible(culledcam, objects);
			m_nodeList[3]->CalculateVisible(culledcam, objects);
		}
		// sinon si aucun des objets n'est assez près
		else {
			// on détruit les anciens noeuds
			DestructNodes();
			// et créons le chunk
			ConstructChunk();
		}
	}
	// si le noeud est invisible
	else {
		// si un des objets a sa position dans la zone recouverte par le noeud
		if (InNode(objects)) {
			if (m_level != m_terrain->GetMaxLevel())
			{
				// donc on subdivise les noeuds
				ConstructNodes();
				DestructChunk();

				// puis on fais la même chose avec nos nouveau noeuds
				m_nodeList[0]->CalculateVisible(culledcam, objects);
				m_nodeList[1]->CalculateVisible(culledcam, objects);
				m_nodeList[2]->CalculateVisible(culledcam, objects);
				m_nodeList[3]->CalculateVisible(culledcam, objects);
			}
			else {
				ConstructChunk();
				DestructNodes();
			}
		}
		// sinon si aucun objets ne se situent sur le noeud
		else {
			DestructNodes();
			DestructChunk();
		}
		DisableChunkVisibility();
	}
}

void KX_ChunkNode::ReCalculateBox(float max, float min)
{
	// redimensionnement de la boite
	m_box[0].z() = min;
	m_box[1].z() = min;
	m_box[2].z() = min;
	m_box[3].z() = min;
	m_box[4].z() = max;
	m_box[5].z() = max;
	m_box[6].z() = max;
	m_box[7].z() = max;
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