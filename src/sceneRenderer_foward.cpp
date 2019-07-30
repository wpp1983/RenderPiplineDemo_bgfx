#include "sceneRenderer_foward.h"
#include "bgfx_utils.h"
#include "bx/timer.h"
#include "model.h"
#include "scene.h"
#include "light.h"
#include "camera.h"
#include "imgui/imgui.h"

void SceneRenderer_foward::Init()
{
	u_lightPosRadius = createUniform("u_lightPosRadius", bgfx::UniformType::Vec4);
	u_lightRgbInnerR = createUniform("u_lightRgbInnerR", bgfx::UniformType::Vec4);

	// Create program from shaders.
	m_BasePassProgram = loadProgram("vs_foward_basepass", "fs_foward_basepass");
	m_AddPassProgram = loadProgram("vs_foward_basepass", "fs_foward_addpass");

	// Create texture sampler uniforms.
	s_texColor = createUniform("s_texColor", bgfx::UniformType::Sampler);
	s_texNormal = createUniform("s_texNormal", bgfx::UniformType::Sampler);

	// Load diffuse texture.
	m_textureColor = loadTexture("textures/fieldstone-rgba.dds");

	// Load normal texture.
	m_textureNormal = loadTexture("textures/fieldstone-n.dds");

	// Set palette color for index 0
	bgfx::setPaletteColor(0, UINT32_C(0x00000000));

	// Set view 0 clear state.
	bgfx::setViewClear(0
	                   , BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
	                   , 1.0f
	                   , 0
	                   , 0
	);
}

void SceneRenderer_foward::OnDestroy()
{
	destroy(u_lightPosRadius);
	destroy(u_lightRgbInnerR);

	destroy(m_BasePassProgram);
	destroy(m_AddPassProgram);

	destroy(s_texColor);
	destroy(s_texNormal);
	destroy(m_textureColor);
	destroy(m_textureNormal);
}


void SceneRenderer_foward::RenderScene(Scene* scene)
{
	// Set view 0 default viewport.
	bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));

	// This dummy draw call is here to make sure that view 0 is cleared
	// if no other draw calls are submitted to view 0.
	bgfx::touch(0);

	bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);

	// Set view and projection matrix for view 0.
	{
		float view[16];
		cameraGetViewMtx(view);

		float proj[16];
		bx::mtxProj(proj, 60.0f, float(m_width) / float(m_height), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
		bgfx::setViewTransform(0, view, proj);

		// Set view 0 default viewport.
		bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));
	}


	//basePass
	{
		for (int index = 0; index < scene->GetModelCount(); ++index)
		{
			setUniform(u_lightPosRadius, scene->direction_light->lightPosRadius);
			setUniform(u_lightRgbInnerR, scene->direction_light->color);

			// Bind textures.
			setTexture(0, s_texColor, m_textureColor);
			setTexture(1, s_texNormal, m_textureNormal);


			scene->DrawModelAsCube(0, 0
			                       | BGFX_STATE_WRITE_RGB
			                       | BGFX_STATE_WRITE_A
			                       | BGFX_STATE_WRITE_Z
			                       | BGFX_STATE_DEPTH_TEST_LESS, m_BasePassProgram, index);
		}
	}

	//add light pass
	// Bind textures.

	{
		for (int index = 0; index < scene->addLights.size(); ++index)
		{
			bool isLightSetted = false;
			Light* curLight = scene->addLights[index];

			Sphere lightSphere;
			lightSphere.center.x = curLight->lightPosRadius[0];
			lightSphere.center.y = curLight->lightPosRadius[1];
			lightSphere.center.z = curLight->lightPosRadius[2];
			lightSphere.radius = curLight->lightPosRadius[3];
			for (int index = 0; index < scene->GetModelCount(); ++index)
			{
				Model* model = scene->models[index];

				Sphere a;
				a.center = model->pos;
				a.radius = model->scale.x;
				if (overlap(a, lightSphere))
				{
					if (!isLightSetted)
					{
						isLightSetted = true;
						setUniform(u_lightRgbInnerR, curLight->color);
						setUniform(u_lightPosRadius, curLight->lightPosRadius);
					}


					setTexture(0, s_texColor, m_textureColor);
					setTexture(1, s_texNormal, m_textureNormal);


					scene->DrawModelAsCube(0, 0
					                       | BGFX_STATE_WRITE_RGB
					                       | BGFX_STATE_BLEND_ADD
					                       | BGFX_STATE_DEPTH_TEST_LEQUAL, m_AddPassProgram, index);
				}
			}
		}
	}


	imguiEndFrame();

	// Advance to next frame. Rendering thread will be kicked to
	// process submitted rendering primitives.
	bgfx::frame();
}

std::string SceneRenderer_foward::GetName()
{
	return "foward";
}
