// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "test.h"
#include "voxel.h"

class TestVoxelArea : public TestBase
{
public:
	TestVoxelArea() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestVoxelArea"; }

	void runTests(IGameDef *gamedef);

	void test_addarea();
	void test_pad();
	void test_extent();
	void test_volume();
	void test_contains_voxelarea();
	void test_contains_point();
	void test_contains_i();
	void test_equal();
	void test_plus();
	void test_minor();
	void test_diff();
	void test_intersect();
	void test_index_xyz_all_pos();
	void test_index_xyz_x_neg();
	void test_index_xyz_y_neg();
	void test_index_xyz_z_neg();
	void test_index_xyz_xy_neg();
	void test_index_xyz_xz_neg();
	void test_index_xyz_yz_neg();
	void test_index_xyz_all_neg();
	void test_index_v3s16_all_pos();
	void test_index_v3s16_x_neg();
	void test_index_v3s16_y_neg();
	void test_index_v3s16_z_neg();
	void test_index_v3s16_xy_neg();
	void test_index_v3s16_xz_neg();
	void test_index_v3s16_yz_neg();
	void test_index_v3s16_all_neg();
	void test_add_x();
	void test_add_y();
	void test_add_z();
	void test_add_p();
};

static TestVoxelArea g_test_instance;

void TestVoxelArea::runTests(IGameDef *gamedef)
{
	TEST(test_addarea);
	TEST(test_pad);
	TEST(test_extent);
	TEST(test_volume);
	TEST(test_contains_voxelarea);
	TEST(test_contains_point);
	TEST(test_contains_i);
	TEST(test_equal);
	TEST(test_plus);
	TEST(test_minor);
	TEST(test_diff);
	TEST(test_intersect);
	TEST(test_index_xyz_all_pos);
	TEST(test_index_xyz_x_neg);
	TEST(test_index_xyz_y_neg);
	TEST(test_index_xyz_z_neg);
	TEST(test_index_xyz_xy_neg);
	TEST(test_index_xyz_xz_neg);
	TEST(test_index_xyz_yz_neg);
	TEST(test_index_xyz_all_neg);
	TEST(test_index_v3s16_all_pos);
	TEST(test_index_v3s16_x_neg);
	TEST(test_index_v3s16_y_neg);
	TEST(test_index_v3s16_z_neg);
	TEST(test_index_v3s16_xy_neg);
	TEST(test_index_v3s16_xz_neg);
	TEST(test_index_v3s16_yz_neg);
	TEST(test_index_v3s16_all_neg);
	TEST(test_add_x);
	TEST(test_add_y);
	TEST(test_add_z);
	TEST(test_add_p);
}

void TestVoxelArea::test_addarea()
{
	VoxelArea v1(v3s16(-1447, -9547, -875), v3s16(-147, 8854, 669));
	VoxelArea v2(v3s16(-887, -8779, -5478), v3s16(447, 4445, 4778));

	v1.addArea(v2);
	UASSERT(v1.MinEdge == v3s16(-1447, -9547, -5478));
	UASSERT(v1.MaxEdge == v3s16(447, 8854, 4778));
}

void TestVoxelArea::test_pad()
{
	VoxelArea v1(v3s16(-1447, -9547, -875), v3s16(-147, 8854, 669));
	auto old_extent = v1.getExtent();
	v1.pad(v3s16(100, 200, 300));

	UASSERT(v1.MinEdge == v3s16(-1547, -9747, -1175));
	UASSERT(v1.MaxEdge == v3s16(-47, 9054, 969));
	UASSERT(v1.getExtent() > old_extent);
}

