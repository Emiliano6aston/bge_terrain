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

#include "SG_Node.h"

#include "RAS_IRasterizer.h"

#include "PHY_IPhysicsController.h"

#include <stdio.h>

#include "glew-mx.h"
#include "GPU_draw.h"
#include "GPU_material.h"

#define DEBUG(msg) std::cout << msg << std::endl;

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
	m_onConstruct(true),
	m_culledState(KX_Camera::INSIDE),
	m_nodeList(NULL),
	m_chunk(NULL),
	m_terrain(terrain)
{
	++m_activeNode;

	// Met a zero les hauteurs de la boite de culling.
	ResetFrustumBoxHeights();

	// la taille et sa moitié du chunk
	const float size = m_terrain->GetChunkSize();
	const float halfwidth = size * relativesize / 2.0f;

	// la taille maximale et minimale en hauteur de la boite de frustum culling
	const float defaultMaxHeight = m_terrain->GetMaxHeight();
	const float defaultMinHeight = m_terrain->GetMinHeight();

	// le rayon du chunk sqrt(x² + y²)
	m_radius = MT_Point3(halfwidth, halfwidth, 0.0f).length();
	/* Le décalage pour que le noeud parent se subdivise avant ses noeuds
	 * enfant evitant ainsi d'enorme création de chunk silmutanement.
	 */
	m_radiusGap = MT_Point3(size * relativesize, size * relativesize, 0.0f).length();

	// la coordonnée reel du chunk
	const float realX = x * size;
	const float realY = y * size;
	m_realPos = MT_Point2(realX, realY);

	/* creation d'une boite temporaire maximale pour la creation
	 * recursive des noeuds. Celle ci sera redimensionner plus tard pour
	 * une meilleur optimization
	 */
	MT_Point3 min(realX - halfwidth, realY - halfwidth, defaultMinHeight);
	MT_Point3 max(realX + halfwidth, realY + halfwidth, defaultMaxHeight);

	m_box[0] = min;
	m_box[1] = MT_Point3(min[0], min[1], max[2]);
	m_box[2] = MT_Point3(min[0], max[1], min[2]);
	m_box[3] = MT_Point3(min[0], max[1], max[2]);
	m_box[4] = MT_Point3(max[0], min[1], min[2]);
	m_box[5] = MT_Point3(max[0], min[1], max[2]);
	m_box[6] = MT_Point3(max[0], max[1], min[2]);
	m_box[7] = max;
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
			// Si ce n'est pas une camera on passe.
			if (!iscamera)
				continue;

			KX_Camera *cam = (KX_Camera *)object;
			// Si la camera n'est pas active on passe aussi.
			if (cam != culledcam && !cam->GetViewport()) {
				continue;
			}
		}

		const float objradius = object->GetSGNode()->Radius();
		float distance = GetCenter().distance(object->NodeGetWorldPosition()) - objradius;
		distance -= m_radius + (iscamera ? m_radiusGap * 2.0f : 2.0f);

		needcreatenode = (m_terrain->GetSubdivision(distance, iscamera) > m_level);
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

		// Les même type de conditions que dans NeedCreateNodes.
		if (!object->GetVisible() ||
			!object->GetPhysicsController() ||
			!object->GetPhysicsController()->IsDynamic() || 
			object->GetPhysicsController()->IsSuspended())
		{
			continue;
		}

		const float objradius = object->GetSGNode()->Radius();
		const float objdistance = GetCenter().distance(object->NodeGetWorldPosition()) - m_radius - objradius;
		innode = (objdistance < 0.0f);
		if (innode)
			break;
	}

	return innode;
}

void KX_ChunkNode::MarkCulled(KX_Camera* culledcam)
{
	/* Si ce noeud possède un parent on se fie à sont état si il est
	 * totalement a l'interieur ou à l'exterieur du champ de la caméra.
	 */
	if (m_parentNode) {
		switch (m_parentNode->GetCulledState()) {
			case KX_Camera::INSIDE:
				m_culledState = KX_Camera::INSIDE;
				return;
			case KX_Camera::OUTSIDE:
				m_culledState = KX_Camera::OUTSIDE;
				return;
		}
	}
	m_culledState = IsCameraVisible(culledcam);
}

