// based on Phys.Bloom OpenGL tutorial https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom
// and ACM Siggraph talk in 2014 by Jorge Jimenez for Call of Duty: Advanced Warfare.
#define rendered texture0

uniform sampler2D rendered;
uniform vec2 texelSize0;

CENTROID_ VARYING_ mediump vec2 varTexCoord;

void main(void)
{
	vec2 tx = 2.0 * texelSize0;
	vec3 a = texture2D(rendered, varTexCoord.st + vec2(-1., -1.) * tx).rgb;
	vec3 b = texture2D(rendered, varTexCoord.st + vec2(0., -1.) * tx).rgb;
	vec3 c = texture2D(rendered, varTexCoord.st + vec2(1., -1.) * tx).rgb;
	vec3 d = texture2D(rendered, varTexCoord.st + vec2(-1., 0.) * tx).rgb;
	vec3 e = texture2D(rendered, varTexCoord.st + vec2(0., 0.) * tx).rgb;
	vec3 f = texture2D(rendered, varTexCoord.st + vec2(1., 0.) * tx).rgb;
	vec3 g = texture2D(rendered, varTexCoord.st + vec2(-1., 1.) * tx).rgb;
	vec3 h = texture2D(rendered, varTexCoord.st + vec2(0., 1.) * tx).rgb;
	vec3 i = texture2D(rendered, varTexCoord.st + vec2(1., 1.) * tx).rgb;
	vec3 j = texture2D(rendered, varTexCoord.st + vec2(-0.5, -0.5) * tx).rgb;
	vec3 k = texture2D(rendered, varTexCoord.st + vec2(0.5, -0.5) * tx).rgb;
	vec3 l = texture2D(rendered, varTexCoord.st + vec2(-0.5, 0.5) * tx).rgb;
	vec3 m = texture2D(rendered, varTexCoord.st + vec2(-0.5, 0.5) * tx).rgb;

	vec3 color =
		(a + c + g + i) * 0.03125 +
		(b + d + f + h) * 0.0625 +
		(e + j + k + l + m) * 0.125;

	gl_FragColor = max(vec4(color, 1.0), 1e-4);
}
