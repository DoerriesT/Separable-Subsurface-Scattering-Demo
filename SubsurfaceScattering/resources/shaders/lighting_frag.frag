#version 450

struct PushConsts
{
	mat4 viewProjectionMatrix;
	mat4 shadowMatrix;
};

layout(set = 0, binding = 0) uniform sampler2DShadow uShadowTexture;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(early_fragment_tests) in;

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;

layout(location = 0) out vec4 oColor;

// based on http://www.thetenthplanet.de/archives/1180
mat3 calculateTBN( vec3 N, vec3 p, vec2 uv )
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( p );
    vec3 dp2 = dFdy( p );
    vec2 duv1 = dFdx( uv );
    vec2 duv2 = dFdy( uv );
 
    // solve the linear system
    vec3 dp2perp = cross( dp2, N );
    vec3 dp1perp = cross( N, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * -invmax, B * -invmax, N );
}

float interleavedGradientNoise(vec2 v)
{
	vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	return fract(magic.z * dot(v, magic.xy));
}

vec2 vogelDiskSample(int sampleIndex, int samplesCount, float phi)
{
	const float goldenAngle = 2.4;
	
	float r = sqrt(sampleIndex + 0.5) / sqrt(samplesCount);
	float theta = sampleIndex * goldenAngle + phi;
	
	return r * vec2(cos(theta), sin(theta));
}

void main() 
{
	//vec3 normal = normalize(vNormal);
	//mat3 tbn = calculateTBN(normal, vWorldPos, vTexCoord);
	
	vec3 result = vec3(1.0);
	vec3 N = normalize(vNormal);
	vec4 shadowPos = uPushConsts.shadowMatrix * vec4(vWorldPos, 1.0);
	shadowPos.xyz /= shadowPos.w;
	shadowPos.xy = shadowPos.xy * 0.5 + 0.5;
	
	vec2 shadowTexelSize = 1.0 / vec2(textureSize(uShadowTexture, 0).xy);
	
	float shadow = 0.0;
	const float noise = interleavedGradientNoise(gl_FragCoord.xy);
	for (int i = 0; i < 16; ++i)
	{
		vec2 sampleOffset = vogelDiskSample(i, 16, noise);
		shadow += texture(uShadowTexture, vec3(shadowPos.xy + sampleOffset * shadowTexelSize * 5.5, shadowPos.z - 0.001)).x * (1.0 / 16.0);
	}
	
	result *= 1.0 - shadow;
	// ambient
	result += vec3(0.1);
	
	oColor = vec4(result, 1.0);
}

