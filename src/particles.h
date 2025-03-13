// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <type_traits>
#include "irrlicht_changes/printing.h"
#include "irrlichttypes_bloated.h"
#include "tileanimation.h"
#include "mapnode.h"
#include "util/serialize.h"
#include "util/numeric.h"

// This file defines the particle-related structures that both the server and
// client need. The ParticleManager and rendering is in client/particles.h

namespace ParticleParamTypes
{
	template<typename T> using BlendFunction = T(float,T,T);
	#define DECL_PARAM_SRZRS(type) \
		void serializeParameterValue  (std::ostream& os, type   v); \
		void deSerializeParameterValue(std::istream& is, type&  r);
	#define DECL_PARAM_OVERLOADS(type) DECL_PARAM_SRZRS(type) \
		type interpolateParameterValue(float  fac,  const type a, const type b); \
		type pickParameterValue       (float* facs, const type a, const type b);

	// Function definition: see "particles.cpp"
	DECL_PARAM_OVERLOADS(u8);  DECL_PARAM_OVERLOADS(s8);
	DECL_PARAM_OVERLOADS(u16); DECL_PARAM_OVERLOADS(s16);
	DECL_PARAM_OVERLOADS(u32); DECL_PARAM_OVERLOADS(s32);
	DECL_PARAM_OVERLOADS(f32);
	DECL_PARAM_OVERLOADS(v2f);
	DECL_PARAM_OVERLOADS(v3f);

	/* C++ is a strongly typed language. this means that enums cannot be implicitly
	 * cast to integers, as they can be in C. while this may sound good in principle,
	 * it means that our normal serialization functions cannot be called on
	 * enumerations unless they are explicitly cast to a particular type first. this
	 * is problematic, because in C++ enums can have any integral type as an underlying
	 * type, and that type would need to be named everywhere an enumeration is
	 * de/serialized.
	 *
	 * this is obviously not cool, both in terms of writing legible, succinct code,
	 * and in terms of robustness: the underlying type might be changed at some point,
	 * e.g. if a bitmask gets too big for its britches. we could use an equivalent of
	 * `std::to_underlying(value)` everywhere we need to deal with enumerations, but
	 * that's hideous and unintuitive. instead, we supply the following functions to
	 * transparently map enumeration types to their underlying values. */

	template <typename E, std::enable_if_t<std::is_enum_v<E>, bool> = true>
	void serializeParameterValue(std::ostream& os, E k) {
		serializeParameterValue(os, (std::underlying_type_t<E>)k);
	}

	template <typename E, std::enable_if_t<std::is_enum_v<E>, bool> = true>
	void deSerializeParameterValue(std::istream& is, E& k) {
		std::underlying_type_t<E> v;
		deSerializeParameterValue(is, v);
		k = (E)v;
	}

	// Describes a single value
	template <typename T, size_t PN>
	struct Parameter
	{
		using ValType = T;
		using pickFactors = float[PN];

		T val = T();
		using This = Parameter<T, PN>;

		Parameter() = default;

		template <typename... Args>
		Parameter(Args... args) : val(args...) {}

		virtual void serialize(std::ostream &os) const
			{ serializeParameterValue  (os, this->val); }
		virtual void deSerialize(std::istream &is)
			{ deSerializeParameterValue(is, this->val); }

		virtual T interpolate(float fac, const This& against) const
		{
			return interpolateParameterValue(fac, this->val, against.val);
		}

		static T pick(float* f, const This& a, const This& b)
		{
			return pickParameterValue(f, a.val, b.val);
		}

		operator T() const { return val; }
		T operator=(T b) { return val = b; }

	};

	// New struct required to differentiate between core::vectorNd-compatible
	// structs for proper value dumping (debugging)
	template <typename T, size_t N>
	struct VectorParameter : public Parameter<T,N> {
		using This = VectorParameter<T,N>;
		template <typename... Args>
		VectorParameter(Args... args) : Parameter<T,N>(args...) {}
	};

	template <typename T, size_t PN>
	inline std::string dump(const Parameter<T,PN>& p)
	{
		return std::to_string(p.val);
	}

	template <typename T, size_t N>
	inline std::string dump(const VectorParameter<T,N>& v)
	{
		std::ostringstream oss;
		oss << v.val;
		return oss.str();
	}

	using f32Parameter = Parameter<f32, 1>;
	using v2fParameter = VectorParameter<v2f, 2>;
	using v3fParameter = VectorParameter<v3f, 3>;
	// Add more parameter types here if you need them ...

	// Bound limits information based on "Parameter" types
	template <typename T>
	struct RangedParameter
	{
		using ValType = T;
		using This = RangedParameter<T>;

		T min, max;
		f32 bias = 0;

		RangedParameter() = default;
		RangedParameter(T _min, T _max)            : min(_min),  max(_max)  {}
		template <typename M> RangedParameter(M b) : min(b),     max(b)     {}

		// Binary format must not be changed. Function is to be deprecated.
		void legacySerialize(std::ostream &os) const
		{
			min.serialize(os);
			max.serialize(os);
		}
		void legacyDeSerialize(std::istream &is)
		{
			min.deSerialize(is);
			max.deSerialize(is);
		}

		void serialize(std::ostream &os) const;
		void deSerialize(std::istream &is);

		This interpolate(float fac, const This against) const
		{
			This r;
			r.min = min.interpolate(fac, against.min);
			r.max = max.interpolate(fac, against.max);
			r.bias = bias;
			return r;
		}

		// Pick a random value (e.g. position) within bounds
		T pickWithin() const;
	};

