#define rendered texture0
#define depthmap texture1

uniform sampler2D rendered;
uniform sampler2D depthmap;

CENTROID_ VARYING_ mediump vec2 varTexCoord;

// Reconstructing a world-space position from the depth buffer needs high
// floating-point precision. On OpenGL ES highp is only optionally supported in
// the fragment stage, and a shader that uses it where it is unsupported fails
// to compile outright (the second_stage shader disables dithering for the same
// reason). Where it is missing, compile a passthrough instead of the effect.
#if defined(GL_ES) && !defined(GL_FRAGMENT_PRECISION_HIGH)
#define MOTION_BLUR_UNSUPPORTED 1
#endif

#ifdef MOTION_BLUR_UNSUPPORTED

void main(void)
{
	gl_FragColor = vec4(texture2D(rendered, varTexCoord.st).rgb, 1.0);
}

#else

#ifdef ENABLE_AUTO_EXPOSURE
#define exposureMap texture2
// 1x1 auto-exposure texture (log2 scale), same one the second stage reads. The
// engine only binds this when auto exposure is enabled.
uniform sampler2D exposureMap;
#endif

// Inverse of the current frame's view-projection matrix (used to reconstruct
// each pixel's world-space position from its depth).
uniform highp mat4 mInvViewProj;
// Previous frame's view-projection matrix, with the camera-offset delta baked
// in (used to find where this pixel was on the screen last frame).
uniform highp mat4 mPrevViewProj;
// User-facing strength multiplier for the effect.
uniform float motionBlurStrength;

// How many samples to take along the velocity vector. Higher = smoother, less
// banded blur but more texture fetches.
//
// This arrives as a compile-time constant from the `motion_blur_quality`
// setting rather than as a uniform, so the loop bound below is a literal the
// driver can unroll. That is also why changing the setting needs the world
// reloaded: it selects a different shader program. The engine clamps the value
// to [2, 32] and caches one program per distinct value.
#ifndef MOTION_BLUR_SAMPLES
#define MOTION_BLUR_SAMPLES 8
#endif

// Maximum blur length in UV units, so fast rotation / the sky don't smear the
// whole screen into mush.
const float MAX_VELOCITY = 0.15;

// --- Exposure-linked blur length ------------------------------------------
// When auto exposure is on, the eye/camera opens up in dark scenes (exposure
// factor > 1) and stops down in bright ones (factor < 1). We tie the blur
// length to that factor so darker, exposure-boosted areas smear MORE and
// well-lit areas smear LESS, mirroring how a real longer exposure both
// brightens the frame and captures more motion.
//
// INFLUENCE: 0 = ignore exposure entirely, 1 = scale blur length directly by
// the exposure factor. MIN/MAX clamp the multiplier so an extreme exposure
// (pitch-black or blown-out) can't collapse or explode the smear.
const float EXPOSURE_BLUR_INFLUENCE = 1.0;
const float EXPOSURE_BLUR_MIN = 0.5;
const float EXPOSURE_BLUR_MAX = 2.5;

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

// Brighter light smears FURTHER. The emissive part of the blur samples along a
// line that is up to this many times longer than the ordinary (diffuse) blur.
// Because we gather with max() over that longer line, a saturated source paints
// a streak well past where the averaged blur fades out, so bright areas visibly
// get "more" motion blur than the rest of the scene. The extra reach is scaled
// per-sample by how emissive the sampled pixel is, so a dim glow only extends a
// little while a full-bright source uses the whole boosted length.
const float EMISSION_LENGTH_BOOST = 2.5;

// The emissive boost must fire for a localized moving light (a lamp, a
// lightsaber) but NOT for a large uniformly-bright region such as the sky, which
// would otherwise flash brighter whenever the camera moves. We tell them apart
// by the DARKEST point along the smear line: a real light source is small, so
// its smear line runs off the source into darker background (low minimum); the
// sky is bright along the entire line (high minimum). If the darkest sample is
// below _LO the streak is fully boosted; above _HI it is fully suppressed.
const float EMISSION_UNIFORM_LO = 0.35;
const float EMISSION_UNIFORM_HI = 0.60;

// --- Working color space ---------------------------------------------------
// The color buffer at this point in the pipeline is gamma-encoded; second_stage
// is what converts it to linear (pow 2.2) much later. Averaging gamma-encoded
// values is not energy-conserving — it makes a smear noticeably darker than the
// light that produced it. Averaging a full-bright pixel with a dark one gives
// ~0.55 encoded (0.27 linear) where the true answer is ~0.50 linear. That
// missing energy is a large part of what the emissive path above exists to
// fight. So decode to linear before combining samples, and re-encode on the way
// out.
//
// Gamma 2.0 rather than the engine's 2.2: the round trip is exact either way,
// so a pixel that ends up unblurred comes out bit-identical and the pass stays
// a true no-op where nothing moved. Only the weighting of the average differs,
// and square/sqrt is far cheaper than a pow() per sample.
vec3 gammaToLinear(vec3 c) { return c * c; }
vec3 linearToGamma(vec3 c) { return sqrt(c); }

