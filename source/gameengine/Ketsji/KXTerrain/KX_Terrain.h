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

#ifndef __KX_TERRAIN_H__
#define __KX_TERRAIN_H__

#include <map>
#include <list>
#include <vector>
#include "MT_Transform.h"
#include "KX_ChunkNode.h" // for Point2D
#include "KX_TerrainZone.h"
#include "KX_GameObject.h"

class RAS_IRasterizer;
class RAS_MaterialBucket;
class CListValue;

class KX_Terrain : public KX_GameObject
{
private:
	/// Le materiaux utilisé pour tous les meshs de chunks.
	RAS_MaterialBucket *m_bucket;

	/** On utilise un objet comme refèrence pour pouvior le copier
	 * pour créer de nouveaux chunks.
	 */
	KX_GameObject *m_templateObject;

	/** Le nombre maximal de subdivision de noeud ou de niveaux dans 
	 * le quad tree.
	 */
	unsigned short m_maxChunkLevel;

	/** En theorie le nombre de vertices en largeur dans un chunk.
	 * Non implementé.
	 */
	unsigned short m_vertexSubdivision;

	/// Le nombre de chunks en largeur dans le terrain.
	unsigned short m_width;

	/** Tous les noeuds dans cette distance peuvent avoir un niveau
	 * plus petit que 2 (soit le minimum).
	 */
	float m_maxDistance2;

	/// La même chose que m_maxDistance2 mais pour les objets physique.
	float m_physicsMaxDistance2;

	/// La largeur du mesh d'un chunk.
	float m_chunkSize;

	/// La hauteur maximale theorique du mesh d'un chunk.
	float m_maxHeight;

	/// La hauteur minimal theorique du mesh d'un chunk.
	float m_minHeight;

	/// Si vrai le terrain et déjà construit et les noeud principaux aussi.
	bool m_construct;

	/** Un petit compteur de frame utilisé pour eviter d'afficher
	 * le messages de debug a chaque frame.
	 */
	unsigned short m_frame;

	/// Les 4 noeuds principaux du terrain.
	KX_ChunkNode **m_nodeTree;

	typedef std::list<KX_Chunk *> KX_ChunkList;

	/// La liste de tous les chunks actifs.
	KX_ChunkList m_chunkList;

	/// La liste de tous les chunks à supprimer à la fin de la frame.
	KX_ChunkList m_euthanasyChunkList;

	std::vector<KX_TerrainZoneMesh *> m_zoneMeshList;

public:
	KX_Terrain(void *sgReplicationInfo,
			   SG_Callbacks callbacks,
			   RAS_MaterialBucket *bucket,
			   KX_GameObject *templateObject,
			   unsigned short maxLevel,
			   unsigned short vertexSubdivision,
			   unsigned short width,
			   float maxDistance,
			   float physicsMaxDistance,
			   float chunkSize);
	~KX_Terrain();

	void Construct();
	void Destruct();

	void CalculateVisibleChunks(KX_Camera *culledcam);
	void UpdateChunksMeshes();
	void RenderChunksMeshes(const MT_Transform &cameratrans, RAS_IRasterizer *rasty);
	void DrawDebugNode();

	/// Le niveau de subdivision maximal
	inline unsigned short GetMaxLevel() const
	{
		return m_maxChunkLevel;
	}
	/// le nombre maximun de face en largeur dans un chunk
	inline unsigned short GetVertexSubdivision() const
	{
		return m_vertexSubdivision;
	}
	/// La largeur du terrain en echelle relative.
	inline unsigned short GetWidth() const
	{
		return m_width;
	}
	/// La distance maximal pour avoir un niveau de subdivision superieur à 1
	inline float GetMaxDistance2() const
	{
		return m_maxDistance2;
	}
	inline float GetPhysicsMaxDistance2() const
	{
		return m_physicsMaxDistance2;
	}
	/// La taille de tous les chunks
	inline float GetChunkSize() const
	{
		return m_chunkSize;
	}
	/// La hauteur maximale
	inline float GetMaxHeight() const
	{
		return m_maxHeight;
	}
	/// La hauteur minimale
	inline float GetMinHeight() const
	{
		return m_minHeight;
	}

	/** le nombre de subdivision par rapport à une distance
	 * et en fonction du type de l'objet : camera ou objet utilisant
	 * un controller physique actif.
	 */
	unsigned short GetSubdivision(float distance, bool iscamera) const;
	KX_ChunkNode *GetNodeRelativePosition(const KX_ChunkNode::Point2D &pos);

	/** La position en 3D d'un vertice. on renvoie une coordonnée et non une 
	 * hauteur car on pourrait imaginer modifier la position en x et y.
	 */
	VertexZoneInfo *GetVertexInfo(float x, float y) const;

	KX_ChunkNode **NewNodeList(KX_ChunkNode *parentNode, int x, int y, unsigned short level);
	KX_Chunk *AddChunk(KX_ChunkNode *node);
	void RemoveChunk(KX_Chunk *chunk);
	void ScheduleEuthanasyChunks();

	void AddTerrainZoneMesh(KX_TerrainZoneMesh *zoneMesh);
};

#endif //__KX_TERRAIN_H__
