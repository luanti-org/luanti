// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>,
// Copyright (C) 2012-2018 RealBadAngel, Maciej Kasatkin
// Copyright (C) 2015-2018 paramat

#include <stack>
#include "treegen.h"
#include "irr_v3d.h"
#include "util/pointer.h"
#include "util/numeric.h"
#include "servermap.h"
#include "mapblock.h"
#include "noise.h"
#include "voxelalgorithms.h"

namespace treegen
{

void TreeDef::resolveNodeNames()
{
	getIdFromNrBacklog(&trunknode.param0, "", CONTENT_IGNORE);
	getIdFromNrBacklog(&leavesnode.param0, "", CONTENT_IGNORE);
	if (leaves2_chance)
		getIdFromNrBacklog(&leaves2node.param0, "", CONTENT_IGNORE);
	getIdFromNrBacklog(&fruitnode.param0, "", CONTENT_IGNORE);
}

/*
	L-System tree gen helper functions

	NOTE: the PseudoRandom parameters here were probably accidentally used
	as by-value instead of by-reference. But don't change this now to keep
	the old behaviour.
*/
static void tree_trunk_placement(MMVManip &vmanip, v3f p0, const TreeDef &def);
static void tree_leaves_placement(MMVManip &vmanip, v3f p0,
	PseudoRandom ps, const TreeDef &def);
static void tree_single_leaves_placement(MMVManip &vmanip, v3f p0,
	PseudoRandom ps, const TreeDef &def);
static void tree_fruit_placement(MMVManip &vmanip, v3f p0, const TreeDef &def);

static void setRotationAxisRadians(core::matrix4 &M, float angle, v3f axis);
static v3f transposeMatrix(const core::matrix4 &M, v3f v);

void make_tree(MMVManip &vmanip, v3s16 p0, bool is_apple_tree,
	const NodeDefManager *ndef, s32 seed)
{
	/*
		NOTE: Tree-placing code is currently duplicated in the engine
		and in games that have saplings; both are deprecated but not
		replaced yet
	*/
	MapNode treenode(ndef->getId("mapgen_tree"));
	MapNode leavesnode(ndef->getId("mapgen_leaves"));
	MapNode applenode(ndef->getId("mapgen_apple"));
	if (treenode == CONTENT_IGNORE)
		errorstream << "Treegen: Mapgen alias 'mapgen_tree' is invalid!" << std::endl;
	if (leavesnode == CONTENT_IGNORE)
		errorstream << "Treegen: Mapgen alias 'mapgen_leaves' is invalid!" << std::endl;
	if (applenode == CONTENT_IGNORE)
		errorstream << "Treegen: Mapgen alias 'mapgen_apple' is invalid!" << std::endl;

	PseudoRandom pr(seed);
	s16 trunk_h = pr.range(4, 5);
	v3s16 p1 = p0;
	for (s16 ii = 0; ii < trunk_h; ii++) {
		if (vmanip.m_area.contains(p1)) {
			u32 vi = vmanip.m_area.index(p1);
			vmanip.m_data[vi] = treenode;
		}
		p1.Y++;
	}

	// p1 is now the last piece of the trunk
	p1.Y -= 1;

	VoxelArea leaves_a(v3s16(-2, -1, -2), v3s16(2, 2, 2));
	Buffer<u8> leaves_d(leaves_a.getVolume());
	for (u32 i = 0; i < leaves_d.getSize(); i++)
		leaves_d[i] = 0;

	// Force leaves at near the end of the trunk
	s16 d = 1;
	for (s16 z = -d; z <= d; z++)
	for (s16 y = -d; y <= d; y++)
	for (s16 x = -d; x <= d; x++) {
		leaves_d[leaves_a.index(v3s16(x, y, z))] = 1;
	}

	// Add leaves randomly
	for (u32 iii = 0; iii < 7; iii++) {
		v3s16 p(
			pr.range(leaves_a.MinEdge.X, leaves_a.MaxEdge.X - d),
			pr.range(leaves_a.MinEdge.Y, leaves_a.MaxEdge.Y - d),
			pr.range(leaves_a.MinEdge.Z, leaves_a.MaxEdge.Z - d)
		);

		for (s16 z = 0; z <= d; z++)
		for (s16 y = 0; y <= d; y++)
		for (s16 x = 0; x <= d; x++) {
			leaves_d[leaves_a.index(p + v3s16(x, y, z))] = 1;
		}
	}

	// Blit leaves to vmanip
	for (s16 z = leaves_a.MinEdge.Z; z <= leaves_a.MaxEdge.Z; z++)
	for (s16 y = leaves_a.MinEdge.Y; y <= leaves_a.MaxEdge.Y; y++) {
		v3s16 pmin(leaves_a.MinEdge.X, y, z);
		u32 i = leaves_a.index(pmin);
		u32 vi = vmanip.m_area.index(pmin + p1);
		for (s16 x = leaves_a.MinEdge.X; x <= leaves_a.MaxEdge.X; x++) {
			v3s16 p(x, y, z);
			if (vmanip.m_area.contains(p + p1) &&
					(vmanip.m_data[vi].getContent() == CONTENT_AIR ||
					vmanip.m_data[vi].getContent() == CONTENT_IGNORE)) {
				if (leaves_d[i] == 1) {
					bool is_apple = pr.range(0, 99) < 10;
					if (is_apple_tree && is_apple)
						vmanip.m_data[vi] = applenode;
					else
						vmanip.m_data[vi] = leavesnode;
				}
			}
			vi++;
			i++;
		}
	}
}


treegen::error spawn_ltree(ServerMap *map, v3s16 p0,
	const TreeDef &tree_definition)
{
	MMVManip vmanip(map);
	v3s16 tree_blockp = getNodeBlockPos(p0);

	vmanip.initialEmerge(tree_blockp - v3s16(1, 1, 1), tree_blockp + v3s16(1, 3, 1));
	treegen::error e = make_ltree(vmanip, p0, tree_definition);
	if (e != SUCCESS)
		return e;

	std::map<v3s16, MapBlock*> modified_blocks;
	voxalgo::blit_back_with_light(map, &vmanip, &modified_blocks);

	// Send a MEET_OTHER event
	MapEditEvent event;
	event.type = MEET_OTHER;
	event.setModifiedBlocks(modified_blocks);
	map->dispatchEvent(event);
	return SUCCESS;
}


treegen::error make_ltree(MMVManip &vmanip, v3s16 p0,
	const TreeDef &tree_definition)
{
	s32 seed;
	if (tree_definition.explicit_seed)
		seed = tree_definition.seed + 14002;
	else
		seed = p0.X * 2 + p0.Y * 4 + p0.Z;  // use the tree position to seed PRNG
	PseudoRandom ps(seed);

	// chance of inserting abcd rules
	constexpr float prop_a = 9;
	constexpr float prop_b = 8;
	constexpr float prop_c = 7;
	constexpr float prop_d = 6;

	//randomize tree growth level, minimum=2
	s16 iterations = tree_definition.iterations;
	if (tree_definition.iterations_random_level > 0)
		iterations -= ps.range(0, tree_definition.iterations_random_level);
	if (iterations < 2)
		iterations = 2;

	constexpr s16 MAX_ANGLE_OFFSET = 5;
	float angle_in_radians = tree_definition.angle * M_PI / 180;
	float angleOffset_in_radians = (s16)(ps.range(0, 1) % MAX_ANGLE_OFFSET) * M_PI / 180;

	//initialize rotation matrix, position and stacks for branches
	core::matrix4 rotation;
	setRotationAxisRadians(rotation, M_PI / 2, v3f(0, 0, 1));
	v3f position;
	position.X = p0.X;
	position.Y = p0.Y;
	position.Z = p0.Z;
	std::stack <core::matrix4> stack_orientation;
	std::stack <v3f> stack_position;

	//generate axiom
	std::string axiom = tree_definition.initial_axiom;
	for (s16 i = 0; i < iterations; i++) {
		std::string temp;
		for (s16 j = 0; j < (s16)axiom.size(); j++) {
			char axiom_char = axiom.at(j);
			switch (axiom_char) {
			case 'A':
				temp += tree_definition.rules_a;
				break;
			case 'B':
				temp += tree_definition.rules_b;
				break;
			case 'C':
				temp += tree_definition.rules_c;
				break;
			case 'D':
				temp += tree_definition.rules_d;
				break;
			case 'a':
				if (prop_a >= ps.range(1, 10))
					temp += tree_definition.rules_a;
				break;
			case 'b':
				if (prop_b >= ps.range(1, 10))
					temp += tree_definition.rules_b;
				break;
			case 'c':
				if (prop_c >= ps.range(1, 10))
					temp += tree_definition.rules_c;
				break;
			case 'd':
				if (prop_d >= ps.range(1, 10))
					temp += tree_definition.rules_d;
				break;
			default:
				temp += axiom_char;
				break;
			}
		}
		axiom = temp;
	}

	// Add trunk nodes below a wide trunk to avoid gaps when tree is on sloping ground
	if (tree_definition.trunk_type == "double") {
		tree_trunk_placement(
			vmanip,
			v3f(position.X + 1, position.Y - 1, position.Z),
			tree_definition
		);
		tree_trunk_placement(
			vmanip,
			v3f(position.X, position.Y - 1, position.Z + 1),
			tree_definition
		);
		tree_trunk_placement(
			vmanip,
			v3f(position.X + 1, position.Y - 1, position.Z + 1),
			tree_definition
		);
	} else if (tree_definition.trunk_type == "crossed") {
		tree_trunk_placement(
			vmanip,
			v3f(position.X + 1, position.Y - 1, position.Z),
			tree_definition
		);
		tree_trunk_placement(
			vmanip,
			v3f(position.X - 1, position.Y - 1, position.Z),
			tree_definition
		);
		tree_trunk_placement(
			vmanip,
			v3f(position.X, position.Y - 1, position.Z + 1),
			tree_definition
		);
		tree_trunk_placement(
			vmanip,
			v3f(position.X, position.Y - 1, position.Z - 1),
			tree_definition
		);
	}

	/* build tree out of generated axiom

	Key for Special L-System Symbols used in Axioms

	  G  - move forward one unit with the pen up
	  F  - move forward one unit with the pen down drawing trunks and branches
	  f  - move forward one unit with the pen down drawing leaves (100% chance)
	  T  - move forward one unit with the pen down drawing trunks only
	  R  - move forward one unit with the pen down placing fruit
	  A  - replace with rules set A
	  B  - replace with rules set B
	  C  - replace with rules set C
	  D  - replace with rules set D
	  a  - replace with rules set A, chance 90%
	  b  - replace with rules set B, chance 80%
	  c  - replace with rules set C, chance 70%
	  d  - replace with rules set D, chance 60%
	  +  - yaw the turtle right by angle degrees
	  -  - yaw the turtle left by angle degrees
	  &  - pitch the turtle down by angle degrees
	  ^  - pitch the turtle up by angle degrees
	  /  - roll the turtle to the right by angle degrees
	  *  - roll the turtle to the left by angle degrees
	  [  - save in stack current state info
	  ]  - recover from stack state info

	 */

	s16 x,y,z;
	for (s16 i = 0; i < (s16)axiom.size(); i++) {
		char axiom_char = axiom.at(i);
		core::matrix4 temp_rotation;
		temp_rotation.makeIdentity();
		v3f dir;
		switch (axiom_char) {
		case 'G':
			dir = v3f(1, 0, 0);
			dir = transposeMatrix(rotation, dir);
			position += dir;
			break;
		case 'T':
			tree_trunk_placement(
				vmanip,
				v3f(position.X, position.Y, position.Z),
				tree_definition
			);
			if (tree_definition.trunk_type == "double" &&
					!tree_definition.thin_branches) {
				tree_trunk_placement(
					vmanip,
					v3f(position.X + 1, position.Y, position.Z),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X, position.Y, position.Z + 1),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X + 1, position.Y, position.Z + 1),
					tree_definition
				);
			} else if (tree_definition.trunk_type == "crossed" &&
					!tree_definition.thin_branches) {
				tree_trunk_placement(
					vmanip,
					v3f(position.X + 1, position.Y, position.Z),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X - 1, position.Y, position.Z),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X, position.Y, position.Z + 1),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X, position.Y, position.Z - 1),
					tree_definition
				);
			}
			dir = v3f(1, 0, 0);
			dir = transposeMatrix(rotation, dir);
			position += dir;
			break;
		case 'F':
			tree_trunk_placement(
				vmanip,
				v3f(position.X, position.Y, position.Z),
				tree_definition
			);
			if ((stack_orientation.empty() &&
					tree_definition.trunk_type == "double") ||
					(!stack_orientation.empty() &&
					tree_definition.trunk_type == "double" &&
					!tree_definition.thin_branches)) {
				tree_trunk_placement(
					vmanip,
					v3f(position.X + 1, position.Y, position.Z),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X, position.Y, position.Z + 1),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X + 1, position.Y, position.Z + 1),
					tree_definition
				);
			} else if ((stack_orientation.empty() &&
					tree_definition.trunk_type == "crossed") ||
					(!stack_orientation.empty() &&
					tree_definition.trunk_type == "crossed" &&
					!tree_definition.thin_branches)) {
				tree_trunk_placement(
					vmanip,
					v3f(position.X + 1, position.Y, position.Z),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X - 1, position.Y, position.Z),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X, position.Y, position.Z + 1),
					tree_definition
				);
				tree_trunk_placement(
					vmanip,
					v3f(position.X, position.Y, position.Z - 1),
					tree_definition
				);
			}
			if (!stack_orientation.empty()) {
				s16 size = 1;
				for (x = -size; x <= size; x++)
				for (y = -size; y <= size; y++)
				for (z = -size; z <= size; z++) {
					if (abs(x) == size &&
							abs(y) == size &&
							abs(z) == size) {
						tree_leaves_placement(
							vmanip,
							v3f(position.X + x + 1, position.Y + y,
									position.Z + z),
							ps.next(),
							tree_definition
						);
						tree_leaves_placement(
							vmanip,
							v3f(position.X + x - 1, position.Y + y,
									position.Z + z),
							ps.next(),
							tree_definition
						);
						tree_leaves_placement(
							vmanip,v3f(position.X + x, position.Y + y,
									position.Z + z + 1),
							ps.next(),
							tree_definition
						);
						tree_leaves_placement(
							vmanip,v3f(position.X + x, position.Y + y,
									position.Z + z - 1),
							ps.next(),
							tree_definition
						);
					}
				}
			}
			dir = v3f(1, 0, 0);
			dir = transposeMatrix(rotation, dir);
			position += dir;
			break;
		case 'f':
			tree_single_leaves_placement(
				vmanip,
				v3f(position.X, position.Y, position.Z),
				ps.next(),
				tree_definition
			);
			dir = v3f(1, 0, 0);
			dir = transposeMatrix(rotation, dir);
			position += dir;
			break;
		case 'R':
			tree_fruit_placement(
				vmanip,
				v3f(position.X, position.Y, position.Z),
				tree_definition
			);
			dir = v3f(1, 0, 0);
			dir = transposeMatrix(rotation, dir);
			position += dir;
			break;

		// turtle orientation commands
		case '[':
			stack_orientation.push(rotation);
			stack_position.push(position);
			break;
		case ']':
			if (stack_orientation.empty())
				return UNBALANCED_BRACKETS;
			rotation = stack_orientation.top();
			stack_orientation.pop();
			position = stack_position.top();
			stack_position.pop();
			break;
		case '+':
			temp_rotation.makeIdentity();
			setRotationAxisRadians(temp_rotation,
					angle_in_radians + angleOffset_in_radians, v3f(0, 0, 1));
			rotation *= temp_rotation;
			break;
		case '-':
			temp_rotation.makeIdentity();
			setRotationAxisRadians(temp_rotation,
					angle_in_radians + angleOffset_in_radians, v3f(0, 0, -1));
			rotation *= temp_rotation;
			break;
		case '&':
			temp_rotation.makeIdentity();
			setRotationAxisRadians(temp_rotation,
					angle_in_radians + angleOffset_in_radians, v3f(0, 1, 0));
			rotation *= temp_rotation;
			break;
		case '^':
			temp_rotation.makeIdentity();
			setRotationAxisRadians(temp_rotation,
					angle_in_radians + angleOffset_in_radians, v3f(0, -1, 0));
			rotation *= temp_rotation;
			break;
		case '*':
			temp_rotation.makeIdentity();
			setRotationAxisRadians(temp_rotation,
					angle_in_radians, v3f(1, 0, 0));
			rotation *= temp_rotation;
			break;
		case '/':
			temp_rotation.makeIdentity();
			setRotationAxisRadians(temp_rotation,
					angle_in_radians, v3f(-1, 0, 0));
			rotation *= temp_rotation;
			break;
		default:
			break;
		}
	}

	return SUCCESS;
}


