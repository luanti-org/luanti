// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "mapnode.h"

/*
	Legacy node definitions
*/

// Backwards compatibility for non-extended content types in v19
extern content_t trans_table_19[21][2];
MapNode mapnode_translate_to_internal(MapNode n_from, u8 version);

// Get legacy node name mapping for loading old blocks
class NameIdMapping;
void content_mapnode_get_name_id_mapping(NameIdMapping *nimap);
