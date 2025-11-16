#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

void main()
{
	vec4 uv = vec4(inVertexPosition.xyz, 1.0) * 0.5 + 0.5;
	varTexCoord = uv.st;
	gl_Position = vec4(inVertexPosition.xyz, 1.0);
}
