#version 450

struct PushConsts
{
	mat4 viewProjectionMatrix;
	uint materialIndex;
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vWorldPos;
layout(location = 3) flat out uint vMaterialIndex;

void main() 
{
	gl_Position = uPushConsts.viewProjectionMatrix * vec4(inPosition, 1.0);
	
	vTexCoord = inTexCoord;
	vNormal = inNormal;
	vWorldPos = inPosition;
	vMaterialIndex = uPushConsts.materialIndex;
}

