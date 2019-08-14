#version 450

struct PushConsts
{
	mat4 viewProjectionMatrix;
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec3 inPosition;

void main() 
{
	gl_Position = uPushConsts.viewProjectionMatrix * vec4(inPosition, 1.0);
}

