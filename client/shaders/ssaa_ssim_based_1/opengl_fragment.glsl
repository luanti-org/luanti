#define rendered texture0

uniform sampler2D rendered;
uniform vec2 texelSize0;

vec3 srgb_to_ycbcr(vec3 col_srgb)
{
	// We approximate the sRGB EOTF with a 2.2 gamma
	vec3 col_rgb = pow(col_srgb, vec3(2.2));
	return mat3(
		0.299, -0.168736, 0.5,
		0.587, -0.331264, -0.418688,
		0.114, 0.5, -0.081312
	) * col_rgb + vec3(0.0, 0.5, 0.5);
}

void main()
{
	// Convert to YCbCr and calculate L and L2
	// gl_FragCoord is offset by 0.5 (pixel center), which needs to be
	// compensated for when searching for the lower left pixel (x1, y2)
	float x1 = (gl_FragCoord.x - 0.5) * SSAA_SCALE + 0.5;
	float y1 = (gl_FragCoord.y - 0.5) * SSAA_SCALE + 0.5;
	vec3 col_ycbcr = vec3(0);
	float col_y_squareds = 0.0;
	for (float y = y1; y < y1+SSAA_SCALE; y += 1) {
		for (float x = x1; x < x1+SSAA_SCALE; x += 1) {
			vec2 texcoords = vec2(x, y) * texelSize0;
			vec3 sample_col_srgb = texture2D(rendered, texcoords).rgb;
			vec3 sample_col_ycbcr = srgb_to_ycbcr(sample_col_srgb);
			col_ycbcr += sample_col_ycbcr;
			col_y_squareds += sample_col_ycbcr.r * sample_col_ycbcr.r;
		}
	}
	col_ycbcr *= 1.0 / (SSAA_SCALE * SSAA_SCALE);
	col_y_squareds *= 1.0 / (SSAA_SCALE * SSAA_SCALE);

	gl_FragData[0] = vec4(col_ycbcr, 1.0);
	gl_FragData[1] = vec4(col_y_squareds, vec3(1.0));
}
