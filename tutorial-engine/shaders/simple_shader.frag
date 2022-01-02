#version 450

#define PI 3.1415926538

layout (location = 0) out vec4 outColor;
layout (location=1)in vec3 bary;

void main() {
	outColor = vec4(bary, 1.0);
}