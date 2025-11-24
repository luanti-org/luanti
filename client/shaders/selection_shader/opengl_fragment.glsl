uniform sampler2D baseTexture;

VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;

void main(void)
{
	vec2 uv = varTexCoord.st;
	vec4 color = texture2D(baseTexture, uv);
	color.rgb *= varColor.rgb;
	gl_FragColor = color;
}
