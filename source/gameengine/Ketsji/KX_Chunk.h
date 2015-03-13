#ifndef __KX_CHUNK_H__
#define __KX_CHUNK_H__

#define POLY_COUNT 4


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
	Vertex *m_center[POLY_COUNT - 1][POLY_COUNT - 1];
	bool m_hasVertexes;
	bool m_needNormalCompute;

	// les dernières jointures
	bool m_lastHasJoint[4];

	/* variables temporaire utilisé par UpdateMesh et EndUpdateMesh, 
	 * cela evite de rearcourir tous l'arbre
	 */
	KX_ChunkNode *m_lastChunkNode[4];

	unsigned int m_originVertexIndex;

	/// construction du mesh
	void ConstructMesh();
	void DestructMesh();

	void ConstructVertexes();
	float GetZVertex(float vertx, float verty) const;

	void InvalidateJointVertexes();

	void ConstructPolygones();
	void ConstructCenterColumnPolygones();
	void ConstructJointColumnPolygones(JointColumn *column, bool reverse);
	void AddMeshPolygonVertexes(Vertex *v1, Vertex *v2, Vertex *v3, bool reverse);

	void ConstructVertexesNormal();
	void ConstructExternVertexesNormal();

public:
	KX_Chunk(void *sgReplicationInfo, SG_Callbacks callbacks, KX_ChunkNode *node, RAS_MaterialBucket *m_bucket);
	virtual ~KX_Chunk();

	/// creation du mesh avec joint des vertices du chunk avec ceux d'à cotés si neccesaire
	void UpdateMesh();
	void EndUpdateMesh();
	void ReconstructMesh();
	void RenderMesh(RAS_IRasterizer *rasty, KX_Camera *cam);

	Vertex *GetColumnExternVertex(COLUMN_TYPE column, unsigned short index);
	inline KX_ChunkNode* GetNode() const { return m_node; }

	static unsigned int m_chunkActive;
};

#endif // __KX_CHUNK_H__