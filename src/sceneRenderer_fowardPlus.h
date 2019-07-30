#ifndef SCENERENDERER_FOWARDPLUS_H
#define SCENERENDERER_FOWARDPLUS_H

#include "sceneRenderer.h"
#include "bgfx/bgfx.h"
#include "bx/bx.h"
#include "bx/math.h"


struct GPULight
{
	float position[4];
	float color[4];
	float intensity;
	float range;
	float padding;
	float padding2;
};

struct Light_float4
{
	float value[4];
};


class SceneRenderer_fowardPlus final : public SceneRenderer
{
public:
	void Init() override;
	void OnDestroy() override;
	
	
	std::string GetName() override;
	

	void RenderScene(Scene* scene) override;

	


	// Block size of a grid element used for 
	// clustered rendering.
	uint32_t g_ClusterGridBlockSize = 64;
	uint32_t clusterDimX = 0;
	uint32_t clusterDimY = 0;
	uint32_t clusterDimZ = 0;
	uint32_t clusterGridNumber = 0;


	const float zNear = 0.1f;
	const float zFar = 100.0f;

	static const int MaxLightNumber = 2048;
	const unsigned int maxLightsPerTile = 100;

	const float fovY = 60.f;

	bool IsUseCPUCluster = false;

private:
	bx::Vec3 ScreenToView(bx::Vec3 screen);
	bx::Vec3 ClipToView(float* clip);
	void InitClusterInfo();
	void DoClusterByCPU(Scene* scene);
	void DoClusterByGPU(Scene* scene);
	void InitClusterBuffer();
	void InitDepthBuffer();
	void SetLightBuffer(Scene* scene);

	bgfx::UniformHandle s_texColor;
	bgfx::UniformHandle s_texNormal;

	bgfx::UniformHandle s_depth;

	bgfx::UniformHandle u_mtx;

	

	bgfx::ProgramHandle m_buildAABBGridProgram= BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_lightCullProgram= BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_detphProgram= BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_baseProgram= BGFX_INVALID_HANDLE;

	bgfx::TextureHandle m_textureColor= BGFX_INVALID_HANDLE;
	bgfx::TextureHandle m_textureNormal= BGFX_INVALID_HANDLE;

	bgfx::TextureHandle m_depthBufferTex = BGFX_INVALID_HANDLE;
	bgfx::FrameBufferHandle m_depthBuffer= BGFX_INVALID_HANDLE;

	bgfx::DynamicVertexBufferHandle m_aabbGridMinPointBuffer= BGFX_INVALID_HANDLE;
	bgfx::DynamicVertexBufferHandle m_aabbGridMaxPointBuffer= BGFX_INVALID_HANDLE;
	bgfx::DynamicVertexBufferHandle m_pointLight_posBuffer= BGFX_INVALID_HANDLE;
	bgfx::DynamicVertexBufferHandle m_pointLight_colorBuffer= BGFX_INVALID_HANDLE;
	bgfx::DynamicIndexBufferHandle m_lightIndexListBuffer= BGFX_INVALID_HANDLE;
	bgfx::DynamicIndexBufferHandle m_lightGridOffsetBuffer= BGFX_INVALID_HANDLE;
	bgfx::DynamicIndexBufferHandle m_lightGridCountBuffer= BGFX_INVALID_HANDLE;
	bgfx::DynamicIndexBufferHandle m_lightIndexGlobalBuffer= BGFX_INVALID_HANDLE;

	bgfx::UniformHandle u_lightCount= BGFX_INVALID_HANDLE;
	bgfx::UniformHandle u_cameraPlane= BGFX_INVALID_HANDLE;
	bgfx::UniformHandle u_tileSizes= BGFX_INVALID_HANDLE;
	bgfx::UniformHandle u_GridDim= BGFX_INVALID_HANDLE;
	

	int m_oldWidth;
	int m_oldHeight;

	Light_float4 m_lightInfo_pos[MaxLightNumber];
	Light_float4 m_lightInfo_color[MaxLightNumber];

	float LogGridDimY;
	bx::Vec3 GridDim;
	float NearK;
};

#endif //SCENERENDERER_FOWARDPLUS_H