	template <typename T>
	inline std::string dump(const RangedParameter<T>& r)
	{
		std::ostringstream s;
		s << "range<" << dump(r.min) << " ~ " << dump(r.max);
		if (r.bias != 0)
			s << " :: " << r.bias;
		s << ">";
		return s.str();
	}

	// Animation styles (fwd is normal, linear interpolation)
	// TweenStyle_END is a dummy value for validity check
	enum class TweenStyle : u8 { fwd, rev, pulse, flicker, TweenStyle_END};

	// "Tweened" pretty much means "animated" in this context
	template <typename T>
	struct TweenedParameter
	{
		using ValType = T;
		using This = TweenedParameter<T>;

		TweenStyle style = TweenStyle::fwd;
		u16 reps = 1; // Blending repetitions (same pattern)
		f32 beginning = 0.0f; // Blending start offset

		T start, end;

		TweenedParameter() = default;
		TweenedParameter(T _start, T _end)          : start(_start),  end(_end) {}
		// For initializer lists and assignment
		template <typename M> TweenedParameter(M b) : start(b),       end(b) {}

		// Blend (or animate) the current value
		T blend(float fac) const;

		void serialize(std::ostream &os) const;
		void deSerialize(std::istream &is);
	};

	template <typename T>
	inline std::string dump(const TweenedParameter<T>& t)
	{
		std::ostringstream s;
		const char* icon;
		switch (t.style) {
			case TweenStyle::fwd: icon = "→"; break;
			case TweenStyle::rev: icon = "←"; break;
			case TweenStyle::pulse: icon = "↔"; break;
			case TweenStyle::flicker: icon = "↯"; break;
		}
		s << "tween<";
		if (t.reps != 1)
			s << t.reps << "x ";
		s << dump(t.start) << " "<<icon<<" " << dump(t.end) << ">";
		return s.str();
	}

	enum class AttractorKind : u8 { none, point, line, plane };
	// Note: Allows at most 8 enum members (due to how this is serialized)
	enum class BlendMode     : u8 { alpha, add, sub, screen, clip, BlendMode_END };

	// these are consistently-named convenience aliases to make code more readable without `using ParticleParamTypes` declarations
	using v3fRange = RangedParameter<v3fParameter>;
	using f32Range = RangedParameter<f32Parameter>;

	using v2fTween      = TweenedParameter<v2fParameter>;
	using v3fTween      = TweenedParameter<v3fParameter>;
	using f32Tween      = TweenedParameter<f32Parameter>;
	using v3fRangeTween = TweenedParameter<v3fRange>;
	using f32RangeTween = TweenedParameter<f32Range>;

	#undef DECL_PARAM_SRZRS
	#undef DECL_PARAM_OVERLOADS
}

struct ParticleTexture
{
	bool animated = false;
	ParticleParamTypes::BlendMode blendmode = ParticleParamTypes::BlendMode::alpha;
	TileAnimationParams animation;
	ParticleParamTypes::f32Tween alpha{1.0f};
	ParticleParamTypes::v2fTween scale{v2f(1.0f)};
};

struct ServerParticleTexture : public ParticleTexture
{
	std::string string;
	void serialize(std::ostream &os, u16 protocol_ver, bool newPropertiesOnly = false,
			bool skipAnimation = false) const;
	void deSerialize(std::istream &is, u16 protocol_ver, bool newPropertiesOnly = false,
			bool skipAnimation = false);
};

struct CommonParticleParams
{
	bool collisiondetection = false;
	bool collision_removal = false;
	bool object_collision = false;
	bool vertical = false;
	ServerParticleTexture texture;
	struct TileAnimationParams animation;
	u8 glow = 0;
	MapNode node;
	u8 node_tile = 0;

	CommonParticleParams() {
		animation.type = TAT_NONE;
		node.setContent(CONTENT_IGNORE);
	}

	/* This helper is useful for copying params from
	 * ParticleSpawnerParameters to ParticleParameters */
	inline void copyCommon(CommonParticleParams &to) const {
		to.collisiondetection = collisiondetection;
		to.collision_removal = collision_removal;
		to.object_collision = object_collision;
		to.vertical = vertical;
		to.texture = texture;
		to.animation = animation;
		to.glow = glow;
		to.node = node;
		to.node_tile = node_tile;
	}
};

struct ParticleParameters : CommonParticleParams
{
	v3f pos, vel, acc, drag;
	f32 size = 1, expirationtime = 1;
	ParticleParamTypes::f32Range bounce;
	ParticleParamTypes::v3fRange jitter;

	void serialize(std::ostream &os, u16 protocol_ver) const;
	void deSerialize(std::istream &is, u16 protocol_ver);
};

struct ParticleSpawnerParameters : CommonParticleParams
{
	u16 amount = 1;
	f32 time = 1;

	std::vector<ServerParticleTexture> texpool;

	ParticleParamTypes::v3fRangeTween
		pos, vel, acc, drag, radius, jitter;

	ParticleParamTypes::AttractorKind
		attractor_kind;
	ParticleParamTypes::v3fTween
		attractor_origin, attractor_direction;
	// object IDs
	u16 attractor_attachment = 0,
	    attractor_direction_attachment = 0;
	// do particles disappear when they cross the attractor threshold?
	bool attractor_kill = true;

	ParticleParamTypes::f32RangeTween
		exptime{1.0f},
		size   {1.0f},
		attract{0.0f},
		bounce {0.0f};

	// For historical reasons no (de-)serialization methods here
};
