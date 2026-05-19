/* Attributes */

attribute vec3 inVertexPosition;
attribute vec3 inVertexNormal;
attribute vec4 inVertexColor_raw;
attribute vec2 inTexCoord0;

#ifdef USE_SKINNING
attribute mediump vec4 inVertexWeights;
attribute mediump uvec4 inVertexJointIDs;
#endif

/* Uniforms */

uniform mat4 uWVPMatrix;
uniform mat4 uWVMatrix;
uniform mat4 uTMatrix0;

uniform float uThickness;

#ifdef USE_SKINNING
layout (std140) uniform uJointMatrices {
	mat4 joints[MAX_JOINTS];
};
#endif

/* Varyings */

varying vec2 vTextureCoord0;
varying vec4 vVertexColor;
varying float vFogCoord;

void main()
{
#ifdef USE_SKINNING
	uvec4 jids = inVertexJointIDs;
	vec3 skinPos = inVertexPosition;
	vec3 skinNormal = inVertexNormal;
	// Alternatively: Introduce neutral bone at index 0 with identity matrix?
	if (inVertexWeights != vec4(0.0)) {
		// Note that this deals correctly with a disabled vertex attribute.
		mat4 mSkin =
				inVertexWeights.x * joints[jids.x] +
				inVertexWeights.y * joints[jids.y] +
				inVertexWeights.z * joints[jids.z] +
				inVertexWeights.w * joints[jids.w];
		skinPos = (mSkin * vec4(inVertexPosition, 1.0)).xyz;
		skinNormal = (mSkin * vec4(inVertexNormal, 0.0)).xyz;
	}
#else
	vec3 skinPos = inVertexPosition;
	vec3 skinNormal = inVertexNormal;
#endif

	// TODO why is the normal unused?
	gl_Position = uWVPMatrix * vec4(skinPos, 1.0);
	gl_PointSize = uThickness;

	vec4 TextureCoord0 = vec4(inTexCoord0.x, inTexCoord0.y, 1.0, 1.0);
	vTextureCoord0 = vec4(uTMatrix0 * TextureCoord0).xy;

	vVertexColor = inVertexColor_raw.bgra;

	vec3 WorldPosition = (uWVMatrix * vec4(skinPos.xyz, 1.0)).xyz;

	vFogCoord = length(WorldPosition);
}
