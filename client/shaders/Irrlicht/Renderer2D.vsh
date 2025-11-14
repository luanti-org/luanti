#version 100

/* Attributes */

attribute vec4 inVertexPosition;
attribute vec4 inVertexColor;
attribute vec2 inTexCoord0;

/* Uniforms */

uniform float uThickness;
uniform mat4 uProjection;

/* Varyings */

varying vec2 vTextureCoord;
varying vec4 vVertexColor;

void main()
{
	// Subpixel offset to fix 2D image distortion
	gl_Position = uProjection * (inVertexPosition + vec4(0.375, 0.375, 0.0, 0.0));
	gl_PointSize = uThickness;
	vTextureCoord = inTexCoord0;
	vVertexColor = inVertexColor.bgra;
}
