#include "sceneRenderer_factory.h"
#include "sceneRenderer_foward.h"
#include "sceneRenderer_deferred.h"
#include "sceneRenderer_fowardPlus.h"

SceneRenderer* SceneRenderer_Factory::CreateSceneRenderer(const std::string Name)
{
	if (Name == "foward")
	{
		return new SceneRenderer_foward;
	}
	if (Name == "deferred")
	{
		return new SceneRenderer_deferred;
	}
	if (Name == "fowardPlus")
	{
		return new SceneRenderer_fowardPlus;
	}
}
