#include "KX_Chunk.h"
#include "KX_ChunkNode.h"
#include "KX_Terrain.h"

#include "KX_Camera.h"
#include "KX_PythonInit.h"

#include "RAS_IRasterizer.h"
#include "RAS_MaterialBucket.h"
#include "RAS_MeshObject.h"
#include "RAS_Polygon.h"

#include "BLI_math.h"

#include "PHY_IPhysicsController.h"
#include "CcdPhysicsController.h"

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
#define STATS

unsigned int KX_Chunk::m_chunkActive = 0;
static const unsigned short polyCount = 4;

struct KX_Chunk::Vertex // vertex temporaire
{
	float xyz[3];
	unsigned int origIndex;
	bool valid;

	Vertex(float x, float y, float z, unsigned int origindex)
		:origIndex(origindex),
		valid(true)
	{
		xyz[0] = x;
		xyz[1] = y;
		xyz[2] = z;
	}

	Vertex()
		:valid(false)
	{
	}
};

std::ostream& operator<<(std::ostream& os, const KX_Chunk::Vertex& vert)
{
	return os << "Vertex(valid = " << vert.valid << ", pos = (x : " << vert.xyz[0] << ", y : " << vert.xyz[1] << ", z : " << vert.xyz[2] << "), origindex = " << vert.origIndex << ")";
}

struct KX_Chunk::JointColumn
{
	Vertex* columnExterne;
	Vertex* columnInterne;
	unsigned short vertexCount;

	JointColumn(const unsigned short vertexcount)
		:vertexCount(vertexcount)
	{
		columnExterne = (Vertex*)malloc(vertexcount * sizeof(Vertex));
		columnInterne = (Vertex*)malloc((vertexcount - 2) * sizeof(Vertex));
	}

	~JointColumn()
	{
		free(columnExterne);
		free(columnInterne);
	}
};

KX_Chunk::KX_Chunk(void* sgReplicationInfo, SG_Callbacks callbacks, KX_ChunkNode* node, RAS_MaterialBucket* bucket)
	:KX_GameObject(sgReplicationInfo, callbacks),
	m_node(node),
	m_bucket(bucket),
	m_lastHasJointLeft(false),
	m_lastHasJointRight(false),
	m_lastHasJointFront(false),
	m_lastHasJointBack(false)
{
	SetName("KX_Chunk");
}

KX_Chunk::~KX_Chunk()
{
}

void KX_Chunk::ReconstructMesh()
{
	if (m_meshes.size() > 0)
	{
		delete m_meshes[0];
		m_meshes.clear();
		
	}

	RAS_MeshObject* meshObject = new RAS_MeshObject(NULL);
	m_meshes.push_back(meshObject);
	m_originVertexIndex = 0;

	ConstructCenterMesh();
	ConstructJointMesh();

	RAS_DisplayArray* array = meshObject->GetMeshMaterial((unsigned int)0)->m_baseslot->CurrentDisplayArray();

	AddMeshUser();

	meshObject->SchedulePolygons(0);

	m_pPhysicsController->ReinstancePhysicsShape(NULL, meshObject);
}

// Alexis, tu mettra ta fonction magique ici.
float KX_Chunk::GetZVertex(float vertx, float verty) const
{
	KX_Terrain* terrain = m_node->GetTerrain();

	const float maxheight = terrain->GetMaxHeight();
	const MT_Point2 realPos = m_node->GetRealPos();

	const float x = vertx + realPos.x();
	const float y = verty + realPos.y();

	const float z = BLI_hnoise(10., x, y, 0.) * maxheight;
	return z;
}

