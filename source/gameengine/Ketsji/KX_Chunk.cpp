#include "KX_Chunk.h"
#include "KX_Terrain.h"

#include "KX_Camera.h"
#include "KX_PythonInit.h"

#include "RAS_IRasterizer.h"
#include "RAS_MaterialBucket.h"

#include <stdio.h>

#define COLORED_PRINT(msg, color) std::cout << /*"\033[" << color << "m" <<*/ msg << /*"\033[30m" <<*/ std::endl;

#define DEBUG(msg) COLORED_PRINT("Debug : " << msg, 30);
#define WARNING(msg) COLORED_PRINT("Warning : " << msg, 33);
#define INFO(msg) COLORED_PRINT("Info : " << msg, 37);
#define ERROR(msg) COLORED_PRINT("Error : " << msg, 31);
#define SUCCES(msg) COLORED_PRINT("Succes : " << msg, 32);

#define DEBUGNOENDL(msg) std::cout << msg;

#define NEWVERT RAS_TexVert(MT_Point3(x, y, BLI_hnoise(60., x, y, 0.) * maxheight), \
												uvs_buffer, \
												MT_Vector4(1., 1., 1., 1.), \
												0, \
												MT_Vector3(0, 0, 1.), \
												true, 0) \

#define INDEX_JOINT -1

unsigned int KX_Chunk::m_chunkActive = 0;
static const unsigned short polyCount = 8;

KX_Chunk::KX_Chunk(short x, short y, unsigned short relativesize, unsigned short level, KX_Terrain *terrain)
	:KX_ChunkNode(x, y, relativesize, level, terrain),
	m_meshSlot(NULL),
	m_jointSlot(NULL),
	m_lastHasJointLeft(false),
	m_lastHasJointRight(false),
	m_lastHasJointFront(false),
	m_lastHasJointBack(false)
{
	m_chunkActive++;
	// la taille et sa moitié du chunk
	const float size = m_terrain->GetChunkSize();
	const float width = size / 2 * relativesize;

	// la hauteur maximal du chunk
	const float maxheight = m_terrain->GetMaxHeight();

	// le rayon du chunk
	m_radius2 = (width * width) * 2;

	// la coordonnée reel du chunk
	const float realX = x * size;
	const float realY = y * size;
	m_realPos = MT_Point2(realX, realY);

	// creation de la boite utilisé pour le frustum culling
	m_box[0] = MT_Point3(realX - width, realY - width, 0.);
	m_box[1] = MT_Point3(realX + width, realY - width, 0.);
	m_box[2] = MT_Point3(realX + width, realY + width, 0.);
	m_box[3] = MT_Point3(realX - width, realY + width, 0.);
	m_box[4] = MT_Point3(realX - width, realY - width, maxheight);
	m_box[5] = MT_Point3(realX + width, realY - width, maxheight);
	m_box[6] = MT_Point3(realX + width, realY + width, maxheight);
	m_box[7] = MT_Point3(realX - width, realY + width, maxheight);
}

KX_Chunk::~KX_Chunk()
{
	m_chunkActive--;
	if (m_meshSlot)
		delete m_meshSlot;
	if (m_jointSlot)
		delete m_jointSlot;
}

struct KX_Chunk::JointColumn
{
	short* columnExterne;
	short* columnInterne;
	unsigned short vertexCount;

	JointColumn(const unsigned short vertexcount)
		:vertexCount(vertexcount)
	{
		columnExterne = (short*)malloc(vertexcount * sizeof(short));
		columnInterne = (short*)malloc((vertexcount - 2) * sizeof(short));
	}

	~JointColumn()
	{
		free(columnExterne);
		free(columnInterne);
	}
};

void KX_Chunk::MarkCulled(KX_Camera* cam)
{
	m_culled = cam->BoxInsideFrustum(m_box) == KX_Camera::OUTSIDE;
}

