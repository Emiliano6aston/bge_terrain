#include "KX_Chunk.h"
#include "KX_Terrain.h"

#include "KX_Camera.h"
#include "KX_PythonInit.h"

#include "RAS_IRasterizer.h"

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

KX_Chunk::KX_Chunk(short x, short y, unsigned short relativesize, unsigned short level, KX_Terrain *terrain)
	:m_relativePosX(x),
	m_relativePosY(y),
	m_relativeSize(relativesize),
	m_terrain(terrain),
	m_meshSlot(NULL),
	m_lastSubDivision(0),
	m_level(level),
	m_lastHasJointLeft(false),
	m_lastHasJointRight(false),
	m_lastHasJointFront(false),
	m_lastHasJointBack(false),
	m_jointSlot(NULL),
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

 	INFO("x : " << x << ", y : " << y << ", real x : " << realX << ", real y : " << realY << ", size / 2 : " << width);

	// creation de la boite utilisé pour le frustum culling
	m_box[0] = MT_Point3(realX - width, realY - width, 0.);
	m_box[1] = MT_Point3(realX + width, realY - width, 0.);
	m_box[2] = MT_Point3(realX + width, realY + width, 0.);
	m_box[3] = MT_Point3(realX - width, realY + width, 0.);
	m_box[4] = MT_Point3(realX - width, realY - width, height);
	m_box[5] = MT_Point3(realX + width, realY - width, height);
	m_box[6] = MT_Point3(realX + width, realY + width, height);
	m_box[7] = MT_Point3(realX - width, realY + width, height);
}

KX_Chunk::~KX_Chunk()
{
	if (m_meshSlot)
		delete m_meshSlot;
	if (m_jointSlot)
		delete m_jointSlot;

	if (m_hasSubChunks)
	{
		delete m_subChunks[0];
		delete m_subChunks[1];
		delete m_subChunks[2];
		delete m_subChunks[3];
	}
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

bool KX_Chunk::IsCulled(KX_Camera* cam) const
{
	return cam->BoxInsideFrustum(m_box) == KX_Camera::OUTSIDE;
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
	const unsigned short polyCount = m_relativeSize * m_subDivisions;
	//Calcul de l'intervalle entre deux points
	const float interval = size * m_relativeSize / polyCount;
	// la motie de la largeur du chunk - interval car on ne fait pas les bords
	const float width = size / 2 * m_relativeSize - interval;

	// la liste contenant toutes les colonnes qui seront relus plus tard pour créer des faces
	short columnList[polyCount-1][polyCount-1];
	// le numero de la colonne active
	unsigned int columnIndex = 0;
	m_meshSlot->CurrentDisplayArray()->m_vertex.reserve((polyCount-1) * (polyCount-1));

	double starttimevertex = KX_GetActiveEngine()->GetRealTime();
	// on construit tous les vertices et on stocke leur numero dans des colonnes
	for(float x = m_realPos.x() - width; x <= m_realPos.x() + width; x += interval)
	{
		unsigned int vertexIndex = 0;
		for(float y = m_realPos.y() - width; y <= m_realPos.y() + width; y += interval)
		{
			//Remplissage de l'uv lambda:
			MT_Point2 uvs_buffer[8] = {MT_Point2(x, y)};
			//bourrage de vertices
			columnList[columnIndex][vertexIndex++] = m_meshSlot->AddVertex(NEWVERT);
		}
		++columnIndex;
	}
	double endtimevertex = KX_GetActiveEngine()->GetRealTime();
	DEBUG("adding vertex spend " << endtimevertex - starttimevertex << " time");
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
	DEBUG("adding index spend " << endtime - starttimeindex << " time");
	DEBUG("add " << m_meshSlot->CurrentDisplayArray()->m_vertex.size() << " vertex and " << m_meshSlot->CurrentDisplayArray()->m_index.size() << " index");
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
}

void KX_Chunk::ConstructJoint()
{
	double starttime = KX_GetActiveEngine()->GetRealTime();
	if (m_jointSlot)
		delete m_jointSlot;

	m_jointSlot = new RAS_MeshSlot();
	m_jointSlot->init(NULL, 3);//on utilise des triangles

	// le nombre de poly en largeur
	const unsigned short polyCount = m_relativeSize * m_subDivisions;
	const unsigned short vertexCountExterne = m_relativeSize * m_subDivisions + 1;
	const unsigned short vertexCountInterne = m_relativeSize * m_subDivisions - 1;
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
	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
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
			if (m_jointSlot)
			{
				delete m_jointSlot;
				m_jointSlot = NULL;
			}

			m_hasSubChunks = true;

			unsigned short relativesize = m_relativeSize / 2;
			unsigned short width = relativesize / 2;
			m_subChunks[0] = new KX_Chunk(m_relativePosX - width, m_relativePosY - width, relativesize, m_level*2, m_terrain);
			m_subChunks[1] = new KX_Chunk(m_relativePosX + width, m_relativePosY - width, relativesize, m_level*2, m_terrain);
			m_subChunks[2] = new KX_Chunk(m_relativePosX - width, m_relativePosY + width, relativesize, m_level*2, m_terrain);
			m_subChunks[3] = new KX_Chunk(m_relativePosX + width, m_relativePosY + width, relativesize, m_level*2, m_terrain);
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
		if (m_subDivisions != m_lastSubDivision || !m_meshSlot)
		{
			ConstructMesh();
		}

		KX_Chunk* chunkLeft = m_terrain->GetChunkRelativePosition(m_relativePosX - 1, m_relativePosY);
		KX_Chunk* chunkRight = m_terrain->GetChunkRelativePosition(m_relativePosX + 1, m_relativePosY);
		KX_Chunk* chunkFront = m_terrain->GetChunkRelativePosition(m_relativePosX, m_relativePosY - 1);
		KX_Chunk* chunkBack = m_terrain->GetChunkRelativePosition(m_relativePosX, m_relativePosY + 1);

		// ensemble de 4 variables verifiant si il y aura besoin d'une jointure
		const bool hasJointLeft = chunkLeft ? chunkLeft->GetSubDivision() < m_subDivisions : false;
		const bool hasJointRight = chunkRight ? chunkRight->GetSubDivision() < m_subDivisions : false;
		const bool hasJointFront = chunkFront ? chunkFront->GetSubDivision() < m_subDivisions : false;
		const bool hasJointBack = chunkBack ? chunkBack->GetSubDivision() < m_subDivisions : false;

		if (m_subDivisions != m_lastSubDivision ||
			!m_jointSlot ||
			m_lastHasJointLeft != hasJointLeft ||
			m_lastHasJointRight != hasJointRight ||
			m_lastHasJointFront != hasJointFront ||
			m_lastHasJointBack != hasJointBack)
		{
			m_lastHasJointLeft = hasJointLeft;
			m_lastHasJointRight = hasJointRight;
			m_lastHasJointFront = hasJointFront;
			m_lastHasJointBack = hasJointBack;
			ConstructJoint();
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
	{
		rasty->IndexPrimitives(*m_meshSlot);
		rasty->IndexPrimitives(*m_jointSlot);
	}
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