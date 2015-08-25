#include "KX_Terrain.h"
#include "KX_Chunk.h"
#include "KX_ChunkMotionState.h"

#include "KX_Camera.h"
#include "KX_PythonInit.h"

#include "RAS_IRasterizer.h"
#include "RAS_MaterialBucket.h"
#include "RAS_MeshObject.h"
#include "RAS_Polygon.h"
#include "RAS_IPolygonMaterial.h"

#include "PHY_IPhysicsController.h"
#ifdef WITH_BULLET
#include "CcdPhysicsEnvironment.h"
#include "CcdPhysicsController.h"
#include "CcdGraphicController.h"
#endif

#include "BLI_math.h"
#include "MT_assert.h"

#include "DNA_material_types.h"

#include <stdio.h>

#define DEBUG(msg) std::cout << msg << std::endl;
#define DEBUG_HEADER(msg) DEBUG("====================== " << msg << " ======================");

unsigned int KX_Chunk::m_chunkActive = 0;
unsigned int KX_Chunk::meshRecreation = 0;
double KX_Chunk::chunkCreationTime = 0.0;
double KX_Chunk::normalComputingTime = 0.0;
double KX_Chunk::vertexAddingTime = 0.0;
double KX_Chunk::vertexCreatingTime = 0.0;
double KX_Chunk::polyAddingTime = 0.0;
double KX_Chunk::physicsCreatingTime = 0.0;

// La colonne opposée.
static KX_Chunk::COLUMN_TYPE OppositeColumn(KX_Chunk::COLUMN_TYPE columnType)
{
	if (columnType == KX_Chunk::COLUMN_LEFT)
		return KX_Chunk::COLUMN_RIGHT;
	else if (columnType == KX_Chunk::COLUMN_RIGHT)
		return KX_Chunk::COLUMN_LEFT;
	else if (columnType == KX_Chunk::COLUMN_FRONT)
		return KX_Chunk::COLUMN_BACK;
	else if (columnType == KX_Chunk::COLUMN_BACK)
		return KX_Chunk::COLUMN_FRONT;
	return KX_Chunk::COLUMN_NONE;
}

static KX_Chunk::COLUMN_TYPE BackColumn(KX_Chunk::COLUMN_TYPE columnType)
{
	if (columnType == KX_Chunk::COLUMN_LEFT)
		return KX_Chunk::COLUMN_BACK;
	else if (columnType == KX_Chunk::COLUMN_RIGHT)
		return KX_Chunk::COLUMN_BACK;
	else if (columnType == KX_Chunk::COLUMN_FRONT)
		return KX_Chunk::COLUMN_RIGHT;
	else if (columnType == KX_Chunk::COLUMN_BACK)
		return KX_Chunk::COLUMN_RIGHT;
	return KX_Chunk::COLUMN_NONE;
}

static KX_Chunk::COLUMN_TYPE FrontColumn(KX_Chunk::COLUMN_TYPE columnType)
{
	return OppositeColumn(BackColumn(columnType));
}

// L'axe d'alignement de la colonne.
static unsigned short ColumnAxis(KX_Chunk::COLUMN_TYPE columnType)
{
	if (columnType == KX_Chunk::COLUMN_LEFT || columnType == KX_Chunk::COLUMN_RIGHT)
		return 1; // alignée sur y donc vers le haut.
	else if (columnType == KX_Chunk::COLUMN_FRONT || columnType == KX_Chunk::COLUMN_BACK)
		return 0; // alignée sur x donc a l'horizontale.
	return 3;
}

// Déplace un point d'une valeur precise vers le centre ou à l'opossé.
static void MoveVectorToCenterColumn(KX_Chunk::COLUMN_TYPE columnType, short &x, short &y, short length)
{
	if (columnType == KX_Chunk::COLUMN_LEFT)
		x += length;
	else if (columnType == KX_Chunk::COLUMN_RIGHT)
		x -= length;
	else if (columnType == KX_Chunk::COLUMN_BACK)
		y -= length;
	else if (columnType == KX_Chunk::COLUMN_FRONT)
		y += length;
}

// Déplace un point d'une valeur precise vers le debut de la colonne.
static void MoveVectorFrontColumn(KX_Chunk::COLUMN_TYPE columnType, short &x, short &y, short length)
{
	if (columnType == KX_Chunk::COLUMN_LEFT || columnType == KX_Chunk::COLUMN_RIGHT)
		y -= length;
	else if (columnType == KX_Chunk::COLUMN_FRONT || columnType == KX_Chunk::COLUMN_BACK)
		x -= length;
}

// Déplace un point d'une valeur precise vers la fin de la colonne.
static void MoveVectorBackColumn(KX_Chunk::COLUMN_TYPE columnType, short &x, short &y, short length)
{
	if (columnType == KX_Chunk::COLUMN_LEFT || columnType == KX_Chunk::COLUMN_RIGHT)
		y += length;
	else if (columnType == KX_Chunk::COLUMN_FRONT || columnType == KX_Chunk::COLUMN_BACK)
		x += length;
}

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
	double totalTime = chunkCreationTime + normalComputingTime + 
			vertexAddingTime + vertexCreatingTime + polyAddingTime + physicsCreatingTime;

	std::cout << "Time Stats : " << std::endl
		<< "\t Mesh Recreation Per Frame : \t" << meshRecreation << std::endl
		<< "\t Total Compute Time : \t\t" << totalTime << std::endl
		<< "\t Chunk Creation Time : \t\t" << chunkCreationTime / totalTime * 100.0f << "%" << std::endl
		<< "\t Normal Computing Time : \t" << normalComputingTime / totalTime * 100.0f << "%" << std::endl
		<< "\t Vertex Adding Time : \t\t" << vertexAddingTime / totalTime * 100.0f << "%" << std::endl
		<< "\t Vertex Creating Time : \t" << vertexCreatingTime / totalTime * 100.0f << "%" << std::endl
		<< "\t Poly Adding Time : \t\t" << polyAddingTime / totalTime * 100.0f << "%" << std::endl
		<< "\t Physics Creating Time : \t" << physicsCreatingTime / totalTime * 100.0f << "%" << std::endl
		<< std::endl;
	std::cout << "Memory Stats : " << std::endl
		<< "\t Active Chunks Count : \t" << m_chunkActive << std::endl
		<< "\t Active Nodes Count : \t" << KX_ChunkNode::m_activeNode << std::endl
		<< std::endl;
}

