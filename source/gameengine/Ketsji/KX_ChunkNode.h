#ifndef __KX_CHUNK_NODE_H__
#define __KX_CHUNK_NODE_H__

#include "MT_Point2.h"
#include "MT_Point3.h"

class KX_Terrain;
class KX_Camera;
class KX_Chunk;

/*
 * Cette classe ne fait que gérer la visibilité des chunks, leur création et destruction.
 * On ce base sur un QuadTree pour la recherche et la création mais les chunks qui sont comme des noeuds
 * finaux sont stockés dans une liste contenue dans le terrain pour le rendu et la mise a jour du mesh
 * et de la physique.
 */

class KX_ChunkNode
{
private:
	/// La position relative du noeud, à multiplier par la largeur d'un chunk pour retrouver la position réelle
	const short m_relativePosX;
	const short m_relativePosY;

	/// La taille relative du noeud la plus petite taille est 1
	const unsigned short m_relativeSize;

	/// La position réelle du noeud
	MT_Point2 m_realPos;

	/// Le rayon du noeud
	float m_radius2;

	/// La boite englobant le noeud pour le frustum culling
	MT_Point3 m_box[8];

	/// Le noeud est il visible ?
	bool m_culled;

	/// Plus ce nombre est grand plus ce noeud est loin dans le QuadTree
	const unsigned short m_level;

	/// Tableau de 4 sous noeuds
	KX_ChunkNode** m_nodeList;

	/// Le chunk ou objet avec mesh et physique
	KX_Chunk* m_chunk;

	/// Le terrain utilisé comme usine à chunks
	KX_Terrain* m_terrain;

	bool NeedCreateSubChunks(KX_Camera* cam) const;
	void DestructAllNodes();
	void ConstructAllNodes();
	void DestructChunk();
	void ConstructChunk();

	void MarkCulled(KX_Camera* cam);

public:
	KX_ChunkNode(short x, short y, unsigned short relativesize, unsigned short level, KX_Terrain* terrain);
	virtual ~KX_ChunkNode();

	/// Teste si le noeud est visible et créer des sous noeuds si besoin
	void CalculateVisible(KX_Camera *cam);

	inline KX_Terrain* GetTerrain() const { return m_terrain; }
	inline unsigned short GetRelativeSize() const { return m_relativeSize; }
	inline MT_Point2 GetRealPos() const { return m_realPos; }
	inline int GetRelativePosX() const { return m_relativePosX; }
	inline int GetRelativePosY() const { return m_relativePosY; }
	inline MT_Point3* GetBox() { return m_box; };

	/// Utilisé pour savoir si un noeud est visible
	inline bool IsCulled() const { return m_culled; }

	/// Utilisé lors des tests jointures
	inline unsigned short GetLevel() const { return m_level; }

	/**
	 * Fonction renvoyant un noeud correspondant à une position.
	 * \param x La position en x.
	 * \param y La position en y.
	 * \return Un sous noeud et au pire des cas lui même.
	 */
	
	KX_ChunkNode* GetNodeRelativePosition(short x, short y);

	static unsigned int m_finalNode;
};

#endif // __KX_CHUNK_NODE_H__