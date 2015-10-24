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

#ifndef __KX_CHUNK_H__
#define __KX_CHUNK_H__

#include "SG_QList.h"
#include "KX_ChunkNode.h"

#define VERTEX_COUNT 5
#define VERTEX_COUNT_INTERN (VERTEX_COUNT - 2)

#define POLY_COUNT (VERTEX_COUNT - 1)
#define POLY_COUNT_INTERN (VERTEX_COUNT_INTERN - 1)

class KX_ChunkNode;
class KX_ChunkNodeProxy;
class RAS_MeshObject;
class RAS_MaterialBucket;
class RAS_IRasterizer;
class PHY_IPhysicsController;

class KX_Chunk
{
public:
	/// Variable used for statistics.

	/// Number of mesh rebuilding per frame.
	static unsigned int meshRecreation;
	/// Time used to create a chunk, without building the mesh.
	static double chunkCreationTime;
	/// Time used to calculate all vertices normals.
	static double normalComputingTime;
	/// Time used to  add vertices to final mesh.
	static double vertexAddingTime;
	/// Time used to create vertices : allocation + building.
	static double vertexCreatingTime;
	/// Time used to add polygons (also includes vetices duplicates search).
	static double polyAddingTime;
	/// Time used to create physical mesh.
	static double physicsCreatingTime;
	/// Number of active chunks.
	static unsigned int m_chunkActive;

	static void ResetTime();
	static void PrintTime();

	struct Vertex;
	struct JointColumn;

	enum COLUMN_TYPE {
		COLUMN_LEFT=0,
		COLUMN_RIGHT=1,
		COLUMN_FRONT=2,
		COLUMN_BACK=3,
		COLUMN_NONE=4,
	};

private:
	/// Parent node.
	KX_ChunkNode *m_node;

	/// Mesh material passed to mesh building.
	RAS_MaterialBucket *m_bucket;
	/// builder mesh.
	RAS_MeshObject *m_meshObj;
	/// Mesh slot
	SG_QList m_meshSlots;
	/// Mesh transforms matrix.
	double m_meshMatrix[16];

	/// Physical controller.
	PHY_IPhysicsController *m_physicsController;

	/// Is chunk visible ?
	bool m_visible;

	/// Store data for a faster rebuilding
	JointColumn *m_columns[4];
	Vertex *m_center[VERTEX_COUNT_INTERN][VERTEX_COUNT_INTERN];
	bool m_hasVertexes;
	bool m_onConstruct;

	float m_maxVertexHeight;
	float m_minVertexHeight;
	bool m_requestCreateBox;

	KX_ChunkNode *m_parentJointNodes[4];

	/// Last joints.
	unsigned short m_lastHasJoint[4]; // TODO rename and use 1 as default value.
	/* All surounding nodes, up to 6 per side (2 adjacent per point), 16 total nodes.
	 * For left and right columns, chunks are from top to bottom and
	 * for top and bottom columns from left to right.
	 */
	KX_ChunkNode *m_jointNode[4][6];

	// store joints nodes for code optimizations.
	float m_jointNodePosition[4][2];

	/// 4 adjacent nodes proxies.
	KX_ChunkNodeProxy *m_jointNodeProxy[4];

	/// Index used during vertices building, to have a uniue one per vertex
	unsigned int m_originVertexIndex;

	void ConstructMesh();
	void DestructMesh();

	void ConstructPhysicsController();

	void ConstructVertexes();
	void ComputeJointVertexesNormal();
	void ComputeColumnJointVertexNormal(COLUMN_TYPE columnType, bool reverse);
	Vertex *GetVertexByChunkRelativePosition(unsigned short x, unsigned short y) const;
	Vertex *GetVertexByTerrainRelativePosition(int x, int y) const;
	KX_ChunkNode::Point2D GetTerrainRelativeVertexPosition(unsigned short x, unsigned short y) const;
	void GetCoorespondingVertexesFromChunk(KX_ChunkNode *jointNode, Vertex *origVertex, COLUMN_TYPE columnType, 
										   Vertex **coExternVertex, Vertex **coInternVertex);
	Vertex *NewVertex(unsigned short relx, unsigned short rely);

	void InvalidateJointVertexesAndIndexes();

	void ConstructPolygones();
	void ConstructCenterColumnPolygones();
	void ConstructJointColumnPolygones(JointColumn *column, bool reverse);
	void AddMeshPolygonVertexes(Vertex *v1, Vertex *v2, Vertex *v3, bool reverse);

	void SetNormal(Vertex *vertexCenter) const;

	/// \section Nodes joints management.
	void GetJointColumnNodes();
	bool GetJointNodesChanged();

public:
	KX_Chunk(KX_ChunkNode *node, RAS_MaterialBucket *m_bucket);
	virtual ~KX_Chunk();

	void UpdateColumnVertexesNormal(COLUMN_TYPE columnType);

	/// mesh building with adjacent chunk vertices merging if needed
	void UpdateMesh();
	void EndUpdateMesh();
	void RenderMesh(RAS_IRasterizer *rasty, KX_Camera *cam);

	inline KX_ChunkNode *GetNode() const
	{
		return m_node;
	}

	inline bool GetVisible() const
	{
		return m_visible;
	}

	inline void SetVisible(bool visible)
	{
		m_visible = visible;
	}

	inline unsigned short GetJointLevel(COLUMN_TYPE columnType) const
	{
		return m_lastHasJoint[columnType];
	}

	unsigned short GetColumnVertexInterval(COLUMN_TYPE columnType) const;
};

#endif // __KX_CHUNK_H__
