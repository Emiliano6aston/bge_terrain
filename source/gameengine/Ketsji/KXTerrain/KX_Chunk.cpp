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
#include "MT_assert.h"

#include "PHY_IPhysicsController.h"

#include <stdio.h>

#define COLORED_PRINT(msg, color) std::cout << /*"\033[" << color << "m" <<*/ msg << /*"\033[30m" <<*/ std::endl;

#define DEBUG(msg) std::cout << "Debug (" << __func__ << ", " << this << ") : " << msg << std::endl;
#define WARNING(msg) COLORED_PRINT("Warning : " << msg, 33);
#define INFO(msg) COLORED_PRINT("Info : " << msg, 37);
#define ERROR(msg) std::cout << "Error (" << __func__ << ", " << this << ") : " << msg << std::endl;
#define SUCCES(msg) COLORED_PRINT("Succes : " << msg, 32);

#define STATS

unsigned int KX_Chunk::m_chunkActive = 0;
unsigned int KX_Chunk::meshRecreation = 0;
double KX_Chunk::chunkCreationTime = 0.0;
double KX_Chunk::normalComputingTime = 0.0;
double KX_Chunk::vertexAddingTime = 0.0;
double KX_Chunk::vertexCreatingTime = 0.0;
double KX_Chunk::polyAddingTime = 0.0;
double KX_Chunk::physicsCreatingTime = 0.0;

void KX_Chunk::ResetTime()
{
	meshRecreation = 0;
	chunkCreationTime = 0.0;
	normalComputingTime = 0.0;
	vertexAddingTime = 0.0;
	vertexCreatingTime = 0.0;
	polyAddingTime = 0.0;
	physicsCreatingTime = 0.0;
}

void KX_Chunk::PrintTime()
{
	std::cout << "Time Stats : " << std::endl
		<< "\t Mesh Recreation Per Frame : \t" << meshRecreation << std::endl
		<< "\t Chunk Creation Time : \t\t\t" << chunkCreationTime << std::endl
		<< "\t Normal Computing Time : \t\t" << normalComputingTime << std::endl
		<< "\t Vertex Adding Time : \t\t\t" << vertexAddingTime << std::endl
		<< "\t Vertex Creating Time : \t\t" << vertexCreatingTime << std::endl
		<< "\t Poly Adding Time : \t\t\t" << polyAddingTime << std::endl
		<< "\t Physics Creating Time : \t\t" << physicsCreatingTime << std::endl
		<< std::endl;
}

struct KX_Chunk::Vertex // vertex temporaire
{
	MT_Point3 xyz;
	short relpos[2];
	unsigned int rgba;
	unsigned int origIndex;
	bool valid;
	MT_Vector3 normal;

	Vertex(short relx, short rely,
		   const MT_Point3 &pos,
		   unsigned int color,
		   unsigned int origindex)
		:rgba(color),
		origIndex(origindex),
		valid(true)
	{
		relpos[0] = relx;
		relpos[1] = rely;
		xyz = pos;
		normal = MT_Vector3(0., 0., 1.);
	}

	void Invalidate() { valid = false; }
	void Validate() { valid = true; }
};

struct KX_Chunk::JointColumn
{
	Vertex *m_columnExterne[VERTEX_COUNT];
	Vertex *m_columnInterne[VERTEX_COUNT_INTERN];
	// si vrai alors on supprime les vertices aux extrémité
	bool m_freeEndVertex;

	JointColumn(bool freeEndVertex)
		:m_freeEndVertex(freeEndVertex)
	{
		for (unsigned short i = 0; i < VERTEX_COUNT; ++i)
			m_columnExterne[i] = NULL;
		for (unsigned short i = 0; i < VERTEX_COUNT_INTERN; ++i)
			m_columnInterne[i] = NULL;
	}

	~JointColumn()
	{
		for (unsigned short i = (m_freeEndVertex ? 0 : 1);
			 i < (POLY_COUNT + (m_freeEndVertex ? 1 : 0)); ++i) 
		{
			Vertex *vertex = m_columnExterne[i];
			delete vertex;
		}
	}

