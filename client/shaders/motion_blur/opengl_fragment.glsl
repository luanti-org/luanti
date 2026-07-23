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

	vec3 sum = vec3(0.0);
	for (int i = 0; i < NUM_SAMPLES; i++) {
		// Spread samples symmetrically around the current pixel: t in [-0.5, 0.5].
		float t = float(i) / float(NUM_SAMPLES - 1) - 0.5;
		vec2 samplePos = clamp(uv + velocity * t, vec2(0.0), vec2(1.0));
		sum += texture2D(rendered, samplePos).rgb;
	}

	gl_FragColor = vec4(sum / float(NUM_SAMPLES), 1.0);
}
