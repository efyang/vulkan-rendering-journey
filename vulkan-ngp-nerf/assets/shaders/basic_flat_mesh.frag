#version 460

layout (location = 0) in vec3 inColor;
//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform SceneData {
	// stick to vec4 and mat4, avoid mixing dtypes, still need to pad
	vec4 fogColor;     // w is for exponent
	vec4 fogDistances; // x min y max, zw unused
	vec4 ambientColor;
	vec4 sunlightDirection; // w for sun power
	vec4 sunlightColor;
} sceneData;

void main()
{
	outFragColor = vec4(inColor + sceneData.ambientColor.xyz, 1.0f);
}
