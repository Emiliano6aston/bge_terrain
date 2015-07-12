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

	/** Le vertice peut être utilisé dans le mesh ?
	 * Très utile pour les jointures, lorsque le vertice est
	 * invalide on passe au suivant et ainsi créant la jointure.
	 */
	bool valid; // 1

	/// La normale du vertice.
	float normal[3]; // 4 * 3 = 12
	// 30 octets, on pourrais aligner sur 32 octets, a voir.

	Vertex(VertexZoneInfo *info,
		   short relx, short rely,
		   float absx, float absy,
		   unsigned int origindex)
		:vertexInfo(info),
		origIndex(origindex),
		valid(true)
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

#if 0
	char c[3];
	sprintf(c, "%d", m_chunkActive);
	SetName("KX_Chunk" + STR_String(c));
#endif

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

/// Creation du nouveau mesh.
void KX_Chunk::ConstructMesh()
{
	m_meshObj = new RAS_MeshObject(NULL);
	m_meshes.push_back(m_meshObj);
}

// Destruction du mesh.
void KX_Chunk::DestructMesh()
{
	RemoveMeshes();

	if (m_meshObj) {
		/* Chaque mesh est unique or le BGE garde une copie pour pouvoir le
		 * dupliquer plus tard, cette copie ce trouve dans meshmat->m_baseslot.
		 * Donc on la supprime.
		 */
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

	// Recreation du mesh.
	DestructMesh();
	ConstructMesh();

	// Si les vertices ne sont pas encore créés on les créés.
	if (!m_hasVertexes) {
		ConstructVertexes();
		m_hasVertexes = true;
	}

	/* On valide ou invalide les vertices externe pour pouvoir
	 * après créer le jointures
	 */
	InvalidateJointVertexes();
	// Construction des polygones.
	ConstructPolygones();

	// Et enfin on créer un mesh de rendu pour cet objet.
	AddMeshUser();

#ifdef STATS
	starttime = KX_GetActiveEngine()->GetRealTime();
#endif

	// Si l'objet utilise une forme physique on essai de la recréer.
	if (m_pPhysicsController)
		m_pPhysicsController->ReinstancePhysicsShape(NULL, m_meshObj, false);

#ifdef STATS
	endtime = KX_GetActiveEngine()->GetRealTime();
	physicsCreatingTime += endtime - starttime;
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

	if (vertz > m_maxVertexHeight)
		m_maxVertexHeight = vertz;

	if (vertz < m_minVertexHeight)
		m_minVertexHeight = vertz;

	Vertex *vertex = new Vertex(info, relx, rely, vertx, verty, m_originVertexIndex++);
	return vertex;
}

/* Cette fonction permet d'acceder à un vertice comme si ils étaient
 * stockés dans un tableau à deux dimension pour les x et y.
 * C'est très utile pour le calcule des normales.
 */
KX_Chunk::Vertex *KX_Chunk::GetVertex(unsigned short x, unsigned short y) const
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
 * Dans ce cas la on calcule la position d'un vertice virtuelle.
 */
void KX_Chunk::SetNormal(Vertex *vertexCenter, bool intern) const
{
	// la position du vertice donc du centre du quad
	const float x = vertexCenter->absolutePos[0];
	const float y = vertexCenter->absolutePos[1];

	// le quadrilatère
	float quad[4][3];

	/* Si le vertice n'est pas un vertice adjacent il suffit de
	 * recupérer les 4 vertice autour.
	 */
	if (intern) {
		// la position relative du vertice
		const unsigned short relx = vertexCenter->relativePos[0];
		const unsigned short rely = vertexCenter->relativePos[1];

		// on trouve les 4 vertices adjacents
		Vertex *vertexes[4] = {
			GetVertex(relx + 1, rely),
			GetVertex(relx, rely + 1),
			GetVertex(relx - 1, rely),
			GetVertex(relx, rely - 1)
		};
		// Puis pour chaque vertice on copie sa position en 3d
		for (unsigned short i = 0; i < 4; ++i) {
			quad[i][0] = vertexes[i]->absolutePos[0];
			quad[i][1] = vertexes[i]->absolutePos[1];
			quad[i][2] = vertexes[i]->vertexInfo->height;
		}
	}
	// Sinon on créer des faux vertices autour.
	else {
#if 0
		// La camera active.
		KX_Camera *cam = KX_GetActiveScene()->GetActiveCamera();
		// La distance de ce point vers la camera.
		const float distance = cam->NodeGetWorldPosition().distance(MT_Point3(x + pos.x(), y + pos.y(), 0.0f));
		// Le rayon reel du terrain.
		const float maxterrainwidth = terrain->GetChunkSize() * terrain->GetWidth();
		// L'interval entre les vertices pour le plus petit des chunk.
		const float mininterval = terrain->GetChunkSize() / POLY_COUNT;
		// L'interval entre les vertices pour le plus grand des chunk, les plus grands des chunks sont le quart du terrain.
		const float maxinterval = maxterrainwidth / POLY_COUNT / 2;
#endif

		KX_Terrain *terrain = m_node->GetTerrain();
		// la position du noeud parent du chunk
		const MT_Point2& nodepos = m_node->GetRealPos();

		// la taille du quad servant a calculer la normale
		const float smothsize = 5.0f;

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
			VertexZoneInfo *info = terrain->GetVertexInfo(quad[i][0] + nodepos.x(), quad[i][1] + nodepos.y());
			quad[i][2] = info->height;
			info->Release();
		}
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
		uvs_1[i] = MT_Point2(v1->absolutePos) + realPos;
		uvs_2[i] = MT_Point2(v2->absolutePos) + realPos;
		uvs_3[i] = MT_Point2(v3->absolutePos) + realPos;
	}

	const MT_Vector4 tangent(0.0f, 0.0f, 0.0f, 0.0f);

	if (reverse) {
		m_meshObj->AddVertex(poly, 0, MT_Point3(v3->absolutePos[0], v3->absolutePos[1], v3->vertexInfo->height),
							 uvs_3, tangent, 
							 rgb_to_cpack(v3->vertexInfo->color[0], v3->vertexInfo->color[1], v3->vertexInfo->color[2]),
							 v3->normal, false, v3->origIndex);
		m_meshObj->AddVertex(poly, 1, MT_Point3(v2->absolutePos[0], v2->absolutePos[1], v2->vertexInfo->height),
							 uvs_2, tangent, 
							 rgb_to_cpack(v2->vertexInfo->color[0], v2->vertexInfo->color[1], v2->vertexInfo->color[2]),
							 v2->normal, false, v2->origIndex);
		m_meshObj->AddVertex(poly, 2, MT_Point3(v1->absolutePos[0], v1->absolutePos[1], v1->vertexInfo->height),
							 uvs_1, tangent,
							 rgb_to_cpack(v1->vertexInfo->color[0], v1->vertexInfo->color[1], v1->vertexInfo->color[2]),
							 v1->normal, false, v1->origIndex);
	}
	else {
		m_meshObj->AddVertex(poly, 0, MT_Point3(v1->absolutePos[0], v1->absolutePos[1], v1->vertexInfo->height),
							 uvs_1, tangent,
							 rgb_to_cpack(v1->vertexInfo->color[0], v1->vertexInfo->color[1], v1->vertexInfo->color[2]),
							 v1->normal, false, v1->origIndex);
		m_meshObj->AddVertex(poly, 1, MT_Point3(v2->absolutePos[0], v2->absolutePos[1], v2->vertexInfo->height),
							 uvs_2, tangent,
							 rgb_to_cpack(v2->vertexInfo->color[0], v2->vertexInfo->color[1], v2->vertexInfo->color[2]),
							 v2->normal, false, v2->origIndex);
		m_meshObj->AddVertex(poly, 2, MT_Point3(v3->absolutePos[0], v3->absolutePos[1], v3->vertexInfo->height), 
							 uvs_3, tangent,
							 rgb_to_cpack(v3->vertexInfo->color[0], v3->vertexInfo->color[1], v3->vertexInfo->color[2]),
							 v3->normal, false, v3->origIndex);
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

	m_maxVertexHeight = 0.0f;
	m_minVertexHeight = 0.0f;

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
		for (unsigned short vertexIndex = 0; vertexIndex < VERTEX_COUNT; ++vertexIndex)
			SetNormal(m_columns[columnIndex]->GetExternVertex(vertexIndex), false);
	}

	for (unsigned short columnIndex = COLUMN_FRONT; columnIndex <= COLUMN_BACK; ++columnIndex) {
		for (unsigned short vertexIndex = 1; vertexIndex < (VERTEX_COUNT - 1); ++vertexIndex)
			SetNormal(m_columns[columnIndex]->GetExternVertex(vertexIndex), false);
	}

	for (unsigned short columnIndex = 1; columnIndex < (VERTEX_COUNT - 1); ++columnIndex) {
		for (unsigned short vertexIndex = 1; vertexIndex < (VERTEX_COUNT - 1); ++vertexIndex)
			SetNormal(m_center[columnIndex - 1][vertexIndex - 1], true);
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
	KX_ChunkNode *m_chunkNodeLeft = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(nodepos.x - relativewidth, nodepos.y));
	KX_ChunkNode *m_chunkNodeRight = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(nodepos.x + relativewidth, nodepos.y));
	KX_ChunkNode *m_chunkNodeFront = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(nodepos.x, nodepos.y - relativewidth));
	KX_ChunkNode *m_chunkNodeBack = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(nodepos.x, nodepos.y + relativewidth));

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
