#include "sceneRenderer_fowardPlus.h"
#include "bgfx_utils.h"
#include "camera.h"
#include "model.h"
#include "scene.h"
#include "light.h"
#include "imgui/imgui.h"
#include "bx/handlealloc.h"
#include "bx/math.h"


std::string SceneRenderer_fowardPlus::GetName()
{
	return "fowardPlus";
}

void SceneRenderer_fowardPlus::InitClusterBuffer()
{
	m_buildAABBGridProgram = createProgram(loadShader("cs_clusterShader"), true);

	bgfx::UniformHandle inverseProjection = createUniform("inverseProjection", bgfx::UniformType::Mat4);
	bgfx::UniformHandle screenParam = createUniform("screenParam", bgfx::UniformType::Vec4);
	bgfx::UniformHandle GridDim = createUniform("GridDim", bgfx::UniformType::Vec4);
	bgfx::UniformHandle ClusterCSize = createUniform("ClusterCSize", bgfx::UniformType::Vec4);

	float cameraPlaneData[4]
	{
		zNear,
		zFar,
		NearK,
		0.f
	};
	float proj[16];
	bx::mtxProj(proj, fovY, float(m_width) / float(m_height), zNear, zFar, bgfx::getCaps()->homogeneousDepth);
	float mtxInv[16];
	bx::mtxInverse(mtxInv, proj);

	float GridDimData[4]
	{
		clusterDimX,
		clusterDimY,
		clusterDimZ,
		0,
	};

	float ClusterCSizeData[4]
	{
		g_ClusterGridBlockSize,
		g_ClusterGridBlockSize,
		0,
		0,
	};

	float screenParamData[4]
	{
		m_width,
		m_height,
		0,
		0
	};

	setUniform(u_cameraPlane, &cameraPlaneData);
	setUniform(inverseProjection, &mtxInv);
	setUniform(GridDim, &GridDimData);
	setUniform(ClusterCSize, &ClusterCSizeData);
	setUniform(screenParam, &screenParamData);
	setBuffer(0, m_aabbGridMinPointBuffer, bgfx::Access::ReadWrite);
	setBuffer(1, m_aabbGridMaxPointBuffer, bgfx::Access::ReadWrite);

	uint32_t threadGroups = static_cast<uint32_t>(bx::ceil((clusterDimX * clusterDimY * clusterDimZ) / 1024.0f));

	dispatch(0, m_buildAABBGridProgram, threadGroups, 1, 1);
}


struct Plane
{
	bx::Vec3 N; // Plane normal.
	float d; // Distance to origin.
};


bx::Vec3 SceneRenderer_fowardPlus::ScreenToView(bx::Vec3 screen)
{
	// Convert to normalized texture coordinates in the range [0 .. 1].
	float texCoordX = screen.x / m_width;
	float texCoordY = screen.y / m_height;

	// Convert to clip space
	// float clip[4]
	// {
	// 	(2.0f * screen.x / m_width - 1.0f) * -1,
	// 	 ( (1.0f - (2.0f * screen.y) / m_height) ) * -1.0f,
	// 	-1,
	// 	1
	// };
	float clip[4]
	{
		(2.0f * texCoordX - 1.0f),
		(2.0f * (1 - texCoordY) - 1.0f),
		1.0,
		1.0
	};

	return ClipToView(clip);
}

bx::Vec3 SceneRenderer_fowardPlus::ClipToView(float* clip)
{
	float proj[16];
	bx::mtxProj(proj, fovY, float(m_width) / float(m_height), zNear, zFar, bgfx::getCaps()->homogeneousDepth);
	float projInverse[16];
	bx::mtxInverse(projInverse, proj);

	float view[4];
	bx::vec4MulMtx(view, clip, projInverse);
	bx::Vec3 result(view[0] / view[3], view[1] / view[3], view[2] / view[3]);
	return result;
}

bool IntersectLinePlane(bx::Vec3 a, bx::Vec3 b, Plane p, bx::Vec3& q)
{
	bx::Vec3 ab = sub(b, a);

	float t = (p.d - dot(p.N, a)) / dot(p.N, ab);

	bool intersect = (t >= 0.0f && t <= 1.0f);

	q = bx::Vec3(0, 0, 0);
	if (intersect)
	{
		q = add(a, mul(ab, t));
	}

	return intersect;
}

