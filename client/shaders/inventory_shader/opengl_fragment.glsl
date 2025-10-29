#ifdef USE_ARRAY_TEXTURE
	uniform mediump sampler2DArray baseTexture;
#else
	uniform sampler2D baseTexture;
#endif

VARYING_ vec3 vNormal;
VARYING_ vec3 vPosition;
#ifdef GL_ES
VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;
VARYING_ float varTexLayer;
#else
centroid VARYING_ vec4 varColor;
centroid VARYING_ vec2 varTexCoord;
centroid VARYING_ float varTexLayer; // actually int
#endif


void main(void)
{
	vec2 uv = varTexCoord.st;

#ifdef USE_ARRAY_TEXTURE
	vec4 base = texture(baseTexture, vec3(uv, varTexLayer)).rgba;
#else
	vec4 base = texture2D(baseTexture, uv).rgba;
#endif

	// Handle transparency by discarding pixel as appropriate.
#ifdef USE_DISCARD
	if (base.a == 0.0)
		discard;
#endif
#ifdef USE_DISCARD_REF
	if (base.a < 0.5)
		discard;
#endif

	vec4 col = vec4(base.rgb * varColor.rgb, base.a);

	gl_FragColor = col;
}