void KX_Chunk::AddMeshPolygonVertexes(const Vertex& v1, const Vertex& v2, const Vertex& v3, bool reverse)
{
	RAS_MeshObject* meshObj = m_meshes[0];
	RAS_Polygon* poly = meshObj->AddPolygon(m_bucket, 3);

	poly->SetVisible(true);
	poly->SetCollider(true);
	poly->SetTwoside(true);

	MT_Point2 uvs_1[8];
	MT_Point2 uvs_2[8];
	MT_Point2 uvs_3[8];

	for (unsigned short i = 0; i < 8; ++i)
	{
		uvs_1[i] = MT_Point2(v1.xyz);
		uvs_2[i] = MT_Point2(v2.xyz);
		uvs_3[i] = MT_Point2(v3.xyz);
	}

	MT_Vector4 tangent(0., 0., 0., 0.);

	if (reverse)
	{
		float fnormal[3];
		normal_tri_v3(fnormal, v3.xyz, v2.xyz, v1.xyz);
		MT_Vector3 normal(fnormal);
		meshObj->AddVertex(poly, 0, MT_Point3(v3.xyz), uvs_3, tangent, 255, normal, false, v3.origIndex);
		meshObj->AddVertex(poly, 1, MT_Point3(v2.xyz), uvs_2, tangent, 255, normal, false, v2.origIndex);
		meshObj->AddVertex(poly, 2, MT_Point3(v1.xyz), uvs_1, tangent, 255, normal, false, v1.origIndex);
	}
	else
	{
		float fnormal[3];
		normal_tri_v3(fnormal, v1.xyz, v2.xyz, v3.xyz);
		MT_Vector3 normal(fnormal);
		meshObj->AddVertex(poly, 0, MT_Point3(v1.xyz), uvs_1, tangent, 255, normal, false, v1.origIndex);
		meshObj->AddVertex(poly, 1, MT_Point3(v2.xyz), uvs_2, tangent, 255, normal, false, v2.origIndex);
		meshObj->AddVertex(poly, 2, MT_Point3(v3.xyz), uvs_3, tangent, 255, normal, false, v3.origIndex);
	}
}

void KX_Chunk::ConstructCenterMesh()
{
#ifdef STATS
	double starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	KX_Terrain* terrain = m_node->GetTerrain();
	//La taille "réelle" du chunk
	const float size = terrain->GetChunkSize();

	const unsigned short relativesize = m_node->GetRelativeSize();

	// le nombre de poly en largeur
	const unsigned short vertexCountInterne = polyCount - 1;

	// la liste contenant toutes les colonnes qui seront relus plus tard pour créer des faces
	Vertex columnList[vertexCountInterne][vertexCountInterne];

	//Calcul de l'intervalle entre deux points
	const float interval = size * relativesize / polyCount;
	// la motie de la largeur du chunk - interval car on ne fait pas les bords
	const float width = size / 2 * relativesize - interval;

#ifdef STATS
	double starttimevertex = KX_GetActiveEngine()->GetRealTime();
#endif

	// on construit tous les vertices et on stocke leur numero dans des colonnes
	for(unsigned short columnIndex = 0; columnIndex < vertexCountInterne; ++columnIndex)
	{
		const float x = interval * columnIndex - width;
		for(unsigned short vertexIndex = 0; vertexIndex < vertexCountInterne; ++vertexIndex)
		{
			const float y = interval * vertexIndex - width;
			// on créer un vertice temporaire, ces donné seront reutilisé lors de la création des polygones
			columnList[columnIndex][vertexIndex] = Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
		}
	}

#ifdef STATS
	double endtimevertex = KX_GetActiveEngine()->GetRealTime();
	DEBUG("adding vertex spend " << endtimevertex - starttimevertex << " time");
	double starttimeindex = KX_GetActiveEngine()->GetRealTime();
#endif

	RAS_MeshObject* meshObj = m_meshes[0];
	// on reserve les vertices pour optimizer le temps
	meshObj->m_sharedvertex_map.resize(m_originVertexIndex);

	// on lit toutes les colonnes pour créer des faces
	for (unsigned int columnIndex = 0; columnIndex < polyCount-2; ++columnIndex)
	{
		for (unsigned int vertexIndex = 0; vertexIndex < polyCount-2; ++vertexIndex)
		{
			const Vertex& firstVertex = columnList[columnIndex][vertexIndex];
			const Vertex& secondVertex = columnList[columnIndex+1][vertexIndex];
			const Vertex& thirdVertex = columnList[columnIndex+1][vertexIndex+1];
			const Vertex& fourthVertex = columnList[columnIndex][vertexIndex+1];

			// création du premier triangle
			AddMeshPolygonVertexes(firstVertex, secondVertex, thirdVertex, false);
			// création du deuxieme triangle
			AddMeshPolygonVertexes(firstVertex, thirdVertex, fourthVertex, false);
		}
	}

#ifdef STATS
	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG("adding index spend " << endtime - starttimeindex << " time");
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
#endif
}