void TestVoxelArea::test_extent()
{
	VoxelArea v1(v3s16(-1337, -547, -789), v3s16(-147, 447, 669));
	UASSERT(v1.getExtent() == v3s32(1191, 995, 1459));

	VoxelArea v2(v3s16(32493, -32507, 32752), v3s16(32508, -32492, 32767));
	UASSERT(v2.getExtent() == v3s32(16, 16, 16));

	// side length bigger than S16_MAX
	VoxelArea v3({-20000, 12, 34}, {20000, 12, 34});
	UASSERT(v3.getExtent() == v3s32(40001, 1, 1));

	UASSERT(VoxelArea().hasEmptyExtent());
	UASSERT(VoxelArea({2,3,4}, {1,2,3}).hasEmptyExtent());
	UASSERT(VoxelArea({2,3,4}, {2,2,3}).hasEmptyExtent());
}

void TestVoxelArea::test_volume()
{
	VoxelArea v1(v3s16(-1337, -547, -789), v3s16(-147, 447, 669));
	UASSERTEQ(u32, v1.getVolume(), 1728980655);

	VoxelArea v2(v3s16(32493, -32507, 32752), v3s16(32508, -32492, 32767));
	UASSERTEQ(u32, v2.getVolume(), 4096);

	// volume bigger than S32_MAX
	VoxelArea v3({1, 1, 1}, {1337, 1337, 1337});
	UASSERTEQ(u32, v3.getVolume(), 2389979753U);

	UASSERTEQ(u32, VoxelArea({2,3,4}, {1,2,3}).getVolume(), 0);
	UASSERTEQ(u32, VoxelArea({2,3,4}, {2,2,3}).getVolume(), 0);
}

void TestVoxelArea::test_contains_voxelarea()
{
	VoxelArea v1(v3s16(-1337, -9547, -789), v3s16(-147, 750, 669));
	UASSERTEQ(bool, v1.contains(VoxelArea(v3s16(-200, 10, 10), v3s16(-150, 10, 10))),
			true);
	UASSERTEQ(bool, v1.contains(VoxelArea(v3s16(-2550, 10, 10), v3s16(10, 10, 10))),
			false);
	UASSERTEQ(bool, v1.contains(VoxelArea(v3s16(-10, 10, 10), v3s16(3500, 10, 10))),
			false);
	UASSERTEQ(bool,
			v1.contains(VoxelArea(
					v3s16(-800, -400, 669), v3s16(-500, 200, 669))),
			true);
	UASSERTEQ(bool,
			v1.contains(VoxelArea(
					v3s16(-800, -400, 670), v3s16(-500, 200, 670))),
			false);
}

void TestVoxelArea::test_contains_point()
{
	VoxelArea v1(v3s16(-1337, -9547, -789), v3s16(-147, 750, 669));
	UASSERTEQ(bool, v1.contains(v3s16(-200, 10, 10)), true);
	UASSERTEQ(bool, v1.contains(v3s16(-10000, 10, 10)), false);
	UASSERTEQ(bool, v1.contains(v3s16(-100, 10000, 10)), false);
	UASSERTEQ(bool, v1.contains(v3s16(-100, 100, 10000)), false);
	UASSERTEQ(bool, v1.contains(v3s16(-100, 100, -10000)), false);
	UASSERTEQ(bool, v1.contains(v3s16(10000, 100, 10)), false);

	VoxelArea v2;
	UASSERTEQ(bool, v2.contains(v3s16(-200, 10, 10)), false);
	UASSERTEQ(bool, v2.contains(v3s16(0, 0, 0)), false);
	UASSERTEQ(bool, v2.contains(v3s16(1, 1, 1)), false);
	UASSERTEQ(bool, v2.contains(v3s16(-1, -1, -1)), false);
}

void TestVoxelArea::test_contains_i()
{
	VoxelArea v1(v3s16(-1337, -9547, -789), v3s16(-147, 750, 669));
	UASSERTEQ(bool, v1.contains(10), true);
	UASSERTEQ(bool, v1.contains(v1.getVolume()), false);
	UASSERTEQ(bool, v1.contains(v1.getVolume() - 1), true);
	UASSERTEQ(bool, v1.contains(v1.getVolume() + 1), false);
	UASSERTEQ(bool, v1.contains(-1), false)

	VoxelArea v2(v3s16(10, 10, 10), v3s16(30, 30, 30));
	UASSERTEQ(bool, v2.contains(10), true);
	UASSERTEQ(bool, v2.contains(0), true);
	UASSERTEQ(bool, v2.contains(-1), false);

	VoxelArea v3;
	UASSERTEQ(bool, v3.contains(0), false);
	UASSERTEQ(bool, v3.contains(-1), false);
	UASSERTEQ(bool, v3.contains(1), false);
	UASSERTEQ(bool, v3.contains(2), false);
}

