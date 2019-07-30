#include "scene.h"

#include "bgfx_utils.h"
#include "model.h"
#include "light.h"
#include "../../bgfx/src/renderer.h"
#include "camera.h"
#include "imgui/imgui.h"

struct PosNormalTangentTexcoordVertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_normal;
	uint32_t m_tangent;
	int16_t m_u;
	int16_t m_v;

	static void init()
	{
		ms_decl
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true)
			.add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Uint8, true, true)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true)
			.end();
	}

	static bgfx::VertexDecl ms_decl;
};

bgfx::VertexDecl PosNormalTangentTexcoordVertex::ms_decl;

static PosNormalTangentTexcoordVertex s_cubeVertices[24] =
{
	{-1.0f, 1.0f, 1.0f, encodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0, 0},
	{1.0f, 1.0f, 1.0f, encodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0x7fff, 0},
	{-1.0f, -1.0f, 1.0f, encodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0, 0x7fff},
	{1.0f, -1.0f, 1.0f, encodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0x7fff, 0x7fff},
	{-1.0f, 1.0f, -1.0f, encodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0, 0},
	{1.0f, 1.0f, -1.0f, encodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0x7fff, 0},
	{-1.0f, -1.0f, -1.0f, encodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0, 0x7fff},
	{1.0f, -1.0f, -1.0f, encodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0x7fff, 0x7fff},
	{-1.0f, 1.0f, 1.0f, encodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0, 0},
	{1.0f, 1.0f, 1.0f, encodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0x7fff, 0},
	{-1.0f, 1.0f, -1.0f, encodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0, 0x7fff},
	{1.0f, 1.0f, -1.0f, encodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0x7fff, 0x7fff},
	{-1.0f, -1.0f, 1.0f, encodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0, 0},
	{1.0f, -1.0f, 1.0f, encodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0x7fff, 0},
	{-1.0f, -1.0f, -1.0f, encodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0, 0x7fff},
	{1.0f, -1.0f, -1.0f, encodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0x7fff, 0x7fff},
	{1.0f, -1.0f, 1.0f, encodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0, 0},
	{1.0f, 1.0f, 1.0f, encodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0x7fff, 0},
	{1.0f, -1.0f, -1.0f, encodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0, 0x7fff},
	{1.0f, 1.0f, -1.0f, encodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0x7fff, 0x7fff},
	{-1.0f, -1.0f, 1.0f, encodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0, 0},
	{-1.0f, 1.0f, 1.0f, encodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0x7fff, 0},
	{-1.0f, -1.0f, -1.0f, encodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0, 0x7fff},
	{-1.0f, 1.0f, -1.0f, encodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0x7fff, 0x7fff},
};

static const uint16_t s_cubeIndices[36] =
{
	0, 2, 1,
	1, 2, 3,
	4, 5, 6,
	5, 7, 6,

	8, 10, 9,
	9, 10, 11,
	12, 13, 14,
	13, 15, 14,

	16, 18, 17,
	17, 18, 19,
	20, 21, 22,
	21, 23, 22,
};

Scene::Scene()
{
	float startX = 0;
	float startY = 0;
	for (int x = 0; x < 10; ++x)
	{
		for (int y = 0; y < 10; ++y)
		{
			Model* newModel = new Model;
			//newModel->mesh = meshLoad("meshes/bunny.bin");
			newModel->pos.x = x * 4 + startX;
			newModel->pos.y = y * 4 + startY;
			newModel->pos.z = 0;
			newModel->rot.x = newModel->rot.y = newModel->rot.z = 0;
			newModel->scale.x = newModel->scale.y = newModel->scale.z = 1;
			models.push_back(newModel);
		}
	}

	direction_light = new Light;
	direction_light->lightPosRadius[0] = 1;
	direction_light->lightPosRadius[1] = 1;
	direction_light->lightPosRadius[2] = 1;
	direction_light->lightPosRadius[3] = 0;
	direction_light->color[0] = 0.0f;
	direction_light->color[1] = 0.0f;
	direction_light->color[2] = 0.0f;
	direction_light->color[3] = 1.0f;


	SetLightNum(256);

	// Create vertex stream declaration.
	PosNormalTangentTexcoordVertex::init();

	calcTangents(s_cubeVertices
	             , BX_COUNTOF(s_cubeVertices)
	             , PosNormalTangentTexcoordVertex::ms_decl
	             , s_cubeIndices
	             , BX_COUNTOF(s_cubeIndices)
	);

	// Create static vertex buffer.
	m_vbh = createVertexBuffer(
		bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices))
		, PosNormalTangentTexcoordVertex::ms_decl
	);

	// Create static index buffer.
	m_ibh = createIndexBuffer(bgfx::makeRef(s_cubeIndices, sizeof(s_cubeIndices)));
}

Scene::~Scene()
{
}

void Scene::SetLightNum(const int number)
{
	for (uint32_t index = 0; index < addLights.size(); ++index)
	{
		delete addLights[index];
	}

	addLights.clear();

	float startX = 0;
	float startY = 0;
	int x = 0;
	int y = 0;
	for (int index = 0; index < number; ++index)
	{
		Light* newLight = new Light;

		uint8_t val = index & 7;
		newLight->color[0] = val & 0x1 ? 1.0f : 0.25f;
		newLight->color[1] = val & 0x2 ? 1.0f : 0.25f;
		newLight->color[2] = val & 0x4 ? 1.0f : 0.25f;
		newLight->color[3] = 0.8f;


		newLight->lightPosRadius[0] = x * 4 + startX;
		newLight->lightPosRadius[1] = y * 4 + startY;
		newLight->lightPosRadius[2] = -2;
		newLight->lightPosRadius[3] = 3;

		addLights.push_back(newLight);
		x += 1;
		if (x > 10)
		{
			y += 1;
			x = 0;
		}
	}
}


void Scene::DrawModelAsCube(int viewIndex, uint64_t state, bgfx::ProgramHandle program, int index)
{
	Model* model = models[index];
	float mtx[16];
	bx::mtxSRT(mtx,
	           1, 1, 1,
	           model->rot.x, model->rot.y, model->rot.z,
	           model->pos.x, model->pos.y, model->pos.z
	);
	bgfx::setTransform(mtx);

	setVertexBuffer(0, m_vbh);
	setIndexBuffer(m_ibh);

	bgfx::setState(state);
	submit(viewIndex, program);
}