void KX_Chunk::ConstructJointMesh()
{
#ifdef STATS
	double starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	KX_Terrain* terrain = m_node->GetTerrain();
	//La taille "réelle" du chunk
	const float size = terrain->GetChunkSize();

	const unsigned short relativesize = m_node->GetRelativeSize();

	// le nombre de poly en largeur
	const unsigned short vertexCountExterne = polyCount + 1;
	const unsigned short vertexCountInterne = polyCount - 1;

	// la motie de la largeur du chunk
	const float width = size / 2 * relativesize;
	//Calcul de l'intervalle entre deux points
	const float interval = size * relativesize / polyCount;
	
	const float startx = -width;
	const float endx = width;
	const float starty = -width;
	const float endy = width;

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

			const Vertex& vertex = Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			jointColumnLeft.columnInterne[vertexIndex] = vertex;

			if (vertexIndex == 0)
				jointColumnFront.columnInterne[0] = vertex;
			else if (vertexIndex == (vertexCountInterne-1))
				jointColumnBack.columnInterne[0] = vertex;
		}
		{ // colonne de droite interne
			const float x = endx - interval;

			const Vertex& vertex = Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			jointColumnRight.columnInterne[vertexIndex] = vertex;

			if (vertexIndex == 0)
				jointColumnFront.columnInterne[vertexCountInterne-1] = vertex;
			else if (vertexIndex == (vertexCountInterne-1))
				jointColumnBack.columnInterne[vertexCountInterne-1] = vertex;
		}
	}
	// construction des vertices externes de gauche et de droite
	for (unsigned short vertexIndex = 0; vertexIndex < vertexCountExterne; ++vertexIndex)
	{
		const float y = starty + interval * vertexIndex;
		{ // colonne de gauche extern
			const float x = startx;

			const Vertex& vertex = (vertexIndex % 2 && m_lastHasJointLeft) ? Vertex() : Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			jointColumnLeft.columnExterne[vertexIndex] = vertex;

			if (vertexIndex == 0)
				jointColumnFront.columnExterne[0] = vertex;
			else if (vertexIndex == (vertexCountExterne-1))
				jointColumnBack.columnExterne[0] = vertex;
		}
		{ // colonne de droite extern
			const float x = endx;

			const Vertex& vertex = (vertexIndex % 2 && m_lastHasJointRight) ? Vertex() : Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			jointColumnRight.columnExterne[vertexIndex] = vertex;

			if (vertexIndex == 0)
				jointColumnFront.columnExterne[vertexCountExterne-1] = vertex;
			else if (vertexIndex == (vertexCountExterne-1))
				jointColumnBack.columnExterne[vertexCountExterne-1] = vertex;
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

			const Vertex& vertex = Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			jointColumnFront.columnInterne[vertexIndex] = vertex;
		}
		{ // colonne de bas interne
			const float y = endy - interval;

			const Vertex& vertex = Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			jointColumnBack.columnInterne[vertexIndex] = vertex;
		}
	}

	for (unsigned short vertexIndex = 1; vertexIndex < (vertexCountExterne - 1); ++vertexIndex)
	{
		const float x = startx + interval * vertexIndex;
		{ // colonne de haut extern
			const float y = starty;

			const Vertex& vertex = (vertexIndex % 2 && m_lastHasJointFront) ? Vertex() : Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			jointColumnFront.columnExterne[vertexIndex] = vertex;
		}
		{ // colonne de bas extern
			const float y = endy;

			const Vertex& vertex = (vertexIndex % 2 && m_lastHasJointBack) ? Vertex() : Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			jointColumnBack.columnExterne[vertexIndex] = vertex;
		}
	}

	RAS_MeshObject* meshObj = m_meshes[0];
	// on reserve les vertices pour optimizer le temps
	meshObj->m_sharedvertex_map.resize(m_originVertexIndex);

	ConstructJointMeshColumnPoly(jointColumnLeft, polyCount, false);
	ConstructJointMeshColumnPoly(jointColumnRight, polyCount, true);
	ConstructJointMeshColumnPoly(jointColumnFront, polyCount, true);
	ConstructJointMeshColumnPoly(jointColumnBack, polyCount, false);

#ifdef STATS
	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
#endif
}

