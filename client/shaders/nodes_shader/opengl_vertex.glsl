uniform mat4 mWorld;
// Color of the light emitted by the sun.
uniform vec3 dayLight;

// The cameraOffset is the current center of the visible world.
uniform highp vec3 cameraOffset;
uniform float animationTimer;

varying vec3 vNormal;
varying vec3 vPosition;
// World position in the visible world (i.e. relative to the cameraOffset.)
// This can be used for many shader effects without loss of precision.
// If the absolute position is required it can be calculated with
// cameraOffset + worldPosition (for large coordinates the limits of float
// precision must be considered).
varying vec3 worldPosition;
// The centroid keyword ensures that after interpolation the texture coordinates
// lie within the same bounds when MSAA is en- and disabled.
// This fixes the stripes problem with nearest-neighbor textures and MSAA.
#ifdef GL_ES
varying lowp vec4 varColor;
varying mediump vec2 varTexCoord;
varying float nightRatio;
#else
centroid varying lowp vec4 varColor;
centroid varying vec2 varTexCoord;
centroid varying float nightRatio;
#endif
#ifdef ENABLE_DYNAMIC_SHADOWS
	// shadow uniforms
	uniform vec3 v_LightDirection;
	uniform float f_textureresolution;
	uniform mat4 m_ShadowViewProj;
	uniform float f_shadowfar;
	uniform float f_shadow_strength;
	uniform float f_timeofday;
	uniform vec4 CameraPos;

	varying float cosLight;
	varying float normalOffsetScale;
	varying float adj_shadow_strength;
	varying float f_normal_length;
	varying vec3 shadow_position;
	varying float perspective_factor;
#endif

varying highp vec3 eyeVec;
// Color of the light emitted by the light sources.
const vec3 artificialLight = vec3(1.04, 1.04, 1.04);
const float e = 2.718281828459;
const float BS = 10.0;
uniform float xyPerspectiveBias0;
uniform float xyPerspectiveBias1;
uniform float zPerspectiveBias;

#ifdef ENABLE_DYNAMIC_SHADOWS

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

// custom smoothstep implementation because it's not defined in glsl1.2
// https://docs.gl/sl4/smoothstep
float mtsmoothstep(in float edge0, in float edge1, in float x)
{
	float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}
#endif


float smoothCurve(float x)
{
	return x * x * (3.0 - 2.0 * x);
}


float triangleWave(float x)
{
	return abs(fract(x + 0.5) * 2.0 - 1.0);
}


float smoothTriangleWave(float x)
{
	return smoothCurve(triangleWave(x)) * 2.0 - 1.0;
}

#if MATERIAL_WAVING_LIQUID && ENABLE_WAVING_WATER

//
// Simple, fast noise function.
// See: https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
//
vec4 perm(vec4 x)
{
	return mod(((x * 34.0) + 1.0) * x, 289.0);
}

float snoise(vec3 p)
{
	vec3 a = floor(p);
	vec3 d = p - a;
	d = d * d * (3.0 - 2.0 * d);

	vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
	vec4 k1 = perm(b.xyxy);
	vec4 k2 = perm(k1.xyxy + b.zzww);

	vec4 c = k2 + a.zzzz;
	vec4 k3 = perm(c);
	vec4 k4 = perm(c + 1.0);

	vec4 o1 = fract(k3 * (1.0 / 41.0));
	vec4 o2 = fract(k4 * (1.0 / 41.0));

	vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
	vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

	return o4.y * d.y + o4.x * (1.0 - d.y);
}

#endif




