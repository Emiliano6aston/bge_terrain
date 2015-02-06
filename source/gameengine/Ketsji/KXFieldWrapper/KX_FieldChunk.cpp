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

/** \file gameengine/Ketsji/KXFieldWrapper/KX_FieldChunk.cpp
 *  \ingroup ketsji
 */

#include "KX_FieldChunk.h"
#include "KX_FieldChunkNode.h"
#include "KX_FieldPoint.h"
#include "KX_FieldWrapper.h"
#include "KX_Field.h"
#include "KX_Camera.h"
#include "KX_PythonInit.h"
#include "KX_RayCast.h"
#include "KX_FieldImage.h"

KX_FieldChunk::KX_FieldChunk(KX_FieldChunkNode* node)
	:m_node(node),
	m_invalid(false)
{
	// on initialise toutes les information utilisé pour la construction d'un point
	m_constructInfo = new KX_FieldChunkConstructInfo();

	KX_Field * field = node->GetField();
	m_constructInfo->m_field = field;

	short posX = node->GetRelativePosX();
	short posY = node->GetRelativePosY();

	m_constructInfo->m_height = field->GetHeight(posX, posY); // la hauteur du point
	m_constructInfo->m_intensity = field->GetIntensity(posX, posY); // la couleur du point
}

KX_FieldChunk::~KX_FieldChunk()
{
	delete m_constructInfo;

	DestructFieldPoints(true);
}

void KX_FieldChunk::QuadrillFieldPoints()
{
	KX_Field * field = m_node->GetField();
	KX_FieldWrapper* fieldWrapper = field->GetFieldWrapper();
	PHY_IPhysicsEnvironment* pe = KX_GetActiveScene()->GetPhysicsEnvironment();

	const float from = fieldWrapper->GetRayCastFrom();
	const float to = fieldWrapper->GetRayCastTo();
	const float gap = field->GetGap();
	const float margin = field->GetMargin();
	const float vardensity = field->GetDensityVariance();
	const MT_Point3 realPos = m_node->GetRealMiddlePosition();
	const float mioffset = field->GetPixelSize() * m_node->GetSize() / 2;

	const float startx = realPos.x() + gap + margin - mioffset;
	const float endx =	 realPos.x() - gap - margin + mioffset;
	const float starty = realPos.y() + gap + margin - mioffset;
	const float endy =	 realPos.y() - gap - margin + mioffset;

	const float diffx = endx - startx;
	const float density = diffx / roundf(diffx / field->GetDensity());
	const float midensity = density / 2;

	unsigned int probPointCount = (diffx / density) * (diffx / density);
	m_inactivePointList.reserve(probPointCount);

	for (float x = startx + midensity; x < endx; x += density)
	{
		for (float y = starty + midensity; y < endy; y += density)
		{
			const float px = x + rand() / (float)RAND_MAX * (vardensity * 2) - vardensity;
			const float py = y + rand() / (float)RAND_MAX * (vardensity * 2) - vardensity;

			KX_RayCast::Callback<KX_FieldChunk> callback(this, NULL, NULL, true);
			KX_RayCast::RayTest(pe, MT_Point3(px, py, from), MT_Point3(px, py, to), callback);

			if (m_pHitObject)
			{
				m_pHitObject = NULL;
				m_inactivePointList.push_back(field->NewFieldPoint(m_constructInfo, callback.m_hitPoint, callback.m_hitNormal));
			}
		}
	}
}

void KX_FieldChunk::Update()
{
	if (m_invalid)
		return;

	KX_Field * field = m_node->GetField();
	const float lastdist = m_node->GetLastDistanceToCamera();
	const float ratio = field->GetRatio(lastdist);
	ConstructFieldPoints(ratio, false);
}

unsigned int KX_FieldChunk::UpdatePointAnimations(float transparencyStep)
{
	const float windfactor = m_node->GetField()->GetWindFactor();

	for (pointit it = m_activePointList.begin(); it != m_activePointList.end(); ++it)
		(*it)->UpdateAnimations(transparencyStep, windfactor);

	for (pointit it = m_euthanasyPointList.begin(); it != m_euthanasyPointList.end();)
	{
		// si UpdateAnimations renvoie true cela signifie que le point vient de finir son fondu descendant il est alors inutile
		if ((*it)->UpdateAnimations(transparencyStep, windfactor))
			it = m_euthanasyPointList.erase(it);
		else
			++it;
	}
	return m_activePointList.size() + m_euthanasyPointList.size();
}

