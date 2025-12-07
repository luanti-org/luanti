uniform mat4 mWorld;
uniform vec3 dayLight;
uniform float animationTimer;
uniform lowp vec4 materialColor;

VARYING_ vec3 vNormal;
VARYING_ vec3 worldPosition;
VARYING_ lowp vec4 varColor;
CENTROID_ VARYING_ mediump vec2 varTexCoord;
CENTROID_ VARYING_ float varTexLayer; // actually int

#ifdef ENABLE_DYNAMIC_SHADOWS
	// shadow uniforms
	uniform vec3 v_LightDirection;
	uniform float f_textureresolution;
	uniform mat4 m_ShadowViewProj;
	uniform float f_shadowfar;
	uniform float f_shadow_strength;
	uniform float f_timeofday;
	uniform vec4 CameraPos;

	VARYING_ float cosLight;
	VARYING_ float adj_shadow_strength;
	VARYING_ float f_normal_length;
	VARYING_ vec3 shadow_position;
	VARYING_ float perspective_factor;
#endif

VARYING_ highp vec3 eyeVec;
VARYING_ float nightRatio;
VARYING_ vec3 sunTint;
// Color of the light emitted by the light sources.
uniform vec3 artificialLight;
VARYING_ float vIDiff;

#ifdef ENABLE_TINTED_SUNLIGHT
	uniform vec3 scattering_coefficients;
#endif

#ifdef ENABLE_DYNAMIC_SHADOWS
uniform float xyPerspectiveBias0;
uniform float xyPerspectiveBias1;
uniform float zPerspectiveBias;

vec4 getRelativePosition(in vec4 position)
{
	vec2 l = position.xy - CameraPos.xy;
	vec2 s = l / abs(l);
	s = (1.0 - s * CameraPos.xy);
	l /= s;
	return vec4(l, s);
}

float getPerspectiveFactor(in vec4 relativePosition)
{
	float pDistance = length(relativePosition.xy);
	float pFactor = pDistance * xyPerspectiveBias0 + xyPerspectiveBias1;
	return pFactor;
}

vec4 applyPerspectiveDistortion(in vec4 position)
{
	vec4 l = getRelativePosition(position);
	float pFactor = getPerspectiveFactor(l);
	l.xy /= pFactor;
	position.xy = l.xy * l.zw + CameraPos.xy;
	position.z *= zPerspectiveBias;
	return position;
}

#if __VERSION__ >= 130
#define mtsmoothstep smoothstep
#else
float mtsmoothstep(in float edge0, in float edge1, in float x)
{
	float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}
#endif

#endif


float directional_ambient(vec3 normal)
{
	vec3 v = normal * normal;

	if (normal.y < 0.0)
		return dot(v, vec3(0.670820, 0.447213, 0.836660));

	return dot(v, vec3(0.670820, 1.000000, 0.836660));
}

#ifdef ENABLE_TINTED_SUNLIGHT
	vec3 getDirectLightScatteringAtGround(vec3 v_LightDirection)
	{
		// Based on talk at 2002 Game Developers Conference by Naty Hoffman and Arcot J. Preetham
		const float unit_conversion = 1e-5; // Rayleigh scattering beta

		const float atmosphere_height = 15000.; // height of the atmosphere in meters
		// sun/moon light at the ground level, after going through the atmosphere
		return exp(-scattering_coefficients * unit_conversion * atmosphere_height / (1e-5 - dot(v_LightDirection, vec3(0., 1., 0.))));
	}
#endif

