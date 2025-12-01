uniform mat4 LightMVP; // world matrix
uniform vec4 CameraPos; // camera position
VARYING_ vec4 tPos;

uniform float xyPerspectiveBias0;
uniform float xyPerspectiveBias1;
uniform float zPerspectiveBias;

#ifdef USE_SKINNING
layout (std140) uniform JointMatrices {
	mat4 joints[MAX_JOINTS];
};
#endif

CENTROID_ VARYING_ mediump vec2 varTexCoord;
CENTROID_ VARYING_ float varTexLayer; // actually int

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
#ifdef USE_SKINNING
	uvec4 jids = inVertexJointIDs;
	vec4 skinPos = inVertexPosition;
	// Alternatively: Introduce neutral bone at index 0 with identity matrix?
	if (inVertexWeights != vec4(0.0)) {
		// Note that this deals correctly with a disabled vertex attribute.
		mat4 mSkin =
				inVertexWeights.x * joints[jids.x] +
				inVertexWeights.y * joints[jids.y] +
				inVertexWeights.z * joints[jids.z] +
				inVertexWeights.w * joints[jids.w];
		skinPos = vec4((mSkin * vec4(inVertexPosition.xyz, 1.0)).xyz, 1.0);
	}
#else
	vec4 skinPos = inVertexPosition;
#endif

	vec4 pos = LightMVP * skinPos;

	tPos = applyPerspectiveDistortion(pos);

	gl_Position = vec4(tPos.xyz, 1.0);

	varTexCoord = (mTexture * vec4(inTexCoord0.xy, 1.0, 1.0)).st;
#ifdef USE_ARRAY_TEXTURE
	varTexLayer = inVertexAux;
#endif
}
