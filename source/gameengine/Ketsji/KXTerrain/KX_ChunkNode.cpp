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

#include "PHY_IPhysicsController.h"

#include <stdio.h>

#include "glew-mx.h"
#include "GPU_draw.h"
#include "GPU_material.h"

#define DEBUG(msg) std::cout << "Debug (" << __func__ << ", " << this << ") : " << msg << std::endl

unsigned int KX_ChunkNode::m_activeNode = 0;

KX_ChunkNode::KX_ChunkNode(KX_ChunkNode *parentNode,
						   int x, int y, 
						   unsigned short relativesize, 
						   unsigned short level,
						   KX_Terrain *terrain)
	:m_parentNode(parentNode),
	m_relativePos(Point2D(x, y)),
	m_relativeSize(relativesize),
	m_level(level),
	m_boxModified(false),
	m_culledState(KX_Camera::INSIDE),
	m_nodeList(NULL),
	m_chunk(NULL),
	m_terrain(terrain)
{
	++m_activeNode;

	// Met a zero les hauteurs de la boite de culling.
	ResetBoxHeight();

	// la taille et sa moitié du chunk
	const float size = m_terrain->GetChunkSize();
	const float width = size / 2.0f * relativesize;

	// la taille maximale et minimale en hauteur de la boite de frustum culling
	const float maxHeight = m_terrain->GetMaxHeight();
	const float minHeight = m_terrain->GetMinHeight();

	// le rayon du chunk
	m_radius2NoGap = (width * width * 2.0f);
	m_radius2Object = m_radius2NoGap + (width * width * 2.0f);
	float gap = size * relativesize * 2.0f;
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
		m_nodeList = m_terrain->NewNodeList(this, m_relativePos.x, m_relativePos.y, m_level);
	}
}

void KX_ChunkNode::DestructNodes()
{
	if (m_nodeList) {
		for (unsigned short i = 0; i < 4; ++i)
			delete m_nodeList[i];

		free(m_nodeList);
		m_nodeList = NULL;
	}
}

void KX_ChunkNode::ConstructChunk()
{
	if (!m_chunk) {
		m_chunk = m_terrain->AddChunk(this);
	}
	else {
		m_chunk->SetVisible(true);
	}
}

void KX_ChunkNode::DestructChunk()
{
	if (m_chunk) {
		m_terrain->RemoveChunk(m_chunk);
		m_chunk = NULL;
	}
}

void KX_ChunkNode::DisableChunkVisibility()
{
	if (m_chunk) {
		m_chunk->SetVisible(false);
	}
}

