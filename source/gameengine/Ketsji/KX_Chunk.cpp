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

KX_Chunk::KX_Chunk(int x, int y, unsigned int relativesize, unsigned short level, KX_Terrain *terrain)
	:m_relativePosX(x),
	m_relativePosY(y),
	m_relativeSize(relativesize),
	m_terrain(terrain),
	m_meshSlot(NULL),
	m_lastSubDivision(0),
	m_level(level),
	m_culled(false),
	m_hasSubChunks(false)
{
	// la taille et sa moitié du chunk
	const float size = m_terrain->GetChunkSize();
	const float width = size / 2 * relativesize;

	// le rayon du chunk
	m_radius = width * width * 2;

	// la hauteur maximal du chunk
	const float height = m_terrain->GetMaxHeight();

	// la coordonnée reel du chunk
	const float realX = x * size;
	const float realY = y * size;
	m_realPos = MT_Point2(realX, realY);

// 	DEBUG("x : " << x << ", y : " << y << ", real x : " << realX << ", real y : " << realY << ", size / 2 : " << width);

	// creation de la boite utilisé pour le frustum culling
	m_box[0] = MT_Point3(realX - width, realY - width, 0);
	m_box[1] = MT_Point3(realX + width, realY - width, 0);
	m_box[2] = MT_Point3(realX + width, realY + width, 0);
	m_box[3] = MT_Point3(realX - width, realY + width, 0);
	m_box[4] = MT_Point3(realX - width, realY - width, height);
	m_box[5] = MT_Point3(realX + width, realY - width, height);
	m_box[6] = MT_Point3(realX + width, realY + width, height);
	m_box[7] = MT_Point3(realX - width, realY + width, height);
}