/** Vertex utilisé pour la construction du mesh du chunk
 */
struct KX_Chunk::Vertex
{
	/** La couleur du vertice, sa hauteur ect…
	 * Ces informations doivent être le plus partagées ente les
	 * chunk lors d'une subdivision ou d'une regression.
	 * \see VertexZoneInfo
	 */
	VertexZoneInfo *vertexInfo; // 1

	/** La position relative du vertice dans le chunk,
	 * a toujours la même echelle d'un chunk a l'autre.
	 */
	short relativePos[2]; // 2 * 2 = 4

	/** La position absolue du vertice dans le mesh du chunk,
	 * cette fois ci elle depend de la taile du chunk.
	 */
	float absolutePos[2]; // 4 * 3 = 12

	/// L'indice de creation du vertice, utilisé lors de la construction du mesh.
	unsigned char origIndex; // 1

	/// L'indice du vertex dans le mesh
	short vertIndex; // 2

	/** Le vertice peut être utilisé dans le mesh ?
	 * Très utile pour les jointures, lorsque le vertice est
	 * invalide on passe au suivant et ainsi créant la jointure.
	 */
	bool valid; // 1

	bool validNormal;

	/// La normale du vertice.
	float normal[3]; // 4 * 3 = 12
	// 32 octets

	Vertex(VertexZoneInfo *info,
		   short relx, short rely,
		   float absx, float absy,
		   unsigned int origindex)
		:vertexInfo(info),
		origIndex(origindex),
		vertIndex(-1),
		valid(true),
		validNormal(false)
	{
		relativePos[0] = relx;
		relativePos[1] = rely;

		absolutePos[0] = absx;
		absolutePos[1] = absy;
	}

	~Vertex()
	{
		vertexInfo->Release();
	}

	void Invalidate()
	{
		valid = false;
	}

	void Validate()
	{
		valid = true;
	}
};

struct KX_Chunk::JointColumn
{
	/// Tous les vertices à l'exterieur du chunk.
	Vertex *m_columnExterne[VERTEX_COUNT];
	/// Tous les vertices une colonne avant l'exterieur.
	Vertex *m_columnInterne[VERTEX_COUNT_INTERN];
	/// Si vrai alors on supprime les vertices aux extrémité.
	bool m_freeEndVertex;

	JointColumn(bool freeEndVertex)
		:m_freeEndVertex(freeEndVertex)
	{
	}

	~JointColumn()
	{
		/** Si m_freeEndVertex et faux on itère que de 1 à POLY_COUNT
		 * sinon de 0 a POLY_COUNT + 1.
		 */
		for (unsigned short i = (m_freeEndVertex ? 0 : 1);
			 i < (POLY_COUNT + (m_freeEndVertex ? 1 : 0)); ++i) 
		{
			delete m_columnExterne[i];
		}
	}

	void SetExternVertex(unsigned short index, Vertex *vertex)
	{
		m_columnExterne[index] = vertex;
	}
	void SetInternVertex(unsigned short index, Vertex *vertex)
	{ 
		m_columnInterne[index] = vertex;
	}

	Vertex *GetInternVertex(unsigned short index)
	{
		return m_columnInterne[index];
	}
	Vertex *GetExternVertex(unsigned short index)
	{
		return m_columnExterne[index];
	}

	/// Cherche le vertice suivant valide.
	Vertex *GetNextValidExternVertex(unsigned short index)
	{
		for (++index; index < VERTEX_COUNT; ++index) {
			Vertex *vertex = m_columnExterne[index];
			if (vertex->valid)
				return vertex;
		}
		return NULL;
	}
};

KX_Chunk::KX_Chunk(KX_ChunkNode *node, RAS_MaterialBucket *bucket)
	:m_node(node),
	m_bucket(bucket),
	m_meshObj(NULL),
	m_physicsController(NULL),
	m_visible(true),
	m_hasVertexes(false),
	m_onConstruct(false)
{
	m_lastHasJoint[COLUMN_LEFT] = false;
	m_lastHasJoint[COLUMN_RIGHT] = false;
	m_lastHasJoint[COLUMN_FRONT] = false;
	m_lastHasJoint[COLUMN_BACK] = false;

#if 0
	char c[3];
	sprintf(c, "%d", m_chunkActive);
	SetName("KX_Chunk" + STR_String(c));
#endif

	m_chunkActive++;

	const MT_Point2 &nodepos = m_node->GetRealPos();

	m_meshMatrix[0] = 1.0f; m_meshMatrix[4] = 0.0f; m_meshMatrix[8] = 0.0f; m_meshMatrix[12] = nodepos.x();
	m_meshMatrix[1] = 0.0f; m_meshMatrix[5] = 1.0f; m_meshMatrix[9] = 0.0f; m_meshMatrix[13] = nodepos.y();
	m_meshMatrix[2] = 0.0f; m_meshMatrix[6] = 0.0f; m_meshMatrix[10] = 1.0f; m_meshMatrix[14] = 0.0f;
	m_meshMatrix[3] = 0.0f; m_meshMatrix[7] = 0.0f; m_meshMatrix[11] = 0.0f; m_meshMatrix[15] = 1.0f;
}

KX_Chunk::~KX_Chunk()
{
	DestructMesh();

	if (m_hasVertexes) {
		for (unsigned short i = 0; i < VERTEX_COUNT_INTERN; ++i) {
			for (unsigned short j = 0; j < VERTEX_COUNT_INTERN; ++j) {
				delete m_center[i][j];
			}
		}

		delete m_columns[0];
		delete m_columns[1];
		delete m_columns[2];
		delete m_columns[3];
	}

	if (m_physicsController)
		delete m_physicsController;

	m_chunkActive--;
}

