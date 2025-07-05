// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "SkinnedMesh.h"
#include "IMeshLoader.h"
#include "IReadFile.h"
#include "irrTypes.h"
#include "path.h"
#include "S3DVertex.h"

#include "tiniergltf.hpp"

#include <functional>
#include <cstddef>
#include <tuple>
#include <vector>

namespace scene
{

class CGLTFMeshFileLoader : public IMeshLoader
{
public:
	CGLTFMeshFileLoader() noexcept {};

	bool isALoadableFileExtension(const io::path &filename) const override;

	IAnimatedMesh *createMesh(io::IReadFile *file) override;

private:
	template <typename T>
	static T rawget(const char *ptr);

	template <class T>
	class Accessor
	{
		struct BufferSource
		{
			const char *ptr;
			std::size_t byteStride;
		};
		using Source = std::variant<BufferSource, std::vector<T>, std::tuple<>>;

	public:
		static Accessor sparseIndices(
				const tiniergltf::GlTF &model,
				const tiniergltf::AccessorSparseIndices &indices,
				const std::size_t count);
		static Accessor sparseValues(
				const tiniergltf::GlTF &model,
				const tiniergltf::AccessorSparseValues &values,
				const std::size_t count,
				const std::size_t defaultByteStride);
		static Accessor base(
				const tiniergltf::GlTF &model,
				std::size_t accessorIdx);
		static Accessor make(const tiniergltf::GlTF &model, std::size_t accessorIdx);
		static constexpr tiniergltf::Accessor::Type getType();
		static constexpr tiniergltf::Accessor::ComponentType getComponentType();
		std::size_t getCount() const { return count; }
		T get(std::size_t i) const;

	private:
		Accessor(const char *ptr, std::size_t byteStride, std::size_t count) :
				source(BufferSource{ptr, byteStride}), count(count) {}
		Accessor(std::vector<T> vec, std::size_t count) :
				source(vec), count(count) {}
		Accessor(std::size_t count) :
				source(std::make_tuple()), count(count) {}
		// Directly from buffer, sparse, or default-initialized
		const Source source;
		const std::size_t count;
	};

	template <typename... Ts>
	using AccessorVariant = std::variant<Accessor<Ts>...>;

	template <std::size_t N, typename... Ts>
	using ArrayAccessorVariant = std::variant<Accessor<std::array<Ts, N>>...>;

	template <std::size_t N>
	using NormalizedValuesAccessor = ArrayAccessorVariant<N, u8, u16, f32>;

	template <std::size_t N>
	static NormalizedValuesAccessor<N> createNormalizedValuesAccessor(
			const tiniergltf::GlTF &model,
			const std::size_t accessorIdx);

	template <std::size_t N, bool validate = true>
	static std::array<f32, N> getNormalizedValues(
			const NormalizedValuesAccessor<N> &accessor,
			const std::size_t i);

	class MeshExtractor
	{
	public:
		MeshExtractor(tiniergltf::GlTF &&model,
				SkinnedMeshBuilder *mesh) noexcept
			: m_gltf_model(std::move(model)), m_irr_model(mesh) {};

		/* Gets indices for the given mesh/primitive.
		 *
		 * Values are return in Irrlicht winding order.
		 */
		std::optional<std::vector<u16>> getIndices(
				const tiniergltf::MeshPrimitive &primitive) const;

		std::optional<std::vector<video::S3DVertex>> getVertices(
				const tiniergltf::MeshPrimitive &primitive) const;

		std::size_t getMeshCount() const;

		std::size_t getPrimitiveCount(const std::size_t meshIdx) const;

		void load();
		const std::unordered_set<std::string> &getWarnings() {
			return warnings;
		}

	private:
		const tiniergltf::GlTF m_gltf_model;
		SkinnedMeshBuilder *m_irr_model;

		std::vector<std::function<void()>> m_mesh_loaders;
		std::vector<SkinnedMesh::SJoint *> m_loaded_nodes;

		std::unordered_set<std::string> warnings;
		void warn(const std::string &warning) {
			warnings.insert(warning);
		}

		void copyPositions(const std::size_t accessorIdx,
				std::vector<video::S3DVertex>& vertices) const;

		void copyNormals(const std::size_t accessorIdx,
				std::vector<video::S3DVertex>& vertices) const;

		void copyTCoords(const std::size_t accessorIdx,
				std::vector<video::S3DVertex>& vertices) const;

		void addPrimitive(const tiniergltf::MeshPrimitive &primitive,
				const std::optional<std::size_t> skinIdx,
				SkinnedMesh::SJoint *parent);

		void deferAddMesh(const std::size_t meshIdx,
				const std::optional<std::size_t> skinIdx,
				SkinnedMesh::SJoint *parentJoint);

		void loadNode(const std::size_t nodeIdx, SkinnedMesh::SJoint *parentJoint);

		void loadNodes();

		void loadSkins();

		void loadAnimation(const std::size_t animIdx);
	};

	tiniergltf::GlTF parseGLTF(io::IReadFile *file);
};

} // namespace scene

