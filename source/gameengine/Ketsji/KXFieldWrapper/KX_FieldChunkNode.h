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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Porteries Tristan
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file KX_FieldChunkNode.h
 *  \ingroup ketsji
 */

#ifndef __KX_FIELD_CHUNK_NODE_H__
#define __KX_FIELD_CHUNK_NODE_H__

#include "KX_FieldChunkBase.h"

class KX_Field;
class KX_FieldChunk;
class KX_Camera;

class KX_FieldChunkNode : public KX_FieldChunkBase
{
private:
	/// Les infomations sur la densité, hauteur de la boite ...
	KX_Field* m_field;

	/**
	 * Les coordonnées en bas a gauche du pixel par rapport à l'image, ces information sont
	 * envoyé lors de la construction mais un noeud les stocke pour pouvoir créer par la suite
	 * les sous pixels.
	 */
	short m_posX;
	short m_posY;

	/// Le point du milieu du pixel en 3d.
	MT_Point3 m_realMiddlePos;

	/// La largeur de l'espace couvert par le groupe de pixels.
	unsigned short m_size;

	/// Le rayon du noeud
	float m_radius;

	/// Le pixel est precedement visible ou non.
	bool m_lastVisible;

	/// Le pixel est precedement à la bonne distance de la caméra.
	bool m_lastIn;

	/// La distance entre la caméra et le noeud lors du dernier update.
	float m_lastDistanceToCamera;

	/**
	 * Tableau des 4 sous pixels possible, ces sous pixels peuvent etre des groupes
	 * de pixels ou des pixel finaux.
	 */
	KX_FieldChunkNode** m_nodeList;
	KX_FieldChunk* m_chunk;


	/**
	 * Generation du milieu du noeud sur un terrain en 3d, on effectue un lancé de 
	 * rayons verticale sur le terrain pour trouver ce milieu, cette fonction modifie
	 * directement m_middlePos.
	 * \param middle Le milieu du pixel en 2d.
	 */
	void GenerateMiddlePixelPoint(MT_Vector2 middle);

	/**
	 * Test pour savoir si le noeud est visible, on se base sur le frustum culling d'un sphere.
	 * C'est plus rapide mais moins précis.
	 * \param cam La camera sur lequel tester le frutum culling.
	 * \return Vrai si la sphère est dans le champ de la caméra donc visible.
	 */
	bool MarkVisible(KX_Camera* cam) const;

	void ConstructNodeListChunk();
	void DestructNodeListChunk();
	void ConstructNodeList();
	void DestructNodeList();
	void ConstructChunk();
	void DestructChunk();

	void UpdateAllNodes(KX_Camera* cam);

public:
	/**
	 * Constructeur d'un noeud.
	 * \param field Toutes les infomations sur la densité du quadrillage, 
	 * la distance maximal, la hauteur de la boite pour le frustum culling...
	 * \param posX La position en x en bas à gauche du pixel par rapport à l'image 
	 * (entre -et+ la moitié de la taille de l'image).
	 * \param posY Pareille que pour posX mais sur l'axe y.
	 * \param size Le nombre de pixels de l'image que couvre ce noeud.
	 */
	KX_FieldChunkNode(KX_Field* field,
					   short posX,
					   short posY,
					   unsigned short size);
	virtual ~KX_FieldChunkNode();

	/**
	 * Mise à jour du noeud, on verifie si le pixel est toujours a la bonne distance,
	 * si il est visible. Si il est visible est a la bonne distance on appelle cette même fonction
	 * pour ses sous pixels.
	 * \param cam La camera utilisé pour le frustum culling est la distance.
	 */
	void Update(KX_Camera* cam);

	inline KX_Field*		GetField() const { return m_field; }
	inline short			GetRelativePosX() const { return m_posX; }
	inline short			GetRelativePosY() const { return m_posY; }
	inline MT_Point3		GetRealMiddlePosition() const { return m_realMiddlePos; }
	inline unsigned short	GetSize() const { return m_size; }
	inline float			GetLastDistanceToCamera() const { return m_lastDistanceToCamera; }

	/*void DrawDebugPixel(KX_Camera* cam) const;*/
	void DrawDebugBox(KX_Camera* cam) const;
	static unsigned int activeChunkNodes;
};

#endif // __KX_FIELD_CHUNK_NODE_H__