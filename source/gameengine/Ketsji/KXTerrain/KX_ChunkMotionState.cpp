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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Porteries Tristan, Gros Alexis. For the 
 * Uchronia project (2015-16).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KXTerrain/KX_ChunkMotionState.cpp
 *  \ingroup ketsji
 */

#include "KX_ChunkMotionState.h"
#include "KX_ChunkNode.h"

KX_ChunkMotionState::KX_ChunkMotionState(KX_ChunkNode *node)
	:m_node(node)
{
}

KX_ChunkMotionState::~KX_ChunkMotionState()
{
}

void KX_ChunkMotionState::GetWorldPosition(float &posX, float &posY, float &posZ)
{
	const MT_Point2& pos = m_node->GetRealPos();
	posX = pos.x();
	posY = pos.y();
	posZ = 0.0f;
}

void KX_ChunkMotionState::GetWorldScaling(float &scaleX, float &scaleY, float &scaleZ)
{
	scaleX = 1.0f;
	scaleY = 1.0f;
	scaleZ = 1.0f;
}

void KX_ChunkMotionState::GetWorldOrientation(float &quatIma0, float &quatIma1, float &quatIma2, float &quatReal)
{
	quatIma0 = 0.0f;
	quatIma1 = 0.0f;
	quatIma2 = 0.0f;
	quatReal = 1.0f;
}
	
void KX_ChunkMotionState::GetWorldOrientation(float *ori)
{
	ori[0] = 1.0f; ori[4] = 0.0f; ori[8] = 0.0f;
	ori[1] = 0.0f; ori[5] = 1.0f; ori[9] = 0.0f;
	ori[2] = 0.0f; ori[6] = 0.0f; ori[10] = 1.0f;
	ori[3] = 0.0f; ori[7] = 0.0f; ori[11] = 0.0f;
}

void KX_ChunkMotionState::SetWorldOrientation(const float *ori)
{
}

void KX_ChunkMotionState::SetWorldPosition(float posX, float posY, float posZ)
{
}

void KX_ChunkMotionState::SetWorldOrientation(float quatIma0, float quatIma1, float quatIma2, float quatReal)
{
}

void KX_ChunkMotionState::CalculateWorldTransformations()
{
}
