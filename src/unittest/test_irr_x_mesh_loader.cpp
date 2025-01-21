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

irr::SIrrlichtCreationParameters p;
p.DriverType = video::EDT_NULL;
auto *driver = irr::createDeviceEx(p);
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
	const auto *mesh = dynamic_cast<irr::scene::SkinnedMesh*>(loadMesh(model_stem + "cool_guy.x"));
	REQUIRE(mesh);
	REQUIRE(mesh->getMeshBufferCount() == 1);

	auto getJoint = [&](auto name) {
		return mesh->getAllJoints()[mesh->getJointNumber(name).value()];
	};

	const auto *root = getJoint("Root");
	const auto *armature = getJoint("Armature");
	const auto *armature_body = getJoint("Armature_body");
	const auto *armature_arm_r = getJoint("Armature_arm_r");


	SECTION("local matrices are correct") {
		REQUIRE(root->LocalMatrix.equals(core::IdentityMatrix));
		REQUIRE(armature->LocalMatrix.equals(core::IdentityMatrix));
		REQUIRE(armature_body->LocalMatrix.equals(core::matrix4(
			-1,0,0,0,
			0,0,1,0,
			0,1,0,0,
			0,2.571201,0,1
		)));
		REQUIRE(armature_arm_r->LocalMatrix.equals(core::matrix4(
			-0.047733,0.997488,-0.05233,0,
			0.901521,0.020464,-0.432251,0,
			-0.430095,-0.067809,-0.900233,
			0,-0.545315,0,1,1
		)));
	}

	SECTION("global matrices are correct") {
		REQUIRE(armature_body->GlobalMatrix.equals(armature_body->LocalMatrix));
		REQUIRE(armature_arm_r->GlobalMatrix.equals(
			armature_body->GlobalMatrix * armature_arm_r->LocalMatrix));
	}
}

driver->closeDevice();
driver->drop();
}