void TestVoxelArea::test_equal()
{
	VoxelArea v1(v3s16(-1337, -9547, -789), v3s16(-147, 750, 669));
	UASSERTEQ(bool, v1 == VoxelArea(v3s16(-1337, -9547, -789), v3s16(-147, 750, 669)),
			true);
	UASSERTEQ(bool, v1 == VoxelArea(v3s16(-147, 0, 0), v3s16(0, 750, 669)), false);
	UASSERTEQ(bool, v1 == VoxelArea(v3s16(0, 0, 0), v3s16(0, 0, 0)), false);
}

void TestVoxelArea::test_plus()
{
	VoxelArea v1(v3s16(-10, -10, -10), v3s16(100, 100, 100));
	UASSERT(v1 + v3s16(10, 0, 0) ==
			VoxelArea(v3s16(0, -10, -10), v3s16(110, 100, 100)));
	UASSERT(v1 + v3s16(10, -10, 0) ==
			VoxelArea(v3s16(0, -20, -10), v3s16(110, 90, 100)));
	UASSERT(v1 + v3s16(0, 0, 35) ==
			VoxelArea(v3s16(-10, -10, 25), v3s16(100, 100, 135)));
}

void TestVoxelArea::test_minor()
{
	VoxelArea v1(v3s16(-10, -10, -10), v3s16(100, 100, 100));
	UASSERT(v1 - v3s16(10, 0, 0) ==
			VoxelArea(v3s16(-20, -10, -10), v3s16(90, 100, 100)));
	UASSERT(v1 - v3s16(10, -10, 0) ==
			VoxelArea(v3s16(-20, 0, -10), v3s16(90, 110, 100)));
	UASSERT(v1 - v3s16(0, 0, 35) ==
			VoxelArea(v3s16(-10, -10, -45), v3s16(100, 100, 65)));
}

void TestVoxelArea::test_diff()
{
	const VoxelArea v1({-10, -10, -10}, {100, 100, 100});
	std::vector<VoxelArea> res;

	v1.diff(VoxelArea({-10, -10, -10}, {99, 100, 100}), res);
	UASSERTEQ(auto, res.size(), 1U);
	UASSERT(res[0] == VoxelArea({100, -10, -10}, {100, 100, 100}));
	res.clear();

	v1.diff(VoxelArea({-10, -10, -10}, {100, 50, 80}), res);
	UASSERTEQ(auto, res.size(), 2U);
	UASSERT(res[0] == VoxelArea({-10, -10, 81}, {100, 100, 100}));
	UASSERT(res[1] == VoxelArea({-10, 51, -10}, {100, 100, 80}));
	res.clear();

	// edge cases
	v1.diff(v1, res);
	UASSERT(res.empty());
	v1.diff(VoxelArea(), res);
	UASSERTEQ(auto, res.size(), 1U);
	UASSERT(res[0] == v1);
}

void TestVoxelArea::test_intersect()
{
	VoxelArea v1({-10, -10, -10}, {10, 10, 10});
	VoxelArea v2({1, 2, 3}, {4, 5, 6});
	VoxelArea v3({11, 11, 11}, {11, 11, 11});
	VoxelArea v4({-11, -2, -10}, {10, 2, 11});
	UASSERT(v2.intersect(v1) == v2);
	UASSERT(!v2.intersect(v1).hasEmptyExtent());
	UASSERT(v1.intersect(v2) == v2.intersect(v1));
	UASSERT(v1.intersect(v3).hasEmptyExtent());
	UASSERT(v3.intersect(v1) == v1.intersect(v3));
	UASSERT(v1.intersect(v4) ==
			VoxelArea({-10, -2, -10}, {10, 2, 10}));

	// edge cases
	UASSERT(VoxelArea().intersect(v1).hasEmptyExtent());
	UASSERT(v1.intersect(VoxelArea()).hasEmptyExtent());
}

