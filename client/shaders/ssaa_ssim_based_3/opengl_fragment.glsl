#define input_l texture0
#define input_m texture1
#define input_r texture2

uniform sampler2D input_l;
uniform sampler2D input_m;
uniform sampler2D input_r;
uniform vec2 texelSize0;

vec3 ycbcr_to_linear_srgb(vec3 col_ycbcr)
{
	return mat3(
		1.0, 1.0, 1.0,
		0.0, -0.344136, 1.772,
		1.402, -0.714136, 0.0
	) * (col_ycbcr - vec3(0.0, 0.5, 0.5));
}

void main()
{
	float x1 = gl_FragCoord.x;
	float y1 = gl_FragCoord.y;
	vec3 col_l = texture2D(input_l, vec2(x1, y1) * texelSize0).rgb;
	float d = 0.0;
	for (float y = y1; y > y1-2; y -= 1) {
		for (float x = x1; x > x1-2; x -= 1) {
			vec2 texcoords = vec2(x, y) * texelSize0;
			float col_m = texture2D(input_m, texcoords).r;
			float col_r = texture2D(input_r, texcoords).r;
			d += col_m + col_r * col_l.r - col_r * col_m;
		}
	}
	d = d * 0.25;

	// Go back to sRGB colours
	gl_FragColor = vec4(ycbcr_to_linear_srgb(vec3(d, col_l.gb)), 1.0);
}
