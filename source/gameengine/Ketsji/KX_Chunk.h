#ifndef __KX_CHUNK_H__
#define __KX_CHUNK_H__

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

private:
	KX_ChunkNode* m_node;

	RAS_MaterialBucket* m_bucket;

	// les dernières jointures
	bool m_lastHasJointLeft;
	bool m_lastHasJointRight;
	bool m_lastHasJointFront;
	bool m_lastHasJointBack;

	unsigned int m_originVertexIndex;

	/// construction du mesh
	void ConstructCenterMesh();
	void ConstructJointMesh();
	void ConstructJointMeshColumnPoly(const JointColumn& column, unsigned short polyCount);
	void AddMeshPolygonVertexes(Vertex v1, Vertex v2, Vertex v3);
	float GetZVertex(float vertx, float verty) const;

public:
	KX_Chunk(void* sgReplicationInfo, SG_Callbacks callbacks, KX_ChunkNode* node, RAS_MaterialBucket* m_bucket);
	virtual ~KX_Chunk();

	/// creation du mesh avec joint des vertices du chunk avec ceux d'à cotés si neccesaire
	void UpdateMesh();
	void ReconstructMesh();
	void RenderMesh(RAS_IRasterizer* rasty);

	static unsigned int m_chunkActive;
};

#endif // __KX_CHUNK_H__