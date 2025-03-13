uniform sampler2D baseTexture;

uniform vec3 dayLight;
uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;

// The cameraOffset is the current center of the visible world.
uniform highp vec3 cameraOffset;
uniform float animationTimer;
#ifdef ENABLE_DYNAMIC_SHADOWS
	// shadow texture
	uniform sampler2D ShadowMapSampler;
	// shadow uniforms
	uniform vec3 v_LightDirection;
	uniform float f_textureresolution;
	uniform mat4 m_ShadowViewProj;
	uniform float f_shadowfar;
	uniform float f_shadow_strength;
	uniform vec4 CameraPos;
	uniform float xyPerspectiveBias0;
	uniform float xyPerspectiveBias1;
	uniform vec3 shadow_tint;

	varying float adj_shadow_strength;
	varying float cosLight;
	varying float f_normal_length;
	varying vec3 shadow_position;
	varying float perspective_factor;
#endif


varying vec3 vNormal;
varying vec3 vPosition;
// World position in the visible world (i.e. relative to the cameraOffset.)
// This can be used for many shader effects without loss of precision.
// If the absolute position is required it can be calculated with
// cameraOffset + worldPosition (for large coordinates the limits of float
// precision must be considered).
varying vec3 worldPosition;
varying lowp vec4 varColor;
#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif
varying highp vec3 eyeVec;
varying float nightRatio;

varying float vIDiff;

#ifdef ENABLE_DYNAMIC_SHADOWS

// assuming near is always 1.0
float getLinearDepth()
{
	return 2.0 * f_shadowfar / (f_shadowfar + 1.0 - (2.0 * gl_FragCoord.z - 1.0) * (f_shadowfar - 1.0));
}

vec3 getLightSpacePosition()
{
	return shadow_position * 0.5 + 0.5;
}
// custom smoothstep implementation because it's not defined in glsl1.2
// https://docs.gl/sl4/smoothstep
float mtsmoothstep(in float edge0, in float edge1, in float x)
{
	float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

#ifdef COLORED_SHADOWS

// c_precision of 128 fits within 7 base-10 digits
const float c_precision = 128.0;
const float c_precisionp1 = c_precision + 1.0;

float packColor(vec3 color)
{
	return floor(color.b * c_precision + 0.5)
		+ floor(color.g * c_precision + 0.5) * c_precisionp1
		+ floor(color.r * c_precision + 0.5) * c_precisionp1 * c_precisionp1;
}

vec3 unpackColor(float value)
{
	vec3 color;
	color.b = mod(value, c_precisionp1) / c_precision;
	color.g = mod(floor(value / c_precisionp1), c_precisionp1) / c_precision;
	color.r = floor(value / (c_precisionp1 * c_precisionp1)) / c_precision;
	return color;
}

vec4 getHardShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	vec4 texDepth = texture2D(shadowsampler, smTexCoord.xy).rgba;

	float visibility = step(0.0, realDistance - texDepth.r);
	vec4 result = vec4(visibility, vec3(0.0,0.0,0.0));//unpackColor(texDepth.g));
	if (visibility < 0.1) {
		visibility = step(0.0, realDistance - texDepth.b);
		result = vec4(visibility, unpackColor(texDepth.a));
	}
	return result;
}

#else

float getHardShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	float texDepth = texture2D(shadowsampler, smTexCoord.xy).r;
	float visibility = step(0.0, realDistance - texDepth);
	return visibility;
}

#endif


#if SHADOW_FILTER == 2
	#define PCFBOUND 2.0 // 5x5
	#define PCFSAMPLES 25
#elif SHADOW_FILTER == 1
	#define PCFBOUND 1.0 // 3x3
	#define PCFSAMPLES 9
#else
	#define PCFBOUND 0.0
	#define PCFSAMPLES 1
#endif

#ifdef COLORED_SHADOWS
float getHardShadowDepth(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	vec4 texDepth = texture2D(shadowsampler, smTexCoord.xy);
	float depth = max(realDistance - texDepth.r, realDistance - texDepth.b);
	return depth;
}
#else
float getHardShadowDepth(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	float texDepth = texture2D(shadowsampler, smTexCoord.xy).r;
	float depth = realDistance - texDepth;
	return depth;
}
#endif

#define BASEFILTERRADIUS 1.0

