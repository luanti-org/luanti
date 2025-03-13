// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 Nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "irr_v3d.h"

#include <map>
#include <vector>
#include <unordered_map>
#include <mutex>

/*
 * This class permits caching getFacePosition call results.
 * This reduces CPU usage and vector calls.
 */
class FacePositionCache {
public:
	static const std::vector<v3s16> &getFacePositions(u16 d);

private:
	static const std::vector<v3s16> &generateFacePosition(u16 d);
	static std::unordered_map<u16, std::vector<v3s16>> cache;
	static std::mutex cache_mutex;
};
