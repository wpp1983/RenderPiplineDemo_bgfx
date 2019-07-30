#ifndef SCENERENDERER_FACTORY_H
#define SCENERENDERER_FACTORY_H

#include "sceneRenderer.h"
#include <string>

class SceneRenderer_Factory
{
public:
	static SceneRenderer* CreateSceneRenderer(const std::string Name);
};

#endif