void KX_Chunk::ConstructMesh()
{
	double starttime = KX_GetActiveEngine()->GetRealTime();
	if (m_meshSlot)
		delete m_meshSlot;

	m_meshSlot = new RAS_MeshSlot();
	m_meshSlot->init(NULL, 3);//on utilise des triangles

	//La taille "réelle" du chunk
	const float size = m_terrain->GetChunkSize();
	// la valeur maximal du bruit de perlin
	const float maxheight = m_terrain->GetMaxHeight();
	// le nombre de poly en largeur
	const unsigned short vertexCountInterne = polyCount - 1;
	//Calcul de l'intervalle entre deux points
	const float interval = size * m_relativeSize / polyCount;
	// la motie de la largeur du chunk - interval car on ne fait pas les bords
	const float width = size / 2 * m_relativeSize - interval;

	// la liste contenant toutes les colonnes qui seront relus plus tard pour créer des faces
	short columnList[vertexCountInterne][vertexCountInterne];
	m_meshSlot->CurrentDisplayArray()->m_vertex.reserve(vertexCountInterne * vertexCountInterne);

	double starttimevertex = KX_GetActiveEngine()->GetRealTime();
	// on construit tous les vertices et on stocke leur numero dans des colonnes
	for(unsigned short columnIndex = 0; columnIndex < vertexCountInterne; ++columnIndex)
	{
		const float x = m_realPos.x() - width + interval * columnIndex;
		for(unsigned short vertexIndex = 0; vertexIndex < vertexCountInterne; ++vertexIndex)
		{
			const float y = m_realPos.y() - width + interval * vertexIndex;
			//Remplissage de l'uv lambda:
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			//bourrage de vertices
			columnList[columnIndex][vertexIndex] = m_meshSlot->AddVertex(NEWVERT);
		}
	}
	double endtimevertex = KX_GetActiveEngine()->GetRealTime();
// 	DEBUG("adding vertex spend " << endtimevertex - starttimevertex << " time");
	double starttimeindex = KX_GetActiveEngine()->GetRealTime();
	// on lit toutes les colonnes pour créer des faces
	for (unsigned int columnIndex = 0; columnIndex < polyCount-2; ++columnIndex)
	{
		for (unsigned int vertexIndex = 0; vertexIndex < polyCount-2; ++vertexIndex)
		{
			const short firstIndex = columnList[columnIndex][vertexIndex];
			const short secondIndex = columnList[columnIndex+1][vertexIndex];
			const short thirdIndex = columnList[columnIndex+1][vertexIndex+1];
			const short fourthIndex = columnList[columnIndex][vertexIndex+1];

			// création du premier triangle
			m_meshSlot->AddPolygonVertex(firstIndex);
			m_meshSlot->AddPolygonVertex(secondIndex);
			m_meshSlot->AddPolygonVertex(thirdIndex);

			// création du deuxieme triangle
			m_meshSlot->AddPolygonVertex(firstIndex);
			m_meshSlot->AddPolygonVertex(thirdIndex);
			m_meshSlot->AddPolygonVertex(fourthIndex);
		}
	}
	double endtime = KX_GetActiveEngine()->GetRealTime();
// 	DEBUG("adding index spend " << endtime - starttimeindex << " time");
// 	DEBUG("add " << m_meshSlot->CurrentDisplayArray()->m_vertex.size() << " vertex and " << m_meshSlot->CurrentDisplayArray()->m_index.size() << " index");
// 	DEBUG(__func__ << " spend " << endtime - starttime << " time");
}

