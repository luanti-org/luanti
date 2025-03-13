#define rendered texture0

struct ExposureParams {
	float compensationFactor;
};

uniform sampler2D rendered;
uniform mediump float bloomStrength;
uniform ExposureParams exposureParams;

#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

#ifdef ENABLE_AUTO_EXPOSURE
varying float exposure; // linear exposure factor, see vertex shader
#endif

void main(void)
{
	vec2 uv = varTexCoord.st;
	vec3 color = texture2D(rendered, uv).rgb;
	// translate to linear colorspace (approximate)
#ifdef GL_ES
	// clamp color to [0,1] range in lieu of centroids
	color = pow(clamp(color, 0.0, 1.0), vec3(2.2));
#else
	color = pow(color, vec3(2.2));
#endif

	color *= exposureParams.compensationFactor * bloomStrength;

#ifdef ENABLE_AUTO_EXPOSURE
	color *= exposure;
#endif

	gl_FragColor = vec4(color, 1.0); // force full alpha to avoid holes in the image.
}