bool KX_ChunkNode::NeedCreateNodes(CListValue *objects, KX_Camera *culledcam) const
{
	bool needcreatenode = false;

	for (unsigned int i = 0; i < objects->GetCount(); ++i) {
		KX_GameObject *object = (KX_GameObject *)objects->GetValue(i);

		bool iscamera = (object->GetGameObjectType() == SCA_IObject::OBJ_CAMERA);
		/* Si l'objet n'est pas visible, n'utilise pas une forme physique, n'est pas
		 * dynamique ou est en pause.
		 */
		if (!object->GetVisible() || 
			!object->GetPhysicsController() ||
			!object->GetPhysicsController()->IsDynamic() || 
			object->GetPhysicsController()->IsSuspended())
		{
			// Si c'est une camera on test en plus si elle est active.
			if (!iscamera)
				continue;

			KX_Camera *cam = (KX_Camera *)object;
			if (cam != culledcam &&
				!cam->GetViewport())
			{
				continue;
			}
		}

		float distance2 = MT_Point3(m_realPos.x(), m_realPos.y(), (m_maxBoxHeight + m_minBoxHeight) / 2.0f).distance2(object->NodeGetWorldPosition());
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

	for (unsigned int i = 0; i < objects->GetCount(); ++i) {
		KX_GameObject *object = (KX_GameObject *)objects->GetValue(i);

		// Les même type de condition que dans NeedCreateNodes.
		if (!object->GetVisible() ||
			!object->GetPhysicsController() ||
			!object->GetPhysicsController()->IsDynamic() || 
			object->GetPhysicsController()->IsSuspended())
		{
			continue;
		}

		const float objdistance2 = MT_Point3(m_realPos.x(), m_realPos.y(), (m_maxBoxHeight + m_minBoxHeight) / 2.0f).distance2(object->NodeGetWorldPosition()) - m_radius2NoGap;
		innode = (objdistance2 < 0.0f);
		if (innode)
			break;
	}

	return innode;
}

void KX_ChunkNode::MarkCulled(KX_Camera* culledcam)
{
	if (m_parentNode) {
		if (m_parentNode->GetCulledState() == KX_Camera::INSIDE) {
			m_culledState = KX_Camera::INSIDE;
			return;
		}
		else if (m_parentNode->GetCulledState() == KX_Camera::OUTSIDE) {
			m_culledState = KX_Camera::OUTSIDE;
			return;
		}
	}
	m_culledState = culledcam->BoxInsideFrustum(m_box);
}

void KX_ChunkNode::CalculateVisible(KX_Camera *culledcam, CListValue *objects)
{
	ReConstructBox();
	MarkCulled(culledcam); // on test si le chunk est visible

	// si le noeud est visible
	if (m_culledState != KX_Camera::OUTSIDE) {
		// le noeud est a une distance suffisante d'un des objets dans la liste requise pour une subdivision
		if (NeedCreateNodes(objects, culledcam)) {
			// donc on subdivise les noeuds
			ConstructNodes();
			// et supprimons le chunk
			DestructChunk();

			// puis on fais la même chose avec nos nouveaux noeuds
			for (unsigned short i = 0; i < 4; ++i)
				m_nodeList[i]->CalculateVisible(culledcam, objects);
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
				for (unsigned short i = 0; i < 4; ++i)
					m_nodeList[i]->CalculateVisible(culledcam, objects);
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

	ResetBoxHeight();
}

bool KX_ChunkNode::IsShadowCameraVisible(KX_Camera *shadowcam)
{
	return shadowcam->BoxInsideFrustum(m_box) != KX_Camera::OUTSIDE;
}

void KX_ChunkNode::DrawDebugInfo(DEBUG_DRAW_MODE mode)
{
	if (mode == DEBUG_BOX) {
		glDisable(GL_CULL_FACE);
// 		GPU_set_material_alpha_blend(GPU_BLEND_ALPHA);

		glColor4f(1.0, 0.0, 0.0, 1.0);
		glBegin(GL_LINE_LOOP);
		for (unsigned int i = 0; i < 4; ++i)
			glVertex3f(m_box[i].x(), m_box[i].y(), m_box[i].z());
		glEnd();
		glBegin(GL_LINE_LOOP);
			glVertex3f(m_box[0].x(), m_box[0].y(), m_box[0].z());
			glVertex3f(m_box[2].x(), m_box[2].y(), m_box[2].z());
			glVertex3f(m_box[1].x(), m_box[1].y(), m_box[1].z());
			glVertex3f(m_box[3].x(), m_box[3].y(), m_box[3].z());
		glEnd();

		glColor4f(0.0, 1.0, 0.0, 1.0);
		glBegin(GL_LINE_LOOP);
		for (unsigned int i = 4; i < 8; ++i)
			glVertex3f(m_box[i].x(), m_box[i].y(), m_box[i].z());
		glEnd();
		glBegin(GL_LINE_LOOP);
			glVertex3f(m_box[4].x(), m_box[4].y(), m_box[4].z());
			glVertex3f(m_box[6].x(), m_box[6].y(), m_box[6].z());
			glVertex3f(m_box[5].x(), m_box[5].y(), m_box[5].z());
			glVertex3f(m_box[7].x(), m_box[7].y(), m_box[7].z());
		glEnd();
	}

	if (m_nodeList) {
		for (unsigned int i = 0; i < 4; ++i)
			m_nodeList[i]->DrawDebugInfo(mode);
	}
}

void KX_ChunkNode::ResetBoxHeight()
{
	m_maxBoxHeight = 0.0f;
	m_minBoxHeight = 0.0f;
	m_requestCreateBox = true;
}

void KX_ChunkNode::CheckBoxHeight(float max, float min)
{
	if (!m_requestCreateBox) {
		if (max > m_maxBoxHeight) {
			m_maxBoxHeight = max;
			m_boxModified = true;
		}
		if (min < m_minBoxHeight) {
			m_minBoxHeight = min;
			m_boxModified = true;
		}
	}
	else {
		m_maxBoxHeight = max;
		m_minBoxHeight = min;
		m_requestCreateBox = false;
	}

	if (m_parentNode)
		m_parentNode->CheckBoxHeight(max, min);

}

void KX_ChunkNode::ReConstructBox()
{
	if (m_boxModified) {
		m_boxModified = false;

		// redimensionnement de la boite
		for (unsigned int i = 0; i < 4; ++i)
			m_box[i].z() = m_minBoxHeight;
		for (unsigned int i = 4; i < 8; ++i)
			m_box[i].z() = m_maxBoxHeight;
	}
}


KX_ChunkNode *KX_ChunkNode::GetNodeRelativePosition(float x, float y)
{
	const unsigned short relativewidth = m_relativeSize / 2;

	if(m_culledState != KX_Camera::OUTSIDE &&
	  (m_relativePos.x - relativewidth) < x && x < (m_relativePos.x + relativewidth) &&
	  (m_relativePos.y - relativewidth) < y && y < (m_relativePos.y + relativewidth))
	{
		if (m_nodeList) {
			for (unsigned short i = 0; i < 4; ++i) {
				KX_ChunkNode *ret = m_nodeList[i]->GetNodeRelativePosition(x, y);
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

std::ostream &operator<< (std::ostream &stream, const KX_ChunkNode::Point2D &pos)
{
	stream << "(x : " << pos.x << ", y : " << pos.y << ")";
	return stream;
}

