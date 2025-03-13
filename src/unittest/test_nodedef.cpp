// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "gamedef.h"
#include "nodedef.h"
#include "network/networkprotocol.h"

#include <catch.h>

#include <ios>
#include <sstream>


TEST_CASE("Given a node definition, "
		"when we serialize and then deserialize it, "
		"then the deserialized one should be equal to the original.",
		"[nodedef]")
{
	ContentFeatures f;
	f.name = "default:stone";
	for (TileDef &tiledef : f.tiledef)
		tiledef.name = "default_stone.png";
	f.is_ground_content = true;

	std::ostringstream os(std::ios::binary);
	f.serialize(os, LATEST_PROTOCOL_VERSION);
	std::istringstream is(os.str(), std::ios::binary);
	ContentFeatures f2;
	f2.deSerialize(is, LATEST_PROTOCOL_VERSION);

	CHECK(f.walkable == f2.walkable);
	CHECK(f.node_box.type == f2.node_box.type);
}
