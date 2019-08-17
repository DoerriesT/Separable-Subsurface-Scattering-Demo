#version 450

struct PushConsts
{
	mat4 invModelViewProjection;
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) out vec4 vRay;

void main() 
{
	float x = -1.0 + float((gl_VertexIndex & 1) << 2);
	float y = -1.0 + float((gl_VertexIndex & 2) << 1);
    gl_Position = vec4(x, y, 1.0, 1.0);
	vRay = uPushConsts.invModelViewProjection * vec4(gl_Position.xy, 1.0,  1.0);
}