	void SetExternVertex(unsigned short index, Vertex *vertex) { m_columnExterne[index] = vertex; }
	void SetInternVertex(unsigned short index, Vertex *vertex) { m_columnInterne[index] = vertex; }
	Vertex *GetInternVertex(unsigned short index) { return m_columnInterne[index]; }
	Vertex *GetExternVertex(unsigned short index) { return m_columnExterne[index]; }
	Vertex *GetExternVertexNext(unsigned short index)
	{
		for (++index; index < VERTEX_COUNT; ++index) {
			Vertex *vertex = m_columnExterne[index];
			if (vertex->valid)
				return vertex;
		}
		return NULL;
	}
};

KX_Chunk::KX_Chunk(void *sgReplicationInfo, SG_Callbacks callbacks, KX_ChunkNode *node, RAS_MaterialBucket *bucket)
	:KX_GameObject(sgReplicationInfo, callbacks),
	m_node(node),
	m_bucket(bucket),
	m_meshObj(NULL),
	m_hasVertexes(false)
{
	m_lastHasJoint[COLUMN_LEFT] = false;
	m_lastHasJoint[COLUMN_RIGHT] = false;
	m_lastHasJoint[COLUMN_FRONT] = false;
	m_lastHasJoint[COLUMN_BACK] = false;

	char c[3];
	sprintf(c, "%d", m_chunkActive);
	SetName("KX_Chunk" + STR_String(c));

	m_chunkActive++;
}

KX_Chunk::~KX_Chunk()
{
	DestructMesh();

	if (m_hasVertexes) {
		for (unsigned short i = 0; i < (POLY_COUNT - 1); ++i) {
			for (unsigned short j = 0; j < (POLY_COUNT - 1); ++j) {
				delete m_center[i][j];
			}
		}

		delete m_columns[0];
		delete m_columns[1];
		delete m_columns[2];
		delete m_columns[3];
	}
}

void KX_Chunk::ConstructMesh()
{
	m_meshObj = new RAS_MeshObject(NULL);
	m_meshes.push_back(m_meshObj);
}

void KX_Chunk::DestructMesh()
{
	RemoveMeshes();

	if (m_meshObj) {
		RAS_MeshMaterial *meshmat = m_meshObj->GetMeshMaterial((unsigned int)0);
		meshmat->m_bucket->RemoveMesh(meshmat->m_baseslot);
		delete m_meshObj;
	}
}

void KX_Chunk::ReconstructMesh()
{
#ifdef STATS
	double starttime;
	double endtime;

	meshRecreation++;
#endif

	DestructMesh();
	ConstructMesh();

	if (!m_hasVertexes) {
		ConstructVertexes();
		m_hasVertexes = true;
	}

	InvalidateJointVertexes();
	ConstructPolygones();

	AddMeshUser();

	m_meshObj->SchedulePolygons(0);

#ifdef STATS
	starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	if (m_pPhysicsController)
		m_pPhysicsController->ReinstancePhysicsShape(NULL, m_meshObj, false);

#ifdef STATS
	endtime = KX_GetActiveEngine()->GetRealTime();
	physicsCreatingTime += endtime - starttime;
#endif
}

const MT_Point2 KX_Chunk::GetVertexPosition(short relx, short rely) const
{
	KX_Terrain *terrain = m_node->GetTerrain();
	//La taille "réelle" du chunk
	const float size = terrain->GetChunkSize();
	const unsigned short relativesize = m_node->GetRelativeSize();
	//Calcul de l'intervalle entre deux points
	const float interval = size * relativesize / POLY_COUNT;
	// la motie de la largeur du chunk
	const float width = size / 2 * relativesize;

	const float vertx = relx * interval - width;
	const float verty = rely * interval - width;

	const MT_Point2 vertpos = MT_Point2(vertx, verty);
	return vertpos;
}