void TestVoxelArea::test_index_xyz_all_pos()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(156, 25, 236), 155);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(156, 25, 236), 1310722495);
}

void TestVoxelArea::test_index_xyz_x_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(-147, 25, 366), -148);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(-147, 25, 366), -821642064);
}

void TestVoxelArea::test_index_xyz_y_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(247, -269, 100), 246);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(247, -269, 100), -951958678);
}

void TestVoxelArea::test_index_xyz_z_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(244, 336, -887), 243);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(244, 336, -887), -190690273);
}

void TestVoxelArea::test_index_xyz_xy_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(-365, -47, 6978), -366);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(-365, -47, 6978), 1797427926);
}

void TestVoxelArea::test_index_xyz_yz_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(66, -58, -789), 65);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(66, -58, -789), 1439223357);
}

void TestVoxelArea::test_index_xyz_xz_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(-36, 589, -992), -37);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(-36, 589, -992), -1937179681);
}

void TestVoxelArea::test_index_xyz_all_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(-88, -99, -1474), -89);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(-88, -99, -1474), -1366133749);
}

void TestVoxelArea::test_index_v3s16_all_pos()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(v3s16(156, 25, 236)), 155);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(v3s16(156, 25, 236)), 1310722495);
}

void TestVoxelArea::test_index_v3s16_x_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(v3s16(-147, 25, 366)), -148);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(v3s16(-147, 25, 366)), -821642064);
}

void TestVoxelArea::test_index_v3s16_y_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(v3s16(247, -269, 100)), 246);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(v3s16(247, -269, 100)), -951958678);
}

void TestVoxelArea::test_index_v3s16_z_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(v3s16(244, 336, -887)), 243);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(v3s16(244, 336, -887)), -190690273);
}

void TestVoxelArea::test_index_v3s16_xy_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(v3s16(-365, -47, 6978)), -366);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(v3s16(-365, -47, 6978)), 1797427926);
}

void TestVoxelArea::test_index_v3s16_yz_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(v3s16(66, -58, -789)), 65);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(v3s16(66, -58, -789)), 1439223357);
}

void TestVoxelArea::test_index_v3s16_xz_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(v3s16(-36, 589, -992)), -37);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(v3s16(-36, 589, -992)), -1937179681);
}

void TestVoxelArea::test_index_v3s16_all_neg()
{
	VoxelArea v1;
	UASSERTEQ(s32, v1.index(v3s16(-88, -99, -1474)), -89);

	VoxelArea v2(v3s16(-147, -9547, -875), v3s16(756, 8854, 669));
	UASSERTEQ(s32, v2.index(v3s16(-88, -99, -1474)), -1366133749);
}

void TestVoxelArea::test_add_x()
{
	v3s32 extent;
	u32 i = 4;
	VoxelArea::add_x(extent, i, 8);
	UASSERTEQ(u32, i, 12)
}

void TestVoxelArea::test_add_y()
{
	v3s32 extent(740, 16, 87);
	u32 i = 8;
	VoxelArea::add_y(extent, i, 88);
	UASSERTEQ(u32, i, 65128)
}

void TestVoxelArea::test_add_z()
{
	v3s32 extent(114, 80, 256);
	u32 i = 4;
	VoxelArea::add_z(extent, i, 8);
	UASSERTEQ(u32, i, 72964)
}

void TestVoxelArea::test_add_p()
{
	v3s32 extent(33, 14, 742);
	v3s16 a(15, 12, 369);
	u32 i = 4;
	VoxelArea::add_p(extent, i, a);
	UASSERTEQ(u32, i, 170893)
}
