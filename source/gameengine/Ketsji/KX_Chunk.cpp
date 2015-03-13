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

//#define STATS

unsigned int KX_Chunk::m_chunkActive = 0;

static KX_Chunk::COLUMN_TYPE oppositeColumn(unsigned short column)
{
	if (column == KX_Chunk::COLUMN_LEFT)
		return KX_Chunk::COLUMN_RIGHT;
	if (column == KX_Chunk::COLUMN_RIGHT)
		return KX_Chunk::COLUMN_LEFT;
	if (column == KX_Chunk::COLUMN_FRONT)
		return KX_Chunk::COLUMN_BACK;
	if (column == KX_Chunk::COLUMN_BACK)
		return KX_Chunk::COLUMN_FRONT;
	return KX_Chunk::COLUMN_NONE;
}

struct KX_Chunk::Vertex // vertex temporaire
{
	float xyz[3];
	unsigned int origIndex;
	bool valid;
	unsigned short m_numnormales;
	float m_normales[8][3];
	MT_Vector3 m_finalNormal;

	Vertex(float x, float y, float z, unsigned int origindex)
		:origIndex(origindex),
		valid(true),
		m_numnormales(0)
	{
		xyz[0] = x;
		xyz[1] = y;
		xyz[2] = z;
	}

	void Invalidate()
	{
		valid = false;
	}

	void Validate()
	{
		valid = true;
	}

	void ResetNormal()
	{
		m_numnormales = 0;
	}

	void AddNormal(float *normal)
	{
		copy_v3_v3(m_normales[m_numnormales++], normal);
	}

	MT_Vector3 GetFinalNormal()
	{
		return m_finalNormal;
	}
	
	void ComputeNormal()
	{
		float normal[3] = {0., 0., 0.};
		for (unsigned short i = 0; i < m_numnormales; ++i)
			add_v3_v3(normal, m_normales[i]);
		m_finalNormal = MT_Vector3(normal);
	}
};

std::ostream& operator<<(std::ostream& os, const KX_Chunk::Vertex& vert)
{
	return os << "Vertex((" << &vert << ") valid = " << vert.valid << ", pos = (x : " << vert.xyz[0] << ", y : " << vert.xyz[1] << ", z : " << vert.xyz[2] << "), origindex = " << vert.origIndex << ")";
}

struct KX_Chunk::JointColumn
{
	Vertex *m_columnExterne[POLY_COUNT + 1];
	Vertex *m_columnInterne[POLY_COUNT - 1];
	// si vrai alors on supprime les vertices aux extrémité
	bool m_freeEndVertex;

	JointColumn(bool freeEndVertex)
		:m_freeEndVertex(freeEndVertex)
	{
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
};

KX_Chunk::KX_Chunk(void *sgReplicationInfo, SG_Callbacks callbacks, KX_ChunkNode *node, RAS_MaterialBucket *bucket)
	:KX_GameObject(sgReplicationInfo, callbacks),
	m_node(node),
	m_bucket(bucket),
	m_meshObj(NULL),
	m_hasVertexes(false),
	m_needNormalCompute(false)
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

	if (m_meshObj)
	{
		RAS_MeshMaterial* meshmat = m_meshObj->GetMeshMaterial((unsigned int)0);
		meshmat->m_bucket->RemoveMesh(meshmat->m_baseslot);
		delete m_meshObj;
	}
}

void KX_Chunk::ReconstructMesh()
{
	DestructMesh();
	ConstructMesh();

	if (!m_hasVertexes) {
		ConstructVertexes();
		m_hasVertexes = true;
	}

	InvalidateJointVertexes();
	ConstructPolygones();
	ConstructVertexesNormal();

	AddMeshUser();

	m_meshObj->SchedulePolygons(0);

	m_needNormalCompute = true;
// 	m_pPhysicsController->ReinstancePhysicsShape(NULL, m_meshObj);
}

