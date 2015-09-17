# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Porteries Tristan.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	..
	../../Expressions
	../../GameLogic
	../../SceneGraph
	../../Rasterizer
	../../Physics/common
	../../../blender/blenlib
	../../../blender/blenkernel
	../../../blender/makesdna
	../../../blender/gpu
	../../../blender/imbuf
	../../../../intern/container
	../../../../intern/string
)

set(INC_SYS
	../../../../intern/moto/include
	../../../../intern/glew-mx
)

set(SRC
	KX_Chunk.cpp
	KX_ChunkCache.cpp
	KX_ChunkNode.cpp
	KX_Terrain.cpp
	KX_TerrainZone.cpp
	KX_ChunkMotionState.cpp

	KX_Chunk.h
	KX_ChunkCache.h
	KX_ChunkNode.h
	KX_Terrain.h
	KX_TerrainZone.h
	KX_ChunkMotionState.h
)

if(WITH_BULLET)
	list(APPEND INC
		../../Physics/Bullet
	)
	list(APPEND INC_SYS
		${BULLET_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_BULLET)
endif()

blender_add_lib(ge_logic_terrain "${SRC}" "${INC}" "${INC_SYS}")
