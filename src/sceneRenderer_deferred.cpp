#include "sceneRenderer_deferred.h"
#include "bx/bx.h"
#include "bgfx_utils.h"
#include "imgui/imgui.h"
#include "camera.h"
#include "model.h"
#include "scene.h"
#include "light.h"


struct PosTexCoord0Vertex
{
	float m_x;
	float m_y;
	float m_z;
	float m_u;
	float m_v;

	static void init()
	{
		ms_decl
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
	}

	static bgfx::VertexDecl ms_decl;
};

bgfx::VertexDecl PosTexCoord0Vertex::ms_decl;

struct DebugVertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_abgr;

	static void init()
	{
		ms_decl
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	}

	static bgfx::VertexDecl ms_decl;
};

bgfx::VertexDecl DebugVertex::ms_decl;


void screenSpaceQuad(float _textureWidth, float _textureHeight, float _texelHalf, bool _originBottomLeft,
                     float _width = 1.0f, float _height = 1.0f)
{
	if (3 == getAvailTransientVertexBuffer(3, PosTexCoord0Vertex::ms_decl))
	{
		bgfx::TransientVertexBuffer vb;
		allocTransientVertexBuffer(&vb, 3, PosTexCoord0Vertex::ms_decl);
		PosTexCoord0Vertex* vertex = (PosTexCoord0Vertex*)vb.data;

		const float minx = -_width;
		const float maxx = _width;
		const float miny = 0.0f;
		const float maxy = _height * 2.0f;

		const float texelHalfW = _texelHalf / _textureWidth;
		const float texelHalfH = _texelHalf / _textureHeight;
		const float minu = -1.0f + texelHalfW;
		const float maxu = 1.0f + texelHalfH;

		const float zz = 0.0f;

		float minv = texelHalfH;
		float maxv = 2.0f + texelHalfH;

		if (_originBottomLeft)
		{
			float temp = minv;
			minv = maxv;
			maxv = temp;

			minv -= 1.0f;
			maxv -= 1.0f;
		}

		vertex[0].m_x = minx;
		vertex[0].m_y = miny;
		vertex[0].m_z = zz;
		vertex[0].m_u = minu;
		vertex[0].m_v = minv;

		vertex[1].m_x = maxx;
		vertex[1].m_y = miny;
		vertex[1].m_z = zz;
		vertex[1].m_u = maxu;
		vertex[1].m_v = minv;

		vertex[2].m_x = maxx;
		vertex[2].m_y = maxy;
		vertex[2].m_z = zz;
		vertex[2].m_u = maxu;
		vertex[2].m_v = maxv;

		setVertexBuffer(0, &vb);
	}
}

constexpr bgfx::ViewId kRenderPassGeometry = 0;
constexpr bgfx::ViewId kRenderPassLight = 1;
constexpr bgfx::ViewId kRenderPassCombine = 2;
constexpr bgfx::ViewId kRenderPassDebugLights = 3;
constexpr bgfx::ViewId kRenderPassDebugGBuffer = 4;

static float s_texelHalf = 0.0f;

std::string SceneRenderer_deferred::GetName()
{
	return "deferred";
}

