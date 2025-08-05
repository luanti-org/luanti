uniform mat4 LightMVP; // world matrix
uniform vec4 CameraPos;
varying vec4 tPos;
#ifdef COLORED_SHADOWS
varying vec3 varColor;
#endif

uniform float xyPerspectiveBias0;
uniform float xyPerspectiveBias1;
uniform float zPerspectiveBias;

#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

vec4 getRelativePosition(in vec4 position)
{
	vec2 l = position.xy - CameraPos.xy;
	vec2 s = l / abs(l);
	s = (1.0 - s * CameraPos.xy);
	l /= s;
	return vec4(l, s);
}

float getPerspectiveFactor(in vec4 relativePosition)
{
	float pDistance = length(relativePosition.xy);
	float pFactor = pDistance * xyPerspectiveBias0 + xyPerspectiveBias1;
	return pFactor;
}

vec4 applyPerspectiveDistortion(in vec4 position)
{
	vec4 l = getRelativePosition(position);
	float pFactor = getPerspectiveFactor(l);
	l.xy /= pFactor;
	position.xy = l.xy * l.zw + CameraPos.xy;
	position.z *= zPerspectiveBias;
	return position;
}

void main()
{
	vec4 pos = LightMVP * inVertexPosition;

	tPos = applyPerspectiveDistortion(pos);

	gl_Position = vec4(tPos.xyz, 1.0);
	varTexCoord = inTexCoord0.xy;

#ifdef COLORED_SHADOWS
	varColor = inVertexColor.rgb;
#endif
}
