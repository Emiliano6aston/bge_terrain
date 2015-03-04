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
public:
	struct Point2D
	{
		int x, y;
		Point2D(int _x, int _y)
			:x(_x),
			y(_y)
		{
		}
	};

private:
	/// La position relative du noeud, à multiplier par la largeur d'un chunk pour retrouver la position réelle
	const Point2D m_relativePos;

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

	bool NeedCreateSubChunks(KX_Camera* campos) const;
	void DestructAllNodes();
	void ConstructAllNodes();
	void DestructChunk();
	void ConstructChunk();

	void MarkCulled(KX_Camera* culldecam);

public:
	KX_ChunkNode(int x, int y, unsigned short relativesize, unsigned short level, KX_Terrain* terrain);
	virtual ~KX_ChunkNode();

	/// Teste si le noeud est visible et créer des sous noeuds si besoin
	void CalculateVisible(KX_Camera *culledcam, KX_Camera* campos);

	KX_ChunkNode* GetNodeRelativePosition(const Point2D& pos);

	inline KX_Terrain* GetTerrain() const { return m_terrain; }
	inline unsigned short GetRelativeSize() const { return m_relativeSize; }
	inline float GetRadius2() const { return m_radius2; }
	inline MT_Point2 GetRealPos() const { return m_realPos; }
	inline const Point2D& GetRelativePos() const { return m_relativePos; }
	inline MT_Point3* GetBox() { return m_box; };

	/// Utilisé pour savoir si un noeud est visible
	inline bool IsCulled() const { return m_culled; }

	/// Utilisé lors des tests jointures
	inline unsigned short GetLevel() const { return m_level; }

	static unsigned int m_finalNode;
};

bool operator<(const KX_ChunkNode::Point2D& pos1, const KX_ChunkNode::Point2D& pos2);

#endif // __KX_CHUNK_NODE_H__