KX_Chunk::Vertex *KX_Chunk::NewVertex(short relx, short rely)
{
	KX_Terrain *terrain = m_node->GetTerrain();
	// la position du noeud parent du chunk
	const MT_Point2& nodepos = m_node->GetRealPos();

	MT_Point2 vertpos = GetVertexPosition(relx, rely);

	// toutes les informations sur le vertice dut au zone : la hauteur, sa couleur
	VertexZoneInfo *info = terrain->GetVertexInfo(nodepos.x() + vertpos.x(), nodepos.y() + vertpos.y());
	const float vertz = info->height;

	if (vertz > m_maxVertexHeight)
		m_maxVertexHeight = vertz;

	if (vertz < m_minVertexHeight)
		m_minVertexHeight = vertz;

	unsigned int color = rgb_to_cpack(info->color[0], info->color[1], info->color[2]);

	Vertex *vertex = new Vertex(relx, rely, MT_Point3(vertpos.x(), vertpos.y(), vertz), color, m_originVertexIndex++);
	delete info;
	return vertex;
}

/* Cette fonction permet d'acceder à un vertice comme si ils étaient
 * stockés dans un tableau à deux dimension pour les x et y.
 * C'est très utile pour le calcule des normales.
 */
KX_Chunk::Vertex *KX_Chunk::GetVertex(short x, short y) const
{
	Vertex *vertex = NULL;
	if (x != -1 && x != VERTEX_COUNT && y != -1 && y != VERTEX_COUNT) {
		if ((0 < x && x < (VERTEX_COUNT - 1)) &&
			(0 < y && y < (VERTEX_COUNT - 1)))
		{
			vertex = m_center[x - 1][y - 1];
		}
		else if (x == 0) {
			vertex = m_columns[COLUMN_LEFT]->GetExternVertex(y);
		}
		else if (x == (VERTEX_COUNT - 1)) {
			vertex = m_columns[COLUMN_RIGHT]->GetExternVertex(y);
		}
		else if (y == 0) {
			vertex = m_columns[COLUMN_FRONT]->GetExternVertex(x);
		}
		else if (y == (VERTEX_COUNT - 1)) {
			vertex = m_columns[COLUMN_BACK]->GetExternVertex(x);
		}
	}

	return vertex;
}

/* On calcule la normale approximative d'un vertice. Pour cela on
 * fait la normale d'un quadrilatère de quatre point autour du vertice
 *
 *      1
 *      |
 *      |
 * 2−−−−C−−−−0
 *      |
 *      |
 *      3
 * C : centre du quadrilatère soit le vertice envoyé
 *
 * Ces quatre points peuvent être des vertices déjà existant, mais parfois non.
 * Dans ce cas la on calcule la position d'un vertice virtuelle, c'est pour cela
 * que la fonction GetVertexPosition est séparé de NewVertex.
 */
const MT_Vector3 KX_Chunk::GetNormal(Vertex *vertexCenter, bool intern) const
{
	// la position du vertice donc du centre du quad
	const float x = vertexCenter->xyz.x();
	const float y = vertexCenter->xyz.y();
	const unsigned short relx = vertexCenter->relpos[0];
	const unsigned short rely = vertexCenter->relpos[1];

	KX_Terrain *terrain = m_node->GetTerrain();
	// la position du noeud parent du chunk
	const MT_Point2& pos = m_node->GetRealPos();

	float quad[4][3];

	if (intern) {
		Vertex *vertexes[4] = {
			GetVertex(relx + 1, rely),
			GetVertex(relx, rely + 1),
			GetVertex(relx - 1, rely),
			GetVertex(relx, rely - 1)
		};
		for (unsigned short i = 0; i < 4; ++i)
			vertexes[i]->xyz.getValue(quad[i]);
	}
	else {
		// La camera active.
		KX_Camera *cam = KX_GetActiveScene()->GetActiveCamera();
		// la taille du quad servant a calculer la normale
		float smothsize = 5.0;
		// on calcule 4 positions autours du vertice
		quad[0][0] = x + smothsize; 
		quad[0][1] = y;
		quad[1][0] = x; 
		quad[1][1] = y + smothsize;
		quad[2][0] = x - smothsize;
		quad[2][1] = y;
		quad[3][0] = x;
		quad[3][1] = y - smothsize;

		for (unsigned short i = 0; i < 4; ++i) {
			VertexZoneInfo *info = terrain->GetVertexInfo(quad[i][0] + pos.x(), quad[i][1] + pos.y());
			quad[i][2] = info->height;
			delete info;
		}
	}

	float normal[3] = {0.0, 0.0, 0.0};
	normal_quad_v3(normal, quad[0], quad[1], quad[2], quad[3]);

	const MT_Vector3 finalnormal = MT_Vector3(normal);
	return finalnormal;
}

