uniform mat4 mWorld;

VARYING_ vec3 vNormal;
CENTROID_ VARYING_ mediump vec2 varTexCoord;

#ifdef USE_SKINNING
layout(std140) uniform JointMatrices {
    mat4 joints[MAX_JOINTS];
};
#endif

void main(void)
{
#ifdef USE_SKINNING
    uvec4 jids = inVertexJointIDs;
    vec4 skinPos = inVertexPosition;
    vec3 skinNormal = inVertexNormal;
    // Alternatively: Introduce neutral bone at index 0 with identity matrix?
    if (inVertexWeights != vec4(0.0)) {
        // Note that this deals correctly with a disabled vertex attribute.
        mat4 mSkin =
            inVertexWeights.x * joints[jids.x] +
                inVertexWeights.y * joints[jids.y] +
                inVertexWeights.z * joints[jids.z] +
                inVertexWeights.w * joints[jids.w];
        skinPos = vec4((mSkin * vec4(inVertexPosition.xyz, 1.0)).xyz, 1.0);
        skinNormal = (mSkin * vec4(inVertexNormal, 0.0)).xyz;
    }
#else
    vec4 skinPos = inVertexPosition;
    vec3 skinNormal = inVertexNormal;
#endif

    varTexCoord = (mTexture * vec4(inTexCoord0.xy, 1.0, 1.0)).st;

    gl_Position = mWorldViewProj * skinPos;

    vNormal = (mWorld * vec4(skinNormal, 0.0)).xyz;
}
