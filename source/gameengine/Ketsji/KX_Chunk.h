#ifndef KX_CHUNK_H
#define KX_CHUNK_H

#include <BLI_noise.h>
#include "RAS_MaterialBucket.h"
#include "MT_Transform.h"
#include "KX_Terrain.h"

class KX_Camera;

class RAS_MaterialBucket;
class RAS_MeshSlot;
class RAS_IRasterizer;

// on utilise un vecteur de int pour acceder aux chunkx à coté ce serait utile si le terrain et circulaire
typedef unsigned short ushort;

class KX_Chunk
{
private:
	int m_id;
	vector2DInt m_position;

	KX_Terrain* m_terrain;

	RAS_MeshSlot *m_meshSlot;//Plein de zolis utilitaires pour créer un mesh, ouiiii
	RAS_MaterialBucket* m_material;//Le matériau du chunk, qu'on pourra envoyer à initde meshSlot

	/// Subdivision level
	ushort m_subDivisions;
	ushort m_lastSubDivision;

	MT_Point3 m_box[8];

	bool m_culled;
	bool IsCulled(KX_Camera* cam) const;

public:
	KX_Chunk(vector2DInt pos, KX_Terrain* terrain, RAS_MaterialBucket* bucket, int id);
	~KX_Chunk();
	/// calcule par rapport à la distance utilisateur le nombre de subdivisions
	void Update(KX_Camera *cam);
	/// Joint les vertices du chunk avec ceux d'à cotés + rendu
	void UpdateDisplayArrayDraw(const MT_Transform& cameratrans, RAS_IRasterizer* rasty);
	/// Initialization du l'ancien niveau de subdivision au nouveau niveau de subdivision
	void EndUpdate();
	
	inline ushort GetSubDivision() const { return m_subDivisions; };
};

#endif