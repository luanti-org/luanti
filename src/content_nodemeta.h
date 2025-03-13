// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <iostream>

class NodeMetadataList;
class NodeTimerList;
class IItemDefManager;

/*
	Legacy nodemeta definitions
*/

void content_nodemeta_deserialize_legacy(std::istream &is,
		NodeMetadataList *meta, NodeTimerList *timers,
		IItemDefManager *item_def_mgr);
