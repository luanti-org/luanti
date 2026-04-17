#version 100

precision mediump float;

/* Uniforms */

uniform float uAlphaRef;
uniform int uTextureUsage0;
uniform sampler2D uTextureUnit0;
uniform int uFogEnable;
uniform int uFogType;
uniform vec4 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uFogDensity;

/* Varyings */

varying vec2 vTextureCoord0;
varying vec4 vVertexColor;
varying float vFogCoord;

float computeFog()
{
	const float LOG2 = 1.442695;
	float FogFactor = 0.0;

	if (uFogType == 0) // Exp
	{
		FogFactor = exp2(-uFogDensity * vFogCoord * LOG2);
	}
	else if (uFogType == 1) // Linear
	{
		float Scale = 1.0 / (uFogEnd - uFogStart);
		FogFactor = (uFogEnd - vFogCoord) * Scale;
	}
	else if (uFogType == 2) // Exp2
	{
		FogFactor = exp2(-uFogDensity * uFogDensity * vFogCoord * vFogCoord * LOG2);
	}

	FogFactor = clamp(FogFactor, 0.0, 1.0);

	return FogFactor;
}

void main()
{
	vec4 Color = vVertexColor;

	if (bool(uTextureUsage0))
	{
		Color *= texture2D(uTextureUnit0, vTextureCoord0);

		// TODO: uAlphaRef should rather control sharpness of alpha, don't know how to do that right now and this works in most cases.
		if (Color.a < uAlphaRef)
			discard;
	}

	if (bool(uFogEnable))
	{
		float FogFactor = computeFog();
		vec4 FogColor = uFogColor;
		// Note: Robust against a bogus FogColor.a or FogFactor
		Color.rgb = mix(FogColor.rgb, Color.rgb, FogFactor);
	}

	gl_FragColor = Color;
}