// Alexis, tu mettra ta fonction magique ici.
float KX_Chunk::GetZVertex(float vertx, float verty) const
{
	KX_Terrain *terrain = m_node->GetTerrain();

	const float maxheight = terrain->GetMaxHeight();
	const MT_Point2 realPos = m_node->GetRealPos();

	const float x = vertx + realPos.x();
	const float y = verty + realPos.y();

	const float z = BLI_hnoise(50., x, y, 0.) * maxheight;
	return z;
}

void KX_Chunk::AddMeshPolygonVertexes(Vertex *v1, Vertex *v2, Vertex *v3, bool reverse)
{
	RAS_Polygon* poly = m_meshObj->AddPolygon(m_bucket, 3);
	const MT_Point2& realPos = m_node->GetRealPos();

	poly->SetVisible(true);
	poly->SetCollider(true);
	poly->SetTwoside(true);

	MT_Point2 uvs_1[8];
	MT_Point2 uvs_2[8];
	MT_Point2 uvs_3[8];

	for (unsigned short i = 0; i < 8; ++i)
	{
		uvs_1[i] = MT_Point2(v1->xyz) + realPos;
		uvs_2[i] = MT_Point2(v2->xyz) + realPos;
		uvs_3[i] = MT_Point2(v3->xyz) + realPos;
	}

	const MT_Vector4 tangent(0., 0., 0., 0.);
	// normale temporaire pour eviter la duplication des vertices lors de leur ajout
	const MT_Vector3 tempnormal(0., 0., 1.);

	if (reverse)
	{
		float fnormal[3] = {0., 0., 1.};
		normal_tri_v3(fnormal, v3->xyz, v2->xyz, v1->xyz);
		v1->AddNormal(fnormal);
		v2->AddNormal(fnormal);
		v3->AddNormal(fnormal);

		m_meshObj->AddVertex(poly, 0, MT_Point3(v3->xyz), uvs_3, tangent, 255, tempnormal, false, v3->origIndex);
		m_meshObj->AddVertex(poly, 1, MT_Point3(v2->xyz), uvs_2, tangent, 255, tempnormal, false, v2->origIndex);
		m_meshObj->AddVertex(poly, 2, MT_Point3(v1->xyz), uvs_1, tangent, 255, tempnormal, false, v1->origIndex);
	}
	else
	{
		float fnormal[3] = {0., 0., 1.};
		normal_tri_v3(fnormal, v1->xyz, v2->xyz, v3->xyz);
		v1->AddNormal(fnormal);
		v2->AddNormal(fnormal);
		v3->AddNormal(fnormal);

		m_meshObj->AddVertex(poly, 0, MT_Point3(v1->xyz), uvs_1, tangent, 255, tempnormal, false, v1->origIndex);
		m_meshObj->AddVertex(poly, 1, MT_Point3(v2->xyz), uvs_2, tangent, 255, tempnormal, false, v2->origIndex);
		m_meshObj->AddVertex(poly, 2, MT_Point3(v3->xyz), uvs_3, tangent, 255, tempnormal, false, v3->origIndex);
	}
}

void KX_Chunk::ConstructVertexes()
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
	const unsigned short vertexCountInterne = POLY_COUNT - 1;
	// le nombre de poly en largeur à l'exterieur
	const unsigned short vertexCountExterne = POLY_COUNT + 1;

	//Calcul de l'intervalle entre deux points
	const float interval = size * relativesize / POLY_COUNT;
	// la motie de la largeur du chunk
	const float width = size / 2 * relativesize;
	// la motie de la largeur du chunk - interval car on ne fait pas les bords
	const float widthcenter = width - interval;

	const float range[] = {-width, width, -width, width};
	const float startx = -width;
	const float endx = width;
	const float starty = -width;
	const float endy = width;

#ifdef STATS
	double starttimevertex = KX_GetActiveEngine()->GetRealTime();
