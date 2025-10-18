varying vec3 vNormal;
varying vec3 vPosition;
#ifdef GL_ES
varying lowp vec4 varColor;
varying mediump vec2 varTexCoord;
varying float varTexLayer;
#else
centroid varying vec4 varColor;
centroid varying vec2 varTexCoord;
centroid varying float varTexLayer; // actually int
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
