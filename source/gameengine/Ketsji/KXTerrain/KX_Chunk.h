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

#ifndef __KX_CHUNK_H__
#define __KX_CHUNK_H__

#define VERTEX_COUNT 5
#define VERTEX_COUNT_INTERN (VERTEX_COUNT - 2)

#define POLY_COUNT (VERTEX_COUNT - 1)
#define POLY_COUNT_INTERN (VERTEX_COUNT_INTERN - 1)

#include <BLI_noise.h>

class KX_ChunkNode;
class RAS_MeshObject;
class RAS_MaterialBucket;
class RAS_IRasterizer;
class PHY_IPhysicsController;

class KX_Chunk
{
public:
	/// Variables utilisées pour faire des statistiques.

	/// Le nombre de recréation de mesh par frame.
	static unsigned int meshRecreation;
	/// Le temps dépensé pour créer le chunk, sans création de mesh.
	static double chunkCreationTime;
	/// Le temps dépensé pour calculer les normales de tous les vertices.
	static double normalComputingTime;
	/// Le temps dépensé pour ajouter les vertice dans le mesh final.
	static double vertexAddingTime;
	/// Le temps dépensé pour créer les vertices : allocation plus constructeur.
	static double vertexCreatingTime;
	/// Le temps dépensé pour ajouter les polygones, comprend aussi la recherche de vertices en doublons.
	static double polyAddingTime;
	/// Le temps dépensé pour créer le mesh physique.
	static double physicsCreatingTime;
	/// Le nombre de chunks actifs.
	static unsigned int m_chunkActive;

	static void ResetTime();
	static void PrintTime();

	struct Vertex;
	struct JointColumn;

	enum COLUMN_TYPE {
		COLUMN_LEFT=0,
		COLUMN_RIGHT=1,
		COLUMN_FRONT=2,
		COLUMN_BACK=3,
		COLUMN_NONE=4,
	};

private:
	/// Le noeud parent.
	KX_ChunkNode *m_node;

	/// Le materiaux utilisé par le mesh, on le passe a la construction du mesh.
	RAS_MaterialBucket *m_bucket;
	/// Le mesh de construction.
	RAS_MeshObject *m_meshObj;
	/// Le mesh slot
	SG_QList m_meshSlots;
	/// La matrice de transformation du mesh.
	double m_meshMatrix[16];

	/// Le controlleur physique.
	PHY_IPhysicsController *m_physicsController;

	/// Le chunk est visible ?
	bool m_visible;

	/// on stocke les colonnes pour un reconstruction plus rapide
	JointColumn *m_columns[4];
	Vertex *m_center[VERTEX_COUNT_INTERN][VERTEX_COUNT_INTERN];
	bool m_hasVertexes;

	float m_maxVertexHeight;
	float m_minVertexHeight;

	/// Les dernières jointures.
	unsigned short m_lastHasJoint[4];

	/** Indice utilisé lors de la construction des vertices pour avoir un indice
	 * unique de vertice.
	 */
	unsigned int m_originVertexIndex;

	void ConstructMesh();
	void DestructMesh();

	void ConstructPhysicsController();

	void ConstructVertexes();
	Vertex *NewVertex(unsigned short relx, unsigned short rely);
	Vertex *GetVertex(unsigned short x, unsigned short y) const;

	void InvalidateJointVertexesAndIndexes();

	void ConstructPolygones();
	void ConstructCenterColumnPolygones();
	void ConstructJointColumnPolygones(JointColumn *column, bool reverse);
	void AddMeshPolygonVertexes(Vertex *v1, Vertex *v2, Vertex *v3, bool reverse);

	void SetNormal(Vertex *vertexCenter, bool intern) const;

public:
	KX_Chunk(KX_ChunkNode *node, RAS_MaterialBucket *m_bucket);
	virtual ~KX_Chunk();

	/// creation du mesh avec joint des vertices du chunk avec ceux d'à cotés si neccesaire
	void UpdateMesh();
	void EndUpdateMesh();
	void ReconstructMesh();
	void RenderMesh(RAS_IRasterizer *rasty, KX_Camera *cam);

	inline KX_ChunkNode* GetNode() const
	{
		return m_node;
	}

	inline bool GetVisible() const
	{
		return m_visible;
	}

	inline void SetVisible(bool visible)
	{
		m_visible = visible;
	}
};

#endif // __KX_CHUNK_H__
