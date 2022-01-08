#version 450

#define PI 3.1415926538

layout (location = 0) in vec3 fragColor;
layout (location = 0) out vec4 outColor;

void main() {
	outColor = vec4(fragColor, 1.0);
}