void tree_trunk_placement(MMVManip &vmanip, v3f p0, const TreeDef &tree_definition)
{
	v3s16 p1 = v3s16(myround(p0.X), myround(p0.Y), myround(p0.Z));
	if (!vmanip.m_area.contains(p1))
		return;
	u32 vi = vmanip.m_area.index(p1);
	content_t current_node = vmanip.m_data[vi].getContent();
	if (current_node != CONTENT_AIR && current_node != CONTENT_IGNORE
			&& current_node != tree_definition.leavesnode.getContent()
			&& current_node != tree_definition.leaves2node.getContent()
			&& current_node != tree_definition.fruitnode.getContent())
		return;
	vmanip.m_data[vi] = tree_definition.trunknode;
}


void tree_leaves_placement(MMVManip &vmanip, v3f p0,
		PseudoRandom ps, const TreeDef &tree_definition)
{
	MapNode leavesnode = tree_definition.leavesnode;
	if (ps.range(1, 100) > 100 - tree_definition.leaves2_chance)
		leavesnode = tree_definition.leaves2node;
	v3s16 p1 = v3s16(myround(p0.X), myround(p0.Y), myround(p0.Z));
	if (!vmanip.m_area.contains(p1))
		return;
	u32 vi = vmanip.m_area.index(p1);
	if (vmanip.m_data[vi].getContent() != CONTENT_AIR
			&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	if (tree_definition.fruit_chance > 0) {
		if (ps.range(1, 100) > 100 - tree_definition.fruit_chance)
			vmanip.m_data[vmanip.m_area.index(p1)] = tree_definition.fruitnode;
		else
			vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
	} else if (ps.range(1, 100) > 20) {
		vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
	}
}


void tree_single_leaves_placement(MMVManip &vmanip, v3f p0,
		PseudoRandom ps, const TreeDef &tree_definition)
{
	MapNode leavesnode = tree_definition.leavesnode;
	if (ps.range(1, 100) > 100 - tree_definition.leaves2_chance)
		leavesnode = tree_definition.leaves2node;
	v3s16 p1 = v3s16(myround(p0.X), myround(p0.Y), myround(p0.Z));
	if (!vmanip.m_area.contains(p1))
		return;
	u32 vi = vmanip.m_area.index(p1);
	if (vmanip.m_data[vi].getContent() != CONTENT_AIR
			&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
}


void tree_fruit_placement(MMVManip &vmanip, v3f p0, const TreeDef &tree_definition)
{
	v3s16 p1 = v3s16(myround(p0.X), myround(p0.Y), myround(p0.Z));
	if (!vmanip.m_area.contains(p1))
		return;
	u32 vi = vmanip.m_area.index(p1);
	if (vmanip.m_data[vi].getContent() != CONTENT_AIR
			&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = tree_definition.fruitnode;
}


void setRotationAxisRadians(core::matrix4 &M, float angle, v3f axis)
{
	float c = std::cos(angle);
	float s = std::sin(angle);
	float t = 1.0f - c;

	float tx  = t * axis.X;
	float ty  = t * axis.Y;
	float tz  = t * axis.Z;
	float sx  = s * axis.X;
	float sy  = s * axis.Y;
	float sz  = s * axis.Z;

	M[0] = tx * axis.X + c;
	M[1] = tx * axis.Y + sz;
	M[2] = tx * axis.Z - sy;

	M[4] = ty * axis.X - sz;
	M[5] = ty * axis.Y + c;
	M[6] = ty * axis.Z + sx;

	M[8]  = tz * axis.X + sy;
	M[9]  = tz * axis.Y - sx;
	M[10] = tz * axis.Z + c;
}


v3f transposeMatrix(const core::matrix4 &M, v3f v)
{
	v3f translated;
	translated.X = M[0] * v.X + M[4] * v.Y + M[8]  * v.Z + M[12];
	translated.Y = M[1] * v.X + M[5] * v.Y + M[9]  * v.Z + M[13];
	translated.Z = M[2] * v.X + M[6] * v.Y + M[10] * v.Z + M[14];
	return translated;
}


void make_jungletree(MMVManip &vmanip, v3s16 p0, const NodeDefManager *ndef,
	s32 seed)
{
	/*
		NOTE: Tree-placing code is currently duplicated in the engine
		and in games that have saplings; both are deprecated but not
		replaced yet
	*/
	content_t c_tree   = ndef->getId("mapgen_jungletree");
	content_t c_leaves = ndef->getId("mapgen_jungleleaves");
	if (c_tree == CONTENT_IGNORE)
		c_tree = ndef->getId("mapgen_tree");
	if (c_leaves == CONTENT_IGNORE)
		c_leaves = ndef->getId("mapgen_leaves");
	if (c_tree == CONTENT_IGNORE)
		errorstream << "Treegen: Mapgen alias 'mapgen_jungletree' is invalid!" << std::endl;
	if (c_leaves == CONTENT_IGNORE)
		errorstream << "Treegen: Mapgen alias 'mapgen_jungleleaves' is invalid!" << std::endl;

	MapNode treenode(c_tree);
	MapNode leavesnode(c_leaves);

	PseudoRandom pr(seed);
	for (s16 x= -1; x <= 1; x++)
	for (s16 z= -1; z <= 1; z++) {
		if (pr.range(0, 2) == 0)
			continue;
		v3s16 p1 = p0 + v3s16(x, 0, z);
		v3s16 p2 = p0 + v3s16(x, -1, z);
		u32 vi1 = vmanip.m_area.index(p1);
		u32 vi2 = vmanip.m_area.index(p2);

		if (vmanip.m_area.contains(p2) &&
				vmanip.m_data[vi2].getContent() == CONTENT_AIR)
			vmanip.m_data[vi2] = treenode;
		else if (vmanip.m_area.contains(p1) &&
				vmanip.m_data[vi1].getContent() == CONTENT_AIR)
			vmanip.m_data[vi1] = treenode;
	}
	vmanip.m_data[vmanip.m_area.index(p0)] = treenode;

	s16 trunk_h = pr.range(8, 12);
	v3s16 p1 = p0;
	for (s16 ii = 0; ii < trunk_h; ii++) {
		if (vmanip.m_area.contains(p1)) {
			u32 vi = vmanip.m_area.index(p1);
			vmanip.m_data[vi] = treenode;
		}
		p1.Y++;
	}

	// p1 is now the last piece of the trunk
	p1.Y -= 1;

	VoxelArea leaves_a(v3s16(-3, -2, -3), v3s16(3, 2, 3));
	Buffer<u8> leaves_d(leaves_a.getVolume());
	for (u32 i = 0; i < leaves_d.getSize(); i++)
		leaves_d[i] = 0;

	// Force leaves at near the end of the trunk
	s16 d = 1;
	for (s16 z = -d; z <= d; z++)
	for (s16 y = -d; y <= d; y++)
	for (s16 x = -d; x <= d; x++) {
		leaves_d[leaves_a.index(v3s16(x,y,z))] = 1;
	}

	// Add leaves randomly
	for (u32 iii = 0; iii < 30; iii++) {
		v3s16 p(
			pr.range(leaves_a.MinEdge.X, leaves_a.MaxEdge.X - d),
			pr.range(leaves_a.MinEdge.Y, leaves_a.MaxEdge.Y - d),
			pr.range(leaves_a.MinEdge.Z, leaves_a.MaxEdge.Z - d)
		);

		for (s16 z = 0; z <= d; z++)
		for (s16 y = 0; y <= d; y++)
		for (s16 x = 0; x <= d; x++) {
			leaves_d[leaves_a.index(p + v3s16(x, y, z))] = 1;
		}
	}

	// Blit leaves to vmanip
	for (s16 z = leaves_a.MinEdge.Z; z <= leaves_a.MaxEdge.Z; z++)
	for (s16 y = leaves_a.MinEdge.Y; y <= leaves_a.MaxEdge.Y; y++) {
		v3s16 pmin(leaves_a.MinEdge.X, y, z);
		u32 i = leaves_a.index(pmin);
		u32 vi = vmanip.m_area.index(pmin + p1);
		for (s16 x = leaves_a.MinEdge.X; x <= leaves_a.MaxEdge.X; x++) {
			v3s16 p(x, y, z);
			if (vmanip.m_area.contains(p + p1) &&
					(vmanip.m_data[vi].getContent() == CONTENT_AIR ||
					vmanip.m_data[vi].getContent() == CONTENT_IGNORE)) {
				if (leaves_d[i] == 1)
					vmanip.m_data[vi] = leavesnode;
			}
			vi++;
			i++;
		}
	}
}


void make_pine_tree(MMVManip &vmanip, v3s16 p0, const NodeDefManager *ndef,
	s32 seed)
{
	/*
		NOTE: Tree-placing code is currently duplicated in the engine
		and in games that have saplings; both are deprecated but not
		replaced yet
	*/
	content_t c_tree   = ndef->getId("mapgen_pine_tree");
	content_t c_leaves = ndef->getId("mapgen_pine_needles");
	content_t c_snow = ndef->getId("mapgen_snow");
	if (c_tree == CONTENT_IGNORE)
		c_tree = ndef->getId("mapgen_tree");
	if (c_leaves == CONTENT_IGNORE)
		c_leaves = ndef->getId("mapgen_leaves");
	if (c_snow == CONTENT_IGNORE)
		c_snow = CONTENT_AIR;
	if (c_tree == CONTENT_IGNORE)
		errorstream << "Treegen: Mapgen alias 'mapgen_pine_tree' is invalid!" << std::endl;
	if (c_leaves == CONTENT_IGNORE)
		errorstream << "Treegen: Mapgen alias 'mapgen_pine_needles' is invalid!" << std::endl;

	MapNode treenode(c_tree);
	MapNode leavesnode(c_leaves);
	MapNode snownode(c_snow);

	PseudoRandom pr(seed);
	u16 trunk_h = pr.range(9, 13);
	v3s16 p1 = p0;
	for (u16 ii = 0; ii < trunk_h; ii++) {
		if (vmanip.m_area.contains(p1)) {
			u32 vi = vmanip.m_area.index(p1);
			vmanip.m_data[vi] = treenode;
		}
		p1.Y++;
	}

	// Make p1 the top node of the trunk
	p1.Y -= 1;

	VoxelArea leaves_a(v3s16(-3, -6, -3), v3s16(3, 3, 3));
	Buffer<u8> leaves_d(leaves_a.getVolume());
	for (u32 i = 0; i < leaves_d.getSize(); i++)
		leaves_d[i] = 0;

	// Upper branches
	u16 dev = 3;
	for (s16 yy = -1; yy <= 1; yy++) {
		for (s16 zz = -dev; zz <= dev; zz++) {
			u32 i = leaves_a.index(v3s16(-dev, yy, zz));
			u32 ia = leaves_a.index(v3s16(-dev, yy+1, zz));
			for (s16 xx = -dev; xx <= dev; xx++) {
				if (pr.range(0, 20) <= 19 - dev) {
					leaves_d[i] = 1;
					leaves_d[ia] = 2;
				}
				i++;
				ia++;
			}
		}
		dev--;
	}

	// Center top nodes
	leaves_d[leaves_a.index(v3s16(0, 1, 0))] = 1;
	leaves_d[leaves_a.index(v3s16(0, 2, 0))] = 1;
	leaves_d[leaves_a.index(v3s16(0, 3, 0))] = 2;

	// Lower branches
	s16 my = -6;
	for (u32 iii = 0; iii < 20; iii++) {
		s16 xi = pr.range(-3, 2);
		s16 yy = pr.range(-6, -5);
		s16 zi = pr.range(-3, 2);
		if (yy > my)
			my = yy;
		for (s16 zz = zi; zz <= zi + 1; zz++) {
			u32 i = leaves_a.index(v3s16(xi, yy, zz));
			u32 ia = leaves_a.index(v3s16(xi, yy + 1, zz));
			for (s32 xx = xi; xx <= xi + 1; xx++) {
				leaves_d[i] = 1;
				if (leaves_d[ia] == 0)
					leaves_d[ia] = 2;
				i++;
				ia++;
			}
		}
	}

	dev = 2;
	for (s16 yy = my + 1; yy <= my + 2; yy++) {
		for (s16 zz = -dev; zz <= dev; zz++) {
			u32 i = leaves_a.index(v3s16(-dev, yy, zz));
			u32 ia = leaves_a.index(v3s16(-dev, yy + 1, zz));
			for (s16 xx = -dev; xx <= dev; xx++) {
				if (pr.range(0, 20) <= 19 - dev) {
					leaves_d[i] = 1;
					leaves_d[ia] = 2;
				}
				i++;
				ia++;
			}
		}
		dev--;
	}

	// Blit leaves to vmanip
	for (s16 z = leaves_a.MinEdge.Z; z <= leaves_a.MaxEdge.Z; z++)
	for (s16 y = leaves_a.MinEdge.Y; y <= leaves_a.MaxEdge.Y; y++) {
		v3s16 pmin(leaves_a.MinEdge.X, y, z);
		u32 i = leaves_a.index(pmin);
		u32 vi = vmanip.m_area.index(pmin + p1);
		for (s16 x = leaves_a.MinEdge.X; x <= leaves_a.MaxEdge.X; x++) {
			v3s16 p(x, y, z);
			if (vmanip.m_area.contains(p + p1) &&
					(vmanip.m_data[vi].getContent() == CONTENT_AIR ||
					vmanip.m_data[vi].getContent() == CONTENT_IGNORE ||
					vmanip.m_data[vi] == snownode)) {
				if (leaves_d[i] == 1)
					vmanip.m_data[vi] = leavesnode;
				else if (leaves_d[i] == 2)
					vmanip.m_data[vi] = snownode;
			}
			vi++;
			i++;
		}
	}
}

std::string error_to_string(error e)
{
	switch (e) {
		case SUCCESS:
			return "success";
		case UNBALANCED_BRACKETS:
			return "closing ']' has no matching opening bracket";
	}
	return "unknown error";
}

}; // namespace treegen