#endif

	m_originVertexIndex = 0;

	for (unsigned short i = 0; i < 4; ++i)
		m_columns[i] = new JointColumn((i < 2));

	// on construit tous les vertices et on stocke leur numero dans des colonnes
	for(unsigned short columnIndex = 0; columnIndex < vertexCountInterne; ++columnIndex) {
		const float x = interval * columnIndex - widthcenter;
		for(unsigned short vertexIndex = 0; vertexIndex < vertexCountInterne; ++vertexIndex) {
			const float y = interval * vertexIndex - widthcenter;
			// on créer un vertice temporaire, ces donné seront reutilisé lors de la création des polygones
			Vertex *vertex = new Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			m_center[columnIndex][vertexIndex] = vertex;

			if (columnIndex == 0)
				m_columns[COLUMN_LEFT]->SetInternVertex(vertexIndex, vertex);
			if (columnIndex == (vertexCountInterne - 1))
				m_columns[COLUMN_RIGHT]->SetInternVertex(vertexIndex, vertex);

			if (vertexIndex == 0)
				m_columns[COLUMN_FRONT]->SetInternVertex(columnIndex, vertex);
			if (vertexIndex == (vertexCountInterne - 1))
				m_columns[COLUMN_BACK]->SetInternVertex(columnIndex, vertex);
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

			Vertex* vertex = new Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			m_columns[COLUMN_LEFT]->SetExternVertex(vertexIndex, vertex);

			if (vertexIndex == 0)
				m_columns[COLUMN_FRONT]->SetExternVertex(0, vertex);
			else if (vertexIndex == (vertexCountExterne - 1))
				m_columns[COLUMN_BACK]->SetExternVertex(0, vertex);
		}
		{ // colonne de droite extern
			const float x = endx;

			Vertex *vertex = new Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			m_columns[COLUMN_RIGHT]->SetExternVertex(vertexIndex, vertex);

			if (vertexIndex == 0)
				m_columns[COLUMN_FRONT]->SetExternVertex(vertexCountExterne - 1, vertex);
			else if (vertexIndex == (vertexCountExterne - 1))
				m_columns[COLUMN_BACK]->SetExternVertex(vertexCountExterne - 1, vertex);
		}
	}

	for (unsigned short vertexIndex = 1; vertexIndex < (vertexCountExterne - 1); ++vertexIndex) {
		const float x = startx + interval * vertexIndex;
		for (unsigned short columnIndex = COLUMN_FRONT; columnIndex <= COLUMN_BACK; ++columnIndex) {
			const float y = range[columnIndex];

			Vertex *vertex = new Vertex(x, y, GetZVertex(x, y), m_originVertexIndex++);
			// le premier vertice de la colonne est déjà partagé donc on remplie ceux après
			m_columns[columnIndex]->SetExternVertex(vertexIndex, vertex);
		}
	}

#ifdef STATS
	double endtime = KX_GetActiveEngine()->GetRealTime();
	DEBUG(__func__ << " spend " << endtime - starttime << " time");
#endif
}

