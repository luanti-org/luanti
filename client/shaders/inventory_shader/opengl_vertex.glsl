CENTROID_ VARYING_ lowp vec4 varColor;
CENTROID_ VARYING_ mediump vec2 varTexCoord;
#ifdef USE_ARRAY_TEXTURE
#ifdef GL_ES
flat VARYING_ int varTexLayer;
#else
CENTROID_ VARYING_ float varTexLayer;
#endif
#endif

void main(void)
{
#ifdef USE_ARRAY_TEXTURE
	varTexLayer = int(inVertexAux);
#endif
	varTexCoord = inTexCoord0.st;

	vec4 pos = inVertexPosition;
	gl_Position = mWorldViewProj * pos;

	vec4 color = inVertexColor;
	varColor = clamp(color, 0.0, 1.0);
}
