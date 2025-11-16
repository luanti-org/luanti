// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch_amalgamated.hpp"
#include "content/subgames.h"
#include "filesys.h"

#include "irrlichttypes.h"
#include "irr_ptr.h"

#include "EDriverTypes.h"
#include "IFileSystem.h"
#include "IReadFile.h"
#include "ISceneManager.h"
#include "SkinnedMesh.h"
#include "irrlicht.h"

#include "catch.h"
#include "matrix4.h"

TEST_CASE("x") {

const auto gamespec = findSubgame("devtest");

if (!gamespec.isValid())
	SKIP();

SIrrlichtCreationParameters p;
p.DriverType = video::EDT_NULL;
auto *driver = createDeviceEx(p);

REQUIRE(driver);

auto *smgr = driver->getSceneManager();
const auto loadMesh = [&] (const io::path& filepath) {
	irr_ptr<io::IReadFile> file(driver->getFileSystem()->createAndOpenFile(filepath));
	REQUIRE(file);
	return smgr->getMesh(file.get());
};

const static auto model_stem = gamespec.gamemods_path +
		DIR_DELIM + "testentities" + DIR_DELIM + "models" + DIR_DELIM + "testentities_";

SECTION("cool guy") {
	const auto *mesh = dynamic_cast<scene::SkinnedMesh*>(loadMesh(model_stem + "cool_guy.x"));
	REQUIRE(mesh);
	REQUIRE(mesh->getMeshBufferCount() == 1);

	auto getJointId = [&](auto name) {
		return mesh->getJointNumber(name).value();
	};

	const auto root = getJointId("Root");
	const auto armature = getJointId("Armature");
	const auto armature_body = getJointId("Armature_body");
	const auto armature_arm_r = getJointId("Armature_arm_r");

	std::vector<core::matrix4> matrices;
	matrices.reserve(mesh->getJointCount());
	for (auto *joint : mesh->getAllJoints()) {
		if (const auto *matrix = std::get_if<core::matrix4>(&joint->transform))
			matrices.push_back(*matrix);
		else
		 	matrices.push_back(std::get<core::Transform>(joint->transform).buildMatrix());
	}
	auto local_matrices = matrices;
	mesh->calculateGlobalMatrices(matrices);

	SECTION("joints are topologically sorted") {
		REQUIRE(root < armature);
		REQUIRE(armature < armature_body);
		REQUIRE(armature_body < armature_arm_r);
	}

	SECTION("parents are correct") {
		const auto get_parent = [&](auto id) {
			return mesh->getAllJoints()[id]->ParentJointID;
		};
		REQUIRE(!get_parent(root));
		REQUIRE(get_parent(armature).value() == root);
		REQUIRE(get_parent(armature_body).value() == armature);
		REQUIRE(get_parent(armature_arm_r).value() == armature_body);
	}

	SECTION("local matrices are correct") {
		REQUIRE(local_matrices[root].equals(core::IdentityMatrix));
		REQUIRE(local_matrices[armature].equals(core::IdentityMatrix));
		REQUIRE(local_matrices[armature_body] == core::matrix4(
			-1,0,0,0,
			0,0,1,0,
			0,1,0,0,
			0,2.571201,0,1
		));
		REQUIRE(local_matrices[armature_arm_r] == core::matrix4(
			-0.047733,0.997488,-0.05233,0,
			0.901521,0.020464,-0.432251,0,
			-0.430095,-0.067809,-0.900233,
			0,-0.545315,0,1,1
		));
	}

	SECTION("global matrices are correct") {
		REQUIRE(matrices[armature_body] == local_matrices[armature_body]);
		REQUIRE(matrices[armature_arm_r] ==
			matrices[armature_body] * local_matrices[armature_arm_r]);
	}
}

driver->closeDevice();
driver->drop();
}
