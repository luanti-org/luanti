// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2020 sfan5 <sfan5@live.de>

#include "particles.h"
#include <type_traits>

using namespace ParticleParamTypes;

template<typename T>
void RangedParameter<T>::serialize(std::ostream &os) const
{
	min.serialize(os);
	max.serialize(os);
	writeF32(os, bias);
}

template<typename T>
void RangedParameter<T>::deSerialize(std::istream &is)
{
	min.deSerialize(is);
	max.deSerialize(is);
	bias = readF32(is);
}


template<typename T>
T RangedParameter<T>::pickWithin() const
{
	typename T::pickFactors values;
	auto p = numericAbsolute(bias) + 1;
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
		if (bias < 0)
			values[i] = 1.0f - std::pow(myrand_float(), p);
		else
			values[i] = std::pow(myrand_float(), p);
	}
	return T::pick(values, min, max);
}


template<typename T>
T TweenedParameter<T>::blend(float fac) const
{
	// warp time coordinates in accordance w/ settings
	if (fac > beginning) {
		// remap for beginning offset
		auto len = 1 - beginning;
		fac -= beginning;
		fac /= len;

		// remap for repetitions
		fac *= reps;
		if (fac > 1) // poor man's modulo
			fac -= (decltype(reps))fac;

		// remap for style
		switch (style) {
			case TweenStyle::fwd: /* do nothing */  break;
			case TweenStyle::rev: fac = 1.0f - fac; break;
			case TweenStyle::pulse:
			case TweenStyle::flicker: {
				if (fac > 0.5f) {
					fac = 1.f - (fac*2.f - 1.f);
				} else {
					fac = fac * 2;
				}
				if (style == TweenStyle::flicker) {
					fac *= myrand_range(0.7f, 1.0f);
				}
			}
			case TweenStyle::TweenStyle_END:
				break;
		}
		if (fac>1.f)
			fac = 1.f;
		else if (fac<0.f)
			fac = 0.f;
	} else {
		fac = (style == TweenStyle::rev) ? 1.f : 0.f;
	}

	return start.interpolate(fac, end);
}

template<typename T>
void TweenedParameter<T>::serialize(std::ostream &os) const
{
	writeU8(os, static_cast<u8>(style));
	writeU16(os, reps);
	writeF32(os, beginning);
	start.serialize(os);
	end.serialize(os);
}

template<typename T>
void TweenedParameter<T>::deSerialize(std::istream &is)
{
	style = static_cast<TweenStyle>(readU8(is));
	if (style >= TweenStyle::TweenStyle_END)
		style = TweenStyle::fwd;
	reps = readU16(is);
	beginning = readF32(is);
	start.deSerialize(is);
	end.deSerialize(is);
}

namespace ParticleParamTypes {
	// For function definitions
	template struct RangedParameter<v3fParameter>;
	template struct RangedParameter<f32Parameter>;

	template struct TweenedParameter<v2fParameter>;
	template struct TweenedParameter<v3fParameter>;
	template struct TweenedParameter<f32Parameter>;
	template struct TweenedParameter<v3fRange>;
	template struct TweenedParameter<f32Range>;
}

// Linear interpolation
template <typename T>
static T numericalBlend(float fac, T min, T max)
{
	return min + ((max - min) * fac);
}

#define PARAM_PVFN(n) ParticleParamTypes::n##ParameterValue
v2f PARAM_PVFN(pick) (float* f, const v2f a, const v2f b) {
	return v2f(
		numericalBlend(f[0], a.X, b.X),
		numericalBlend(f[1], a.Y, b.Y)
	);
}

v3f PARAM_PVFN(pick) (float* f, const v3f a, const v3f b) {
	return v3f(
		numericalBlend(f[0], a.X, b.X),
		numericalBlend(f[1], a.Y, b.Y),
		numericalBlend(f[2], a.Z, b.Z)
	);
}

v2f PARAM_PVFN(interpolate) (float fac, const v2f a, const v2f b)
	{ return b.getInterpolated(a, fac); }
v3f PARAM_PVFN(interpolate) (float fac, const v3f a, const v3f b)
	{ return b.getInterpolated(a, fac); }

#define PARAM_DEF_SRZR(T, wr, rd) \
	void PARAM_PVFN(serialize)  (std::ostream& os, T  v) {wr(os,v);  } \
	void PARAM_PVFN(deSerialize)(std::istream& is, T& v) {v = rd(is);}


#define PARAM_DEF_NUM(T, wr, rd) PARAM_DEF_SRZR(T, wr, rd) \
	T PARAM_PVFN(interpolate)(float fac, const T a, const T b) \
			{ return numericalBlend<T>(fac,a,b); } \
	T PARAM_PVFN(pick)       (float* f, const T a, const T b) \
			{ return numericalBlend<T>(f[0],a,b); }

