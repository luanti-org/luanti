uniform sampler2D baseTexture;
#define normalTexture texture1
uniform sampler2D normalTexture;
uniform vec3 yawVec;

VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;

void main (void)
{
	vec2 uv = varTexCoord.st;

	//texture sampling rate
	const float step = 1.0 / 256.0;
	float tl = texture2D(normalTexture, vec2(uv.x - step, uv.y + step)).r;
	float t  = texture2D(normalTexture, vec2(uv.x,        uv.y + step)).r;
	float tr = texture2D(normalTexture, vec2(uv.x + step, uv.y + step)).r;
	float r  = texture2D(normalTexture, vec2(uv.x + step, uv.y       )).r;
	float br = texture2D(normalTexture, vec2(uv.x + step, uv.y - step)).r;
	float b  = texture2D(normalTexture, vec2(uv.x,        uv.y - step)).r;
	float bl = texture2D(normalTexture, vec2(uv.x - step, uv.y - step)).r;
	float l  = texture2D(normalTexture, vec2(uv.x - step, uv.y       )).r;
	float dX = (tr + 2.0 * r + br) - (tl + 2.0 * l + bl);
	float dY = (bl + 2.0 * b + br) - (tl + 2.0 * t + tr);
	vec4 bump = vec4 (normalize(vec3 (dX, dY, 0.1)),1.0);
	float height = 2.0 * texture2D(normalTexture, vec2(uv.x, uv.y)).r - 1.0;
	vec4 base = texture2D(baseTexture, uv).rgba;
	vec3 L = normalize(vec3(0.0, 0.75, 1.0));
	float specular = pow(clamp(dot(reflect(L, bump.xyz), yawVec), 0.0, 1.0), 1.0);
	float diffuse = dot(yawVec, bump.xyz);

	vec3 color = (1.1 * diffuse + 0.05 * height + 0.5 * specular) * base.rgb;
	vec4 col = vec4(color.rgb, base.a);
	col *= varColor;
	gl_FragColor = vec4(col.rgb, base.a);
}