MT_Point3 KX_ChunkNode::GetCenter() const
{
	float z = 0.0f;
	if (m_chunk) {
		z = (m_maxBoxHeight + m_minBoxHeight) / 2.0f;
	}
	else if (m_parentNode) {
		z = (m_parentNode->GetMaxBoxHeight() + m_parentNode->GetMinBoxHeight()) / 2.0f;
	}

	return MT_Point3(m_realPos.x(), m_realPos.y(), z);
}

short KX_ChunkNode::IsCameraVisible(KX_Camera *cam)
{
	/*if (!m_onConstruct) {
		const float radius = (m_box[7] - m_box[0]).length();
		short culledState = cam->SphereInsideFrustum(GetCenter(), radius);
		if (culledState != KX_Camera::INTERSECT)
			return culledState;
	}*/

	return cam->BoxInsideFrustum(m_box);
}

void KX_ChunkNode::CalculateVisible(KX_Camera *culledcam, CListValue *objects)
{
	/* On reconstruit la boite si elle est modifiée par une création
	 * de chunk à la dernière mise à jour.
	 */
	ReConstructFrustumBoxAndRadius();
	// On test si le chunk est visible.
	MarkCulled(culledcam);

	// Si le noeud est visible.
	if (m_culledState != KX_Camera::OUTSIDE) {
		/* Le noeud est a une distance suffisante d'un des objets dans 
		 * la liste requise pour une subdivision.
		 */
		if (NeedCreateNodes(objects, culledcam)) {
			// Donc on subdivise les noeuds.
			ConstructNodes();
			// Et supprimons le chunk.
			DestructChunk();

			// Puis on fais la même chose avec nos nouveaux noeuds.
			for (unsigned short i = 0; i < 4; ++i)
				m_nodeList[i]->CalculateVisible(culledcam, objects);
		}
		// Sinon si aucun des objets n'est assez près.
		else {
			// On détruit les anciens noeuds.
			DestructNodes();
			// Et créons le chunk.
			ConstructChunk();
		}
	}
	// Si le noeud est invisible.
	else {
		// Si un des objets a sa position dans la zone recouverte par le noeud.
		if (InNode(objects)) {
			/* Le seul moyen d'eviter de subdiviser à l'infinie les noeud.
			 * Pour le cas où le noeud est visible GetSubdivision fait cette condition.
			 */
			if (m_level != m_terrain->GetMaxLevel()) {
				// Donc on subdivise les noeuds.
				ConstructNodes();
				// Et detruisont le chunk.
				DestructChunk();

				// Puis on fais la même chose avec nos nouveau noeuds.
				for (unsigned short i = 0; i < 4; ++i)
					m_nodeList[i]->CalculateVisible(culledcam, objects);
			}
			else {
				ConstructChunk();
				DestructNodes();
			}
		}
		// Sinon si aucun objets ne se situent sur le noeud.
		else {
			DestructNodes();
			DestructChunk();
		}
		// Rend le chunk invisble au rendu.
		DisableChunkVisibility();
	}

	// Reinitialise les hauteurs de la boite de frustum culling.
	ResetFrustumBoxHeights();
	/* Le chunk n'est plus considére "en construction" et peut maintenant utiliser 
	 * une sphere pour le frustum culling.
	 */
	m_onConstruct = false;
}

