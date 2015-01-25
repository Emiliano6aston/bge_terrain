#ifndef KX_CHUNK_H
#define KX_CHUNK_H

#include <BLI_noise.h>
#include "RAS_MaterialBucket.h"

class RAS_DisplayArray;
class KX_Camera;
class KX_Terrain;

typedef std::pair<int, int> Vector2DInt;
typedef unsigned short ushort;

class KX_Chunk
{
private:
	
	/// Subdivision level
	ushort m_subDivisions;
	ushort m_lastSubDivision;

	RAS_MeshSlot *m_meshSlot;//Plein de zolis utilitaires pour créer un mesh, ouiiii
	RAS_MaterialBucket* m_material;//Le matériau du chunk, qu'on pourra envoyer à initde meshSlot
	
	KX_Terrain* m_terrain;
	MT_Point3 m_box[8];
	

	bool IsVisible(KX_Camera* cam) const;
public:
	KX_Chunk(Vector2DInt pos, KX_Terrain* terrain);
	~KX_Chunk();
	/// calcule par rapport à la distance utilisateur le nombre de subdivisions
	void Update(KX_Camera *cam);
	/// Joint les vertices du chunk avec ceux d'à cotés
	void UpdateDisplayArray(Vector2DInt pos);
	void EndUpdate();
	
	ushort GetSubDivisions();
	ushort GetNbreVertices();
};

#endif