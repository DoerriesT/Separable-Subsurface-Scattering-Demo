#version 450

layout(set = 0, binding = 4) uniform samplerCube uSkybox;

layout(early_fragment_tests) in;

layout(location = 0) in vec4 vRay;

layout(location = 0) out vec4 oColor;

void main() 
{
	oColor = vec4(textureLod(uSkybox, vRay.xyz / vRay.w, 0.0).rgb, 1.0);
}