void KX_Chunk::ConstructJointMeshColumnPoly(const JointColumn& column, unsigned short polyCount, bool reverse)
{
	for (unsigned short vertexIndex = 0; vertexIndex < polyCount; ++vertexIndex)
	{
		bool firstTriangle = true;
		bool secondTriangle = true;
		Vertex firstVertex, secondVertex, thirdVertex, fourthVertex;
		firstVertex = column.columnExterne[vertexIndex]; // les seuls points fixes
		fourthVertex = column.columnExterne[vertexIndex+1];
		if (!fourthVertex.valid)
		{
			fourthVertex = column.columnExterne[vertexIndex+2];
		}

		// debut de la colonne
		if (vertexIndex == 0)
		{
			/*
			 * 1 
			 * |\
			 * | \
			 * 3--2
			 */

			secondVertex = column.columnInterne[0];
			thirdVertex = fourthVertex;
			secondTriangle = false;
		}
		// fin de la colonne
		else if (vertexIndex == (polyCount-1))
		{
			/*
			 * 1--2
			 * | /
			 * |/
			 * 3
			 */

			if (!firstVertex.valid)
			{
				firstTriangle = false;
			}
			secondVertex = column.columnInterne[vertexIndex-1];
			thirdVertex = column.columnExterne[polyCount];
			secondTriangle = false;
		}
		// miliue de la colonne
		else
		{
			/*
			 * 1--2
			 * |\ |
			 * | \|
			 * 4--3
			 */
			secondVertex = column.columnInterne[vertexIndex-1];
			thirdVertex = column.columnInterne[vertexIndex];
			if (!firstVertex.valid)
			{
				firstVertex = fourthVertex;
				secondTriangle = false;
			}
		}
		if (firstTriangle)
		{
			// création du premier triangle
			AddMeshPolygonVertexes(firstVertex, secondVertex, thirdVertex, reverse);
		}
		if (secondTriangle)
		{
			// création du deuxieme triangle
			AddMeshPolygonVertexes(firstVertex, thirdVertex, fourthVertex, reverse);
		}
	}
}

void KX_Chunk::UpdateMesh()
{
	/*KX_Terrain* terrain = m_node->GetTerrain();

	int relativePosX = m_node->GetRelativePosX();
	int relativePosY = m_node->GetRelativePosY();

	KX_ChunkNode* nodeLeft = terrain->GetNodeRelativePosition(relativePosX - 1, relativePosY);
	KX_ChunkNode* nodeRight = terrain->GetNodeRelativePosition(relativePosX + 1, relativePosY);
	KX_ChunkNode* nodeFront = terrain->GetNodeRelativePosition(relativePosX, relativePosY - 1);
	KX_ChunkNode* nodeBack = terrain->GetNodeRelativePosition(relativePosX, relativePosY + 1);

	// ensemble de 4 variables verifiant si il y aura besoin d'une jointure
	const bool hasJointLeft = nodeLeft ? nodeLeft->GetLevel() < m_node->GetLevel() : false;
	const bool hasJointRight = nodeRight ? nodeRight->GetLevel() < m_node->GetLevel() : false;
	const bool hasJointFront = nodeFront ? nodeFront->GetLevel() < m_node->GetLevel() : false;
	const bool hasJointBack = nodeBack ? nodeBack->GetLevel() < m_node->GetLevel() : false;

	if (m_lastHasJointLeft != hasJointLeft ||
		m_lastHasJointRight != hasJointRight ||
		m_lastHasJointFront != hasJointFront ||
		m_lastHasJointBack != hasJointBack)
	{
		m_lastHasJointLeft = hasJointLeft;
		m_lastHasJointRight = hasJointRight;
		m_lastHasJointFront = hasJointFront;
		m_lastHasJointBack = hasJointBack;

		RAS_MeshObject* meshToDelete = m_meshes[0];
		RemoveMeshes();
		delete meshToDelete;

		RAS_MeshObject* meshObj = new RAS_MeshObject(NULL);
		AddMesh(meshObj);

		ConstructMesh();
		ConstructJoint();

		AddMeshUser();
	}*/
}

void KX_Chunk::RenderMesh(RAS_IRasterizer* rasty)
{
	const MT_Point2 realPos = m_node->GetRealPos();

	/*MT_Point3* box = m_node->GetBox();

	for (int i = 0; i < 4; i++)
	{
		static MT_Vector3 color(1., 1., 1.);
		KX_RasterizerDrawDebugLine(box[i], box[i + 4], color);
		KX_RasterizerDrawDebugLine(box[i], box[(i < 3) ? i + 1 : 0], color);
		KX_RasterizerDrawDebugLine(box[i + 4], box[(i < 3) ? i + 5 : 4], color);
	}*/
	KX_RasterizerDrawDebugLine(MT_Point3(realPos.x(), realPos.y(), 0.),
							MT_Point3(realPos.x(), realPos.y(), 10.),
							MT_Vector3(1., 0., 0.));
// 	DEBUG("physic controller wants sleep : " << m_pPhysicsController->WantsSleeping());
}