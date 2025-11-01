#ifdef ENABLE_AUTO_EXPOSURE
#define exposureMap texture1

uniform sampler2D exposureMap;

VARYING_ float exposure;
#endif

CENTROID_ VARYING_ mediump vec2 varTexCoord;


void main(void)
{
#ifdef ENABLE_AUTO_EXPOSURE
	// value in the texture is on a logarithtmic scale
	exposure = texture2D(exposureMap, vec2(0.5)).r;
	exposure = pow(2., exposure);
#endif

	varTexCoord.st = inTexCoord0.st;
	gl_Position = inVertexPosition;
}