void KX_Chunk::ConstructJoint()
{
// 	double starttime = KX_GetActiveEngine()->GetRealTime();
	if (m_jointSlot)
		delete m_jointSlot;

	m_jointSlot = new RAS_MeshSlot();
	m_jointSlot->init(NULL, 3);//on utilise des triangles

	// le nombre de poly en largeur
	const unsigned short vertexCountExterne = polyCount + 1;
	const unsigned short vertexCountInterne = polyCount - 1;
	//La taille "réelle" du chunk
	const float size = m_terrain->GetChunkSize();
	// la motie de la largeur du chunk
	const float width = size / 2 * m_relativeSize;
	// la valeur maximal du bruit de perlin
	const float maxheight = m_terrain->GetMaxHeight();
	//Calcul de l'intervalle entre deux points
	const float interval = size * m_relativeSize / polyCount;
	
	const float startx = m_realPos.x() - width;
	const float endx = m_realPos.x() + width;
	const float starty = m_realPos.y() - width;
	const float endy = m_realPos.y() + width;

	/*       1(left)     2(right)
	 *       |           |
	 *       |           |
	 * / \ s−−−u−−−u−−−u−−−s
	 *  |  |\|_|__/|__/|_/_|___3(back)
	 *  |  | \ | / | / |/| |
	 *  |  u−−−s−−−u−−−s−−−u
	 *  |  |\| |       |\| |
	 *  |  | \ |       | \ |
	 *  |  u−−−u       u−−−u
	 *  |  |\| |       |\| |
	 *  |  | \ |       | \ |
	 *  |  u−−−s−−−u−−−s−−−u
	 *  |  |_/_| /_| /_|\|_|___4(front)
	 *  |  |/| |/| |/  | \ |
	 *  |  s−−−u−−−u−−−u−−−s
	 *  |
	 *  |__________________\
	 *                     /
	 * 
	 * s = shared, vertice commun
	 * u = unique, vertice independant
	 */

	JointColumn jointColumnLeft(vertexCountExterne);
	JointColumn jointColumnRight(vertexCountExterne);
	JointColumn jointColumnFront(vertexCountExterne);
	JointColumn jointColumnBack(vertexCountExterne);

	/* construction de tous les vertices contenue dans les colonnes internes de gauche et de droite, on construit
	 * aussi les vertice communs entre les colonnes internes (s sur le schema)
	 */
	for (unsigned short vertexIndex = 0; vertexIndex < vertexCountInterne; ++vertexIndex)
	{
		const float y = starty + interval * (1 + vertexIndex);
		{ // colonne de gauche interne
			const float x = startx + interval;
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			short index = m_jointSlot->AddVertex(NEWVERT);
			jointColumnLeft.columnInterne[vertexIndex] = index;
			if (vertexIndex == 0)
			{
				jointColumnFront.columnInterne[0] = index;
			}
			else if (vertexIndex == (vertexCountInterne-1))
			{
				jointColumnBack.columnInterne[0] = index;
			}
		}
		{ // colonne de droite interne
			const float x = endx - interval;
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			short index = m_jointSlot->AddVertex(NEWVERT);
			jointColumnRight.columnInterne[vertexIndex] = index;
			if (vertexIndex == 0)
			{
				jointColumnFront.columnInterne[vertexCountInterne-1] = index;
			}
			else if (vertexIndex == (vertexCountInterne-1))
			{
				jointColumnBack.columnInterne[vertexCountInterne-1] = index;
			}
		}
	}
	// construction des vertices externes de gauche et de droite
	for (unsigned short vertexIndex = 0; vertexIndex < vertexCountExterne; ++vertexIndex)
	{
		const float y = starty + interval * vertexIndex;
		{ // colonne de gauche extern
			const float x = startx;
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			short index = (vertexIndex % 2 && m_lastHasJointLeft) ? INDEX_JOINT : m_jointSlot->AddVertex(NEWVERT);
			jointColumnLeft.columnExterne[vertexIndex] = index;
			if (vertexIndex == 0)
			{
				jointColumnFront.columnExterne[0] = index;
			}
			else if (vertexIndex == (vertexCountExterne-1))
			{
				jointColumnBack.columnExterne[0] = index;
			}
		}
		{ // colonne de droite extern
			const float x = endx;
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			short index = (vertexIndex % 2 && m_lastHasJointRight) ? INDEX_JOINT : m_jointSlot->AddVertex(NEWVERT);
			jointColumnRight.columnExterne[vertexIndex] = index;
			if (vertexIndex == 0)
			{
				jointColumnFront.columnExterne[vertexCountExterne-1] = index;
			}
			else if (vertexIndex == (vertexCountExterne-1))
			{
				jointColumnBack.columnExterne[vertexCountExterne-1] = index;
			}
		}
	}

	/* construction des vertices internes des colonnes du haut et du bas, on ne reconstruit pas les vertices communs
	 * de ce fait la boucle et plus petite de 2 iterations.
	 */
	for (unsigned short vertexIndex = 1; vertexIndex < (vertexCountInterne - 1); ++vertexIndex)
	{
		const float x = startx + interval * (1 + vertexIndex);
		{ // colonne de haut interne
			const float y = starty + interval;
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			unsigned int index = m_jointSlot->AddVertex(NEWVERT);
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			jointColumnFront.columnInterne[vertexIndex] = index;
		}
		{ // colonne de bas interne
			const float y = endy - interval;
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			unsigned int index = m_jointSlot->AddVertex(NEWVERT);
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			jointColumnBack.columnInterne[vertexIndex] = index;
		}
	}

	for (unsigned short vertexIndex = 1; vertexIndex < (vertexCountExterne - 1); ++vertexIndex)
	{
		const float x = startx + interval * vertexIndex;
		{ // colonne de haut extern
			const float y = starty;
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			unsigned int index = (vertexIndex % 2 && m_lastHasJointFront) ? INDEX_JOINT : m_jointSlot->AddVertex(NEWVERT);
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			jointColumnFront.columnExterne[vertexIndex] = index;
		}
		{ // colonne de bas extern
			const float y = endy;
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			unsigned int index = (vertexIndex % 2 && m_lastHasJointBack) ? INDEX_JOINT : m_jointSlot->AddVertex(NEWVERT);
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			jointColumnBack.columnExterne[vertexIndex] = index;
		}
	}

	ConstructJointColumnPoly(jointColumnLeft, polyCount);
	ConstructJointColumnPoly(jointColumnRight, polyCount);
	ConstructJointColumnPoly(jointColumnFront, polyCount);
	ConstructJointColumnPoly(jointColumnBack, polyCount);
// 	double endtime = KX_GetActiveEngine()->GetRealTime();
// 	DEBUG(__func__ << " spend " << endtime - starttime << " time");
}

