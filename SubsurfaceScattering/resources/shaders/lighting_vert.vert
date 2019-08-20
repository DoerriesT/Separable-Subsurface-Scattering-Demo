#version 450

layout(set = 1, binding = 0) uniform CONSTANTS
{
	mat4 viewProjectionMatrix;
	mat4 shadowMatrix;
	vec4 lightPositionRadius;
	vec4 lightColorInvSqrAttRadius;
	vec4 cameraPosition;
} uConsts;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vWorldPos;

void main() 
{
	gl_Position = uConsts.viewProjectionMatrix * vec4(inPosition, 1.0);
	
	vTexCoord = inTexCoord;
	vNormal = inNormal;
	vWorldPos = inPosition;
}

