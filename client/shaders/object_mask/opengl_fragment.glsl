// Fragment stage of the camera-rigid object mask. Writes a flat 1.0 wherever a
// camera-rigid object is visible; the target is cleared to 0 beforehand, and the
// shared scene depth buffer takes care of rejecting parts hidden behind terrain.

uniform sampler2D baseTexture;

CENTROID_ VARYING_ mediump vec2 varTexCoord;

void main(void)
{
	// Honour alpha cutouts so the mask follows the visible silhouette rather
	// than the bounding geometry. Without this, the transparent parts of a model
	// (the player's hat layer, say) would mask the background around the object
	// and leave a conspicuous unblurred halo. Threshold matches shadow/pass1,
	// which ignores the node alpha mode in the same way.
	if (texture2D(baseTexture, varTexCoord).a < 0.70)
		discard;

	gl_FragColor = vec4(1.0);
}