void SceneRenderer_deferred::Init()
{
	// Set palette color for index 0
	bgfx::setPaletteColor(0, UINT32_C(0x00000000));

	// Set palette color for index 1
	bgfx::setPaletteColor(1, UINT32_C(0x303030ff));

	// Set geometry pass view clear state.
	bgfx::setViewClear(kRenderPassGeometry
	                   , BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
	                   , 1.0f
	                   , 0
	                   , 1
	);

	// Set light pass view clear state.
	bgfx::setViewClear(kRenderPassLight
	                   , BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
	                   , 1.0f
	                   , 0
	                   , 0
	);

	PosTexCoord0Vertex::init();
	DebugVertex::init();
	// Create texture sampler uniforms.
	s_texColor = createUniform("s_texColor", bgfx::UniformType::Sampler);
	s_texNormal = createUniform("s_texNormal", bgfx::UniformType::Sampler);

	s_albedo = createUniform("s_albedo", bgfx::UniformType::Sampler);
	s_normal = createUniform("s_normal", bgfx::UniformType::Sampler);
	s_depth = createUniform("s_depth", bgfx::UniformType::Sampler);
	s_light = createUniform("s_light", bgfx::UniformType::Sampler);

	u_mtx = createUniform("u_mtx", bgfx::UniformType::Mat4);
	u_lightPosRadius = createUniform("u_lightPosRadius", bgfx::UniformType::Vec4);
	u_lightRgbInnerR = createUniform("u_lightRgbInnerR", bgfx::UniformType::Vec4);

	// Create program from shaders.
	m_geomProgram = loadProgram("vs_deferred_geom", "fs_deferred_geom");
	m_lightProgram = loadProgram("vs_deferred_light", "fs_deferred_light");
	m_combineProgram = loadProgram("vs_deferred_combine", "fs_deferred_combine");
	m_debugProgram = loadProgram("vs_deferred_debug", "fs_deferred_debug");
	m_lineProgram = loadProgram("vs_deferred_debug_line", "fs_deferred_debug_line");

	m_useTArray = false;
	m_useUav = false;
	m_showScissorRects = false;

	if (0 != (BGFX_CAPS_TEXTURE_2D_ARRAY & bgfx::getCaps()->supported))
	{
		m_lightTaProgram = loadProgram("vs_deferred_light", "fs_deferred_light_ta");
	}
	else
	{
		m_lightTaProgram = BGFX_INVALID_HANDLE;
	}

	if (0 != (BGFX_CAPS_FRAMEBUFFER_RW & bgfx::getCaps()->supported))
	{
		m_lightUavProgram = loadProgram("vs_deferred_light", "fs_deferred_light_uav");
		m_clearUavProgram = loadProgram("vs_deferred_light", "fs_deferred_clear_uav");
	}
	else
	{
		m_lightUavProgram = BGFX_INVALID_HANDLE;
		m_clearUavProgram = BGFX_INVALID_HANDLE;
	}

	// Load diffuse texture.
	m_textureColor = loadTexture("textures/fieldstone-rgba.dds");

	// Load normal texture.
	m_textureNormal = loadTexture("textures/fieldstone-n.dds");

	m_gbufferTex[0].idx = bgfx::kInvalidHandle;
	m_gbufferTex[1].idx = bgfx::kInvalidHandle;
	m_gbufferTex[2].idx = bgfx::kInvalidHandle;
	m_gbuffer.idx = bgfx::kInvalidHandle;
	m_lightBuffer.idx = bgfx::kInvalidHandle;
}

void SceneRenderer_deferred::OnDestroy()
{
	if (isValid(m_gbuffer))
	{
		destroy(m_gbuffer);
		destroy(m_lightBuffer);
	}


	destroy(m_geomProgram);
	destroy(m_lightProgram);

	if (isValid(m_lightTaProgram))
	{
		destroy(m_lightTaProgram);
	}

	if (isValid(m_lightUavProgram))
	{
		destroy(m_lightUavProgram);
		destroy(m_clearUavProgram);
	}

	destroy(m_combineProgram);
	destroy(m_debugProgram);
	destroy(m_lineProgram);

	destroy(m_textureColor);
	destroy(m_textureNormal);
	destroy(s_texColor);
	destroy(s_texNormal);

	destroy(s_albedo);
	destroy(s_normal);
	destroy(s_depth);
	destroy(s_light);

	destroy(u_lightPosRadius);
	destroy(u_lightRgbInnerR);
	destroy(u_mtx);
}


void SceneRenderer_deferred::OnGui()
{
	ImGui::SetNextWindowPos(
		ImVec2(m_width - m_width / 5.0f - 10.0f, 10.0f)
		, ImGuiCond_FirstUseEver
	);
	ImGui::SetNextWindowSize(
		ImVec2(m_width / 5.0f, m_height / 3.0f)
		, ImGuiCond_FirstUseEver
	);
	ImGui::Begin("Settings"
	             , nullptr
	             , 0
	);

	ImGui::Checkbox("Show G-Buffer.", &m_showGBuffer);
	ImGui::Checkbox("Show light scissor.", &m_showScissorRects);

	if (isValid(m_lightTaProgram))
	{
		ImGui::Checkbox("Use texture array frame buffer.", &m_useTArray);
	}
	else
	{
		ImGui::Text("Texture array frame buffer is not supported.");
	}

	if (isValid(m_lightUavProgram))
	{
		ImGui::Checkbox("Use UAV frame buffer attachment.", &m_useUav);
	}
	else
	{
		ImGui::Text("UAV frame buffer attachment is not supported.");
	}

	ImGui::End();
}


