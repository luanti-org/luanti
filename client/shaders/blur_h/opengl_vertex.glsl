CENTROID_ VARYING_ mediump vec2 varTexCoord;

void main(void)
{
	varTexCoord.st = inTexCoord0.st;
	gl_Position = inVertexPosition;
}