/// Creation du nouveau mesh.
void KX_Chunk::ConstructMesh()
{
	m_meshObj = new RAS_MeshObject(NULL);
}

// Destruction du mesh.
void KX_Chunk::DestructMesh()
{
	if (m_meshObj) {
		m_meshObj->RemoveFromBuckets(this);
		/* Chaque mesh est unique or le BGE garde une copie pour pouvoir le
		 * dupliquer plus tard, cette copie ce trouve dans meshmat->m_baseslot.
		 * Donc on la supprime.
		 */
		RAS_MeshMaterial *meshmat = m_meshObj->GetMeshMaterial((unsigned int)0);
		meshmat->m_bucket->RemoveMesh(meshmat->m_baseslot);
		delete m_meshObj;
	}
}

void KX_Chunk::ConstructPhysicsController()
{
	KX_Terrain *terrain = m_node->GetTerrain();

	if (m_node->GetLevel() < terrain->GetMinPhysicsLevel())
		return;

#ifdef WITH_BULLET
	Material *material = terrain->GetBlenderMaterial();

	CcdPhysicsController *phyCtrl = (CcdPhysicsController *)m_physicsController;
	CcdPhysicsEnvironment *phyEnv = (CcdPhysicsEnvironment *)terrain->GetScene()->GetPhysicsEnvironment();

	CcdShapeConstructionInfo *shapeInfo = phyCtrl ? 
				phyCtrl->GetShapeInfo() : new CcdShapeConstructionInfo();

	shapeInfo->m_shapeType = PHY_SHAPE_MESH;
	// On met a jour le mesh de la forme physique.
	shapeInfo->UpdateMesh(NULL, m_meshObj);

	// Puis on créer la forme physique.
	btCollisionShape *shape = shapeInfo->CreateBulletShape(0.0f);

	// Si le controlleur physique n'existe pas alors on le créer.
	if (!phyCtrl) {
		CcdConstructionInfo ci;

		ci.m_collisionShape = shape;
		ci.m_shapeInfo = shapeInfo;
		ci.m_MotionState = new KX_ChunkMotionState(m_node);
		ci.m_physicsEnv = phyEnv;
		ci.m_fh_damping = material->xyfrict;
		ci.m_fh_distance = material->fhdist;
		ci.m_fh_spring = material->fh;
		ci.m_friction = material->friction;
		ci.m_restitution = material->reflect;
		ci.m_collisionFilterMask = CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::StaticFilter;
		ci.m_collisionFilterGroup = CcdConstructionInfo::StaticFilter;

		phyCtrl = new CcdPhysicsController(ci);
		phyCtrl->SetNewClientInfo(terrain->getClientInfo());

		// Puis on l'ajoute dans l'environnement physique.
		phyEnv->AddCcdPhysicsController(phyCtrl);
	}
	else {
		/* Sinon si le controlleur physique existe déjà on remplace juste
		 * la forme physique.
		 */
		phyCtrl->ReplaceControllerShape(shape);
	}

	m_physicsController = phyCtrl;
#endif
}

KX_Chunk::Vertex *KX_Chunk::NewVertex(unsigned short relx, unsigned short rely)
{
	KX_Terrain *terrain = m_node->GetTerrain();
	// la position du noeud parent du chunk
	const MT_Point2& nodepos = m_node->GetRealPos();
	//La taille "réelle" du chunk
	const float size = terrain->GetChunkSize();

	const unsigned short relativesize = m_node->GetRelativeSize();
	//Calcul de l'intervalle entre deux points
	const float interval = size * relativesize / POLY_COUNT;
	// la motie de la largeur du chunk
	const float width = size / 2 * relativesize;

	const float vertx = relx * interval - width;
	const float verty = rely * interval - width;

	// toutes les informations sur le vertice dut au zone : la hauteur, sa couleur
	VertexZoneInfo *info = terrain->GetVertexInfo(nodepos.x() + vertx, nodepos.y() + verty);
	const float vertz = info->height;

	if (!m_requestCreateBox) {
		if (vertz > m_maxVertexHeight)
			m_maxVertexHeight = vertz;
		else if (vertz < m_minVertexHeight)
			m_minVertexHeight = vertz;
	}
	else {
		m_maxVertexHeight = vertz;
		m_minVertexHeight = vertz;
		m_requestCreateBox = false;
	}

	Vertex *vertex = new Vertex(info, relx, rely, vertx, verty, m_originVertexIndex++);
	return vertex;
}

/* Cette fonction permet d'acceder à un vertice comme si ils étaient
 * stockés dans un tableau à deux dimension pour les x et y.
 * C'est très utile pour le calcule des normales.
 */
KX_Chunk::Vertex *KX_Chunk::GetVertexByChunkRelativePosition(unsigned short x, unsigned short y) const
{
	Vertex *vertex = NULL;

	// Pour un vertice interne.
	if ((0 < x && x < (VERTEX_COUNT - 1)) &&
		(0 < y && y < (VERTEX_COUNT - 1)))
	{
		vertex = m_center[x - 1][y - 1];
	}
	// Sinon pour les colonnes
	else if (x == 0)
		vertex = m_columns[COLUMN_LEFT]->GetExternVertex(y);
	else if (x == (VERTEX_COUNT - 1))
		vertex = m_columns[COLUMN_RIGHT]->GetExternVertex(y);
	else if (y == 0)
		vertex = m_columns[COLUMN_FRONT]->GetExternVertex(x);
	else if (y == (VERTEX_COUNT - 1))
		vertex = m_columns[COLUMN_BACK]->GetExternVertex(x);

	return vertex;
}

KX_Chunk::Vertex *KX_Chunk::GetVertexByTerrainRelativePosition(int x, int y) const
{
	const unsigned short size = m_node->GetRelativeSize();
	// l'espace entre 2 vertice * 2 : size / 4 * 2
	const unsigned short interval = size * 2 / 4;

	const KX_ChunkNode::Point2D nodepos = m_node->GetRelativePos();

	// le bas du chunk par rapport au terrain * 2 pour les vertices
	const int bottomx = nodepos.x * 2 - interval * 2;
	const int bottomy = nodepos.y * 2 - interval * 2;

	const unsigned short diffx = x - bottomx;
	const unsigned short diffy = y - bottomy;

	return GetVertexByChunkRelativePosition(diffx / interval, diffy / interval);
}

