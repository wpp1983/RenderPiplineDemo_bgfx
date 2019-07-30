$input v_wpos, v_view, v_posVS, v_normal, v_tangent, v_bitangent, v_texcoord0// in...

/*
 * Copyright 2011-2019 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "common/common.sh"
#include "bgfx_compute.sh"

SAMPLER2D(s_texColor,  0);
SAMPLER2D(s_texNormal, 1);

struct PointLight{
    vec4 position;
    vec4 color;
    float intensity;
    float range;
};

BUFFER_RO(lightGridOffset, uint, 2);
BUFFER_RO(lightGridCount, uint, 3);
BUFFER_RO(globalLightIndexList, uint, 4);
BUFFER_RO(pointLight_position, vec4, 5);
BUFFER_RO(pointLight_color, vec4, 6);

uniform vec4 cameraPlane;
uniform vec4 tileSize;
uniform vec4 GridDim;


mat3 mtx3FromCols(vec3 c0, vec3 c1, vec3 c2)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	return mat3(c0, c1, c2);
#else
	return transpose(mat3(c0, c1, c2));
#endif
}

float linearDepth(float depthSample){
    float depthRange = 2.0 * depthSample - 1.0;
	float zNear = cameraPlane[0];
	float zFar = cameraPlane[1];
    // Near... Far... wherever you are...
    float linearValue = 2.0 * zNear * zFar / (zFar + zNear - depthRange * (zFar - zNear));
    return linearValue;
}

int3 ComputeClusterIndex3D( float2 screenPos, float viewZ )
{
	float zNear = cameraPlane[0];
    int i = screenPos.x  / tileSize[0];
    int j = screenPos.y  / tileSize[1];
    // It is assumed that view space z is negative (right-handed coordinate system)
    // so the view-space z coordinate needs to be negated to make it positive.
	uint k = log( viewZ / zNear ) * tileSize[3];
	//int k = (viewZ - zNear) / tileSize[3];

    return int3( i, j, k );
}

int ComputeClusterIndex1D( int3 clusterIndex3D )
{
    return clusterIndex3D.x + 
	 GridDim.x * ( clusterIndex3D.y + GridDim.y * clusterIndex3D.z ) ;
	    //  return clusterIndex3D.x + GridDim.x * ( clusterIndex3D.y  ) ;
}

void main()
{

	mat3 tbn = mtx3FromCols(v_tangent, v_bitangent, v_normal);

	vec3 normal;
	normal.xy = texture2D(s_texNormal, v_texcoord0).xy * 2.0 - 1.0;
	normal.z = sqrt(1.0 - dot(normal.xy, normal.xy) );

	vec3 view = normalize(v_view);


   int3 clusterIndex3D = ComputeClusterIndex3D( gl_FragCoord.xy, v_posVS.z );
    int clusterIndex1D = ComputeClusterIndex1D( clusterIndex3D );

	
	int lightCount       = lightGridCount[clusterIndex1D];
    int lightIndexOffset = lightGridOffset[clusterIndex1D];

	 vec3 lightColor = 0.1;
	//Reading from the global light list and calculating the radiance contribution of each light.
    for(uint i = 0; i < lightCount; i++){
        uint lightVectorIndex = globalLightIndexList[lightIndexOffset + i];
		vec3 position = pointLight_position[lightVectorIndex].xyz;
    	vec3 color    = pointLight_color[lightVectorIndex].rgb;
    	float radius  = pointLight_position[lightVectorIndex].w;
        lightColor += calcLight(v_wpos, normal, view, position, radius, color, 1);
		//lightColor += 0.02;
    }

	vec4 color = toLinear(texture2D(s_texColor, v_texcoord0) );

	gl_FragColor.xyz =  lightColor.xyz * color.xyz;
	gl_FragColor.w = 1.0;
	gl_FragColor = toGamma(gl_FragColor);
}