/* creation des points de la liste en fonction d'une densité de 0 a 1*/
void KX_FieldChunk::ConstructFieldPoints(float ratio, bool force)
{
	// on quadrille si besoin
	if (m_inactivePointList.empty())
		QuadrillFieldPoints();

	// le nombre de points inactifs donc le nombre maximum de points actifs possible
	const unsigned short inactivePointCount = m_inactivePointList.size();
	// le nombre actuel de points actifs
	const unsigned short activePointCount = m_activePointList.size();
	// le nombre de points total
	const short nbpoints = ratio * inactivePointCount;

	// si creation du meme nombre d'objets qu'avant
	if (nbpoints == activePointCount)
		return;

	// si creation de aucun objets
	if (nbpoints == 0)
	{
		DestructFieldPoints(false);
		return;
	}

	// pour l'ajout d'objets actifs a partir d'objets inactifs
	unsigned short i = 0;
	// creation d'un certain nombre d'objets entre 0 et le maximun
	const bool add = nbpoints > activePointCount; // ajout ou suppresion de points ?

	// on se reference a la liste des objets actifs
	while (m_activePointList.size() != nbpoints)
	{
		// si ajout d'objets
		if (add)
		{
			// on essaie d'abord de vider la liste des objets à tuer
			if (!m_euthanasyPointList.empty())
			{
				pointit it = m_euthanasyPointList.begin();
				// inversion du fondu (transparent => opaque)
				(*it)->ReverseFade();
				m_activePointList.push_back(*it);
				m_euthanasyPointList.pop_front();
			}
			// si la liste et vide alors on créer des objets actifs à partir d'objets inactifs
			else
			{
				i += rand() % m_inactivePointList.size();
				if (i >= inactivePointCount)
					i = 0;

				KX_FieldPoint* point = m_inactivePointList[i];
				if (!point->IsActive())
				{
					m_activePointList.push_back(point);
					point->Activate(force);
				}
			}
		}
		// si suppression d'objets
		else
		{
			pointit it = m_activePointList.begin();
			(*it)->Desactivate(false);
			m_euthanasyPointList.push_back(*it);
			m_activePointList.pop_front();
		}
	}
}

void KX_FieldChunk::DestructFieldPoints(bool force)
{
	if (m_invalid)
		return;

	for (pointit it = m_activePointList.begin(); it != m_activePointList.end(); ++it)
	{
		(*it)->Desactivate(force);
		if (!force)
			m_euthanasyPointList.push_back(*it);
	}
	m_activePointList.clear();

	if (force)
	{
		for (pointit it = m_euthanasyPointList.begin(); it != m_euthanasyPointList.end(); ++it)
			(*it)->Desactivate(true);
		m_euthanasyPointList.clear();

		for (size_t i = 0; i < m_inactivePointList.size(); ++i)
			delete m_inactivePointList[i];
		m_inactivePointList.clear();
	}
}
#if 0
void KX_FieldChunk::DrawDebugPixel() const
{
}

void KX_FieldChunk::DrawDebugBox() const
{
	if (m_invalid)
		return;

		MT_Vector3 color(0., 0., 0.);

		/* table de couleurs :
		 * 		rouge : hors de distance et aucun objets
		 * 		jaune : hors de distance et avec objets = ERREUR
		 * 		bleu : bonne distance et des objets actifs et a tuer
		 * 		vert : bonne distance et que des objets actifs
		 * 		bleu clair : bonne distance et que des objets a tuer
		 * 		blanc : bonne distance et aucun objets = INUTILE
		 */

		if (!m_in)
		{
			if (!m_activePointList.size() && !m_euthanasyPointList.size())
				color = MT_Vector3(1., 0., 0.);
			else if (m_activePointList.size() || m_euthanasyPointList.size())
				color = MT_Vector3(1., 1., 0.);
		}
		else
		{
			if (m_activePointList.size() && m_euthanasyPointList.size())
				color = MT_Vector3(0., 0., 1.);
			else if (m_activePointList.size() && !m_euthanasyPointList.size())
				color = MT_Vector3(0., 1., 0.);
			else if (!m_activePointList.size() && m_euthanasyPointList.size())
				color = MT_Vector3(0., 1., 1.);
			else if (!m_activePointList.size() && !m_euthanasyPointList.size())
				color = MT_Vector3(1., 1., 1.);
		}
		
		for (int i = 0; i < 4; i++)
		{
			KX_RasterizerDrawDebugLine(m_box[i], m_box[i + 4], color);
			KX_RasterizerDrawDebugLine(m_box[i], m_box[(i < 3) ? i + 1 : 0], color);
			KX_RasterizerDrawDebugLine(m_box[i + 4], m_box[(i < 3) ? i + 5 : 4], color);
		}
	}
}
#endif