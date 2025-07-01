#pragma once

#include <json/json.h>
#include "util/base64.h"

#include <cstdint>
#include <functional>
#include <stack>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <array>
#include <optional>
#include <limits>
#include <memory> // unique_ptr
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#if JSONCPP_VERSION_HEXA < 0x01090000 /* 1.9.0 */
namespace Json {
	using String = JSONCPP_STRING; // Polyfill
}
#endif

namespace tiniergltf {

static inline void check(bool cond) {
	if (!cond)
		throw std::runtime_error("invalid glTF");
}

template <typename T>
static inline void checkIndex(const std::optional<std::vector<T>> &vec,
		const std::optional<std::size_t> &i) {
	if (!i.has_value()) return;
	check(vec.has_value());
	check(i < vec->size());
}

template <typename T>
static inline void checkIndex(const std::vector<T> &vec,
		const std::optional<std::size_t> &i) {
	if (!i.has_value()) return;
	check(i < vec.size());
}

template <typename T, typename F>
static inline void checkForall(const std::optional<std::vector<T>> &vec, const F &cond) {
	if (!vec.has_value())
		return;
	for (const T &v : vec.value())
		cond(v);
}

template <typename T>
static inline void checkDuplicateFree(const std::vector<T> &vec) {
	check(std::unordered_set<T>(vec.begin(), vec.end()).size() == vec.size());
}

template <typename T>
static inline T as(const Json::Value &o);

template<>
bool as(const Json::Value &o) {
	check(o.isBool());
	return o.asBool();
}

template<>
double as (const Json::Value &o) {
	check(o.isDouble());
	return o.asDouble();
}

template<>
std::size_t as(const Json::Value &o) {
	check(o.isUInt64());
	auto u = o.asUInt64();
	check(u <= std::numeric_limits<std::size_t>::max());
	return u;
}

template<>
std::string as(const Json::Value &o) {
	check(o.isString());
	return o.asString();
}

template<typename U>
std::vector<U> asVec(const Json::Value &o) {
	check(o.isArray());
	std::vector<U> res;
	res.reserve(o.size());
	for (Json::ArrayIndex i = 0; i < o.size(); ++i) {
		res.push_back(as<U>(o[i]));
	}
	return res;
}

template<typename U, std::size_t n>
std::array<U, n> asArr(const Json::Value &o) {
	check(o.isArray());
	check(o.size() == n);
	std::array<U, n> res;
	for (Json::ArrayIndex i = 0; i < n; ++i) {
		res[i] = as<U>(o[i]);
	}
	return res;
}

struct AccessorSparseIndices {
	std::size_t bufferView;
	std::size_t byteOffset;
	// as defined in the glTF specification
	enum class ComponentType {
		UNSIGNED_BYTE,
		UNSIGNED_SHORT,
		UNSIGNED_INT,
	};
	ComponentType componentType;
	std::size_t componentSize() const {
		switch (componentType) {
		case ComponentType::UNSIGNED_BYTE:
			return 1;
		case ComponentType::UNSIGNED_SHORT:
			return 2;
		case ComponentType::UNSIGNED_INT:
			return 4;
		}
		throw std::logic_error("invalid component type");
	}
	std::size_t elementSize() const {
		return componentSize();
	}
	AccessorSparseIndices(const Json::Value &o)
		: bufferView(as<std::size_t>(o["bufferView"]))
		, byteOffset(0)
	{
		check(o.isObject());
		if (o.isMember("byteOffset")) {
			byteOffset = as<std::size_t>(o["byteOffset"]);
			check(byteOffset >= 0);
		}
		{
			static std::unordered_map<Json::UInt64, ComponentType> map = {
				{5121, ComponentType::UNSIGNED_BYTE},
				{5123, ComponentType::UNSIGNED_SHORT},
				{5125, ComponentType::UNSIGNED_INT},
			};
			const auto &v = o["componentType"]; check(v.isUInt64());
			componentType = map.at(v.asUInt64());
		}
	}
};
template<> AccessorSparseIndices as(const Json::Value &o) { return o; }

struct AccessorSparseValues {
	std::size_t bufferView;
	std::size_t byteOffset;
	AccessorSparseValues(const Json::Value &o)
		: bufferView(as<std::size_t>(o["bufferView"]))
		, byteOffset(0)
	{
		check(o.isObject());
		if (o.isMember("byteOffset")) {
			byteOffset = as<std::size_t>(o["byteOffset"]);
			check(byteOffset >= 0);
		}
	}
};
template<> AccessorSparseValues as(const Json::Value &o) { return o; }

struct AccessorSparse {
	std::size_t count;
	AccessorSparseIndices indices;
	AccessorSparseValues values;
	AccessorSparse(const Json::Value &o)
		: count(as<std::size_t>(o["count"]))
		, indices(as<AccessorSparseIndices>(o["indices"]))
		, values(as<AccessorSparseValues>(o["values"]))
	{
		check(o.isObject());
		check(count >= 1);
	}
};
template<> AccessorSparse as(const Json::Value &o) { return o; }

struct Accessor {
	std::optional<std::size_t> bufferView;
	std::size_t byteOffset;
	// as defined in the glTF specification
	enum class ComponentType {
		BYTE,
		UNSIGNED_BYTE,
		SHORT,
		UNSIGNED_SHORT,
		UNSIGNED_INT,
		FLOAT,
	};
	ComponentType componentType;
	std::size_t componentSize() const {
		switch (componentType) {
		case ComponentType::BYTE:
		case ComponentType::UNSIGNED_BYTE:
			return 1;
		case ComponentType::SHORT:
		case ComponentType::UNSIGNED_SHORT:
			return 2;
		case ComponentType::UNSIGNED_INT:
		case ComponentType::FLOAT:
			return 4;
		}
		throw std::logic_error("invalid component type");
	}
	std::size_t count;
	std::optional<std::vector<double>> max;
	std::optional<std::vector<double>> min;
	std::optional<std::string> name;
	bool normalized;
	std::optional<AccessorSparse> sparse;
	enum class Type {
		MAT2,
		MAT3,
		MAT4,
		SCALAR,
		VEC2,
		VEC3,
		VEC4,
	};
	std::size_t typeCount() const {
		switch (type) {
		case Type::SCALAR:
			return 1;
		case Type::VEC2:
			return 2;
		case Type::VEC3:
			return 3;
		case Type::MAT2:
		case Type::VEC4:
			return 4;
		case Type::MAT3:
			return 9;
		case Type::MAT4:
			return 16;
		}
		throw std::logic_error("invalid type");
	}
	Type type;
	std::size_t elementSize() const {
		return componentSize() * typeCount();
	}
	Accessor(const Json::Value &o)
		: byteOffset(0)
		, count(as<std::size_t>(o["count"]))
		, normalized(false)
	{
		check(o.isObject());
		if (o.isMember("bufferView")) {
			bufferView = as<std::size_t>(o["bufferView"]);
		}
		{
			static std::unordered_map<Json::UInt64, ComponentType> map = {
				{5120, ComponentType::BYTE},
				{5121, ComponentType::UNSIGNED_BYTE},
				{5122, ComponentType::SHORT},
				{5123, ComponentType::UNSIGNED_SHORT},
				{5125, ComponentType::UNSIGNED_INT},
				{5126, ComponentType::FLOAT},
			};
			const auto &v = o["componentType"]; check(v.isUInt64());
			componentType = map.at(v.asUInt64());
		}
		if (o.isMember("byteOffset")) {
			byteOffset = as<std::size_t>(o["byteOffset"]);
			check(byteOffset >= 0);
			check(byteOffset % componentSize() == 0);
		}
		check(count >= 1);
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (o.isMember("normalized")) {
			normalized = as<bool>(o["normalized"]);
		}
		if (o.isMember("sparse")) {
			sparse = as<AccessorSparse>(o["sparse"]);
		}
		{
			static std::unordered_map<Json::String, Type> map = {
				{"MAT2", Type::MAT2},
				{"MAT3", Type::MAT3},
				{"MAT4", Type::MAT4},
				{"SCALAR", Type::SCALAR},
				{"VEC2", Type::VEC2},
				{"VEC3", Type::VEC3},
				{"VEC4", Type::VEC4},
			};
			const auto &v = o["type"]; check(v.isString());
			type = map.at(v.asString());
		}
		if (o.isMember("max")) {
			max = asVec<double>(o["max"]);
			check(max->size() == typeCount());
		}
		if (o.isMember("min")) {
			min = asVec<double>(o["min"]);
			check(min->size() == typeCount());
		}
	}
};
template<> Accessor as(const Json::Value &o) { return o; }

struct AnimationChannelTarget {
	std::optional<std::size_t> node;
	enum class Path {
		ROTATION,
		SCALE,
		TRANSLATION,
		WEIGHTS,
	};
	Path path;
	AnimationChannelTarget(const Json::Value &o)
	{
		check(o.isObject());
		if (o.isMember("node")) {
			node = as<std::size_t>(o["node"]);
		}
		{
			static std::unordered_map<Json::String, Path> map = {
				{"rotation", Path::ROTATION},
				{"scale", Path::SCALE},
				{"translation", Path::TRANSLATION},
				{"weights", Path::WEIGHTS},
			};
			const auto &v = o["path"]; check(v.isString());
			path = map.at(v.asString());
		}
	}
};
template<> AnimationChannelTarget as(const Json::Value &o) { return o; }

struct AnimationChannel {
	std::size_t sampler;
	AnimationChannelTarget target;
	AnimationChannel(const Json::Value &o)
		: sampler(as<std::size_t>(o["sampler"]))
		, target(as<AnimationChannelTarget>(o["target"]))
	{
		check(o.isObject());
	}
};
template<> AnimationChannel as(const Json::Value &o) { return o; }

struct AnimationSampler {
	std::size_t input;
	enum class Interpolation {
		CUBICSPLINE,
		LINEAR,
		STEP,
	};
	Interpolation interpolation;
	std::size_t output;
	AnimationSampler(const Json::Value &o)
		: input(as<std::size_t>(o["input"]))
		, interpolation(Interpolation::LINEAR)
		, output(as<std::size_t>(o["output"]))
	{
		check(o.isObject());
		if (o.isMember("interpolation")) {
			static std::unordered_map<Json::String, Interpolation> map = {
				{"CUBICSPLINE", Interpolation::CUBICSPLINE},
				{"LINEAR", Interpolation::LINEAR},
				{"STEP", Interpolation::STEP},
			};
			const auto &v = o["interpolation"]; check(v.isString());
			interpolation = map.at(v.asString());
		}
	}
};
template<> AnimationSampler as(const Json::Value &o) { return o; }

struct Animation {
	std::vector<AnimationChannel> channels;
	std::optional<std::string> name;
	std::vector<AnimationSampler> samplers;
	Animation(const Json::Value &o)
		: channels(asVec<AnimationChannel>(o["channels"]))
		, samplers(asVec<AnimationSampler>(o["samplers"]))
	{
		check(o.isObject());
		check(channels.size() >= 1);
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		check(samplers.size() >= 1);
	}
};
template<> Animation as(const Json::Value &o) { return o; }

struct Asset {
	std::optional<std::string> copyright;
	std::optional<std::string> generator;
	std::optional<std::string> minVersion;
	std::string version;
	Asset(const Json::Value &o)
		: version(as<std::string>(o["version"]))
	{
		check(o.isObject());
		if (o.isMember("copyright")) {
			copyright = as<std::string>(o["copyright"]);
		}
		if (o.isMember("generator")) {
			generator = as<std::string>(o["generator"]);
		}
		if (o.isMember("minVersion")) {
			minVersion = as<std::string>(o["minVersion"]);
		}
	}
};
template<> Asset as(const Json::Value &o) { return o; }

struct BufferView {
	std::size_t buffer;
	std::size_t byteLength;
	std::size_t byteOffset;
	std::optional<std::size_t> byteStride;
	std::optional<std::string> name;
	enum class Target {
		ARRAY_BUFFER,
		ELEMENT_ARRAY_BUFFER,
	};
	std::optional<Target> target;
	BufferView(const Json::Value &o)
		: buffer(as<std::size_t>(o["buffer"]))
		, byteLength(as<std::size_t>(o["byteLength"]))
		, byteOffset(0)
	{
		check(o.isObject());
		check(byteLength >= 1);
		if (o.isMember("byteOffset")) {
			byteOffset = as<std::size_t>(o["byteOffset"]);
			check(byteOffset >= 0);
		}
		if (o.isMember("byteStride")) {
			byteStride = as<std::size_t>(o["byteStride"]);
			check(byteStride.value() >= 4);
			check(byteStride.value() <= 252);
			check(byteStride.value() % 4 == 0);
		}
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (o.isMember("target")) {
			static std::unordered_map<Json::UInt64, Target> map = {
				{34962, Target::ARRAY_BUFFER},
				{34963, Target::ELEMENT_ARRAY_BUFFER},
			};
			const auto &v = o["target"]; check(v.isUInt64());
			target = map.at(v.asUInt64());
		}
	}
};
template<> BufferView as(const Json::Value &o) { return o; }

struct Buffer {
	std::size_t byteLength;
	std::optional<std::string> name;
	std::string data;
	Buffer(const Json::Value &o,
			const std::function<std::string(const std::string &uri)> &resolveURI,
			std::optional<std::string> &&glbData = std::nullopt)
		: byteLength(as<std::size_t>(o["byteLength"]))
	{
		check(o.isObject());
		check(byteLength >= 1);
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (glbData.has_value()) {
			check(!o.isMember("uri"));
			data = *std::move(glbData);
			// GLB allows padding, which need not be reflected in the JSON
			check(byteLength + 3 >= data.size());
			check(data.size() >= byteLength);
		} else {
			check(o.isMember("uri"));
			bool dataURI = false;
			const std::string uri = as<std::string>(o["uri"]);
			for (auto &prefix : std::array<std::string, 2> {
				"data:application/octet-stream;base64,",
				"data:application/gltf-buffer;base64,"
			}) {
				if (std::string_view(uri).substr(0, prefix.length()) == prefix) {
					auto view = std::string_view(uri).substr(prefix.length());
					check(base64_is_valid(view));
					data = base64_decode(view);
					dataURI = true;
					break;
				}
			}
			if (!dataURI)
				data = resolveURI(uri);
			check(data.size() >= byteLength);
		}
		data.resize(byteLength);
	}
};

struct CameraOrthographic {
	double xmag;
	double ymag;
	double zfar;
	double znear;
	CameraOrthographic(const Json::Value &o)
		: xmag(as<double>(o["xmag"]))
		, ymag(as<double>(o["ymag"]))
		, zfar(as<double>(o["zfar"]))
		, znear(as<double>(o["znear"]))
	{
		check(o.isObject());
		check(zfar > 0);
		check(znear >= 0);
	}
};
template<> CameraOrthographic as(const Json::Value &o) { return o; }

struct CameraPerspective {
	std::optional<double> aspectRatio;
	double yfov;
	std::optional<double> zfar;
	double znear;
	CameraPerspective(const Json::Value &o)
		: yfov(as<double>(o["yfov"]))
		, znear(as<double>(o["znear"]))
	{
		check(o.isObject());
		if (o.isMember("aspectRatio")) {
			aspectRatio = as<double>(o["aspectRatio"]);
			check(aspectRatio.value() > 0);
		}
		check(yfov > 0);
		if (o.isMember("zfar")) {
			zfar = as<double>(o["zfar"]);
			check(zfar.value() > 0);
		}
		check(znear > 0);
	}
};
template<> CameraPerspective as(const Json::Value &o) { return o; }

struct Camera {
	std::optional<std::string> name;
	std::optional<CameraOrthographic> orthographic;
	std::optional<CameraPerspective> perspective;
	enum class Type {
		ORTHOGRAPHIC,
		PERSPECTIVE,
	};
	Type type;
	Camera(const Json::Value &o)
	{
		check(o.isObject());
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (o.isMember("orthographic")) {
			orthographic = as<CameraOrthographic>(o["orthographic"]);
		}
		if (o.isMember("perspective")) {
			perspective = as<CameraPerspective>(o["perspective"]);
		}
		{
			static std::unordered_map<Json::String, Type> map = {
				{"orthographic", Type::ORTHOGRAPHIC},
				{"perspective", Type::PERSPECTIVE},
			};
			const auto &v = o["type"]; check(v.isString());
			type = map.at(v.asString());
		}
	}
};
template<> Camera as(const Json::Value &o) { return o; }

struct Image {
	std::optional<std::size_t> bufferView;
	enum class MimeType {
		IMAGE_JPEG,
		IMAGE_PNG,
	};
	std::optional<MimeType> mimeType;
	std::optional<std::string> name;
	std::optional<std::string> uri;
	Image(const Json::Value &o)
	{
		check(o.isObject());
		if (o.isMember("bufferView")) {
			bufferView = as<std::size_t>(o["bufferView"]);
		}
		if (o.isMember("mimeType")) {
			static std::unordered_map<Json::String, MimeType> map = {
				{"image/jpeg", MimeType::IMAGE_JPEG},
				{"image/png", MimeType::IMAGE_PNG},
			};
			const auto &v = o["mimeType"]; check(v.isString());
			mimeType = map.at(v.asString());
		}
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (o.isMember("uri")) {
			uri = as<std::string>(o["uri"]);
		}
	}
};
template<> Image as(const Json::Value &o) { return o; }

struct TextureInfo {
	std::size_t index;
	std::size_t texCoord;
	TextureInfo(const Json::Value &o)
		: index(as<std::size_t>(o["index"]))
		, texCoord(0)
	{
		check(o.isObject());
		if (o.isMember("texCoord")) {
			texCoord = as<std::size_t>(o["texCoord"]);
			check(texCoord >= 0);
		}
	}
};
template<> TextureInfo as(const Json::Value &o) { return o; }

struct MaterialNormalTextureInfo {
	std::size_t index;
	double scale;
	std::size_t texCoord;
	MaterialNormalTextureInfo(const Json::Value &o)
		: index(as<std::size_t>(o["index"]))
		, scale(1)
		, texCoord(0)
	{
		check(o.isObject());
		if (o.isMember("scale")) {
			scale = as<double>(o["scale"]);
		}
		if (o.isMember("texCoord")) {
			texCoord = as<std::size_t>(o["texCoord"]);
		}
	}
};
template<> MaterialNormalTextureInfo as(const Json::Value &o) { return o; }

struct MaterialOcclusionTextureInfo {
	std::size_t index;
	double strength;
	std::size_t texCoord;
	MaterialOcclusionTextureInfo(const Json::Value &o)
		: index(as<std::size_t>(o["index"]))
		, strength(1)
		, texCoord(0)
	{
		check(o.isObject());
		if (o.isMember("strength")) {
			strength = as<double>(o["strength"]);
			check(strength >= 0);
			check(strength <= 1);
		}
		if (o.isMember("texCoord")) {
			texCoord = as<std::size_t>(o["texCoord"]);
		}
	}
};
template<> MaterialOcclusionTextureInfo as(const Json::Value &o) { return o; }

struct MaterialPbrMetallicRoughness {
	std::array<double, 4> baseColorFactor;
	std::optional<TextureInfo> baseColorTexture;
	double metallicFactor;
	std::optional<TextureInfo> metallicRoughnessTexture;
	double roughnessFactor;
	MaterialPbrMetallicRoughness(const Json::Value &o)
		: baseColorFactor{1, 1, 1, 1}
		, metallicFactor(1)
		, roughnessFactor(1)
	{
		check(o.isObject());
		if (o.isMember("baseColorFactor")) {
			baseColorFactor = asArr<double, 4>(o["baseColorFactor"]);
			for (auto v: baseColorFactor) {
				check(v >= 0);
				check(v <= 1);
			}
		}
		if (o.isMember("baseColorTexture")) {
			baseColorTexture = as<TextureInfo>(o["baseColorTexture"]);
		}
		if (o.isMember("metallicFactor")) {
			metallicFactor = as<double>(o["metallicFactor"]);
			check(metallicFactor >= 0);
			check(metallicFactor <= 1);
		}
		if (o.isMember("metallicRoughnessTexture")) {
			metallicRoughnessTexture = as<TextureInfo>(o["metallicRoughnessTexture"]);
		}
		if (o.isMember("roughnessFactor")) {
			roughnessFactor = as<double>(o["roughnessFactor"]);
			check(roughnessFactor >= 0);
			check(roughnessFactor <= 1);
		}
	}
};
template<> MaterialPbrMetallicRoughness as(const Json::Value &o) { return o; }

struct Material {
	double alphaCutoff;
	enum class AlphaMode {
		BLEND,
		MASK,
		OPAQUE,
	};
	AlphaMode alphaMode;
	bool doubleSided;
	std::array<double, 3> emissiveFactor;
	std::optional<TextureInfo> emissiveTexture;
	std::optional<std::string> name;
	std::optional<MaterialNormalTextureInfo> normalTexture;
	std::optional<MaterialOcclusionTextureInfo> occlusionTexture;
	std::optional<MaterialPbrMetallicRoughness> pbrMetallicRoughness;
	Material(const Json::Value &o)
		: alphaCutoff(0.5)
		, alphaMode(AlphaMode::OPAQUE)
		, doubleSided(false)
		, emissiveFactor{0, 0, 0}
	{
		check(o.isObject());
		if (o.isMember("alphaCutoff")) {
			alphaCutoff = as<double>(o["alphaCutoff"]);
			check(alphaCutoff >= 0);
		}
		if (o.isMember("alphaMode")){
			static std::unordered_map<Json::String, AlphaMode> map = {
				{"BLEND", AlphaMode::BLEND},
				{"MASK", AlphaMode::MASK},
				{"OPAQUE", AlphaMode::OPAQUE},
			};
			const auto &v = o["alphaMode"]; check(v.isString());
			alphaMode = map.at(v.asString());
		}
		if (o.isMember("doubleSided")) {
			doubleSided = as<bool>(o["doubleSided"]);
		}
		if (o.isMember("emissiveFactor")) {
			emissiveFactor = asArr<double, 3>(o["emissiveFactor"]);
			for (const auto &v: emissiveFactor) {
				check(v >= 0);
				check(v <= 1);
			}
		}
		if (o.isMember("emissiveTexture")) {
			emissiveTexture = as<TextureInfo>(o["emissiveTexture"]);
		}
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (o.isMember("normalTexture")) {
			normalTexture = as<MaterialNormalTextureInfo>(o["normalTexture"]);
		}
		if (o.isMember("occlusionTexture")) {
			occlusionTexture = as<MaterialOcclusionTextureInfo>(o["occlusionTexture"]);
		}
		if (o.isMember("pbrMetallicRoughness")) {
			pbrMetallicRoughness = as<MaterialPbrMetallicRoughness>(o["pbrMetallicRoughness"]);
		}
	}
};
template<> Material as(const Json::Value &o) { return o; }

struct MeshPrimitive {
	static void enumeratedProps(const Json::Value &o, const std::string &name, std::optional<std::vector<std::size_t>> &attr) {
		for (std::size_t i = 0;; ++i) {
			const std::string s = name + "_" + std::to_string(i);
			if (!o.isMember(s)) break;
			if (i == 0) {
				attr = std::vector<std::size_t>();
			}
			attr->push_back(as<std::size_t>(o[s]));
		}
	}
	struct Attributes {
		std::optional<std::size_t> position, normal, tangent;
		std::optional<std::vector<std::size_t>> texcoord, color, joints, weights;
		Attributes(const Json::Value &o) {
			if (o.isMember("POSITION"))
				position = as<std::size_t>(o["POSITION"]);
			if (o.isMember("NORMAL"))
				normal = as<std::size_t>(o["NORMAL"]);
			if (o.isMember("TANGENT"))
				tangent = as<std::size_t>(o["TANGENT"]);
			enumeratedProps(o, "TEXCOORD", texcoord);
			enumeratedProps(o, "COLOR", color);
			enumeratedProps(o, "JOINTS", joints);
			enumeratedProps(o, "WEIGHTS", weights);
			check(joints.has_value() == weights.has_value());
			if (joints.has_value()) {
				check(joints->size() == weights->size());
			}
			check(position.has_value()
					|| normal.has_value()
					|| tangent.has_value()
					|| texcoord.has_value()
					|| color.has_value()
					|| joints.has_value()
					|| weights.has_value());
		}
	};
	Attributes attributes;
	std::optional<std::size_t> indices;
	std::optional<std::size_t> material;
	enum class Mode {
		POINTS,
		LINES,
		LINE_LOOP,
		LINE_STRIP,
		TRIANGLES,
		TRIANGLE_STRIP,
		TRIANGLE_FAN,
	};
	Mode mode;
	struct MorphTargets {
		std::optional<std::size_t> position, normal, tangent;
		std::optional<std::vector<std::size_t>> texcoord, color;
		MorphTargets(const Json::Value &o) {
			if (o.isMember("POSITION"))
				position = as<std::size_t>(o["POSITION"]);
			if (o.isMember("NORMAL"))
				normal = as<std::size_t>(o["NORMAL"]);
			if (o.isMember("TANGENT"))
				tangent = as<std::size_t>(o["TANGENT"]);
			enumeratedProps(o, "TEXCOORD", texcoord);
			enumeratedProps(o, "COLOR", color);
			check(position.has_value()
					|| normal.has_value()
					|| tangent.has_value()
					|| texcoord.has_value()
					|| color.has_value());
		}
	};
	std::optional<std::vector<MorphTargets>> targets;
	MeshPrimitive(const Json::Value &o)
		: attributes(Attributes(o["attributes"]))
		, mode(Mode::TRIANGLES)
	{
		check(o.isObject());
		if (o.isMember("indices")) {
			indices = as<std::size_t>(o["indices"]);
		}
		if (o.isMember("material")) {
			material = as<std::size_t>(o["material"]);
		}
		if (o.isMember("mode")) {
			static std::unordered_map<Json::UInt64, Mode> map = {
				{0, Mode::POINTS},
				{1, Mode::LINES},
				{2, Mode::LINE_LOOP},
				{3, Mode::LINE_STRIP},
				{4, Mode::TRIANGLES},
				{5, Mode::TRIANGLE_STRIP},
				{6, Mode::TRIANGLE_FAN},
			};
			const auto &v = o["mode"]; check(v.isUInt64());
			mode = map.at(v.asUInt64());
		}
		if (o.isMember("targets")) {
			targets = asVec<MorphTargets>(o["targets"]);
			check(targets->size() >= 1);
		}
	}
};
template<> MeshPrimitive::MorphTargets as(const Json::Value &o) { return o; }
template<> MeshPrimitive as(const Json::Value &o) { return o; }

struct Mesh {
	std::optional<std::string> name;
	std::vector<MeshPrimitive> primitives;
	std::optional<std::vector<double>> weights;
	Mesh(const Json::Value &o)
		: primitives(asVec<MeshPrimitive>(o["primitives"]))
	{
		check(o.isObject());
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		check(primitives.size() >= 1);
		if (o.isMember("weights")) {
			weights = asVec<double>(o["weights"]);
			check(weights->size() >= 1);
		}
	}
};
template<> Mesh as(const Json::Value &o) { return o; }

struct Node {
	std::optional<std::size_t> camera;
	std::optional<std::vector<std::size_t>> children;
	typedef std::array<double, 16> Matrix;
	struct TRS {
		std::array<double, 3> translation = {0, 0, 0};
		std::array<double, 4> rotation = {0, 0, 0, 1};
		std::array<double, 3> scale = {1, 1, 1};
	};
	std::variant<Matrix, TRS> transform;
	std::optional<std::size_t> mesh;
	std::optional<std::string> name;
	std::optional<std::size_t> skin;
	std::optional<std::vector<double>> weights;
	Node(const Json::Value &o)
		: transform(TRS{})
	{
		check(o.isObject());
		if (o.isMember("camera")) {
			camera = as<std::size_t>(o["camera"]);
		}
		if (o.isMember("children")) {
			children = asVec<std::size_t>(o["children"]);
			check(children->size() >= 1);
			checkDuplicateFree(*children);
		}
		bool hasTRS = o.isMember("translation") || o.isMember("rotation") || o.isMember("scale");
		if (o.isMember("matrix")) {
			check(!hasTRS);
			transform = asArr<double, 16>(o["matrix"]);
		} else if (hasTRS) {
			TRS trs;
			if (o.isMember("translation")) {
				trs.translation = asArr<double, 3>(o["translation"]);
			}
			if (o.isMember("rotation")) {
				trs.rotation = asArr<double, 4>(o["rotation"]);
				for (auto v: trs.rotation) {
					check(v >= -1);
					check(v <= 1);
				}
			}
			if (o.isMember("scale")) {
				trs.scale = asArr<double, 3>(o["scale"]);
			}
			transform = trs;
		}
		if (o.isMember("mesh")) {
			mesh = as<std::size_t>(o["mesh"]);
		}
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (o.isMember("skin")) {
			check(mesh.has_value());
			skin = as<std::size_t>(o["skin"]);
		}
		if (o.isMember("weights")) {
			weights = asVec<double>(o["weights"]);
			check(weights->size() >= 1);
		}
	}
};
template<> Node as(const Json::Value &o) { return o; }

struct Sampler {
	enum class MagFilter {
		NEAREST,
		LINEAR,
	};
	std::optional<MagFilter> magFilter;
	enum class MinFilter {
		NEAREST,
		LINEAR,
		NEAREST_MIPMAP_NEAREST,
		LINEAR_MIPMAP_NEAREST,
		NEAREST_MIPMAP_LINEAR,
		LINEAR_MIPMAP_LINEAR,
	};
	std::optional<MinFilter> minFilter;
	std::optional<std::string> name;
	enum class Wrap {
		REPEAT,
		CLAMP_TO_EDGE,
		MIRRORED_REPEAT,
	};
	Wrap wrapS;
	Wrap wrapT;
	Sampler(const Json::Value &o)
		: wrapS(Wrap::REPEAT)
		, wrapT(Wrap::REPEAT)
	{
		check(o.isObject());
		if (o.isMember("magFilter")) {
			static std::unordered_map<Json::UInt64, MagFilter> map = {
				{9728, MagFilter::NEAREST},
				{9729, MagFilter::LINEAR},
			};
			const auto &v = o["magFilter"]; check(v.isUInt64());
			magFilter = map.at(v.asUInt64());
		}
		if (o.isMember("minFilter")) {
			static std::unordered_map<Json::UInt64, MinFilter> map = {
				{9728, MinFilter::NEAREST},
				{9729, MinFilter::LINEAR},
				{9984, MinFilter::NEAREST_MIPMAP_NEAREST},
				{9985, MinFilter::LINEAR_MIPMAP_NEAREST},
				{9986, MinFilter::NEAREST_MIPMAP_LINEAR},
				{9987, MinFilter::LINEAR_MIPMAP_LINEAR},
			};
			const auto &v = o["minFilter"]; check(v.isUInt64());
			minFilter = map.at(v.asUInt64());
		}
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		static std::unordered_map<Json::UInt64, Wrap> map = {
			{10497, Wrap::REPEAT},
			{33071, Wrap::CLAMP_TO_EDGE},
			{33648, Wrap::MIRRORED_REPEAT},
		};
		if (o.isMember("wrapS")) {
			const auto &v = o["wrapS"]; check(v.isUInt64());
			wrapS = map.at(v.asUInt64());
		}
		if (o.isMember("wrapT")) {
			const auto &v = o["wrapT"]; check(v.isUInt64());
			wrapT = map.at(v.asUInt64());
		}
	}
};
template<> Sampler as(const Json::Value &o) { return o; }

struct Scene {
	std::optional<std::string> name;
	std::optional<std::vector<std::size_t>> nodes;
	Scene(const Json::Value &o)
	{
		check(o.isObject());
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (o.isMember("nodes")) {
			nodes = asVec<std::size_t>(o["nodes"]);
			check(nodes->size() >= 1);
			checkDuplicateFree(*nodes);
		}
	}
};
template<> Scene as(const Json::Value &o) { return o; }

struct Skin {
	std::optional<std::size_t> inverseBindMatrices;
	std::vector<std::size_t> joints;
	std::optional<std::string> name;
	std::optional<std::size_t> skeleton;
	Skin(const Json::Value &o)
		: joints(asVec<std::size_t>(o["joints"]))
	{
		check(o.isObject());
		if (o.isMember("inverseBindMatrices")) {
			inverseBindMatrices = as<std::size_t>(o["inverseBindMatrices"]);
		}
		check(joints.size() >= 1);
		checkDuplicateFree(joints);
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (o.isMember("skeleton")) {
			skeleton = as<std::size_t>(o["skeleton"]);
		}
	}
};
template<> Skin as(const Json::Value &o) { return o; }

struct Texture {
	std::optional<std::string> name;
	std::optional<std::size_t> sampler;
	std::optional<std::size_t> source;
	Texture(const Json::Value &o)
	{
		check(o.isObject());
		if (o.isMember("name")) {
			name = as<std::string>(o["name"]);
		}
		if (o.isMember("sampler")) {
			sampler = as<std::size_t>(o["sampler"]);
		}
		if (o.isMember("source")) {
			source = as<std::size_t>(o["source"]);
		}
	}
};
template<> Texture as(const Json::Value &o) { return o; }

using UriResolver = std::function<std::string(const std::string &uri)>;
static inline std::string uriError(const std::string &uri) {
	// only base64 data URI support by default
	throw std::runtime_error("unsupported URI: " + uri);
}

struct GlTF {
	std::optional<std::vector<Accessor>> accessors;
	std::optional<std::vector<Animation>> animations;
	Asset asset;
	std::optional<std::vector<BufferView>> bufferViews;
	std::optional<std::vector<Buffer>> buffers;
	std::optional<std::vector<Camera>> cameras;
	std::optional<std::vector<std::string>> extensionsRequired;
	std::optional<std::vector<std::string>> extensionsUsed;
	std::optional<std::vector<Image>> images;
	std::optional<std::vector<Material>> materials;
	std::optional<std::vector<Mesh>> meshes;
	std::optional<std::vector<Node>> nodes;
	std::optional<std::vector<Sampler>> samplers;
	std::optional<std::size_t> scene;
	std::optional<std::vector<Scene>> scenes;
	std::optional<std::vector<Skin>> skins;
	std::optional<std::vector<Texture>> textures;