void KX_Chunk::AddMeshPolygonVertexes(Vertex *v1, Vertex *v2, Vertex *v3, bool reverse)
{
#ifdef STATS
	double starttime;
	double endtime;
#endif

#ifdef STATS
	starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	RAS_Polygon* poly = m_meshObj->AddPolygon(m_bucket, 3);
	const MT_Point2& realPos = m_node->GetRealPos();

	poly->SetVisible(true);
	poly->SetCollider(true);
	poly->SetTwoside(true);

#ifdef STATS
	endtime = KX_GetActiveEngine()->GetRealTime();
	polyAddingTime += endtime - starttime;
	starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	MT_Point2 uvs_1[8];
	MT_Point2 uvs_2[8];
	MT_Point2 uvs_3[8];

	for (unsigned short i = 0; i < 8; ++i) {
		uvs_1[i] = MT_Point2(v1->xyz[0], v1->xyz[1]) + realPos;
		uvs_2[i] = MT_Point2(v2->xyz[0], v2->xyz[1]) + realPos;
		uvs_3[i] = MT_Point2(v3->xyz[0], v3->xyz[1]) + realPos;
	}

	const MT_Vector4 tangent(0., 0., 0., 0.);

	if (reverse) {
		m_meshObj->AddVertex(poly, 0, v3->xyz, uvs_3, tangent, v3->rgba, v3->normal, false, v3->origIndex);
		m_meshObj->AddVertex(poly, 1, v2->xyz, uvs_2, tangent, v2->rgba, v2->normal, false, v2->origIndex);
		m_meshObj->AddVertex(poly, 2, v1->xyz, uvs_1, tangent, v1->rgba, v1->normal, false, v1->origIndex);
	}
	else {
		m_meshObj->AddVertex(poly, 0, v1->xyz, uvs_1, tangent, v1->rgba, v1->normal, false, v1->origIndex);
		m_meshObj->AddVertex(poly, 1, v2->xyz, uvs_2, tangent, v2->rgba, v2->normal, false, v2->origIndex);
		m_meshObj->AddVertex(poly, 2, v3->xyz, uvs_3, tangent, v3->rgba, v3->normal, false, v3->origIndex);
	}

#ifdef STATS
	endtime = KX_GetActiveEngine()->GetRealTime();
	vertexAddingTime += endtime - starttime;
	starttime = KX_GetActiveEngine()->GetRealTime();
#endif
}

