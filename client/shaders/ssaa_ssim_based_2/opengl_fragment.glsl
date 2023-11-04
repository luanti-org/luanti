#define input_l texture0
#define input_l2 texture1

uniform sampler2D input_l;
uniform sampler2D input_l2;
uniform vec2 texelSize0;

void main()
{
	float x1 = gl_FragCoord.x;
	float y1 = gl_FragCoord.y;
	float mv = 0.0;
	float r_1 = 0.0;
	// We use a constant patch size of 2x2 pixels
	for (float y = y1; y < y1+2; y += 1) {
		for (float x = x1; x < x1+2; x += 1) {
			vec2 texcoords = vec2(x, y) * texelSize0;
			float col_l = texture2D(input_l, texcoords).r;
			mv += col_l;
			r_1 += col_l * col_l;
		}
	}
	mv = mv * 0.25;
	r_1 = r_1 * 0.25;
	// Here we calculate the mean via bilinear interpolation on the GPU
	float r_2 = texture2D(input_l2, vec2(x1+0.5, y1+0.5) * texelSize0).r;

	float slv = r_1 - mv * mv;
	float shv = r_2 - mv * mv;
	float rv;
	// To avoid numerical errors, we need 32-bit float textures and this
	// threshold
	if (slv >= 0.000001)
		rv = sqrt(shv / slv);
	else
		rv = 1;

	gl_FragData[0] = vec4(mv, vec3(1.0));
	gl_FragData[1] = vec4(rv, vec3(1.0));
}
