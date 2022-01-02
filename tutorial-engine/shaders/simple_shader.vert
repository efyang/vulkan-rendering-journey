#version 450

vec2 positions[3]=vec2[](
	vec2(0.,-.5),
	vec2(.5,.5),
	vec2(-.5,.7)
);

vec3 barypos[3]=vec3[](
	vec3(0,0,1),
	vec3(0,1,0),
	vec3(1,0,0)
);
layout(location=1)out vec3 bary;

void main(){
	gl_Position=vec4(positions[gl_VertexIndex],0.,1.);
	bary=barypos[gl_VertexIndex];
}
