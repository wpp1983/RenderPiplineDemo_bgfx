#ifndef SCENERENDERER_FOWARD_H
#define SCENERENDERER_FOWARD_H

#include "sceneRenderer.h"
#include "bgfx/bgfx.h"

class SceneRenderer_foward final : public SceneRenderer
{
public:
	void RenderScene(Scene* scene) override;

	void Init() override;
	std::string GetName() override;
	void OnDestroy() override;
private:

	bgfx::UniformHandle u_lightPosRadius;
	bgfx::UniformHandle u_lightRgbInnerR;

	bgfx::ProgramHandle m_BasePassProgram;
	bgfx::ProgramHandle m_AddPassProgram;
	bgfx::TextureHandle m_textureColor;
	bgfx::TextureHandle m_textureNormal;

	bgfx::UniformHandle s_texColor;
	bgfx::UniformHandle s_texNormal;
};



#endif