void KX_Chunk::InvalidateJointVertexes()
{
	const unsigned short vertexCountInterne = POLY_COUNT - 1;
	// le nombre de poly en largeur à l'exterieur
	const unsigned short vertexCountExterne = POLY_COUNT + 1;

	for (unsigned short i = 0; i < vertexCountInterne; ++i) {
			for (unsigned short j = 0; j < vertexCountInterne; ++j) {
				m_center[i][j]->ResetNormal();
			}
		}

	for (unsigned short vertexIndex = 0; vertexIndex < vertexCountExterne; ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_LEFT; columnIndex <= COLUMN_RIGHT; ++columnIndex) {
			Vertex* vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);
			vertex->ResetNormal();
			if (vertexIndex % 2 && m_lastHasJoint[columnIndex])
				vertex->Invalidate();
			else
				vertex->Validate();
		}
	}

	for (unsigned short vertexIndex = 1; vertexIndex < (vertexCountExterne - 1); ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_FRONT; columnIndex <= COLUMN_BACK; ++columnIndex) {
			Vertex* vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);
			vertex->ResetNormal();
			if (vertexIndex % 2 && m_lastHasJoint[columnIndex])
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
	for (unsigned short vertexIndex = 0; vertexIndex < POLY_COUNT; ++vertexIndex) {
		bool firstTriangle = true;
		bool secondTriangle = true;

		Vertex *firstVertex, *secondVertex, *thirdVertex, *fourthVertex;

		// les seuls points fixes
		firstVertex = column->GetExternVertex(vertexIndex);
		fourthVertex = column->GetExternVertex(vertexIndex + 1);

		if (!fourthVertex->valid) {
			fourthVertex = column->GetExternVertex(vertexIndex + 2);
		}

		// debut de la colonne
		if (vertexIndex == 0) {
			/*
			 * 1 
			 * |\
			 * | \
			 * 3--2
			 */

			secondVertex = column->GetInternVertex(0);
			thirdVertex = fourthVertex;
			secondTriangle = false;
		}
		// fin de la colonne
		else if (vertexIndex == (POLY_COUNT - 1)) {
			/*
			 * 1--2
			 * | /
			 * |/
			 * 3
			 */

			if (!firstVertex->valid) {
				firstTriangle = false;
			}
			secondVertex = column->GetInternVertex(vertexIndex - 1);
			thirdVertex = column->GetExternVertex(POLY_COUNT);
			secondTriangle = false;
		}
		// miliue de la colonne
		else {
			/*
			 * 1--2
			 * |\ |
			 * | \|
			 * 4--3
			 */
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

void KX_Chunk::ConstructVertexesNormal()
{
	/* On calcule la moyenne de toutes les normales pour avoir un smoth, 
	 * mais les vertices sur les bords ont une fausse normale, pour cela il faut accéder
	 * aux vertices des chunks adjacents mais les chunks adjacent non pas forcement
	 * reconstruit leur vertices avant, il faut donc recalculer les normales pour les vertices
	 * exterieur après dans EndUpdateMesh
	 */
	// le nombre de poly en largeur à l'interieur
	const unsigned short vertexCountInterne = POLY_COUNT - 1;
	// le nombre de poly en largeur à l'exterieur
	const unsigned short vertexCountExterne = POLY_COUNT + 1;

	// on lit toutes les colonnes pour ajuster les normales
	for (unsigned int columnIndex = 0; columnIndex < vertexCountInterne; ++columnIndex) {
		for (unsigned int vertexIndex = 0; vertexIndex < vertexCountInterne; ++vertexIndex) {
			Vertex *vertex = m_center[columnIndex][vertexIndex];
			vertex->ComputeNormal();

			RAS_MeshObject::SharedVertex& sharedvertex = m_meshObj->m_sharedvertex_map[vertex->origIndex][0];
			sharedvertex.m_darray->m_vertex[sharedvertex.m_offset].SetNormal(vertex->GetFinalNormal());
		}
	}

	for (unsigned int vertexIndex = 0; vertexIndex < vertexCountExterne; ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_LEFT; columnIndex <= COLUMN_RIGHT; ++columnIndex) { 
			Vertex *vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);

			if (vertex->valid) {
				vertex->ComputeNormal();
			}
		}
	}

	for (unsigned int vertexIndex = 0; vertexIndex < vertexCountInterne; ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_FRONT; columnIndex <= COLUMN_BACK; ++columnIndex) {
			Vertex *vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex + 1);

			if (vertex->valid) {
				vertex->ComputeNormal();
			}
		}
	}
}