void KX_Chunk::ConstructJointColumnPoly(const JointColumn& column, unsigned short polyCount)
{
	for (unsigned short vertexIndex = 0; vertexIndex < polyCount; ++vertexIndex)
	{
		bool firstTriangle = true;
		bool secondTriangle = true;
		short firstIndex, secondIndex, thirdIndex, fourthIndex;
		firstIndex = column.columnExterne[vertexIndex]; // les seuls points fixes
		fourthIndex = column.columnExterne[vertexIndex+1];
		if (fourthIndex == INDEX_JOINT)
		{
			fourthIndex = column.columnExterne[vertexIndex+2];
		}

		if (vertexIndex == 0)
		{
			/*
			 * 1 
			 * |\
			 * | \
			 * 3--2
			 */

			secondIndex = column.columnInterne[0];
			thirdIndex = fourthIndex;
			secondTriangle = false;
		}
		else if (vertexIndex == (polyCount-1))
		{
			/*
			 * 1--2
			 * | /
			 * |/
			 * 3
			 */

			if (firstIndex == INDEX_JOINT)
			{
				firstTriangle = false;
			}
			secondIndex = column.columnInterne[vertexIndex-1];
			thirdIndex = column.columnExterne[polyCount];
			secondTriangle = false;
		}
		else
		{
			/*
			 * 1--2
			 * |\ |
			 * | \|
			 * 4--3
			 */
			secondIndex = column.columnInterne[vertexIndex-1];
			thirdIndex = column.columnInterne[vertexIndex];
			if (firstIndex == INDEX_JOINT)
			{
				firstIndex = fourthIndex;
				secondTriangle = false;
			}
		}
		if (firstTriangle)
		{
			// création du premier triangle
			m_jointSlot->AddPolygonVertex(firstIndex);
			m_jointSlot->AddPolygonVertex(secondIndex);
			m_jointSlot->AddPolygonVertex(thirdIndex);
		}
		if (secondTriangle)
		{
			// création du deuxieme triangle
			m_jointSlot->AddPolygonVertex(firstIndex);
			m_jointSlot->AddPolygonVertex(thirdIndex);
			m_jointSlot->AddPolygonVertex(fourthIndex);
		}
	}
}