KX_ChunkNode::Point2D KX_Chunk::GetTerrainRelativeVertexPosition(unsigned short x, unsigned short y) const
{
	const unsigned short size = m_node->GetRelativeSize();
	const unsigned short interval = size * 2 / 4;

	const KX_ChunkNode::Point2D nodepos = m_node->GetRelativePos();

	// le bas du chunk par rapport au terrain * 2 pour les vertices
	const int bottomx = nodepos.x * 2 - interval * 2;
	const int bottomy = nodepos.y * 2 - interval * 2;

	return KX_ChunkNode::Point2D(bottomx + x * interval, bottomy + y * interval);
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
 * Ces quatre points doivent être des vertices déjà existant.
 */
void KX_Chunk::SetNormal(Vertex *vertexCenter) const
{
	// le quadrilatère
	float quad[4][3];

	/* Si le vertice n'est pas un vertice adjacent il suffit de
	 * recupérer les 4 vertice autour.
	 */
	// la position relative du vertice
	const unsigned short relx = vertexCenter->relativePos[0];
	const unsigned short rely = vertexCenter->relativePos[1];

	// on trouve les 4 vertices adjacents
	Vertex *vertexes[4] = {
		GetVertexByChunkRelativePosition(relx + 1, rely),
		GetVertexByChunkRelativePosition(relx, rely + 1),
		GetVertexByChunkRelativePosition(relx - 1, rely),
		GetVertexByChunkRelativePosition(relx, rely - 1)
	};
	// Puis pour chaque vertice on copie sa position en 3d
	for (unsigned short i = 0; i < 4; ++i) {
		quad[i][0] = vertexes[i]->absolutePos[0];
		quad[i][1] = vertexes[i]->absolutePos[1];
		quad[i][2] = vertexes[i]->vertexInfo->height;
	}

	normal_quad_v3(vertexCenter->normal, quad[0], quad[1], quad[2], quad[3]);
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
	const MT_Point2& nodepos = m_node->GetRealPos();

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
		uvs_1[i] = MT_Point2(v1->absolutePos) + nodepos;
		uvs_2[i] = MT_Point2(v2->absolutePos) + nodepos;
		uvs_3[i] = MT_Point2(v3->absolutePos) + nodepos;
	}

	const MT_Vector4 tangent(0.0f, 0.0f, 0.0f, 0.0f);

	if (v1->vertIndex == -1) {
		v1->vertIndex = m_meshObj->AddVertex(m_bucket, MT_Point3(v1->absolutePos[0], v1->absolutePos[1], v1->vertexInfo->height),
				uvs_1, tangent, 0, v1->normal, false, v1->origIndex, 3);
	}
	if (v2->vertIndex == -1) {
		v2->vertIndex = m_meshObj->AddVertex(m_bucket, MT_Point3(v2->absolutePos[0], v2->absolutePos[1], v2->vertexInfo->height),
				uvs_2, tangent, 0, v2->normal, false, v2->origIndex, 3);
	}
	if (v3->vertIndex == -1) {
		v3->vertIndex = m_meshObj->AddVertex(m_bucket, MT_Point3(v3->absolutePos[0], v3->absolutePos[1], v3->vertexInfo->height), 
				uvs_3, tangent, 0, v3->normal, false, v3->origIndex, 3);
	}

	if (reverse) {
		m_meshObj->AddPolygonVertex(poly, 0, v3->vertIndex);
		m_meshObj->AddPolygonVertex(poly, 1, v2->vertIndex);
		m_meshObj->AddPolygonVertex(poly, 2, v1->vertIndex);
	}
	else {
		m_meshObj->AddPolygonVertex(poly, 0, v1->vertIndex);
		m_meshObj->AddPolygonVertex(poly, 1, v2->vertIndex);
		m_meshObj->AddPolygonVertex(poly, 2, v3->vertIndex);
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
	 *  y    |           |
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
	 *  |__________________\ x
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

	m_requestCreateBox = true;

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

	for (unsigned short columnIndex = 1; columnIndex < (VERTEX_COUNT - 1); ++columnIndex) {
		for (unsigned short vertexIndex = 1; vertexIndex < (VERTEX_COUNT - 1); ++vertexIndex)
			SetNormal(m_center[columnIndex - 1][vertexIndex - 1]);
	}

#ifdef STATS
	endtime = KX_GetActiveEngine()->GetRealTime();
	normalComputingTime += endtime - starttime;
#endif
}

/* On accede au vertice dans le chunk voisin le plus proche et alignée
 * au vertice referent.
 */
void KX_Chunk::GetCoorespondingVertexesFromChunk(KX_ChunkNode *jointNode, Vertex *origVertex, COLUMN_TYPE columnType, 
												 Vertex **coExternVertex, Vertex **coInternVertex)
{
	// La position du vertice par rapport au terrain
	KX_ChunkNode::Point2D terrainVertexPos = GetTerrainRelativeVertexPosition(origVertex->relativePos[0], origVertex->relativePos[1]);

	KX_Chunk *jointChunk = jointNode->GetChunk();

	/** Ce vertice est a la même position que celui entrain de calculer la normal.
	 * Donc pour optimizer on copiera la normal.
	 */
	Vertex *externVertex = jointChunk->GetVertexByTerrainRelativePosition(terrainVertexPos.x, terrainVertexPos.y);
	if (coExternVertex)
		(*coExternVertex) = externVertex;

	if (externVertex->vertexInfo->pos[0] != origVertex->vertexInfo->pos[0] || 
		externVertex->vertexInfo->pos[1] != origVertex->vertexInfo->pos[1])
	{
		DEBUG("wrong coresponding vertex");
		return;
	}

	if (coInternVertex) {
		if (!externVertex) {
			DEBUG("can't compute intern vertex");
			return;
		}

		const COLUMN_TYPE opposedColumn = OppositeColumn(columnType);
		// la colonne adjacent au bas de la colonne opposée.
		const COLUMN_TYPE backColumn = BackColumn(opposedColumn);
		// la colonne adjacent au bas de la colonne opposée.
		const COLUMN_TYPE frontColumn = FrontColumn(opposedColumn);

		const unsigned short frontColumnJointLevel = jointChunk->GetJointLevel(frontColumn);
		const unsigned short frontColumnVertexRatio = ((frontColumnJointLevel > 0) ? frontColumnJointLevel * 2 : 1);
		const unsigned short backColumnJointLevel = jointChunk->GetJointLevel(backColumn);
		const unsigned short backColumnVertexRatio = (backColumnJointLevel > 0) ? backColumnJointLevel * 2 : 1;

		short x = externVertex->relativePos[0];
		short y = externVertex->relativePos[1];

		const unsigned short vertexIndex = (ColumnAxis(opposedColumn) == 0) ? x : y;

		const bool onFront = (vertexIndex == 0);
		const bool onBack = (vertexIndex == (VERTEX_COUNT - 1));

		// Le decalage en vertice par rapport a terrain.
		const unsigned short vertexRatio = onFront ? frontColumnVertexRatio : (onBack ? backColumnVertexRatio : 1);

		// Puis on decale cette position vers l'interieur.
		MoveVectorToCenterColumn(opposedColumn, x, y, vertexRatio);
		(*coInternVertex) = jointChunk->GetVertexByChunkRelativePosition(x, y);
	}
}

#if 1
void KX_Chunk::ComputeColumnJointVertexNormal(COLUMN_TYPE columnType, bool reverse)
{
	// La colonne adjacente au sommet de cette colonne.
	const COLUMN_TYPE backColumn = BackColumn(columnType);
	// La colonne adjacente au bas de cette colonne.
	const COLUMN_TYPE frontColumn = FrontColumn(columnType);
	// Le numero de noeud sur les colonnes adjacentes.
	const unsigned short backFrontNodeIndex = (columnType == COLUMN_LEFT || columnType == COLUMN_FRONT) ? 1 : (VERTEX_COUNT - 1);
	// Le noeud adjacent au haut de la colonne.
	KX_ChunkNode *backNode = m_jointNode[backColumn][backFrontNodeIndex];
	// Le noeud adjacent au bas de la colonne.
	KX_ChunkNode *frontNode = m_jointNode[frontColumn][backFrontNodeIndex];

	// Le niveau de jointure sur cette colonne.
	const unsigned short columnJointLevel = m_lastHasJoint[columnType];
	// Le ratio de vertice sur une jointure 2/5 4/5 ect…
	const unsigned short vertexRatio = (columnJointLevel > 0) ? columnJointLevel * 2 : 1;

	const unsigned short frontColumnJointLevel = m_lastHasJoint[frontColumn];
	const unsigned short frontVertexRatio = (frontColumnJointLevel > 0) ? frontColumnJointLevel * 2 : 1;

	const unsigned short backColumnJointLevel = m_lastHasJoint[backColumn];
	const unsigned short backVertexRatio = (backColumnJointLevel > 0) ? backColumnJointLevel * 2 : 1;

	for (unsigned short vertexIndex = (columnType == COLUMN_LEFT || columnType == COLUMN_RIGHT) ? 0 : 1;
		 vertexIndex < ((columnType == COLUMN_LEFT || columnType == COLUMN_RIGHT) ? VERTEX_COUNT : (VERTEX_COUNT - 1));
		 ++vertexIndex)
	{
		Vertex *vertex = m_columns[columnType]->GetExternVertex(vertexIndex);
		if (!vertex->valid || vertex->validNormal)
			continue;

		/* Le 1er et 2eme vertice utilise le même chunk.
		 * 
		 * ----
		 * |  /| -- Ce chunk et commun a la colonne que de un point
		 * |C5 |    mais sont vertice commun doit avoir la même normale.
		 * ----v4
		 * |  /|
		 * |C4\|
		 * ----v3
		 * |  /|
		 * |C3 |
		 * ----v2
		 * |  /|
		 * |C2 |
		 * ----v1
		 * |  /|
		 * |C1 |
		 * ----v0
		 * |  /| -- Même chose ici.
		 * |C0 |
		 * ----
		 */

		const unsigned short nodeIndex = vertexIndex > 3 ? 4 : vertexIndex + 1;
		KX_ChunkNode *jointNode = m_jointNode[columnType][nodeIndex];
		if (!jointNode) {
			vertex->normal[0] = 0.0f;
			vertex->normal[1] = 0.0f;
			vertex->normal[2] = 1.0f;
			continue;
		}

		// Les 4 vertices du quadrilatère.
		Vertex *quadVertexes[4] = {NULL, NULL, NULL, NULL};
		/* Les 4 potentielles vertices coorespondant dans les chunks voisins,
		 * ces vertice utiliseront la même normale.
		 */
		Vertex *coVertexes[4] = {NULL, NULL, NULL, NULL};

		// Le quadrilatère utilisé pour calculer la normale.
		float quad[4][3];

		// La position du vertice central dans le chunk.
		const unsigned short vertx = vertex->relativePos[0];
		const unsigned short verty = vertex->relativePos[1];

		// Le vertice est à une extremitée du chunk.
		const bool onEdge = (vertx == 0 || vertx == (VERTEX_COUNT - 1) || verty == 0 || verty == (VERTEX_COUNT - 1));

		const bool onColumnFront = (vertexIndex == 0);
		const bool onColumnBack = (vertexIndex == (VERTEX_COUNT - 1));
		// Le vertice est a une extremitée de la colonne.
		const bool onColumnEdge = (onColumnFront || onColumnBack);

		/* iteration des 4 vertices autour :
		 *
		 *        v3       sens de la colonne
		 *        |        ^ back
		 *        |        |
		 * v2-----v-----v0 |
		 *        |        |----------> vers le centre du chunk
		 *        |        |
		 *        v1       | front
		 * 
		 * - v0 est toujours vers l'interieur du chunk sont calcul est le plus simple.
		 * - v1 et avant v dans la colonne donc entre 0 et 3, entre 1 et 3 le vertice est à
		 * l'interieur du chunk et à 0 il est à l'exterieur.
		 * - v3 et après v dans la colonne donc entre 1 et 4, entre 1 et 3 le vertice est à
		 * l'interieur du chunk et à 4 il est à l'exterieur.
		 * - v2 et toujours à l'exterieur du chunk.
		 */

		// Calcul de v0.
		{
			short x = vertx;
			short y = verty;
			/* On accede au vertice plus proche du centre de 1 unité si il n'y a pas
			 * de joint sinon du ratio de vertice sur la jointure.
			 */
			MoveVectorToCenterColumn(columnType, x, y, onColumnFront ? frontVertexRatio : (onColumnBack ? backVertexRatio : 1));
			// On acced finalement au vertice le plus simplement possible.
			quadVertexes[0] = GetVertexByChunkRelativePosition(x, y);
		}

		// Calcul de v1.
		{
			short x = vertx;
			short y = verty;


			if (vertexIndex > 0) {
				/* si le vertice n'est pas sur les extremitées on accede simplement au
				 * vertice avant.
				 */
				MoveVectorFrontColumn(columnType, x, y, onEdge ? vertexRatio : 1);
				quadVertexes[1] = GetVertexByChunkRelativePosition(x, y);

			}
			else if (frontNode)
				GetCoorespondingVertexesFromChunk(frontNode, vertex, frontColumn, coVertexes, quadVertexes + 1);
		}

		// Calcul de v2
		{
			if (jointNode)
				GetCoorespondingVertexesFromChunk(jointNode, vertex, columnType, coVertexes + 1, quadVertexes + 2);
		}

		// Calcul de v3.
		{
			short x = vertx;
			short y = verty;

			if (vertexIndex < (VERTEX_COUNT - 1)) {
				/* si le vertice n'est pas sur les extremitées on accede simplement au
				 * vertice après.
				 */
				MoveVectorBackColumn(columnType, x, y, onEdge ? vertexRatio : 1);
				quadVertexes[3] = GetVertexByChunkRelativePosition(x, y);
			}
			else if (backNode && backNode->GetChunk())
				GetCoorespondingVertexesFromChunk(backNode, vertex, backColumn, coVertexes + 2, quadVertexes + 3);
		}

		/** Si le vertice client et sur une extremitées il a alors 3 equivalent
		 * (si il y'a tous les chunks autour de la colonne). On accede alors
		 * a ce vertice equivalent.
		 */
		if (onColumnEdge) {
			KX_ChunkNode *node = m_jointNode[columnType][onColumnFront ? 0 : 5];
			if (node && node->GetChunk())
				GetCoorespondingVertexesFromChunk(node, vertex, columnType, coVertexes + 3, NULL);
		}

		if (quadVertexes[0] && quadVertexes[1] && quadVertexes[2] && quadVertexes[3]) {
			vertex->validNormal = true;

			for (unsigned short i = 0; i < 4; ++i) {
				quad[i][0] = quadVertexes[i]->vertexInfo->pos[0];
				quad[i][1] = quadVertexes[i]->vertexInfo->pos[1];
				quad[i][2] = quadVertexes[i]->vertexInfo->height;
			}

			if (reverse)
				normal_quad_v3(vertex->normal, quad[0], quad[1], quad[2], quad[3]);
			else
				normal_quad_v3(vertex->normal, quad[3], quad[2], quad[1], quad[0]);

			for (unsigned short i = 0; i < 4; ++i) {
				Vertex *coVertex = coVertexes[i];
				if (!coVertex)
					continue;

				coVertex->normal[0] = vertex->normal[0];
				coVertex->normal[1] = vertex->normal[1];
				coVertex->normal[2] = vertex->normal[2];
				coVertex->validNormal = true;
			}
		}
		else {
			vertex->normal[0] = 0.0f;
			vertex->normal[1] = 0.0f;
			vertex->normal[2] = 1.0f;
		}
	}
}

void KX_Chunk::UpdateColumnVertexesNormal(COLUMN_TYPE columnType)
{
	if (!m_meshObj || m_onConstruct) {
		return;
	}

	for (unsigned short vertexIndex = 0; vertexIndex < VERTEX_COUNT; ++vertexIndex) {
		Vertex *vertex = m_columns[columnType]->GetExternVertex(vertexIndex);
		if (!vertex->valid)
			continue;

		RAS_TexVert *rasvert = m_meshObj->GetVertex(0, vertex->vertIndex);
		rasvert->SetNormal(MT_Vector3(vertex->normal));
	}
	m_meshObj->SetMeshModified(true);
}

void KX_Chunk::ComputeJointVertexesNormal()
{
	KX_Terrain* terrain = m_node->GetTerrain();

	const KX_ChunkNode::Point2D& nodepos = m_node->GetRelativePos();
	const float width = m_node->GetRelativeSize();
	// La largeur du noeud parent.
	const float relativewidth = width / 2.0f;

	for (unsigned short i = 0; i < 6; ++i) {
		// De la gauche vers la droite.
		{
			const float y = nodepos.y - relativewidth + (width / 4.0f * (i - 1)) + width / 8.0f;
			{
				const float x = nodepos.x - (relativewidth + 1);
				m_jointNode[COLUMN_LEFT][i] = terrain->GetNodeRelativePosition(x, y);
			}

			{
				const float x = nodepos.x + (relativewidth + 1);
				m_jointNode[COLUMN_RIGHT][i] = terrain->GetNodeRelativePosition(x, y);
			}
		}

		// De bas en haut.
		{
			const float x = nodepos.x - relativewidth + (width / 4.0f * (i - 1)) + width / 8.0f;
			{
				const float y = nodepos.y - (relativewidth + 1);
				m_jointNode[COLUMN_FRONT][i] = terrain->GetNodeRelativePosition(x, y);
			}
			{
				const float y = nodepos.y + (relativewidth + 1);
				m_jointNode[COLUMN_BACK][i] = terrain->GetNodeRelativePosition(x, y);
			}
		}
	}

	ComputeColumnJointVertexNormal(COLUMN_LEFT, false);
	ComputeColumnJointVertexNormal(COLUMN_RIGHT, true);
	ComputeColumnJointVertexNormal(COLUMN_FRONT, true);
	ComputeColumnJointVertexNormal(COLUMN_BACK, false);

	for (unsigned short columnIndex = COLUMN_LEFT; columnIndex <= COLUMN_BACK; ++columnIndex) {
		for (unsigned short nodeIndex = 0; nodeIndex < 6; ++nodeIndex) {
			KX_ChunkNode *node = m_jointNode[columnIndex][nodeIndex];
			if (node && node->GetChunk()) {
				node->GetChunk()->UpdateColumnVertexesNormal(OppositeColumn((COLUMN_TYPE)columnIndex));
			}
		}
	}
}
#endif

void KX_Chunk::InvalidateJointVertexesAndIndexes()
{
	for (unsigned short vertexIndex = 0; vertexIndex < VERTEX_COUNT; ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_LEFT; columnIndex <= COLUMN_RIGHT; ++columnIndex) {
			Vertex* vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);
			if (m_lastHasJoint[columnIndex] && vertexIndex % (m_lastHasJoint[columnIndex] * 2))
				vertex->Invalidate();
			else
				vertex->Validate();
			vertex->vertIndex = -1;
			vertex->validNormal = false;
		}
	}

	for (unsigned short vertexIndex = 1; vertexIndex < (VERTEX_COUNT - 1); ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_FRONT; columnIndex <= COLUMN_BACK; ++columnIndex) {
			Vertex* vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);
			if (m_lastHasJoint[columnIndex] && vertexIndex % (m_lastHasJoint[columnIndex] * 2))
				vertex->Invalidate();
			else
				vertex->Validate();
			vertex->vertIndex = -1;
			vertex->validNormal = false;
		}
	}

	for(unsigned short columnIndex = 0; columnIndex < VERTEX_COUNT_INTERN; ++columnIndex) {
		for(unsigned short vertexIndex = 0; vertexIndex < VERTEX_COUNT_INTERN; ++vertexIndex) {
			Vertex *vertex = m_center[columnIndex][vertexIndex];
			vertex->vertIndex = -1;
			vertex->validNormal = false;
		}
	}
}

