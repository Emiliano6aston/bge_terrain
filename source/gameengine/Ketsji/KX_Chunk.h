#ifndef KX_CHUNK_H
#define KX_CHUNK_H

#include <BLI_noise.h>
#include "RAS_MaterialBucket.h"
#include "MT_Transform.h"

class KX_Terrain;

class KX_Camera;

class RAS_MaterialBucket;
class RAS_MeshSlot;
class RAS_IRasterizer;

// on utilise un vecteur de int pour acceder aux chunkx à coté ce serait utile si le terrain et circulaire
typedef unsigned short ushort;

class KX_Chunk
{
private:
	int m_relativePosX;
	int m_relativePosY;
	MT_Point2 m_realPos;
	unsigned short m_relativeSize;

	KX_Terrain* m_terrain;

	RAS_MeshSlot *m_meshSlot;//Plein de zolis utilitaires pour créer un mesh, ouiiii

	/// Subdivision level
	ushort m_subDivisions;
	ushort m_lastSubDivision;
	// plus ce nombre est grand plus ce chunk est loin dans le QuadTree
	unsigned short m_level;
	// les derniere jointures
	bool m_lastJointChunk[4];

	MT_Point3 m_box[8];

	float m_radius;
	bool m_culled;
	bool IsCulled(KX_Camera* cam) const;

	bool m_hasSubChunks;
	KX_Chunk* m_subChunks[4];

public:
	KX_Chunk(int x, int y, unsigned int relativesize, unsigned short level, KX_Terrain* terrain);
	~KX_Chunk();
	/// construction du mesh
	void ConstructMesh(const bool jointLeft, const bool jointRight, const bool jointFront, const bool jointBack);
	/// calcule par rapport à la distance utilisateur le nombre de subdivisions
	void Update(KX_Camera *cam);
	/// creation du mesh avec joint des vertices du chunk avec ceux d'à cotés si neccesaire
	void UpdateMesh();
	void RenderMesh(RAS_IRasterizer* rasty);
	/// Initialization du l'ancien niveau de subdivision au nouveau niveau de subdivision
	void EndUpdate();
	
	inline ushort GetSubDivision() const { return m_subDivisions; };

	KX_Chunk* GetChunkRelativePosition(int x, int y);
};

#endif