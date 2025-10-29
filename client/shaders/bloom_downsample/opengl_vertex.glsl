#ifdef GL_ES
VARYING_ mediump vec2 varTexCoord;
#else
centroid VARYING_ vec2 varTexCoord;
#endif

void main(void)
{
	varTexCoord.st = inTexCoord0.st;
	gl_Position = inVertexPosition;
}