void KX_Chunk::ConstructVertexes()
{
#ifdef STATS
	double starttime;
	double endtime;
#endif

	/* Schéma global de l'organisation des faces dans le chunk
	 * Attention la colonne "BACK" est inversé avec la colonne "FRONT"
	 * 
	 *       1(left)     2(right)
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
	starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	/* On met à zero le numéro actuelle de vertices, à chaque fois que l'on ajoute un vertice
	 * cette variable est incrementé de 1, puis on la reutilise pour allouer le tableau de vertices
	 * partagés lors de la création des faces.
	 */
	m_originVertexIndex = 0;

	m_maxVertexHeight = 0.0;
	m_minVertexHeight = 0.0;

	for (unsigned short i = 0; i < 4; ++i)
		m_columns[i] = new JointColumn((i < 2));

	// on construit tous les vertices et on stocke leur numero dans des colonnes
	for(unsigned short columnIndex = 1; columnIndex < (VERTEX_COUNT - 1); ++columnIndex) {
		for(unsigned short vertexIndex = 1; vertexIndex < (VERTEX_COUNT - 1) ; ++vertexIndex) {
			// on créer un vertice temporaire, ces donné seront reutilisé lors de la création des polygones
			Vertex *vertex = NewVertex(columnIndex, vertexIndex);
			m_center[columnIndex - 1][vertexIndex - 1] = vertex;

			if (columnIndex == 1)
				m_columns[COLUMN_LEFT]->SetInternVertex(vertexIndex - 1, vertex);
			else if (columnIndex == VERTEX_COUNT_INTERN)
				m_columns[COLUMN_RIGHT]->SetInternVertex(vertexIndex - 1, vertex);

			if (vertexIndex == 1)
				m_columns[COLUMN_FRONT]->SetInternVertex(columnIndex - 1, vertex);
			else if (vertexIndex == VERTEX_COUNT_INTERN)
				m_columns[COLUMN_BACK]->SetInternVertex(columnIndex - 1, vertex);
		}
	}

	// construction des vertices externes de gauche et de droite
	for (unsigned short vertexIndex = 0; vertexIndex < VERTEX_COUNT; ++vertexIndex) {
		{ // colonne de gauche externe
			Vertex* vertex = NewVertex(0, vertexIndex);
			m_columns[COLUMN_LEFT]->SetExternVertex(vertexIndex, vertex);

			if (vertexIndex == 0)
				m_columns[COLUMN_FRONT]->SetExternVertex(0, vertex);
			else if (vertexIndex == (VERTEX_COUNT - 1))
				m_columns[COLUMN_BACK]->SetExternVertex(0, vertex);
		}
		{ // colonne de droite externe
			Vertex *vertex = NewVertex((VERTEX_COUNT - 1), vertexIndex);
			m_columns[COLUMN_RIGHT]->SetExternVertex(vertexIndex, vertex);

			if (vertexIndex == 0)
				m_columns[COLUMN_FRONT]->SetExternVertex(VERTEX_COUNT - 1, vertex);
			else if (vertexIndex == (VERTEX_COUNT - 1))
				m_columns[COLUMN_BACK]->SetExternVertex(VERTEX_COUNT - 1, vertex);
		}
	}

	for (unsigned short vertexIndex = 1; vertexIndex < (VERTEX_COUNT - 1); ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_FRONT; columnIndex <= COLUMN_BACK; ++columnIndex) {
			Vertex *vertex = NewVertex(vertexIndex, (columnIndex == COLUMN_FRONT) ? 0 : (VERTEX_COUNT - 1));
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			m_columns[columnIndex]->SetExternVertex(vertexIndex, vertex);
		}
	}

#ifdef STATS
	endtime = KX_GetActiveEngine()->GetRealTime();
	vertexCreatingTime += endtime - starttime;
	starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	for (unsigned short columnIndex = COLUMN_LEFT; columnIndex <= COLUMN_RIGHT; ++columnIndex) {
		for (unsigned short vertexIndex = 0; vertexIndex < VERTEX_COUNT; ++vertexIndex) {
			Vertex *vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);
			vertex->normal = GetNormal(vertex, false);
		}
	}

	for (unsigned short columnIndex = COLUMN_FRONT; columnIndex <= COLUMN_BACK; ++columnIndex) {
		for (unsigned short vertexIndex = 1; vertexIndex < (VERTEX_COUNT - 1); ++vertexIndex) {
			Vertex *vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);
			vertex->normal = GetNormal(vertex, false);
		}
	}

	for (unsigned short columnIndex = 1; columnIndex < (VERTEX_COUNT - 1); ++columnIndex) {
		for (unsigned short vertexIndex = 1; vertexIndex < (VERTEX_COUNT - 1); ++vertexIndex) {
			Vertex *vertex = m_center[columnIndex - 1][vertexIndex - 1];
			vertex->normal = GetNormal(vertex, true);
		}
	}

