#version 450

struct PushConsts
{
	float exposure;
};

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput uInputTexture;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};


layout(location = 0) out vec4 oColor;

vec3 uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 accurateLinearToSRGB(in vec3 linearCol)
{
	vec3 sRGBLo = linearCol * 12.92;
	vec3 sRGBHi = (pow(abs(linearCol), vec3(1.0/2.4)) * 1.055) - 0.055;
	vec3 sRGB = mix(sRGBLo, sRGBHi, vec3(greaterThan(linearCol, vec3(0.0031308))));
	return sRGB;
}

void main() 
{
	vec3 result = subpassLoad(uInputTexture).rgb;
	
	result = uncharted2Tonemap(result * uPushConsts.exposure);
		
	vec3 whiteScale = 1.0/uncharted2Tonemap(vec3(11.2));
	result *= whiteScale;
	result = accurateLinearToSRGB(result);
	
	oColor = vec4(result, 1.0);
}