float getPenumbraRadius(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	// Return fast if sharp shadows are requested
	if (PCFBOUND == 0.0 || SOFTSHADOWRADIUS <= 0.0)
		return 0.0;

	vec2 clampedpos;
	float y, x;
	float depth = getHardShadowDepth(shadowsampler, smTexCoord.xy, realDistance);
	// A factor from 0 to 1 to reduce blurring of short shadows
	float sharpness_factor = 1.0;
	// conversion factor from shadow depth to blur radius
	float depth_to_blur = f_shadowfar / SOFTSHADOWRADIUS / xyPerspectiveBias0;
	if (depth > 0.0 && f_normal_length > 0.0)
		// 5 is empirical factor that controls how fast shadow loses sharpness
		sharpness_factor = clamp(5 * depth * depth_to_blur, 0.0, 1.0);
	depth = 0.0;

	float world_to_texture = xyPerspectiveBias1 / perspective_factor / perspective_factor
			* f_textureresolution / 2.0 / f_shadowfar;
	float world_radius = 0.2; // shadow blur radius in world float coordinates, e.g. 0.2 = 0.02 of one node

	return max(BASEFILTERRADIUS * f_textureresolution / 4096.0,  sharpness_factor * world_radius * world_to_texture * SOFTSHADOWRADIUS);
}

#ifdef POISSON_FILTER
const vec2[64] poissonDisk = vec2[64](
	vec2(0.170019, -0.040254),
	vec2(-0.299417, 0.791925),
	vec2(0.645680, 0.493210),
	vec2(-0.651784, 0.717887),
	vec2(0.421003, 0.027070),
	vec2(-0.817194, -0.271096),
	vec2(-0.705374, -0.668203),
	vec2(0.977050, -0.108615),
	vec2(0.063326, 0.142369),
	vec2(0.203528, 0.214331),
	vec2(-0.667531, 0.326090),
	vec2(-0.098422, -0.295755),
	vec2(-0.885922, 0.215369),
	vec2(0.566637, 0.605213),
	vec2(0.039766, -0.396100),
	vec2(0.751946, 0.453352),
	vec2(0.078707, -0.715323),
	vec2(-0.075838, -0.529344),
	vec2(0.724479, -0.580798),
	vec2(0.222999, -0.215125),
	vec2(-0.467574, -0.405438),
	vec2(-0.248268, -0.814753),
	vec2(0.354411, -0.887570),
	vec2(0.175817, 0.382366),
	vec2(0.487472, -0.063082),
	vec2(0.355476, 0.025357),
	vec2(-0.084078, 0.898312),
	vec2(0.488876, -0.783441),
	vec2(0.470016, 0.217933),
	vec2(-0.696890, -0.549791),
	vec2(-0.149693, 0.605762),
	vec2(0.034211, 0.979980),
	vec2(0.503098, -0.308878),
	vec2(-0.016205, -0.872921),
	vec2(0.385784, -0.393902),
	vec2(-0.146886, -0.859249),
	vec2(0.643361, 0.164098),
	vec2(0.634388, -0.049471),
	vec2(-0.688894, 0.007843),
	vec2(0.464034, -0.188818),
	vec2(-0.440840, 0.137486),
	vec2(0.364483, 0.511704),
	vec2(0.034028, 0.325968),
	vec2(0.099094, -0.308023),
	vec2(0.693960, -0.366253),
	vec2(0.678884, -0.204688),
	vec2(0.001801, 0.780328),
	vec2(0.145177, -0.898984),
	vec2(0.062655, -0.611866),
	vec2(0.315226, -0.604297),
	vec2(-0.780145, 0.486251),
	vec2(-0.371868, 0.882138),
	vec2(0.200476, 0.494430),
	vec2(-0.494552, -0.711051),
	vec2(0.612476, 0.705252),
	vec2(-0.578845, -0.768792),
	vec2(-0.772454, -0.090976),
	vec2(0.504440, 0.372295),
	vec2(0.155736, 0.065157),
	vec2(0.391522, 0.849605),
	vec2(-0.620106, -0.328104),
	vec2(0.789239, -0.419965),
	vec2(-0.545396, 0.538133),
	vec2(-0.178564, -0.596057)
);

#ifdef COLORED_SHADOWS

