#define current texture0
#define previous texture1

uniform sampler2D current;
uniform sampler2D previous;
uniform vec2 texelSize0;
uniform mediump float bloomRadius;

CENTROID_ VARYING_ mediump vec2 varTexCoord;

void main(void)
{
	vec2 offset = bloomRadius * texelSize0;

	vec3 a = texture2D(previous, varTexCoord.st + vec2(-1., -1.) * offset).rgb;
	vec3 b = texture2D(previous, varTexCoord.st + vec2(0., -1.) * offset).rgb;
	vec3 c = texture2D(previous, varTexCoord.st + vec2(1., -1.) * offset).rgb;
	vec3 d = texture2D(previous, varTexCoord.st + vec2(-1., 0.) * offset).rgb;
	vec3 e = texture2D(previous, varTexCoord.st + vec2(0., 0.) * offset).rgb;
	vec3 f = texture2D(previous, varTexCoord.st + vec2(1., 0.) * offset).rgb;
	vec3 g = texture2D(previous, varTexCoord.st + vec2(-1., 1.) * offset).rgb;
	vec3 h = texture2D(previous, varTexCoord.st + vec2(0., 1.) * offset).rgb;
	vec3 i = texture2D(previous, varTexCoord.st + vec2(1., 1.) * offset).rgb;

	vec3 base = texture2D(current, varTexCoord.st).rgb;

	gl_FragColor = max(vec4(base +
			(a + c + g + i) * 0.0625 +
			(b + d + f + h) * 0.125 +
			e * 0.25, 1.), 1e-4);
}