void main(void)
{
#ifdef USE_ARRAY_TEXTURE
	varTexLayer = inVertexAux;
#endif
	varTexCoord = (mTexture * vec4(inTexCoord0.xy, 1.0, 1.0)).st;

	gl_Position = mWorldViewProj * inVertexPosition;

	vNormal = (mWorld * vec4(inVertexNormal, 0.0)).xyz;
	worldPosition = (mWorld * inVertexPosition).xyz;
	eyeVec = -(mWorldView * inVertexPosition).xyz;

#if (MATERIAL_TYPE == TILE_MATERIAL_PLAIN) || (MATERIAL_TYPE == TILE_MATERIAL_PLAIN_ALPHA)
	vIDiff = 1.0;
#else
	// This is intentional comparison with zero without any margin.
	// If normal is not equal to zero exactly, then we assume it's a valid, just not normalized vector
	vIDiff = length(vNormal) == 0.0
		? 1.0
		: directional_ambient(normalize(vNormal));
#endif

	vec4 color = inVertexColor;

	color *= materialColor;

	// The alpha gives the ratio of sunlight in the incoming light.
	nightRatio = 1.0 - color.a;
	color.rgb = color.rgb * (color.a * dayLight.rgb +
		nightRatio * max(artificialLight.rgb, vec3(0.0))) * 2.0;
	color.a = 1.0;

	// Emphase blue a bit in darker places
	// See C++ implementation in mapblock_mesh.cpp final_color_blend()
	float brightness = (color.r + color.g + color.b) / 3.0;
	color.b += max(0.0, 0.021 - abs(0.2 * brightness - 0.021) +
		0.07 * brightness);

	varColor = clamp(color, 0.0, 1.0);


#ifdef ENABLE_DYNAMIC_SHADOWS
	if (f_shadow_strength > 0.0) {
		vec3 nNormal = normalize(vNormal);
		f_normal_length = length(vNormal);

		/* normalOffsetScale is in world coordinates (1/10th of a meter)
		   z_bias is in light space coordinates */
		float normalOffsetScale, z_bias;
		float pFactor = getPerspectiveFactor(getRelativePosition(m_ShadowViewProj * mWorld * inVertexPosition));
		if (f_normal_length > 0.0) {
			nNormal = normalize(vNormal);
			cosLight = max(1e-5, dot(nNormal, -v_LightDirection));
			float sinLight = pow(1.0 - pow(cosLight, 2.0), 0.5);
			normalOffsetScale = 0.1 * pFactor * pFactor * sinLight * min(f_shadowfar, 500.0) /
					xyPerspectiveBias1 / f_textureresolution;
			z_bias = 1e3 * sinLight / cosLight * (0.5 + f_textureresolution / 1024.0);
		}
		else {
			nNormal = vec3(0.0);
			cosLight = clamp(dot(v_LightDirection, normalize(vec3(v_LightDirection.x, 0.0, v_LightDirection.z))), 1e-2, 1.0);
			float sinLight = pow(1.0 - pow(cosLight, 2.0), 0.5);
			normalOffsetScale = 0.0;
			z_bias = 3.6e3 * sinLight / cosLight;
		}
		z_bias *= pFactor * pFactor / f_textureresolution / f_shadowfar;

		shadow_position = applyPerspectiveDistortion(m_ShadowViewProj * mWorld * (inVertexPosition + vec4(normalOffsetScale * nNormal, 0.0))).xyz;
		shadow_position.z -= z_bias;
		perspective_factor = pFactor;

		// The sun rises at 5:00 and sets at 19:00, which corresponds to 5/24=0.208 and 19/24 = 0.792.
		float nightFactor = 0.;
		sunTint = vec3(1.0);
		if (f_timeofday < 0.208) {
			adj_shadow_strength = f_shadow_strength * 0.5 *
				(1.0 - mtsmoothstep(0.178, 0.208, f_timeofday));
		} else if (f_timeofday >= 0.792) {
			adj_shadow_strength = f_shadow_strength * 0.5 *
				mtsmoothstep(0.792, 0.822, f_timeofday);
		} else {
			adj_shadow_strength = f_shadow_strength *
				mtsmoothstep(0.208, 0.238, f_timeofday) *
				(1.0 - mtsmoothstep(0.762, 0.792, f_timeofday));
			nightFactor = adj_shadow_strength / f_shadow_strength;
#ifdef ENABLE_TINTED_SUNLIGHT
			float tint_strength = 1.0 - mtsmoothstep(0.792, 0.5, f_timeofday) * mtsmoothstep(0.208, 0.5, f_timeofday);
			sunTint = mix(vec3(1.0), getDirectLightScatteringAtGround(v_LightDirection), nightFactor * tint_strength);
#endif
		}
	}
#endif
}
