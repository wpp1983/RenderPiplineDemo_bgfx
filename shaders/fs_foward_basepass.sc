$input v_wpos, v_view, v_normal, v_tangent, v_bitangent, v_texcoord0// in...

/*
 * Copyright 2011-2019 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "common/common.sh"

SAMPLER2D(s_texColor,  0);
SAMPLER2D(s_texNormal, 1);
uniform vec4 u_lightPosRadius;
uniform vec4 u_lightRgbInnerR;



vec3 calcDirLight( mat3 _tbn, vec3 _wpos, vec3 _normal, vec3 _view)
{
	// 	vec3 lp = u_lightPosRadius[_idx].xyz - _wpos;
	// float attn = 1.0 - smoothstep(u_lightRgbInnerR[_idx].w, 1.0, length(lp) / u_lightPosRadius[_idx].w);
	// vec3 lightDir = mul( normalize(lp), _tbn );

	vec3 lp = u_lightPosRadius.xyz;
	float attn = 1.0;
	vec3 lightDir = normalize(u_lightPosRadius.xyz);

	
	vec2 bln = blinn(lightDir, _normal, _view);
	vec4 lc = lit(bln.x, bln.y, 1.0);
	vec3 rgb = u_lightRgbInnerR.xyz * saturate(lc.y) * attn;
	return rgb;
}

mat3 mtx3FromCols(vec3 c0, vec3 c1, vec3 c2)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	return mat3(c0, c1, c2);
#else
	return transpose(mat3(c0, c1, c2));
#endif
}


void main()
{
	mat3 tbn = mtx3FromCols(v_tangent, v_bitangent, v_normal);

	vec3 normal;
	normal.xy = texture2D(s_texNormal, v_texcoord0).xy * 2.0 - 1.0;
	normal.z = sqrt(1.0 - dot(normal.xy, normal.xy) );
	vec3 view = normalize(v_view);

	vec3 lightColor = 0.1;
	lightColor +=  calcDirLight( tbn, v_wpos, normal, view);

	vec4 color = toLinear(texture2D(s_texColor, v_texcoord0) );

	gl_FragColor.xyz =  lightColor.xyz * color.xyz;
	gl_FragColor.w = 1.0;
	gl_FragColor = toGamma(gl_FragColor);
}
