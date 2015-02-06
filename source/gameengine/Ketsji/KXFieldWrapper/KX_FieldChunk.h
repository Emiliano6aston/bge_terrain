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

/** \file KX_FieldChunk.h
 *  \ingroup ketsji
 */

#ifndef __KX_FIELD_CHUNK_H__
#define __KX_FIELD_CHUNK_H__

#include "KX_FieldChunkBase.h"
#include <vector>
#include <list>

class KX_FieldPoint;
class KX_Field;
class KX_FieldChunkNode;
class KX_Camera;

struct KX_FieldChunkConstructInfo
{
	KX_Field* m_field;
	float m_height;
	float m_intensity;
};

typedef std::list<KX_FieldPoint*>::iterator pointit;

class KX_FieldChunk : public KX_FieldChunkBase
{
protected:
	KX_FieldChunkNode* m_node;

	bool m_invalid;

	KX_FieldChunkConstructInfo* m_constructInfo;

	std::vector<KX_FieldPoint*> m_inactivePointList; // tous les points resultant du quadrillage
	std::list<KX_FieldPoint*> m_activePointList; // tous les points en creation (fade) et les points crées
	std::list<KX_FieldPoint*> m_euthanasyPointList; // tous les points en suppression (fade)

	/// Quadrillage de tous les points de la zone.
	void QuadrillFieldPoints();

	/// Creations d'un certain nombre d'objets en fonction d'un reel de 0 a 1.
	void ConstructFieldPoints(float ratio, bool force);

public:
	KX_FieldChunk(KX_FieldChunkNode* node);
	virtual ~KX_FieldChunk();

	/**
	 * Mise à jour du pixel, on verifie si le pixel est toujours a la bonne distance,
	 * si il est visible. Si il est visible est a la bonne distance on créer un certain
	 * nombre d'objets en fonction de la distance par rapport à la caméra.
	 * \param cam La camera utilisé pour le frustum culling est la distance.
	 */
	void Update();
	unsigned int UpdatePointAnimations(float transparencyStep);

	/**
	 * Suppression de tous les point actifs du pixel donc de tous les objets.
	 * \param force Si est vrai alors tous les objets sont supprimé directement et si faux
	 * les objets effectuent en fondu avant leur suppresion.
	 */
	void DestructFieldPoints(bool force);

	/// fonction de debug de la boite ou du pixel
	/*void DrawDebugPixel(KX_Camera* cam) const;
	void DrawDebugBox(KX_Camera* cam) const;*/
};

#endif // __KX_FIELD_CHUNK_H__