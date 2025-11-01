#define rendered texture0

uniform sampler2D rendered;
uniform vec2 texelSize0;
uniform mediump float bloomRadius;
uniform mat3 bloomBlurWeights;

CENTROID_ VARYING_ mediump vec2 varTexCoord;

// smoothstep - squared
float smstsq(float f)
{
	f = f * f * (3. - 2. * f);
	return f;
}

void main(void)
{
	// kernel distance and linear size
	mediump float n = 2. * bloomRadius + 1.;

	vec2 uv = varTexCoord.st - vec2(bloomRadius * texelSize0.x, 0.);
	vec4 color = vec4(0.);
	mediump float sum = 0.;
	for (mediump float i = 0.; i < n; i++) {
		mediump float weight = smstsq(1. - (abs(i / bloomRadius - 1.)));
		color.rgb += texture2D(rendered, uv).rgb * weight;
		sum += weight;
		uv += vec2(texelSize0.x, 0.);
	}
	color /= sum;
	gl_FragColor = vec4(color.rgb, 1.0); // force full alpha to avoid holes in the image.
}
