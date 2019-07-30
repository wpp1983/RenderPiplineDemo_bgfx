#ifndef SCENERENDERER_H
#define SCENERENDERER_H
#include <string>

class Scene;

class SceneRenderer
{
public:
	virtual ~SceneRenderer() = default;
	virtual void Init() = 0;
	virtual void OnDestroy() = 0;
	virtual void RenderScene(Scene* scene) = 0;
	virtual std::string GetName() = 0;

	int m_width;
	int m_height;
};

#endif