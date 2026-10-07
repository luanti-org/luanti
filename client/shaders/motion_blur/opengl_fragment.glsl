#define rendered texture0
#define depthmap texture1
#define rigidMask texture3

uniform sampler2D rendered;
uniform sampler2D depthmap;
// Mask of objects that move with the camera: the player's own body and whatever
// it is riding. See CameraRigidMaskStep. 1 where such an object is visible.
uniform sampler2D rigidMask;

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
const float EMISSION_STRENGTH = 1.15;

// Brighter light smears FURTHER. The emissive part of the blur samples along a
// line that is up to this many times longer than the ordinary (diffuse) blur.
// Because we gather with max() over that longer line, a saturated source paints
// a streak well past where the averaged blur fades out, so bright areas visibly
// get "more" motion blur than the rest of the scene. The extra reach is scaled
// per-sample by how emissive the sampled pixel is, so a dim glow only extends a
// little while a full-bright source uses the whole boosted length.
const float EMISSION_LENGTH_BOOST = 1.9;

// The emissive boost must fire for a localized light (a lamp, a lightsaber, the
// sun) but NOT for a large uniformly-bright region such as plain sky, which
// would otherwise flash brighter whenever the camera moves.
//
// What separates the two is CONTRAST along the smear line, not absolute
// brightness: a light source is small, so its line runs off the source into
// something darker, whereas open sky looks the same all the way along. Testing
// the darkest point alone (as this used to) fails the sun, which sits in a sky
// bright enough to keep that minimum high and so gets suppressed along with it.
//
// Below _LO the line is treated as uniform and the streak is suppressed; above
// _HI it is a localized source and streaks fully.
const float EMISSION_CONTRAST_LO = 0.08;
const float EMISSION_CONTRAST_HI = 0.25;

// The localness test above stops the SKY being boosted, but on its own it does
// nothing to stop the sky being painted ONTO something else. A max() gather
// takes the brightest thing anywhere along the line, so every ground pixel
// within reach of the horizon picks up sky brightness and the background bleeds
// forward over the terrain. At full strength that reach is a fifth of the
// screen, which is exactly what it looks like.
//
// So weight each emissive sample by how close it is to this pixel in depth.
// Depth is non-linear, but (1 - depth) behaves like 1/distance, so their ratio
// is a scale-invariant "how much nearer or farther" measure: ~1 is the same
// distance, below 1 is farther away, above 1 is nearer. Samples at or in front
// of this pixel count fully, ones clearly behind it fade out. The sky sits at
// the far plane, so it scores 0 against any real geometry and is rejected
// outright, while sky-on-sky (a sun streak) still works because both sides of
// the ratio then vanish together.
//
// Deliberately strict: a sample must be within roughly 1.5x this pixel's
// distance before it counts at all. Samples NEARER than this pixel always pass,
// which is the direction you want kept -- a torch in the foreground should still
// streak across the hills behind it.
const float EMISSION_DEPTH_LO = 0.65;
const float EMISSION_DEPTH_HI = 0.92;
// Added to both sides of the depth ratio so that far-against-far reads as "same
// distance" rather than 0/0. Roughly "distances beyond this are all infinity".
const float EMISSION_DEPTH_EPSILON = 1e-4;

// Ceiling on how far the emissive trail may lift a pixel above its ordinary
// blurred value, as a fraction: 0 disables the emissive path entirely, 1 lets it
// win outright. This is the master dial for "how much do bright things take over
// the image" -- turn it down if light sources feel overbearing, up if they feel
// flat.
const float EMISSION_MAX_GAIN = 0.4;

