// Vertex stage of the camera-rigid object mask.
//
// This must place vertices at exactly the same clip position that object_shader
// gave them when the scene was drawn, or the mask will not line up with the
// object it is masking. That means reproducing the GPU skinning, otherwise an
// animated model would be masked in its bind pose. Compare shadow/pass1, which
// has the same requirement for the same reason.

#ifdef USE_SKINNING
layout (std140) uniform JointMatrices {
	mat4 joints[MAX_JOINTS];
};
#endif

CENTROID_ VARYING_ mediump vec2 varTexCoord;

void main(void)
{
#ifdef USE_SKINNING
	uvec4 jids = inVertexJointIDs;
	vec4 skinPos = inVertexPosition;
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

	varTexCoord = (mTexture * vec4(inTexCoord0.xy, 1.0, 1.0)).st;
	gl_Position = mWorldViewProj * skinPos;
}
