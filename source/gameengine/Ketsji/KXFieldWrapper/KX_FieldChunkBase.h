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

/** \file KX_FieldChunkBase.h
 *  \ingroup ketsji
 */

#ifndef __KX_FIELD_CHUNK_BASE_H__
#define __KX_FIELD_CHUNK_BASE_H__

#include "KX_PyMath.h"

class KX_GameObject;
class KX_ClientObjectInfo;
class KX_RayCast;

class KX_FieldChunkBase
{
protected:
	/// L'objet touché lors d'un lancé de rayons.
	KX_GameObject* m_pHitObject;

public:
	KX_FieldChunkBase();
	virtual ~KX_FieldChunkBase();

	/**
	 * Fonction appelé quand le lancé de rayons touche un objet, on verifie que l'objet est bien
	 * le terrain et si c'est le cas on assigne cet objet à m_pHitObject.
	 * \param client Representation de l'objet.
	 * \param result Toutes les données du lancé de rayons : position, normal mesh, polygone...
	 * \param data Des informations supplementaires.
	 */
	bool RayHit(KX_ClientObjectInfo *client, KX_RayCast *result, void * const data);

	/**
	 * Un test pour savoir si un objet peut potentiellement être touché, on teste si l'objet est 
	 * egal au terrain.
	 * \param client Classe contenant l'objet.
	 * \return Vrai si l'objet est le terrain.
	 */
	bool NeedRayCast(KX_ClientObjectInfo *client);
};

#endif // __KX_FIELD_CHUNK_BASE_H__