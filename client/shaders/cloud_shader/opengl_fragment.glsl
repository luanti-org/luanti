uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;
VARYING_ highp vec3 eyeVec;

VARYING_ lowp vec4 varColor;

void main(void)
{
	vec4 col = varColor;

	float clarity = clamp(fogShadingParameter
		- fogShadingParameter * length(eyeVec) / fogDistance, 0.0, 1.0);
	col.rgb = mix(fogColor.rgb, col.rgb, clarity);

	gl_FragColor = col;
}
