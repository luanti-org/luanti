#define rendered texture0
#define depthmap texture1

uniform sampler2D rendered;
uniform sampler2D depthmap;

// Inverse of the current frame's view-projection matrix (used to reconstruct
// each pixel's world-space position from its depth).
uniform highp mat4 mInvViewProj;
// Previous frame's view-projection matrix, with the camera-offset delta baked
// in (used to find where this pixel was on the screen last frame).
uniform highp mat4 mPrevViewProj;
// User-facing strength multiplier for the effect.
uniform float motionBlurStrength;

CENTROID_ VARYING_ mediump vec2 varTexCoord;

// Number of samples taken along the velocity vector. Higher = smoother blur
// but more texture fetches.
const int NUM_SAMPLES = 8;

// Maximum blur length in UV units, so fast rotation / the sky don't smear the
// whole screen into mush.
const float MAX_VELOCITY = 0.15;

// --- Emissive ("lightsaber") smearing -------------------------------------
// Ordinary motion blur AVERAGES the samples, so a moving object's light is
// spread over many pixels and the smear looks dimmer than the object. Real
// bright light sources don't behave that way: as they sweep across the sensor
// each pixel they cross gets saturated with light, so the trail stays at full
// source brightness (a solid bright bar), which then blooms. We reproduce that
// by detecting emissive pixels and accumulating them with max() instead of
// averaging, so the streak does not dim along its length.
//
// Detection uses the value channel max(r,g,b) rather than luminance, so that
// saturated colored lights (e.g. a red glow, whose luminance is low) are still
// treated as emissive.

// Value at which a pixel starts to count as "emissive". Lower = more of the
// scene streaks brightly; higher = only the brightest sources do.
const float EMISSION_THRESHOLD = 0.70;

// How hot the emissive streak is pushed. >1 drives the trail toward full
// brightness (clamped by the buffer) so the following bloom pass halos it.
const float EMISSION_STRENGTH = 1.25;

void main(void)
{
	vec2 uv = varTexCoord.st;
	highp float rawDepth = texture2D(depthmap, uv).r;

	// Reconstruct the world-space (camera-relative) position of this pixel.
	highp vec4 ndc = vec4(uv * 2.0 - 1.0, rawDepth * 2.0 - 1.0, 1.0);
	highp vec4 worldPos = mInvViewProj * ndc;
	worldPos /= worldPos.w;

	// Reproject it through last frame's camera to find its previous screen pos.
	highp vec4 prevClip = mPrevViewProj * vec4(worldPos.xyz, 1.0);
	vec2 prevUv = (prevClip.xy / prevClip.w) * 0.5 + 0.5;

	// Screen-space velocity in UV units.
	vec2 velocity = (uv - prevUv) * motionBlurStrength;

	float len = length(velocity);
	if (len > MAX_VELOCITY)
		velocity *= MAX_VELOCITY / len;

	vec4 color = texture2D(rendered, uv);

	// Nothing (or barely anything) moved: skip the blur entirely.
	if (len < 0.0005) {
		gl_FragColor = vec4(color.rgb, 1.0);
		return;
	}

	// Per-pixel jitter of the sample offsets. Without it every pixel samples the
	// same fixed positions along the line, so a small bright source is stamped as
	// a few discrete ghost copies (the dashed gaps). Offsetting each pixel by a
	// sub-step random amount makes neighbouring pixels cover the positions in
	// between, so the emissive max() fills into a continuous streak. Free: no
	// extra texture fetches.
	highp float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);

	// Diffuse blur: energy-conserving average of the samples (the normal smear).
	vec3 sum = vec3(0.0);
	// Emissive trail: per-channel MAX of the emissive part of each sample, so a
	// bright source paints a full-brightness bar along its path instead of a
	// dimmed average.
	vec3 emissive = vec3(0.0);
	for (int i = 0; i < NUM_SAMPLES; i++) {
		// Spread samples symmetrically around the current pixel: t in [-0.5, 0.5],
		// jittered by a fraction of the step so the trail has no gaps.
		float t = (float(i) + jitter) / float(NUM_SAMPLES) - 0.5;
		vec2 samplePos = clamp(uv + velocity * t, vec2(0.0), vec2(1.0));
		vec3 c = texture2D(rendered, samplePos).rgb;
		sum += c;

		// How emissive this sample is, by its brightest channel (soft knee).
		float e = smoothstep(EMISSION_THRESHOLD, 1.0, max(c.r, max(c.g, c.b)));
		emissive = max(emissive, c * e);
	}

	vec3 base = sum / float(NUM_SAMPLES);
	// Where the trail is emissive, take the bright bar; elsewhere it is ~0 and
	// the normal averaged blur shows through unchanged.
	vec3 result = max(base, emissive * EMISSION_STRENGTH);
	gl_FragColor = vec4(result, 1.0);
}
