#version 460

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 texCoord;
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

layout(set=2,binding=0) uniform sampler2D tex1;

void main()
{
	vec3 color = texture(tex1,texCoord).xyz;
	outFragColor = vec4(color, 1.0f);
}
