#version 450

#define PI (3.14159265359)

struct PushConsts
{
	mat4 viewProjectionMatrix;
	mat4 shadowMatrix;
};

layout(set = 0, binding = 0) uniform sampler2D uTextures[5];
layout(set = 0, binding = 1) uniform sampler2D uBrdfLUT;
layout(set = 0, binding = 2) uniform samplerCube uRadianceTexture;
layout(set = 0, binding = 3) uniform samplerCube uIrradianceTexture;

layout(set = 1, binding = 0) uniform CONSTANTS
{
	vec4 lightPositionRadius;
	vec4 lightColorInvSqrAttRadius;
	vec4 cameraPosition;
} uConsts;

layout(set = 1, binding = 1) uniform sampler2DShadow uShadowTexture;

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
    return mat3( T * -invmax, B * invmax, N );
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

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a2 = roughness*roughness;
    a2 *= a2;
    float NdotH2 = max(dot(N, H), 0.0);
    NdotH2 *= NdotH2;

    float nom   = a2;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;

    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
	float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float ggx2 =  NdotV / max(NdotV * (1.0 - k) + k, 0.0000001);
    float ggx1 = NdotL / max(NdotL * (1.0 - k) + k, 0.0000001);

    return ggx1 * ggx2;
}


vec3 fresnelSchlick(float HdotV, vec3 F0)
{
	float power = (-5.55473 * HdotV - 6.98316) * HdotV;
	return F0 + (1.0 - F0) * pow(2.0, power);
}

float smoothDistanceAtt(float squaredDistance, float invSqrAttRadius)
{
	float factor = squaredDistance * invSqrAttRadius;
	float smoothFactor = clamp(1.0 - factor * factor, 0.0, 1.0);
	return smoothFactor * smoothFactor;
}

float getDistanceAtt(vec3 unnormalizedLightVector, float invSqrAttRadius)
{
	float sqrDist = dot(unnormalizedLightVector, unnormalizedLightVector);
	float attenuation = 1.0 / (max(sqrDist, invSqrAttRadius));
	attenuation *= smoothDistanceAtt(sqrDist, invSqrAttRadius);
	
	return attenuation;
}

vec3 cookTorranceSpecularBrdf(vec3 radiance, vec3 L, vec3 V, vec3 N, vec3 F0, vec3 albedo, float roughness)
{
	const vec3 H = normalize(V + L);
	const float NdotL = max(dot(N, L), 0.0);
	const float NdotV = max(dot(N, V), 0.0);
	
	// Cook-Torrance BRDF
	const float NDF = DistributionGGX(N, H, roughness);
	const float G = GeometrySmith(NdotV, NdotL, roughness);
	const vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
	
	const vec3 numerator = NDF * G * F;
	const float denominator = max(4.0 * NdotV * NdotL, 1e-6);

	const vec3 specular = numerator * (1.0 / denominator);
	
	// because of energy conversion kD and kS must add up to 1.0.
	const vec3 kD = (vec3(1.0) - F);

	return (kD * albedo * (1.0 / PI) + specular) * radiance * NdotL;
}

vec3 accurateSRGBToLinear(in vec3 sRGBCol)
{
	vec3 linearRGBLo = sRGBCol * (1.0 / 12.92);
	vec3 linearRGBHi = pow((sRGBCol + vec3(0.055)) * vec3(1.0 / 1.055), vec3(2.4));
	vec3 linearRGB = mix(linearRGBLo, linearRGBHi, vec3(greaterThan(sRGBCol, vec3(0.04045))));
	return linearRGB;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}  

void main() 
{
	vec3 N = normalize(vNormal);
	mat3 tbn = calculateTBN(N, vWorldPos, vTexCoord);
	
	vec3 tangentSpaceNormal = texture(uTextures[1], vTexCoord).xyz * 2.0 - 1.0;
	N = normalize(tbn * tangentSpaceNormal);
	

	const vec3 unnormalizedLightVector = uConsts.lightPositionRadius.xyz - vWorldPos;
	const vec3 L = normalize(unnormalizedLightVector);
	const float att = getDistanceAtt(unnormalizedLightVector, uConsts.lightColorInvSqrAttRadius.w);
	const vec3 radiance = uConsts.lightColorInvSqrAttRadius.rgb * att;
	
	const vec3 V = normalize(uConsts.cameraPosition.xyz - vWorldPos);
	const vec3 albedo = accurateSRGBToLinear(texture(uTextures[0], vTexCoord).rgb);
	const float roughness = 1.0 - texture(uTextures[2], vTexCoord).x * 0.638;
	vec3 F0 = texture(uTextures[3], vTexCoord).rgb * 0.272 * texture(uTextures[4], vTexCoord).x;
	
	vec3 result = cookTorranceSpecularBrdf(radiance, L, V, N, F0, albedo, roughness);
	
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
	
	// ambient lighting (we now use IBL as the ambient term)
    const vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    
    const vec3 kS = F;
    const vec3 kD = 1.0 - kS;
    
    const vec3 irradiance = texture(uIrradianceTexture, N).rgb;
    const vec3 diffuse = irradiance * albedo;
    
    // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
    const float MAX_REFLECTION_LOD = 4.0;
    const vec3 prefilteredColor = textureLod(uRadianceTexture, reflect(-V, N), roughness * MAX_REFLECTION_LOD).rgb;    
    const vec2 brdf = textureLod(uBrdfLUT, vec2(max(dot(N, V), 0.0), roughness), 0.0).rg;
    const vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    const vec3 ambient = (kD * diffuse + specular);
    
	result += ambient;
	
	oColor = vec4(result, 1.0);
}

