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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Porteries Tristan
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KXFieldWrapper/KX_FieldChunkNode.cpp
 *  \ingroup ketsji
 */

#include "KX_FieldChunkNode.h"
#include "KX_FieldChunk.h"
#include "KX_FieldWrapper.h"
#include "KX_FieldImage.h"
#include "KX_Field.h"
#include "KX_Camera.h"
#include "KX_PythonInit.h"
#include "KX_RayCast.h"

#define DEBUG(msg) std::cout << "Debug : " << msg << std::endl

unsigned int KX_FieldChunkNode::activeChunkNodes = 0;

KX_FieldChunkNode::KX_FieldChunkNode(KX_Field* field,
										short posX,
										short posY,
										unsigned short size)
	:m_field(field),
	m_posX(posX),
	m_posY(posY),
	m_size(size),
	m_lastVisible(true),
	m_lastIn(false),
	m_nodeList(NULL),
	m_chunk(NULL)
{
	float weigth = (size * field->GetPixelSize() / 2) + field->GetMargin();
	float radius2 = (weigth * weigth) * 2;
	m_radius = sqrt(radius2);
	activeChunkNodes++;
	GenerateMiddlePixelPoint(MT_Vector2(posX + size / 2, posY + size / 2) * field->GetPixelSize());
}

KX_FieldChunkNode::~KX_FieldChunkNode()
{
	activeChunkNodes--;
	DestructNodeListChunk();
}

void KX_FieldChunkNode::GenerateMiddlePixelPoint(MT_Vector2 middle)
{
	KX_FieldWrapper* fieldWrapper = KX_GetFieldWrapper();

	PHY_IPhysicsEnvironment* pe = KX_GetActiveScene()->GetPhysicsEnvironment();
	KX_RayCast::Callback<KX_FieldChunkNode> callback(this, NULL);

	KX_RayCast::RayTest(pe, MT_Point3(middle.x(), middle.y(), fieldWrapper->GetRayCastFrom()),
						MT_Point3(middle.x(), middle.y(), fieldWrapper->GetRayCastTo()), callback);

	if (m_pHitObject)
	{
		m_pHitObject = NULL;
		m_realMiddlePos = callback.m_hitPoint;
	}
	else
		m_realMiddlePos = MT_Point3(middle.x(), middle.y(), 0.);
}

void KX_FieldChunkNode::ConstructNodeListChunk()
{
	if (m_size == m_field->GetPixelZoneSizeMin()) // le noeud a la taille la plus petite
		ConstructChunk();
	else
		ConstructNodeList();
}

void KX_FieldChunkNode::DestructNodeListChunk()
{
	DestructChunk();
	DestructNodeList();
}

void KX_FieldChunkNode::ConstructNodeList()
{
	if (m_nodeList)
		return;

	m_nodeList = (KX_FieldChunkNode**)malloc(4 * sizeof(KX_FieldChunkNode*));
	unsigned short index = 0;

	for (unsigned short y = 0; y < m_size; y += (m_size / 2))
	{
		for (unsigned short x = 0; x < m_size; x += (m_size / 2))
			m_nodeList[index++] = new KX_FieldChunkNode(m_field, m_posX + x, m_posY + y, m_size / 2);
	}
}

void KX_FieldChunkNode::DestructNodeList()
{
	if (!m_nodeList)
		return;

	for (unsigned short i = 0; i < 4; ++i)
		delete m_nodeList[i];

	delete m_nodeList;
	m_nodeList = NULL;
}

void KX_FieldChunkNode::ConstructChunk()
{
	if (m_chunk)
		return;

	m_chunk = new KX_FieldChunk(this);
	m_field->AddFieldChunk(m_chunk);
}

void KX_FieldChunkNode::DestructChunk()
{
	if (!m_chunk)
		return;

	m_field->RemoveFieldChunk(m_chunk);
	delete m_chunk;
	m_chunk = NULL;
}

bool KX_FieldChunkNode::MarkVisible(KX_Camera* cam) const
{
	return cam->SphereInsideFrustum(m_realMiddlePos, m_radius) != KX_Camera::OUTSIDE;
}

void KX_FieldChunkNode::Update(KX_Camera* cam)
{
	const float dist = cam->NodeGetWorldPosition().distance2(m_realMiddlePos) - m_radius * m_radius;
	const bool in = dist <= m_field->GetMaxDistance();

	// le pixel et entierement dans la distance de la camera
	if (in)
	{
		// le noeud est il actuelement visible ?
		const bool visible = MarkVisible(cam);
		if (visible)
		{
			ConstructNodeListChunk();
			UpdateAllNodes(cam);
		}
		else if (m_lastVisible)
		{
			DestructNodeListChunk();
		}

		m_lastVisible = visible;
	}
	else if (m_lastIn)
		DestructNodeListChunk();

	m_lastIn = in;
	m_lastDistanceToCamera = dist;
}

void KX_FieldChunkNode::UpdateAllNodes(KX_Camera* cam)
{
	if (!m_nodeList)
		return;

	for (unsigned short i = 0; i < 4; ++i)
		m_nodeList[i]->Update(cam);
}

/*
void KX_FieldChunkNode::DrawDebugPixel(KX_Camera* cam) const
{
}*/

void KX_FieldChunkNode::DrawDebugBox(KX_Camera* cam) const
{
	bool visible = true;
	if (!m_lastIn)
		visible = MarkVisible(cam);

	if (visible)
	{
		if (!m_nodeList)
		{
			KX_RasterizerDrawDebugLine(m_realMiddlePos,
									   m_realMiddlePos + MT_Point3(0., 0., 1.),
									   MT_Vector3(1., 0., 0.));

			KX_RasterizerDrawDebugCircle(m_realMiddlePos,
										 m_radius, 
										 MT_Vector3(1., 1., 1.),
										 MT_Vector3(0., 0., 1.),
										 32);
		}
		else
		{
			for (unsigned short i = 0; i < 4; ++i)
				m_nodeList[i]->DrawDebugBox(cam);
		}
	}
}