#ifdef STATS
	endtime = KX_GetActiveEngine()->GetRealTime();
	normalComputingTime += endtime - starttime;
#endif
}

void KX_Chunk::InvalidateJointVertexes()
{
	for (unsigned short vertexIndex = 0; vertexIndex < VERTEX_COUNT; ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_LEFT; columnIndex <= COLUMN_RIGHT; ++columnIndex) {
			Vertex* vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);
			if (m_lastHasJoint[columnIndex] && vertexIndex % (m_lastHasJoint[columnIndex] * 2))
				vertex->Invalidate();
			else
				vertex->Validate();
		}
	}

	for (unsigned short vertexIndex = 1; vertexIndex < (VERTEX_COUNT - 1); ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_FRONT; columnIndex <= COLUMN_BACK; ++columnIndex) {
			Vertex* vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);
			if (m_lastHasJoint[columnIndex] && vertexIndex % (m_lastHasJoint[columnIndex] * 2))
				vertex->Invalidate();
			else
				vertex->Validate();
		}
	}
}

void KX_Chunk::ConstructPolygones()
{
	m_meshObj->m_sharedvertex_map.resize(m_originVertexIndex);
	ConstructCenterColumnPolygones();

	ConstructJointColumnPolygones(m_columns[COLUMN_LEFT], false);
	ConstructJointColumnPolygones(m_columns[COLUMN_RIGHT], true);
	ConstructJointColumnPolygones(m_columns[COLUMN_FRONT], true);
	ConstructJointColumnPolygones(m_columns[COLUMN_BACK], false);
}

void KX_Chunk::ConstructCenterColumnPolygones()
{
	// on lit toutes les colonnes pour créer des faces
	for (unsigned int columnIndex = 0; columnIndex < POLY_COUNT - 2; ++columnIndex) {
		for (unsigned int vertexIndex = 0; vertexIndex < POLY_COUNT - 2; ++vertexIndex) {
			Vertex *firstVertex = m_center[columnIndex][vertexIndex];
			Vertex *secondVertex = m_center[columnIndex + 1][vertexIndex];
			Vertex *thirdVertex = m_center[columnIndex + 1][vertexIndex + 1];
			Vertex *fourthVertex = m_center[columnIndex][vertexIndex + 1];

			// création du premier triangle
			AddMeshPolygonVertexes(firstVertex, secondVertex, thirdVertex, false);
			// création du deuxieme triangle
			AddMeshPolygonVertexes(firstVertex, thirdVertex, fourthVertex, false);
		}
	}
}

