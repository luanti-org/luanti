// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "CGLTFMeshFileLoader.h"

#include "SMaterialLayer.h"
#include "coreutil.h"
#include "CSkinnedMesh.h"
#include "IAnimatedMesh.h"
#include "IReadFile.h"
#include "irrTypes.h"
#include "irr_ptr.h"
#include "matrix4.h"
#include "path.h"
#include "quaternion.h"
#include "vector2d.h"
#include "vector3d.h"
#include "os.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace irr {

/* Notes on the coordinate system.
 *
 * glTF uses a right-handed coordinate system where +Z is the
 * front-facing axis, and Irrlicht uses a left-handed coordinate
 * system where -Z is the front-facing axis.
 * We convert between them by mirroring the mesh across the X axis.
 * Doing this correctly requires negating the Z coordinate on
 * vertex positions and normals, and reversing the winding order
 * of the vertex indices.
 */

// Right-to-left handedness conversions

template <typename T>
static inline T convertHandedness(const T &t);

template <>
core::vector3df convertHandedness(const core::vector3df &p)
{
	return core::vector3df(p.X, p.Y, -p.Z);
}

template <>
core::quaternion convertHandedness(const core::quaternion &q)
{
	return core::quaternion(q.X, q.Y, -q.Z, q.W);
}

template <>
core::matrix4 convertHandedness(const core::matrix4 &mat)
{
	// Base transformation between left & right handed coordinate systems.
	static const core::matrix4 invertZ = core::matrix4(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, -1, 0,
			0, 0, 0, 1);
	// Convert from left-handed to right-handed,
	// then apply mat,
	// then convert from right-handed to left-handed.
	// Both conversions just invert Z.
	return invertZ * mat * invertZ;
}