void KX_Chunk::ConstructPolygones()
{
	// on redimensione la liste des vertice partagés
	m_meshObj->m_sharedvertex_map.resize(m_originVertexIndex);

	// puis on construit les polygones pour le centre et les jointures
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

		// Si le 4 ème vertice est invalide on passe au suivant.
		if (!fourthVertex->valid)
			fourthVertex = column->GetNextValidExternVertex(vertexIndex);

		/* Dans le cas d'un début de colonne (cas 1) on ne construit
		 * que le premier triangle et on déplace les vertices 2 et 3.
		 */
		if (vertexIndex == 0) {
			secondVertex = column->GetInternVertex(0);
			thirdVertex = fourthVertex;
			secondTriangle = false;
		}
		/* Dans le cas contraire d'une fin de colonne (cas 2) on ne
		 * construit que le premier triangle si il n'y a pas de jointure.
		 */
		else if (vertexIndex == (POLY_COUNT - 1)) {
			// si il y a un joint on ne fait rien
			if (!firstVertex->valid)
				firstTriangle = false;

			secondVertex = column->GetInternVertex(vertexIndex - 1);
			thirdVertex = column->GetExternVertex(POLY_COUNT);
			secondTriangle = false;
		}
		/* Et enfine dans le cas normale d'un milieu de colonne (cas 3)
		 * on desactive le deuxième triangle que si il y a une jointure.
		 */
		else {
			secondVertex = column->GetInternVertex(vertexIndex - 1);
			thirdVertex = column->GetInternVertex(vertexIndex);
			if (!firstVertex->valid) {
				firstVertex = fourthVertex;
				secondTriangle = false;
			}
		}

		// création du premier triangle
		if (firstTriangle)
			AddMeshPolygonVertexes(firstVertex, secondVertex, thirdVertex, reverse);

		// création du deuxieme triangle
		if (secondTriangle)
			AddMeshPolygonVertexes(firstVertex, thirdVertex, fourthVertex, reverse);
	}
}

