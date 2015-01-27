#include "KX_Chunk.h"
#include "KX_Terrain.h"
#include "KX_Camera.h"

#include "RAS_IRasterizer.h"

#define DEBUG(msg) std::cout << msg << std::endl;

KX_Chunk::KX_Chunk(vector2DInt pos, KX_Terrain *terrain, RAS_MaterialBucket* bucket, int id)
	:m_id(id),
	m_position(pos),
	m_terrain(terrain),
	m_meshSlot(NULL),
	m_material(bucket),
	m_lastSubDivision(0)
{
// 	DEBUG("Create chunk, pos : " << pos.m_x << ", " << pos.m_y << ", id : " << id);
	//On crée la bwate
	/* <---------
	 * |******** /\
	 * |*      * |
	 * |******** |
	 * \/------->
	 */ 
	const float size=m_terrain->GetChunkSize();
	const float height=m_terrain->GetMaxHeight();
	m_box[0]=MT_Point3(pos.m_x * size, pos.m_y * size, 0);
	m_box[0]=MT_Point3(pos.m_x * size + size, pos.m_y * size, 0);
	m_box[0]=MT_Point3(pos.m_x * size + size, pos.m_y * size + size, 0);
	m_box[0]=MT_Point3(pos.m_x * size, pos.m_y * size + size, 0);
	m_box[0]=MT_Point3(pos.m_x * size, pos.m_y * size, height);
	m_box[0]=MT_Point3(pos.m_x * size + size, pos.m_y * size, height);
	m_box[0]=MT_Point3(pos.m_x * size + size, pos.m_y * size + size, height);
	m_box[0]=MT_Point3(pos.m_x * size, pos.m_y * size + size, height);
	m_subDivisions = (rand() % 2) ? 8 : 16;
}

KX_Chunk::~KX_Chunk()
{
	if (m_meshSlot)
		delete m_meshSlot;
}

bool KX_Chunk::IsCulled(KX_Camera* cam) const
{
	return cam->BoxInsideFrustum(m_box) == KX_Camera::OUTSIDE;
}

void KX_Chunk::Update(KX_Camera* cam)
{
	m_culled = IsCulled(cam);
	const float size = m_terrain->GetChunkSize();
	const float distance = MT_Point3(m_position.m_x * size, m_position.m_y * size, 0.).distance2(cam->NodeGetWorldPosition());
// 	m_subDivisions = m_terrain->GetSubdivision(0.);
}

void KX_Chunk::UpdateDisplayArrayDraw(const MT_Transform& cameratrans, RAS_IRasterizer* rasty)
{
	if (m_culled)
		return;

	//Cleaning:
	if (m_subDivisions != m_lastSubDivision)
	{
		if (m_meshSlot)
			delete m_meshSlot;

		m_meshSlot = new RAS_MeshSlot();
		m_meshSlot->init(m_material, 4);//temporairement on utilise des carrés

		// tous les chunk adjacent
		KX_Chunk* chunkLeft = m_terrain->GetChunk(m_position.m_x - 1, m_position.m_y);
		KX_Chunk* chunkRight = m_terrain->GetChunk(m_position.m_x + 1, m_position.m_y);
		KX_Chunk* chunkFront = m_terrain->GetChunk(m_position.m_x, m_position.m_y + 1);
		KX_Chunk* chunkBack = m_terrain->GetChunk(m_position.m_x, m_position.m_y - 1);

		// ensemble de 4 variables verifiant si il y aura besoin d'une jointure
		const bool jointLeft = chunkLeft ? chunkLeft->GetSubDivision() < m_subDivisions : false;
		const bool jointRight = chunkRight ? chunkRight->GetSubDivision() < m_subDivisions : false;
		const bool jointFront = chunkFront ? chunkFront->GetSubDivision() < m_subDivisions : false;
		const bool jointBack = chunkBack ? chunkBack->GetSubDivision() < m_subDivisions : false;

		//La taille "réelle" du chunk
		const float size = m_terrain->GetChunkSize();
		// la valeur maximal du bruit de perlin
		const float maxheight = m_terrain->GetMaxHeight();
		//Calcul de l'intervalle entre deux points
		const float interval = size / m_subDivisions;
		// la position en bas a gauche
		const MT_Point2 realPos(m_position.m_x * size, m_position.m_y * size);

		// la liste contenant toutes les colonnes qui seront relus plus tard pour créer des faces
		unsigned short columnList[m_subDivisions+1][m_subDivisions+1];
		unsigned int columnIndex = 0;

		// on construit tous les vertices et on stocke leur numero dans des colonnes
		for(float x = realPos.x(); x <= realPos.x() + size; x += interval)
		{
			unsigned int vertexIndex = 0;
			for(float y = realPos.y(); y <= realPos.y() + size; y += interval)
			{
				float height = 0.;
				if (((jointFront && vertexIndex == m_subDivisions) || (jointBack && vertexIndex == 0)) && (columnIndex % 2))
					height = (BLI_hnoise(10., x - interval, y, 0.) + BLI_hnoise(10., x + interval, y, 0.)) / 2.;
				else if (((jointRight && columnIndex == m_subDivisions) || (jointLeft && columnIndex == 0)) && (vertexIndex % 2))
					height = (BLI_hnoise(10., x, y - interval, 0.) + BLI_hnoise(10., x, y + interval, 0.)) / 2.;
				else
					height = BLI_hnoise(10., x, y, 0.);

				//Remplissage de l'uv lambda:
				MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
				//bourrage de vertices
				columnList[columnIndex][vertexIndex++] = m_meshSlot->AddVertex(
														RAS_TexVert(MT_Point3(x, y, -height * maxheight),
														uvs_buffer,
														MT_Vector4(1., 1., 1., 1.),
														0, // Couleur + transparence
														MT_Vector3(0, 0, 1.),
														true, 0));
			}
			++columnIndex;
		}

		// on lit toutes les colonnes pour créer des faces
		for (unsigned int i = 0; i < m_subDivisions; ++i)
		{
			for (unsigned int j = 0; j < m_subDivisions; ++j)
			{
				m_meshSlot->AddPolygon(4);
				m_meshSlot->AddPolygonVertex(columnList[i][j]);
				m_meshSlot->AddPolygonVertex(columnList[i][j+1]);
				m_meshSlot->AddPolygonVertex(columnList[i+1][j+1]);
				m_meshSlot->AddPolygonVertex(columnList[i+1][j]);
			}
		}
	}
	while (m_material->ActivateMaterial(cameratrans, rasty))
		rasty->IndexPrimitives(*m_meshSlot);
}

void KX_Chunk::EndUpdate()
{
	m_lastSubDivision=m_subDivisions;
}



