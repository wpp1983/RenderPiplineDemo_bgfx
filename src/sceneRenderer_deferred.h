#ifndef SCENERENDERER_DEFERRED_H
#define SCENERENDERER_DEFERRED_H

#include "sceneRenderer.h"
#include "bgfx/bgfx.h"

class SceneRenderer_deferred final : public SceneRenderer
{
public:
	void RenderScene(Scene* scene) override;
	std::string GetName() override;

	void Init() override;
	void OnDestroy() override;


	uint32_t m_debug;
	uint32_t m_reset;

	int m_oldWidth;
	int m_oldHeight;
	uint32_t m_oldReset;

	bool m_useTArray;
	bool m_oldUseTArray;

	bool m_useUav;
	bool m_oldUseUav;

	int32_t m_scrollArea;
	bool m_showScissorRects;
	bool m_showGBuffer;

private:
	void InitGBuffer();
	void OnGui();
private:
	bgfx::UniformHandle s_texColor;
	bgfx::UniformHandle s_texNormal;

	bgfx::UniformHandle s_albedo;
	bgfx::UniformHandle s_normal;
	bgfx::UniformHandle s_depth;
	bgfx::UniformHandle s_light;

	bgfx::UniformHandle u_mtx;
	bgfx::UniformHandle u_lightPosRadius;
	bgfx::UniformHandle u_lightRgbInnerR;

	bgfx::ProgramHandle m_geomProgram;
	bgfx::ProgramHandle m_lightProgram;
	bgfx::ProgramHandle m_lightTaProgram;
	bgfx::ProgramHandle m_lightUavProgram;
	bgfx::ProgramHandle m_clearUavProgram;
	bgfx::ProgramHandle m_combineProgram;
	bgfx::ProgramHandle m_debugProgram;
	bgfx::ProgramHandle m_lineProgram;
	bgfx::TextureHandle m_textureColor;
	bgfx::TextureHandle m_textureNormal;

	bgfx::TextureHandle m_gbufferTex[3];
	bgfx::TextureHandle m_lightBufferTex;
	bgfx::FrameBufferHandle m_gbuffer;
	bgfx::FrameBufferHandle m_lightBuffer;
};

#endif