/** Cette fonction trouve les niveaux de jointures et demande la reconstruction du mesh
 * si besoin.
 */
void KX_Chunk::UpdateMesh()
{
	KX_Terrain* terrain = m_node->GetTerrain();

	const KX_ChunkNode::Point2D& nodepos = m_node->GetRelativePos();
	// La largeur du noeud parent + 1 pour être sûre de tomber sur le noeud adjacent et pas lui même.
	const unsigned short relativewidth = m_node->GetRelativeSize() / 2 + 1;

	// Les 4 noeuds autour du chunk.
	KX_ChunkNode *m_chunkNodeLeft = terrain->GetNodeRelativePosition(nodepos.x - relativewidth, nodepos.y);
	KX_ChunkNode *m_chunkNodeRight = terrain->GetNodeRelativePosition(nodepos.x + relativewidth, nodepos.y);
	KX_ChunkNode *m_chunkNodeFront = terrain->GetNodeRelativePosition(nodepos.x, nodepos.y - relativewidth);
	KX_ChunkNode *m_chunkNodeBack = terrain->GetNodeRelativePosition(nodepos.x, nodepos.y + relativewidth);

	/* Ces 4 variables nous indique si il y a besion d'une jointure et le niveau de la 
	 * jointure : 0 = pas de jointure, 1 = jointure un vertice sur 2, 2 = jointure
	 * de tous les vertices externes.
	 */
	const unsigned short hasJointLeft = m_chunkNodeLeft ? (m_node->GetLevel() - m_chunkNodeLeft->GetLevel()) : 0;
	const unsigned short hasJointRight = m_chunkNodeRight ? (m_node->GetLevel() - m_chunkNodeRight->GetLevel()) : 0;
	const unsigned short hasJointFront = m_chunkNodeFront ? (m_node->GetLevel() - m_chunkNodeFront->GetLevel()) : 0;
	const unsigned short hasJointBack = m_chunkNodeBack ? (m_node->GetLevel() - m_chunkNodeBack->GetLevel()) : 0;

	if (m_lastHasJoint[COLUMN_LEFT] != hasJointLeft ||
		m_lastHasJoint[COLUMN_RIGHT] != hasJointRight ||
		m_lastHasJoint[COLUMN_FRONT] != hasJointFront ||
		m_lastHasJoint[COLUMN_BACK] != hasJointBack ||
		!m_meshObj)
	{
		m_lastHasJoint[COLUMN_LEFT] = hasJointLeft;
		m_lastHasJoint[COLUMN_RIGHT] = hasJointRight;
		m_lastHasJoint[COLUMN_FRONT] = hasJointFront;
		m_lastHasJoint[COLUMN_BACK] = hasJointBack;

		m_onConstruct = true;
		meshRecreation++;

		// Si les vertices ne sont pas déjà créés on les créés.
		if (!m_hasVertexes) {
			ConstructVertexes();
			m_hasVertexes = true;
		}

		/* On valide ou invalide les vertices externes pour pouvoir
		 * après créer les jointures.
		 * On remet aussi à default les indices des vertices.
		 */
		InvalidateJointVertexesAndIndexes();
	}
	m_node->CheckBoxHeight(m_maxVertexHeight, m_minVertexHeight);
}