	GlTF(const Json::Value &o,
			const UriResolver &resolveUri = uriError,
			std::optional<std::string> &&glbData = std::nullopt)
		: asset(as<Asset>(o["asset"]))
	{
		check(o.isObject());
		if (o.isMember("accessors")) {
			accessors = asVec<Accessor>(o["accessors"]);
			check(accessors->size() >= 1);
		}
		if (o.isMember("animations")) {
			animations = asVec<Animation>(o["animations"]);
			check(animations->size() >= 1);
		}
		if (o.isMember("bufferViews")) {
			bufferViews = asVec<BufferView>(o["bufferViews"]);
			check(bufferViews->size() >= 1);
		}
		if (o.isMember("buffers")) {
			auto b = o["buffers"];
			check(b.isArray());
			std::vector<Buffer> bufs;
			bufs.reserve(b.size());
			for (Json::ArrayIndex i = 0; i < b.size(); ++i) {
				bufs.emplace_back(b[i], resolveUri,
						i == 0 ? std::move(glbData) : std::nullopt);
			}
			check(bufs.size() >= 1);
			buffers = std::move(bufs);
		}
		if (o.isMember("cameras")) {
			cameras = asVec<Camera>(o["cameras"]);
			check(cameras->size() >= 1);
		}
		if (o.isMember("extensionsRequired")) {
			extensionsRequired = asVec<std::string>(o["extensionsRequired"]);
			check(extensionsRequired->size() >= 1);
			checkDuplicateFree(*extensionsRequired);
		}
		if (o.isMember("extensionsUsed")) {
			extensionsUsed = asVec<std::string>(o["extensionsUsed"]);
			check(extensionsUsed->size() >= 1);
			checkDuplicateFree(*extensionsUsed);
		}
		if (o.isMember("images")) {
			images = asVec<Image>(o["images"]);
			check(images->size() >= 1);
		}
		if (o.isMember("materials")) {
			materials = asVec<Material>(o["materials"]);
			check(materials->size() >= 1);
		}
		if (o.isMember("meshes")) {
			meshes = asVec<Mesh>(o["meshes"]);
			check(meshes->size() >= 1);
		}
		if (o.isMember("nodes")) {
			nodes = asVec<Node>(o["nodes"]);
			check(nodes->size() >= 1);
			// Nodes must be a forest:
			// 1. Each node should have indegree 0 or 1:
			std::vector<std::size_t> indeg(nodes->size());
			for (std::size_t i = 0; i < nodes->size(); ++i) {
				auto children = nodes->at(i).children;
				if (!children.has_value()) continue;
				for (auto child : children.value()) {
					++indeg.at(child);
				}
			}
			for (const auto deg : indeg) {
				check(deg <= 1);
			}
			// 2. There should be no cycles:
			std::vector<bool> visited(nodes->size());
			std::stack<std::size_t, std::vector<std::size_t>> toVisit;
			for (std::size_t i = 0; i < nodes->size(); ++i) {
				// Only start DFS in roots.
				if (indeg[i] > 0)
					continue;

				toVisit.push(i);
				do {
					std::size_t j = toVisit.top();
					check(!visited.at(j));
					visited[j] = true;
					toVisit.pop();
					auto children = nodes->at(j).children;
					if (!children.has_value())
						continue;
					for (auto child : *children) {
						toVisit.push(child);
					}
				} while (!toVisit.empty());
			}
		}
		if (o.isMember("samplers")) {
			samplers = asVec<Sampler>(o["samplers"]);
			check(samplers->size() >= 1);
		}
		if (o.isMember("scene")) {
			scene = as<std::size_t>(o["scene"]);
		}
		if (o.isMember("scenes")) {
			scenes = asVec<Scene>(o["scenes"]);
			check(scenes->size() >= 1);
		}
		if (o.isMember("skins")) {
			skins = asVec<Skin>(o["skins"]);
			check(skins->size() >= 1);
		}
		if (o.isMember("textures")) {
			textures = asVec<Texture>(o["textures"]);
			check(textures->size() >= 1);
		}

		// Validation

		checkForall(bufferViews, [&](const BufferView &view) {
			check(buffers.has_value());
			const Buffer &buf = buffers->at(view.buffer);
			// Be careful because of possible integer overflows.
			check(view.byteOffset < buf.byteLength);
			check(view.byteLength <= buf.byteLength);
			check(view.byteOffset <= buf.byteLength - view.byteLength);
		});

		const auto checkAccessor = [&](const auto &accessor,
				std::size_t bufferView, std::size_t byteOffset, std::size_t count) {
			const BufferView &view = bufferViews->at(bufferView);
			if (view.byteStride.has_value())
				check(*view.byteStride % accessor.componentSize() == 0);
			check(byteOffset < view.byteLength);
			// Use division to avoid overflows.
			const auto effective_byte_stride = view.byteStride.value_or(accessor.elementSize());
			check(count <= (view.byteLength - byteOffset) / effective_byte_stride);
		};
		checkForall(accessors, [&](const Accessor &accessor) {
			if (accessor.bufferView.has_value())
				checkAccessor(accessor, *accessor.bufferView, accessor.byteOffset, accessor.count);
			if (accessor.sparse.has_value()) {
				const auto &indices = accessor.sparse->indices;
				checkAccessor(indices, indices.bufferView, indices.byteOffset, accessor.sparse->count);
				const auto &values = accessor.sparse->values;
				checkAccessor(accessor, values.bufferView, values.byteOffset, accessor.sparse->count);
			}
		});

		checkForall(images, [&](const Image &image) {
			checkIndex(bufferViews, image.bufferView);
		});

		checkForall(meshes, [&](const Mesh &mesh) {
			for (const auto &primitive : mesh.primitives) {
				checkIndex(accessors, primitive.indices);
				checkIndex(materials, primitive.material);
				checkIndex(accessors, primitive.attributes.normal);
				checkIndex(accessors, primitive.attributes.position);
				checkIndex(accessors, primitive.attributes.tangent);
				checkForall(primitive.attributes.texcoord, [&](const std::size_t &i) {
					checkIndex(accessors, i);
				});
				checkForall(primitive.attributes.color, [&](const std::size_t &i) {
					checkIndex(accessors, i);
				});
				checkForall(primitive.attributes.joints, [&](const std::size_t &i) {
					checkIndex(accessors, i);
				});
				checkForall(primitive.attributes.weights, [&](const std::size_t &i) {
					checkIndex(accessors, i);
				});
				if (primitive.material.has_value()) {
					const Material &material = materials->at(primitive.material.value());
					if (material.emissiveTexture.has_value()) {
						check(primitive.attributes.texcoord.has_value());
						check(material.emissiveTexture->texCoord < primitive.attributes.texcoord->size());
					}
					if (material.normalTexture.has_value()) {
						check(primitive.attributes.texcoord.has_value());
						check(material.normalTexture->texCoord < primitive.attributes.texcoord->size());
					}
					if (material.occlusionTexture.has_value()) {
						check(primitive.attributes.texcoord.has_value());
						check(material.occlusionTexture->texCoord < primitive.attributes.texcoord->size());
					}
				}
				checkForall(primitive.targets, [&](const MeshPrimitive::MorphTargets &target) {
					checkIndex(accessors, target.normal);
					checkIndex(accessors, target.position);
					checkIndex(accessors, target.tangent);
					checkForall(target.texcoord, [&](const std::size_t &i) {
						checkIndex(accessors, i);
					});
					checkForall(target.color, [&](const std::size_t &i) {
						checkIndex(accessors, i);
					});
				});
			}
		});

		checkForall(nodes, [&](const Node &node) {
			checkIndex(cameras, node.camera);
			checkIndex(meshes, node.mesh);
			checkIndex(skins, node.skin);
		});

		checkForall(scenes, [&](const Scene &scene) {
			checkForall(scene.nodes, [&](const size_t &i) {
				checkIndex(nodes, i);
			});
		});

		checkForall(skins, [&](const Skin &skin) {
			checkIndex(accessors, skin.inverseBindMatrices);
			for (const std::size_t &i : skin.joints)
				checkIndex(nodes, i);
			checkIndex(nodes, skin.skeleton);
		});

		checkForall(textures, [&](const Texture &texture) {
			checkIndex(samplers, texture.sampler);
			checkIndex(images, texture.source);
		});

		checkForall(animations, [&](const Animation &animation) {
			for (const auto &sampler : animation.samplers) {
				checkIndex(accessors, sampler.input);
				const auto &accessor = accessors->at(sampler.input);
				check(accessor.type == Accessor::Type::SCALAR);
				check(accessor.componentType == Accessor::ComponentType::FLOAT);
				checkIndex(accessors, sampler.output);
			}
			for (const auto &channel : animation.channels) {
				checkIndex(nodes, channel.target.node);
				checkIndex(animation.samplers, channel.sampler);
			}
		});

		checkIndex(scenes, scene);
	}
};

// std::span is C++ 20, so we roll our own little struct here.
template <typename T>
struct Span {
	T *ptr;
	uint32_t len;
	bool empty() const {
		return len == 0;
	}
	T *end() const {
		return ptr + len;
	}
	template <typename U>
	Span<U> cast() const {
		return {(U *) ptr, len};
	}
};

static Json::Value readJson(Span<const char> span) {
	Json::CharReaderBuilder builder;
	const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	Json::Value json;
	Json::String err;
	if (!reader->parse(span.ptr, span.end(), &json, &err))
		throw std::runtime_error(std::string("invalid JSON: ") + err);
	return json;
}

inline GlTF readGlb(const char *data, std::size_t len, const UriResolver &resolveUri = uriError) {
	struct Chunk {
		uint32_t type;
		Span<const uint8_t> span;
	};

	struct Stream {
		Span<const uint8_t> span;

		bool eof() const {
			return span.empty();
		}

		void advance(uint32_t n) {
			span.len -= n;
			span.ptr += n;
		}

		uint32_t readUint32() {
			if (span.len < 4)
				throw std::runtime_error("premature EOF");
			uint32_t res = 0;
			for (int i = 0; i < 4; ++i)
				res += span.ptr[i] << (i * 8);
			advance(4);
			return res;
		}

		Chunk readChunk() {
			const auto chunkLen = readUint32();
			if (chunkLen % 4 != 0)
				throw std::runtime_error("chunk length must be multiple of 4");
			const auto chunkType = readUint32();

			auto chunkPtr = span.ptr;
			if (span.len < chunkLen)
				throw std::runtime_error("premature EOF");
			advance(chunkLen);
			return {chunkType, {chunkPtr, chunkLen}};
		}
	};

	constexpr uint32_t MAGIC_GLTF = 0x46546C67;
	constexpr uint32_t MAGIC_JSON = 0x4E4F534A;
	constexpr uint32_t MAGIC_BIN = 0x004E4942;

	if (len > std::numeric_limits<uint32_t>::max())
		throw std::runtime_error("too large");

	Stream is{{(const uint8_t *) data, static_cast<uint32_t>(len)}};

	const auto magic = is.readUint32();
	if (magic != MAGIC_GLTF)
		throw std::runtime_error("wrong magic number");
	const auto version = is.readUint32();
	if (version != 2)
		throw std::runtime_error("wrong version");
	const auto length = is.readUint32();
	if (length != len)
		throw std::runtime_error("wrong length");

	const auto json = is.readChunk();
	if (json.type != MAGIC_JSON)
		throw std::runtime_error("expected JSON chunk");

	std::optional<std::string> buffer;
	if (!is.eof()) {
		const auto chunk = is.readChunk();
		if (chunk.type == MAGIC_BIN)
			buffer = std::string((const char *) chunk.span.ptr, chunk.span.len);
		else if (chunk.type == MAGIC_JSON)
			throw std::runtime_error("unexpected chunk");
		// Ignore all other chunks. We still want to validate that
		// 1. These chunks are valid;
		// 2. These chunks are *not* JSON or BIN chunks
		while (!is.eof()) {
			const auto type = is.readChunk().type;
			if (type == MAGIC_JSON || type == MAGIC_BIN)
				throw std::runtime_error("unexpected chunk");
		}
	}

	return GlTF(readJson(json.span.cast<const char>()), resolveUri, std::move(buffer));
}

inline GlTF readGlTF(const char *data, std::size_t len, const UriResolver &resolveUri = uriError) {
	if (len > std::numeric_limits<uint32_t>::max())
		throw std::runtime_error("too large");

	return GlTF(readJson({data, static_cast<uint32_t>(len)}), resolveUri);
}

}