void SceneRenderer_deferred::RenderScene(Scene* scene)
{
	InitGBuffer();

	OnGui();

	const bgfx::Caps* caps = bgfx::getCaps();

	float view[16];
	cameraGetViewMtx(view);

	// Setup views
	float vp[16];
	float invMvp[16];
	{
		bgfx::setViewRect(kRenderPassGeometry, 0, 0, uint16_t(m_width), uint16_t(m_height));
		bgfx::setViewRect(kRenderPassLight, 0, 0, uint16_t(m_width), uint16_t(m_height));
		bgfx::setViewRect(kRenderPassCombine, 0, 0, uint16_t(m_width), uint16_t(m_height));
		bgfx::setViewRect(kRenderPassDebugLights, 0, 0, uint16_t(m_width), uint16_t(m_height));
		bgfx::setViewRect(kRenderPassDebugGBuffer, 0, 0, uint16_t(m_width), uint16_t(m_height));

		setViewFrameBuffer(kRenderPassLight, m_lightBuffer);


		float proj[16];
		bx::mtxProj(proj, 60.0f, float(m_width) / float(m_height), 0.1f, 100.0f, caps->homogeneousDepth);

		setViewFrameBuffer(kRenderPassGeometry, m_gbuffer);
		bgfx::setViewTransform(kRenderPassGeometry, view, proj);

		bx::mtxMul(vp, view, proj);
		bx::mtxInverse(invMvp, vp);


		bx::mtxOrtho(proj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, caps->homogeneousDepth);
		bgfx::setViewTransform(kRenderPassLight, nullptr, proj);
		bgfx::setViewTransform(kRenderPassCombine, nullptr, proj);

		const float aspectRatio = float(m_height) / float(m_width);
		const float size = 10.0f;
		bx::mtxOrtho(proj, -size, size, size * aspectRatio, -size * aspectRatio, 0.0f, 1000.0f, 0.0f,
		             caps->homogeneousDepth);
		bgfx::setViewTransform(kRenderPassDebugGBuffer, nullptr, proj);

		bx::mtxOrtho(proj, 0.0f, (float)m_width, 0.0f, (float)m_height, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);
		bgfx::setViewTransform(kRenderPassDebugLights, nullptr, proj);
	}

	//geometry pass
	for (int index = 0; index < scene->models.size(); ++index)
	{
		// Bind textures.
		setTexture(0, s_texColor, m_textureColor);
		setTexture(1, s_texNormal, m_textureNormal);

		scene->DrawModelAsCube(kRenderPassGeometry, 0
		           | BGFX_STATE_WRITE_RGB
		           | BGFX_STATE_WRITE_A
		           | BGFX_STATE_WRITE_Z
		           | BGFX_STATE_DEPTH_TEST_LESS
		           | BGFX_STATE_MSAA, m_geomProgram, index);
	}

	// Clear UAV texture
	if (m_useUav)
	{
		screenSpaceQuad((float)m_width, (float)m_height, s_texelHalf, caps->originBottomLeft);
		bgfx::setState(0
			| BGFX_STATE_WRITE_RGB
			| BGFX_STATE_WRITE_A
		);
		submit(kRenderPassLight, m_clearUavProgram);
	}

	// Draw lights into light buffer.

	for (int index = 0; index < scene->addLights.size(); ++index)
	{
		Light* curLight = scene->addLights[index];
		Sphere lightSphere;
		lightSphere.center.x = curLight->lightPosRadius[0];
		lightSphere.center.y = curLight->lightPosRadius[1];
		lightSphere.center.z = curLight->lightPosRadius[2];
		lightSphere.radius = curLight->lightPosRadius[3];

		Aabb aabb;
		toAabb(aabb, lightSphere);

		const bx::Vec3 box[8] =
		{
			{aabb.min.x, aabb.min.y, aabb.min.z},
			{aabb.min.x, aabb.min.y, aabb.max.z},
			{aabb.min.x, aabb.max.y, aabb.min.z},
			{aabb.min.x, aabb.max.y, aabb.max.z},
			{aabb.max.x, aabb.min.y, aabb.min.z},
			{aabb.max.x, aabb.min.y, aabb.max.z},
			{aabb.max.x, aabb.max.y, aabb.min.z},
			{aabb.max.x, aabb.max.y, aabb.max.z},
		};

		bx::Vec3 xyz = mulH(box[0], vp);
		bx::Vec3 min = xyz;
		bx::Vec3 max = xyz;

		for (uint32_t ii = 1; ii < 8; ++ii)
		{
			xyz = mulH(box[ii], vp);
			min = bx::min(min, xyz);
			max = bx::max(max, xyz);
		}

		// Cull light if it's fully behind camera.
		if (max.z >= 0.0f)
		{
			const float x0 = bx::clamp((min.x * 0.5f + 0.5f) * m_width, 0.0f, (float)m_width);
			const float y0 = bx::clamp((min.y * 0.5f + 0.5f) * m_height, 0.0f, (float)m_height);
			const float x1 = bx::clamp((max.x * 0.5f + 0.5f) * m_width, 0.0f, (float)m_width);
			const float y1 = bx::clamp((max.y * 0.5f + 0.5f) * m_height, 0.0f, (float)m_height);

			if (m_showScissorRects)
			{
				bgfx::TransientVertexBuffer tvb;
				bgfx::TransientIndexBuffer tib;
				if (allocTransientBuffers(&tvb, DebugVertex::ms_decl, 4, &tib, 8))
				{
					uint32_t abgr = 0x8000ff00;

					DebugVertex* vertex = (DebugVertex*)tvb.data;
					vertex->m_x = x0;
					vertex->m_y = y0;
					vertex->m_z = 0.0f;
					vertex->m_abgr = abgr;
					++vertex;

					vertex->m_x = x1;
					vertex->m_y = y0;
					vertex->m_z = 0.0f;
					vertex->m_abgr = abgr;
					++vertex;

					vertex->m_x = x1;
					vertex->m_y = y1;
					vertex->m_z = 0.0f;
					vertex->m_abgr = abgr;
					++vertex;

					vertex->m_x = x0;
					vertex->m_y = y1;
					vertex->m_z = 0.0f;
					vertex->m_abgr = abgr;

					uint16_t* indices = (uint16_t*)tib.data;
					*indices++ = 0;
					*indices++ = 1;
					*indices++ = 1;
					*indices++ = 2;
					*indices++ = 2;
					*indices++ = 3;
					*indices++ = 3;
					*indices++ = 0;

					setVertexBuffer(0, &tvb);
					setIndexBuffer(&tib);
					bgfx::setState(0
						| BGFX_STATE_WRITE_RGB
						| BGFX_STATE_PT_LINES
						| BGFX_STATE_BLEND_ALPHA
					);
					submit(kRenderPassDebugLights, m_lineProgram);
				}
			}


			// Draw light.
			setUniform(u_lightRgbInnerR, curLight->color);
			setUniform(u_lightPosRadius, curLight->lightPosRadius);
			setUniform(u_mtx, invMvp);
			const uint16_t scissorHeight = uint16_t(y1 - y0);
			bgfx::setScissor(uint16_t(x0), uint16_t(m_height - scissorHeight - y0), uint16_t(x1 - x0),
			                 uint16_t(scissorHeight));
			setTexture(0, s_normal, getTexture(m_gbuffer, 1));
			setTexture(1, s_depth, getTexture(m_gbuffer, 2));
			bgfx::setState(0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				| BGFX_STATE_BLEND_ADD
			);
			screenSpaceQuad((float)m_width, (float)m_height, s_texelHalf, caps->originBottomLeft);

			if (isValid(m_lightTaProgram)
				&& m_useTArray)
			{
				submit(kRenderPassLight, m_lightTaProgram);
			}
			else if (isValid(m_lightUavProgram)
				&& m_useUav)
			{
				submit(kRenderPassLight, m_lightUavProgram);
			}
			else
			{
				submit(kRenderPassLight, m_lightProgram);
			}
		}
	}

	// Combine color and light buffers.
	setTexture(0, s_albedo, m_gbufferTex[0]);
	setTexture(1, s_light, m_lightBufferTex);
	bgfx::setState(0
		| BGFX_STATE_WRITE_RGB
		| BGFX_STATE_WRITE_A
	);
	screenSpaceQuad((float)m_width, (float)m_height, s_texelHalf, caps->originBottomLeft);
	submit(kRenderPassCombine, m_combineProgram);

	// 	if (m_showGBuffer)
	// {
	// 	const float aspectRatio = float(m_width)/float(m_height);
	//
	// 	// Draw m_debug m_gbuffer.
	// 	for (uint32_t ii = 0; ii < BX_COUNTOF(m_gbufferTex); ++ii)
	// 	{
	// 		float mtx[16];
	// 		bx::mtxSRT(mtx
	// 			, aspectRatio, 1.0f, 1.0f
	// 			, 0.0f, 0.0f, 0.0f
	// 			, -7.9f - BX_COUNTOF(m_gbufferTex)*0.1f*0.5f + ii*2.1f*aspectRatio, 4.0f, 0.0f
	// 			);
	//
	// 		bgfx::setTransform(mtx);
	// 		bgfx::setVertexBuffer(0, m_vbh);
	// 		bgfx::setIndexBuffer(m_ibh, 0, 6);
	// 		bgfx::setTexture(0, s_texColor, m_gbufferTex[ii]);
	// 		bgfx::setState(BGFX_STATE_WRITE_RGB);
	// 		bgfx::submit(kRenderPassDebugGBuffer, m_debugProgram);
	// 	}
	// }

	imguiEndFrame();

	// Advance to next frame. Rendering thread will be kicked to
	// process submitted rendering primitives.
	bgfx::frame();
}