// EMISSION_STRENGTH expresses a perceptual multiplier, so square it to apply in
// the linear domain we now accumulate in.
const float EMISSION_STRENGTH_LINEAR = EMISSION_STRENGTH * EMISSION_STRENGTH;

void main(void)
{
	vec2 uv = varTexCoord.st;
	vec4 color = texture2D(rendered, uv);
	highp float rawDepth = texture2D(depthmap, uv).r;

	// Reconstruct the world-space (camera-relative) position of this pixel.
	highp vec4 ndc = vec4(uv * 2.0 - 1.0, rawDepth * 2.0 - 1.0, 1.0);
	highp vec4 worldPos = mInvViewProj * ndc;
	worldPos /= worldPos.w;

	// Reproject it through last frame's camera to find its previous screen pos.
	highp vec4 prevClip = mPrevViewProj * vec4(worldPos.xyz, 1.0);

	// A pixel visible now may have been BEHIND the previous camera: during a
	// fast turn, or when moving past geometry that was very close. Then w is
	// negative, the perspective divide mirrors the position through the origin,
	// and the resulting velocity points the wrong way — producing a full-length
	// smear in the opposite direction (the magnitude clamp below does not help,
	// it only limits length). Leave those pixels unblurred instead.
	if (prevClip.w <= 0.0) {
		gl_FragColor = vec4(color.rgb, 1.0);
		return;
	}

	vec2 prevUv = (prevClip.xy / prevClip.w) * 0.5 + 0.5;

	// Screen-space velocity in UV units.
	vec2 velocity = (uv - prevUv) * motionBlurStrength;

#ifdef ENABLE_AUTO_EXPOSURE
	// Auto-exposure is stored on a log2 scale; pow(2, x) gives the linear
	// factor (>1 dark scene, <1 bright scene). Scale the blur length by it so
	// the smear grows in the dark and shrinks in the light.
	float exposure = pow(2., texture2D(exposureMap, vec2(0.5)).r);
	float exposureFactor = clamp(mix(1.0, exposure, EXPOSURE_BLUR_INFLUENCE),
			EXPOSURE_BLUR_MIN, EXPOSURE_BLUR_MAX);
	velocity *= exposureFactor;
#endif

	float len = length(velocity);
	if (len > MAX_VELOCITY)
		velocity *= MAX_VELOCITY / len;

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
	// Darkest value-channel sample seen along the emissive line, used below to
	// decide whether this is a localized source (line dips dark) or a uniform
	// bright region like the sky (line stays bright).
	float minEmissiveV = 1.0;
	const float invSamples = 1.0 / float(MOTION_BLUR_SAMPLES);
	for (int i = 0; i < MOTION_BLUR_SAMPLES; i++) {
		// Spread samples symmetrically around the current pixel: t in [-0.5, 0.5],
		// jittered by a fraction of the step so the trail has no gaps.
		float t = (float(i) + jitter) * invSamples - 0.5;

		// Diffuse sample along the ordinary blur line.
		vec2 samplePos = clamp(uv + velocity * t, vec2(0.0), vec2(1.0));
		vec3 c = texture2D(rendered, samplePos).rgb;
		sum += gammaToLinear(c);

		// Emissive sample along an EXTENDED line so bright sources reach further.
		// The reach grows with how emissive the sampled pixel is, so brighter =
		// longer smear.
		vec2 emPos = clamp(uv + velocity * (t * EMISSION_LENGTH_BOOST), vec2(0.0), vec2(1.0));
		vec3 ec = texture2D(rendered, emPos).rgb;
		// Note that the emissive *detection* below deliberately stays in the
		// gamma-encoded domain: these thresholds describe how bright a pixel
		// looks, which is a perceptual question, and keeping them here means
		// they mean exactly what they did before the linear-space change.
		float ev = max(ec.r, max(ec.g, ec.b));
		float e = smoothstep(EMISSION_THRESHOLD, 1.0, ev);
		emissive = max(emissive, gammaToLinear(ec) * e);
		minEmissiveV = min(minEmissiveV, ev);
	}

	// Suppress the boost for uniformly-bright regions (the sky): if even the
	// darkest point along the smear line is bright, this is not a localized light
	// source, so don't push it brighter on motion. A real light source's line
	// dips into dark background, keeping localness ~1 across its whole streak
	// (body included), so the source stays a solid bright bar with no leftover.
	float localness = 1.0 - smoothstep(EMISSION_UNIFORM_LO, EMISSION_UNIFORM_HI, minEmissiveV);
	emissive *= localness;

	vec3 base = sum * invSamples;
	// Where the trail is emissive, take the bright bar; elsewhere it is ~0 and
	// the normal averaged blur shows through unchanged.
	vec3 result = max(base, emissive * EMISSION_STRENGTH_LINEAR);
	gl_FragColor = vec4(linearToGamma(result), 1.0);
}

#endif // MOTION_BLUR_UNSUPPORTED