void KX_Chunk::EndUpdateMesh()
{
#ifdef STATS
	double starttime;
	double endtime;
#endif

	if (m_onConstruct) {
		// Recreation du mesh.
		DestructMesh();
		ConstructMesh();

		// Calcule des normales.
		ComputeJointVertexesNormal();

		// Construction des polygones.
		ConstructPolygones();

		// Et enfin on créer un mesh de rendu pour cet objet.
		m_meshObj->AddMeshUser(this, &m_meshSlots, NULL);

		/** On initialize toutes les informations pour le rendu :
		 * matrice de rotation, couleur, visibilité, et client.
		 */
		SG_QList::iterator<RAS_MeshSlot> mit(m_meshSlots);
		for (mit.begin(); !mit.end(); ++mit) {
			RAS_MeshSlot *ms = *mit;
			ms->m_bObjectColor = false;
			ms->m_RGBAcolor = MT_Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			ms->m_bVisible = true;
			ms->m_bCulled = false;
			ms->m_OpenGLMatrix = m_meshMatrix;
			/* Vraiment provisoire, si le client et a NULL alors
			* on ne fait pas de cast vers un KX_Gameobject et
			* ainsi on bloque certaine fonctionalités comme les
			* billboards, la taille négative...
			* TODO : Un flag pour le type du client.
			*/
			ms->m_clientObj = NULL;
		}

#ifdef STATS
		starttime = KX_GetActiveEngine()->GetRealTime();
#endif

		// On créer la forme physique du chunk.
		ConstructPhysicsController();

#ifdef STATS
		endtime = KX_GetActiveEngine()->GetRealTime();
		physicsCreatingTime += endtime - starttime;
#endif
		m_onConstruct = false;
	}
}