vec4 getShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	float radius = getPenumbraRadius(shadowsampler, smTexCoord, realDistance);
	if (radius < 0.1) {
		// we are in the middle of even brightness, no need for filtering
		return getHardShadowColor(shadowsampler, smTexCoord.xy, realDistance);
	}

	vec2 clampedpos;
	vec4 visibility = vec4(0.0);
	float scale_factor = radius / f_textureresolution;

	int samples = (1 + 1 * int(SOFTSHADOWRADIUS > 1.0)) * PCFSAMPLES; // scale max samples for the soft shadows
	samples = int(clamp(pow(4.0 * radius + 1.0, 2.0), 1.0, float(samples)));
	int init_offset = int(floor(mod(((smTexCoord.x * 34.0) + 1.0) * smTexCoord.y, 64.0-samples)));
	int end_offset = int(samples) + init_offset;

	for (int x = init_offset; x < end_offset; x++) {
		clampedpos = poissonDisk[x] * scale_factor + smTexCoord.xy;
		visibility += getHardShadowColor(shadowsampler, clampedpos.xy, realDistance);
	}

	return visibility / samples;
}

#else

float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	float radius = getPenumbraRadius(shadowsampler, smTexCoord, realDistance);
	if (radius < 0.1) {
		// we are in the middle of even brightness, no need for filtering
		return getHardShadow(shadowsampler, smTexCoord.xy, realDistance);
	}

	vec2 clampedpos;
	float visibility = 0.0;
	float scale_factor = radius / f_textureresolution;

	int samples = (1 + 1 * int(SOFTSHADOWRADIUS > 1.0)) * PCFSAMPLES; // scale max samples for the soft shadows
	samples = int(clamp(pow(4.0 * radius + 1.0, 2.0), 1.0, float(samples)));
	int init_offset = int(floor(mod(((smTexCoord.x * 34.0) + 1.0) * smTexCoord.y, 64.0-samples)));
	int end_offset = int(samples) + init_offset;

	for (int x = init_offset; x < end_offset; x++) {
		clampedpos = poissonDisk[x] * scale_factor + smTexCoord.xy;
		visibility += getHardShadow(shadowsampler, clampedpos.xy, realDistance);
	}

	return visibility / samples;
}

#endif

#else
/* poisson filter disabled */

#ifdef COLORED_SHADOWS

vec4 getShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	float radius = getPenumbraRadius(shadowsampler, smTexCoord, realDistance);
	if (radius < 0.1) {
		// we are in the middle of even brightness, no need for filtering
		return getHardShadowColor(shadowsampler, smTexCoord.xy, realDistance);
	}

	vec2 clampedpos;
	vec4 visibility = vec4(0.0);
	float x, y;
	float bound = (1 + 0.5 * int(SOFTSHADOWRADIUS > 1.0)) * PCFBOUND; // scale max bound for soft shadows
	bound = clamp(0.5 * (4.0 * radius - 1.0), 0.5, bound);
	float scale_factor = radius / bound / f_textureresolution;
	float n = 0.0;

	// basic PCF filter
	for (y = -bound; y <= bound; y += 1.0)
	for (x = -bound; x <= bound; x += 1.0) {
		clampedpos = vec2(x,y) * scale_factor + smTexCoord.xy;
		visibility += getHardShadowColor(shadowsampler, clampedpos.xy, realDistance);
		n += 1.0;
	}

	return visibility / max(n, 1.0);
}

#else
float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	float radius = getPenumbraRadius(shadowsampler, smTexCoord, realDistance);
	if (radius < 0.1) {
		// we are in the middle of even brightness, no need for filtering
		return getHardShadow(shadowsampler, smTexCoord.xy, realDistance);
	}

	vec2 clampedpos;
	float visibility = 0.0;
	float x, y;
	float bound = (1 + 0.5 * int(SOFTSHADOWRADIUS > 1.0)) * PCFBOUND; // scale max bound for soft shadows
	bound = clamp(0.5 * (4.0 * radius - 1.0), 0.5, bound);
	float scale_factor = radius / bound / f_textureresolution;
	float n = 0.0;

	// basic PCF filter
	for (y = -bound; y <= bound; y += 1.0)
	for (x = -bound; x <= bound; x += 1.0) {
		clampedpos = vec2(x,y) * scale_factor + smTexCoord.xy;
		visibility += getHardShadow(shadowsampler, clampedpos.xy, realDistance);
		n += 1.0;
	}

	return visibility / max(n, 1.0);
}

#endif

#endif
#endif


