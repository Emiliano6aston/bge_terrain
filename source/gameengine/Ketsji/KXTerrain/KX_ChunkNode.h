 /*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Porteries Tristan, Gros Alexis. For the 
 * Uchronia project (2015-16).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __KX_CHUNK_NODE_H__
#define __KX_CHUNK_NODE_H__

#include "MT_Point2.h"
#include "MT_Point3.h"

class KX_Terrain;
class KX_Camera;
class CListValue;
class KX_Chunk;

/** Cette classe ne fait que gérer la visibilité des chunks, leur création et destruction.
 * On ce base sur un QuadTree pour la recherche et la création mais les chunks qui sont comme des noeuds
 * finaux sont stockés dans une liste contenue dans le terrain pour le rendu et la mise à jour du mesh
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
	/// Le noeud parent.
	KX_ChunkNode *m_parentNode;

	/** La position relative du noeud, à multiplier par la largeur d'un 
	 * chunk pour retrouver la position réelle.
	 */
	const Point2D m_relativePos;
	/// La taille relative du noeud la plus petite taille est 2.
	const unsigned short m_relativeSize;
	/// La position réelle du noeud.
	MT_Point2 m_realPos;
	/// Plus ce nombre est grand plus ce noeud est loin dans le QuadTree.
	const unsigned short m_level;

	/// Le rayon du noeud.
	float m_radius;
	/// La marge du rayon du noeud.
	float m_radiusMargin;

	/// La boite englobant le noeud pour le frustum culling.
	MT_Point3 m_box[8];
	/// Si vrai la boite de culling est modifiée.
	bool m_boxModified;
	/// La hauteur maximal de la boite.
	float m_maxBoxHeight;
	/// La hauteur minimal de la boite.
	float m_minBoxHeight;
	float m_minBoxHeightOptimized;
	float m_maxBoxHeightOptimized;
	/// Doit ont recreer la boite ?
	bool m_requestCreateBox;
	bool m_onConstruct;
	short m_onConstructSubNodes;
	/// Le noeud est il visible ?
	short m_culledState;

	/// Tableau de 4 sous noeuds.
	KX_ChunkNode **m_nodeList;
	/// Le chunk ou objet avec mesh et physique.
	KX_Chunk *m_chunk;

	/// Le terrain utilisé comme usine à chunks.
	KX_Terrain *m_terrain;

	bool NeedCreateNodes(CListValue *objects, KX_Camera *culledcam) const;
	bool InNode(CListValue *objects) const;
	void DestructNodes();
	void ConstructNodes();
	void DestructChunk();
	void ConstructChunk();
	void DisableChunkVisibility();

	void MarkCulled(KX_Camera *culldecam);

	MT_Point3 GetCenter() const;

public:
	KX_ChunkNode(KX_ChunkNode *parentNode,
				 int x, int y, 
				 unsigned short relativesize, 
				 unsigned short level, 
				 KX_Terrain* terrain);
	virtual ~KX_ChunkNode();

	/// Teste si le noeud est visible par une camera.
	short IsCameraVisible(KX_Camera *cam);
	/// Teste si le noeud est visible et créer des sous noeuds si besoin.
	void CalculateVisible(KX_Camera *culledcam, CListValue *objects);
	/// Draw debug info for culling box
	void DrawDebugInfo(short mode);

	/// On remet à 0 les variables m_maxBoxHeight et m_minBoxHeight.
	void ResetFrustumBoxHeights();

	/** On verifie que les arguments max et min ne sont pas plus grand/petit 
	 * que m_maxBoxHeight et m_minBoxHeight.
	 */
	void ExtendFrustumBoxHeights(float max, float min);

	/// Reconstruction de la boite et du rayon.
	void ReConstructFrustumBoxAndRadius();

	KX_ChunkNode *GetNodeRelativePosition(float x, float y);

	/** Renvoie le noeud parent du noeud adjacent qu'on veut trouver.
	 * \param x Le position du en x noeud ajecent par rapport a ce noeud : -1 / 0 / 1.
	 * \param y Comme l'argument x mais pour en y.
	 */
	KX_ChunkNode *GetAdjacentParentNode(short x, short y) const;

	inline KX_Terrain *GetTerrain() const
	{
		return m_terrain;
	}
	inline unsigned short GetRelativeSize() const
	{
		return m_relativeSize;
	}
	inline const MT_Point2 &GetRealPos() const
	{
		return m_realPos;
	}
	inline const Point2D &GetRelativePos() const
	{
		return m_relativePos;
	}
	inline MT_Point3 *GetBox()
	{ 
		return m_box;
	}

	/// Utilisé pour savoir si un noeud est visible.
	inline short GetCulledState() const
	{
		return m_culledState;
	}
	inline KX_Chunk *GetChunk() const
	{
		return m_chunk; 
	}
	/// Utilisé lors des testes jointures.
	inline unsigned short GetLevel() const
	{
		return m_level;
	}

	inline float GetMinBoxHeight() const
	{
		return m_minBoxHeight;
	}

	inline float GetMaxBoxHeight() const
	{
		return m_maxBoxHeight;
	}

	inline KX_ChunkNode *GetParentNode() const
	{
		return m_parentNode;
	}

	static unsigned int m_activeNode;
};

bool operator<(const KX_ChunkNode::Point2D& pos1, const KX_ChunkNode::Point2D& pos2);
std::ostream &operator<< (std::ostream &stream, const KX_ChunkNode::Point2D &pos);

#endif // __KX_CHUNK_NODE_H__
