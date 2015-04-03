#ifndef __KX_CHUNK_H__
#define __KX_CHUNK_H__

#define VERTEX_COUNT 5
#define VERTEX_COUNT_INTERN (VERTEX_COUNT - 2)

#define POLY_COUNT (VERTEX_COUNT - 1)
#define POLY_COUNT_INTERN (VERTEX_COUNT_INTERN - 1)

#include "KX_GameObject.h"
#include <BLI_noise.h>

class KX_ChunkNode;
class RAS_MeshObject;
class RAS_MaterialBucket;
class RAS_IRasterizer;

class KX_Chunk : public KX_GameObject
{
public:
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
	KX_ChunkNode *m_node;

	RAS_MaterialBucket *m_bucket;
	RAS_MeshObject *m_meshObj;

	/** on stocke les colonnes pour un reconstruction plus rapide et un accés 
	 * aux vertices pour les chunks autour
	 */
	JointColumn *m_columns[4];
	Vertex *m_center[VERTEX_COUNT_INTERN][VERTEX_COUNT_INTERN];
	bool m_hasVertexes;

	float m_maxVertexHeight;
	float m_minVertexHeight;

	// les dernières jointures
	bool m_lastHasJoint[4];

	/* variables temporaire utilisé par UpdateMesh et EndUpdateMesh, 
	 * cela evite de rearcourir tous l'arbre
	 */
	KX_ChunkNode *m_lastChunkNode[4];

	unsigned int m_originVertexIndex;

	unsigned short m_life;

	/// construction du mesh
	void ConstructMesh();
	void DestructMesh();

	void ConstructVertexes();
	const MT_Point2 GetVertexPosition(short relx, short rely) const;
	Vertex *NewVertex(short relx, short rely);
	Vertex *GetVertex(short x, short y) const;

	void InvalidateJointVertexes();

	void ConstructPolygones();
	void ConstructCenterColumnPolygones();
	void ConstructJointColumnPolygones(JointColumn *column, bool reverse);
	void AddMeshPolygonVertexes(Vertex *v1, Vertex *v2, Vertex *v3, bool reverse);

	const MT_Vector3 GetNormal(Vertex *vertexCenter, bool intern) const;

public:
	KX_Chunk(void *sgReplicationInfo, SG_Callbacks callbacks, KX_ChunkNode *node, RAS_MaterialBucket *m_bucket);
	virtual ~KX_Chunk();

	/// creation du mesh avec joint des vertices du chunk avec ceux d'à cotés si neccesaire
	void UpdateMesh();
	void EndUpdateMesh();
	void ReconstructMesh();
	void RenderMesh(RAS_IRasterizer *rasty, KX_Camera *cam);

	inline KX_ChunkNode* GetNode() const { return m_node; }

	static unsigned int m_chunkActive;
};

#endif // __KX_CHUNK_H__