#include "KX_Terrain.h"
#include "KX_Chunk.h"

#include "KX_Camera.h"
#include "KX_PythonInit.h"

#include "RAS_IRasterizer.h"
#include "RAS_MaterialBucket.h"
#include "RAS_MeshObject.h"
#include "RAS_Polygon.h"
#include "RAS_IPolygonMaterial.h"

#include "BLI_math.h"

#include "PHY_IPhysicsController.h"

#include <stdio.h>

#define COLORED_PRINT(msg, color) std::cout << /*"\033[" << color << "m" <<*/ msg << /*"\033[30m" <<*/ std::endl;

#define DEBUG(msg) std::cout << "\033[33mDebug (" << __func__ << ", " << this << ") : " << msg << std::endl;
#define WARNING(msg) COLORED_PRINT("Warning : " << msg, 33);
#define INFO(msg) COLORED_PRINT("Info : " << msg, 37);
#define ERROR(msg) std::cout << "Error (" << __func__ << ", " << this << ")" << msg << std::endl
#define SUCCES(msg) COLORED_PRINT("Succes : " << msg, 32);

#define DEBUGNOENDL(msg) std::cout << msg;

#define NEWVERT RAS_TexVert(MT_Point3(x, y, BLI_hnoise(60., x, y, 0.) * maxheight), \
												uvs_buffer, \
												MT_Vector4(1., 1., 1., 1.), \
												0, \
												MT_Vector3(0, 0, 1.), \
												true, 0) \

#define INDEX_JOINT -1
//#define STATS

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
	m_meshObj(NULL),
	m_lastHasJointLeft(false),
	m_lastHasJointRight(false),
	m_lastHasJointFront(false),
	m_lastHasJointBack(false),
	m_createdMesh(false)
{
	int n = 123;

	char c[3];
	sprintf(c, "%d", m_chunkActive);

	SetName("KX_Chunk" + STR_String(c));
	m_meshes.clear();
	m_chunkActive++;
}

KX_Chunk::~KX_Chunk()
{
	RemoveMeshes();

	if (m_meshObj) {
		RAS_MeshMaterial* meshmat = m_meshObj->GetMeshMaterial((unsigned int)0);
		meshmat->m_bucket->RemoveMesh(meshmat->m_baseslot);
		delete m_meshObj;
	}
}

void KX_Chunk::ReconstructMesh()
{
	RemoveMeshes();

	if (m_meshObj)
	{
		RAS_MeshMaterial* meshmat = m_meshObj->GetMeshMaterial((unsigned int)0);
		meshmat->m_bucket->RemoveMesh(meshmat->m_baseslot);
		delete m_meshObj;
	}

	m_createdMesh = true;

	m_meshObj = new RAS_MeshObject(NULL);
	m_meshes.push_back(m_meshObj);

	m_originVertexIndex = 0;

	ConstructMesh();

	AddMeshUser();

	m_meshObj->SchedulePolygons(0);

// 	m_pPhysicsController->ReinstancePhysicsShape(NULL, m_meshObj);
}

// Alexis, tu mettra ta fonction magique ici.
float KX_Chunk::GetZVertex(float vertx, float verty) const
{
	KX_Terrain* terrain = m_node->GetTerrain();

	const float maxheight = terrain->GetMaxHeight();
	const MT_Point2 realPos = m_node->GetRealPos();

	const float x = vertx + realPos.x();
	const float y = verty + realPos.y();

	const float z = BLI_hnoise(5., x, y, 0.) * maxheight;
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
		meshObj->AddVertex(poly, 0, MT_Point3(v3.xyz), uvs_3, tangent, 255, normal, true, v3.origIndex);
		meshObj->AddVertex(poly, 1, MT_Point3(v2.xyz), uvs_2, tangent, 255, normal, true, v2.origIndex);
		meshObj->AddVertex(poly, 2, MT_Point3(v1.xyz), uvs_1, tangent, 255, normal, true, v1.origIndex);
	}
	else
	{
		float fnormal[3];
		normal_tri_v3(fnormal, v1.xyz, v2.xyz, v3.xyz);
		MT_Vector3 normal(fnormal);
		meshObj->AddVertex(poly, 0, MT_Point3(v1.xyz), uvs_1, tangent, 255, normal, true, v1.origIndex);
		meshObj->AddVertex(poly, 1, MT_Point3(v2.xyz), uvs_2, tangent, 255, normal, true, v2.origIndex);
		meshObj->AddVertex(poly, 2, MT_Point3(v3.xyz), uvs_3, tangent, 255, normal, true, v3.origIndex);
	}
}