void KX_Chunk::RenderMesh(RAS_IRasterizer *rasty, KX_Camera *cam)
{
	const MT_Point3 realPos = MT_Point3(m_node->GetRealPos().x(), m_node->GetRealPos().y(), 0.);
// 	const MT_Point3 camPos = cam->NodeGetWorldPosition();
// 	const MT_Vector3 norm = (camPos - realPos).normalized() * sqrt(m_node->GetRadius2());

	/*KX_RasterizerDrawDebugLine(realPos + MT_Point3(0.0, 0.0, m_minVertexHeight + 1.0),
							   realPos + MT_Point3(0.0, 0.0, m_maxVertexHeight - 1.0), MT_Vector3(1., 0., 0.));*/

#if 0
	for (unsigned short columnIndex = COLUMN_LEFT; columnIndex <= COLUMN_BACK; ++columnIndex) {
		for (unsigned short vertexIndex = (columnIndex == COLUMN_LEFT || columnIndex == COLUMN_RIGHT) ? 0 : 1;
			 vertexIndex < ((columnIndex == COLUMN_LEFT || columnIndex == COLUMN_RIGHT) ? VERTEX_COUNT : (VERTEX_COUNT - 1));
			 ++vertexIndex)
		{
			Vertex *vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);
			if (!vertex->validNormal)
				continue;

// 			if (m_node->GetRelativeSize() != 4 /*|| vertexIndex != 2*/)
// 				continue;

			MT_Point3 vertPos(vertex->absolutePos[0], vertex->absolutePos[1], vertex->vertexInfo->height);
			vertPos += realPos;
			KX_RasterizerDrawDebugLine(vertPos, vertPos + (MT_Point3(vertex->normal) * 20), MT_Vector3(1., 0., 1.));

			RAS_TexVert *rasvert = m_meshObj->GetVertex(0, vertex->vertIndex);
			if (rasvert)
				KX_RasterizerDrawDebugLine(vertPos + (MT_Point3(rasvert->getNormal()) * 20), vertPos + (MT_Point3(rasvert->getNormal()) * 60), MT_Vector3(1., 0., 0.));

			MT_Vector3 color(0.25f, 0.25f, 0.0f);
			color *= m_node->GetRelativeSize(); //vertexIndex + 1;
			KX_RasterizerDrawDebugLine(MT_Point3(vertex->quad[0]), MT_Point3(vertex->quad[1]), color);
			KX_RasterizerDrawDebugLine(MT_Point3(vertex->quad[1]), MT_Point3(vertex->quad[2]), color);
			KX_RasterizerDrawDebugLine(MT_Point3(vertex->quad[2]), MT_Point3(vertex->quad[3]), color);
			KX_RasterizerDrawDebugLine(MT_Point3(vertex->quad[3]), MT_Point3(vertex->quad[0]), color);
		}
	}
#endif

	if (!m_visible)
		return;

	SG_QList::iterator<RAS_MeshSlot> mit(m_meshSlots);
	for (mit.begin(); !mit.end(); ++mit) {
		RAS_MeshSlot *ms = *mit;
		ms->m_bucket->ActivateMesh(ms);
	}
}
