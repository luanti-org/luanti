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
	// Subpixel offset to render pixel-perfect 2D images
	// This is a compromise that is expected to work well on most drivers.
	// Explanation: https://jvm-gaming.org/t/pixel-perfect-rendering-in-2d/26261/4
	gl_Position = uProjection * (inVertexPosition + vec4(0.375, 0.375, 0.0, 0.0));
	gl_PointSize = uThickness;
	vTextureCoord = inTexCoord0;
	vVertexColor = inVertexColor.bgra;
}
