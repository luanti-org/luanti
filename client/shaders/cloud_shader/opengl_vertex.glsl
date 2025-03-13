uniform lowp vec4 materialColor;

varying lowp vec4 varColor;

varying highp vec3 eyeVec;

void main(void)
{
	gl_Position = mWorldViewProj * inVertexPosition;

	vec4 color = inVertexColor;

	color *= materialColor;
	varColor = color;

	eyeVec = -(mWorldView * inVertexPosition).xyz;
}
