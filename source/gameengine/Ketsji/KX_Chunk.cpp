#include "KX_Chunk.h"
#include "KX_Terrain.h"
#include "KX_Camera.h"

KX_Chunk::KX_Chunk(Vector2DInt pos, KX_Terrain *terrain, RAS_MaterialBucket* bucket)
	: m_terrain(terrain), m_material(bucket)
{
	//On crée la bwate
	/* <---------
	 * |******** /\
	 * |*      * |
	 * |******** |
	 * \/------->
	 */ 
	const float size=m_terrain->GetChunkSize();
	const float height=m_terrain->GetMaxHeight();
	m_box[0]=MT_Point3(pos.first*size, pos.second*size, 0);
	m_box[0]=MT_Point3(pos.first*size+size, pos.second*size, 0);
	m_box[0]=MT_Point3(pos.first*size+size, pos.second*size+size, 0);
	m_box[0]=MT_Point3(pos.first*size, pos.second*size+size, 0);
	m_box[0]=MT_Point3(pos.first*size, pos.second*size, height);
	m_box[0]=MT_Point3(pos.first*size+size, pos.second*size, height);
	m_box[0]=MT_Point3(pos.first*size+size, pos.second*size+size, height);
	m_box[0]=MT_Point3(pos.first*size, pos.second*size+size, height);
	
	m_displayArray=new RAS_DisplayArray;
}

KX_Chunk::~KX_Chunk()
{
	delete m_displayArray;
}

bool KX_Chunk::IsVisible(KX_Camera* cam) const
{
	return cam->BoxInsideFrustum(m_box) != KX_Camera::OUTSIDE;
}

void KX_Chunk::Update(KX_Camera* cam)
{
	m_subDivisions = m_terrain->GetSubdivision(0.);
}

void KX_Chunk::UpdateDisplayArray(Vector2DInt pos)
{
	//Cleaning:
	m_meshSlot = new RAS_MeshSlot();
	m_meshSlot->init(m_material, 4);//temporairement on use carrés
	
	//La taille "réelle" du chunk
	const float size = m_terrain->GetChunkSize();
	//Calcul de l'intervalle entere deux points
	const float interval = size / m_subDivisions;
	
	const ushort nbreVertices=m_subDivisions+4-1;
	
	unsigned int nbvertex = 0;
	for(float x = pos.first * size; x < pos.first * size + size; x += interval)
	{
		 for(float y = pos.second * size; y < pos.second * size + size; y += interval)
		 {
			 if (nbvertex % 4)
				 m_meshSlot->AddPolygon(4);

			//Remplissage de l'uv lambda:
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			//bourrage de vertices
			m_meshSlot->AddVertex(
				RAS_TexVert(MT_Point3(x, y, BLI_hnoise(10., x, y, 0.)),
				uvs_buffer,
				MT_Vector4(0, 0, 0, 0),
				0xFFFF, // Couleur + transparence
				MT_Vector3(0, 0, 0),
				true,
				0));
			 m_meshSlot->AddPolygonVertex(nbvertex++);
		 }
	}
	
	//on build le mesh:
	int c = 0;
	while(c!=nbreVertices*nbreVertices)
	{
		//Crée un polygone à 4 points:
		m_meshSlot->AddPolygon(c);
		c++;
		m_meshSlot->AddPolygon(c);
		c+=nbreVertices;
		m_meshSlot->AddPolygon(c);
		c--;
		m_meshSlot->AddPolygon(c);
		c-=nbreVertices+2;
	}
	
	/*
	for(int c=0;c!=size;c++)
	{
		
	}*/
}

void KX_Chunk::EndUpdate()
{
	m_lastSubDivision=m_subDivisions;
}



