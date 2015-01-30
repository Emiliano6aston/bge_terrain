#include "KX_Chunk.h"
#include "KX_Terrain.h"
#include "KX_Camera.h"

#include "KX_PythonInit.h"

#include "RAS_IRasterizer.h"

#define DEBUG(msg) std::cout << msg << std::endl;

static const short INDICE_JOINT_LEFT = -1;
static const short INDICE_JOINT_RIGHT = -3;
static const short INDICE_JOINT_FRONT = -2;
static const short INDICE_JOINT_BACK = -4;

KX_Chunk::KX_Chunk(vector2DInt pos, KX_Terrain *terrain)
	:m_position(pos),
	m_terrain(terrain),
	m_meshSlot(NULL),
	m_lastSubDivision(0),
	m_culled(false)
{
// 	DEBUG("Create chunk, pos : " << pos.m_x << ", " << pos.m_y);
	//On crée la boite
	/* <---------
	 * |******** /\
	 * |*      * |
	 * |******** |
	 * \/------->
	 */ 
	const float size=m_terrain->GetChunkSize();
	const float height=m_terrain->GetMaxHeight();
	m_box[0] = MT_Point3(pos.m_x * size, pos.m_y * size, 0);
	m_box[1] = MT_Point3(pos.m_x * size + size, pos.m_y * size, 0);
	m_box[2] = MT_Point3(pos.m_x * size + size, pos.m_y * size + size, 0);
	m_box[3] = MT_Point3(pos.m_x * size, pos.m_y * size + size, 0);
	m_box[4] = MT_Point3(pos.m_x * size, pos.m_y * size, height);
	m_box[5] = MT_Point3(pos.m_x * size + size, pos.m_y * size, height);
	m_box[6] = MT_Point3(pos.m_x * size + size, pos.m_y * size + size, height);
	m_box[7] = MT_Point3(pos.m_x * size, pos.m_y * size + size, height);
	m_subDivisions = 2;
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

void KX_Chunk::ConstructMesh(const bool jointLeft, const bool jointRight, const bool jointFront, const bool jointBack)
{

	if (m_meshSlot)
		delete m_meshSlot;

	m_meshSlot = new RAS_MeshSlot();
	m_meshSlot->init(NULL, 3);//on utilise des triangles

	//La taille "réelle" du chunk
	const float size = m_terrain->GetChunkSize();
	// la valeur maximal du bruit de perlin
	const float maxheight = m_terrain->GetMaxHeight();
	//Calcul de l'intervalle entre deux points
	const float interval = size / m_subDivisions;
	// la position en bas a gauche
	const MT_Point2 realPos(m_position.m_x * size, m_position.m_y * size);

	// la liste contenant toutes les colonnes qui seront relus plus tard pour créer des faces
	short columnList[m_subDivisions+1][m_subDivisions+1] = {-5};
	unsigned int columnIndex = 0;

	// on construit tous les vertices et on stocke leur numero dans des colonnes
	for(float x = realPos.x(); x <= realPos.x() + size; x += interval)
	{
		unsigned int vertexIndex = 0;
		for(float y = realPos.y(); y <= realPos.y() + size; y += interval)
		{
			// si la colonne est paire alors on peut avoir des exeptions

			if (jointBack && vertexIndex == m_subDivisions && columnIndex % 2)
			{
				// on indique que ce vertice est inexistant car il y a une jointure en haut
				columnList[columnIndex][vertexIndex++] = INDICE_JOINT_BACK; 
			}
			else if (jointFront && vertexIndex == 0 && columnIndex % 2)
			{
				// on indique que ce vertice est inexistant car il y a une jointure en bas
				columnList[columnIndex][vertexIndex++] = INDICE_JOINT_FRONT; 
			}

			else if (jointRight && columnIndex == m_subDivisions && vertexIndex % 2)
			{
				// on indique que ce vertice est inexistant car il y a une jointure à gauche ou à droite
				columnList[columnIndex][vertexIndex++] = INDICE_JOINT_RIGHT; 
			}
			else if (jointLeft && columnIndex == 0 && vertexIndex % 2)
			{
				// on indique que ce vertice est inexistant car il y a une jointure à gauche ou à droite
				columnList[columnIndex][vertexIndex++] = INDICE_JOINT_LEFT; 
			}

			else
			{
				//Remplissage de l'uv lambda:
				MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
				//bourrage de vertices
				columnList[columnIndex][vertexIndex++] = m_meshSlot->AddVertex(
													RAS_TexVert(MT_Point3(x, y, BLI_hnoise(10., x, y, 0.) * maxheight),
													uvs_buffer,
													MT_Vector4(1., 1., 1., 1.),
													0, // Couleur + transparence
													MT_Vector3(0, 0, 1.),
													true, 0));
			}
		}
		++columnIndex;
	}

	// on lit toutes les colonnes pour créer des faces
	for (unsigned int columnIndex = 0; columnIndex < m_subDivisions; ++columnIndex)
	{
		for (unsigned int vertexIndex = 0; vertexIndex < m_subDivisions; ++vertexIndex)
		{
			bool side = columnIndex < m_subDivisions / 2;
			bool firstTriangle = true;
			bool secondTriangle = true;
			short firstIndex = columnList[columnIndex][vertexIndex];
			short secondIndex = columnList[columnIndex+1][vertexIndex];
			short thirdIndex = columnList[columnIndex+1][vertexIndex+1];
			short fourthIndex = columnList[columnIndex][vertexIndex+1];

			if (fourthIndex == INDICE_JOINT_LEFT)
			{
				// si le vertice en bas a gauche est egale à INDICE_JOINT_LEFT on passe à celui plus bas
				fourthIndex = columnList[columnIndex][vertexIndex+2];
			}
			else if (firstIndex == INDICE_JOINT_LEFT)
			{
				firstIndex = fourthIndex;
				secondTriangle = false;
			}

			if (thirdIndex == INDICE_JOINT_RIGHT)
			{
				thirdIndex = columnList[columnIndex+1][vertexIndex+2];
			}
			else if (secondIndex == INDICE_JOINT_RIGHT)
			{
				secondIndex = firstIndex;
				firstTriangle = false;
			}

			if (secondIndex == INDICE_JOINT_FRONT)
			{
				if (side)
				{
					secondIndex = columnList[columnIndex+2][vertexIndex];
				}
				else
				{
					secondIndex = firstIndex;
					firstTriangle = false;
				}
			}
			else if (firstIndex == INDICE_JOINT_FRONT)
			{
				if (side)
				{
					firstIndex = fourthIndex;
					secondTriangle = false;
				}
				else
				{
					firstIndex = columnList[columnIndex-1][vertexIndex];
				}
			}

			if (thirdIndex == INDICE_JOINT_BACK)
			{
				if (side)
				{
					thirdIndex = fourthIndex;
					secondTriangle = false;
				}
				else
				{
					thirdIndex = columnList[columnIndex+2][vertexIndex+1];
				}
			}
			else if (fourthIndex == INDICE_JOINT_BACK)
			{
				if (side)
				{
					fourthIndex = columnList[columnIndex-1][vertexIndex+1];
				}
				else
				{
					fourthIndex = columnList[columnIndex+1][vertexIndex+1];
					secondTriangle = false;
				}
			}

			if (side)
			{
				if (firstTriangle)
				{
					// création du premier triangle
					m_meshSlot->AddPolygonVertex(firstIndex);
					m_meshSlot->AddPolygonVertex(secondIndex);
					m_meshSlot->AddPolygonVertex(thirdIndex);
				}
				if (secondTriangle)
				{
					// création du deuxieme triangle
					m_meshSlot->AddPolygonVertex(firstIndex);
					m_meshSlot->AddPolygonVertex(thirdIndex);
					m_meshSlot->AddPolygonVertex(fourthIndex);
				}
			}
			else
			{
				if (firstTriangle)
				{
					// création du premier triangle
					m_meshSlot->AddPolygonVertex(firstIndex);
					m_meshSlot->AddPolygonVertex(secondIndex);
					m_meshSlot->AddPolygonVertex(fourthIndex);
				}
				if (secondTriangle)
				{
					// création du deuxieme triangle
					m_meshSlot->AddPolygonVertex(secondIndex);
					m_meshSlot->AddPolygonVertex(thirdIndex);
					m_meshSlot->AddPolygonVertex(fourthIndex);
				}
			}
		}
	}
}

void KX_Chunk::Update(KX_Camera* cam)
{
	m_culled = IsCulled(cam);
	const float size = m_terrain->GetChunkSize();
	const float distance = MT_Point3(m_position.m_x * size, m_position.m_y * size, 0.).distance2(cam->NodeGetWorldPosition());
	m_subDivisions = m_terrain->GetSubdivision(distance);
}

void KX_Chunk::UpdateDisplayArrayDraw(RAS_IRasterizer* rasty)
{
	if (m_culled)
		return;

	KX_Chunk* chunkLeft = m_terrain->GetChunk(m_position.m_x - 1, m_position.m_y);
	KX_Chunk* chunkRight = m_terrain->GetChunk(m_position.m_x + 1, m_position.m_y);
	KX_Chunk* chunkFront = m_terrain->GetChunk(m_position.m_x, m_position.m_y - 1);
	KX_Chunk* chunkBack = m_terrain->GetChunk(m_position.m_x, m_position.m_y + 1);

	// ensemble de 4 variables verifiant si il y aura besoin d'une jointure
	const bool jointLeft = chunkLeft ? chunkLeft->GetSubDivision() < m_subDivisions : true;
	const bool jointRight = chunkRight ? chunkRight->GetSubDivision() < m_subDivisions : true;
	const bool jointFront = chunkFront ? chunkFront->GetSubDivision() < m_subDivisions : true;
	const bool jointBack = chunkBack ? chunkBack->GetSubDivision() < m_subDivisions : true;

	//Cleaning:
	if (m_subDivisions != m_lastSubDivision ||
		!m_meshSlot ||
		m_jointChunk[0] != jointLeft ||
		m_jointChunk[1] != jointRight ||
		m_jointChunk[2] != jointFront ||
		m_jointChunk[3] != jointBack)
	{
		m_jointChunk[0] = jointLeft;
		m_jointChunk[1] = jointRight;
		m_jointChunk[2] = jointFront;
		m_jointChunk[3] = jointBack;
		ConstructMesh(jointLeft, jointRight, jointFront, jointBack);
	}

	if (m_meshSlot)
		rasty->IndexPrimitives(*m_meshSlot);

	for (int i = 0; i < 4; i++)
	{
		static MT_Vector3 color(1., 1., 1.);
		KX_RasterizerDrawDebugLine(m_box[i], m_box[i + 4], color);
		KX_RasterizerDrawDebugLine(m_box[i], m_box[(i < 3) ? i + 1 : 0], color);
		KX_RasterizerDrawDebugLine(m_box[i + 4], m_box[(i < 3) ? i + 5 : 4], color);
	}
}

void KX_Chunk::EndUpdate()
{
	m_lastSubDivision=m_subDivisions;
}



