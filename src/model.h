#ifndef MODEL_H
#define MODEL_H

#include "bgfx_utils.h"

struct Model
{
	bx::Vec3 pos;
	bx::Vec3 scale;
	bx::Vec3 rot;
	
	Mesh* mesh;

};


#endif