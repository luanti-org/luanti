// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti Authors

#include "catch.h"
#include "voxel.h"
#include "mapblock.h"

// Benchmark copyFrom and copyTo for a single block (32x32x32)
TEST_CASE("benchmark_voxelmanipulator_copy") {
	const v3s16 blockSize(MAP_BLOCKSIZE);

	// Source data buffer (simulates MapBlock's node data)
	MapNode src_data[MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE];
	// Destination data buffer
	MapNode dst_data[MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE];

	// Fill source with some content
	for (u32 i = 0; i < MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE; i++) {
		src_data[i] = MapNode(CONTENT_AIR);
	}

	VoxelArea data_area(v3s16(0), blockSize - v3s16(1));

	// Benchmark copyFrom: MapBlock -> VoxelManipulator
	BENCHMARK_ADVANCED("copyFrom_single_block")(Catch::Benchmark::Chronometer meter) {
		v3s16 pos(-MAP_BLOCKSIZE, -MAP_BLOCKSIZE, -MAP_BLOCKSIZE);
		VoxelArea vm_area(pos, pos + blockSize - v3s16(1));
		VoxelArea dst_area(pos);
		meter.measure([&] {
			VoxelManipulator vm;
			vm.addArea(vm_area);
			vm.copyFrom(src_data, false, data_area, dst_area, blockSize);
		});
	};

	// Benchmark copyTo: VoxelManipulator -> MapBlock
	BENCHMARK_ADVANCED("copyTo_single_block")(Catch::Benchmark::Chronometer meter) {
		v3s16 pos(-MAP_BLOCKSIZE, -MAP_BLOCKSIZE, -MAP_BLOCKSIZE);
		VoxelArea vm_area(pos, pos + blockSize - v3s16(1));
		VoxelArea src_area(pos);
		VoxelArea dst_area(pos);
		meter.measure([&] {
			VoxelManipulator vm;
			vm.addArea(vm_area);
			// Fill with data
			vm.copyFrom(src_data, false, data_area, src_area, blockSize);
			// Now copy back out
			vm.copyTo(dst_data, src_area, dst_area, blockSize);
		});
	};

	// Benchmark round-trip: copyFrom + copyTo
	BENCHMARK_ADVANCED("copyRoundTrip_single_block")(Catch::Benchmark::Chronometer meter) {
		v3s16 pos(-MAP_BLOCKSIZE, -MAP_BLOCKSIZE, -MAP_BLOCKSIZE);
		VoxelArea vm_area(pos, pos + blockSize - v3s16(1));
		VoxelArea vm_area_pos(pos);
		meter.measure([&] {
			VoxelManipulator vm;
			vm.addArea(vm_area);
			vm.copyFrom(src_data, false, data_area, vm_area_pos, blockSize);
			vm.copyTo(dst_data, vm_area_pos, data_area, blockSize);
		});
	};

	// Benchmark with mono block (single content type)
	BENCHMARK_ADVANCED("copyFrom_mono_block")(Catch::Benchmark::Chronometer meter) {
		v3s16 pos(-MAP_BLOCKSIZE, -MAP_BLOCKSIZE, -MAP_BLOCKSIZE);
		VoxelArea vm_area(pos, pos + blockSize - v3s16(1));
		VoxelArea dst_area(pos);
		meter.measure([&] {
			VoxelManipulator vm;
			vm.addArea(vm_area);
			vm.copyFrom(src_data, true, data_area, dst_area, blockSize);
		});
	};
}

// Benchmark with multiple blocks in a larger VoxelManipulator area
TEST_CASE("benchmark_voxelmanipulator_copy_multiple_blocks") {
	const v3s16 blockSize(MAP_BLOCKSIZE);
	const u32 num_blocks = 8;

	MapNode src_data[MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE];
	MapNode dst_data[MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE];

	for (u32 i = 0; i < MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE; i++) {
		src_data[i] = MapNode(CONTENT_AIR);
	}

	VoxelArea data_area(v3s16(0), blockSize - v3s16(1));

	BENCHMARK_ADVANCED("copyFrom_8_blocks")(Catch::Benchmark::Chronometer meter) {
		v3s16 area_min(-MAP_BLOCKSIZE * 2, -MAP_BLOCKSIZE * 2, -MAP_BLOCKSIZE * 2);
		v3s16 area_max = area_min + v3s16(MAP_BLOCKSIZE * 4) - v3s16(1);
		meter.measure([&] {
			VoxelManipulator vm;
			vm.addArea(VoxelArea(area_min, area_max));
			for (u32 i = 0; i < num_blocks; i++) {
				v3s16 pos(-MAP_BLOCKSIZE * 2 + (i % 2) * MAP_BLOCKSIZE,
				           -MAP_BLOCKSIZE * 2 + (i / 2) * MAP_BLOCKSIZE,
				           -MAP_BLOCKSIZE * 2 + (i / 4) * MAP_BLOCKSIZE);
				VoxelArea dst_area(pos);
				vm.copyFrom(src_data, false, data_area, dst_area, blockSize);
			}
		});
	};

	BENCHMARK_ADVANCED("copyTo_8_blocks")(Catch::Benchmark::Chronometer meter) {
		v3s16 area_min(-MAP_BLOCKSIZE * 2, -MAP_BLOCKSIZE * 2, -MAP_BLOCKSIZE * 2);
		v3s16 area_max = area_min + v3s16(MAP_BLOCKSIZE * 4) - v3s16(1);
		meter.measure([&] {
			VoxelManipulator vm;
			vm.addArea(VoxelArea(area_min, area_max));
			// Fill with data first
			for (u32 i = 0; i < num_blocks; i++) {
				v3s16 pos(-MAP_BLOCKSIZE * 2 + (i % 2) * MAP_BLOCKSIZE,
				           -MAP_BLOCKSIZE * 2 + (i / 2) * MAP_BLOCKSIZE,
				           -MAP_BLOCKSIZE * 2 + (i / 4) * MAP_BLOCKSIZE);
				VoxelArea area(pos);
				vm.copyFrom(src_data, false, data_area, area, blockSize);
			}
			// Now copy out
			for (u32 i = 0; i < num_blocks; i++) {
				v3s16 pos(-MAP_BLOCKSIZE * 2 + (i % 2) * MAP_BLOCKSIZE,
				           -MAP_BLOCKSIZE * 2 + (i / 2) * MAP_BLOCKSIZE,
				           -MAP_BLOCKSIZE * 2 + (i / 4) * MAP_BLOCKSIZE);
				VoxelArea area(pos);
				vm.copyTo(dst_data, area, data_area, blockSize);
			}
		});
	};
}