void KX_Chunk::ConstructExternVertexesNormal()
{
	if (!m_needNormalCompute)
		return;
	m_needNormalCompute = false;

	KX_Terrain* terrain = m_node->GetTerrain();

	const KX_ChunkNode::Point2D& pos = m_node->GetRelativePos();
	unsigned short relativewidth = m_node->GetRelativeSize() / 2 + 1;
	// le nombre de poly en largeur à l'interieur
	const unsigned short vertexCountInterne = POLY_COUNT - 1;
	// le nombre de poly en largeur à l'exterieur
	const unsigned short vertexCountExterne = POLY_COUNT + 1;

	KX_Chunk *chunks[4];
	for (unsigned short columnIndex = COLUMN_LEFT; columnIndex <= COLUMN_BACK; ++columnIndex)
		chunks[columnIndex] = m_lastChunkNode[columnIndex] ? m_lastChunkNode[columnIndex]->GetChunk() : NULL;


	for (unsigned int vertexIndex = 1; vertexIndex < (vertexCountExterne - 1); ++vertexIndex) {
		for (unsigned short columnIndex = COLUMN_LEFT; columnIndex <= COLUMN_BACK; ++columnIndex) {
			KX_Chunk* chunk = chunks[columnIndex];
			if (chunk && !m_lastHasJoint[columnIndex]) { // le noeud est invisible ?
				Vertex *vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);

				if (vertex->valid) {
					Vertex *otherVertex = chunk->GetColumnExternVertex(oppositeColumn(columnIndex), vertexIndex);
					MT_Vector3 normal = otherVertex->GetFinalNormal() + vertex->GetFinalNormal();

					RAS_MeshObject::SharedVertex& sharedvertex = m_meshObj->m_sharedvertex_map[vertex->origIndex][0];
					sharedvertex.m_darray->m_vertex[sharedvertex.m_offset].SetNormal(normal);
				}
			}
		}
	}

	KX_ChunkNode *cornerChunkNode[4];
	cornerChunkNode[0] = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x - relativewidth,
																						  pos.y - relativewidth));
	cornerChunkNode[1] = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x - relativewidth,
																						   pos.y + relativewidth));
	cornerChunkNode[2] = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x + relativewidth,
																						   pos.y - relativewidth));
	cornerChunkNode[3] = terrain->GetNodeRelativePosition(KX_ChunkNode::Point2D(pos.x + relativewidth, 
																						  pos.y + relativewidth));

	KX_Chunk *cornerChunks[4];
	for (unsigned short i = 0; i < 4; ++i)
		cornerChunks[i] = cornerChunkNode[i] ? cornerChunkNode[i]->GetChunk() : NULL;

	if (!m_lastHasJoint[COLUMN_LEFT]) {
		if (cornerChunks[0]) {
			Vertex *vertex = m_columns[COLUMN_LEFT]->GetExternVertex(0);
			if (vertex->valid) {
				Vertex *otherVertex = cornerChunks[0]->GetColumnExternVertex(COLUMN_RIGHT, POLY_COUNT);
				MT_Vector3 normal = otherVertex->GetFinalNormal() + vertex->GetFinalNormal();

				RAS_MeshObject::SharedVertex& sharedvertex = m_meshObj->m_sharedvertex_map[vertex->origIndex][0];
				sharedvertex.m_darray->m_vertex[sharedvertex.m_offset].SetNormal(normal);
			}
		}
		if (cornerChunks[1]) {
			Vertex *vertex = m_columns[COLUMN_LEFT]->GetExternVertex(POLY_COUNT);
			if (vertex->valid) {
				Vertex *otherVertex = cornerChunks[1]->GetColumnExternVertex(COLUMN_RIGHT, 0);
				MT_Vector3 normal = otherVertex->GetFinalNormal() + vertex->GetFinalNormal();

				RAS_MeshObject::SharedVertex& sharedvertex = m_meshObj->m_sharedvertex_map[vertex->origIndex][0];
				sharedvertex.m_darray->m_vertex[sharedvertex.m_offset].SetNormal(normal);
			}
		}
	}
	if (!m_lastHasJoint[COLUMN_RIGHT]) {
		if (cornerChunks[2]) {
			Vertex *vertex = m_columns[COLUMN_RIGHT]->GetExternVertex(0);
			if (vertex->valid) {
				Vertex *otherVertex = cornerChunks[2]->GetColumnExternVertex(COLUMN_LEFT, POLY_COUNT);
				MT_Vector3 normal = otherVertex->GetFinalNormal() + vertex->GetFinalNormal();

				RAS_MeshObject::SharedVertex& sharedvertex = m_meshObj->m_sharedvertex_map[vertex->origIndex][0];
				sharedvertex.m_darray->m_vertex[sharedvertex.m_offset].SetNormal(normal);
			}
		}
		if (cornerChunks[3]) {
			Vertex *vertex = m_columns[COLUMN_RIGHT]->GetExternVertex(POLY_COUNT);
			if (vertex->valid) {
				Vertex *otherVertex = cornerChunks[3]->GetColumnExternVertex(COLUMN_LEFT, 0);
				MT_Vector3 normal = otherVertex->GetFinalNormal() + vertex->GetFinalNormal();

				RAS_MeshObject::SharedVertex& sharedvertex = m_meshObj->m_sharedvertex_map[vertex->origIndex][0];
				sharedvertex.m_darray->m_vertex[sharedvertex.m_offset].SetNormal(normal);
			}
		}
	}

	/*for (unsigned short columnIndex = 0; columnIndex < 4; ++columnIndex) {
		for (unsigned short vertexIndex = 0; vertexIndex < vertexCountExterne; vertexIndex += (vertexCountExterne - 1)) {
			DEBUG("vertexIndex : " << vertexIndex);
			KX_Chunk *chunk;
			if (vertexIndex == 0)
				chunk = cornerChunks[columnIndex];
			else
				chunk = cornerChunks[columnIndex + 1];

			if (chunk && !m_lastHasJoint[columnIndex]) { // le noeud est invisible ?
				Vertex *vertex = m_columns[columnIndex]->GetExternVertex(vertexIndex);

				if (vertex->valid) {
					Vertex *otherVertex = chunk->GetColumnExternVertex(oppositeColumn(columnIndex),
																	   (vertexCountExterne - 1) - vertexIndex);
					MT_Vector3 normal = otherVertex->GetFinalNormal() + vertex->GetFinalNormal();

					RAS_MeshObject::SharedVertex& sharedvertex = m_meshObj->m_sharedvertex_map[vertex->origIndex][0];
					sharedvertex.m_darray->m_vertex[sharedvertex.m_offset].SetNormal(normal);
				}
			}
		}
	}*/
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
	const bool hasJointLeft = m_lastChunkNode[COLUMN_LEFT] ? m_lastChunkNode[COLUMN_LEFT]->GetLevel() < m_node->GetLevel() : false;
	const bool hasJointRight = m_lastChunkNode[COLUMN_RIGHT] ? m_lastChunkNode[COLUMN_RIGHT]->GetLevel() < m_node->GetLevel() : false;
	const bool hasJointFront = m_lastChunkNode[COLUMN_FRONT] ? m_lastChunkNode[COLUMN_FRONT]->GetLevel() < m_node->GetLevel() : false;
	const bool hasJointBack = m_lastChunkNode[COLUMN_BACK] ? m_lastChunkNode[COLUMN_BACK]->GetLevel() < m_node->GetLevel() : false;

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
}

void KX_Chunk::EndUpdateMesh()
{
	ConstructExternVertexesNormal();
}

void KX_Chunk::RenderMesh(RAS_IRasterizer *rasty, KX_Camera *cam)
{
	const MT_Point3 realPos = MT_Point3(m_node->GetRealPos().x(), m_node->GetRealPos().y(), 0.);
	const MT_Point3 camPos = cam->NodeGetWorldPosition();
	const MT_Vector3 norm = (camPos - realPos).normalized() * sqrt(m_node->GetRadius2());

	KX_RasterizerDrawDebugLine(realPos, realPos + norm, MT_Vector3(1., 0., 0.));
}

KX_Chunk::Vertex *KX_Chunk::GetColumnExternVertex(COLUMN_TYPE column, unsigned short index)
{
	return m_columns[column]->GetExternVertex(index);
}