bool KX_Chunk::NeedCreateSubChunks(KX_Camera* cam) const
{
	const float distance2 = MT_Point3(m_realPos.x(), m_realPos.y(), 0.).distance2(cam->NodeGetWorldPosition()) - m_radius2;
	const unsigned short subdivision = m_terrain->GetSubdivision(distance2);
	return subdivision >= (m_level*2);
}

void KX_Chunk::ConstructSubChunks()
{
	if (IsFinalNode())
	{
		if (m_meshSlot)
		{
			delete m_meshSlot;
			m_meshSlot = NULL;
		}
		if (m_jointSlot)
		{
			delete m_jointSlot;
			m_jointSlot = NULL;
		}
	}
	KX_ChunkNode::ConstructSubChunks();
}

void KX_Chunk::UpdateMesh()
{
	if (IsFinalNode()) 
	{
		if (!m_meshSlot)
			ConstructMesh();

		KX_ChunkNode* chunkLeft = m_terrain->GetChunkRelativePosition(m_relativePosX - 1, m_relativePosY);
		KX_ChunkNode* chunkRight = m_terrain->GetChunkRelativePosition(m_relativePosX + 1, m_relativePosY);
		KX_ChunkNode* chunkFront = m_terrain->GetChunkRelativePosition(m_relativePosX, m_relativePosY - 1);
		KX_ChunkNode* chunkBack = m_terrain->GetChunkRelativePosition(m_relativePosX, m_relativePosY + 1);

		// ensemble de 4 variables verifiant si il y aura besoin d'une jointure
		const bool hasJointLeft = chunkLeft ? chunkLeft->GetLevel() < m_level : false;
		const bool hasJointRight = chunkRight ? chunkRight->GetLevel() < m_level : false;
		const bool hasJointFront = chunkFront ? chunkFront->GetLevel() < m_level : false;
		const bool hasJointBack = chunkBack ? chunkBack->GetLevel() < m_level : false;

		if (m_lastHasJointLeft != hasJointLeft ||
			m_lastHasJointRight != hasJointRight ||
			m_lastHasJointFront != hasJointFront ||
			m_lastHasJointBack != hasJointBack ||
			!m_jointSlot)
		{
			m_lastHasJointLeft = hasJointLeft;
			m_lastHasJointRight = hasJointRight;
			m_lastHasJointFront = hasJointFront;
			m_lastHasJointBack = hasJointBack;
			ConstructJoint();
		}
	}
	else
		KX_ChunkNode::UpdateMesh();
}

void KX_Chunk::RenderMesh(RAS_IRasterizer* rasty)
{
	if (IsFinalNode())
	{
		rasty->IndexPrimitives(*m_meshSlot);
		rasty->IndexPrimitives(*m_jointSlot);
	}
	else
		KX_ChunkNode::RenderMesh(rasty);

	if (m_level > 1)
	{
		/*for (int i = 0; i < 4; i++)
		{
			static MT_Vector3 color(1., 1., 1.);
			KX_RasterizerDrawDebugLine(m_box[i], m_box[i + 4], color);
			KX_RasterizerDrawDebugLine(m_box[i], m_box[(i < 3) ? i + 1 : 0], color);
			KX_RasterizerDrawDebugLine(m_box[i + 4], m_box[(i < 3) ? i + 5 : 4], color);
		}*/
		KX_RasterizerDrawDebugLine(MT_Point3(m_realPos.x(), m_realPos.y(), 0.),
								MT_Point3(m_realPos.x(), m_realPos.y(), m_level*2),
								MT_Vector3(1., 0., 0.));
	}
}