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

#ifndef __KX_TERRAIN_H__
#define __KX_TERRAIN_H__

#include <map>
#include <list>
#include <vector>

#include "MT_Transform.h"

#include "KX_ChunkNode.h" // for Point2D
#include "KX_TerrainZone.h"
#include "KX_GameObject.h"

class RAS_IRasterizer;
class RAS_MaterialBucket;
class CListValue;
struct Material;
class KX_ChunkRootCache;

class KX_Terrain : public KX_GameObject
{
private:
	/// Material used for mesh chunks.
	RAS_MaterialBucket *m_bucket;

	/// Blender material used to build physical controllers.
	Material *m_material;

	/// Maximum node subdivision level used in the quad tree.
	unsigned short m_maxChunkLevel;

	/// Minimum subdivision level for a physical chunk.
	unsigned short m_minPhysicsLevel;

	/** Number of vertices widths in a chunk (In theory)
	 * NOT IMPLEMENTED YET.
	 */
	unsigned short m_vertexSubdivision;

	/// Number of vertices widths in the terrain.
	unsigned short m_width;

	/** Tous les noeuds dans cette distance peuvent avoir un niveau
	 * plus petit que 2 (soit le minimum).
	 */
	float m_cameraMaxDistance;

	/// Same as m_cameraMaxDistance except for physical objects.
	float m_objectMaxDistance;

	/// Mesh chunk width.
	float m_chunkSize;

	/// Le facteur de la marge du rayon d'un noeud.
	float m_marginFactor;

	/// Le mode de déboguage des noeuds.
	short m_debugMode;
	/// Number of skipped frames between each time debugging.
	unsigned short m_debugTimeFrame;

	/// Si vrai le terrain et déjà construit et les noeud principaux aussi.
	bool m_construct;

	/// Frames counter to prevent debug message appearing on each frame
	unsigned short m_debugFrame;

	/// Le noeud principal du terrain.
	KX_ChunkNode *m_nodeTree;

	typedef std::list<KX_Chunk *> KX_ChunkList;

	/// List of all active chunks.
	KX_ChunkList m_chunkList;

	/// List of all chunks marked for deletion after the end of a frame.
	KX_ChunkList m_euthanasyChunkList;

	std::vector<KX_TerrainZoneMesh *> m_zoneMeshList;

	/// Vertices creation cache.
	bool m_useCache;

	/// Tant maximum de vie pour un noeud de cache sans activité, en frame.
	unsigned short m_cacheRefreshTime;

	unsigned short m_cacheFrame;

	// Vertices cache.
	KX_ChunkRootCache *m_chunkRootCache;

public:
	KX_Terrain(void *sgReplicationInfo,
			   SG_Callbacks callbacks,
			   RAS_MaterialBucket *bucket,
			   Material *material,
			   unsigned short maxLevel,
			   unsigned short minPhysicsLevel,
			   unsigned short vertexSubdivision,
			   unsigned short width,
			   float cameraMaxDistance,
			   float objectMaxDistance,
			   float chunkSize,
			   float marginFactor,
			   short debugMode,
			   unsigned short debugTimeFrame,
			   bool useCache,
			   unsigned short cacheRefreshTime);
	~KX_Terrain();

	void Construct();
	void Destruct();

	void CalculateVisibleChunks(KX_Camera *culledcam);
	void UpdateChunksMeshes();
	void RenderChunksMeshes(KX_Camera *cam, RAS_IRasterizer *rasty);
	void DrawDebugNode();

	/// Maximal subdivision level.
	inline unsigned short GetMaxLevel() const
	{
		return m_maxChunkLevel;
	}
	/// Minimum subdivision level for a physical chunk..
	inline unsigned short GetMinPhysicsLevel() const
	{
		return m_minPhysicsLevel;
	}
	/// Maximum number of width faces per chunk.
	inline unsigned short GetVertexSubdivision() const
	{
		return m_vertexSubdivision;
	}
	/// Relative terrain width.
	inline unsigned short GetWidth() const
	{
		return m_width;
	}
	/// Maximum distance to get a subdivision level higher than 1.
	inline float GetCameraMaxDistance() const
	{
		return m_cameraMaxDistance;
	}
	inline float GetObjectMaxDistance() const
	{
		return m_objectMaxDistance;
	}
	/// All the chunks sizes.
	inline float GetChunkSize() const
	{
		return m_chunkSize;
	}
	/// Le facteur de la marge du rayon d'un noeud.
	inline float GetMarginFactor() const
	{
		return m_marginFactor;
	}
	/// Blender material.
	inline Material *GetBlenderMaterial() const
	{
		return m_material;
	}

	inline short GetDebugMode() const
	{
		return m_debugMode;
	}

	/** le nombre de subdivision par rapport à une distance
	 * et en fonction du type de l'objet : camera ou objet utilisant
	 * un controller physique actif.
	 */
	unsigned short GetSubdivision(float distance, bool iscamera) const;
	KX_ChunkNode *GetNodeRelativePosition(float x, float y);

	/** 3D vertex position. We return a coord and not a height
	 * as we could modify the position using x and y.
	 */
	VertexZoneInfo *GetVertexInfo(int x, int y) const;
	VertexZoneInfo *NewVertexInfo(int x, int y) const;

	KX_ChunkNode **NewNodeList(KX_ChunkNode *parentNode, int x, int y, unsigned short level);
	KX_Chunk *AddChunk(KX_ChunkNode *node);
	void RemoveChunk(KX_Chunk *chunk);
	void ScheduleEuthanasyChunks();

	void AddTerrainZoneMesh(KX_TerrainZoneMesh *zoneMesh);
};

#endif //__KX_TERRAIN_H__
