// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier, sapier at gmx dot net

#pragma once

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <vector>
#include "irr_v3d.h"

/******************************************************************************/
/* Forward declarations                                                       */
/******************************************************************************/

class NodeDefManager;
class Map;

/******************************************************************************/
/* Typedefs and macros                                                        */
/******************************************************************************/

typedef enum {
	DIR_XP,
	DIR_XM,
	DIR_ZP,
	DIR_ZM
} PathDirections;

/** List of supported algorithms */
typedef enum {
	PA_DIJKSTRA,           /**< Dijkstra shortest path algorithm             */
	PA_PLAIN,            /**< A* algorithm using heuristics to find a path */
	PA_PLAIN_NP          /**< A* algorithm without prefetching of map data */
} PathAlgorithm;

/******************************************************************************/
/* declarations                                                               */
/******************************************************************************/

/** c wrapper function to use from scriptapi */
std::vector<v3s16> get_path(Map *map, const NodeDefManager *ndef,
		v3s16 source,
		v3s16 destination,
		unsigned int searchdistance,
		unsigned int max_jump,
		unsigned int max_drop,
		PathAlgorithm algo);