KX_Chunk::~KX_Chunk()
{
	if (m_meshSlot)
		delete m_meshSlot;

	if (m_hasSubChunks)
	{
		delete m_subChunks[0];
		delete m_subChunks[1];
		delete m_subChunks[2];
		delete m_subChunks[3];
	}
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

	// le nombre de poly en largeur
	const unsigned short polySubdivisions = m_relativeSize * m_subDivisions;
	//La taille "réelle" du chunk
	const float size = m_terrain->GetChunkSize();
	// la motie de la largeur du chunk
	const float width = size / 2 * m_relativeSize;
	// la valeur maximal du bruit de perlin
	const float maxheight = m_terrain->GetMaxHeight();
	//Calcul de l'intervalle entre deux points
	const float interval = size * m_relativeSize / polySubdivisions;
	// la liste contenant toutes les colonnes qui seront relus plus tard pour créer des faces
	short columnList[polySubdivisions+1][polySubdivisions+1] = {-5};
	unsigned int columnIndex = 0;

	// on construit tous les vertices et on stocke leur numero dans des colonnes
	for(float x = m_realPos.x() - width; x <= m_realPos.x() + width; x += interval)
	{
		unsigned int vertexIndex = 0;
		for(float y = m_realPos.y() - width; y <= m_realPos.y() + width; y += interval)
		{
			// si la colonne est paire alors on peut avoir des exeptions

			if (jointBack && vertexIndex == polySubdivisions && columnIndex % 2)
			{
				// on indique que ce vertice est inexistant car il y a une jointure en haut
				columnList[columnIndex][vertexIndex++] = INDICE_JOINT_BACK; 
			}
			else if (jointFront && vertexIndex == 0 && columnIndex % 2)
			{
				// on indique que ce vertice est inexistant car il y a une jointure en bas
				columnList[columnIndex][vertexIndex++] = INDICE_JOINT_FRONT; 
			}

			else if (jointRight && columnIndex == polySubdivisions && vertexIndex % 2)
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
	for (unsigned int columnIndex = 0; columnIndex < polySubdivisions; ++columnIndex)
	{
		for (unsigned int vertexIndex = 0; vertexIndex < polySubdivisions; ++vertexIndex)
		{
			bool side = columnIndex < polySubdivisions / 2;
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
	const float distance = MT_Point3(m_realPos.x(), m_realPos.y(), 0.).distance2(cam->NodeGetWorldPosition()) + m_radius;
	m_subDivisions = m_terrain->GetSubdivision(distance);

	// creation des sous chunks
	if (m_subDivisions > (m_level * 2) && !m_culled)
	{
		if (!m_hasSubChunks)
		{
			if (m_meshSlot)
			{
				delete m_meshSlot;
				m_meshSlot = NULL;
			}

			m_hasSubChunks = true;

			int size = m_relativeSize / 2;
			int width = size / 2;

			m_subChunks[0] = new KX_Chunk(m_relativePosX - width, m_relativePosY - width, size, m_level * 2, m_terrain);
			m_subChunks[1] = new KX_Chunk(m_relativePosX + width, m_relativePosY - width, size, m_level * 2, m_terrain);
			m_subChunks[2] = new KX_Chunk(m_relativePosX - width, m_relativePosY + width, size, m_level * 2, m_terrain);
			m_subChunks[3] = new KX_Chunk(m_relativePosX + width, m_relativePosY + width, size, m_level * 2, m_terrain);
		}
	}

	else if (m_hasSubChunks)
	{
		m_hasSubChunks = false;
		delete m_subChunks[0];
		delete m_subChunks[1];
		delete m_subChunks[2];
		delete m_subChunks[3];
	}


	if (m_hasSubChunks && !m_culled)
	{
		m_subChunks[0]->Update(cam);
		m_subChunks[1]->Update(cam);
		m_subChunks[2]->Update(cam);
		m_subChunks[3]->Update(cam);
	}
}

void KX_Chunk::UpdateMesh()
{
	if (m_culled)
		return;

	if (m_hasSubChunks)
	{
		m_subChunks[0]->UpdateMesh();
		m_subChunks[1]->UpdateMesh();
		m_subChunks[2]->UpdateMesh();
		m_subChunks[3]->UpdateMesh();
	}

	else 
	{
		KX_Chunk* chunkLeft = m_terrain->GetChunkRelativePosition(m_relativePosX - 1, m_relativePosY);
		KX_Chunk* chunkRight = m_terrain->GetChunkRelativePosition(m_relativePosX + 1, m_relativePosY);
		KX_Chunk* chunkFront = m_terrain->GetChunkRelativePosition(m_relativePosX, m_relativePosY - 1);
		KX_Chunk* chunkBack = m_terrain->GetChunkRelativePosition(m_relativePosX, m_relativePosY + 1);

		// ensemble de 4 variables verifiant si il y aura besoin d'une jointure
		const bool jointLeft = chunkLeft ? chunkLeft->GetSubDivision() < m_subDivisions : false;
		const bool jointRight = chunkRight ? chunkRight->GetSubDivision() < m_subDivisions : false;
		const bool jointFront = chunkFront ? chunkFront->GetSubDivision() < m_subDivisions : false;
		const bool jointBack = chunkBack ? chunkBack->GetSubDivision() < m_subDivisions : false;

		if (m_subDivisions != m_lastSubDivision ||
			m_lastJointChunk[0] != jointLeft ||
			m_lastJointChunk[1] != jointRight ||
			m_lastJointChunk[2] != jointFront ||
			m_lastJointChunk[3] != jointBack ||
			!m_meshSlot)
		{
			m_lastJointChunk[0] = jointLeft;
			m_lastJointChunk[1] = jointRight;
			m_lastJointChunk[2] = jointFront;
			m_lastJointChunk[3] = jointBack;
			ConstructMesh(jointLeft, jointRight, jointFront, jointBack);
		}
	}

	for (int i = 0; i < 4; i++)
	{
		static MT_Vector3 color(1., 1., 1.);
		KX_RasterizerDrawDebugLine(m_box[i], m_box[i + 4], color);
		KX_RasterizerDrawDebugLine(m_box[i], m_box[(i < 3) ? i + 1 : 0], color);
		KX_RasterizerDrawDebugLine(m_box[i + 4], m_box[(i < 3) ? i + 5 : 4], color);
	}
	KX_RasterizerDrawDebugLine(MT_Point3(m_realPos.x(), m_realPos.y(), 0.),
							   MT_Point3(m_realPos.x(), m_realPos.y(), 1.),
							   MT_Vector3(1., 0., 0.));
}

void KX_Chunk::RenderMesh(RAS_IRasterizer* rasty)
{
	if (m_culled)
		return;

	if (m_hasSubChunks)
	{
		m_subChunks[0]->RenderMesh(rasty);
		m_subChunks[1]->RenderMesh(rasty);
		m_subChunks[2]->RenderMesh(rasty);
		m_subChunks[3]->RenderMesh(rasty);
	}
	else
		rasty->IndexPrimitives(*m_meshSlot);
}
void KX_Chunk::EndUpdate()
{
	m_lastSubDivision=m_subDivisions;
	if (m_hasSubChunks)
	{
		m_subChunks[0]->EndUpdate();
		m_subChunks[1]->EndUpdate();
		m_subChunks[2]->EndUpdate();
		m_subChunks[3]->EndUpdate();
	}
}

KX_Chunk* KX_Chunk::GetChunkRelativePosition(int x, int y)
{
	if((m_relativePosX - m_relativeSize) <= x && x <= (m_relativePosX + m_relativeSize) &&
		(m_relativePosY - m_relativeSize) <= y && y <= (m_relativePosY + m_relativeSize))
	{
		if (m_hasSubChunks)
		{
			for (unsigned short i = 0; i < 4; ++i)
			{
				KX_Chunk* ret = m_subChunks[i]->GetChunkRelativePosition(x, y);
				if (ret)
					return ret;
			}
		}
		else
			return this;
	}
	return NULL;
}