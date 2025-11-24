#ifdef USE_ARRAY_TEXTURE
	uniform mediump sampler2DArray baseTexture;
#else
	uniform sampler2D baseTexture;
#endif
VARYING_ vec4 tPos;

CENTROID_ VARYING_ mediump vec2 varTexCoord;
CENTROID_ VARYING_ float varTexLayer; // actually int

void main()
{
#ifdef USE_ARRAY_TEXTURE
	vec4 base = texture(baseTexture, vec3(varTexCoord, varTexLayer)).rgba;
#else
	vec4 base = texture2D(baseTexture, varTexCoord).rgba;
#endif
	// (this totally ignores the node's alpha mode)
	if (base.a < 0.70)
		discard;

	float depth = 0.5 + tPos.z * 0.5;
	gl_FragColor = vec4(depth, 0.0, 0.0, 1.0);
}