namespace scene {

using SelfType = CGLTFMeshFileLoader;

template <class T>
SelfType::Accessor<T>
SelfType::Accessor<T>::sparseIndices(const tiniergltf::GlTF &model,
		const tiniergltf::AccessorSparseIndices &indices,
		const std::size_t count)
{
	const auto &view = model.bufferViews->at(indices.bufferView);
	const auto byteStride = view.byteStride.value_or(indices.elementSize());

	const auto &buffer = model.buffers->at(view.buffer);
	const auto source = buffer.data.data() + view.byteOffset + indices.byteOffset;

	return SelfType::Accessor<T>(source, byteStride, count);
}

template <class T>
SelfType::Accessor<T>
SelfType::Accessor<T>::sparseValues(const tiniergltf::GlTF &model,
		const tiniergltf::AccessorSparseValues &values,
		const std::size_t count,
		const std::size_t defaultByteStride)
{
	const auto &view = model.bufferViews->at(values.bufferView);
	const auto byteStride = view.byteStride.value_or(defaultByteStride);

	const auto &buffer = model.buffers->at(view.buffer);
	const auto source = buffer.data.data() + view.byteOffset + values.byteOffset;

	return SelfType::Accessor<T>(source, byteStride, count);
}

template <class T>
SelfType::Accessor<T>
SelfType::Accessor<T>::base(const tiniergltf::GlTF &model, std::size_t accessorIdx)
{
	const auto &accessor = model.accessors->at(accessorIdx);

	if (!accessor.bufferView.has_value()) {
		return Accessor<T>(accessor.count);
	}

	const auto &view = model.bufferViews->at(accessor.bufferView.value());
	const auto byteStride = view.byteStride.value_or(accessor.elementSize());

	const auto &buffer = model.buffers->at(view.buffer);
	const auto source = buffer.data.data() + view.byteOffset + accessor.byteOffset;

	return Accessor<T>(source, byteStride, accessor.count);
}

template <class T>
SelfType::Accessor<T>
SelfType::Accessor<T>::make(const tiniergltf::GlTF &model, std::size_t accessorIdx)
{
	const auto &accessor = model.accessors->at(accessorIdx);
	if (accessor.componentType != getComponentType() || accessor.type != getType())
		throw std::runtime_error("invalid accessor");

	const auto base = Accessor<T>::base(model, accessorIdx);

	if (accessor.sparse.has_value()) {
		std::vector<T> vec(accessor.count);
		for (std::size_t i = 0; i < accessor.count; ++i) {
			vec[i] = base.get(i);
		}
		const auto overriddenCount = accessor.sparse->count;
		const auto indicesAccessor = ([&]() -> AccessorVariant<u8, u16, u32> {
			switch (accessor.sparse->indices.componentType) {
			case tiniergltf::AccessorSparseIndices::ComponentType::UNSIGNED_BYTE:
				return Accessor<u8>::sparseIndices(model, accessor.sparse->indices, overriddenCount);
			case tiniergltf::AccessorSparseIndices::ComponentType::UNSIGNED_SHORT:
				return Accessor<u16>::sparseIndices(model, accessor.sparse->indices, overriddenCount);
			case tiniergltf::AccessorSparseIndices::ComponentType::UNSIGNED_INT:
				return Accessor<u32>::sparseIndices(model, accessor.sparse->indices, overriddenCount);
			}
			throw std::logic_error("invalid enum value");
		})();

		const auto valuesAccessor = Accessor<T>::sparseValues(model,
				accessor.sparse->values, overriddenCount,
				accessor.bufferView.has_value()
						? model.bufferViews->at(*accessor.bufferView).byteStride.value_or(accessor.elementSize())
						: accessor.elementSize());

		for (std::size_t i = 0; i < overriddenCount; ++i) {
			u32 index;
			std::visit([&](auto &&acc) { index = acc.get(i); }, indicesAccessor);
			if (index >= accessor.count)
				throw std::runtime_error("index out of bounds");
			vec[index] = valuesAccessor.get(i);
		}
		return Accessor<T>(vec, accessor.count);
	}

	return base;
}

#define ACCESSOR_TYPES(T, U, V)                                                             \
	template <>                                                                             \
	constexpr tiniergltf::Accessor::Type SelfType::Accessor<T>::getType()                   \
	{                                                                                       \
		return tiniergltf::Accessor::Type::U;                                               \
	}                                                                                       \
	template <>                                                                             \
	constexpr tiniergltf::Accessor::ComponentType SelfType::Accessor<T>::getComponentType() \
	{                                                                                       \
		return tiniergltf::Accessor::ComponentType::V;                                      \
	}

#define VEC_ACCESSOR_TYPES(T, U, N)                                                                        \
	template <>                                                                                            \
	constexpr tiniergltf::Accessor::Type SelfType::Accessor<std::array<T, N>>::getType()                   \
	{                                                                                                      \
		return tiniergltf::Accessor::Type::VEC##N;                                                         \
	}                                                                                                      \
	template <>                                                                                            \
	constexpr tiniergltf::Accessor::ComponentType SelfType::Accessor<std::array<T, N>>::getComponentType() \
	{                                                                                                      \
		return tiniergltf::Accessor::ComponentType::U;                                                     \
	}                                                                                                      \
	template <>                                                                                            \
	std::array<T, N> SelfType::rawget(const char *ptr)                                                       \
	{                                                                                                      \
		std::array<T, N> res;                                                                              \
		for (int i = 0; i < N; ++i)                                                                         \
			res[i] = rawget<T>(ptr + sizeof(T) * i);                                                       \
		return res;                                                                                        \
	}

#define ACCESSOR_PRIMITIVE(T, U) \
	ACCESSOR_TYPES(T, SCALAR, U) \
	VEC_ACCESSOR_TYPES(T, U, 2)  \
	VEC_ACCESSOR_TYPES(T, U, 3)  \
	VEC_ACCESSOR_TYPES(T, U, 4)

ACCESSOR_PRIMITIVE(f32, FLOAT)
ACCESSOR_PRIMITIVE(u8, UNSIGNED_BYTE)
ACCESSOR_PRIMITIVE(u16, UNSIGNED_SHORT)
ACCESSOR_PRIMITIVE(u32, UNSIGNED_INT)

ACCESSOR_TYPES(core::vector3df, VEC3, FLOAT)
ACCESSOR_TYPES(core::quaternion, VEC4, FLOAT)
ACCESSOR_TYPES(core::matrix4, MAT4, FLOAT)

template <class T>
T SelfType::Accessor<T>::get(std::size_t i) const
{
	// Buffer-based accessor: Read directly from the buffer.
	if (std::holds_alternative<BufferSource>(source)) {
		const auto bufsrc = std::get<BufferSource>(source);
		return rawget<T>(bufsrc.ptr + i * bufsrc.byteStride);
	}
	// Array-based accessor (used for sparse accessors): Read from array.
	if (std::holds_alternative<std::vector<T>>(source)) {
		return std::get<std::vector<T>>(source)[i];
	}
	// Default-initialized accessor.
	// We differ slightly from glTF here in that
	// we default-initialize quaternions and matrices properly,
	// but this does not cause any discrepancies for valid glTF models.
	std::get<std::tuple<>>(source);
	return T();
}

template <typename T>
T SelfType::rawget(const char *ptr)
{
	T dest;
	std::memcpy(&dest, ptr, sizeof(dest));
#ifdef __BIG_ENDIAN__
	return os::Byteswap::byteswap(dest);
#else
	return dest;
#endif
}

// Note that these "more specialized templates" should win.

template <>
core::matrix4 SelfType::rawget(const char *ptr)
{
	core::matrix4 mat;
	for (u8 i = 0; i < 16; ++i) {
		mat[i] = rawget<f32>(ptr + i * sizeof(f32));
	}
	return mat;
}

template <>
core::vector3df SelfType::rawget(const char *ptr)
{
	return core::vector3df(
			rawget<f32>(ptr),
			rawget<f32>(ptr + sizeof(f32)),
			rawget<f32>(ptr + 2 * sizeof(f32)));
}

template <>
core::quaternion SelfType::rawget(const char *ptr)
{
	return core::quaternion(
			rawget<f32>(ptr),
			rawget<f32>(ptr + sizeof(f32)),
			rawget<f32>(ptr + 2 * sizeof(f32)),
			rawget<f32>(ptr + 3 * sizeof(f32)));
}

template <std::size_t N>
SelfType::NormalizedValuesAccessor<N>
SelfType::createNormalizedValuesAccessor(
		const tiniergltf::GlTF &model,
		const std::size_t accessorIdx)
{
	const auto &acc = model.accessors->at(accessorIdx);
	switch (acc.componentType) {
	case tiniergltf::Accessor::ComponentType::UNSIGNED_BYTE:
		return Accessor<std::array<u8, N>>::make(model, accessorIdx);
	case tiniergltf::Accessor::ComponentType::UNSIGNED_SHORT:
		return Accessor<std::array<u16, N>>::make(model, accessorIdx);
	case tiniergltf::Accessor::ComponentType::FLOAT:
		return Accessor<std::array<f32, N>>::make(model, accessorIdx);
	default:
		throw std::runtime_error("invalid component type");
	}
}

template <std::size_t N>
std::array<f32, N> SelfType::getNormalizedValues(
		const NormalizedValuesAccessor<N> &accessor,
		const std::size_t i)
{
	std::array<f32, N> values;
	if (std::holds_alternative<Accessor<std::array<u8, N>>>(accessor)) {
		const auto u8s = std::get<Accessor<std::array<u8, N>>>(accessor).get(i);
		for (u8 i = 0; i < N; ++i)
			values[i] = static_cast<f32>(u8s[i]) / std::numeric_limits<u8>::max();
	} else if (std::holds_alternative<Accessor<std::array<u16, N>>>(accessor)) {
		const auto u16s = std::get<Accessor<std::array<u16, N>>>(accessor).get(i);
		for (u8 i = 0; i < N; ++i)
			values[i] = static_cast<f32>(u16s[i]) / std::numeric_limits<u16>::max();
	} else {
		values = std::get<Accessor<std::array<f32, N>>>(accessor).get(i);
		for (u8 i = 0; i < N; ++i) {
			if (values[i] < 0 || values[i] > 1)
				throw std::runtime_error("invalid normalized value");
		}
	}
	return values;
}

bool SelfType::isALoadableFileExtension(
		const io::path& filename) const
{
	return core::hasFileExtension(filename, "gltf") ||
			core::hasFileExtension(filename, "glb");
}

/**
 * Entry point into loading a GLTF model.
*/
IAnimatedMesh* SelfType::createMesh(io::IReadFile* file)
{
	const char *filename = file->getFileName().c_str();
	try {
		tiniergltf::GlTF model = parseGLTF(file);
		irr_ptr<CSkinnedMesh> mesh(new CSkinnedMesh());
		MeshExtractor extractor(std::move(model), mesh.get());
		try {
			extractor.load();
			for (const auto &warning : extractor.getWarnings()) {
				os::Printer::log(filename, warning.c_str(), ELL_WARNING);
			}
			return mesh.release();
		} catch (const std::runtime_error &e) {
			os::Printer::log("error converting gltf to irrlicht mesh", e.what(), ELL_ERROR);
		}
	} catch (const std::runtime_error &e) {
		os::Printer::log("error parsing gltf", e.what(), ELL_ERROR);
	}
	return nullptr;
}

static void transformVertices(std::vector<video::S3DVertex> &vertices, const core::matrix4 &transform)
{
	for (auto &vertex : vertices) {
		// Apply scaling, rotation and rotation (in that order) to the position.
		transform.transformVect(vertex.Pos);
		// For the normal, we do not want to apply the translation.
		vertex.Normal = transform.rotateAndScaleVect(vertex.Normal);
		// Renormalize (length might have been affected by scaling).
		vertex.Normal.normalize();
	}
}

static void checkIndices(const std::vector<u16> &indices, const std::size_t nVerts)
{
	for (u16 index : indices) {
		if (index >= nVerts)
			throw std::runtime_error("index out of bounds");
	}
}

static std::vector<u16> generateIndices(const std::size_t nVerts)
{
	std::vector<u16> indices(nVerts);
	for (std::size_t i = 0; i < nVerts; i += 3) {
		// Reverse winding order per triangle
		indices[i] = i + 2;
		indices[i + 1] = i + 1;
		indices[i + 2] = i;
	}
	return indices;
}

using Wrap = tiniergltf::Sampler::Wrap;
static video::E_TEXTURE_CLAMP convertTextureWrap(const Wrap wrap) {
	switch (wrap) {
		case Wrap::REPEAT:
			return video::ETC_REPEAT;
		case Wrap::CLAMP_TO_EDGE:
			return video::ETC_CLAMP_TO_EDGE;
		case Wrap::MIRRORED_REPEAT:
            return video::ETC_MIRROR;
		default:
			throw std::runtime_error("invalid sampler wrapping mode");
    }
}

void SelfType::MeshExtractor::addPrimitive(
		const tiniergltf::MeshPrimitive &primitive,
		const std::optional<std::size_t> skinIdx,
		CSkinnedMesh::SJoint *parent)
{
	auto vertices = getVertices(primitive);
	if (!vertices.has_value())
		return; // "When positions are not specified, client implementations SHOULD skip primitive’s rendering"

	const auto n_vertices = vertices->size();

	// Excludes the max value for consistency.
	if (n_vertices >= std::numeric_limits<u16>::max())
		throw std::runtime_error("too many vertices");

	// Apply the global transform along the parent chain.
	transformVertices(*vertices, parent->GlobalMatrix);

	auto maybeIndices = getIndices(primitive);
	std::vector<u16> indices;
	if (maybeIndices.has_value()) {
		indices = std::move(*maybeIndices);
		checkIndices(indices, vertices->size());
	} else {
		// Non-indexed geometry
		indices = generateIndices(vertices->size());
	}

	m_irr_model->addMeshBuffer(
			new SSkinMeshBuffer(std::move(*vertices), std::move(indices)));
	const auto meshbufNr = m_irr_model->getMeshBufferCount() - 1;
	auto *meshbuf = m_irr_model->getMeshBuffer(meshbufNr);

	if (primitive.material.has_value()) {
		const auto &material = m_gltf_model.materials->at(*primitive.material);
		if (material.pbrMetallicRoughness.has_value()) {
			const auto &texture = material.pbrMetallicRoughness->baseColorTexture;
			if (texture.has_value()) {
				m_irr_model->setTextureSlot(meshbufNr, static_cast<u32>(texture->index));
				const auto samplerIdx = m_gltf_model.textures->at(texture->index).sampler;
				if (samplerIdx.has_value()) {
					auto &sampler = m_gltf_model.samplers->at(*samplerIdx);
					auto &layer = meshbuf->getMaterial().TextureLayers[0];
					layer.TextureWrapU = convertTextureWrap(sampler.wrapS);
					layer.TextureWrapV = convertTextureWrap(sampler.wrapT);
				}
			}
		}
	}

	if (!skinIdx.has_value()) {
		// No skin => all vertices belong entirely to their parent
		for (std::size_t v = 0; v < n_vertices; ++v) {
			auto *weight = m_irr_model->addWeight(parent);
			weight->buffer_id = meshbufNr;
			weight->vertex_id = v;
			weight->strength = 1.0f;
		}
		return;
	}

	const auto &skin = m_gltf_model.skins->at(*skinIdx);

	const auto &attrs = primitive.attributes;
	const auto &joints = attrs.joints;
	if (!joints.has_value())
		return;

	const auto &weights = attrs.weights;
	for (std::size_t set = 0; set < joints->size(); ++set) {
		const auto jointAccessor = ([&]() -> ArrayAccessorVariant<4, u8, u16> {
			const auto idx = joints->at(set);
			const auto &acc = m_gltf_model.accessors->at(idx);

			switch (acc.componentType) {
			case tiniergltf::Accessor::ComponentType::UNSIGNED_BYTE:
				return Accessor<std::array<u8, 4>>::make(m_gltf_model, idx);
			case tiniergltf::Accessor::ComponentType::UNSIGNED_SHORT:
				return Accessor<std::array<u16, 4>>::make(m_gltf_model, idx);
			default:
				throw std::runtime_error("invalid component type");
			}
		})();

		const auto weightAccessor = createNormalizedValuesAccessor<4>(m_gltf_model, weights->at(set));

		for (std::size_t v = 0; v < n_vertices; ++v) {
			std::array<u16, 4> jointIdxs;
			if (std::holds_alternative<Accessor<std::array<u8, 4>>>(jointAccessor)) {
				const auto jointIdxsU8 = std::get<Accessor<std::array<u8, 4>>>(jointAccessor).get(v);
				jointIdxs = {jointIdxsU8[0], jointIdxsU8[1], jointIdxsU8[2], jointIdxsU8[3]};
			} else if (std::holds_alternative<Accessor<std::array<u16, 4>>>(jointAccessor)) {
				jointIdxs = std::get<Accessor<std::array<u16, 4>>>(jointAccessor).get(v);
			}
			std::array<f32, 4> strengths = getNormalizedValues(weightAccessor, v);

			// 4 joints per set
			for (std::size_t in_set = 0; in_set < 4; ++in_set) {
				u16 jointIdx = jointIdxs[in_set];
				f32 strength = strengths[in_set];
				if (strength == 0)
					continue;

				CSkinnedMesh::SWeight *weight = m_irr_model->addWeight(m_loaded_nodes.at(skin.joints.at(jointIdx)));
				weight->buffer_id = meshbufNr;
				weight->vertex_id = v;
				weight->strength = strength;
			}
		}
	}
}

/**
 * Load up the rawest form of the model. The vertex positions and indices.
 * Documentation: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes
 * If material is undefined, then a default material MUST be used.
 */
void SelfType::MeshExtractor::deferAddMesh(
		const std::size_t meshIdx,
		const std::optional<std::size_t> skinIdx,
		CSkinnedMesh::SJoint *parent)
{
	m_mesh_loaders.emplace_back([=] {
		for (std::size_t pi = 0; pi < getPrimitiveCount(meshIdx); ++pi) {
			const auto &primitive = m_gltf_model.meshes->at(meshIdx).primitives.at(pi);
			addPrimitive(primitive, skinIdx, parent);
		}
	});
}

// Base transformation between left & right handed coordinate systems.
// This just inverts the Z axis.
static const core::matrix4 leftToRight = core::matrix4(
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, -1, 0,
	0, 0, 0, 1
);
static const core::matrix4 rightToLeft = leftToRight;

static core::matrix4 loadTransform(const tiniergltf::Node::Matrix &m, CSkinnedMesh::SJoint *joint)
{
	// Note: Under the hood, this casts these doubles to floats.
	core::matrix4 mat = convertHandedness(core::matrix4(
			m[0], m[1], m[2], m[3],
			m[4], m[5], m[6], m[7],
			m[8], m[9], m[10], m[11],
			m[12], m[13], m[14], m[15]));

	// Decompose the matrix into translation, scale, and rotation.
	joint->Animatedposition = mat.getTranslation();

	auto scale = mat.getScale();
	joint->Animatedscale = scale;
	core::matrix4 inverseScale;
	inverseScale.setScale(core::vector3df(
			scale.X == 0 ? 0 : 1 / scale.X,
			scale.Y == 0 ? 0 : 1 / scale.Y,
			scale.Z == 0 ? 0 : 1 / scale.Z));

	core::matrix4 axisNormalizedMat = inverseScale * mat;
	joint->Animatedrotation = axisNormalizedMat.getRotationDegrees();
	// Invert the rotation because it is applied using `getMatrix_transposed`,
	// which again inverts.
	joint->Animatedrotation.makeInverse();

	return mat;
}

static core::matrix4 loadTransform(const tiniergltf::Node::TRS &trs, CSkinnedMesh::SJoint *joint)
{
	const auto &trans = trs.translation;
	const auto &rot = trs.rotation;
	const auto &scale = trs.scale;
	core::matrix4 transMat;
	joint->Animatedposition = convertHandedness(core::vector3df(trans[0], trans[1], trans[2]));
	transMat.setTranslation(joint->Animatedposition);
	core::matrix4 rotMat;
	joint->Animatedrotation = convertHandedness(core::quaternion(rot[0], rot[1], rot[2], rot[3]));
	core::quaternion(joint->Animatedrotation).getMatrix_transposed(rotMat);
	joint->Animatedscale = core::vector3df(scale[0], scale[1], scale[2]);
	core::matrix4 scaleMat;
	scaleMat.setScale(joint->Animatedscale);
	return transMat * rotMat * scaleMat;
}

static core::matrix4 loadTransform(std::optional<std::variant<tiniergltf::Node::Matrix, tiniergltf::Node::TRS>> transform,
		CSkinnedMesh::SJoint *joint) {
	if (!transform.has_value()) {
		return core::matrix4();
	}
	return std::visit([joint](const auto &t) { return loadTransform(t, joint); }, *transform);
}

void SelfType::MeshExtractor::loadNode(
		const std::size_t nodeIdx,
		CSkinnedMesh::SJoint *parent)
{
	const auto &node = m_gltf_model.nodes->at(nodeIdx);
	auto *joint = m_irr_model->addJoint(parent);
	const core::matrix4 transform = loadTransform(node.transform, joint);
	joint->LocalMatrix = transform;
	joint->GlobalMatrix = parent ? parent->GlobalMatrix * joint->LocalMatrix : joint->LocalMatrix;
	if (node.name.has_value()) {
		joint->Name = node.name->c_str();
	}
	m_loaded_nodes[nodeIdx] = joint;
	if (node.mesh.has_value()) {
		deferAddMesh(*node.mesh, node.skin, joint);
	}
	if (node.children.has_value()) {
		for (const auto &child : *node.children) {
			loadNode(child, joint);
		}
	}
}

void SelfType::MeshExtractor::loadNodes()
{
	m_loaded_nodes = std::vector<CSkinnedMesh::SJoint *>(m_gltf_model.nodes->size());

	std::vector<bool> isChild(m_gltf_model.nodes->size());
	for (const auto &node : *m_gltf_model.nodes) {
		if (!node.children.has_value())
			continue;
		for (const auto &child : *node.children) {
			isChild[child] = true;
		}
	}
	// Load all nodes that aren't children.
	// Children will be loaded by their parent nodes.
	for (std::size_t i = 0; i < m_gltf_model.nodes->size(); ++i) {
		if (!isChild[i]) {
			loadNode(i, nullptr);
		}
	}
}

void SelfType::MeshExtractor::loadSkins()
{
	if (!m_gltf_model.skins.has_value())
		return;

	for (const auto &skin : *m_gltf_model.skins) {
		if (!skin.inverseBindMatrices.has_value())
			continue;
		const auto accessor = Accessor<core::matrix4>::make(m_gltf_model, *skin.inverseBindMatrices);
		if (accessor.getCount() < skin.joints.size())
			throw std::runtime_error("accessor contains too few matrices");
		for (std::size_t i = 0; i < skin.joints.size(); ++i) {
			m_loaded_nodes.at(skin.joints[i])->GlobalInversedMatrix = convertHandedness(accessor.get(i));
		}
	}
}

void SelfType::MeshExtractor::loadAnimation(const std::size_t animIdx)
{
	const auto &anim = m_gltf_model.animations->at(animIdx);
	for (const auto &channel : anim.channels) {

		const auto &sampler = anim.samplers.at(channel.sampler);
		if (sampler.interpolation != tiniergltf::AnimationSampler::Interpolation::LINEAR)
			throw std::runtime_error("unsupported interpolation");

		const auto inputAccessor = Accessor<f32>::make(m_gltf_model, sampler.input);
		const auto n_frames = inputAccessor.getCount();

		if (!channel.target.node.has_value())
			throw std::runtime_error("no animated node");

		const auto &joint = m_loaded_nodes.at(*channel.target.node);
		switch (channel.target.path) {
		case tiniergltf::AnimationChannelTarget::Path::TRANSLATION: {
			const auto outputAccessor = Accessor<core::vector3df>::make(m_gltf_model, sampler.output);
			for (std::size_t i = 0; i < n_frames; ++i) {
				auto *key = m_irr_model->addPositionKey(joint);
				key->frame = inputAccessor.get(i);
				key->position = convertHandedness(outputAccessor.get(i));
			}
			break;
		}
		case tiniergltf::AnimationChannelTarget::Path::ROTATION: {
			const auto outputAccessor = Accessor<core::quaternion>::make(m_gltf_model, sampler.output);
			for (std::size_t i = 0; i < n_frames; ++i) {
				auto *key = m_irr_model->addRotationKey(joint);
				key->frame = inputAccessor.get(i);
				key->rotation = convertHandedness(outputAccessor.get(i));
			}
			break;
		}
		case tiniergltf::AnimationChannelTarget::Path::SCALE: {
			const auto outputAccessor = Accessor<core::vector3df>::make(m_gltf_model, sampler.output);
			for (std::size_t i = 0; i < n_frames; ++i) {
				auto *key = m_irr_model->addScaleKey(joint);
				key->frame = inputAccessor.get(i);
				key->scale = outputAccessor.get(i);
			}
			break;
		}
		case tiniergltf::AnimationChannelTarget::Path::WEIGHTS:
			throw std::runtime_error("no support for morph animations");
		}
	}
}

void SelfType::MeshExtractor::load()
{
	if (m_gltf_model.extensionsRequired)
		throw std::runtime_error("model requires extensions, but we support none");

	if (!(m_gltf_model.buffers.has_value()
			&& m_gltf_model.bufferViews.has_value()
			&& m_gltf_model.accessors.has_value()
			&& m_gltf_model.meshes.has_value()
			&& m_gltf_model.nodes.has_value())) {
		throw std::runtime_error("missing required fields");
	}

	if (m_gltf_model.images.has_value())
		warn("embedded images are not supported");

	try {
		loadNodes();
		for (const auto &load_mesh : m_mesh_loaders) {
			load_mesh();
		}
		loadSkins();
		// Load the first animation, if there is one.
		if (m_gltf_model.animations.has_value()) {
			if (m_gltf_model.animations->size() > 1)
				warn("multiple animations are not supported");

			loadAnimation(0);
			m_irr_model->setAnimationSpeed(1);
		}
	} catch (const std::out_of_range &e) {
		throw std::runtime_error(e.what());
	} catch (const std::bad_optional_access &e) {
		throw std::runtime_error(e.what());
	}

	m_irr_model->finalize();
}

/**
 * Extracts GLTF mesh indices.
 */
std::optional<std::vector<u16>> SelfType::MeshExtractor::getIndices(
		const tiniergltf::MeshPrimitive &primitive) const
{
	const auto accessorIdx = primitive.indices;
	if (!accessorIdx.has_value())
		return std::nullopt; // non-indexed geometry

	const auto accessor = ([&]() -> AccessorVariant<u8, u16, u32> {
		const auto &acc = m_gltf_model.accessors->at(*accessorIdx);
		switch (acc.componentType) {
		case tiniergltf::Accessor::ComponentType::UNSIGNED_BYTE:
			return Accessor<u8>::make(m_gltf_model, *accessorIdx);
		case tiniergltf::Accessor::ComponentType::UNSIGNED_SHORT:
			return Accessor<u16>::make(m_gltf_model, *accessorIdx);
		case tiniergltf::Accessor::ComponentType::UNSIGNED_INT:
			return Accessor<u32>::make(m_gltf_model, *accessorIdx);
		default:
			throw std::runtime_error("invalid component type");
		}
	})();
	const auto count = std::visit([](auto &&a) { return a.getCount(); }, accessor);

	std::vector<u16> indices;
	for (std::size_t i = 0; i < count; ++i) {
		// TODO (low-priority, maybe never) also reverse winding order based on determinant of global transform
		// FIXME this hack also reverses triangle draw order
		std::size_t elemIdx = count - i - 1; // reverse index order
		u16 index;
		// Note: glTF forbids the max value for each component type.
		if (std::holds_alternative<Accessor<u8>>(accessor)) {
			index = std::get<Accessor<u8>>(accessor).get(elemIdx);
			if (index == std::numeric_limits<u8>::max())
				throw std::runtime_error("invalid index");
		} else if (std::holds_alternative<Accessor<u16>>(accessor)) {
			index = std::get<Accessor<u16>>(accessor).get(elemIdx);
			if (index == std::numeric_limits<u16>::max())
				throw std::runtime_error("invalid index");
		} else if (std::holds_alternative<Accessor<u32>>(accessor)) {
			u32 indexWide = std::get<Accessor<u32>>(accessor).get(elemIdx);
			// Use >= here for consistency.
			if (indexWide >= std::numeric_limits<u16>::max())
				throw std::runtime_error("index too large (>= 65536)");
			index = static_cast<u16>(indexWide);
		}
		indices.push_back(index);
	}

	return indices;
}

/**
 * Create a vector of video::S3DVertex (model data) from a mesh & primitive index.
 */
std::optional<std::vector<video::S3DVertex>> SelfType::MeshExtractor::getVertices(
		const tiniergltf::MeshPrimitive &primitive) const
{
	const auto &attributes = primitive.attributes;
	const auto positionAccessorIdx = attributes.position;
	if (!positionAccessorIdx.has_value()) {
		// "When positions are not specified, client implementations SHOULD skip primitive's rendering"
		return std::nullopt;
	}

	std::vector<video::S3DVertex> vertices;
	const auto vertexCount = m_gltf_model.accessors->at(*positionAccessorIdx).count;
	vertices.resize(vertexCount);
	copyPositions(*positionAccessorIdx, vertices);

	const auto normalAccessorIdx = attributes.normal;
	if (normalAccessorIdx.has_value()) {
		copyNormals(normalAccessorIdx.value(), vertices);
	}
	// TODO verify that the automatic normal recalculation done in Minetest indeed works correctly

	const auto &texcoords = attributes.texcoord;
	if (texcoords.has_value()) {
		const auto tCoordAccessorIdx = texcoords->at(0);
		copyTCoords(tCoordAccessorIdx, vertices);
	}

	return vertices;
}

/**
 * Get the amount of meshes that a model contains.
*/
std::size_t SelfType::MeshExtractor::getMeshCount() const
{
	return m_gltf_model.meshes->size();
}

/**
 * Get the amount of primitives that a mesh in a model contains.
*/
std::size_t SelfType::MeshExtractor::getPrimitiveCount(
		const std::size_t meshIdx) const
{
	return m_gltf_model.meshes->at(meshIdx).primitives.size();
}

/**
 * Streams vertex positions raw data into usable buffer via reference.
 * Buffer: ref Vector<video::S3DVertex>
*/
void SelfType::MeshExtractor::copyPositions(
		const std::size_t accessorIdx,
		std::vector<video::S3DVertex>& vertices) const
{
	const auto accessor = Accessor<core::vector3df>::make(m_gltf_model, accessorIdx);
	for (std::size_t i = 0; i < accessor.getCount(); i++) {
		vertices[i].Pos = convertHandedness(accessor.get(i));
	}
}

/**
 * Streams normals raw data into usable buffer via reference.
 * Buffer: ref Vector<video::S3DVertex>
*/
void SelfType::MeshExtractor::copyNormals(
		const std::size_t accessorIdx,
		std::vector<video::S3DVertex>& vertices) const
{
	const auto accessor = Accessor<core::vector3df>::make(m_gltf_model, accessorIdx);
	for (std::size_t i = 0; i < accessor.getCount(); ++i) {
		vertices[i].Normal = convertHandedness(accessor.get(i));
	}
}

/**
 * Streams texture coordinate raw data into usable buffer via reference.
 * Buffer: ref Vector<video::S3DVertex>
*/
void SelfType::MeshExtractor::copyTCoords(
		const std::size_t accessorIdx,
		std::vector<video::S3DVertex>& vertices) const
{
	const auto componentType = m_gltf_model.accessors->at(accessorIdx).componentType;
	if (componentType == tiniergltf::Accessor::ComponentType::FLOAT) {
		// If floats are used, they need not be normalized: Wrapping may take effect.
		const auto accessor = Accessor<std::array<f32, 2>>::make(m_gltf_model, accessorIdx);
		for (std::size_t i = 0; i < accessor.getCount(); ++i) {
			vertices[i].TCoords = core::vector2d<f32>(accessor.get(i));
		}
	} else {
		const auto accessor = createNormalizedValuesAccessor<2>(m_gltf_model, accessorIdx);
		const auto count = std::visit([](auto &&a) { return a.getCount(); }, accessor);
		for (std::size_t i = 0; i < count; ++i) {
			vertices[i].TCoords = core::vector2d<f32>(getNormalizedValues(accessor, i));
		}
	}
}

/**
 * This is where the actual model's GLTF file is loaded and parsed by tiniergltf.
*/
tiniergltf::GlTF SelfType::parseGLTF(io::IReadFile* file)
{
	const bool isGlb = core::hasFileExtension(file->getFileName(), "glb");
	auto size = file->getSize();
	if (size < 0) // this can happen if `ftell` fails
		throw std::runtime_error("error reading file");
	if (size == 0)
		throw std::runtime_error("file is empty");

	std::unique_ptr<char[]> buf(new char[size + 1]);
	if (file->read(buf.get(), size) != static_cast<std::size_t>(size))
		throw std::runtime_error("file ended prematurely");
	// We probably don't need this, but add it just to be sure.
	buf[size] = '\0';
	try {
		if (isGlb)
			return tiniergltf::readGlb(buf.get(), size);
		else
			return tiniergltf::readGlTF(buf.get(), size);
	} catch (const std::out_of_range &e) {
		throw std::runtime_error(e.what());
	} catch (const std::bad_optional_access &e) {
		throw std::runtime_error(e.what());
	}
}

} // namespace scene

} // namespace irr