void SceneRenderer_fowardPlus::InitClusterInfo()
{
	float fieldOfViewY = bx::toRad(fovY * 0.5f);

	// Number of clusters in the screen X direction.
	clusterDimX = static_cast<uint32_t>(bx::ceil(m_width / (float)g_ClusterGridBlockSize));
	// Number of clusters in the screen Y direction.
	clusterDimY = static_cast<uint32_t>(bx::ceil(m_height / (float)g_ClusterGridBlockSize));

	// The depth of the cluster grid during clustered rendering is dependent on the 
	// number of clusters subdivisions in the screen Y direction.
	// Source: Clustered Deferred and Forward Shading (2012) (Ola Olsson, Markus Billeter, Ulf Assarsson).
	float sD = 2.0f * bx::tan(fieldOfViewY) / (float)clusterDimY;
	float logDimY = 1.0f / bx::log(1.0f + sD);

	float logDepth = bx::log(zFar / zNear);
	clusterDimZ = static_cast<uint32_t>(bx::floor(logDepth * logDimY));


	clusterGridNumber = clusterDimX * clusterDimY * clusterDimZ;

	GridDim.x = clusterDimX;
	GridDim.y = clusterDimY;
	GridDim.z = clusterDimZ;

	NearK = 1.0f + sD;
	LogGridDimY = logDimY;
}

void SceneRenderer_fowardPlus::DoClusterByCPU(Scene* scene)
{
	float view[16];
	cameraGetViewMtx(view);

	bx::Vec3 eyePosInView = mul(cameraGetPosition(), view);

	float proj[16];
	bx::mtxProj(proj, fovY, float(m_width) / float(m_height), zNear, zFar, bgfx::getCaps()->homogeneousDepth);
	float _viewProj[16];
	bx::mtxMul(_viewProj, view, proj);
	bx::Plane planes[6];
	buildFrustumPlanes(planes, _viewProj);

	const bx::Vec3 points[8] =
	{
		intersectPlanes(planes[0], planes[2], planes[4]),
		intersectPlanes(planes[0], planes[3], planes[4]),
		intersectPlanes(planes[0], planes[3], planes[5]),
		intersectPlanes(planes[0], planes[2], planes[5]),
		intersectPlanes(planes[1], planes[2], planes[4]),
		intersectPlanes(planes[1], planes[3], planes[4]),
		intersectPlanes(planes[1], planes[3], planes[5]),
		intersectPlanes(planes[1], planes[2], planes[5]),
	};

	bx::Vec3 Viewpoints[8];
	for (int index = 0; index < 8; ++index)
	{
		Viewpoints[index] = mul(points[index], view);
	}


	bx::Vec3* aaddMinAll = new bx::Vec3[clusterGridNumber];
	bx::Vec3* aaddMaxAll = new bx::Vec3[clusterGridNumber];


	//init aabb
	{
		for (int z = 0; z < clusterDimZ; ++z)
		{
			for (int y = 0; y < clusterDimY; ++y)
			{
				for (int x = 0; x < clusterDimX; ++x)
				{
					int index = x + clusterDimX * (y + clusterDimY * z);
					Plane nearPlane;
					nearPlane.N.x = 0.0f;
					nearPlane.N.y = 0.0f;
					nearPlane.N.z = 1.0f;
					nearPlane.d = zNear * pow(bx::abs(NearK), z);
					Plane farPlane;
					farPlane.N.x = 0.0f;
					farPlane.N.y = 0.0f;
					farPlane.N.z = 1.0f;
					farPlane.d = zNear * pow(bx::abs(NearK), z + 1);

					bx::Vec3 pMin;
					pMin.x = x * g_ClusterGridBlockSize;
					pMin.y = y * g_ClusterGridBlockSize;
					bx::Vec3 pMax;
					pMax.x = (x + 1) * g_ClusterGridBlockSize;
					pMax.y = (y + 1) * g_ClusterGridBlockSize;

					pMin = ScreenToView(pMin);
					pMax = ScreenToView(pMax);

					// Find the min and max points on the near and far planes.
					bx::Vec3 nearMin, nearMax, farMin, farMax;
					// Origin (camera eye position)
					bx::Vec3 eye = bx::Vec3(0, 0, 0);

					IntersectLinePlane(eye, pMin, nearPlane, nearMin);
					IntersectLinePlane(eye, pMax, nearPlane, nearMax);
					IntersectLinePlane(eye, pMin, farPlane, farMin);
					IntersectLinePlane(eye, pMax, farPlane, farMax);

					bx::Vec3 aabbMin = min(nearMin, min(nearMax, min(farMin, farMax)));
					bx::Vec3 aabbMax = max(nearMin, max(nearMax, max(farMin, farMax)));

					aaddMinAll[index] = aabbMin;
					aaddMaxAll[index] = aabbMax;
				}
			}
		}
	}

	int* globalLightIndexList = new int[clusterGridNumber * maxLightsPerTile];
	int* lightGridOffset = new int[clusterGridNumber];
	int* lightGridCount = new int[clusterGridNumber];
	//light cull
	{
		int globalIndexCount = 0;

		for (int z = 0; z < clusterDimZ; ++z)
		{
			for (int y = 0; y < clusterDimY; ++y)
			{
				for (int x = 0; x < clusterDimX; ++x)
				{
					int visibleLightCount = 0;
					int visibleLightIndices[100];

					int index = x + clusterDimX * (y + clusterDimY * z);
					bx::Vec3 minPos = aaddMinAll[index];
					bx::Vec3 maxPos = aaddMaxAll[index];
					Aabb aabb;
					aabb.min = minPos;
					aabb.max = maxPos;
					for (int lightIndex = 0; lightIndex < scene->addLights.size(); ++ lightIndex)
					{
						Light* curLight = scene->addLights[lightIndex];
						bx::Vec3 worldPos;
						worldPos.x = curLight->lightPosRadius[0];
						worldPos.y = curLight->lightPosRadius[1];
						worldPos.z = curLight->lightPosRadius[2];
						bx::Vec3 viewPos = mul(worldPos, view);
						Sphere lightSphere;
						lightSphere.center = viewPos;
						lightSphere.radius = curLight->lightPosRadius[3];
						if (overlap(aabb, lightSphere))
						{
							visibleLightIndices[visibleLightCount] = lightIndex;
							visibleLightCount += 1;
						}
					}

					int offset = globalIndexCount;
					globalIndexCount += visibleLightCount;

					for (int i = 0; i < visibleLightCount; ++i)
					{
						globalLightIndexList[offset + i] = visibleLightIndices[i];
					}


					lightGridOffset[index] = offset;
					lightGridCount[index] = visibleLightCount;
				}
			}
		}
	}

	//set buffer
	{
		const bgfx::Memory* mem_lightIndex = bgfx::copy(globalLightIndexList,
		                                                sizeof(int) * clusterGridNumber * maxLightsPerTile);
		update(m_lightIndexListBuffer, 0, mem_lightIndex);

		const bgfx::Memory* mem_lightGridOffset = bgfx::copy(lightGridOffset, sizeof(int) * clusterGridNumber);
		update(m_lightGridOffsetBuffer, 0, mem_lightGridOffset);

		const bgfx::Memory* mem_lightGridCount = bgfx::copy(lightGridCount, sizeof(int) * clusterGridNumber);
		update(m_lightGridCountBuffer, 0, mem_lightGridCount);
	}
}


