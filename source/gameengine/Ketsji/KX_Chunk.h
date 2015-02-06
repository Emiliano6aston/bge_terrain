#ifndef __KX_CHUNK_H__
#define __KX_CHUNK_H__

#include "KX_ChunkNode.h"
#include <BLI_noise.h>
#include "MT_Point2.h"
#include "MT_Point3.h"

class RAS_MeshSlot;
class RAS_IRasterizer;

class KX_Chunk : public KX_ChunkNode
{
protected:
	virtual bool NeedCreateSubChunks(KX_Camera* cam) const;
	virtual void ConstructSubChunks();
	virtual void MarkCulled(KX_Camera* cam);

private:
	MT_Point2 m_realPos;

	RAS_MeshSlot* m_meshSlot;
	RAS_MeshSlot* m_jointSlot;

	// les dernières jointures
	bool m_lastHasJointLeft;
	bool m_lastHasJointRight;
	bool m_lastHasJointFront;
	bool m_lastHasJointBack;

	struct JointColumn;

	MT_Point3 m_box[8];

	float m_radius2;

	/// construction du mesh
	void ConstructMesh();
	void ConstructJoint();
	void ConstructJointColumnPoly(const JointColumn& column, unsigned short polyCount);

public:
	KX_Chunk(short x, short y, unsigned short relativesize, unsigned short level, KX_Terrain* terrain);
	virtual ~KX_Chunk();

	/// creation du mesh avec joint des vertices du chunk avec ceux d'à cotés si neccesaire
	virtual void UpdateMesh();
	virtual void RenderMesh(RAS_IRasterizer* rasty);

	static unsigned int m_chunkActive;
};

#endif // __KX_CHUNK_H__