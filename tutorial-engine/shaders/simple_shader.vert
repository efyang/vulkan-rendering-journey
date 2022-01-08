#version 450

layout(location=0)in vec2 position;

vec3 barypos[3]=vec3[](
	vec3(0,0,1),
	vec3(0,1,0),
	vec3(1,0,0)
);
layout(location=1)out vec3 bary;

void main(){
	gl_Position=vec4(position,0.,1.);
	bary=barypos[gl_VertexIndex%3];
}
