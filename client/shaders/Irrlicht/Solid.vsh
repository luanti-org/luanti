#version 100

/* Attributes */

attribute vec3 inVertexPosition;
attribute vec3 inVertexNormal;
attribute vec4 inVertexColor;
attribute vec2 inTexCoord0;

/* Uniforms */

uniform mat4 uWVPMatrix;
uniform mat4 uWVMatrix;
uniform mat4 uTMatrix0;

uniform float uThickness;

/* Varyings */

varying vec2 vTextureCoord0;
varying vec4 vVertexColor;
varying float vFogCoord;

void main()
{
	gl_Position = uWVPMatrix * vec4(inVertexPosition, 1.0);
	gl_PointSize = uThickness;

	vec4 TextureCoord0 = vec4(inTexCoord0.x, inTexCoord0.y, 1.0, 1.0);
	vTextureCoord0 = vec4(uTMatrix0 * TextureCoord0).xy;

	vVertexColor = inVertexColor.bgra;

	vec3 Position = (uWVMatrix * vec4(inVertexPosition, 1.0)).xyz;

	vFogCoord = length(Position);
}
