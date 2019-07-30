#ifndef SCENE_H
#define SCENE_H
#include <vector>
#include "bgfx/bgfx.h"

struct Model;
struct Light;

class Scene
{
public:
	//Builds scene using a string to a JSON scene description file
	Scene();
	~Scene();


	void SetLightNum(int number);

	void DrawModelAsCube(int viewIndex, uint64_t state, bgfx::ProgramHandle program, int index);

	int GetModelCount() const
	{
		return models.size();
	}

	std::vector<Model*> models;

	Light* direction_light;

	std::vector<Light*> addLights;


private:
		bgfx::VertexBufferHandle m_vbh;
	bgfx::IndexBufferHandle m_ibh;

};

#endif
