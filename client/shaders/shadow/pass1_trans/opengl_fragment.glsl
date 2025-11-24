#ifdef USE_ARRAY_TEXTURE
	uniform mediump sampler2DArray baseTexture;
#else
	uniform sampler2D baseTexture;
#endif
VARYING_ vec4 tPos;

CENTROID_ VARYING_ mediump vec2 varTexCoord;
CENTROID_ VARYING_ float varTexLayer; // actually int

#ifdef COLORED_SHADOWS
VARYING_ vec3 varColor;

// c_precision of 128 fits within 7 base-10 digits
const float c_precision = 128.0;
const float c_precisionp1 = c_precision + 1.0;

float packColor(vec3 color)
{
	return floor(color.b * c_precision + 0.5)
		+ floor(color.g * c_precision + 0.5) * c_precisionp1
		+ floor(color.r * c_precision + 0.5) * c_precisionp1 * c_precisionp1;
}
#endif

void main()
{
#ifdef USE_ARRAY_TEXTURE
	vec4 base = texture(baseTexture, vec3(varTexCoord, varTexLayer)).rgba;
#else
	vec4 base = texture2D(baseTexture, varTexCoord).rgba;
#endif
#ifndef COLORED_SHADOWS
	if (base.a < 0.5)
		discard;
#endif

	float depth = 0.5 + tPos.z * 0.5;
	// depth in [0, 1] for texture

#ifdef COLORED_SHADOWS
	base.rgb *= varColor.rgb;
	// premultiply color alpha (see-through side)
	float packedColor = packColor(base.rgb * (1.0 - base.a));
	gl_FragColor = vec4(depth, packedColor, 0.0,1.0);
#else
	gl_FragColor = vec4(depth, 0.0, 0.0, 1.0);
#endif
}