PARAM_DEF_NUM(u8,  writeU8,    readU8);  PARAM_DEF_NUM(s8,  writeS8,    readS8);
PARAM_DEF_NUM(u16, writeU16,   readU16); PARAM_DEF_NUM(s16, writeS16,   readS16);
PARAM_DEF_NUM(u32, writeU32,   readU32); PARAM_DEF_NUM(s32, writeS32,   readS32);
PARAM_DEF_NUM(f32, writeF32,   readF32);
PARAM_DEF_SRZR(v2f, writeV2F32, readV2F32);
PARAM_DEF_SRZR(v3f, writeV3F32, readV3F32);

enum class ParticleTextureFlags : u8 {
	/* each value specifies a bit in a bitmask; if the maximum value
	 * goes above 1<<7 the type of the flags field must be changed
	 * from u8, which will necessitate a protocol change! */

	// the first bit indicates whether the texture is animated
	animated = 1,

	/* the next three bits indicate the blending mode of the texture
	 * blendmode is encoded by (flags |= (u8)blend << 1); retrieve with
	 * (flags & ParticleTextureFlags::blend) >> 1. note that the third
	 * bit is currently reserved for adding more blend modes in the future */
	blend = 0x7 << 1,
};

/* define some shorthand so we don't have to repeat ourselves or use
 * decltype everywhere */
using FlagT = std::underlying_type_t<ParticleTextureFlags>;

void ServerParticleTexture::serialize(std::ostream &os, u16 protocol_ver,
		bool newPropertiesOnly, bool skipAnimation) const
{
	/* newPropertiesOnly is used to de/serialize parameters of the legacy texture
	 * field, which are encoded separately from the texspec string */
	FlagT flags = 0;
	if (animated)
		flags |= FlagT(ParticleTextureFlags::animated);
	// Default to `blend = "alpha"` for older clients that don't support `blend = "clip"`
	auto sent_blendmode = (protocol_ver < 47 && blendmode == BlendMode::clip)
			? BlendMode::alpha : blendmode;
	flags |= FlagT(sent_blendmode) << 1;
	serializeParameterValue(os, flags);

	alpha.serialize(os);
	scale.serialize(os);
	if (!newPropertiesOnly)
		os << serializeString32(string);

	if (!skipAnimation && animated)
		animation.serialize(os, protocol_ver);
}

void ServerParticleTexture::deSerialize(std::istream &is, u16 protocol_ver,
		bool newPropertiesOnly, bool skipAnimation)
{
	FlagT flags = 0;
	deSerializeParameterValue(is, flags);
	// new texture properties were missing in ParticleParameters::serialize
	// before Minetest 5.9.0
	if (is.eof())
		return;

	animated = !!(flags & FlagT(ParticleTextureFlags::animated));
	blendmode = BlendMode((flags & FlagT(ParticleTextureFlags::blend)) >> 1);
	if (blendmode >= BlendMode::BlendMode_END)
		throw SerializationError("invalid blend mode");

	alpha.deSerialize(is);
	scale.deSerialize(is);
	if (!newPropertiesOnly)
		string = deSerializeString32(is);

	if (!skipAnimation && animated)
		animation.deSerialize(is, protocol_ver);
}

void ParticleParameters::serialize(std::ostream &os, u16 protocol_ver) const
{
	writeV3F32(os, pos);
	writeV3F32(os, vel);
	writeV3F32(os, acc);
	writeF32(os, expirationtime);
	writeF32(os, size);
	writeU8(os, collisiondetection);
	os << serializeString32(texture.string);
	writeU8(os, vertical);
	writeU8(os, collision_removal);
	animation.serialize(os, protocol_ver);
	writeU8(os, glow);
	writeU8(os, object_collision);
	writeU16(os, node.param0);
	writeU8(os, node.param2);
	writeU8(os, node_tile);
	writeV3F32(os, drag);
	jitter.serialize(os);
	bounce.serialize(os);
	texture.serialize(os, protocol_ver, true, true);
}

template <typename T, T (reader)(std::istream& is)>
inline bool streamEndsBeforeParam(T& val, std::istream& is)
{
	// This is kinda awful
	T tmp = reader(is);
	if (is.eof())
		return true;
	val = tmp;
	return false;
}

void ParticleParameters::deSerialize(std::istream &is, u16 protocol_ver)
{
	pos                = readV3F32(is);
	vel                = readV3F32(is);
	acc                = readV3F32(is);
	expirationtime     = readF32(is);
	size               = readF32(is);
	collisiondetection = readU8(is);
	texture.string     = deSerializeString32(is);
	vertical           = readU8(is);
	collision_removal  = readU8(is);
	animation.deSerialize(is, protocol_ver);
	glow               = readU8(is);
	object_collision   = readU8(is);

	if (streamEndsBeforeParam<u16, readU16>(node.param0, is))
		return;
	node.param2 = readU8(is);
	node_tile   = readU8(is);

	if (streamEndsBeforeParam<v3f, readV3F32>(drag, is))
		return;
	jitter.deSerialize(is);
	bounce.deSerialize(is);
	texture.deSerialize(is, protocol_ver, true, true);
}