void SceneRenderer_fowardPlus::Init()
{
	InitClusterInfo();

	// Create program from shaders.

	m_baseProgram = loadProgram("vs_fowardplus_basepass", "fs_fowardplus_renderpass");
	m_lightCullProgram = createProgram(loadShader("cs_clusterLightCull"), true);


	bgfx::VertexDecl computeVertexDecl;
	computeVertexDecl.begin()
	                 .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
	                 .end();
	m_aabbGridMinPointBuffer =
		createDynamicVertexBuffer(clusterGridNumber, computeVertexDecl, BGFX_BUFFER_COMPUTE_READ_WRITE);
	m_aabbGridMaxPointBuffer =
		createDynamicVertexBuffer(clusterGridNumber, computeVertexDecl, BGFX_BUFFER_COMPUTE_READ_WRITE);

	m_pointLight_posBuffer =
		createDynamicVertexBuffer(MaxLightNumber, computeVertexDecl, BGFX_BUFFER_COMPUTE_READ);
	m_pointLight_colorBuffer =
		createDynamicVertexBuffer(MaxLightNumber, computeVertexDecl, BGFX_BUFFER_COMPUTE_READ);

	if (!IsUseCPUCluster)
	{
		uint16_t flags = BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32;
		m_lightIndexListBuffer = bgfx::createDynamicIndexBuffer(clusterGridNumber * maxLightsPerTile, flags);
		m_lightGridOffsetBuffer = bgfx::createDynamicIndexBuffer(clusterGridNumber, flags);
		m_lightGridCountBuffer = bgfx::createDynamicIndexBuffer(clusterGridNumber, flags);
		m_lightIndexGlobalBuffer = bgfx::createDynamicIndexBuffer(1, flags);
	}
	else
	{
		uint16_t flags = BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_INDEX32;
		m_lightIndexListBuffer = bgfx::createDynamicIndexBuffer(clusterGridNumber * maxLightsPerTile, flags);
		m_lightGridOffsetBuffer = bgfx::createDynamicIndexBuffer(clusterGridNumber, flags);
		m_lightGridCountBuffer = bgfx::createDynamicIndexBuffer(clusterGridNumber, flags);
	}


	u_lightCount = createUniform("lightCount", bgfx::UniformType::Vec4);
	u_cameraPlane = createUniform("cameraPlane", bgfx::UniformType::Vec4);
	u_tileSizes = createUniform("tileSize", bgfx::UniformType::Vec4);
	u_GridDim = createUniform("GridDim", bgfx::UniformType::Vec4);


	// Create texture sampler uniforms.
	s_texColor = createUniform("s_texColor", bgfx::UniformType::Sampler);
	s_texNormal = createUniform("s_texNormal", bgfx::UniformType::Sampler);
	// Load diffuse texture.
	m_textureColor = loadTexture("textures/fieldstone-rgba.dds");
	// Load normal texture.
	m_textureNormal = loadTexture("textures/fieldstone-n.dds");

	//InitDepthBuffer();
}