void main(void)
{
	varTexCoord = inTexCoord0.st;

	float disp_x;
	float disp_z;
// OpenGL < 4.3 does not support continued preprocessor lines
#if (MATERIAL_TYPE == TILE_MATERIAL_WAVING_LEAVES && ENABLE_WAVING_LEAVES) || (MATERIAL_TYPE == TILE_MATERIAL_WAVING_PLANTS && ENABLE_WAVING_PLANTS)
	vec4 pos2 = mWorld * inVertexPosition;
	float tOffset = (pos2.x + pos2.y) * 0.001 + pos2.z * 0.002;
	disp_x = (smoothTriangleWave(animationTimer * 23.0 + tOffset) +
		smoothTriangleWave(animationTimer * 11.0 + tOffset)) * 0.4;
	disp_z = (smoothTriangleWave(animationTimer * 31.0 + tOffset) +
		smoothTriangleWave(animationTimer * 29.0 + tOffset) +
		smoothTriangleWave(animationTimer * 13.0 + tOffset)) * 0.5;
#endif

	vec4 pos = inVertexPosition;
#if MATERIAL_WAVING_LIQUID && ENABLE_WAVING_WATER
	// Generate waves with Perlin-type noise.
	// The constants are calibrated such that they roughly
	// correspond to the old sine waves.
	vec3 wavePos = (mWorld * pos).xyz + cameraOffset;
	// The waves are slightly compressed along the z-axis to get
	// wave-fronts along the x-axis.
	wavePos.x /= WATER_WAVE_LENGTH * 3.0;
	wavePos.z /= WATER_WAVE_LENGTH * 2.0;
	wavePos.z += animationTimer * WATER_WAVE_SPEED * 10.0;
	pos.y += (snoise(wavePos) - 1.0) * WATER_WAVE_HEIGHT * 5.0;
#elif MATERIAL_TYPE == TILE_MATERIAL_WAVING_LEAVES && ENABLE_WAVING_LEAVES
	pos.x += disp_x;
	pos.y += disp_z * 0.1;
	pos.z += disp_z;
#elif MATERIAL_TYPE == TILE_MATERIAL_WAVING_PLANTS && ENABLE_WAVING_PLANTS
	if (varTexCoord.y < 0.05) {
		pos.x += disp_x;
		pos.z += disp_z;
	}
#endif
	worldPosition = (mWorld * pos).xyz;
	gl_Position = mWorldViewProj * pos;

	vPosition = gl_Position.xyz;
	eyeVec = -(mWorldView * pos).xyz;
#ifdef SECONDSTAGE
	normalPass = normalize((inVertexNormal+1)/2);
#endif
	vNormal = inVertexNormal;

	// Calculate color.
	vec4 color = inVertexColor;
	// Red, green and blue components are pre-multiplied with
	// the brightness, so now we have to multiply these
	// colors with the color of the incoming light.
	// The pre-baked colors are halved to prevent overflow.
	// The alpha gives the ratio of sunlight in the incoming light.
	nightRatio = 1.0 - color.a;
	color.rgb = color.rgb * (color.a * dayLight.rgb +
		nightRatio * artificialLight.rgb) * 2.0;
	color.a = 1.0;

	// Emphase blue a bit in darker places
	// See C++ implementation in mapblock_mesh.cpp final_color_blend()
	float brightness = (color.r + color.g + color.b) / 3.0;
	color.b += max(0.0, 0.021 - abs(0.2 * brightness - 0.021) +
		0.07 * brightness);

	varColor = clamp(color, 0.0, 1.0);

#ifdef ENABLE_DYNAMIC_SHADOWS
	if (f_shadow_strength > 0.0) {
#if MATERIAL_TYPE == TILE_MATERIAL_WAVING_PLANTS && ENABLE_WAVING_PLANTS
		// The shadow shaders don't apply waving when creating the shadow-map.
		// We are using the not waved inVertexPosition to avoid ugly self-shadowing.
		vec4 shadow_pos = inVertexPosition;
#else
		vec4 shadow_pos = pos;
#endif
		vec3 nNormal;
		f_normal_length = length(vNormal);

		/* normalOffsetScale is in world coordinates (1/10th of a meter)
		   z_bias is in light space coordinates */
		float normalOffsetScale, z_bias;
		float pFactor = getPerspectiveFactor(getRelativePosition(m_ShadowViewProj * mWorld * shadow_pos));
		if (f_normal_length > 0.0) {
			nNormal = normalize(vNormal);
			cosLight = max(1e-5, dot(nNormal, -v_LightDirection));
			float sinLight = pow(1.0 - pow(cosLight, 2.0), 0.5);
			normalOffsetScale = 2.0 * pFactor * pFactor * sinLight * min(f_shadowfar, 500.0) /
					xyPerspectiveBias1 / f_textureresolution;
			z_bias = 1.0 * sinLight / cosLight;
		}
		else {
			nNormal = vec3(0.0);
			cosLight = clamp(dot(v_LightDirection, normalize(vec3(v_LightDirection.x, 0.0, v_LightDirection.z))), 1e-2, 1.0);
			float sinLight = pow(1.0 - pow(cosLight, 2.0), 0.5);
			normalOffsetScale = 0.0;
			z_bias = 3.6e3 * sinLight / cosLight;
		}
		z_bias *= pFactor * pFactor / f_textureresolution / f_shadowfar;

		shadow_position = applyPerspectiveDistortion(m_ShadowViewProj * mWorld * (shadow_pos + vec4(normalOffsetScale * nNormal, 0.0))).xyz;
#if !defined(ENABLE_TRANSLUCENT_FOLIAGE) || MATERIAL_TYPE != TILE_MATERIAL_WAVING_LEAVES
		shadow_position.z -= z_bias;
#endif
		perspective_factor = pFactor;

		if (f_timeofday < 0.2) {
			adj_shadow_strength = f_shadow_strength * 0.5 *
				(1.0 - mtsmoothstep(0.18, 0.2, f_timeofday));
		} else if (f_timeofday >= 0.8) {
			adj_shadow_strength = f_shadow_strength * 0.5 *
				mtsmoothstep(0.8, 0.83, f_timeofday);
		} else {
			adj_shadow_strength = f_shadow_strength *
				mtsmoothstep(0.20, 0.25, f_timeofday) *
				(1.0 - mtsmoothstep(0.7, 0.8, f_timeofday));
		}
	}
#endif
}
