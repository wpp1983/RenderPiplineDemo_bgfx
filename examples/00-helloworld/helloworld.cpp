/*
 * Copyright 2011-2019 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include <bx/uint32_t.h>
#include "common.h"
#include "bgfx_utils.h"
#include "imgui/imgui.h"

#include "scene.h"
#include "sceneRenderer_factory.h"
#include "camera.h"
#include "../../src/light.h"

namespace
{
	const char* FowardRenderName = "foward";
	const char* DeferredRenderName = "deferred";
	const char* FowardPlusRenderName = "fowardPlus";

	class ExampleHelloWorld : public entry::AppI
	{
	public:
		ExampleHelloWorld(const char* _name, const char* _description)
			: AppI(_name, _description)
		{
		}

		void InitRenderer(std::string rendererName)
		{
			m_renderer = SceneRenderer_Factory::CreateSceneRenderer(rendererName);
			m_renderer->m_width = m_width;
			m_renderer->m_height = m_height;
			m_renderer->Init();
		}

		void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
		{
			Args args(_argc, _argv);

			m_width = _width;
			m_height = _height;
			m_debug = BGFX_DEBUG_TEXT;
			m_reset = BGFX_RESET_VSYNC;

			m_timeOffset = bx::getHPCounter();

			bgfx::Init init;
			init.type = args.m_type;
			init.vendorId = args.m_pciId;
			init.resolution.width = m_width;
			init.resolution.height = m_height;
			init.resolution.reset = m_reset;
			bgfx::init(init);

			// Enable debug text.
			bgfx::setDebug(m_debug);


			imguiCreate();

			cameraCreate();
			cameraSetPosition({0.0f, 0.0f, -15.0f});
			cameraSetVerticalAngle(0.0f);

			m_scene = new Scene;

			InitRenderer(FowardPlusRenderName);
		}

		int shutdown() override
		{
			imguiDestroy();

			m_renderer->OnDestroy();

			// Shutdown bgfx.
			bgfx::shutdown();

			return 0;
		}

		void SceneUpdate()
		{
			int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency());
			const float deltaTime = float(frameTime / freq);


			// Update camera.
			cameraUpdate(deltaTime, m_mouseState);

			ImGui::SetNextWindowPos(
				ImVec2(m_width / 2.0f, 10.0f)
				, ImGuiCond_FirstUseEver
			);
			ImGui::SetNextWindowSize(
				ImVec2(m_width / 5.0f, m_height / 3.0f)
				, ImGuiCond_FirstUseEver
			);
			ImGui::Begin("Lights"
			             , nullptr
			             , 0);

			if (ImGui::Button(FowardRenderName))
			{
				if (m_renderer->GetName() != FowardRenderName)
				{
					m_renderer->OnDestroy();
					delete m_renderer;
					InitRenderer(FowardRenderName);
				}
			}

			if (ImGui::Button(DeferredRenderName))
			{
				if (m_renderer->GetName() != DeferredRenderName)
				{
					m_renderer->OnDestroy();
					delete m_renderer;
					InitRenderer(DeferredRenderName);
				}
			}
			if (ImGui::Button(FowardPlusRenderName))
			{
				if (m_renderer->GetName() != FowardPlusRenderName)
				{
					m_renderer->OnDestroy();
					delete m_renderer;
					InitRenderer(FowardPlusRenderName);
				}
			}


			int numLights = (int)m_scene->addLights.size();

			ImGui::SliderInt("Num lights", &numLights, 1, 2048);
			if (numLights != (int)m_scene->addLights.size())
			{
				m_scene->SetLightNum(numLights);
			}



			ImGui::End();

			float time = (float)((now - m_timeOffset) / freq);
			const uint32_t dim = 40;
			const float offset = (float(dim - 1) * 3.0f) * 0.5f;
			for (uint32_t index = 0; index < m_scene->addLights.size(); ++index)
			{
				Light* light = m_scene->addLights[index];
				float lightTime = time * m_lightAnimationSpeed * (bx::sin(
						index / float(m_scene->addLights.size()) * bx::kPiHalf) * 0.5f
					+ 0.5f);
				light->lightPosRadius[0] = bx::sin(((lightTime + index * 0.47f) + bx::kPiHalf * 1.37f)) * offset;
				light->lightPosRadius[1] = bx::cos(((lightTime + index * 0.69f) + bx::kPiHalf * 1.49f)) * offset;
				light->lightPosRadius[2] = bx::sin(((lightTime + index * 0.37f) + bx::kPiHalf * 1.57f)) * 2.0f;
			}
		}

		bool update() override
		{
			if (!processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState))
			{
				imguiBeginFrame(m_mouseState.m_mx
				                , m_mouseState.m_my
				                , (m_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
				                | (m_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
				                | (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				                , m_mouseState.m_mz
				                , uint16_t(m_width)
				                , uint16_t(m_height)
				);

				showExampleDialog(this);
				SceneUpdate();
				m_renderer->RenderScene(m_scene);

				return true;
			}

			return false;
		}

		Scene* m_scene;
		SceneRenderer* m_renderer;

		entry::MouseState m_mouseState;

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;

	private:
		int64_t m_timeOffset;
		float m_lightAnimationSpeed = 1;
	};
} // namespace

ENTRY_IMPLEMENT_MAIN(ExampleHelloWorld, "00-helloworld", "Initialization and debug text.");