void SceneRenderer_fowardPlus::OnDestroy()
{
	if (isValid(m_depthBufferTex))
	{
		destroy(m_depthBuffer);
		destroy(m_detphProgram);
		destroy(m_depthBufferTex);
	}

	destroy(m_lightCullProgram);
	destroy(m_buildAABBGridProgram);
	destroy(m_baseProgram);

	destroy(m_textureColor);
	destroy(m_textureNormal);


	destroy(m_aabbGridMinPointBuffer);
	destroy(m_aabbGridMaxPointBuffer);
	destroy(m_pointLight_posBuffer);
	destroy(m_pointLight_colorBuffer);
	destroy(m_lightIndexListBuffer);
	destroy(m_lightGridOffsetBuffer);

	destroy(m_lightGridCountBuffer);
	destroy(m_lightIndexGlobalBuffer);
	destroy(u_lightCount);
	destroy(u_cameraPlane);
	destroy(u_tileSizes);
	destroy(u_GridDim);
}

void SceneRenderer_fowardPlus::InitDepthBuffer()
{
	m_detphProgram = loadProgram("vs_fowardplus_depth", "fs_fowardplus_depth");

	if (m_oldWidth != m_width
		|| m_oldHeight != m_height
		|| !isValid(m_depthBuffer))
	{
		const uint64_t tsFlags = 0
			| BGFX_SAMPLER_MIN_POINT
			| BGFX_SAMPLER_MAG_POINT
			| BGFX_SAMPLER_MIP_POINT
			| BGFX_SAMPLER_U_CLAMP
			| BGFX_SAMPLER_V_CLAMP;

		m_depthBufferTex = createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1,
		                                   bgfx::TextureFormat::D24S8,
		                                   BGFX_TEXTURE_RT | tsFlags);

		m_depthBuffer = createFrameBuffer(1, &m_depthBufferTex, true);
	}
}

void SceneRenderer_fowardPlus::SetLightBuffer(Scene* scene)
{
	float view[16];
	cameraGetViewMtx(view);

	for (int lightIndex = 0; lightIndex < scene->addLights.size(); ++ lightIndex)
	{
		if (lightIndex >= MaxLightNumber)
		{
			break;
		}


		Light* curLight = scene->addLights[lightIndex];
		bx::Vec3 worldPos;
		worldPos.x = curLight->lightPosRadius[0];
		worldPos.y = curLight->lightPosRadius[1];
		worldPos.z = curLight->lightPosRadius[2];


		m_lightInfo_pos[lightIndex].value[0] = worldPos.x;
		m_lightInfo_pos[lightIndex].value[1] = worldPos.y;
		m_lightInfo_pos[lightIndex].value[2] = worldPos.z;
		m_lightInfo_pos[lightIndex].value[3] = curLight->lightPosRadius[3];
		m_lightInfo_color[lightIndex].value[0] = curLight->color[0];
		m_lightInfo_color[lightIndex].value[1] = curLight->color[1];
		m_lightInfo_color[lightIndex].value[2] = curLight->color[2];
	}

	const bgfx::Memory* mem_pos = bgfx::copy(m_lightInfo_pos, sizeof(Light_float4) * MaxLightNumber);
	update(m_pointLight_posBuffer, 0, mem_pos);
	setBuffer(5, m_pointLight_posBuffer, bgfx::Access::Read);

	const bgfx::Memory* mem_color = bgfx::copy(m_lightInfo_color, sizeof(Light_float4) * MaxLightNumber);
	update(m_pointLight_colorBuffer, 0, mem_color);
	setBuffer(6, m_pointLight_colorBuffer, bgfx::Access::Read);
}