// How far outside the view to push the previous position of a pixel that was
// behind the previous camera. Only the direction survives; the clamp does the
// rest. Anything comfortably above 1 works.
const float BEHIND_CAMERA_PUSH = 8.0;

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

	// Objects rigidly attached to the camera are motionless on screen, but the
	// reprojection below assumes every pixel is fixed in the world and would
	// give them the full camera smear — worst of all for the player's own body
	// and vehicle, which sit closest to the camera. They are masked out during
	// the scene draw, so leave them exactly as they are.
	if (texture2D(rigidMask, uv).r > 0.5) {
		gl_FragColor = vec4(color.rgb, 1.0);
		return;
	}

	highp float rawDepth = texture2D(depthmap, uv).r;

	// Reconstruct the world-space (camera-relative) position of this pixel.
	highp vec4 ndc = vec4(uv * 2.0 - 1.0, rawDepth * 2.0 - 1.0, 1.0);
	highp vec4 worldPos = mInvViewProj * ndc;
	worldPos /= worldPos.w;

	// Reproject it through last frame's camera to find its previous screen pos.
	highp vec4 prevClip = mPrevViewProj * vec4(worldPos.xyz, 1.0);

	// A pixel visible now may have been BEHIND the previous camera: during a
	// fast turn, or when moving past geometry that was very close. Then w is
	// negative and the perspective divide mirrors the position through the
	// origin, so the naive velocity points the wrong way.
	//
	// Dividing by |w| instead of w undoes exactly that mirroring, recovering the
	// correct direction. The magnitude is meaningless for such a pixel (its true
	// previous position is off screen entirely), so push it far out and let the
	// clamp below turn it into a full-length smear the right way round.
	//
	// Note that simply leaving these pixels unblurred is NOT good enough: during
	// a fast spin they form a whole band, which reads as a sharply defined
	// unblurred patch on the side the camera is turning away from.
	highp vec2 prevNdc = prevClip.xy / max(abs(prevClip.w), 1e-6);
	if (prevClip.w < 0.0)
		prevNdc *= BEHIND_CAMERA_PUSH;

	vec2 prevUv = prevNdc * 0.5 + 0.5;

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
	// Darkest and brightest value-channel samples along the emissive line. Their
	// difference is the contrast test below, which decides whether this is a
	// localized source or a uniformly bright region.
	float minEmissiveV = 1.0;
	float maxEmissiveV = 0.0;
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

		// Reject samples that lie well behind this pixel, so a bright
		// background cannot smear itself forward over nearer geometry. The
		// epsilon on both sides is what makes far-against-far read as "same
		// distance" (ratio 1) rather than 0/0, so a bright sky still streaks
		// against itself while being rejected against any real geometry.
		highp float emDepth = texture2D(depthmap, emPos).r;
		float depthRatio = (1.0 - emDepth + EMISSION_DEPTH_EPSILON)
				/ (1.0 - rawDepth + EMISSION_DEPTH_EPSILON);
		e *= smoothstep(EMISSION_DEPTH_LO, EMISSION_DEPTH_HI, depthRatio);

		emissive = max(emissive, gammaToLinear(ec) * e);
		minEmissiveV = min(minEmissiveV, ev);
		maxEmissiveV = max(maxEmissiveV, ev);
	}

	// Suppress the boost where the smear line is uniformly bright (open sky) and
	// allow it where the line has contrast (a light source against its
	// surroundings, including the sun against the sky).
	float localness = smoothstep(EMISSION_CONTRAST_LO, EMISSION_CONTRAST_HI,
			maxEmissiveV - minEmissiveV);
	emissive *= localness;

	vec3 base = sum * invSamples;
	// Where the trail is emissive, take the bright bar; elsewhere it is ~0 and
	// the normal averaged blur shows through unchanged. The result is then
	// admitted only up to EMISSION_MAX_GAIN, so bright sources streak without
	// taking over the image.
	vec3 boosted = max(base, emissive * EMISSION_STRENGTH_LINEAR);
	vec3 result = mix(base, boosted, EMISSION_MAX_GAIN);
	gl_FragColor = vec4(linearToGamma(result), 1.0);
}

#endif // MOTION_BLUR_UNSUPPORTED