void main(void)
{
	vec3 color;
	vec2 uv = varTexCoord.st;

	vec4 base = texture2D(baseTexture, uv).rgba;
	// If alpha is zero, we can just discard the pixel. This fixes transparency
	// on GPUs like GC7000L, where GL_ALPHA_TEST is not implemented in mesa,
	// and also on GLES 2, where GL_ALPHA_TEST is missing entirely.
#ifdef USE_DISCARD
	if (base.a == 0.0)
		discard;
#endif
#ifdef USE_DISCARD_REF
	if (base.a < 0.5)
		discard;
#endif

	color = base.rgb;
	vec4 col = vec4(color.rgb * varColor.rgb, 1.0);
	col.rgb *= vIDiff;

#ifdef ENABLE_DYNAMIC_SHADOWS
	if (f_shadow_strength > 0.0) {
		float shadow_int = 0.0;
		vec3 shadow_color = vec3(0.0, 0.0, 0.0);
		vec3 posLightSpace = getLightSpacePosition();

		float distance_rate = (1.0 - pow(clamp(2.0 * length(posLightSpace.xy - 0.5),0.0,1.0), 10.0));
		if (max(abs(posLightSpace.x - 0.5), abs(posLightSpace.y - 0.5)) > 0.5)
			distance_rate = 0.0;
		float f_adj_shadow_strength = max(adj_shadow_strength - mtsmoothstep(0.9, 1.1, posLightSpace.z),0.0);

		if (distance_rate > 1e-7) {

#ifdef COLORED_SHADOWS
			vec4 visibility;
			if (cosLight > 0.0 || f_normal_length < 1e-3)
				visibility = getShadowColor(ShadowMapSampler, posLightSpace.xy, posLightSpace.z);
			else
				visibility = vec4(1.0, 0.0, 0.0, 0.0);
			shadow_int = visibility.r;
			shadow_color = visibility.gba;
#else
			if (cosLight > 0.0 || f_normal_length < 1e-3)
				shadow_int = getShadow(ShadowMapSampler, posLightSpace.xy, posLightSpace.z);
			else
				shadow_int = 1.0;
#endif
			shadow_int *= distance_rate;
			shadow_int = clamp(shadow_int, 0.0, 1.0);

		}

		// turns out that nightRatio falls off much faster than
		// actual brightness of artificial light in relation to natual light.
		// Power ratio was measured on torches in MTG (brightness = 14).
		float adjusted_night_ratio = pow(max(0.0, nightRatio), 0.6);

		// Apply self-shadowing when light falls at a narrow angle to the surface
		// Cosine of the cut-off angle.
		const float self_shadow_cutoff_cosine = 0.14;
		if (f_normal_length != 0 && cosLight < self_shadow_cutoff_cosine) {
			shadow_int = max(shadow_int, 1 - clamp(cosLight, 0.0, self_shadow_cutoff_cosine)/self_shadow_cutoff_cosine);
			shadow_color = mix(vec3(0.0), shadow_color, min(cosLight, self_shadow_cutoff_cosine)/self_shadow_cutoff_cosine);
		}

		shadow_int *= f_adj_shadow_strength;

		// calculate fragment color from components:
		col.rgb =
				adjusted_night_ratio * col.rgb + // artificial light
				(1.0 - adjusted_night_ratio) * ( // natural light
						col.rgb * (1.0 - shadow_int * (1.0 - shadow_color) * (1.0 - shadow_tint)) +  // filtered texture color
						dayLight * shadow_color * shadow_int);                 // reflected filtered sunlight/moonlight
	}
#endif

	// Due to a bug in some (older ?) graphics stacks (possibly in the glsl compiler ?),
	// the fog will only be rendered correctly if the last operation before the
	// clamp() is an addition. Else, the clamp() seems to be ignored.
	// E.g. the following won't work:
	//      float clarity = clamp(fogShadingParameter
	//		* (fogDistance - length(eyeVec)) / fogDistance), 0.0, 1.0);
	// As additions usually come for free following a multiplication, the new formula
	// should be more efficient as well.
	// Note: clarity = (1 - fogginess)
	float clarity = clamp(fogShadingParameter
		- fogShadingParameter * length(eyeVec) / fogDistance, 0.0, 1.0);
	float fogColorMax = max(max(fogColor.r, fogColor.g), fogColor.b);
	// Prevent zero division.
	if (fogColorMax < 0.0000001) fogColorMax = 1.0;
	// For high clarity (light fog) we tint the fog color.
	// For this to not make the fog color artificially dark we need to normalize using the
	// fog color's brightest value. We then blend our base color with this to make the fog.
	col = mix(fogColor * pow(fogColor / fogColorMax, vec4(2.0 * clarity)), col, clarity);
	col = vec4(col.rgb, base.a);

	gl_FragData[0] = col;
}