void SceneRenderer_fowardPlus::DoClusterByGPU(Scene* scene)
{
	InitClusterBuffer();

	//light cull
	{
		float view[16];

		cameraGetViewMtx(view);
		//clusters buffer
		setBuffer(0, m_aabbGridMinPointBuffer, bgfx::Access::Read);
		setBuffer(1, m_aabbGridMaxPointBuffer, bgfx::Access::Read);


		{
			setBuffer(2, m_lightGridOffsetBuffer, bgfx::Access::Write);
			setBuffer(3, m_lightGridCountBuffer, bgfx::Access::Write);
			setBuffer(4, m_lightIndexListBuffer, bgfx::Access::Write);

			setBuffer(5, m_pointLight_posBuffer, bgfx::Access::Read);
		}


		{
			setBuffer(8, m_lightIndexGlobalBuffer, bgfx::Access::Write);
		}

		float lightCountData[4]
		{
			scene->addLights.size(),
			0,
			0,
			0,
		};
		setUniform(u_lightCount, &lightCountData);
		dispatch(0, m_lightCullProgram, clusterDimX * clusterDimY * clusterDimZ, 1, 1);
	}
}

void SceneRenderer_fowardPlus::RenderScene(Scene* scene)
{
	bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);

	// Set view 0 clear state.
	bgfx::setViewClear(0
	                   , BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
	                   , 1.0f
	                   , 0
	                   , 0
	);

	const bgfx::Caps* caps = bgfx::getCaps();

	bgfx::touch(0);

	// Setup views
	float view[16];
	{
		bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));


		cameraGetViewMtx(view);

		float proj[16];
		bx::mtxProj(proj, fovY, float(m_width) / float(m_height), zNear, zFar, bgfx::getCaps()->homogeneousDepth);
		bgfx::setViewTransform(0, view, proj);

		// Set view 0 default viewport.
		bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));
	}

	//depth Pass
	// {
	// 	setViewFrameBuffer(0, m_depthBuffer);
	// 	for (int index = 0; index < scene->models.size(); ++index)
	// 	{
	// 		Model* model = scene->models[index];
	// 		float mtx[16];
	// 		bx::mtxSRT(mtx,
	// 		           model->scale.x, model->scale.y, model->scale.z,
	// 		           model->rot.x, model->rot.y, model->rot.z,
	// 		           model->pos.x, model->pos.y, model->pos.z
	// 		);
	//
	// 		meshSubmit(model->mesh, 0, m_detphProgram, mtx, 0
	// 		           | BGFX_STATE_WRITE_RGB
	// 		           | BGFX_STATE_WRITE_A
	// 		           | BGFX_STATE_WRITE_Z
	// 		           | BGFX_STATE_DEPTH_TEST_LESS
	// 		           | BGFX_STATE_MSAA);
	// 	}
	// }


	SetLightBuffer(scene);


	if (IsUseCPUCluster)
	{
		DoClusterByCPU(scene);
	}
	else
	{
		DoClusterByGPU(scene);
	}

	//render pass
	{
		float cameraPlaneData[4]
		{
			zNear,
			zFar,
			0.f,
			0.f
		};

		float tileSizeData[4]
		{
			g_ClusterGridBlockSize,
			g_ClusterGridBlockSize,
			0,
			LogGridDimY,
		};

		float GridDimData[4]
		{
			GridDim.x,
			GridDim.y,
			GridDim.z,
			LogGridDimY,
		};


		for (int index = 0; index < scene->GetModelCount(); ++index)
		{
			// Bind textures.
			setTexture(0, s_texColor, m_textureColor);
			setTexture(1, s_texNormal, m_textureNormal);

			setBuffer(2, m_lightGridOffsetBuffer, bgfx::Access::Read);
			setBuffer(3, m_lightGridCountBuffer, bgfx::Access::Read);
			setBuffer(4, m_lightIndexListBuffer, bgfx::Access::Read);
			setBuffer(5, m_pointLight_posBuffer, bgfx::Access::Read);
			setBuffer(6, m_pointLight_colorBuffer, bgfx::Access::Read);

			setUniform(u_cameraPlane, &cameraPlaneData);
			setUniform(u_tileSizes, &tileSizeData);
			setUniform(u_GridDim, &GridDimData);


			scene->DrawModelAsCube(0, 0
			                       | BGFX_STATE_WRITE_RGB
			                       | BGFX_STATE_WRITE_A
			                       | BGFX_STATE_WRITE_Z
			                       | BGFX_STATE_DEPTH_TEST_LESS, m_baseProgram, index);
		}
	}


	imguiEndFrame();

	// Advance to next frame. Rendering thread will be kicked to
	// process submitted rendering primitives.
	bgfx::frame();
}