void KX_Chunk::ConstructJointColumnPolygones(JointColumn *column, bool reverse)
{
	/*     
	 * 1)	   2)	   3)
	 * 
	 * 1       1--2    1--2
	 * |\      | /     |\ |
	 * | \     |/      | \|
	 * 3--2    3       4--3
	 */

	for (unsigned short vertexIndex = 0; vertexIndex < POLY_COUNT; ++vertexIndex) {
		bool firstTriangle = true;
		bool secondTriangle = true;
		Vertex *firstVertex, *secondVertex, *thirdVertex, *fourthVertex;

		// les seuls points fixes
		firstVertex = column->GetExternVertex(vertexIndex);
		fourthVertex = column->GetExternVertex(vertexIndex + 1);

		if (!fourthVertex->valid) {
			fourthVertex = column->GetExternVertexNext(vertexIndex);
			if (!fourthVertex) {
				std::cout << "Error fourth vertex NULL ! , index : " << vertexIndex << std::endl;
			}
		}

		// debut de la colonne cas 1
		if (vertexIndex == 0) {
			secondVertex = column->GetInternVertex(0);
			thirdVertex = fourthVertex;
			secondTriangle = false;
		}
		// fin de la colonne cas 2
		else if (vertexIndex == (POLY_COUNT - 1)) {
			// si il y aun joit on ne fait rien
			if (!firstVertex->valid) {
				firstTriangle = false;
			}
			secondVertex = column->GetInternVertex(vertexIndex - 1);
			thirdVertex = column->GetExternVertex(POLY_COUNT);
			secondTriangle = false;
		}
		// milieu de la colonne cas 3
		else {
			secondVertex = column->GetInternVertex(vertexIndex - 1);
			thirdVertex = column->GetInternVertex(vertexIndex);
			if (!firstVertex->valid) {
				firstVertex = fourthVertex;
				secondTriangle = false;
			}
		}

		if (firstTriangle) {
			// création du premier triangle
			AddMeshPolygonVertexes(firstVertex, secondVertex, thirdVertex, reverse);
		}
		if (secondTriangle) {
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

	m_lastChunkNode[COLUMN_LEFT] = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x - relativewidth, pos.y));
	m_lastChunkNode[COLUMN_RIGHT] = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x + relativewidth, pos.y));
	m_lastChunkNode[COLUMN_FRONT] = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x, pos.y - relativewidth));
	m_lastChunkNode[COLUMN_BACK] = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x, pos.y + relativewidth));

	// ensemble de 4 variables verifiant si il y aura besoin d'une jointure
	const unsigned short hasJointLeft = m_lastChunkNode[COLUMN_LEFT] ? (m_node->GetLevel() - m_lastChunkNode[COLUMN_LEFT]->GetLevel()) : 0;
	const unsigned short hasJointRight = m_lastChunkNode[COLUMN_RIGHT] ? (m_node->GetLevel() - m_lastChunkNode[COLUMN_RIGHT]->GetLevel()) : 0;
	const unsigned short hasJointFront = m_lastChunkNode[COLUMN_FRONT] ? (m_node->GetLevel() - m_lastChunkNode[COLUMN_FRONT]->GetLevel()) : 0;
	const unsigned short hasJointBack = m_lastChunkNode[COLUMN_BACK] ? (m_node->GetLevel() - m_lastChunkNode[COLUMN_BACK]->GetLevel()) : 0;

	if (!m_meshObj) {
		m_lastHasJoint[COLUMN_LEFT] = hasJointLeft;
		m_lastHasJoint[COLUMN_RIGHT] = hasJointRight;
		m_lastHasJoint[COLUMN_FRONT] = hasJointFront;
		m_lastHasJoint[COLUMN_BACK] = hasJointBack;

		ReconstructMesh();
	}
	else if (m_lastHasJoint[COLUMN_LEFT] != hasJointLeft ||
		m_lastHasJoint[COLUMN_RIGHT] != hasJointRight ||
		m_lastHasJoint[COLUMN_FRONT] != hasJointFront ||
		m_lastHasJoint[COLUMN_BACK] != hasJointBack)
	{
		m_lastHasJoint[COLUMN_LEFT] = hasJointLeft;
		m_lastHasJoint[COLUMN_RIGHT] = hasJointRight;
		m_lastHasJoint[COLUMN_FRONT] = hasJointFront;
		m_lastHasJoint[COLUMN_BACK] = hasJointBack;

		ReconstructMesh();
	}
	m_node->CheckBoxHeight(m_maxVertexHeight, m_minVertexHeight);
}

void KX_Chunk::EndUpdateMesh()
{
}

void KX_Chunk::RenderMesh(RAS_IRasterizer *rasty, KX_Camera *cam)
{
// 	const MT_Point3 realPos = MT_Point3(m_node->GetRealPos().x(), m_node->GetRealPos().y(), 0.);
// 	const MT_Point3 camPos = cam->NodeGetWorldPosition();
// 	const MT_Vector3 norm = (camPos - realPos).normalized() * sqrt(m_node->GetRadius2());

	/*KX_RasterizerDrawDebugLine(realPos + MT_Point3(0.0, 0.0, m_minVertexHeight + 1.0),
							   realPos + MT_Point3(0.0, 0.0, m_maxVertexHeight - 1.0), MT_Vector3(1., 0., 0.));*/
}