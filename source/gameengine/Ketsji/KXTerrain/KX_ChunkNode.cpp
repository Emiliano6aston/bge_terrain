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

#include "DNA_terrain_types.h"

#include <stdio.h>

#include "glew-mx.h"
#include "GPU_draw.h"
#include "GPU_material.h"

#include "BLI_utildefines.h"
#include "BLI_math_base.h"

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
	m_boxModified(true),
	m_onConstruct(true),
	m_onConstructSubNodes(0),
	m_culledState(KX_Camera::INSIDE),
	m_nodeList(NULL),
	m_chunk(NULL),
	m_terrain(terrain)
{
	++m_activeNode;

	// la taille et sa moitié du chunk
	const float size = m_terrain->GetChunkSize();
	const float halfwidth = size * relativesize / 2.0f;

	// le rayon du chunk sqrt(x² + y²)
	m_radius = MT_Point3(halfwidth, halfwidth, 0.0f).length();
	/* Le décalage pour que le noeud parent se subdivise avant ses noeuds
	 * enfant evitant ainsi d'enorme création de chunk silmutanement.
	 */
	m_radiusMargin = MT_Point3(size * relativesize, size * relativesize, 0.0f).length();

	// la coordonnée reel du chunk
	const float realX = x * size;
	const float realY = y * size;
	m_realPos = MT_Point2(realX, realY);

	// Creation d'une boite aplatit.
	m_box[0] = MT_Point3(realX - halfwidth, realY - halfwidth, 0.0f);
	m_box[1] = MT_Point3(realX - halfwidth, realY - halfwidth, 0.0f);
	m_box[2] = MT_Point3(realX - halfwidth, realY + halfwidth, 0.0f);
	m_box[3] = MT_Point3(realX - halfwidth, realY + halfwidth, 0.0f);
	m_box[4] = MT_Point3(realX + halfwidth, realY - halfwidth, 0.0f);
	m_box[5] = MT_Point3(realX + halfwidth, realY - halfwidth, 0.0f);
	m_box[6] = MT_Point3(realX + halfwidth, realY + halfwidth, 0.0f);
	m_box[7] = MT_Point3(realX + halfwidth, realY + halfwidth, 0.0f);

	/* On calcule une boite de visibilité temporaire en procédant
	 * a un echantillonnage de 5 vertice (coins + centre).
	 */
	GetFrustumBoxHeightsSampling();
	// Puis on étire notre boite avec une certaine marge.
	ReConstructFrustumBoxAndRadius();

	// Initialisation du proxy, on ne fait pas de AddRef car m_refCount est a 1 par defaut.
	m_proxy = new KX_ChunkNodeProxy(this);
}

KX_ChunkNode::~KX_ChunkNode()
{
	DestructNodes();
	DestructChunk();
	--m_activeNode;

	// Le noeud est modifié, car c'est une desubdivision du noeud parent.
	m_proxy->SetModified(true);
	m_proxy->Release();
}

void KX_ChunkNode::ConstructNodes()
{
	if (!m_nodeList) {
		m_onConstructSubNodes = 2;
		m_nodeList = m_terrain->NewNodeList(this, m_relativePos.x, m_relativePos.y, m_level);

		// Subdivision : le noeud est modifié.
		m_proxy->SetModified(true);
	}
}

void KX_ChunkNode::DestructNodes()
{
	if (m_terrain->GetDebugMode() & DEBUG_WARNINGS && m_onConstructSubNodes == 1) {
		DEBUG("Warning : destruct sub nodes just after construction");
	}

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
		distance -= m_radius + (iscamera ? m_radiusMargin * m_terrain->GetMarginFactor() : m_radius);

		unsigned short newlevel = m_terrain->GetSubdivision(distance, iscamera);

		needcreatenode = (newlevel > m_level);
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
	if (m_parentNode && m_parentNode->GetCulledState() != KX_Camera::INTERSECT) {
		m_culledState = m_parentNode->GetCulledState();
		return;
	}
	m_culledState = IsCameraVisible(culledcam);
}