void KX_Chunk::ConstructMesh()
{
	/*       1(left)     2(right)
	 *       |           |
	 *       |           |
	 * / \ s−−−u−−−u−−−u−−−s
	 *  |  |\|_|__/|__/|_/_|___3(back)
	 *  |  | \ | / | / |/| |
	 *  |  u−−−s−−−s−−−s−−−u
	 *  |  |\| |  /|  /|\| |
	 *  |  | \ | / | / | \ |
	 *  |  u−−−s−−−u−−−s−−−u
	 *  |  |\| |  /|  /|\| |
	 *  |  | \ | / | / | \ |
	 *  |  u−−−s−−−s−−−s−−−u
	 *  |  |_/_| /_| /_|\|_|___4(front)
	 *  |  |/| |/| |/  | \ |
	 *  |  s−−−u−−−u−−−u−−−s
	 *  |
	 *  |__________________\
	 *                     /
	 * 
	 * s = shared, vertice commun entre les colonnes et le centre
	 * u = unique, vertice independant
	 */

#ifdef STATS
	double starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	KX_Terrain* terrain = m_node->GetTerrain();
	//La taille "réelle" du chunk
	const float size = terrain->GetChunkSize();

	const unsigned short relativesize = m_node->GetRelativeSize();

	// le nombre de poly en largeur à l'interieur
	const unsigned short vertexCountInterne = polyCount - 1;
	// le nombre de poly en largeur à l'exterieur
	const unsigned short vertexCountExterne = polyCount + 1;

	// la liste contenant toutes les colonnes qui seront relus plus tard pour créer des faces
	Vertex columnList[vertexCountInterne][vertexCountInterne];
	// colonne gauche
	JointColumn jointColumnLeft(vertexCountExterne);
	// colonne droite
	JointColumn jointColumnRight(vertexCountExterne);
	// colonne haut
	JointColumn jointColumnFront(vertexCountExterne);
	// colonne bas
	JointColumn jointColumnBack(vertexCountExterne);

	//Calcul de l'intervalle entre deux points
	const float interval = size * relativesize / polyCount;
	// la motie de la largeur du chunk
	const float width = size / 2 * relativesize;
	// la motie de la largeur du chunk - interval car on ne fait pas les bords
	const float widthcenter = width - interval;

	const float startx = -width;
	const float endx = width;
	const float starty = -width;
	const float endy = width;

#ifdef STATS
	double starttimevertex = KX_GetActiveEngine()->GetRealTime();
#endif

	// on construit tous les vertices et on stocke leur numero dans des colonnes
	for(unsigned short columnIndex = 0; columnIndex < vertexCountInterne; ++columnIndex) {
		const float x = interval * columnIndex - widthcenter;
		for(unsigned short vertexIndex = 0; vertexIndex < vertexCountInterne; ++vertexIndex) {
			const float y = interval * vertexIndex - widthcenter;
			// on créer un vertice temporaire, ces donné seront reutilisé lors de la création des polygones
			const Vertex& vertex = Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			columnList[columnIndex][vertexIndex] = vertex;

			if (columnIndex == 0)
				jointColumnLeft.columnInterne[vertexIndex] = vertex;
			if (columnIndex == (vertexCountInterne-1))
				jointColumnRight.columnInterne[vertexIndex] = vertex;
			if (vertexIndex == 0)
				jointColumnFront.columnInterne[columnIndex] = vertex;
			if (vertexIndex == (vertexCountInterne-1))
				jointColumnBack.columnInterne[columnIndex] = vertex;
		}
	}

#ifdef STATS
	double endtimevertex = KX_GetActiveEngine()->GetRealTime();
	DEBUG("adding vertex spend " << endtimevertex - starttimevertex << " time");
	double starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	// construction des vertices externes de gauche et de droite
	for (unsigned short vertexIndex = 0; vertexIndex < vertexCountExterne; ++vertexIndex) {
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

	for (unsigned short vertexIndex = 1; vertexIndex < (vertexCountExterne - 1); ++vertexIndex) {
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

	// on reserve les vertices pour optimizer le temps
	m_meshObj->m_sharedvertex_map.resize(m_originVertexIndex);

	// on lit toutes les colonnes pour créer des faces
	for (unsigned int columnIndex = 0; columnIndex < polyCount-2; ++columnIndex) {
		for (unsigned int vertexIndex = 0; vertexIndex < polyCount-2; ++vertexIndex) {
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
	KX_Terrain* terrain = m_node->GetTerrain();

	const KX_ChunkNode::Point2D& pos = m_node->GetRelativePos();
	unsigned short relativewidth = m_node->GetRelativeSize() / 2 + 1;

	KX_ChunkNode* nodeLeft = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x - relativewidth, pos.y));
	KX_ChunkNode* nodeRight = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x + relativewidth, pos.y));
	KX_ChunkNode* nodeFront = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x, pos.y - relativewidth));
	KX_ChunkNode* nodeBack = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x, pos.y + relativewidth));

	// ensemble de 4 variables verifiant si il y aura besoin d'une jointure
	const bool hasJointLeft = nodeLeft ? nodeLeft->GetLevel() < m_node->GetLevel() : false;
	const bool hasJointRight = nodeRight ? nodeRight->GetLevel() < m_node->GetLevel() : false;
	const bool hasJointFront = nodeFront ? nodeFront->GetLevel() < m_node->GetLevel() : false;
	const bool hasJointBack = nodeBack ? nodeBack->GetLevel() < m_node->GetLevel() : false;

	if (m_meshes.size() == 0) {
		m_lastHasJointLeft = hasJointLeft;
		m_lastHasJointRight = hasJointRight;
		m_lastHasJointFront = hasJointFront;
		m_lastHasJointBack = hasJointBack;

		ReconstructMesh();
	}
	else if (m_lastHasJointLeft != hasJointLeft ||
		m_lastHasJointRight != hasJointRight ||
		m_lastHasJointFront != hasJointFront ||
		m_lastHasJointBack != hasJointBack)
	{
		m_lastHasJointLeft = hasJointLeft;
		m_lastHasJointRight = hasJointRight;
		m_lastHasJointFront = hasJointFront;
		m_lastHasJointBack = hasJointBack;

		ReconstructMesh();
	}
}

void KX_Chunk::RenderMesh(RAS_IRasterizer* rasty, KX_Camera* cam)
{
	const MT_Point3 realPos = MT_Point3(m_node->GetRealPos().x(), m_node->GetRealPos().y(), 0.);
	const MT_Point3 camPos = cam->NodeGetWorldPosition();
	const MT_Vector3 norm = (camPos - realPos).normalized() * sqrt(m_node->GetRadius2());

	/*MT_Point3* box = m_node->GetBox();

	for (int i = 0; i < 4; i++)
	{
		static MT_Vector3 color(1., 1., 1.);
		KX_RasterizerDrawDebugLine(box[i], box[i + 4], color);
		KX_RasterizerDrawDebugLine(box[i], box[(i < 3) ? i + 1 : 0], color);
		KX_RasterizerDrawDebugLine(box[i + 4], box[(i < 3) ? i + 5 : 4], color);
	}*/
	KX_RasterizerDrawDebugLine(realPos, realPos + norm, MT_Vector3(1., 0., 0.));
}