#pragma once

namespace video
{

//! Enumeration for all vertex attributes there are.
enum E_VERTEX_ATTRIBUTES
{
	EVA_POSITION = 0,
	EVA_NORMAL,
	EVA_COLOR,
	EVA_TCOORD0,
	EVA_TCOORD1,
	EVA_TANGENT,
	EVA_BINORMAL,
	EVA_COUNT
};

//! Array holding the built in vertex attribute names
const char *const sBuiltInVertexAttributeNames[] = {
		"inVertexPosition",
		"inVertexNormal",
		"inVertexColor",
		"inTexCoord0",
		"inTexCoord1",
		"inVertexTangent",
		"inVertexBinormal",
		0,
	};

} // end namespace video
