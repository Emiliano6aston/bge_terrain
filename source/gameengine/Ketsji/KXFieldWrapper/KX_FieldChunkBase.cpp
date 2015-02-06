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

/** \file gameengine/Ketsji/KX_FieldChunk.cpp
 *  \ingroup ketsji
 */

#include "KX_FieldChunkBase.h"
#include "KX_FieldWrapper.h"
#include "KX_RayCast.h"
#include "KX_ClientObjectInfo.h"
#include "KX_PythonInit.h"
#include "KX_GameObject.h"

KX_FieldChunkBase::KX_FieldChunkBase()
	:m_pHitObject(NULL)
{
}

KX_FieldChunkBase::~KX_FieldChunkBase()
{
}

// fonctions pour le raycast
bool KX_FieldChunkBase::RayHit(KX_ClientObjectInfo *client, KX_RayCast *result, void * const data)
{
	KX_GameObject* hitKXObj = client->m_gameobject;
	if (hitKXObj == KX_GetFieldWrapper()->GetTerrain())
	{
		m_pHitObject = hitKXObj;
		return true;
	}
	return true;
}

bool KX_FieldChunkBase::NeedRayCast(KX_ClientObjectInfo *client)
{
	KX_GameObject* hitKXObj = client->m_gameobject;
	
	if (client->m_type > KX_ClientObjectInfo::ACTOR)
	{
		printf("Invalid client type %d found in ray casting\n", client->m_type);
		return false;
	}
	if (hitKXObj == KX_GetFieldWrapper()->GetTerrain())
	{
		return true;
	}
	return false;
}