void KX_ChunkNode::DrawDebugInfo(DEBUG_DRAW_MODE mode)
{
	if (mode == DEBUG_BOX/* && m_level == 9*/) {
		MT_Point3 nodepos3d(GetCenter());

		glBegin(GL_LINES);
			glColor4f(1.0, 0.0, 0.0, 1.0);
			glVertex3f(m_box[0].x(), m_box[0].y(), m_box[0].z());
			glVertex3f(m_box[1].x(), m_box[1].y(), m_box[1].z());
			glVertex3f(m_box[1].x(), m_box[1].y(), m_box[1].z());
			glVertex3f(m_box[3].x(), m_box[3].y(), m_box[3].z());
			glVertex3f(m_box[3].x(), m_box[3].y(), m_box[3].z());
			glVertex3f(m_box[2].x(), m_box[2].y(), m_box[2].z());
			glVertex3f(m_box[2].x(), m_box[2].y(), m_box[2].z());
			glVertex3f(m_box[0].x(), m_box[0].y(), m_box[0].z());

			glVertex3f(m_box[4].x(), m_box[4].y(), m_box[4].z());
			glVertex3f(m_box[5].x(), m_box[5].y(), m_box[5].z());
			glVertex3f(m_box[5].x(), m_box[5].y(), m_box[5].z());
			glVertex3f(m_box[7].x(), m_box[7].y(), m_box[7].z());
			glVertex3f(m_box[7].x(), m_box[7].y(), m_box[7].z());
			glVertex3f(m_box[6].x(), m_box[6].y(), m_box[6].z());
			glVertex3f(m_box[6].x(), m_box[6].y(), m_box[6].z());
			glVertex3f(m_box[4].x(), m_box[4].y(), m_box[4].z());

			glVertex3f(m_box[1].x(), m_box[1].y(), m_box[1].z());
			glVertex3f(m_box[5].x(), m_box[5].y(), m_box[5].z());
			glVertex3f(m_box[3].x(), m_box[3].y(), m_box[3].z());
			glVertex3f(m_box[7].x(), m_box[7].y(), m_box[7].z());
			glVertex3f(m_box[0].x(), m_box[0].y(), m_box[0].z());
			glVertex3f(m_box[4].x(), m_box[4].y(), m_box[4].z());
			glVertex3f(m_box[2].x(), m_box[2].y(), m_box[2].z());
			glVertex3f(m_box[6].x(), m_box[6].y(), m_box[6].z());

			glColor4f(1.0, 0.0, 0.0, 1.0);
			glVertex3f(nodepos3d.x(), nodepos3d.y(), nodepos3d.z());
			glVertex3f(nodepos3d.x() + 1.0, nodepos3d.y(), nodepos3d.z());
			glColor4f(0.0, 1.0, 0.0, 1.0);
			glVertex3f(nodepos3d.x(), nodepos3d.y(), nodepos3d.z());
			glVertex3f(nodepos3d.x(), nodepos3d.y() + 1.0, nodepos3d.z());
			glColor4f(0.0, 0.0, 1.0, 1.0);
			glVertex3f(nodepos3d.x(), nodepos3d.y(), nodepos3d.z());
			glVertex3f(nodepos3d.x(), nodepos3d.y(), nodepos3d.z() + 1.0);
		glEnd();

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		GPU_set_material_alpha_blend(GPU_BLEND_ALPHA);
		glBegin(GL_QUADS);
			if (m_culledState == KX_Camera::OUTSIDE) {
				glColor4f(0.0, 0.0, 0.0, 1.0);
			}
			else if (m_culledState == KX_Camera::INSIDE) {
				glColor4f(0.0, 1.0, 0.0, 0.01);
			}
			else {
				glColor4f(1.0, 0.0, 0.0, 0.01);
			}

			glVertex3f(m_box[0].x(), m_box[0].y(), m_box[0].z());
			glVertex3f(m_box[1].x(), m_box[1].y(), m_box[1].z());
			glVertex3f(m_box[3].x(), m_box[3].y(), m_box[3].z());
			glVertex3f(m_box[2].x(), m_box[2].y(), m_box[2].z());
			glVertex3f(m_box[4].x(), m_box[4].y(), m_box[4].z());
			glVertex3f(m_box[5].x(), m_box[5].y(), m_box[5].z());
			glVertex3f(m_box[7].x(), m_box[7].y(), m_box[7].z());
			glVertex3f(m_box[6].x(), m_box[6].y(), m_box[6].z());

			glVertex3f(m_box[0].x(), m_box[0].y(), m_box[0].z());
			glVertex3f(m_box[1].x(), m_box[1].y(), m_box[1].z());
			glVertex3f(m_box[5].x(), m_box[5].y(), m_box[5].z());
			glVertex3f(m_box[4].x(), m_box[4].y(), m_box[4].z());
			glVertex3f(m_box[6].x(), m_box[6].y(), m_box[6].z());
			glVertex3f(m_box[2].x(), m_box[2].y(), m_box[2].z());
			glVertex3f(m_box[3].x(), m_box[3].y(), m_box[3].z());
			glVertex3f(m_box[7].x(), m_box[7].y(), m_box[7].z());

			glVertex3f(m_box[5].x(), m_box[5].y(), m_box[5].z());
			glVertex3f(m_box[1].x(), m_box[1].y(), m_box[1].z());
			glVertex3f(m_box[3].x(), m_box[3].y(), m_box[3].z());
			glVertex3f(m_box[7].x(), m_box[7].y(), m_box[7].z());
			glVertex3f(m_box[4].x(), m_box[4].y(), m_box[4].z());
			glVertex3f(m_box[0].x(), m_box[0].y(), m_box[0].z());
			glVertex3f(m_box[2].x(), m_box[2].y(), m_box[2].z());
			glVertex3f(m_box[6].x(), m_box[6].y(), m_box[6].z());
		glEnd();
	}

	if (m_nodeList) {
		for (unsigned int i = 0; i < 4; ++i)
			m_nodeList[i]->DrawDebugInfo(mode);
	}
}

