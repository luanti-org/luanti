CENTROID_ VARYING_ mediump vec2 varTexCoord;

void main()
{
	vec4 uv = vec4(inVertexPosition.xyz, 1.0) * 0.5 + 0.5;
	varTexCoord = uv.st;
	gl_Position = vec4(inVertexPosition.xyz, 1.0);
}
