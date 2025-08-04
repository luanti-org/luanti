// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IAnimatedMesh.h"
#include "ISceneManager.h"
#include "CMeshBuffer.h"
#include "SSkinMeshBuffer.h"
#include "aabbox3d.h"
#include "irrMath.h"
#include "irrTypes.h"
#include "matrix4.h"
#include "quaternion.h"
#include "vector3d.h"
#include "Transform.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace scene
{

class IAnimatedMeshSceneNode;
class IBoneSceneNode;
class ISceneManager;

class SkinnedMesh : public IAnimatedMesh
{
public:

	enum class SourceFormat {
		B3D,
		X,
		GLTF,
		OTHER,
	};

	//! constructor
	SkinnedMesh(SourceFormat src_format) :
		EndFrame(0.f), FramesPerSecond(25.f),
		HasAnimation(false), PreparedForSkinning(false),
		AnimateNormals(true),
		SrcFormat(src_format)
	{
		SkinningBuffers = &LocalBuffers;
	}

	//! destructor
	virtual ~SkinnedMesh();

	//! The source (file) format the mesh was loaded from.
	//! Important for legacy reasons pertaining to different mesh loader behavior.
	SourceFormat getSourceFormat() const { return SrcFormat; }

	//! If the duration is 0, it is a static (=non animated) mesh.
	f32 getMaxFrameNumber() const override;

	//! Gets the default animation speed of the animated mesh.
	/** \return Amount of frames per second. If the amount is 0, it is a static, non animated mesh. */
	f32 getAnimationSpeed() const override;

	//! Gets the frame count of the animated mesh.
	/** \param fps Frames per second to play the animation with. If the amount is 0, it is not animated.
	The actual speed is set in the scene node the mesh is instantiated in.*/
	void setAnimationSpeed(f32 fps) override;

	//! Turns the given array of local matrices into an array of global matrices
	//! by multiplying with respective parent matrices.
	void calculateGlobalMatrices(std::vector<core::matrix4> &matrices) const;

	//! Performs a software skin on this mesh based on the given joint matrices
	void skinMesh(const std::vector<core::matrix4> &animated_transforms);

	//! returns amount of mesh buffers.
	u32 getMeshBufferCount() const override;

	//! returns pointer to a mesh buffer
	IMeshBuffer *getMeshBuffer(u32 nr) const override;

	//! Returns pointer to a mesh buffer which fits a material
	/** \param material: material to search for
	\return Returns the pointer to the mesh buffer or
	NULL if there is no such mesh buffer. */
	IMeshBuffer *getMeshBuffer(const video::SMaterial &material) const override;

	u32 getTextureSlot(u32 meshbufNr) const override;

	void setTextureSlot(u32 meshbufNr, u32 textureSlot);

	//! Returns bounding box of the mesh *in static pose*.
	const core::aabbox3d<f32> &getBoundingBox() const override {
		// TODO ideally we shouldn't be forced to implement this
		return StaticPoseBox;
	}

	//! Set bounding box of the mesh *in static pose*.
	void setBoundingBox(const core::aabbox3df &box) override {
		StaticPoseBox = box;
	}

	//! set the hardware mapping hint, for driver
	void setHardwareMappingHint(E_HARDWARE_MAPPING newMappingHint, E_BUFFER_TYPE buffer = EBT_VERTEX_AND_INDEX) override;

	//! flags the meshbuffer as changed, reloads hardware buffers
	void setDirty(E_BUFFER_TYPE buffer = EBT_VERTEX_AND_INDEX) override;

	//! Returns the type of the animated mesh.
	E_ANIMATED_MESH_TYPE getMeshType() const override {
		return EAMT_SKINNED;
	}

	//! Gets joint count.
	u32 getJointCount() const;

	//! Gets the name of a joint.
	/** \param number: Zero based index of joint.
	\return Name of joint and null if an error happened. */
	const std::optional<std::string> &getJointName(u32 number) const;

	//! Gets a joint number from its name
	/** \param name: Name of the joint.
	\return Number of the joint or std::nullopt if not found. */
	std::optional<u32> getJointNumber(const std::string &name) const;

	//! Update Normals when Animating
	/** \param on If false don't animate, which is faster.
	Else update normals, which allows for proper lighting of
	animated meshes (default). */
	void updateNormalsWhenAnimating(bool on) {
		AnimateNormals = on;
	}

	//! converts the vertex type of all meshbuffers to tangents.
	/** E.g. used for bump mapping. */
	void convertMeshToTangents();

	//! Does the mesh have no animation
	bool isStatic() const {
		return !HasAnimation;
	}

	//! Refreshes vertex data cached in joints such as positions and normals
	void refreshJointCache();

	//! Moves the mesh into static position.
	void resetAnimation();

	//! Creates an array of joints from this mesh as children of node
	std::vector<IBoneSceneNode *> addJoints(
			IAnimatedMeshSceneNode *node, ISceneManager *smgr);

	//! A vertex weight
	struct SWeight
	{
		//! Index of the mesh buffer
		u16 buffer_id; // I doubt 32bits is needed

		//! Index of the vertex
		u32 vertex_id; // Store global ID here

		//! Weight Strength/Percentage (0-1)
		f32 strength;

	private:
		//! Internal members used by SkinnedMesh
		friend class SkinnedMesh;
		char *Moved;
		core::vector3df StaticPos;
		core::vector3df StaticNormal;
	};

	template <class T>
	struct Channel {
		struct Frame {
			f32 time;
			T value;
		};
		std::vector<Frame> frames;
		bool interpolate = true;

		bool empty() const {
			return frames.empty();
		}

		f32 getEndFrame() const {
			return frames.empty() ? 0 : frames.back().time;
		}

		void pushBack(f32 time, const T &value) {
			frames.push_back({time, value});
		}

		void append(const Channel<T> &other) {
			frames.insert(frames.end(), other.frames.begin(), other.frames.end());
		}

		void cleanup() {
			if (frames.empty())
				return;

			std::vector<Frame> ordered;
			ordered.push_back(frames.front());
			// Drop out-of-order frames
			for (auto it = frames.begin() + 1; it != frames.end(); ++it) {
				if (it->time > ordered.back().time) {
					ordered.push_back(*it);
				}
			}
			frames.clear();
			// Drop redundant middle keys
			frames.push_back(ordered.front());
			for (u32 i = 1; i < ordered.size() - 1; ++i) {
				if (ordered[i - 1].value != ordered[i].value
						|| ordered[i + 1].value != ordered[i].value) {
					frames.push_back(ordered[i]);
				}
			}
			if (ordered.size() > 1)
				frames.push_back(ordered.back());
			frames.shrink_to_fit();
		}

		static core::quaternion interpolateValue(core::quaternion from, core::quaternion to, f32 time) {
			core::quaternion result;
			result.slerp(from, to, time);
			return result;
		}

		static core::vector3df interpolateValue(core::vector3df from, core::vector3df to, f32 time) {
			// Note: `from` and `to` are swapped here compared to quaternion slerp
			return to.getInterpolated(from, time);
		}

		std::optional<T> get(f32 time) const {
			if (frames.empty())
				return std::nullopt;

			const auto next = std::lower_bound(frames.begin(), frames.end(), time, [](const auto& frame, f32 time) {
				return frame.time < time;
			});
			if (next == frames.begin())
				return next->value;
			if (next == frames.end())
				return frames.back().value;

			const auto prev = next - 1;
			if (!interpolate)
				return prev->value;

			return interpolateValue(prev->value, next->value, (time - prev->time) / (next->time - prev->time));
		}
	};

	struct Keys {
		Channel<core::vector3df> position;
		Channel<core::quaternion> rotation;
		Channel<core::vector3df> scale;

		bool empty() const {
			return position.empty() && rotation.empty() && scale.empty();
		}

		void append(const Keys &other) {
			position.append(other.position);
			rotation.append(other.rotation);
			scale.append(other.scale);
		}

		f32 getEndFrame() const {
			return std::max({
				position.getEndFrame(),
				rotation.getEndFrame(),
				scale.getEndFrame()
			});
		}

		void updateTransform(f32 frame, core::Transform &transform) const
		{
			if (auto pos = position.get(frame))
				transform.translation = *pos;
			if (auto rot = rotation.get(frame))
				transform.rotation = *rot;
			if (auto scl = scale.get(frame))
				transform.scale = *scl;
		}

		void cleanup() {
			position.cleanup();
			rotation.cleanup();
			scale.cleanup();
		}
	};

	//! Joints
	struct SJoint
	{
		SJoint() {}

		//! The name of this joint
		std::optional<std::string> Name;

		//! Local transformation to be set by loaders. Mutated by animation.
		using VariantTransform = std::variant<core::Transform, core::matrix4>;
		VariantTransform transform{core::Transform{}};

		VariantTransform animate(f32 frame) const {
			if (keys.empty())
				return transform;

			if (std::holds_alternative<core::matrix4>(transform)) {
				// .x lets animations override matrix transforms entirely,
				// which is what we implement here.
				// .gltf does not allow animation of nodes using matrix transforms.
				// Note that a decomposition into a TRS transform need not exist!
				core::Transform trs;
				keys.updateTransform(frame, trs);
				return {trs};
			}

			auto trs = std::get<core::Transform>(transform);
			keys.updateTransform(frame, trs);
			return {trs};
		}


		//! List of attached meshes
		std::vector<u32> AttachedMeshes;

		// Animation keyframes for translation, rotation, scale
		Keys keys;

		//! Skin weights
		std::vector<SWeight> Weights;

		//! Bounding box of all affected vertices, in local space
		core::aabbox3df LocalBoundingBox{{0, 0, 0}};

		//! Unnecessary for loaders, will be overwritten on finalize
		core::matrix4 GlobalMatrix; // loaders may still choose to set this (temporarily) to calculate absolute vertex data.

		// The .x and .gltf formats pre-calculate this
		std::optional<core::matrix4> GlobalInversedMatrix;

		void setParent(SJoint *parent) {
			ParentJointID = parent ? parent->JointID : std::optional<u16>{};
		}

		u16 JointID; // TODO refactor away: pointers -> IDs (problem: .x loader abuses SJoint)
		std::optional<u16> ParentJointID;
	};

	//! Animates joints based on frame input
	std::vector<SJoint::VariantTransform> animateMesh(f32 frame);

	//! Calculates a bounding box given an animation in the form of global joint transforms.
	core::aabbox3df calculateBoundingBox(
			const std::vector<core::matrix4> &global_transforms);

	void recalculateBaseBoundingBoxes();

	const std::vector<SJoint *> &getAllJoints() const {
		return AllJoints;
	}

protected:
	bool checkForAnimation() const;

	void topoSortJoints();

	void prepareForSkinning();

	void calculateStaticBoundingBox();
	void calculateJointBoundingBoxes();
	void calculateBufferBoundingBoxes();

	void normalizeWeights();

	void calculateTangents(core::vector3df &normal,
			core::vector3df &tangent, core::vector3df &binormal,
			const core::vector3df &vt1, const core::vector3df &vt2, const core::vector3df &vt3,
			const core::vector2df &tc1, const core::vector2df &tc2, const core::vector2df &tc3);

	std::vector<SSkinMeshBuffer *> *SkinningBuffers; // Meshbuffer to skin, default is to skin localBuffers

	std::vector<SSkinMeshBuffer *> LocalBuffers;
	//! Mapping from meshbuffer number to bindable texture slot
	std::vector<u32> TextureSlots;

	//! Joints, topologically sorted (parents come before their children).
	std::vector<SJoint *> AllJoints;

	// bool can't be used here because std::vector<bool>
	// doesn't allow taking a reference to individual elements.
	std::vector<std::vector<char>> Vertices_Moved;

	//! Bounding box of just the static parts of the mesh
	core::aabbox3df StaticPartsBox{{0, 0, 0}};

	//! Bounding box of the mesh in static pose
	core::aabbox3df StaticPoseBox{{0, 0, 0}};

	f32 EndFrame;
	f32 FramesPerSecond;

	bool HasAnimation;
	bool PreparedForSkinning;
	bool AnimateNormals;

	SourceFormat SrcFormat;
};

// Interface for mesh loaders
class SkinnedMeshBuilder : public SkinnedMesh {
public:
	SkinnedMeshBuilder(SourceFormat src_format) : SkinnedMesh(src_format) {}

	//! loaders should call this after populating the mesh
	// returns *this, so do not try to drop the mesh builder instance
	SkinnedMesh *finalize();

	//! alternative method for adding joints
	std::vector<SJoint *> &getAllJoints() {
		return AllJoints;
	}

	//! Adds a new meshbuffer to the mesh, access it as last one
	SSkinMeshBuffer *addMeshBuffer();

	//! Adds a new meshbuffer to the mesh, access it as last one
	void addMeshBuffer(SSkinMeshBuffer *meshbuf);

	//! Adds a new joint to the mesh, access it as last one
	SJoint *addJoint(SJoint *parent = nullptr);

	void addPositionKey(SJoint *joint, f32 frame, core::vector3df pos);
	void addRotationKey(SJoint *joint, f32 frame, core::quaternion rotation);
	void addScaleKey(SJoint *joint, f32 frame, core::vector3df scale);

	//! Adds a new weight to the mesh, access it as last one
	SWeight *addWeight(SJoint *joint);
};

} // end namespace scene