void SceneRenderer_deferred::InitGBuffer()
{
	if (m_oldWidth != m_width
		|| m_oldHeight != m_height
		|| m_oldReset != m_reset
		|| m_oldUseTArray != m_useTArray
		|| m_oldUseUav != m_useUav
		|| !isValid(m_gbuffer))
	{
		// Recreate variable size render targets when resolution changes.
		m_oldWidth = m_width;
		m_oldHeight = m_height;
		m_oldReset = m_reset;
		m_oldUseTArray = m_useTArray;
		m_oldUseUav = m_useUav;

		if (isValid(m_gbuffer))
		{
			destroy(m_gbuffer);
			m_gbufferTex[0].idx = bgfx::kInvalidHandle;
			m_gbufferTex[1].idx = bgfx::kInvalidHandle;
			m_gbufferTex[2].idx = bgfx::kInvalidHandle;
		}

		const uint64_t tsFlags = 0
			| BGFX_SAMPLER_MIN_POINT
			| BGFX_SAMPLER_MAG_POINT
			| BGFX_SAMPLER_MIP_POINT
			| BGFX_SAMPLER_U_CLAMP
			| BGFX_SAMPLER_V_CLAMP;

		bgfx::Attachment gbufferAt[3];

		if (m_useTArray)
		{
			m_gbufferTex[0] = createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 2,
			                                  bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | tsFlags);
			gbufferAt[0].init(m_gbufferTex[0], bgfx::Access::Write, 0);
			gbufferAt[1].init(m_gbufferTex[0], bgfx::Access::Write, 1);
		}
		else
		{
			m_gbufferTex[0] = createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1,
			                                  bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | tsFlags);
			m_gbufferTex[1] = createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1,
			                                  bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | tsFlags);
			gbufferAt[0].init(m_gbufferTex[0]);
			gbufferAt[1].init(m_gbufferTex[1]);
		}

		m_gbufferTex[2] = createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::D24S8,
		                                  BGFX_TEXTURE_RT | tsFlags);
		gbufferAt[2].init(m_gbufferTex[2]);

		m_gbuffer = createFrameBuffer(BX_COUNTOF(gbufferAt), gbufferAt, true);

		if (isValid(m_lightBuffer))
		{
			destroy(m_lightBuffer);
		}

		if (m_useUav)
		{
			bgfx::Attachment lightAt[2];

			bgfx::TextureHandle target = createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1,
			                                             bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | tsFlags);
			m_lightBufferTex = createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1,
			                                   bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_COMPUTE_WRITE | tsFlags);
			lightAt[0].init(target);
			lightAt[1].init(m_lightBufferTex, bgfx::Access::ReadWrite);

			m_lightBuffer = createFrameBuffer(BX_COUNTOF(lightAt), lightAt, true);
		}
		else
		{
			m_lightBufferTex = createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1,
			                                   bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | tsFlags);
			m_lightBuffer = createFrameBuffer(1, &m_lightBufferTex, true);
		}
	}
}