MT_Point3 KX_ChunkNode::GetCenter() const
{
	return MT_Point3(m_realPos.x(), m_realPos.y(), (m_maxBoxHeight + m_minBoxHeight) / 2.0f);
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
	// Renitialisation du proxy, on annule l'état modifié (si vrai).
	m_proxy->SetModified(false);

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

	/* Le chunk n'est plus considére "en construction" et peut maintenant utiliser 
	 * une sphere pour le frustum culling.
	 */
	m_onConstruct = false;

	if (m_onConstructSubNodes > 0) {
		--m_onConstructSubNodes;
	}
}

void KX_ChunkNode::DrawDebugInfo(short mode)
{
	if (mode == 0) {
		return;
	}

	MT_Point3 nodepos3d(GetCenter());

	glBegin(GL_LINES);
		if (mode & DEBUG_DRAW_LINES) {
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
		}

		if (mode & DEBUG_DRAW_CENTERS) {
			glColor4f(1.0, 0.0, 0.0, 1.0);
			glVertex3f(nodepos3d.x(), nodepos3d.y(), nodepos3d.z());
			glVertex3f(nodepos3d.x() + 1.0, nodepos3d.y(), nodepos3d.z());
			glColor4f(0.0, 1.0, 0.0, 1.0);
			glVertex3f(nodepos3d.x(), nodepos3d.y(), nodepos3d.z());
			glVertex3f(nodepos3d.x(), nodepos3d.y() + 1.0, nodepos3d.z());
			glColor4f(0.0, 0.0, 1.0, 1.0);
			glVertex3f(nodepos3d.x(), nodepos3d.y(), nodepos3d.z());
			glVertex3f(nodepos3d.x(), nodepos3d.y(), nodepos3d.z() + 1.0);
		}
	glEnd();

	if (mode & DEBUG_DRAW_BOXES) {
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		GPU_set_material_alpha_blend(GPU_BLEND_ALPHA);
		glBegin(GL_QUADS);
			if (m_culledState == KX_Camera::OUTSIDE) {
				glColor4f(0.0, 0.0, 0.0, 1.0);
			}
			else if (m_culledState == KX_Camera::INSIDE) {
				glColor4f(0.0, 1.0, 0.0, 0.1);
			}
			else {
				glColor4f(1.0, 0.0, 0.0, 0.1);
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

void KX_ChunkNode::ReConstructFrustumBoxAndRadius()
{
	if (m_boxModified) {
		m_boxModified = false;

		const float factor = 1.0f - ((float)m_level - 1) / (m_terrain->GetMaxLevel() - 1);
		const float margin = (m_maxBoxHeight - m_minBoxHeight) * factor;

		// Redimensionnement de la boite.
		for (unsigned int i = 0; i < 8; i += 2) {
			m_box[i].z() = m_minBoxHeight - margin;
			m_box[i + 1].z() = m_maxBoxHeight + margin;
		}
	}
}

void KX_ChunkNode::ExtendFrustumBoxHeights(float max, float min)
{
	if (max > m_maxBoxHeight) {
		m_maxBoxHeight = max;
		m_boxModified = true;
	}
	else if (min < m_minBoxHeight) {
		m_minBoxHeight = min;
		m_boxModified = true;
	}

	if (m_parentNode) {
		m_parentNode->ExtendFrustumBoxHeights(max, min);
	}
}

void KX_ChunkNode::GetFrustumBoxHeightsSampling()
{
	static const short relativeVertexesPos[5][2] = {
		{-POLY_COUNT / 2, -POLY_COUNT / 2}, // bas-gauche
		{ POLY_COUNT / 2, -POLY_COUNT / 2}, // bas-droite
		{-POLY_COUNT / 2,  POLY_COUNT / 2}, // haut-gauche
		{ POLY_COUNT / 2,  POLY_COUNT / 2}, // haut-droite
		{ 0,               0} // centre
	};

	// la motie de la largeur du chunk
	const unsigned short halfrelativesize = m_relativeSize / 2;

	/* Le facteur pour passer de la position d'un noeud à celle d'un vertice absolue,
	 * 2 = la taille minimun d'un noeud.
	 */
	const unsigned short scale = POLY_COUNT / 2;

	for (unsigned short i = 0; i < 5; ++i) {
		// le bas du chunk par rapport au terrain * 2 pour les vertices
		const int x = m_relativePos.x * scale + relativeVertexesPos[i][0] * halfrelativesize;
		const int y = m_relativePos.y * scale + relativeVertexesPos[i][1] * halfrelativesize;

		VertexZoneInfo *info = m_terrain->GetVertexInfo(x, y);
		const float height = info->height;

		if (i == 0) {
			m_maxBoxHeight = m_minBoxHeight = height;
		}
		else {
			m_minBoxHeight = min_ff(m_minBoxHeight, height);
			m_maxBoxHeight = max_ff(m_maxBoxHeight, height);
		}

		info->Release();
	}
}

KX_ChunkNode *KX_ChunkNode::GetNodeRelativePosition(float x, float y)
{
	// La motié de la largeur.
	const unsigned short relativewidth = m_relativeSize / 2;

	/* Si le noeud est invisble on ne doit pas l'utiliser car il peut y
	 * avoir de grandes differences de niveau entre un noeud et son noeud
	 * adjacent invisible, ce qui peut causer des problèmes lors de la
	 * création des jointures d'un chunk.
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

KX_ChunkNode *KX_ChunkNode::GetAdjacentParentNode(short x, short y) const
{
	const unsigned short halfrelativesize = m_relativeSize / 2;
	const unsigned int halfterrainwidth = m_terrain->GetWidth() / 2;

	KX_ChunkNode *parent = m_parentNode;

	if (x != 0) {
		int nodelevelposX = (m_relativePos.x - halfrelativesize + halfterrainwidth) / 2;
		if (x == -1) {
			nodelevelposX -= halfrelativesize;
		}

		if (nodelevelposX < 0 || nodelevelposX > (halfterrainwidth - m_relativeSize)) {
			return NULL;
		}

		unsigned short size = m_relativeSize;

		// Tant que non ne vas pas au dessus du noeud tronc.
		while (parent) {
			// Si i % s == s - 1 ça signifie qu'il faut passer au niveau superieur.
			if ((nodelevelposX % size) == (size - halfrelativesize) && parent->GetParentNode()) {
				// On accede au parent du dernier noeud parent.
				parent = parent->GetParentNode();
				size *= 2;
			}
			else {
				break;
			}
		}
	}
	else if (y != 0) {
		int nodelevelposY = (m_relativePos.y - halfrelativesize + halfterrainwidth) / 2;
		if (y == -1) {
			nodelevelposY -= halfrelativesize;
		}

		if (nodelevelposY < 0 || nodelevelposY > (halfterrainwidth - m_relativeSize)) {
			return NULL;
		}

		unsigned short size = m_relativeSize;

		// Tant que non ne vas pas au dessus du noeud tronc.
		while (parent) {
			// Si i % s == s - 1 ça signifie qu'il faut passer au niveau superieur.
			if ((nodelevelposY % size) == (size - halfrelativesize) && parent->GetParentNode()) {
				// On accede au parent du dernier noeud parent.
				parent = parent->GetParentNode();
				size *= 2;
			}
			else {
				break;
			}
		}
	}

	return parent;
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

KX_ChunkNodeProxy::KX_ChunkNodeProxy(KX_ChunkNode *node)
	:m_refCount(1),
	m_modified(false),
	m_node(node)
{
}

KX_ChunkNodeProxy::~KX_ChunkNodeProxy()
{
}

