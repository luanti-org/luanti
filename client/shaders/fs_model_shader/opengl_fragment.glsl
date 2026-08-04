uniform sampler2D baseTexture;

VARYING_ vec3 vNormal;
CENTROID_ VARYING_ mediump vec2 varTexCoord;

void main(void)
{
    vec2 uv = varTexCoord.st;
    vec4 base = texture2D(baseTexture, uv).rgba;

    // Handle transparency by discarding pixel as appropriate.
    if (base.a == 0.0)
        discard;

    // TODO use vNormal for directional lighting, c.f. drawItemStack()
    gl_FragColor = vec4(base.rgb, 1.0);
}
