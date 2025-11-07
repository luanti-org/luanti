VARYING_ vec3 vNormal;
VARYING_ vec3 vPosition;
CENTROID_ VARYING_ lowp vec4 varColor;
CENTROID_ VARYING_ mediump vec2 varTexCoord;
CENTROID_ VARYING_ float varTexLayer; // actually int

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
