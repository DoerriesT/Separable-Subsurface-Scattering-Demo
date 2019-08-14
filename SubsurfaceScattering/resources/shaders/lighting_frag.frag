#version 450

layout(early_fragment_tests) in;

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) flat in uint vMaterialIndex;

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

void main() 
{
	vec3 normal = normalize(vNormal);
	//mat3 tbn = calculateTBN(normal, vWorldPos, vTexCoord);
	oColor = vec4(normal * 2.0 - 1.0, 1.0);
}

