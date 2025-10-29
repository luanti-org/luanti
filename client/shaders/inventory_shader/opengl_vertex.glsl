VARYING_ vec3 vNormal;
VARYING_ vec3 vPosition;
#ifdef GL_ES
VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;
VARYING_ float varTexLayer;
#else
centroid VARYING_ vec4 varColor;
centroid VARYING_ vec2 varTexCoord;
centroid VARYING_ float varTexLayer; // actually int
#endif

void main(void)
{
#ifdef USE_ARRAY_TEXTURE
	varTexLayer = inVertexAux;
#endif
	varTexCoord = inTexCoord0.st;

	vec4 pos = inVertexPosition;
	gl_Position = mWorldViewProj * pos;
	vPosition = gl_Position.xyz;
	vNormal = inVertexNormal;

	vec4 color = inVertexColor;
	varColor = clamp(color, 0.0, 1.0);
}
