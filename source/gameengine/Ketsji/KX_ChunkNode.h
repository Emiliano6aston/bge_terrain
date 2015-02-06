#ifndef __KX_CHUNK_NODE_H__
#define __KX_CHUNK_NODE_H__

class KX_Terrain;
class KX_Camera;
class RAS_IRasterizer;

class KX_ChunkNode
{
protected: // toutes les variables et fonctions communes à KX_Chunk et KX_ChunkNode
	const short m_relativePosX;
	const short m_relativePosY;
	const unsigned short m_relativeSize;

	// plus ce nombre est grand plus ce chunk est loin dans le QuadTree
	const unsigned short m_level;

	KX_Terrain* m_terrain;

	bool m_culled;

	virtual bool NeedCreateSubChunks(KX_Camera* cam) const = 0;
	virtual void DestructSubChunks();
	virtual void ConstructSubChunks();
	inline bool IsFinalNode() const { return !m_subChunks; }
	virtual void MarkCulled(KX_Camera* cam) = 0;

private:
	// tableau de 4 sous chunk
	KX_ChunkNode** m_subChunks;

public:
	KX_ChunkNode(short x, short y, unsigned short relativesize, unsigned short level, KX_Terrain* terrain);
	virtual ~KX_ChunkNode();

	/// teste si le mesh est visible et créer des sous chunk si besoin
	virtual void CalculateVisible(KX_Camera *cam);
	/// reconstruction du mesh lors de la création du chunk ou si un chunk adjacent change (création de joints)
	virtual void UpdateMesh();
	/// dessin du mesh du centre et des joints
	virtual void RenderMesh(RAS_IRasterizer* rasty);

	inline bool IsCulled() const { return m_culled; }

	/// utilisé lors des tests jointures
	inline unsigned short GetLevel() const { return m_level; }

	KX_ChunkNode* GetChunkRelativePosition(short x, short y);

	static unsigned int m_finalNode;
};

#endif // __KX_CHUNK_NODE_H__