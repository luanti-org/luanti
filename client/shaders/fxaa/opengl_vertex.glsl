uniform vec2 texelSize0;

#ifdef GL_ES
VARYING_ mediump vec2 varTexCoord;
#else
centroid VARYING_ vec2 varTexCoord;
#endif

VARYING_ vec2 sampleNW;
VARYING_ vec2 sampleNE;
VARYING_ vec2 sampleSW;
VARYING_ vec2 sampleSE;

/*
Based on
https://github.com/mattdesl/glsl-fxaa/
Portions Copyright (c) 2011 by Armin Ronacher.
*/
void main(void)
{
	varTexCoord.st = inTexCoord0.st;
	sampleNW = varTexCoord.st + vec2(-1.0, -1.0) * texelSize0;
	sampleNE = varTexCoord.st + vec2(1.0, -1.0) * texelSize0;
	sampleSW = varTexCoord.st + vec2(-1.0, 1.0) * texelSize0;
	sampleSE = varTexCoord.st + vec2(1.0, 1.0) * texelSize0;
	gl_Position = inVertexPosition;
}