void KX_ChunkNode::ResetFrustumBoxHeights()
{
	m_maxBoxHeight = 0.0f;
	m_minBoxHeight = 0.0f;
	m_requestCreateBox = true;
}

void KX_ChunkNode::ExtendFrustumBoxHeights(float max, float min)
{
	const float invertlevel = 1.0f - ((float)m_level - 1) / (m_terrain->GetMaxLevel() - 1);

	const float defaultMaxHeight = m_terrain->GetMaxHeight();
	const float defaultMinHeight = m_terrain->GetMinHeight();

	const float correctmax = max + (defaultMaxHeight - max) * invertlevel;
	const float correctmin = min + (defaultMinHeight - min) * invertlevel;

	if (m_requestCreateBox) {
		m_maxBoxHeight = correctmax;
		m_minBoxHeight = correctmin;
		m_boxModified = true;
		m_requestCreateBox = false;
	}
	else {
		if (correctmax > m_maxBoxHeight) {
			m_maxBoxHeight = correctmax;
			m_boxModified = true;
		}
		if (correctmin < m_minBoxHeight) {
			m_minBoxHeight = correctmin;
			m_boxModified = true;
		}
	}

	if (m_parentNode) {
		m_parentNode->ExtendFrustumBoxHeights(max, min);
	}
}

void KX_ChunkNode::ReConstructFrustumBoxAndRadius()
{
	if (m_boxModified) {
		m_boxModified = false;

		const float margin = 0.0f; //(m_maxBoxHeight - m_minBoxHeight);
		// Redimensionnement de la boite.
		for (unsigned int i = 0; i < 7; i += 2)
			m_box[i].z() = m_minBoxHeight - margin;
		for (unsigned int i = 1; i < 8; i += 2)
			m_box[i].z() = m_maxBoxHeight + margin;
	}
}


KX_ChunkNode *KX_ChunkNode::GetNodeRelativePosition(float x, float y)
{
	// La motié de la largeur.
	const unsigned short relativewidth = m_relativeSize / 2;

	/* Si le noeud et invisble on considére qu'il ne doit pas être utilisable
	 * Cela arrvie que pour les chunk physique des objets cachés.
	 */
	if(m_culledState != KX_Camera::OUTSIDE &&
	  (m_relativePos.x - relativewidth) < x && x < (m_relativePos.x + relativewidth) &&
	  (m_relativePos.y - relativewidth) < y && y < (m_relativePos.y + relativewidth))
	{
		if (m_nodeList) {
			for (unsigned short i = 0; i < 4; ++i) {
				KX_ChunkNode *ret = m_nodeList[i]->GetNodeRelativePosition(x, y);
				if (ret) {
					return ret;
				}
			}
		}
		else {
			return this;
		}
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

