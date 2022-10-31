#version 460

layout(location=0)in vec3 vPosition;
layout(location=1)in vec3 vNormal;
layout(location=2)in vec3 vColor;

layout(location=0)out vec3 outColor;

layout(set=0,binding=0)uniform CameraBuffer{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
}cameraData;

struct ObjectData{
	mat4 model;
};
layout(std140,set=1,binding=0)readonly buffer ObjectBuffer{
	ObjectData objects[];
}objectBuffer;

struct ObjectLightingData{
	vec4 objectAmbientLighting;
};
layout(std140,set=1,binding=1)readonly buffer ObjectLightingBuffer{
	ObjectLightingData objectLightings[];
}objectLightingBuffer;

layout(push_constant)uniform constants{
	vec4 data;
	mat4 render_matrix;
}PushConstants;

void main(){
	mat4 modelMatrix=objectBuffer.objects[gl_BaseInstance].model;
	mat4 transformMatrix=(cameraData.viewproj*modelMatrix);
	gl_Position=transformMatrix*vec4(vPosition,1.f);
	outColor=(vNormal+1)/2;
	outColor=(outColor+objectLightingBuffer.objectLightings[gl_BaseInstance].objectAmbientLighting.xyz)/2;
}